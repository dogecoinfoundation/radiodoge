//! RadioDoge GUI — Tauri 2 backend library.
//!
//! v0.3.10 additions:
//!   - Android USB serial via tauri-plugin-serialplugin (CP2102/CH340 over USB-OTG)
//!   - Android BLE via tauri-plugin-blec (btleplug on desktop, native BLE on Android)
//!   - Mobile packet bridge: JS feeds raw bytes → Rust frames packets → emits events
//!   - New commands: mobile_push_bytes, mobile_build_ping, mobile_build_connect_queries,
//!     mobile_build_tx_packets, mobile_build_lora_settings_packet,
//!     mobile_set_connected, mobile_set_disconnected, mobile_clear_accumulator,
//!     mobile_ble_scan, mobile_ble_connect, mobile_ble_disconnect,
//!     mobile_ble_write_characteristic
//!   - Platform detection via tauri-plugin-os
//!
//! v0.3.8: Battery voltage, board MAC, persistent wallet, address book.
//! v0.3.7: WiFi toggle, mesh neighbors, addr-conflict detection, ping fix.
//! v0.3.6: board-sync, WIF import, history, gateway mode, USB/BLE toggle.
//!
//! All protocol, wallet, and serial logic lives in `radiodoge-core`.

use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use tauri::{AppHandle, Emitter, Manager, State};
use tauri_plugin_notification::NotificationExt;
use tokio::sync::Mutex;

#[cfg(desktop)]
mod tray;

use radiodoge_core::{radio, wallet};
use hex;
use radiodoge_core::serial::SerialManager;
use radiodoge_core::types::{
    BoardSettings, ConnectionStatusEvent, IncomingPacket, LoraSettings, NeighborEntry,
    NodeAddress, PortInfo, RadioStats, TransactionRequest, TxHistoryEntry, WalletInfo,
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
    /// v0.3.6 — Connection type: "usb", "ble", or "usb-android"
    pub connection_type: Arc<Mutex<String>>,
    /// v0.3.10 — Raw byte accumulator for the Android USB bridge.
    /// The JS layer (using tauri-plugin-serialplugin) feeds received bytes here;
    /// Rust extracts complete packets using the same framing logic as the desktop
    /// serial read-loop and emits the standard "radio-packet" / "board-sync" events.
    pub mobile_accumulator: Arc<Mutex<Vec<u8>>>,
    /// v0.3.10 — Firmware version string cached during the Android USB connect
    /// sequence. Stored when GET_FIRMWARE_VERSION (0x20) response is received,
    /// included in the "connection-status: connected" event emitted after GET_SETTINGS.
    pub mobile_fw_version: Arc<Mutex<Option<String>>>,
    /// v0.3.10 — BLE device address (MAC) when connected via Bluetooth, None otherwise.
    /// Set by mobile_ble_connect, cleared by mobile_ble_disconnect.
    pub ble_device_address: Arc<Mutex<Option<String>>>,
    /// v0.3.15 — Running count of packets received via the Android mobile path.
    /// Used to synthesize `radio-stats-update` events so the Dashboard packet
    /// counters and NavBar signal bars stay current on mobile USB/BLE.
    pub mobile_packets_rx: Arc<Mutex<u32>>,
    /// v0.3.15 — Last known RSSI seen on the mobile path.
    /// Always 0 on USB (RSSI is not available over serial); may be non-zero
    /// in a future BLE path that reports received signal strength.
    pub mobile_last_rssi: Arc<Mutex<i16>>,
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
            mobile_accumulator: Arc::new(Mutex::new(Vec::new())),
            mobile_fw_version: Arc::new(Mutex::new(None)),
            ble_device_address: Arc::new(Mutex::new(None)),
            mobile_packets_rx: Arc::new(Mutex::new(0)),
            mobile_last_rssi: Arc::new(Mutex::new(0)),
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

        // v0.3.7 — Emit addr-conflict event to frontend when board detects duplicate addr
        if packet.command == radio::CMD_ADDR_CONFLICT {
            let addr_str = packet.decoded.clone().unwrap_or_else(|| "unknown".to_string());
            let _ = app_for_packets.emit("addr-conflict", serde_json::json!({
                "detected": true,
                "address": addr_str,
            }));
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

    #[cfg(desktop)]
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
                    #[cfg(desktop)]
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
    #[cfg(desktop)]
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

    // v0.3.7 — Find radiodoge-cli: check same dir as this executable first (bundled),
    // then fall back to PATH.  This eliminates "program not found" for packaged apps.
    let exe_dir = std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|d| d.to_path_buf()));

    let cli_name = if cfg!(windows) { "radiodoge-cli.exe" } else { "radiodoge-cli" };
    let cli_path = exe_dir
        .map(|d| d.join(cli_name))
        .filter(|p| p.exists())
        .map(|p| p.to_string_lossy().to_string())
        .unwrap_or_else(|| cli_name.to_string()); // fall back to PATH

    let child = tokio::process::Command::new(&cli_path)
        .args(["daemon", "-p", &port])
        .spawn()
        .map_err(|e| format!(
            "Failed to spawn '{}': {}. \
             Bundle radiodoge-cli next to the app executable, or add it to PATH.",
            cli_path, e
        ))?;

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

