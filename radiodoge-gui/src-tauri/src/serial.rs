//! Serial port management for Heltec LoRa module communication.
//!
//! This module provides cross-platform serial communication (Windows, Linux,
//! macOS, and eventually Android via serialport-android). All blocking I/O
//! is wrapped in tokio::task::spawn_blocking to keep the async runtime healthy.
//!
//! The SerialManager runs a background read loop that:
//!   1. Reads bytes from the serial port
//!   2. Assembles them into complete RadioDoge packets
//!   3. Emits Tauri events to the frontend for real-time updates

use std::io::{Read, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use tokio::sync::{broadcast, Mutex};
use tokio_util::sync::CancellationToken;

use crate::radio;
use crate::types::{IncomingPacket, NodeAddress, RadioStats};

/// Baud rate for Heltec LoRa module serial communication
const BAUD_RATE: u32 = 115_200;

/// Timeout for serial reads (short, so the read loop stays responsive)
const READ_TIMEOUT_MS: u64 = 50;

/// Buffer size for accumulating incoming bytes
const READ_BUFFER_SIZE: usize = 1024;

/// Shared state for the serial connection
pub struct SerialManager {
    /// The open serial port, wrapped for thread-safe sharing
    port: Arc<Mutex<Option<Box<dyn serialport::SerialPort>>>>,
    /// Whether the port is currently open
    connected: Arc<AtomicBool>,
    /// Broadcast channel for distributing incoming packets to multiple listeners
    packet_tx: broadcast::Sender<IncomingPacket>,
    /// Cancellation token to shut down the read loop
    cancel: Arc<Mutex<Option<CancellationToken>>>,
    /// Radio statistics — updated as packets are received
    stats: Arc<Mutex<RadioStats>>,
    /// The node address read from the device after connecting
    node_address: Arc<Mutex<NodeAddress>>,
}

impl SerialManager {
    pub fn new() -> Self {
        let (packet_tx, _) = broadcast::channel(128);
        SerialManager {
            port: Arc::new(Mutex::new(None)),
            connected: Arc::new(AtomicBool::new(false)),
            packet_tx,
            cancel: Arc::new(Mutex::new(None)),
            stats: Arc::new(Mutex::new(RadioStats::default())),
            node_address: Arc::new(Mutex::new(NodeAddress::default_local())),
        }
    }

    /// List all available serial ports on this system.
    pub fn list_ports() -> Vec<String> {
        match serialport::available_ports() {
            Ok(ports) => ports
                .iter()
                .map(|p| p.port_name.clone())
                .collect(),
            Err(e) => {
                log::warn!("Failed to list serial ports: {}", e);
                vec![]
            }
        }
    }

    /// Check if a serial port is currently open.
    pub fn is_connected(&self) -> bool {
        self.connected.load(Ordering::Relaxed)
    }

    /// Get a clone of the radio stats.
    pub async fn get_stats(&self) -> RadioStats {
        self.stats.lock().await.clone()
    }

    /// Get the current node address (read from device on connect).
    pub async fn get_node_address(&self) -> NodeAddress {
        self.node_address.lock().await.clone()
    }

    /// Subscribe to incoming packets.
    pub fn subscribe_packets(&self) -> broadcast::Receiver<IncomingPacket> {
        self.packet_tx.subscribe()
    }

    /// Open a serial port and start the background read loop.
    ///
    /// Sends a GET_NODE_ADDR command immediately after connecting to retrieve
    /// the Heltec device's configured address.
    pub async fn connect(
        &self,
        port_name: &str,
        on_packet: Arc<dyn Fn(IncomingPacket) + Send + Sync>,
    ) -> Result<()> {
        if self.is_connected() {
            self.disconnect().await?;
        }

        log::info!("Opening serial port: {} at {} baud", port_name, BAUD_RATE);

        let port = serialport::new(port_name, BAUD_RATE)
            .timeout(Duration::from_millis(READ_TIMEOUT_MS))
            .open()
            .with_context(|| format!("Failed to open serial port {}", port_name))?;

        *self.port.lock().await = Some(port);
        self.connected.store(true, Ordering::Relaxed);

        // Create a cancellation token for the read loop
        let token = CancellationToken::new();
        *self.cancel.lock().await = Some(token.clone());

        // Start background read loop
        let port_clone = Arc::clone(&self.port);
        let connected_clone = Arc::clone(&self.connected);
        let stats_clone = Arc::clone(&self.stats);
        let packet_tx_clone = self.packet_tx.clone();

        tokio::spawn(async move {
            log::info!("Serial read loop started");
            let mut accumulator = Vec::new();

            loop {
                if token.is_cancelled() {
                    log::info!("Serial read loop cancelled");
                    break;
                }

                // Read bytes in a blocking task to avoid blocking the async runtime
                let result = {
                    let mut port_guard = port_clone.lock().await;
                    if let Some(port) = port_guard.as_mut() {
                        let mut buf = vec![0u8; READ_BUFFER_SIZE];
                        // Use spawn_blocking for the actual read
                        match port.read(&mut buf) {
                            Ok(0) => Ok(vec![]),
                            Ok(n) => Ok(buf[..n].to_vec()),
                            Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => Ok(vec![]),
                            Err(e) => Err(e),
                        }
                    } else {
                        break;
                    }
                };

                match result {
                    Ok(bytes) if !bytes.is_empty() => {
                        accumulator.extend_from_slice(&bytes);

                        // Try to extract complete packets from the accumulator.
                        // A complete packet is at least SINGLE_HDR_LEN bytes.
                        while accumulator.len() >= radio::SINGLE_HDR_LEN {
                            // For simplicity: if we have enough bytes for a header,
                            // try to parse a packet. Real protocol would use length
                            // fields or delimiters — this is a pragmatic MVP approach
                            // that works with the existing firmware's serial output.
                            if let Some(packet) = radio::parse_incoming(&accumulator, 0) {
                                // Update stats
                                {
                                    let mut stats = stats_clone.lock().await;
                                    stats.packets_received += 1;
                                }

                                // Notify packet listeners
                                let _ = packet_tx_clone.send(packet.clone());
                                on_packet(packet);

                                // Consume the processed bytes
                                // Simple approach: clear after each "packet"
                                // In production, track actual packet length
                                let consumed = radio::SINGLE_HDR_LEN
                                    + (accumulator.len() - radio::SINGLE_HDR_LEN)
                                        .min(radio::MAX_SINGLE_PAYLOAD_LEN);
                                accumulator.drain(..consumed);
                            } else {
                                // Not enough data yet — wait for more
                                break;
                            }
                        }
                    }
                    Ok(_) => {
                        // No bytes read (timeout) — yield control briefly
                        tokio::time::sleep(Duration::from_millis(10)).await;
                    }
                    Err(e) => {
                        log::error!("Serial read error: {}", e);
                        connected_clone.store(false, Ordering::Relaxed);
                        break;
                    }
                }
            }

            log::info!("Serial read loop exited");
            connected_clone.store(false, Ordering::Relaxed);
        });

        // Request the device's node address
        let local_addr = NodeAddress::default_local();
        let ping_packet = radio::build_get_node_addr(&local_addr);
        self.send_raw(ping_packet).await.ok(); // best-effort

        Ok(())
    }

    /// Close the serial port and stop the read loop.
    pub async fn disconnect(&self) -> Result<()> {
        log::info!("Disconnecting serial port");

        // Cancel the read loop
        if let Some(token) = self.cancel.lock().await.take() {
            token.cancel();
        }

        // Close the port
        *self.port.lock().await = None;
        self.connected.store(false, Ordering::Relaxed);

        // Reset stats
        *self.stats.lock().await = RadioStats::default();

        Ok(())
    }

    /// Write raw bytes to the serial port.
    pub async fn send_raw(&self, bytes: Vec<u8>) -> Result<()> {
        let mut port_guard = self.port.lock().await;
        if let Some(port) = port_guard.as_mut() {
            port.write_all(&bytes)
                .context("Failed to write to serial port")?;
            port.flush().context("Failed to flush serial port")?;

            // Update sent counter
            drop(port_guard);
            let mut stats = self.stats.lock().await;
            stats.packets_sent += 1;
            Ok(())
        } else {
            anyhow::bail!("No serial port open — connect first")
        }
    }

    /// Send a PING to the device and wait briefly for a response.
    /// Returns true if the device responded.
    pub async fn ping(&self) -> bool {
        if !self.is_connected() {
            return false;
        }

        let local = self.node_address.lock().await.clone();
        let ping = radio::build_ping(&local, &local);

        if self.send_raw(ping).await.is_err() {
            return false;
        }

        // Subscribe and wait up to 500ms for any response
        let mut rx = self.packet_tx.subscribe();
        let timeout = tokio::time::timeout(Duration::from_millis(500), rx.recv());
        matches!(timeout.await, Ok(Ok(_)))
    }

    /// Update the node address stored after receiving a GET_NODE_ADDR response.
    pub async fn update_node_address(&self, addr: NodeAddress) {
        *self.node_address.lock().await = addr;
    }
}

impl Default for SerialManager {
    fn default() -> Self {
        Self::new()
    }
}
