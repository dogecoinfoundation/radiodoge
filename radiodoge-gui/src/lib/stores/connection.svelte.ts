/**
 * Connection state store using Svelte 5 runes.
 *
 * This module holds all state related to the serial connection and
 * radio statistics. It's imported directly into components via
 * `import { connection } from '$lib/stores/connection.svelte'`.
 */

import type { BoardSettings, ConnectionStatusType, NeighborEntry, NodeAddress, RadioStats } from '$lib/types';

/**
 * Reactive connection state — use $state() (Svelte 5 rune).
 * Exported as a plain object so components can access properties directly.
 */
export const connection = $state({
  /** Whether we are currently connected to a Heltec device */
  isConnected: false,
  /** The COM port name (e.g. "COM3" or "/dev/ttyUSB0") */
  portName: '',
  /** All available COM ports discovered on this machine */
  availablePorts: [] as string[],
  /** Current connection state */
  status: 'disconnected' as ConnectionStatusType,
  /** Error message if connection failed */
  error: null as string | null,
  /** The Heltec device's RadioDoge node address */
  nodeAddress: null as NodeAddress | null,
  /** Latest radio statistics */
  stats: null as RadioStats | null,
  /** Whether a connection attempt is in progress */
  isConnecting: false,
  /** Whether auto-reconnect is in progress after unexpected disconnect */
  isReconnecting: false,
  /** Firmware version string reported by the Heltec device, e.g. "v1.2.3" */
  firmwareVersion: null as string | null,
  /** v0.3.6 — Whether board is in gateway mode (synced from board on connect) */
  gatewayMode: false,
  /** v0.3.6 — Connection type selected by user: "usb" or "ble" */
  connectionType: 'usb' as 'usb' | 'ble',
  /** v0.3.6 — Whether a gateway daemon process is running */
  gatewayOnline: false,
  /** v0.3.7 — Whether the board's WiFi radio is enabled */
  wifiEnabled: true,
  /** v0.3.7 — Whether a duplicate node address has been detected on the mesh */
  addrConflict: false,
  /** v0.3.7 — Recently heard mesh neighbors */
  neighbors: [] as NeighborEntry[],
  /** v0.3.8 — Battery voltage in mV (null = not yet queried) */
  batteryMv: null as number | null,
  /** v0.3.8 — Board MAC address string (e.g. "A0:B1:C2:D3:E4:F5") */
  boardMac: null as string | null,
});

/** Update available ports list */
export function setAvailablePorts(ports: string[]) {
  connection.availablePorts = ports;
}

/** Set connected state */
export function setConnected(port: string, nodeAddress: NodeAddress | null, firmwareVersion?: string | null) {
  connection.isConnected = true;
  connection.isConnecting = false;
  connection.isReconnecting = false;
  connection.portName = port;
  connection.nodeAddress = nodeAddress;
  connection.status = 'connected';
  connection.error = null;
  // Strip redundant "RadioDoge " prefix that the firmware prepends to version strings
  // e.g. "RadioDoge NV3FW01" → "NV3FW01", "RadioDoge NV55" → "NV55"
  connection.firmwareVersion = firmwareVersion
    ? firmwareVersion.replace(/^RadioDoge\s+/i, '').trim() || firmwareVersion
    : null;
}

/** Set connecting state */
export function setConnecting(port: string) {
  connection.isConnecting = true;
  connection.isReconnecting = false;
  connection.portName = port;
  connection.status = 'connecting';
  connection.error = null;
}

/** Set reconnecting state (auto-reconnect in progress) */
export function setReconnecting(port: string) {
  connection.isConnected = false;
  connection.isConnecting = false;
  connection.isReconnecting = true;
  connection.portName = port;
  connection.status = 'reconnecting';
  connection.error = null;
}

/** v0.3.6 — Sync board settings from board-sync event (board is source of truth) */
export function applyBoardSync(bs: BoardSettings) {
  connection.nodeAddress = bs.nodeAddress;
  connection.gatewayMode = bs.gatewayMode;
  if (bs.wifiEnabled !== undefined) connection.wifiEnabled = bs.wifiEnabled;
}

/** v0.3.7 — Set address conflict flag */
export function setAddrConflict(detected: boolean) {
  connection.addrConflict = detected;
}

/** v0.3.7 — Update neighbors list */
export function setNeighbors(neighbors: NeighborEntry[]) {
  connection.neighbors = neighbors;
}

/** v0.3.6 — Update gateway online status from gateway-status event */
export function setGatewayOnline(online: boolean) {
  connection.gatewayOnline = online;
}

/** Set disconnected state */
export function setDisconnected() {
  connection.isConnected = false;
  connection.isConnecting = false;
  connection.isReconnecting = false;
  connection.portName = '';
  connection.nodeAddress = null;
  connection.status = 'disconnected';
  connection.stats = null;
  connection.firmwareVersion = null;
  connection.gatewayMode = false;
  connection.gatewayOnline = false;
  connection.wifiEnabled = true;
  connection.addrConflict = false;
  connection.neighbors = [];
  connection.batteryMv = null;
  connection.boardMac = null;
}

/** Set error state */
export function setError(msg: string) {
  connection.isConnected = false;
  connection.isConnecting = false;
  connection.isReconnecting = false;
  connection.status = 'error';
  connection.error = msg;
}

/** Update radio statistics */
export function updateStats(stats: RadioStats) {
  connection.stats = stats;
}

/** v0.3.8 — Update battery voltage */
export function setBatteryMv(mv: number | null) {
  connection.batteryMv = mv;
}

/** v0.3.8 — Update board MAC address */
export function setBoardMac(mac: string | null) {
  connection.boardMac = mac;
}
