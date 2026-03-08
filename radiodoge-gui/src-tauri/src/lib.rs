//! RadioDoge GUI — Tauri 2 backend library.
//!
//! v0.3.6 additions:
//!   - Board-is-source-of-truth: CMD_GET_SETTINGS (0x22) queried on connect, UI syncs from board
//!   - WIF wallet import (`import_wif`)
//!   - Transaction history (in-memory + JSON persistence)
//!   - Gateway mode: `set_gateway_mode` / `start_gateway` / `stop_gateway`
//!   - Connection type toggle (USB / BLE) — Rust scaffold, UI toggle
//!
//! All protocol, wallet, and serial logic lives in `radiodoge-core`.

use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use tauri::{AppHandle, Emitter, Manager, State};
use tauri_plugin_notification::NotificationExt;
use tokio::sync::Mutex;

mod tray;

use radiodoge_core::{radio, wallet};
use hex;
use radiodoge_core::serial::SerialManager;
use radiodoge_core::types::{
    BoardSettings, ConnectionStatusEvent, IncomingPacket, LoraSettings, NodeAddress, PortInfo,
    RadioStats, TransactionRequest, TxHistoryEntry, WalletInfo,
};

/// Maximum transaction history entries kept in memory + persisted to disk.
const MAX_HISTORY: usize = 50;

/// Shared application state — injected into every Tauri command via `State<AppState>`.
pub struct AppState {
    pub serial: Arc<SerialManager>,
    pub current_port: Arc<Mutex<Option<String>>>,
    pub lora_settings: Arc<Mutex<LoraSettings>>,
    /// Set to false when the user explicitly disconnects — stops the reconnect watchdog.
    pub reconnect_enabled: Arc<AtomicBool>,
    /// v0.3.6 — Transaction history (last MAX_HISTORY entries), persisted to JSON.
    pub tx_history: Arc<Mutex<Vec<TxHistoryEntry>>>,
    /// v0.3.6 — Background gateway daemon process handle.
    pub gateway_process: Arc<Mutex<Option<tokio::process::Child>>>,
    /// v0.3.6 — Connection type: "usb" or "ble"
    pub connection_type: Arc<Mutex<String>>,
}

impl AppState {
    fn new() -> Self {
        AppState {
            serial: Arc::new(SerialManager::new()),
            current_port: Arc::new(Mutex::new(None)),
            lora_settings: Arc::new(Mutex::new(LoraSettings::default())),
            reconnect_enabled: Arc::new(AtomicBool::new(false)),
            tx_history: Arc::new(Mutex::new(Vec::new())),
            gateway_process: Arc::new(Mutex::new(None)),
            connection_type: Arc::new(Mutex::new("usb".to_string())),
        }
    }
}

// ─── Debug helper ─────────────────────────────────────────────────────────────

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

// ─── History helpers ──────────────────────────────────────────────────────────

/// Load transaction history from disk (best-effort; returns empty vec on any error).
async fn load_history_from_disk(app: &AppHandle) -> Vec<TxHistoryEntry> {
    let path = match app.path().app_data_dir() {
        Ok(p) => p.join("tx_history.json"),
        Err(_) => return vec![],
    };
    match tokio::fs::read_to_string(&path).await {
        Ok(json) => serde_json::from_str(&json).unwrap_or_default(),
        Err(_) => vec![],
    }
}

/// Persist transaction history to disk (best-effort; silently ignores errors).
async fn save_history_to_disk(app: &AppHandle, history: &[TxHistoryEntry]) {
    let dir = match app.path().app_data_dir() {
        Ok(p) => p,
        Err(_) => return,
    };
    let _ = tokio::fs::create_dir_all(&dir).await;
    let path = dir.join("tx_history.json");
    if let Ok(json) = serde_json::to_string_pretty(history) {
        let _ = tokio::fs::write(&path, json).await;
    }
}

// ─── Tauri Commands ───────────────────────────────────────────────────────────

#[tauri::command]
async fn list_ports() -> Result<Vec<String>, String> {
    Ok(SerialManager::list_ports())
}

#[tauri::command]
async fn list_ports_detailed() -> Result<Vec<PortInfo>, String> {
    Ok(SerialManager::list_ports_with_info())
}

