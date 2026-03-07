/**
 * TypeScript types mirroring the Rust types in src-tauri/src/types.rs.
 * These are the shapes of data passed over Tauri IPC.
 */

export interface RadioStats {
  frequencyMhz: number;
  powerDbm: number;
  spreadingFactor: number;
  bandwidthKhz: number;
  codingRate: string;
  rssi: number;
  snr: number;
  packetsSent: number;
  packetsReceived: number;
}

export interface NodeAddress {
  region: number;
  community: number;
  node: number;
}

export function nodeAddressToString(addr: NodeAddress): string {
  return `${addr.region}.${addr.community}.${addr.node}`;
}

export interface WalletInfo {
  address: string;
  publicKeyHex: string;
  privateKeyWif: string;
}

export interface LoraSettings {
  frequencyMhz: number;
  powerDbm: number;
  spreadingFactor: number;
  bandwidthKhz: number;
  codingRate: string;
  nodeAddress: NodeAddress;
}

export interface TransactionRequest {
  toAddress: string;
  amountDoge: number;
  memo?: string;
  fromPrivateKeyWif?: string;
}

export interface IncomingPacket {
  timestamp: number;
  source: NodeAddress;
  destination: NodeAddress;
  command: number;
  payloadHex: string;
  decoded?: string;
  rssi: number;
}

export type ConnectionStatusType = 'disconnected' | 'connecting' | 'connected' | 'error';

export interface ConnectionStatusEvent {
  status: ConnectionStatusType;
  port?: string;
  nodeAddress?: NodeAddress;
  message?: string;
}

/**
 * Map RSSI (dBm) to a signal bar count (1-5).
 */
export function rssiToBars(rssi: number): number {
  if (rssi >= -50) return 5;
  if (rssi >= -65) return 4;
  if (rssi >= -80) return 3;
  if (rssi >= -95) return 2;
  return 1;
}

/**
 * Format a Unix timestamp to a human-readable time string.
 */
export function formatTimestamp(ts: number): string {
  return new Date(ts * 1000).toLocaleTimeString();
}

/**
 * Map a command byte to a human-readable label.
 */
export function commandName(cmd: number): string {
  const map: Record<number, string> = {
    0x00: 'GET ADDR',
    0x01: 'SET ADDR',
    0x02: 'PING',
    0x03: 'MESSAGE',
    0x04: 'BROADCAST',
    0x05: 'MULTIPART',
    0x10: 'DOGE TX',
    0x11: 'BALANCE',
  };
  return map[cmd] ?? `CMD(0x${cmd.toString(16).toUpperCase()})`;
}

/** The Doge-speak phrases randomly selected for UI copy */
export const DOGE_PHRASES = [
  'much wow',
  'very transaction',
  'such radio',
  'many signals',
  'wow so doge',
  'very blockchain',
  'much LoRa',
  'such decentralize',
  'to the moon 🌙',
  'very shibu',
];

export function randomDogephrase(): string {
  return DOGE_PHRASES[Math.floor(Math.random() * DOGE_PHRASES.length)];
}
