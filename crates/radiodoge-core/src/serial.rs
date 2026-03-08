//! Serial port management for Heltec LoRa module communication.
//!
//! Cross-platform (Windows, Linux, macOS; Android via serialport-android later).
//! All blocking I/O is wrapped in `tokio::task::spawn_blocking` so the async
//! runtime is never blocked — this was a bug in earlier versions that caused
//! UI lag and potential deadlocks.
//!
//! ## Thread-safety model
//!
//! - `port` and `cancel` use `std::sync::Mutex` so they can be locked inside
//!   `spawn_blocking` threads without involving the Tokio scheduler.
//! - `stats` and `node_address` use `tokio::sync::Mutex` since they are only
//!   ever accessed from async contexts.

use std::io::{Read, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex as StdMutex};
use std::time::Duration;

use anyhow::{Context, Result};
use tokio::sync::{broadcast, Mutex as TokioMutex};
use tokio_util::sync::CancellationToken;

use crate::radio;
use crate::types::{BoardSettings, IncomingPacket, NodeAddress, PortInfo, RadioStats};

// ─── USB VID/PID tables for common ESP32 / Heltec serial adapters ────────────

/// Silicon Labs CP210x (CP2102, CP2104 etc.) — the most common adapter on
/// Heltec WiFi LoRa 32 V2 boards.
const CP210X_VID: u16 = 0x10C4;
const CP210X_PID: u16 = 0xEA60;

/// Jiangsu Qinheng CH340 / CH341 — common on cheap ESP32 dev boards.
const CH340_VID: u16 = 0x1A86;
const CH340_PID: u16 = 0x7523;

/// CH9102 — newer Qinheng chip found on some Heltec V3 boards.
const CH9102_VID: u16 = 0x1A86;
const CH9102_PID: u16 = 0x55D4;

/// Espressif built-in USB-CDC (ESP32-S2, S3, C3 native USB).
const ESP_NATIVE_VID: u16 = 0x303A;

/// FTDI FT232 — less common on Heltec but used by some breakout boards.
const FTDI_VID: u16 = 0x0403;

/// Returns `true` if the VID/PID matches a USB serial adapter commonly found
/// on Heltec ESP32 LoRa boards.
fn is_likely_heltec_vid_pid(vid: u16, pid: u16) -> bool {
    matches!(
        (vid, pid),
        (CP210X_VID, CP210X_PID)
            | (CH340_VID, CH340_PID)
            | (CH9102_VID, CH9102_PID)
            | (ESP_NATIVE_VID, _)
            | (FTDI_VID, _)
    )
}

/// Baud rate for Heltec LoRa module serial communication
const BAUD_RATE: u32 = 115_200;

/// Timeout for each serial read call (short, so the read loop stays responsive)
const READ_TIMEOUT_MS: u64 = 50;

/// Maximum bytes per read call
const READ_BUFFER_SIZE: usize = 1024;

/// Shared application-level state for one serial connection.
///
/// Clone the `Arc<SerialManager>` to share across tasks; the inner data is
/// behind mutexes and atomics.
pub struct SerialManager {
    /// The open serial port.
    ///
    /// `StdMutex` (not tokio) so it can be locked inside `spawn_blocking`
    /// without blocking the async executor.
    port: Arc<StdMutex<Option<Box<dyn serialport::SerialPort>>>>,

    /// Whether a port is currently open and the read loop is running.
    connected: Arc<AtomicBool>,

    /// Broadcast channel: every incoming packet is sent here so multiple
    /// consumers (GUI, CLI watch loop, ping waiter) can all receive it.
    packet_tx: broadcast::Sender<IncomingPacket>,

    /// Cancellation token for the read loop.
    ///
    /// `StdMutex` so `.cancel()` can be called from either sync or async code.
    cancel: Arc<StdMutex<Option<CancellationToken>>>,

    /// Cumulative radio statistics updated as packets flow in and out.
    stats: Arc<TokioMutex<RadioStats>>,

