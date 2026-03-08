//! RadioDoge LoRa packet protocol implementation.
//!
//! This module encodes and decodes the binary packet format used between
//! the desktop app and the Heltec ESP32 LoRa module over serial.
//!
//! Packet format (derived from RadioDogeSharp & serdog source):
//!
//! Single packet:
//!   [0]  Command byte (see CMD_* constants)
//!   [1]  Flags byte (0x00 for standard, 0x01 for multipart)
//!   [2]  Source region
//!   [3]  Source community
//!   [4]  Source node
//!   [5]  Destination region
//!   [6]  Destination community
//!   [7]  Destination node
//!   [8+] Payload (up to MAX_SINGLE_PAYLOAD_LEN bytes)
//!
//! Multipart packet (for payloads > MAX_SINGLE_PAYLOAD_LEN):
//!   [0..8]   Same header as single packet (flags byte = 0x01)
//!   [8]  Total parts (1 byte)
//!   [9]  Part index (0-based, 1 byte)
//!   [10] Part ID high byte
//!   [11] Part ID low byte
//!   [12+] Payload chunk

use std::time::{SystemTime, UNIX_EPOCH};

use crate::types::{IncomingPacket, NodeAddress};

/// Maximum payload bytes in a single (non-multipart) packet
pub const MAX_SINGLE_PAYLOAD_LEN: usize = 192;

/// Maximum parts in a multipart message
pub const MAX_MULTIPART_PARTS: u8 = 20;

// Command bytes (matching RadioDogeSharp's SerialCommandType enum)
pub const CMD_GET_NODE_ADDR: u8 = 0x00;
pub const CMD_SET_NODE_ADDRS: u8 = 0x01;
pub const CMD_PING: u8 = 0x02;
pub const CMD_MESSAGE: u8 = 0x03;
pub const CMD_BROADCAST: u8 = 0x04;
pub const CMD_MULTIPART: u8 = 0x05;
pub const CMD_DOGE_TX: u8 = 0x10;     // Dogecoin transaction
pub const CMD_REQUEST_BALANCE: u8 = 0x11; // Balance request
pub const CMD_GET_FIRMWARE_VERSION: u8 = 0x20; // Query firmware version string
pub const CMD_SET_LORA_PARAMS: u8 = 0x21;      // Set SF/BW/CR/frequency/TX-power (v0.3.3)

/// Single packet header length in bytes
pub const SINGLE_HDR_LEN: usize = 8;
/// Multipart packet header length (single header + 4 multipart bytes)
pub const MULTIPART_HDR_LEN: usize = 12;

/// Flags byte: standard single packet
pub const FLAG_STANDARD: u8 = 0x00;
/// Flags byte: this packet is part of a multipart sequence
pub const FLAG_MULTIPART: u8 = 0x01;

/// Build a GET_NODE_ADDR command to query the Heltec device's address.
pub fn build_get_node_addr(src: &NodeAddress) -> Vec<u8> {
    let broadcast = NodeAddress::broadcast();
    build_header(CMD_GET_NODE_ADDR, FLAG_STANDARD, src, &broadcast)
}

/// Build a GET_FIRMWARE_VERSION command (CMD 0x20).
/// The device should respond with a packet whose payload is the version string.
pub fn build_get_firmware_version(src: &NodeAddress) -> Vec<u8> {
    build_header(CMD_GET_FIRMWARE_VERSION, FLAG_STANDARD, src, &NodeAddress::broadcast())
}

/// Build a PING packet to check if the device is alive.
pub fn build_ping(src: &NodeAddress, dst: &NodeAddress) -> Vec<u8> {
    build_header(CMD_PING, FLAG_STANDARD, src, dst)
}

/// Build a text MESSAGE packet.
pub fn build_message(src: &NodeAddress, dst: &NodeAddress, text: &str) -> Vec<u8> {
    let mut packet = build_header(CMD_MESSAGE, FLAG_STANDARD, src, dst);
    let payload = &text.as_bytes()[..text.as_bytes().len().min(MAX_SINGLE_PAYLOAD_LEN)];
    packet.extend_from_slice(payload);
    packet
}

