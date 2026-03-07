//! RadioDoge GUI — Tauri 2 backend library.
//!
//! This file wires together the Tauri app:
//!   - Registers all Tauri commands (IPC from frontend to Rust)
//!   - Sets up shared AppState
//!   - Initializes the system tray
//!   - Starts background stats polling when connected

use std::sync::Arc;
use std::time::Duration;

use tauri::{AppHandle, Emitter, Manager, Runtime, State};
use tokio::sync::Mutex;

mod radio;
mod serial;
mod tray;
mod types;
mod wallet;

use serial::SerialManager;
use types::{
    ConnectionStatusEvent, IncomingPacket, LoraSettings, NodeAddress, RadioStats,
    TransactionRequest, WalletInfo,
};

/// Shared application state — injected into every Tauri command via `State<AppState>`.
pub struct AppState {
    pub serial: Arc<SerialManager>,
    pub current_port: Arc<Mutex<Option<String>>>,
    pub lora_settings: Arc<Mutex<LoraSettings>>,
}

impl AppState {
    fn new() -> Self {
        AppState {
            serial: Arc::new(SerialManager::new()),
            current_port: Arc::new(Mutex::new(None)),
            lora_settings: Arc::new(Mutex::new(LoraSettings::default())),
        }
    }
}

// ─── Tauri Commands ───────────────────────────────────────────────────────────

/// List all available serial ports on this system.
/// Returns port names like "COM3" (Windows) or "/dev/ttyUSB0" (Linux).
#[tauri::command]
async fn list_ports() -> Result<Vec<String>, String> {
    Ok(SerialManager::list_ports())
}

/// Open a serial connection to the Heltec device on the given port.
/// Emits "connection-status" events as the connection progresses.
#[tauri::command]
async fn connect_port(
    port: String,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    log::info!("connect_port: {}", port);

    // Emit "connecting" status
    let _ = app.emit("connection-status", ConnectionStatusEvent::connecting(&port));

    // Clone app handle for use inside the packet callback
    let app_for_packets = app.clone();
    let on_packet = Arc::new(move |packet: IncomingPacket| {
        let _ = app_for_packets.emit("radio-packet", &packet);
    });

    state
        .serial
        .connect(&port, on_packet)
        .await
        .map_err(|e| e.to_string())?;

    // Store the current port name
    *state.current_port.lock().await = Some(port.clone());

    // Emit "connected" status with node address
    let node_addr = state.serial.get_node_address().await;
    let _ = app.emit(
        "connection-status",
        ConnectionStatusEvent::connected(&port, node_addr.clone()),
    );

    // Update tray
    tray::update_tray_status(&app, true, Some(&port));

    // Start background stats polling (every 2 seconds)
    let serial = Arc::clone(&state.serial);
    let app_for_poll = app.clone();
    tokio::spawn(async move {
        loop {
            if !serial.is_connected() {
                break;
            }
            let stats = serial.get_stats().await;
            let _ = app_for_poll.emit("radio-stats-update", &stats);
            tokio::time::sleep(Duration::from_secs(2)).await;
        }
    });

    Ok(())
}