    /// The LoRa mesh address reported by the Heltec device on connect.
    node_address: Arc<TokioMutex<NodeAddress>>,

    /// Firmware version string, populated after CMD_GET_FIRMWARE_VERSION response.
    firmware_version: Arc<TokioMutex<Option<String>>>,

    /// v0.3.6 — Board settings, populated after CMD_GET_SETTINGS (0x22) response.
    /// Board is source of truth — app queries on connect and syncs UI.
    board_settings: Arc<TokioMutex<Option<BoardSettings>>>,
}

impl SerialManager {
    pub fn new() -> Self {
        let (packet_tx, _) = broadcast::channel(128);
        SerialManager {
            port: Arc::new(StdMutex::new(None)),
            connected: Arc::new(AtomicBool::new(false)),
            packet_tx,
            cancel: Arc::new(StdMutex::new(None)),
            stats: Arc::new(TokioMutex::new(RadioStats::default())),
            node_address: Arc::new(TokioMutex::new(NodeAddress::default_local())),
            firmware_version: Arc::new(TokioMutex::new(None)),
            board_settings: Arc::new(TokioMutex::new(None)),
        }
    }

    /// List all serial ports visible to the OS (names only, for backward compat).
    ///
    /// USB serial devices (CP210x, CH340, etc.) are sorted first.
    /// Prefer [`list_ports_with_info`] for the rich UI representation.
    pub fn list_ports() -> Vec<String> {
        Self::list_ports_with_info()
            .into_iter()
            .map(|p| p.name)
            .collect()
    }

    /// List all serial ports with rich USB device information.
    ///
    /// - USB serial adapters (CP210x, CH340, CH9102, FTDI, Espressif native) appear first.
    /// - Known Heltec/ESP32 adapters are flagged with `is_likely_heltec = true`.
    /// - Ports are sorted: likely-Heltec first, then other USB, then non-USB.
    pub fn list_ports_with_info() -> Vec<PortInfo> {
        let raw = match serialport::available_ports() {
            Ok(p) => p,
            Err(e) => {
                log::warn!("Failed to list serial ports: {}", e);
                return vec![];
            }
        };

        let mut ports: Vec<PortInfo> = raw
            .into_iter()
            .map(|p| {
                let name = p.port_name.clone();

                match p.port_type {
                    serialport::SerialPortType::UsbPort(usb) => {
                        let vid = usb.vid;
                        let pid = usb.pid;
                        let likely = is_likely_heltec_vid_pid(vid, pid);

                        let mfr = usb.manufacturer.clone();
                        let prod = usb.product.clone();

                        // Build a short human-readable label: "COM3 — CP2102 USB to UART"
                        let adapter_label = prod
                            .clone()
                            .or_else(|| mfr.clone())
                            .unwrap_or_else(|| format!("USB VID:{:04X} PID:{:04X}", vid, pid));

                        let description = format!("{} — {}", name, adapter_label);

                        PortInfo {
                            name,
                            is_usb: true,
                            manufacturer: mfr,
                            product: prod,
                            vid: Some(vid),
                            pid: Some(pid),
                            description,
                            is_likely_heltec: likely,
                        }
                    }
                    _ => PortInfo {
                        description: name.clone(),
                        name,
                        is_usb: false,
                        manufacturer: None,
                        product: None,
                        vid: None,
                        pid: None,
                        is_likely_heltec: false,
                    },
                }
            })
            .collect();

        // Sort: likely-Heltec USB ports first, then other USB, then non-USB.
        ports.sort_by_key(|p| {
            if p.is_likely_heltec { 0u8 } else if p.is_usb { 1 } else { 2 }
        });

        ports
    }

    /// `true` if a port is open and the read loop is running.
    pub fn is_connected(&self) -> bool {
        self.connected.load(Ordering::Relaxed)
    }

    /// Snapshot of radio statistics.
    pub async fn get_stats(&self) -> RadioStats {
        self.stats.lock().await.clone()
    }