/// Open a serial connection to the Heltec device.
/// v0.3.6: After connecting, queries CMD_GET_SETTINGS (0x22) to sync board state.
/// Board is source of truth — node address and gateway_mode update from board reply.
#[tauri::command]
async fn connect_port(
    port: String,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    log::info!("connect_port: {}", port);

    let _ = app.emit("connection-status", ConnectionStatusEvent::connecting(&port));

    state.reconnect_enabled.store(true, Ordering::Relaxed);

    let app_for_packets = app.clone();
    let on_packet = Arc::new(move |packet: IncomingPacket| {
        let _ = app_for_packets.emit("radio-packet", &packet);

        let parsed = packet.decoded.clone().unwrap_or_default();
        emit_debug_traffic(&app_for_packets, "RX", &packet.payload_hex, &parsed);

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

    // ── Query firmware version (up to 3 attempts) ────────────────────────────
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

    // ── v0.3.6: Query board settings (CMD_GET_SETTINGS 0x22) ─────────────────
    // Board is source of truth. App syncs node address + gateway_mode from board reply.
    let settings_query = radio::build_get_settings(&NodeAddress::default_local());
    let sq_hex = hex::encode(&settings_query);
    emit_debug_traffic(&app, "TX", &sq_hex, "CMD_GET_SETTINGS (0x22) — board-sync on connect");
    state.serial.send_raw(settings_query).await.ok();
    tokio::time::sleep(Duration::from_millis(500)).await;
    let board_settings = state.serial.get_board_settings().await;

    // If board reported settings, emit board-sync event and update node address.
    // Board wins on mismatch — no local cache overrides board state.
    if let Some(ref bs) = board_settings {
        log::info!("Board sync: addr={} gateway={}", bs.node_address.to_display_string(), bs.gateway_mode);
        let _ = app.emit("board-sync", bs);
        state.serial.update_node_address(bs.node_address.clone()).await;
    }

    let node_addr = state.serial.get_node_address().await;
    let _ = app.emit(
        "connection-status",
        ConnectionStatusEvent::connected(&port, node_addr, firmware_version),
    );

    tray::update_tray_status(&app, true, Some(&port));

    // Background stats polling every 2 s
    let serial_poll = Arc::clone(&state.serial);
    let app_for_poll = app.clone();
    tokio::spawn(async move {
        loop {
            if !serial_poll.is_connected() { break; }
            let stats = serial_poll.get_stats().await;
            let _ = app_for_poll.emit("radio-stats-update", &stats);
            tokio::time::sleep(Duration::from_secs(2)).await;
        }
    });

    // Auto-reconnect watchdog (exponential backoff 5s → 60s)
    let serial_wr = Arc::clone(&state.serial);
    let current_port_wr = Arc::clone(&state.current_port);
    let reconnect_enabled_wr = Arc::clone(&state.reconnect_enabled);
    let app_wr = app.clone();
    let port_wr = port.clone();
    tokio::spawn(async move {
        let mut backoff = Duration::from_secs(5);
        loop {
            tokio::time::sleep(Duration::from_secs(2)).await;
            if !reconnect_enabled_wr.load(Ordering::Relaxed) { break; }
            if serial_wr.is_connected() {
                backoff = Duration::from_secs(5);
                continue;
            }

            log::info!("Auto-reconnect: connection lost on {}, retrying…", port_wr);
            let _ = app_wr.emit("connection-status", ConnectionStatusEvent::reconnecting(&port_wr));

            let app_inner = app_wr.clone();
            let on_pkt = Arc::new(move |packet: IncomingPacket| {
                let _ = app_inner.emit("radio-packet", &packet);
                let parsed = packet.decoded.clone().unwrap_or_default();
                emit_debug_traffic(&app_inner, "RX", &packet.payload_hex, &parsed);
                if packet.command == radio::CMD_DOGE_TX {
                    let body = packet.decoded.clone().unwrap_or_else(|| "Incoming Dogecoin transaction".to_string());
                    let _ = app_inner.notification().builder().title("🐕 DOGE Transaction Received!").body(&body).show();
                }
            });

            match serial_wr.connect(&port_wr, on_pkt).await {
                Ok(_) => {
                    *current_port_wr.lock().await = Some(port_wr.clone());
                    // v0.3.6 — re-sync board settings after reconnect
                    let sq = radio::build_get_settings(&NodeAddress::default_local());
                    serial_wr.send_raw(sq).await.ok();
                    tokio::time::sleep(Duration::from_millis(500)).await;
                    if let Some(bs) = serial_wr.get_board_settings().await {
                        let _ = app_wr.emit("board-sync", &bs);
                        serial_wr.update_node_address(bs.node_address).await;
                    }
                    let node_addr = serial_wr.get_node_address().await;
                    let _ = app_wr.emit("connection-status", ConnectionStatusEvent::connected(&port_wr, node_addr, None));
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

    // Load persisted history on connect (once per session)
    {
        let mut hist = state.tx_history.lock().await;
        if hist.is_empty() {
            *hist = load_history_from_disk(&app).await;
        }
    }

    Ok(())
}

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

#[tauri::command]
async fn is_connected(state: State<'_, AppState>) -> Result<bool, String> {
    Ok(state.serial.is_connected())
}

#[tauri::command]
async fn get_radio_stats(state: State<'_, AppState>) -> Result<RadioStats, String> {
    Ok(state.serial.get_stats().await)
}

#[tauri::command]
async fn get_node_address(state: State<'_, AppState>) -> Result<NodeAddress, String> {
    Ok(state.serial.get_node_address().await)
}

#[tauri::command]
async fn get_firmware_version(state: State<'_, AppState>) -> Result<Option<String>, String> {
    Ok(state.serial.get_firmware_version().await)
}

#[tauri::command]
async fn query_firmware_version(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<Option<String>, String> {
    if !state.serial.is_connected() { return Ok(None); }
    let fw_query = radio::build_get_firmware_version(&NodeAddress::default_local());
    let fw_hex = hex::encode(&fw_query);
    emit_debug_traffic(&app, "TX", &fw_hex, "CMD_GET_FIRMWARE_VERSION (manual re-query)");
    state.serial.send_raw(fw_query).await.map_err(|e| e.to_string())?;
    tokio::time::sleep(Duration::from_millis(600)).await;
    Ok(state.serial.get_firmware_version().await)
}

#[tauri::command]
async fn ping_device(state: State<'_, AppState>, app: AppHandle) -> Result<bool, String> {
    if !state.serial.is_connected() {
        emit_debug_traffic(&app, "TX", "", "CMD_PING — not connected, skipped");
        return Ok(false);
    }
    let local = state.serial.get_node_address().await;
    let pkt_hex = hex::encode(radio::build_ping(&local, &local));
    emit_debug_traffic(&app, "TX", &pkt_hex, "CMD_PING → device health check");
    let ok = state.serial.ping().await;
    emit_debug_traffic(
        &app, "RX", "",
        if ok { "✅ PONG — device responded within 500 ms 🐕" }
        else  { "❌ No PONG within 500 ms — firmware may be busy" },
    );
    Ok(ok)
}

#[tauri::command]
async fn generate_wallet() -> Result<WalletInfo, String> {
    wallet::generate_keypair().map_err(|e| e.to_string())
}

/// v0.3.6 — Import a Dogecoin wallet from a WIF-encoded private key.
/// Only compressed mainnet keys (starting with "Q") are supported.
/// ⚠️ Hot wallet warning — only use with test amounts.
#[tauri::command]
async fn import_wif(wif: String) -> Result<WalletInfo, String> {
    wallet::import_wif(&wif).map_err(|e| e.to_string())
}

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

    // v0.3.6 — Record to history
    {
        let entry = TxHistoryEntry {
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs(),
            to_address: tx.to_address.clone(),
            amount_doge: tx.amount_doge,
            memo: tx.memo.clone(),
            status: "sent".to_string(),
        };
        let mut hist = state.tx_history.lock().await;
        hist.insert(0, entry);
        hist.truncate(MAX_HISTORY);
        save_history_to_disk(&app, &hist).await;
    }

    log::info!("Transaction sent: {}", msg);
    Ok(msg)
}

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

    let freq_khz = (settings.frequency_mhz * 1000.0) as u32;
    let bw_idx: u8 = match settings.bandwidth_khz as u32 {
        250 => 1,
        500 => 2,
        _   => 0,
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

    let verified = state
        .serial
        .verify_node_address_set(&settings.node_address, 1500)
        .await;

    if verified {
        state.serial.update_node_address(settings.node_address.clone()).await;
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

#[tauri::command]
async fn get_lora_settings(state: State<'_, AppState>) -> Result<LoraSettings, String> {
    Ok(state.lora_settings.lock().await.clone())
}

/// v0.3.6 — Query board live state (CMD_GET_SETTINGS 0x22).
/// Board is source of truth — returns node address + gateway_mode as stored on board.
#[tauri::command]
async fn get_board_settings(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<Option<BoardSettings>, String> {
    if !state.serial.is_connected() {
        return Ok(None);
    }
    let src = state.serial.get_node_address().await;
    let pkt = radio::build_get_settings(&src);
    let pkt_hex = hex::encode(&pkt);
    emit_debug_traffic(&app, "TX", &pkt_hex, "CMD_GET_SETTINGS (0x22) — manual query");
    state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;
    tokio::time::sleep(Duration::from_millis(600)).await;
    Ok(state.serial.get_board_settings().await)
}

/// v0.3.6 — Set gateway mode on board (CMD_SET_GATEWAY 0x23).
/// Persists to NVS flash — survives power cycles. Board is source of truth.
#[tauri::command]
async fn set_gateway_mode(
    enable: bool,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<bool, String> {
    if !state.serial.is_connected() {
        return Err("Not connected to board — cannot set gateway mode.".to_string());
    }
    let src = state.serial.get_node_address().await;
    let pkt = radio::build_set_gateway(&src, enable);
    let pkt_hex = hex::encode(&pkt);
    emit_debug_traffic(&app, "TX", &pkt_hex, &format!("CMD_SET_GATEWAY (0x23) → {}", if enable { "ON" } else { "OFF" }));
    state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;
    tokio::time::sleep(Duration::from_millis(500)).await;

    // Re-query to confirm board saved the setting
    let confirm_pkt = radio::build_get_settings(&src);
    state.serial.send_raw(confirm_pkt).await.ok();
    tokio::time::sleep(Duration::from_millis(500)).await;
    let bs = state.serial.get_board_settings().await;
    let confirmed = bs.as_ref().map(|s| s.gateway_mode).unwrap_or(false);

    if let Some(ref bs) = bs {
        let _ = app.emit("board-sync", bs);
    }

    log::info!("Gateway mode set: enable={} confirmed={}", enable, confirmed);
    Ok(confirmed)
}

/// v0.3.6 — Transaction history (last MAX_HISTORY entries).
#[tauri::command]
async fn get_history(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<Vec<TxHistoryEntry>, String> {
    let hist = state.tx_history.lock().await.clone();
    if hist.is_empty() {
        // Try loading from disk on first call (e.g. app just started, no TX yet)
        let loaded = load_history_from_disk(&app).await;
        if !loaded.is_empty() {
            drop(hist);
            *state.tx_history.lock().await = loaded.clone();
            return Ok(loaded);
        }
    }
    Ok(hist)
}

/// v0.3.6 — Spawn `radiodoge-cli daemon -p <port>` as a background gateway process.
/// Emits "gateway-status" event on start.
#[tauri::command]
async fn start_gateway(
    port: String,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    // Stop any existing daemon first
    {
        let mut gp = state.gateway_process.lock().await;
        if let Some(ref mut child) = *gp {
            let _ = child.kill().await;
        }
        *gp = None;
    }

    let child = tokio::process::Command::new("radiodoge-cli")
        .args(["daemon", "-p", &port])
        .spawn()
        .map_err(|e| format!("Failed to spawn radiodoge-cli daemon: {}. Make sure radiodoge-cli is in PATH.", e))?;

    *state.gateway_process.lock().await = Some(child);
    let _ = app.emit("gateway-status", serde_json::json!({ "online": true, "port": port }));
    log::info!("Gateway daemon started on port {}", port);
    Ok(())
}

/// v0.3.6 — Stop the background gateway daemon.
#[tauri::command]
async fn stop_gateway(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    if let Some(mut child) = state.gateway_process.lock().await.take() {
        let _ = child.kill().await;
    }
    let _ = app.emit("gateway-status", serde_json::json!({ "online": false }));
    log::info!("Gateway daemon stopped");
    Ok(())
}

/// v0.3.6 — Get or set connection type ("usb" | "ble").
#[tauri::command]
async fn get_connection_type(state: State<'_, AppState>) -> Result<String, String> {
    Ok(state.connection_type.lock().await.clone())
}

#[tauri::command]
async fn set_connection_type(
    conn_type: String,
    state: State<'_, AppState>,
) -> Result<(), String> {
    if conn_type != "usb" && conn_type != "ble" {
        return Err("Connection type must be 'usb' or 'ble'".to_string());
    }
    *state.connection_type.lock().await = conn_type;
    Ok(())
}

// ─── App Builder ─────────────────────────────────────────────────────────────

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
            import_wif,
            send_transaction,
            update_lora_settings,
            get_lora_settings,
            get_board_settings,
            set_gateway_mode,
            get_history,
            start_gateway,
            stop_gateway,
            get_connection_type,
            set_connection_type,
        ])
        .setup(|app| {
            tray::setup_tray(app)?;
            log::info!("RadioDoge GUI v0.3.6 started — much wow 🐕");
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("Error running RadioDoge GUI")
}
