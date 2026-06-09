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

// ── v0.3.10 — Android mobile connection types ─────────────────────────────────

/**
 * Android connection lifecycle status.
 * Used by the mobile UI state machine in ConnectionPanel.
 */
export type MobileConnectionStatus =
  | 'idle'        // Nothing happening yet
  | 'searching'   // Device scan running
  | 'connecting'  // Port open, handshake in progress
  | 'connected'   // Board acknowledged — fully ready
  | 'failed';     // Something went wrong

/**
 * A physical device discovered by the Android USB/Bluetooth scanner.
 * Mirrors MobileDeviceInfo in connection-bridge.ts.
 */
export interface MobileDeviceInfo {
  /** OS path or BT address, e.g. "/dev/bus/usb/001/002" */
  path: string;
  /** Human-readable label for the device picker */
  description: string;
  /** Connection medium */
  type: 'usb' | 'bluetooth' | 'unknown';
  /** True if VID/PID matches a known Heltec / ESP32 USB adapter */
  isLikelyHeltec: boolean;
  /** USB Vendor ID (undefined for Bluetooth devices) */
  vid?: number;
  /** USB Product ID (undefined for Bluetooth devices) */
  pid?: number;
}

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
  /** v0.3.7 — Whether the WiFi radio is enabled on the board */
  wifiEnabled?: boolean;
}

/** v0.3.7 — A recently heard mesh neighbor. */
export interface NeighborEntry {
  address: NodeAddress;
  rssi: number;
  lastSeen: number; // Unix seconds
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
    0x24: 'WIFI TOGGLE',
    0x25: 'ADDR CONFLICT',
    0x26: 'GET BATTERY',
    0x27: 'GET MAC',
    0x28: 'BLE TOGGLE',
  };
  return map[cmd] ?? `CMD(0x${cmd.toString(16).toUpperCase()})`;
}

/** v0.3.8 — Address book entry: a labelled Dogecoin address saved locally. */
export interface AddressBookEntry {
  id: string;           // UUID (timestamp-based)
  label: string;        // Human-readable nickname, e.g. "Main wallet"
  address: string;      // Dogecoin address (starts with "D")
  createdAt: number;    // Unix timestamp (seconds)
  notes?: string;       // Optional free-text notes
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
