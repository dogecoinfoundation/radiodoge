/**
 * Radio/LoRa state store using Svelte 5 runes.
 *
 * Holds incoming + outgoing packets and LoRa settings state.
 * v0.3.1: Packets now carry a `direction` field ('TX' | 'RX') for the
 *         combined TX/RX packet log in ReceiveTab and Dashboard.
 */

import type { IncomingPacket, LoraSettings } from '$lib/types';

/** A displayable packet entry — extends IncomingPacket with a direction flag. */
export interface PacketEntry extends IncomingPacket {
  /** 'TX' for outgoing packets sent by this node, 'RX' for incoming. */
  direction: 'TX' | 'RX';
}

/** Maximum packets kept in the receive buffer (capped at 100) */
const MAX_PACKETS = 100;

export const radio = $state({
  /** All packets (TX + RX), most recent first, capped at MAX_PACKETS */
  packets: [] as PacketEntry[],
  /** Count of total RX packets received (including dropped from buffer) */
  totalReceived: 0,
  /** Count of total TX packets sent in this session */
  totalSent: 0,
  /** Current LoRa settings (editable in Settings tab) */
  settings: {
    frequencyMhz: 915.0,
    powerDbm: 5,
    spreadingFactor: 7,
    bandwidthKhz: 125.0,
    codingRate: '4/5',
    nodeAddress: { region: 10, community: 0, node: 1 },
  } as LoraSettings,
  /** Whether settings are being saved */
  isSavingSettings: false,
  /** Whether last send succeeded */
  lastSendSuccess: false,
  /** Message from last send operation */
  lastSendMessage: '',
});

/** Add an incoming (RX) packet to the buffer. */
export function addPacket(packet: IncomingPacket) {
  const entry: PacketEntry = { ...packet, direction: 'RX' };
  radio.packets = [entry, ...radio.packets].slice(0, MAX_PACKETS);
  radio.totalReceived += 1;
}

/**
 * Add a sent (TX) packet to the buffer.
 * Called when the frontend receives a `radio-packet-tx` event from Rust.
 */
export function addSentPacket(packet: IncomingPacket) {
  const entry: PacketEntry = { ...packet, direction: 'TX' };
  radio.packets = [entry, ...radio.packets].slice(0, MAX_PACKETS);
  radio.totalSent += 1;
}

/** Clear all packets from the buffer. */
export function clearPackets() {
  radio.packets = [];
  radio.totalReceived = 0;
  radio.totalSent = 0;
}

/** Update LoRa settings from the Settings tab. */
export function updateSettings(settings: LoraSettings) {
  radio.settings = settings;
}