/// Close the serial connection.
#[tauri::command]
async fn disconnect_port(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    log::info!("disconnect_port");

    state
        .serial
        .disconnect()
        .await
        .map_err(|e| e.to_string())?;

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

/// Send a PING to the connected device.
/// Returns true if the device responded within 500ms.
#[tauri::command]
async fn ping_device(state: State<'_, AppState>) -> Result<bool, String> {
    Ok(state.serial.ping().await)
}

/// Generate a new Dogecoin keypair (address + keys).
/// Uses OS-level entropy — safe to call multiple times.
/// ⚠️  The returned private key must be saved by the user!
#[tauri::command]
async fn generate_wallet() -> Result<WalletInfo, String> {
    wallet::generate_keypair().map_err(|e| e.to_string())
}

/// Send a Dogecoin transaction over the LoRa radio.
///
/// This packages the transaction as a RadioDoge packet and writes it
/// to the connected Heltec device via serial. The device then broadcasts
/// it over LoRa.
#[tauri::command]
async fn send_transaction(
    tx: TransactionRequest,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<String, String> {
    if !state.serial.is_connected() {
        return Err("Not connected to a Heltec device. Please connect first.".to_string());
    }

    // Validate the recipient address
    if !wallet::is_valid_address(&tx.to_address) {
        return Err(format!(
            "Invalid Dogecoin address: {}. Addresses start with 'D'.",
            tx.to_address
        ));
    }

    if tx.amount_doge <= 0.0 {
        return Err("Amount must be greater than 0 DOGE".to_string());
    }

    // Encode the transaction as a binary payload
    let payload = wallet::encode_transaction_payload(
        &tx.to_address,
        tx.amount_doge,
        tx.memo.as_deref(),
    )
    .map_err(|e| e.to_string())?;

    let src = state.serial.get_node_address().await;
    let dst = NodeAddress::broadcast(); // Broadcast to all nodes

    // Build the packet(s) — use multipart if payload is large
    let bytes = if payload.len() <= radio::MAX_SINGLE_PAYLOAD_LEN {
        radio::build_doge_tx(&src, &dst, &payload)
    } else {
        // For MVP: truncate to single packet; full multipart handled in future
        radio::build_doge_tx(&src, &dst, &payload[..radio::MAX_SINGLE_PAYLOAD_LEN])
    };

    state
        .serial
        .send_raw(bytes)
        .await
        .map_err(|e| e.to_string())?;

    let msg = format!(
        "Broadcast {:.8} DOGE → {} via LoRa! 🐕🌙",
        tx.amount_doge, tx.to_address
    );

    // Emit a success event so the frontend can trigger confetti 🎉
    let _ = app.emit("transaction-sent", &msg);

    log::info!("Transaction sent: {}", msg);
    Ok(msg)
}

/// Update LoRa settings and send them to the Heltec device.
#[tauri::command]
async fn update_lora_settings(
    settings: LoraSettings,
    state: State<'_, AppState>,
) -> Result<(), String> {
    // Store settings locally
    *state.lora_settings.lock().await = settings.clone();

    if !state.serial.is_connected() {
        // Not connected — just store for later
        return Ok(());
    }

    // Send the node address update to the device
    let current_addr = state.serial.get_node_address().await;
    let set_addr_packet = radio::build_set_node_addr(&current_addr, &settings.node_address);

    state
        .serial
        .send_raw(set_addr_packet)
        .await
        .map_err(|e| e.to_string())?;

    // Note: Other LoRa parameters (freq, SF, BW, etc.) would be sent via
    // additional commands once the firmware supports runtime reconfiguration.
    // For now, those are set at flash time on the device.

    log::info!(
        "LoRa settings updated: {}MHz, SF{}, {}kHz",
        settings.frequency_mhz,
        settings.spreading_factor,
        settings.bandwidth_khz
    );

    Ok(())
}

/// Get the current stored LoRa settings.
#[tauri::command]
async fn get_lora_settings(state: State<'_, AppState>) -> Result<LoraSettings, String> {
    Ok(state.lora_settings.lock().await.clone())
}

// ─── App Builder ─────────────────────────────────────────────────────────────

/// Entry point for both desktop (main.rs) and mobile (lib.rs for CDyLib).
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    env_logger::init();

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_notification::init())
        .manage(AppState::new())
        .invoke_handler(tauri::generate_handler![
            list_ports,
            connect_port,
            disconnect_port,
            is_connected,
            get_radio_stats,
            get_node_address,
            ping_device,
            generate_wallet,
            send_transaction,
            update_lora_settings,
            get_lora_settings,
        ])
        .setup(|app| {
            // Set up system tray
            tray::setup_tray(app)?;
            log::info!("RadioDoge GUI started — much wow 🐕");
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("Error running RadioDoge GUI");
}
