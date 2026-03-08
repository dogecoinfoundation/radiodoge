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

/** Rich information about a serial port, including USB device details. */
export interface PortInfo {
  /** OS port name, e.g. "COM3" or "/dev/ttyUSB0" */
  name: string;
  /** True if this is a USB serial adapter */
  isUsb: boolean;
  /** USB manufacturer string (if reported) */
  manufacturer?: string;
  /** USB product string (if reported), e.g. "CP2102 USB to UART Bridge" */
  product?: string;
  /** USB Vendor ID */
  vid?: number;
  /** USB Product ID */
  pid?: number;
  /** Short human-readable label for display, e.g. "COM3 — CP2102 USB to UART Bridge" */
  description: string;
  /** True if the VID/PID matches a known Heltec / ESP32 USB adapter */
  isLikelyHeltec: boolean;
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

export type ConnectionStatusType = 'disconnected' | 'connecting' | 'connected' | 'error' | 'reconnecting';

export interface ConnectionStatusEvent {
  status: ConnectionStatusType;
  port?: string;
  nodeAddress?: NodeAddress;
  message?: string;
  firmwareVersion?: string;
}

/** v0.3.6 — Board live state, reported by CMD_GET_SETTINGS (0x22). Board is source of truth. */
export interface BoardSettings {
  nodeAddress: NodeAddress;
  gatewayMode: boolean;
}

/** v0.3.6 — Transaction history entry (last 50, persisted to local JSON). */
export interface TxHistoryEntry {
  timestamp: number;
  toAddress: string;
  amountDoge: number;
  memo?: string;
  status: 'sent' | 'failed' | string;
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
    0x20: 'FW VERSION',
    0x21: 'SET LORA',
    0x22: 'GET SETTINGS',
    0x23: 'SET GATEWAY',
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