    /// The LoRa mesh address of the connected device (e.g. `10.0.1`).
    pub async fn get_node_address(&self) -> NodeAddress {
        self.node_address.lock().await.clone()
    }

    /// Subscribe to the incoming packet broadcast channel.
    pub fn subscribe_packets(&self) -> broadcast::Receiver<IncomingPacket> {
        self.packet_tx.subscribe()
    }

    /// Open `port_name` and start the background read loop.
    ///
    /// `on_packet` is called (on the Tokio thread pool) for every complete
    /// packet received. It must be `Send + Sync` because it is invoked from
    /// an async task.
    ///
    /// Immediately sends `GET_NODE_ADDR` to the device so the node address
    /// is populated as quickly as possible.
    pub async fn connect(
        &self,
        port_name: &str,
        on_packet: Arc<dyn Fn(IncomingPacket) + Send + Sync>,
    ) -> Result<()> {
        if self.is_connected() {
            self.disconnect().await?;
        }

        log::info!("Opening serial port {} at {} baud", port_name, BAUD_RATE);

        let port = serialport::new(port_name, BAUD_RATE)
            .timeout(Duration::from_millis(READ_TIMEOUT_MS))
            .open()
            .with_context(|| format!("Failed to open serial port {}", port_name))?;

        // Store port with std mutex — no await needed
        *self.port.lock().expect("port mutex poisoned") = Some(port);
        self.connected.store(true, Ordering::Relaxed);

        // Cancellation token for the read loop
        let token = CancellationToken::new();
        *self.cancel.lock().expect("cancel mutex poisoned") = Some(token.clone());

        // ── Spawn the background read loop ──────────────────────────────────
        let port_clone = Arc::clone(&self.port);
        let connected_clone = Arc::clone(&self.connected);
        let stats_clone = Arc::clone(&self.stats);
        let node_address_clone = Arc::clone(&self.node_address);
        let firmware_version_clone = Arc::clone(&self.firmware_version);
        let board_settings_clone = Arc::clone(&self.board_settings);
        let packet_tx_clone = self.packet_tx.clone();

        tokio::spawn(async move {
            log::info!("Serial read loop started");
            let mut accumulator: Vec<u8> = Vec::with_capacity(512);

            loop {
                if token.is_cancelled() {
                    log::info!("Serial read loop cancelled");
                    break;
                }

                // ── FIXED: use spawn_blocking so blocking read never stalls Tokio ──
                // Earlier versions called port.read() while holding a tokio::sync::Mutex
                // guard, which blocked the entire async executor. This is now correct.
                let port_for_read = Arc::clone(&port_clone);
                let read_result = tokio::task::spawn_blocking(move || {
                    let mut guard = port_for_read.lock().expect("port mutex poisoned");
                    if let Some(port) = guard.as_mut() {
                        let mut buf = vec![0u8; READ_BUFFER_SIZE];
                        match port.read(&mut buf) {
                            Ok(0) => Ok(vec![]),
                            Ok(n) => Ok(buf[..n].to_vec()),
                            Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => Ok(vec![]),
                            Err(e) => Err(e.to_string()),
                        }
                    } else {
                        // Port was closed (disconnect called)
                        Err("port_closed".to_string())
                    }
                })
                .await;

                match read_result {
                    Err(join_err) => {
                        log::error!("Read task panicked: {}", join_err);
                        connected_clone.store(false, Ordering::Relaxed);
                        break;
                    }
                    Ok(Err(ref e)) if e == "port_closed" => {
                        log::info!("Port closed — exiting read loop");
                        break;
                    }
                    Ok(Err(e)) => {
                        log::error!("Serial read error: {}", e);
                        connected_clone.store(false, Ordering::Relaxed);
                        break;
                    }
                    Ok(Ok(bytes)) if bytes.is_empty() => {
                        // Timeout — yield to other async tasks
                        tokio::time::sleep(Duration::from_millis(5)).await;
                    }
                    Ok(Ok(bytes)) => {
                        accumulator.extend_from_slice(&bytes);

                        // Extract complete packets from the accumulator.
                        // OPTIMIZED FOR DESKTOP v0.3.3 – SAFE
                        // If the first byte is not a known command, discard it and re-sync.
                        // This prevents stale firmware text output (e.g. Serial.println debug)
                        // from being misinterpreted as packet headers.
                        const KNOWN_CMDS: &[u8] = &[
                            0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                            0x10, 0x11, 0x20, 0x21, 0x22, 0x23,  // 0x22/0x23 = v0.3.6
                            0x3F, 0x62, 0x64, 0x68, 0x6D, 0xFE, // firmware-side IDs
                        ];
                        while !accumulator.is_empty() {
                            if !KNOWN_CMDS.contains(&accumulator[0]) {
                                accumulator.remove(0);
                                continue;
                            }
                            if accumulator.len() < radio::SINGLE_HDR_LEN {
                                break;
                            }
                            if let Some(packet) = radio::parse_incoming(&accumulator, 0) {
                                // Update stats
                                {
                                    let mut stats = stats_clone.lock().await;
                                    stats.packets_received += 1;
                                }

                                // Auto-update node address from GET_NODE_ADDR responses
                                if packet.command == radio::CMD_GET_NODE_ADDR
                                    && packet.source != NodeAddress::broadcast()
                                {
                                    *node_address_clone.lock().await = packet.source.clone();
                                }

                                // Parse firmware version from CMD_GET_FIRMWARE_VERSION responses
                                if packet.command == radio::CMD_GET_FIRMWARE_VERSION {
                                    if let Ok(bytes) = hex::decode(&packet.payload_hex) {
                                        if let Ok(ver) = String::from_utf8(bytes) {
                                            let ver = ver.trim_matches('\0').trim().to_string();
                                            if !ver.is_empty() {
                                                *firmware_version_clone.lock().await = Some(ver);
                                            }
                                        }
                                    }
                                }

                                // v0.3.6 — Parse CMD_GET_SETTINGS (0x22) responses.
                                // Payload: [region, community, node, gateway_mode_byte]
                                // Board is source of truth — update local state from board.
                                if packet.command == radio::CMD_GET_SETTINGS {
                                    if let Ok(payload_bytes) = hex::decode(&packet.payload_hex) {
                                        if payload_bytes.len() >= 4 {
                                            let addr = NodeAddress::new(
                                                payload_bytes[0],
                                                payload_bytes[1],
                                                payload_bytes[2],
                                            );
                                            let gw = payload_bytes[3] != 0;
                                            // Board address is authoritative
                                            if addr != NodeAddress::broadcast() {
                                                *node_address_clone.lock().await = addr.clone();
                                            }
                                            *board_settings_clone.lock().await = Some(BoardSettings {
                                                node_address: addr,
                                                gateway_mode: gw,
                                            });
                                        }
                                    }
                                }

                                // Broadcast to all subscribers
                                let _ = packet_tx_clone.send(packet.clone());

                                // Invoke the caller-supplied callback (GUI emitter, CLI printer…)
                                on_packet(packet);

                                // Consume the processed bytes
                                let consumed = radio::SINGLE_HDR_LEN
                                    + (accumulator.len() - radio::SINGLE_HDR_LEN)
                                        .min(radio::MAX_SINGLE_PAYLOAD_LEN);
                                accumulator.drain(..consumed);
                            } else {
                                // parse_incoming returned None (buffer too short) — wait for more data
                                break;
                            }
                        }
                    }
                }
            }

            connected_clone.store(false, Ordering::Relaxed);
            log::info!("Serial read loop exited");
        });

        // Request device address immediately after opening (best-effort)
        let local_addr = NodeAddress::default_local();
        let addr_req = radio::build_get_node_addr(&local_addr);
        self.send_raw(addr_req).await.ok();

        Ok(())
    }

