/**
 * Radio/LoRa state store using Svelte 5 runes.
 *
 * Holds incoming packets and LoRa settings state.
 */

import type { IncomingPacket, LoraSettings } from '$lib/types';

/** Maximum packets kept in the receive buffer */
const MAX_PACKETS = 100;

export const radio = $state({
  /** All received packets (capped at MAX_PACKETS) */
  packets: [] as IncomingPacket[],
  /** Count of total packets received (including dropped from buffer) */
  totalReceived: 0,
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

/** Add an incoming packet to the buffer. */
export function addPacket(packet: IncomingPacket) {
  radio.packets = [packet, ...radio.packets].slice(0, MAX_PACKETS);
  radio.totalReceived += 1;
}

/** Clear all packets from the buffer. */
export function clearPackets() {
  radio.packets = [];
  radio.totalReceived = 0;
}

/** Update LoRa settings from the Settings tab. */
export function updateSettings(settings: LoraSettings) {
  radio.settings = settings;
}
