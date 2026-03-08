//! RadioDoge GUI — Tauri 2 backend library.
//!
//! Wires together the Tauri app:
//!   - Registers all Tauri commands (IPC from frontend to Rust)
//!   - Sets up shared AppState
//!   - Initialises the system tray
//!   - Starts background stats polling when connected
//!   - Auto-reconnects on USB re-plug (exponential backoff)
//!   - Sends desktop notifications for incoming DOGE TX packets
//!   - Emits debug-serial-traffic events for the Debug Console
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
use hex;
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

// ─── Debug helper ─────────────────────────────────────────────────────────────

/// Emit a debug-serial-traffic event to the frontend Debug Console.
/// direction: "TX" or "RX", raw_hex: hex-encoded bytes, parsed: optional human-readable label.
fn emit_debug_traffic(app: &AppHandle, direction: &str, raw_hex: &str, parsed: &str) {
    let ts = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64;
    let _ = app.emit("debug-serial-traffic", serde_json::json!({
        "timestamp": ts,
        "direction": direction,
        "rawHex": raw_hex,
        "parsed": parsed,
    }));
}

// ─── Tauri Commands ───────────────────────────────────────────────────────────

/// List all available serial ports on this system.
#[tauri::command]
async fn list_ports() -> Result<Vec<String>, String> {
    Ok(SerialManager::list_ports())
}