    /// Close the serial port and stop the read loop.
    pub async fn disconnect(&self) -> Result<()> {
        log::info!("Disconnecting serial port");

        // Cancel the read loop
        if let Some(token) = self.cancel.lock().expect("cancel mutex poisoned").take() {
            token.cancel();
        }

        // Drop the port (closes the file handle)
        *self.port.lock().expect("port mutex poisoned") = None;
        self.connected.store(false, Ordering::Relaxed);

        // Reset stats, firmware version, and board settings
        *self.stats.lock().await = RadioStats::default();
        *self.firmware_version.lock().await = None;
        *self.board_settings.lock().await = None;

        Ok(())
    }

    /// Write `bytes` to the serial port using `spawn_blocking`.
    ///
    /// This is non-blocking from the perspective of the Tokio scheduler.
    pub async fn send_raw(&self, bytes: Vec<u8>) -> Result<()> {
        let port_arc = Arc::clone(&self.port);

        tokio::task::spawn_blocking(move || {
            let mut guard = port_arc.lock().expect("port mutex poisoned");
            let port = guard
                .as_mut()
                .ok_or_else(|| anyhow::anyhow!("No serial port open — connect first"))?;
            port.write_all(&bytes).context("Failed to write to serial port")?;
            port.flush().context("Failed to flush serial port")?;
            Ok::<(), anyhow::Error>(())
        })
        .await
        .context("send_raw spawn_blocking join error")??;

        // Update sent counter
        let mut stats = self.stats.lock().await;
        stats.packets_sent += 1;

        Ok(())
    }