/// v0.3.7 — Enable or disable the WiFi radio on the board (CMD_WIFI_TOGGLE 0x24).
/// Setting persisted to NVS — survives power cycles.
#[tauri::command]
async fn set_wifi_enabled(
    enable: bool,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<bool, String> {
    if !state.serial.is_connected() {
        return Err("Not connected to board.".to_string());
    }
    let src = state.serial.get_node_address().await;
    let pkt = radio::build_wifi_toggle(&src, enable);
    let pkt_hex = hex::encode(&pkt);
    emit_debug_traffic(&app, "TX", &pkt_hex, &format!("CMD_WIFI_TOGGLE (0x24) → {}", if enable { "ON" } else { "OFF" }));
    state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;
    tokio::time::sleep(Duration::from_millis(600)).await;
    // Re-query to confirm
    let confirm = radio::build_get_settings(&src);
    state.serial.send_raw(confirm).await.ok();
    tokio::time::sleep(Duration::from_millis(500)).await;
    let bs = state.serial.get_board_settings().await;
    let confirmed = bs.as_ref().map(|s| s.wifi_enabled).unwrap_or(enable);
    if let Some(ref bs) = bs {
        let _ = app.emit("board-sync", bs);
    }
    Ok(confirmed)
}

/// v0.3.7 — Return recently heard mesh neighbors.
#[tauri::command]
async fn get_neighbors(state: State<'_, AppState>) -> Result<Vec<NeighborEntry>, String> {
    Ok(state.serial.get_neighbors().await)
}

/// v0.3.7 — Whether the board has reported a duplicate node address.
#[tauri::command]
async fn get_addr_conflict(state: State<'_, AppState>) -> Result<bool, String> {
    Ok(state.serial.has_addr_conflict())
}

/// v0.3.7 — Dismiss the address-conflict warning (user acknowledged).
#[tauri::command]
async fn clear_addr_conflict(state: State<'_, AppState>) -> Result<(), String> {
    state.serial.clear_addr_conflict();
    Ok(())
}

// ─── v0.3.8: Battery + MAC ────────────────────────────────────────────────────

/// v0.3.8 — Query board battery voltage (CMD_GET_BATTERY 0x26).
/// Sends the command and waits up to 600 ms for the board to reply.
/// Returns voltage in millivolts, or None if not connected / no reply.
#[tauri::command]
async fn query_battery(state: State<'_, AppState>, app: AppHandle) -> Result<Option<u16>, String> {
    if !state.serial.is_connected() {
        return Ok(None);
    }
    let src = state.serial.get_node_address().await;
    let pkt = radio::build_get_battery(&src);
    let pkt_hex = hex::encode(&pkt);
    emit_debug_traffic(&app, "TX", &pkt_hex, "CMD_GET_BATTERY (0x26)");
    state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;
    tokio::time::sleep(Duration::from_millis(600)).await;
    Ok(state.serial.get_battery_mv().await)
}

/// v0.3.8 — Query board MAC address (CMD_GET_MAC 0x27).
/// Returns the WiFi station MAC as a colon-separated hex string, or None if unavailable.
#[tauri::command]
async fn query_mac(state: State<'_, AppState>, app: AppHandle) -> Result<Option<String>, String> {
    if !state.serial.is_connected() {
        return Ok(None);
    }
    let src = state.serial.get_node_address().await;
    let pkt = radio::build_get_mac(&src);
    let pkt_hex = hex::encode(&pkt);
    emit_debug_traffic(&app, "TX", &pkt_hex, "CMD_GET_MAC (0x27)");
    state.serial.send_raw(pkt).await.map_err(|e| e.to_string())?;
    tokio::time::sleep(Duration::from_millis(400)).await;
    Ok(state.serial.get_board_mac().await)
}

// ─── v0.3.8: Persistent Wallet ───────────────────────────────────────────────

/// v0.3.8 — Persist the current wallet to app-local storage (best-effort, unencrypted).
/// ⚠️ This is a HOT wallet — only use for small test amounts!
/// The user must confirm with the phrase "THIS IS MUCH INSECURE" in the UI before calling this.
#[tauri::command]
async fn save_wallet(wallet_info: WalletInfo, app: AppHandle) -> Result<(), String> {
    let dir = app.path().app_data_dir().map_err(|e| e.to_string())?;
    tokio::fs::create_dir_all(&dir).await.map_err(|e| e.to_string())?;
    let path = dir.join("wallet.json");
    let json = serde_json::to_string_pretty(&wallet_info).map_err(|e| e.to_string())?;
    tokio::fs::write(&path, json).await.map_err(|e| e.to_string())?;
    log::warn!("Wallet saved to disk — hot wallet, use with care!");
    Ok(())
}

/// v0.3.8 — Load a previously saved wallet from app-local storage.
/// Returns None if no wallet has been saved.
#[tauri::command]
async fn load_saved_wallet(app: AppHandle) -> Result<Option<WalletInfo>, String> {
    let path = match app.path().app_data_dir() {
        Ok(p) => p.join("wallet.json"),
        Err(_) => return Ok(None),
    };
    match tokio::fs::read_to_string(&path).await {
        Ok(json) => {
            let w: WalletInfo = serde_json::from_str(&json).map_err(|e| e.to_string())?;
            Ok(Some(w))
        }
        Err(_) => Ok(None), // File doesn't exist
    }
}

/// v0.3.8 — Delete the saved wallet from disk (user explicitly cleared it).
#[tauri::command]
async fn delete_saved_wallet(app: AppHandle) -> Result<(), String> {
    let path = match app.path().app_data_dir() {
        Ok(p) => p.join("wallet.json"),
        Err(_) => return Ok(()),
    };
    let _ = tokio::fs::remove_file(&path).await;
    Ok(())
}

// ─── v0.3.8: Address Book ────────────────────────────────────────────────────

/// v0.3.8 — Load the address book from app-local storage.
/// Returns an empty array if no address book has been saved.
#[tauri::command]
async fn load_address_book(app: AppHandle) -> Result<Vec<serde_json::Value>, String> {
    let path = match app.path().app_data_dir() {
        Ok(p) => p.join("address_book.json"),
        Err(_) => return Ok(vec![]),
    };
    match tokio::fs::read_to_string(&path).await {
        Ok(json) => Ok(serde_json::from_str(&json).unwrap_or_default()),
        Err(_) => Ok(vec![]),
    }
}

/// v0.3.8 — Save the address book to app-local storage.
#[tauri::command]
async fn save_address_book(entries: Vec<serde_json::Value>, app: AppHandle) -> Result<(), String> {
    let dir = app.path().app_data_dir().map_err(|e| e.to_string())?;
    tokio::fs::create_dir_all(&dir).await.map_err(|e| e.to_string())?;
    let path = dir.join("address_book.json");
    let json = serde_json::to_string_pretty(&entries).map_err(|e| e.to_string())?;
    tokio::fs::write(&path, json).await.map_err(|e| e.to_string())?;
    Ok(())
}

// ─── v0.3.10: Android USB Serial Bridge ──────────────────────────────────────
//
// On Android, the `serialport` crate cannot open USB-OTG devices — that
// requires Android's USB Host API via Java. `tauri-plugin-serialplugin` wraps
// `usb-serial-for-android` and exposes a JavaScript API the frontend can call.
//
// The mobile bridge pattern:
//   1. JS lists USB devices  → SerialPort.available_ports()
//   2. JS opens the port     → SerialPort.open()  [triggers Android permission dialog]
//   3. JS writes bytes       → SerialPort.write()
//   4. JS receives bytes     → listener callback → calls mobile_push_bytes()
//   5. Rust frames packets   → same logic as desktop read-loop
//   6. Rust emits events     → "radio-packet", "board-sync", "connection-status", …
//
// All packet-building uses the existing radio:: functions so there is zero
// protocol duplication between platforms.

/// Notify the Rust backend that an Android USB serial connection is opening.
/// Sets `current_port` and emits "connection-status: connecting" so the
/// frontend (which listens to the same event as on desktop) updates its state.
#[tauri::command]
async fn mobile_set_connected(
    device_path: String,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    *state.current_port.lock().await = Some(device_path.clone());
    *state.connection_type.lock().await = "usb-android".to_string();
    // Clear any stale firmware version from a previous session
    *state.mobile_fw_version.lock().await = None;
    state.mobile_accumulator.lock().await.clear();
    let _ = app.emit("connection-status", ConnectionStatusEvent::connecting(&device_path));
    log::info!("mobile_set_connected: {}", device_path);
    Ok(())
}

/// Notify the Rust backend that the Android USB connection has been closed.
/// Clears state and emits "connection-status: disconnected".
#[tauri::command]
async fn mobile_set_disconnected(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    *state.current_port.lock().await = None;
    *state.mobile_fw_version.lock().await = None;
    state.mobile_accumulator.lock().await.clear();
    *state.mobile_packets_rx.lock().await = 0;
    *state.mobile_last_rssi.lock().await = 0;
    let _ = app.emit("connection-status", ConnectionStatusEvent::disconnected());
    log::info!("mobile_set_disconnected");
    Ok(())
}

/// Discard all buffered bytes in the Android accumulator (called on disconnect).
#[tauri::command]
async fn mobile_clear_accumulator(state: State<'_, AppState>) -> Result<(), String> {
    state.mobile_accumulator.lock().await.clear();
    Ok(())
}

/// Feed raw bytes received from the Android USB serial plugin into the packet
/// accumulator. Extracts complete RadioDoge packets using the same framing
/// logic as the desktop serial read-loop:
///
/// - Unknown command bytes at the front are discarded (sync recovery)
/// - Fixed-length commands advance by `exact_packet_len()` bytes
/// - Variable-length commands (messages, FW version) consume up to MAX_SINGLE_PAYLOAD_LEN
///
/// For each complete packet this function:
/// - Emits "radio-packet" to the frontend
/// - On CMD_GET_FIRMWARE_VERSION: caches the version string
/// - On CMD_GET_SETTINGS: emits "board-sync" + "connection-status: connected"
/// - On CMD_DOGE_TX: emits "mobile-doge-tx-received" notification event
/// - On CMD_ADDR_CONFLICT: emits "addr-conflict"
///
/// Returns the number of complete packets extracted (useful for debug logging).
#[tauri::command]
async fn mobile_push_bytes(
    bytes: Vec<u8>,
    rssi: i16,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<u32, String> {
    // Same set of known first-byte values as the desktop read-loop
    const KNOWN_CMDS: &[u8] = &[
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x10, 0x11,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x3F, 0x62, 0x64, 0x68, 0x6D, 0xFE,
    ];

    let mut acc = state.mobile_accumulator.lock().await;
    acc.extend_from_slice(&bytes);

    let mut packets_extracted: u32 = 0;

    loop {
        // Discard leading bytes whose first byte is not a known command (sync recovery).
        // This mirrors the desktop loop's behavior for stale firmware debug output.
        while !acc.is_empty() && !KNOWN_CMDS.contains(&acc[0]) {
            acc.remove(0);
        }
        if acc.len() < radio::SINGLE_HDR_LEN {
            break; // Need more bytes
        }

        let packet = match radio::parse_incoming(&acc, rssi) {
            Some(p) => p,
            None => break,
        };

        let cmd = packet.command;
        packets_extracted += 1;

        // Emit to UI (same event as desktop)
        let _ = app.emit("radio-packet", &packet);
        emit_debug_traffic(
            &app,
            "RX",
            &packet.payload_hex,
            packet.decoded.as_deref().unwrap_or(""),
        );

        // Handle specific packet types
        match cmd {
            radio::CMD_GET_FIRMWARE_VERSION => {
                if let Ok(raw) = hex::decode(&packet.payload_hex) {
                    if let Ok(ver) = String::from_utf8(raw) {
                        let ver = ver.trim_matches('\0').trim().to_string();
                        if !ver.is_empty() {
                            log::info!("Mobile: firmware version = {}", ver);
                            *state.mobile_fw_version.lock().await = Some(ver.clone());
                            // Emit so the frontend can display the badge without waiting
                            let _ = app.emit("mobile-firmware-version", &ver);
                        }
                    }
                }
            }

            radio::CMD_GET_SETTINGS => {
                // Payload: [region, community, node, gateway_mode, wifi_enabled(v0.3.7)]
                if let Ok(payload) = hex::decode(&packet.payload_hex) {
                    if payload.len() >= 4 {
                        let addr = NodeAddress::new(payload[0], payload[1], payload[2]);
                        let gw   = payload[3] != 0;
                        let wifi = payload.get(4).map(|&b| b != 0).unwrap_or(true);

                        // Only trust the addr if it is not the broadcast address
                        if addr != NodeAddress::broadcast() {
                            state.serial.update_node_address(addr.clone()).await;
                        }

                        let bs = BoardSettings {
                            node_address: addr.clone(),
                            gateway_mode: gw,
                            wifi_enabled: wifi,
                        };
                        let _ = app.emit("board-sync", &bs);

                        // Now emit "connected" — we have everything we need.
                        // Include the firmware version if it arrived before settings.
                        let fw = state.mobile_fw_version.lock().await.clone();
                        let port = state.current_port.lock().await.clone()
                            .unwrap_or_else(|| "usb-android".to_string());
                        let _ = app.emit(
                            "connection-status",
                            ConnectionStatusEvent::connected(&port, addr, fw),
                        );
                        log::info!("Mobile: board sync complete, emitting connected");
                    }
                }
            }

            radio::CMD_DOGE_TX => {
                // No system-tray notification on Android; emit an in-app event instead
                let body = packet.decoded.clone()
                    .unwrap_or_else(|| "Incoming Dogecoin transaction over LoRa".to_string());
                let _ = app.emit(
                    "mobile-doge-tx-received",
                    serde_json::json!({ "body": body }),
                );
            }

            radio::CMD_ADDR_CONFLICT => {
                let addr_str = packet.decoded.clone().unwrap_or_else(|| "unknown".to_string());
                let _ = app.emit("addr-conflict", serde_json::json!({
                    "detected": true,
                    "address": addr_str,
                }));
            }

            _ => {}
        }

        // Advance the accumulator by exactly the right number of bytes.
        // For fixed-size commands use the precise length; for variable-length
        // commands consume the header + up to MAX_SINGLE_PAYLOAD_LEN bytes.
        let consumed = radio::exact_packet_len(cmd).unwrap_or_else(|| {
            radio::SINGLE_HDR_LEN
                + (acc.len() - radio::SINGLE_HDR_LEN).min(radio::MAX_SINGLE_PAYLOAD_LEN)
        });
        let consumed = consumed.min(acc.len());
        acc.drain(..consumed);
    }

    // v0.3.15 — Emit radio-stats-update so the Dashboard packet counters and
    // NavBar signal bars stay current on mobile (USB/BLE).  On USB the RSSI
    // is always 0 (serial carries no signal-strength metadata), but the
    // packets_received counter will now increment correctly.
    if packets_extracted > 0 {
        let mut rx_count = state.mobile_packets_rx.lock().await;
        *rx_count += packets_extracted;
        let count_snap = *rx_count;
        drop(rx_count);

        // Accept a non-zero RSSI from the caller (future BLE path may supply it).
        if rssi != 0 {
            *state.mobile_last_rssi.lock().await = rssi;
        }
        let rssi_snap = *state.mobile_last_rssi.lock().await;

        let lora = state.lora_settings.lock().await.clone();
        let stats = RadioStats {
            frequency_mhz: lora.frequency_mhz,
            power_dbm: lora.power_dbm,
            spreading_factor: lora.spreading_factor,
            bandwidth_khz: lora.bandwidth_khz,
            coding_rate: lora.coding_rate.clone(),
            rssi: rssi_snap,
            packets_sent: 0,
            packets_received: count_snap,
        };
        let _ = app.emit("radio-stats-update", &stats);
    }

    Ok(packets_extracted)
}

/// Build a raw PING packet for the Android USB bridge to write directly to serial.
/// Uses the current node address from AppState (synced from board on connect).
#[tauri::command]
async fn mobile_build_ping(state: State<'_, AppState>) -> Result<Vec<u8>, String> {
    let src = state.serial.get_node_address().await;
    Ok(radio::build_ping(&src, &NodeAddress::broadcast()))
}

/// Build the two-packet connect sequence for the Android USB bridge:
///   [0] GET_FIRMWARE_VERSION (0x20)
///   [1] GET_SETTINGS         (0x22)
///
/// The JS layer sends these in order after opening the serial port.
/// When the board replies, `mobile_push_bytes` processes the responses and
/// emits "mobile-firmware-version" and "connection-status: connected".
#[tauri::command]
async fn mobile_build_connect_queries(state: State<'_, AppState>) -> Result<Vec<Vec<u8>>, String> {
    let src = state.serial.get_node_address().await;
    Ok(vec![
        radio::build_get_firmware_version(&src),
        radio::build_get_settings(&src),
    ])
}

/// Build Dogecoin transaction packet(s) for the Android USB bridge.
///
/// Returns a `Vec<Vec<u8>>` — each inner Vec is one complete packet to write
/// in order. Payloads ≤ 192 bytes produce a single packet; larger payloads
/// are split into a multipart sequence (identical to the desktop send path).
#[tauri::command]
async fn mobile_build_tx_packets(
    tx: TransactionRequest,
    state: State<'_, AppState>,
) -> Result<Vec<Vec<u8>>, String> {
    let src = state.serial.get_node_address().await;
    let dst = NodeAddress::broadcast();

    // encode_transaction_payload signature: (to_address, amount_doge, memo)
    let payload = wallet::encode_transaction_payload(
        &tx.to_address,
        tx.amount_doge,
        tx.memo.as_deref(),
    )
    .map_err(|e| e.to_string())?;

    if payload.len() <= radio::MAX_SINGLE_PAYLOAD_LEN {
        Ok(vec![radio::build_doge_tx(&src, &dst, &payload)])
    } else {
        Ok(radio::build_multipart_packets(&src, &dst, radio::CMD_DOGE_TX, &payload))
    }
}

/// Build a SET_LORA_PARAMS packet (CMD 0x21) for the Android USB bridge.
/// Mirrors the desktop `update_lora_settings` command but returns raw bytes
/// instead of writing them to the (non-existent on Android) Rust serial handle.
#[tauri::command]
async fn mobile_build_lora_settings_packet(settings: LoraSettings) -> Vec<u8> {
    let bw_idx: u8 = match settings.bandwidth_khz as u32 {
        250 => 1,
        500 => 2,
        _   => 0, // default 125 kHz
    };
    let cr: u8 = match settings.coding_rate.as_str() {
        "4/6" => 6,
        "4/7" => 7,
        "4/8" => 8,
        _     => 5, // default 4/5
    };
    let freq_khz = (settings.frequency_mhz * 1000.0) as u32;
    radio::build_set_lora_params(
        &settings.node_address,
        settings.spreading_factor,
        bw_idx,
        cr,
        freq_khz,
        settings.power_dbm as u8,
    )
}

// ─── v0.3.10 Android BLE commands ────────────────────────────────────────────
//
// Architecture:
//   The tauri-plugin-blec plugin provides the actual BLE transport via its own
//   IPC commands (scan, connect, sendData, onReceiveData).  The four commands
//   below handle the *RadioDoge-side state machine* that wraps each BLE operation:
//
//   • mobile_ble_scan      — update connection_type, emit ble-scan-started event
//   • mobile_ble_connect   — set ble_device_address, emit "connection-status: connecting"
//   • mobile_ble_disconnect — clear BLE state, emit "connection-status: disconnected"
//   • mobile_ble_write_characteristic — log outgoing bytes in the debug traffic stream
//
//   Incoming BLE data:
//     blec plugin → JS onReceiveData callback → invoke('mobile_push_bytes', …)
//     → same accumulator + parser path as USB-OTG → same "radio-packet" / "board-sync"
//     events consumed by the existing frontend code.
//   No protocol duplication between BLE and USB paths.

/// Notify Rust that a BLE device scan is starting.
/// Updates connection_type and emits "ble-scan-started" so the frontend can
/// animate a scan spinner independently of the mobileRefreshing state.
///
/// The actual scan results come from the blec plugin's JS `scan(timeoutMs)`
/// function; Rust receives them only if cached via future state extensions.
#[tauri::command]
async fn mobile_ble_scan(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    *state.connection_type.lock().await = "ble-android".to_string();
    let _ = app.emit("ble-scan-started", serde_json::json!({}));
    log::info!("mobile_ble_scan: BLE scan initiated");
    Ok(())
}

/// Notify Rust that a BLE connection to `address` is being opened.
///
/// - Caches the device MAC address in `ble_device_address`
/// - Sets `connection_type` = "ble-android"
/// - Clears the mobile accumulator and cached firmware version
/// - Emits "connection-status: connecting" (identical event shape to USB/desktop)
///
/// The JS layer then calls the blec plugin's `connect(address)` IPC,
/// subscribes to characteristic notifications (→ mobile_push_bytes on each chunk),
/// and sends the GET_FIRMWARE_VERSION + GET_SETTINGS connect queries.
#[tauri::command]
async fn mobile_ble_connect(
    address: String,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    *state.current_port.lock().await = Some(address.clone());
    *state.connection_type.lock().await = "ble-android".to_string();
    *state.ble_device_address.lock().await = Some(address.clone());
    *state.mobile_fw_version.lock().await = None;
    state.mobile_accumulator.lock().await.clear();
    let _ = app.emit("connection-status", ConnectionStatusEvent::connecting(&address));
    log::info!("mobile_ble_connect: {}", address);
    Ok(())
}

/// Notify Rust that the BLE connection has been closed.
///
/// Mirrors mobile_set_disconnected but for the BLE transport:
/// clears ble_device_address, current_port, mobile_fw_version, accumulator,
/// and emits "connection-status: disconnected".
#[tauri::command]
async fn mobile_ble_disconnect(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    *state.current_port.lock().await = None;
    *state.ble_device_address.lock().await = None;
    *state.mobile_fw_version.lock().await = None;
    state.mobile_accumulator.lock().await.clear();
    let _ = app.emit("connection-status", ConnectionStatusEvent::disconnected());
    log::info!("mobile_ble_disconnect");
    Ok(())
}

/// Log bytes being written to the BLE TX characteristic in the debug traffic stream.
///
/// The actual GATT write is performed by the JS layer via the blec plugin's
/// `sendData(serviceUuid, charUuid, data)` API.  This command exists to:
///   1. Emit a "TX-BLE" debug-serial-traffic event visible in the Debug Console
///   2. Provide a single place to add future Rust-side flow-control / rate-limiting
#[tauri::command]
async fn mobile_ble_write_characteristic(
    data: Vec<u8>,
    app: AppHandle,
) -> Result<(), String> {
    let hex = hex::encode(&data);
    emit_debug_traffic(&app, "TX-BLE", &hex, "BLE write characteristic");
    Ok(())
}

// ─── App Builder ─────────────────────────────────────────────────────────────

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    env_logger::init();

    tauri::Builder::default()
        // Core plugins
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_notification::init())
        // v0.3.10 — Android USB serial bridge (usb-serial-for-android under the hood)
        // Also available on desktop (no-op for the primary serial path, but provides
        // the JS API surface that connection-bridge.ts calls on Android).
        .plugin(tauri_plugin_serialplugin::init())
        // v0.3.10 — Platform detection (returns "android", "windows", "linux", …)
        .plugin(tauri_plugin_os::init())
        // v0.3.10 — BLE client: btleplug on desktop, native Android BLE on mobile.
        // Exposes scan / connect / sendData / onReceiveData to the JS layer.
        .plugin(tauri_plugin_blec::init())
        .manage(AppState::new())
        .invoke_handler(tauri::generate_handler![
            // ── Desktop serial (existing) ─────────────────────────────────
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
            set_wifi_enabled,
            get_neighbors,
            get_addr_conflict,
            clear_addr_conflict,
            query_battery,
            query_mac,
            save_wallet,
            load_saved_wallet,
            delete_saved_wallet,
            load_address_book,
            save_address_book,
            // ── v0.3.10 Android USB bridge ────────────────────────────────
            // JS layer handles physical serial I/O; Rust handles framing + protocol
            mobile_set_connected,
            mobile_set_disconnected,
            mobile_clear_accumulator,
            mobile_push_bytes,
            mobile_build_ping,
            mobile_build_connect_queries,
            mobile_build_tx_packets,
            mobile_build_lora_settings_packet,
            // ── v0.3.10 Android BLE bridge ────────────────────────────────
            // blec plugin handles GATT transport; Rust handles state + debug logging
            mobile_ble_scan,
            mobile_ble_connect,
            mobile_ble_disconnect,
            mobile_ble_write_characteristic,
        ])
        .setup(|app| {
            #[cfg(desktop)]
            tray::setup_tray(app)?;
            log::info!("RadioDoge GUI v0.3.15 started — much mesh, very wow 🐕");
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("Error running RadioDoge GUI")
}
