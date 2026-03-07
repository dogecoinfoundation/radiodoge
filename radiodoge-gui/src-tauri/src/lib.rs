//! RadioDoge GUI — Tauri 2 backend library.
//!
//! Wires together the Tauri app:
//!   - Registers all Tauri commands (IPC from frontend to Rust)
//!   - Sets up shared AppState
//!   - Initialises the system tray
//!   - Starts background stats polling when connected
//!   - Auto-reconnects on USB re-plug (exponential backoff)
//!   - Sends desktop notifications for incoming DOGE TX packets
//!
//! All protocol, wallet, and serial logic lives in `radiodoge-core`
//! (no Tauri dependency) so it is shared with `radiodoge-cli`.

use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use tauri::{AppHandle, Emitter, Manager, State};
use tauri_plugin_notification::NotificationExt;
use tokio::sync::Mutex;

// Tauri-specific module (stays in the GUI crate)
mod tray;

// All protocol + hardware logic from the shared core library
use radiodoge_core::{radio, wallet};
use radiodoge_core::serial::SerialManager;
use radiodoge_core::types::{
    ConnectionStatusEvent, IncomingPacket, LoraSettings, NodeAddress, PortInfo, RadioStats,
    TransactionRequest, WalletInfo,
};

/// Shared application state — injected into every Tauri command via `State<AppState>`.
pub struct AppState {
    pub serial: Arc<SerialManager>,
    pub current_port: Arc<Mutex<Option<String>>>,
    pub lora_settings: Arc<Mutex<LoraSettings>>,
    /// Set to false when the user explicitly disconnects — stops the reconnect watchdog.
    pub reconnect_enabled: Arc<AtomicBool>,
}

impl AppState {
    fn new() -> Self {
        AppState {
            serial: Arc::new(SerialManager::new()),
            current_port: Arc::new(Mutex::new(None)),
            lora_settings: Arc::new(Mutex::new(LoraSettings::default())),
            reconnect_enabled: Arc::new(AtomicBool::new(false)),
        }
    }
}

// ─── Tauri Commands ───────────────────────────────────────────────────────────

/// List all available serial ports on this system.
/// Returns port names like "COM3" (Windows) or "/dev/ttyUSB0" (Linux).
/// USB serial devices (CP210x / CH340 / CH9102 etc.) are sorted first.
#[tauri::command]
async fn list_ports() -> Result<Vec<String>, String> {
    Ok(SerialManager::list_ports())
}

/// List all available serial ports with rich USB device information.
///
/// Returns [`PortInfo`] structs including USB VID/PID, manufacturer, product
/// string, and whether the port matches a known Heltec/ESP32 adapter.
/// Likely-Heltec ports are sorted first so the UI can auto-select them.
#[tauri::command]
async fn list_ports_detailed() -> Result<Vec<PortInfo>, String> {
    Ok(SerialManager::list_ports_with_info())
}