    /// Send a PING and return `true` if the device responds within 500 ms.
    pub async fn ping(&self) -> bool {
        if !self.is_connected() {
            return false;
        }

        let local = self.node_address.lock().await.clone();
        let ping_pkt = radio::build_ping(&local, &local);

        if self.send_raw(ping_pkt).await.is_err() {
            return false;
        }

        let mut rx = self.packet_tx.subscribe();
        matches!(
            tokio::time::timeout(Duration::from_millis(500), rx.recv()).await,
            Ok(Ok(_))
        )
    }

    /// Update the stored node address (called when we receive a GET_NODE_ADDR response).
    pub async fn update_node_address(&self, addr: NodeAddress) {
        *self.node_address.lock().await = addr;
    }

    /// The firmware version string reported by the connected device, if available.
    pub async fn get_firmware_version(&self) -> Option<String> {
        self.firmware_version.lock().await.clone()
    }

    /// v0.3.6 — Board settings (node address + gateway_mode) as last reported by the device.
    /// Populated after CMD_GET_SETTINGS (0x22) response. Board is source of truth.
    pub async fn get_board_settings(&self) -> Option<BoardSettings> {
        self.board_settings.lock().await.clone()
    }

    /// After sending SET_NODE_ADDR, verify the device accepted it by querying
    /// GET_NODE_ADDR and checking the response source matches `expected`.
    ///
    /// Returns `true` if the device confirmed the new address within `timeout_ms`.
    pub async fn verify_node_address_set(&self, expected: &NodeAddress, timeout_ms: u64) -> bool {
        if !self.is_connected() {
            return false;
        }

        let mut rx = self.packet_tx.subscribe();
        let local = self.node_address.lock().await.clone();
        let query = radio::build_get_node_addr(&local);
        if self.send_raw(query).await.is_err() {
            return false;
        }

        let expected_owned = expected.clone();
        let deadline = std::time::Duration::from_millis(timeout_ms);
        match tokio::time::timeout(deadline, async move {
            loop {
                match rx.recv().await {
                    Ok(pkt) if pkt.command == radio::CMD_GET_NODE_ADDR
                        && pkt.source == expected_owned => return true,
                    Ok(_) => continue,
                    Err(_) => return false,
                }
            }
        })
        .await
        {
            Ok(verified) => verified,
            Err(_) => false, // timeout
        }
    }
}

impl Default for SerialManager {
    fn default() -> Self {
        Self::new()
    }
}