/// Build a BROADCAST packet (sent to all nodes).
pub fn build_broadcast(src: &NodeAddress, text: &str) -> Vec<u8> {
    build_message(src, &NodeAddress::broadcast(), text)
}

/// Build a Dogecoin transaction packet.
/// If payload exceeds MAX_SINGLE_PAYLOAD_LEN, use build_multipart_packets instead.
pub fn build_doge_tx(src: &NodeAddress, dst: &NodeAddress, tx_payload: &[u8]) -> Vec<u8> {
    let mut packet = build_header(CMD_DOGE_TX, FLAG_STANDARD, src, dst);
    let clamped = &tx_payload[..tx_payload.len().min(MAX_SINGLE_PAYLOAD_LEN)];
    packet.extend_from_slice(clamped);
    packet
}

/// Build a SET_LORA_PARAMS packet (CMD 0x21) — v0.3.3 additive.
///
/// Payload format (8 bytes):
///   [0]  Spreading factor (7–12)
///   [1]  Bandwidth index (0=125kHz, 1=250kHz, 2=500kHz)
///   [2]  Coding rate denominator (5–8, meaning 4/5 .. 4/8)
///   [3]  Frequency high byte  (freq_khz >> 8)
///   [4]  Frequency low byte   (freq_khz & 0xFF)  — freq in kHz, e.g. 915000
///   [5]  TX power in dBm (2–22)
///   [6-7] Reserved (0x00)
pub fn build_set_lora_params(
    src: &NodeAddress,
    sf: u8,
    bw_idx: u8,
    cr: u8,
    freq_khz: u32,
    tx_power: u8,
) -> Vec<u8> {
    let mut packet = build_header(CMD_SET_LORA_PARAMS, FLAG_STANDARD, src, &NodeAddress::broadcast());
    let freq_hi = ((freq_khz >> 8) & 0xFF) as u8;
    let freq_lo = (freq_khz & 0xFF) as u8;
    packet.extend_from_slice(&[sf, bw_idx, cr, freq_hi, freq_lo, tx_power, 0x00, 0x00]);
    packet
}

/// Build a SET_NODE_ADDRS packet to configure the device's LoRa address.
pub fn build_set_node_addr(src: &NodeAddress, new_addr: &NodeAddress) -> Vec<u8> {
    let mut packet = build_header(CMD_SET_NODE_ADDRS, FLAG_STANDARD, src, &NodeAddress::broadcast());
    // Payload: the new address bytes
    packet.push(new_addr.region);
    packet.push(new_addr.community);
    packet.push(new_addr.node);
    packet
}

/// Split a large payload into multipart packets (for payloads > MAX_SINGLE_PAYLOAD_LEN).
///
/// Each part has a 12-byte header:
///   [0..8]  Standard header (with FLAG_MULTIPART)
///   [8]     Total parts count
///   [9]     This part's index (0-based)
///   [10-11] Unique session ID (random u16, same for all parts)
pub fn build_multipart_packets(
    src: &NodeAddress,
    dst: &NodeAddress,
    cmd: u8,
    payload: &[u8],
) -> Vec<Vec<u8>> {
    let chunk_size = MAX_SINGLE_PAYLOAD_LEN - (MULTIPART_HDR_LEN - SINGLE_HDR_LEN);
    let chunks: Vec<&[u8]> = payload.chunks(chunk_size).collect();
    let total_parts = chunks.len().min(MAX_MULTIPART_PARTS as usize);
    let session_id = rand::random::<u16>();

    chunks
        .into_iter()
        .take(total_parts)
        .enumerate()
        .map(|(idx, chunk)| {
            let mut packet = build_header(cmd, FLAG_MULTIPART, src, dst);
            packet.push(total_parts as u8);
            packet.push(idx as u8);
            packet.extend_from_slice(&session_id.to_be_bytes());
            packet.extend_from_slice(chunk);
            packet
        })
        .collect()
}