/// Open a serial connection to the Heltec device on the given port.
/// Emits "connection-status" events as the connection progresses.
/// Spawns auto-reconnect watchdog and desktop notifications for DOGE TX.
#[tauri::command]
async fn connect_port(
    port: String,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    log::info!("connect_port: {}", port);

    let _ = app.emit("connection-status", ConnectionStatusEvent::connecting(&port));

    // Enable the reconnect watchdog for this session
    state.reconnect_enabled.store(true, Ordering::Relaxed);

    let app_for_packets = app.clone();
    let on_packet = Arc::new(move |packet: IncomingPacket| {
        let _ = app_for_packets.emit("radio-packet", &packet);

        // Desktop toast notification for incoming Dogecoin transactions
        if packet.command == radio::CMD_DOGE_TX {
            let body = packet
                .decoded
                .clone()
                .unwrap_or_else(|| "Incoming Dogecoin transaction over LoRa".to_string());
            let _ = app_for_packets
                .notification()
                .builder()
                .title("🐕 DOGE Transaction Received!")
                .body(&body)
                .show();
        }
    });

    state
        .serial
        .connect(&port, on_packet)
        .await
        .map_err(|e| {
            let raw = e.to_string();
            // Surface actionable hints for the most common Windows serial failures.
            if raw.contains("Access is denied") || raw.contains("Permission denied") {
                format!(
                    "Access denied on {}.\n\
                     Hint: Another program (Arduino IDE, PuTTY, device manager) may \
                     already have this port open. Close it and try again.",
                    port
                )
            } else if raw.contains("could not open")
                || raw.contains("No such file")
                || raw.contains("The system cannot find")
            {
                format!(
                    "Could not open {}.\n\
                     Hint: Install the CP210x or CH340 USB driver for your Heltec board.\n\
                     CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers\n\
                     CH340: https://www.wch-ic.com/products/CH340.html",
                    port
                )
            } else if raw.contains("The device is not connected")
                || raw.contains("device has been removed")
            {
                format!(
                    "Device disconnected unexpectedly on {}.\n\
                     Hint: Check the USB cable — some USB-C cables are power-only.",
                    port
                )
            } else {
                raw
            }
        })?;

    *state.current_port.lock().await = Some(port.clone());

    // Query firmware version — give the device up to 500 ms to respond
    let fw_query = radio::build_get_firmware_version(&NodeAddress::default_local());
    state.serial.send_raw(fw_query).await.ok();
    tokio::time::sleep(Duration::from_millis(500)).await;
    let firmware_version = state.serial.get_firmware_version().await;

    let node_addr = state.serial.get_node_address().await;
    let _ = app.emit(
        "connection-status",
        ConnectionStatusEvent::connected(&port, node_addr, firmware_version),
    );

    tray::update_tray_status(&app, true, Some(&port));

    // Background stats polling — pushes stats to the frontend every 2 s
    let serial_poll = Arc::clone(&state.serial);
    let app_for_poll = app.clone();
    tokio::spawn(async move {
        loop {
            if !serial_poll.is_connected() {
                break;
            }
            let stats = serial_poll.get_stats().await;
            let _ = app_for_poll.emit("radio-stats-update", &stats);
            tokio::time::sleep(Duration::from_secs(2)).await;
        }
    });

    // Auto-reconnect watchdog — monitors connection health and retries with
    // exponential backoff (1s → 2s → 4s → … → 30s max) up on USB re-plug.
    let serial_wr = Arc::clone(&state.serial);
    let current_port_wr = Arc::clone(&state.current_port);
    let reconnect_enabled_wr = Arc::clone(&state.reconnect_enabled);
    let app_wr = app.clone();
    let port_wr = port.clone();
    tokio::spawn(async move {
        let mut backoff = Duration::from_secs(1);

        loop {
            tokio::time::sleep(Duration::from_secs(2)).await;

            if !reconnect_enabled_wr.load(Ordering::Relaxed) {
                // User explicitly disconnected — stop watchdog
                break;
            }

            if serial_wr.is_connected() {
                backoff = Duration::from_secs(1); // reset on healthy connection
                continue;
            }

            // Unexpected disconnect detected — attempt reconnect
            log::info!("Auto-reconnect: connection lost on {}, retrying…", port_wr);
            let _ = app_wr.emit(
                "connection-status",
                ConnectionStatusEvent::reconnecting(&port_wr),
            );

            let app_inner = app_wr.clone();
            let on_pkt = Arc::new(move |packet: IncomingPacket| {
                let _ = app_inner.emit("radio-packet", &packet);
                if packet.command == radio::CMD_DOGE_TX {
                    let body = packet
                        .decoded
                        .clone()
                        .unwrap_or_else(|| "Incoming Dogecoin transaction".to_string());
                    let _ = app_inner
                        .notification()
                        .builder()
                        .title("🐕 DOGE Transaction Received!")
                        .body(&body)
                        .show();
                }
            });

            match serial_wr.connect(&port_wr, on_pkt).await {
                Ok(_) => {
                    *current_port_wr.lock().await = Some(port_wr.clone());
                    let node_addr = serial_wr.get_node_address().await;
                    let _ = app_wr.emit(
                        "connection-status",
                        ConnectionStatusEvent::connected(&port_wr, node_addr, None),
                    );
                    tray::update_tray_status(&app_wr, true, Some(&port_wr));
                    backoff = Duration::from_secs(1);
                    log::info!("Auto-reconnect: reconnected to {}", port_wr);
                }
                Err(e) => {
                    log::warn!("Auto-reconnect failed: {} — retrying in {:?}", e, backoff);
                    tokio::time::sleep(backoff).await;
                    backoff = (backoff * 2).min(Duration::from_secs(30));
                }
            }
        }
    });

    Ok(())
}

/// Close the serial connection (disables auto-reconnect).
#[tauri::command]
async fn disconnect_port(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    log::info!("disconnect_port");

    // Disable the reconnect watchdog before disconnecting
    state.reconnect_enabled.store(false, Ordering::Relaxed);

    state.serial.disconnect().await.map_err(|e| e.to_string())?;

    *state.current_port.lock().await = None;

    let _ = app.emit("connection-status", ConnectionStatusEvent::disconnected());
    tray::update_tray_status(&app, false, None);

    Ok(())
}

/// Check if a serial port is currently open.
#[tauri::command]
async fn is_connected(state: State<'_, AppState>) -> Result<bool, String> {
    Ok(state.serial.is_connected())
}

/// Get the current radio statistics.
#[tauri::command]
async fn get_radio_stats(state: State<'_, AppState>) -> Result<RadioStats, String> {
    Ok(state.serial.get_stats().await)
}

/// Get the current node address (e.g. "10.0.1").
#[tauri::command]
async fn get_node_address(state: State<'_, AppState>) -> Result<NodeAddress, String> {
    Ok(state.serial.get_node_address().await)
}