/// List all available serial ports with rich USB device information.
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
        // Emit to frontend packet log
        let _ = app_for_packets.emit("radio-packet", &packet);

        // Emit to debug console as RX traffic
        let parsed = packet.decoded.clone().unwrap_or_default();
        emit_debug_traffic(&app_for_packets, "RX", &packet.payload_hex, &parsed);

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

    // ── Query firmware version with retry (up to 3 attempts, 400ms each) ────
    // The Heltec device may be slow to respond on first connect, so we retry.
    let mut firmware_version: Option<String> = None;
    for attempt in 1..=3 {
        let fw_query = radio::build_get_firmware_version(&NodeAddress::default_local());
        let fw_hex = hex::encode(&fw_query);
        emit_debug_traffic(&app, "TX", &fw_hex, &format!("CMD_GET_FIRMWARE_VERSION (attempt {})", attempt));
        state.serial.send_raw(fw_query).await.ok();
        tokio::time::sleep(Duration::from_millis(400)).await;
        firmware_version = state.serial.get_firmware_version().await;
        if firmware_version.is_some() {
            log::info!("Firmware version received on attempt {}: {:?}", attempt, firmware_version);
            break;
        }
    }

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

    // Auto-reconnect watchdog — exponential backoff (5s → 10s → 20s → … → 60s max)
    let serial_wr = Arc::clone(&state.serial);
    let current_port_wr = Arc::clone(&state.current_port);
    let reconnect_enabled_wr = Arc::clone(&state.reconnect_enabled);
    let app_wr = app.clone();
    let port_wr = port.clone();
    tokio::spawn(async move {
        let mut backoff = Duration::from_secs(5);

        loop {
            tokio::time::sleep(Duration::from_secs(2)).await;

            if !reconnect_enabled_wr.load(Ordering::Relaxed) {
                break;
            }

            if serial_wr.is_connected() {
                backoff = Duration::from_secs(5);
                continue;
            }

            log::info!("Auto-reconnect: connection lost on {}, retrying…", port_wr);
            let _ = app_wr.emit(
                "connection-status",
                ConnectionStatusEvent::reconnecting(&port_wr),
            );

            let app_inner = app_wr.clone();
            let on_pkt = Arc::new(move |packet: IncomingPacket| {
                let _ = app_inner.emit("radio-packet", &packet);
                let parsed = packet.decoded.clone().unwrap_or_default();
                emit_debug_traffic(&app_inner, "RX", &packet.payload_hex, &parsed);
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
                    backoff = Duration::from_secs(5);
                    log::info!("Auto-reconnect: reconnected to {}", port_wr);
                }
                Err(e) => {
                    log::warn!("Auto-reconnect failed: {} — retrying in {:?}", e, backoff);
                    tokio::time::sleep(backoff).await;
                    backoff = (backoff * 2).min(Duration::from_secs(60));
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
#[tauri::command]
async fn get_firmware_version(state: State<'_, AppState>) -> Result<Option<String>, String> {
    Ok(state.serial.get_firmware_version().await)
}

/// Re-query the firmware version from the device (manual retry from UI).
#[tauri::command]
async fn query_firmware_version(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<Option<String>, String> {
    if !state.serial.is_connected() {
        return Ok(None);
    }
    let fw_query = radio::build_get_firmware_version(&NodeAddress::default_local());
    let fw_hex = hex::encode(&fw_query);
    emit_debug_traffic(&app, "TX", &fw_hex, "CMD_GET_FIRMWARE_VERSION (manual re-query)");
    state.serial.send_raw(fw_query).await.map_err(|e| e.to_string())?;
    tokio::time::sleep(Duration::from_millis(600)).await;
    Ok(state.serial.get_firmware_version().await)
}

/// Send a PING to the connected device.
#[tauri::command]
async fn ping_device(state: State<'_, AppState>) -> Result<bool, String> {
    Ok(state.serial.ping().await)
}

/// Generate a new Dogecoin keypair (address + keys).
#[tauri::command]
async fn generate_wallet() -> Result<WalletInfo, String> {
    wallet::generate_keypair().map_err(|e| e.to_string())
}

/// Send a Dogecoin transaction over the LoRa radio.
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

    if payload.len() <= radio::MAX_SINGLE_PAYLOAD_LEN {
        let pkt = radio::build_doge_tx(&src, &dst, &payload);
        let pkt_hex = hex::encode(&pkt);
        emit_debug_traffic(&app, "TX", &pkt_hex, "CMD_DOGE_TX (single packet)");
        state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;
    } else {
        let pkts = radio::build_multipart_packets(&src, &dst, radio::CMD_DOGE_TX, &payload);
        for (i, pkt) in pkts.iter().enumerate() {
            let pkt_hex = hex::encode(pkt);
            emit_debug_traffic(&app, "TX", &pkt_hex, &format!("CMD_DOGE_TX (multipart {}/{})", i + 1, pkts.len()));
            state.serial.send_raw(pkt.clone()).await.map_err(|e| e.to_string())?;
            tokio::time::sleep(Duration::from_millis(50)).await;
        }
    }

    let msg = format!(
        "Broadcast {:.8} DOGE → {} via LoRa! 🐕🌙",
        tx.amount_doge, tx.to_address
    );

    let _ = app.emit("transaction-sent", &msg);

    // Emit a TX packet event for the ReceiveTab/Dashboard combined log
    let tx_log_payload = serde_json::json!({
        "timestamp": std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs(),
        "source": { "region": src.region, "community": src.community, "node": src.node },
        "destination": { "region": 255, "community": 255, "node": 255 },
        "command": radio::CMD_DOGE_TX,
        "payloadHex": hex::encode(&payload),
        "decoded": format!("🐕 DOGE TX: {:.8} DOGE → {}{}", tx.amount_doge, tx.to_address,
            tx.memo.as_deref().map(|m| format!(" [{}]", m)).unwrap_or_default()),
        "rssi": 0,
        "direction": "TX"
    });
    let _ = app.emit("radio-packet-tx", &tx_log_payload);

    log::info!("Transaction sent: {}", msg);
    Ok(msg)
}

/// Update LoRa settings and push the node address to the Heltec device.
///
/// Returns `true` if the device confirmed the new address (round-trip verified).
/// On verified save, emits a connection-status event so the UI updates the node address everywhere.
#[tauri::command]
async fn update_lora_settings(
    settings: LoraSettings,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<bool, String> {
    *state.lora_settings.lock().await = settings.clone();

    if !state.serial.is_connected() {
        return Ok(false);
    }

    let current_addr = state.serial.get_node_address().await;
    let pkt = radio::build_set_node_addr(&current_addr, &settings.node_address);
    let pkt_hex = hex::encode(&pkt);
    emit_debug_traffic(
        &app, "TX", &pkt_hex,
        &format!("CMD_SET_NODE_ADDRS → {}.{}.{}",
            settings.node_address.region, settings.node_address.community, settings.node_address.node),
    );
    state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;

    // OPTIMIZED FOR DESKTOP v0.3.3 – SAFE
    // Also send CMD_SET_LORA_PARAMS (0x21) so the device applies the RF settings.
    // This is additive — the device ACKs the command and applies settings as supported.
    let freq_khz = (settings.frequency_mhz * 1000.0) as u32;
    let bw_idx: u8 = match settings.bandwidth_khz as u32 {
        250 => 1,
        500 => 2,
        _   => 0, // default 125 kHz
    };
    let cr: u8 = settings.coding_rate
        .trim_start_matches("4/")
        .parse::<u8>()
        .unwrap_or(5);
    let tx_power = settings.power_dbm.max(2).min(22) as u8;
    let lora_pkt = radio::build_set_lora_params(
        &settings.node_address, settings.spreading_factor, bw_idx, cr, freq_khz, tx_power,
    );
    let lora_pkt_hex = hex::encode(&lora_pkt);
    emit_debug_traffic(&app, "TX", &lora_pkt_hex, &format!(
        "CMD_SET_LORA_PARAMS → SF{} BW{}kHz CR4/{} {}MHz {}dBm",
        settings.spreading_factor, settings.bandwidth_khz, cr, settings.frequency_mhz, tx_power,
    ));
    state.serial.send_raw(lora_pkt).await.map_err(|e| e.to_string())?;

    // Verify the device accepted the new address with a round-trip check
    let verified = state
        .serial
        .verify_node_address_set(&settings.node_address, 1500)
        .await;

    if verified {
        state
            .serial
            .update_node_address(settings.node_address.clone())
            .await;

        // ── FIX: Emit connection-status with the NEW address so the UI updates everywhere ──
        // This was the bug causing the ConnectionPanel to still show 10.0.1 after save.
        let port = state.current_port.lock().await.clone().unwrap_or_default();
        let fw = state.serial.get_firmware_version().await;
        let _ = app.emit(
            "connection-status",
            ConnectionStatusEvent::connected(&port, settings.node_address.clone(), fw),
        );

        emit_debug_traffic(&app, "RX", "", &format!(
            "✅ Node address verified: {}.{}.{}",
            settings.node_address.region, settings.node_address.community, settings.node_address.node
        ));
    }

    log::info!(
        "LoRa settings updated (verified={}): {}MHz, SF{}, {}kHz, addr={}.{}.{}",
        verified,
        settings.frequency_mhz,
        settings.spreading_factor,
        settings.bandwidth_khz,
        settings.node_address.region,
        settings.node_address.community,
        settings.node_address.node,
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
            query_firmware_version,
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