/// Parse an incoming byte buffer into an IncomingPacket.
///
/// Returns None if the buffer is too short or malformed.
pub fn parse_incoming(buf: &[u8], rssi: i16) -> Option<IncomingPacket> {
    if buf.len() < SINGLE_HDR_LEN {
        return None;
    }

    let command = buf[0];
    let _flags = buf[1];

    let source = NodeAddress::new(buf[2], buf[3], buf[4]);
    let destination = NodeAddress::new(buf[5], buf[6], buf[7]);

    let payload = &buf[SINGLE_HDR_LEN..];
    let payload_hex = hex::encode(payload);

    // Try to decode known payload formats
    let decoded = decode_payload(command, payload);

    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();

    Some(IncomingPacket {
        timestamp,
        source,
        destination,
        command,
        payload_hex,
        decoded,
        rssi,
    })
}

/// Attempt to decode a packet payload based on command type.
fn decode_payload(command: u8, payload: &[u8]) -> Option<String> {
    match command {
        CMD_PING => Some("🏓 PING".to_string()),
        CMD_MESSAGE | CMD_BROADCAST => {
            std::str::from_utf8(payload)
                .ok()
                .map(|s| format!("💬 {}", s.trim_end_matches('\0')))
        }
        CMD_GET_NODE_ADDR => Some("📍 GET NODE ADDRESS".to_string()),
        CMD_SET_NODE_ADDRS => {
            if payload.len() >= 3 {
                Some(format!("📍 SET ADDR: {}.{}.{}", payload[0], payload[1], payload[2]))
            } else {
                None
            }
        }
        CMD_DOGE_TX => {
            crate::wallet::decode_transaction_payload(payload)
                .map(|s| format!("🐕 {}", s))
        }
        CMD_GET_FIRMWARE_VERSION => {
            std::str::from_utf8(payload)
                .ok()
                .map(|s| format!("🔧 FW: {}", s.trim_matches('\0').trim()))
        }
        _ => None,
    }
}

/// Build a standard 8-byte packet header.
fn build_header(
    cmd: u8,
    flags: u8,
    src: &NodeAddress,
    dst: &NodeAddress,
) -> Vec<u8> {
    vec![
        cmd,
        flags,
        src.region,
        src.community,
        src.node,
        dst.region,
        dst.community,
        dst.node,
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_src() -> NodeAddress { NodeAddress::new(10, 0, 1) }
    fn test_dst() -> NodeAddress { NodeAddress::new(10, 0, 2) }

    #[test]
    fn test_build_ping() {
        let packet = build_ping(&test_src(), &test_dst());
        assert_eq!(packet.len(), SINGLE_HDR_LEN);
        assert_eq!(packet[0], CMD_PING);
        assert_eq!(packet[2], 10); // src region
        assert_eq!(packet[5], 10); // dst region
    }

    #[test]
    fn test_build_message() {
        let packet = build_message(&test_src(), &test_dst(), "much wow");
        assert_eq!(packet[0], CMD_MESSAGE);
        let payload = &packet[SINGLE_HDR_LEN..];
        assert_eq!(payload, b"much wow");
    }

    #[test]
    fn test_multipart_splitting() {
        let big_payload = vec![0xAA; 400];
        let parts = build_multipart_packets(&test_src(), &test_dst(), CMD_MESSAGE, &big_payload);
        assert!(parts.len() > 1, "Large payload should be split into multiple parts");
        for part in &parts {
            assert!(part.len() <= MAX_SINGLE_PAYLOAD_LEN + SINGLE_HDR_LEN + 4);
        }
    }

    #[test]
    fn test_parse_incoming() {
        let packet = build_ping(&test_src(), &test_dst());
        let parsed = parse_incoming(&packet, -70).expect("Should parse valid packet");
        assert_eq!(parsed.command, CMD_PING);
        assert_eq!(parsed.source.region, 10);
        assert_eq!(parsed.rssi, -70);
    }
}