/// Get the firmware version string from the connected device.
/// Returns None if the device has not responded to the version query yet.
#[tauri::command]
async fn get_firmware_version(state: State<'_, AppState>) -> Result<Option<String>, String> {
    Ok(state.serial.get_firmware_version().await)
}

/// Send a PING to the connected device.
/// Returns true if the device responded within 500ms.
#[tauri::command]
async fn ping_device(state: State<'_, AppState>) -> Result<bool, String> {
    Ok(state.serial.ping().await)
}

/// Generate a new Dogecoin keypair (address + keys).
/// Uses OS-level entropy — safe to call multiple times.
/// ⚠️  The returned private key must be saved by the user — it is never stored!
#[tauri::command]
async fn generate_wallet() -> Result<WalletInfo, String> {
    wallet::generate_keypair().map_err(|e| e.to_string())
}

/// Send a Dogecoin transaction over the LoRa radio.
///
/// Large payloads are correctly split into multipart packets (was silently
/// truncated in previous versions — that bug is now fixed).
#[tauri::command]
async fn send_transaction(
    tx: TransactionRequest,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<String, String> {
    if !state.serial.is_connected() {
        return Err("Not connected to a Heltec device. Please connect first.".to_string());
    }

    if !wallet::is_valid_address(&tx.to_address) {
        return Err(format!(
            "Invalid Dogecoin address: {}. Addresses start with 'D'.",
            tx.to_address
        ));
    }

    if tx.amount_doge <= 0.0 {
        return Err("Amount must be greater than 0 DOGE".to_string());
    }

    let payload = wallet::encode_transaction_payload(
        &tx.to_address,
        tx.amount_doge,
        tx.memo.as_deref(),
    )
    .map_err(|e| e.to_string())?;

    let src = state.serial.get_node_address().await;
    let dst = NodeAddress::broadcast();

    // ── FIXED: proper multipart for large payloads (was silently truncated before) ──
    if payload.len() <= radio::MAX_SINGLE_PAYLOAD_LEN {
        let pkt = radio::build_doge_tx(&src, &dst, &payload);
        state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;
    } else {
        let pkts = radio::build_multipart_packets(&src, &dst, radio::CMD_DOGE_TX, &payload);
        for pkt in pkts {
            state
                .serial
                .send_raw(pkt)
                .await
                .map_err(|e| e.to_string())?;
            // Small inter-packet gap to avoid flooding the device buffer
            tokio::time::sleep(Duration::from_millis(50)).await;
        }
    }

    let msg = format!(
        "Broadcast {:.8} DOGE → {} via LoRa! 🐕🌙",
        tx.amount_doge, tx.to_address
    );

    // Trigger confetti 🎉 on the frontend
    let _ = app.emit("transaction-sent", &msg);

    log::info!("Transaction sent: {}", msg);
    Ok(msg)
}

/// Update LoRa settings and push the node address to the Heltec device.
///
/// Returns `true` if the device confirmed the new address (round-trip verified),
/// `false` if saved locally but the device did not respond within 1 second.
#[tauri::command]
async fn update_lora_settings(
    settings: LoraSettings,
    state: State<'_, AppState>,
) -> Result<bool, String> {
    *state.lora_settings.lock().await = settings.clone();

    if !state.serial.is_connected() {
        return Ok(false); // Not connected — just store for later, not verified
    }

    let current_addr = state.serial.get_node_address().await;
    let pkt = radio::build_set_node_addr(&current_addr, &settings.node_address);
    state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;

    // Verify the device accepted the new address with a round-trip check
    let verified = state
        .serial
        .verify_node_address_set(&settings.node_address, 1000)
        .await;

    if verified {
        state
            .serial
            .update_node_address(settings.node_address.clone())
            .await;
    }

    log::info!(
        "LoRa settings updated (verified={}): {}MHz, SF{}, {}kHz",
        verified,
        settings.frequency_mhz,
        settings.spreading_factor,
        settings.bandwidth_khz
    );

    Ok(verified)
}

/// Get the current stored LoRa settings.
#[tauri::command]
async fn get_lora_settings(state: State<'_, AppState>) -> Result<LoraSettings, String> {
    Ok(state.lora_settings.lock().await.clone())
}

// ─── App Builder ─────────────────────────────────────────────────────────────

/// Entry point for desktop (main.rs) and mobile (lib for CDyLib).
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    env_logger::init();

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_notification::init())
        .manage(AppState::new())
        .invoke_handler(tauri::generate_handler![
            list_ports,
            list_ports_detailed,
            connect_port,
            disconnect_port,
            is_connected,
            get_radio_stats,
            get_node_address,
            get_firmware_version,
            ping_device,
            generate_wallet,
            send_transaction,
            update_lora_settings,
            get_lora_settings,
        ])
        .setup(|app| {
            tray::setup_tray(app)?;
            log::info!("RadioDoge GUI started — much wow 🐕");
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("Error running RadioDoge GUI")
}
