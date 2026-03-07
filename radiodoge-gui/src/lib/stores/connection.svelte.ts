/**
 * Connection state store using Svelte 5 runes.
 *
 * This module holds all state related to the serial connection and
 * radio statistics. It's imported directly into components via
 * `import { connection } from '$lib/stores/connection.svelte'`.
 */

import type { ConnectionStatusType, NodeAddress, RadioStats } from '$lib/types';

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
});

/** Update available ports list */
export function setAvailablePorts(ports: string[]) {
  connection.availablePorts = ports;
}

/** Set connected state */
export function setConnected(port: string, nodeAddress: NodeAddress | null) {
  connection.isConnected = true;
  connection.isConnecting = false;
  connection.portName = port;
  connection.nodeAddress = nodeAddress;
  connection.status = 'connected';
  connection.error = null;
}

/** Set connecting state */
export function setConnecting(port: string) {
  connection.isConnecting = true;
  connection.portName = port;
  connection.status = 'connecting';
  connection.error = null;
}

/** Set disconnected state */
export function setDisconnected() {
  connection.isConnected = false;
  connection.isConnecting = false;
  connection.portName = '';
  connection.nodeAddress = null;
  connection.status = 'disconnected';
  connection.stats = null;
}

/** Set error state */
export function setError(msg: string) {
  connection.isConnected = false;
  connection.isConnecting = false;
  connection.status = 'error';
  connection.error = msg;
}

/** Update radio statistics */
export function updateStats(stats: RadioStats) {
  connection.stats = stats;
}
