/**
 * RadioDoge connection bridge — v0.3.10
 *
 * Platform-aware abstraction over serial communication:
 *
 *   Desktop (Windows / Linux / macOS)
 *     → The Rust `SerialManager` owns the port.
 *       All operations go through existing `invoke('connect_port', …)` etc.
 *
 *   Android (USB-OTG via CP2102 / CH340 on a Heltec ESP32)
 *     → `tauri-plugin-serialplugin` owns the port via `usb-serial-for-android`.
 *       Physical I/O stays in JS; protocol encoding/decoding is delegated back
 *       to Rust via the `mobile_*` commands so there is zero duplication.
 *
 *   Android Bluetooth (experimental, BLE scan only for now)
 *     → Stub — UI framework is wired up, full BLE GATT characteristic comms
 *       will land once the firmware BLE service UUID is finalised.
 *
 * Usage:
 *   import { bridge } from '$lib/device/connection-bridge';
 *   const android = await bridge.isAndroid();
 *   const devices = await bridge.listDevices();
 *   await bridge.connect(devices[0].path);
 *   await bridge.disconnect();
 */

import { invoke } from '@tauri-apps/api/core';
import { platform } from '@tauri-apps/plugin-os';
import { listen } from '@tauri-apps/api/event';
import type { UnlistenFn } from '@tauri-apps/api/event';
import { SerialPort } from '@s00d/tauri-plugin-serialplugin';
import type { LoraSettings, TransactionRequest } from '$lib/types';

// ─── Types ────────────────────────────────────────────────────────────────────

/** A serial/Bluetooth device discovered on this platform. */
export interface MobileDeviceInfo {
  /** OS path or device address, e.g. "/dev/bus/usb/001/002" or a BT MAC */
  path: string;
  /** Human-readable label shown in the device picker */
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

/** Lifecycle status of the Android mobile connection (not the same as
 *  ConnectionStatusType in the store — this is just for the UI state machine). */
export type MobileConnectionStatus =
  | 'idle'        // Nothing happening
  | 'searching'   // Device scan in progress
  | 'connecting'  // Opening serial port, waiting for board response
  | 'connected'   // Board responded to GET_SETTINGS — fully ready
  | 'failed';     // Open or handshake failed

// ─── USB VID/PID recognition ─────────────────────────────────────────────────

// Vendor IDs for common ESP32 USB adapters used on Heltec boards
const HELTEC_USB_VIDS = new Set([
  0x10C4, // Silicon Labs CP210x (most Heltec V3)
  0x1A86, // WCH CH340 / CH9102
  0x0403, // FTDI
  0x303A, // Espressif native USB (ESP32-S3 CDC)
]);

function isLikelyHeltecDevice(vid: number | undefined): boolean {
  return vid != null && HELTEC_USB_VIDS.has(vid);
}

// ─── Platform detection ───────────────────────────────────────────────────────

let _platformCache: string | null = null;

/** Lazily cached platform string ("android", "windows", "linux", …). */
export async function getPlatform(): Promise<string> {
  if (_platformCache === null) {
    _platformCache = await platform();
  }
  return _platformCache;
}

/** Returns true when running on Android. */
export async function isAndroid(): Promise<boolean> {
  return (await getPlatform()) === 'android';
}

// ─── Active Android connection state ─────────────────────────────────────────

/** Currently open serial port on Android (null when disconnected). */
let activePort: InstanceType<typeof SerialPort> | null = null;

/** Teardown function returned by the serialplugin's listen() call. */
let activeListenerTeardown: (() => void) | null = null;

/** Tauri event unlisten handles registered during a session. */
let sessionUnlistens: UnlistenFn[] = [];

// ─── Device discovery ─────────────────────────────────────────────────────────

/**
 * List available serial devices.
 *
 * Desktop: proxies through `list_ports_detailed` Rust command.
 * Android: queries USB devices via @s00d/tauri-plugin-serialplugin.
 */
export async function listDevices(): Promise<MobileDeviceInfo[]> {
  if (await isAndroid()) {
    return _listAndroidDevices();
  }
  return _listDesktopPorts();
}

async function _listDesktopPorts(): Promise<MobileDeviceInfo[]> {
  const ports = await invoke<Array<{
    name: string;
    description: string;
    isLikelyHeltec: boolean;
    isUsb: boolean;
    vid?: number;
    pid?: number;
  }>>('list_ports_detailed');

  return ports.map(p => ({
    path: p.name,
    description: p.description,
    type: (p.isUsb ? 'usb' : 'unknown') as MobileDeviceInfo['type'],
    isLikelyHeltec: p.isLikelyHeltec,
    vid: p.vid,
    pid: p.pid,
  }));
}

async function _listAndroidDevices(): Promise<MobileDeviceInfo[]> {
  let portsMap: Record<string, Record<string, unknown>>;
  try {
    portsMap = (await SerialPort.available_ports()) as Record<string, Record<string, unknown>>;
  } catch (e) {
    console.warn('[bridge] available_ports() failed:', e);
    return [];
  }

  const devices: MobileDeviceInfo[] = [];

  for (const [path, info] of Object.entries(portsMap)) {
    // The plugin returns slightly different shapes on different versions;
    // guard each access so we never throw on unexpected shapes.
    const typeRaw = String((info.type ?? info.port_type ?? '')).toLowerCase();
    const isBluetooth = typeRaw.includes('bluetooth') || typeRaw.includes('bt');
    const connType: MobileDeviceInfo['type'] = isBluetooth ? 'bluetooth' : 'usb';

    // VID/PID may be nested under vid_pid or at the top level
    const vid: number | undefined =
      (info.vid_pid as Record<string, number> | undefined)?.vendor_id ??
      (info.vid as number | undefined);
    const pid: number | undefined =
      (info.vid_pid as Record<string, number> | undefined)?.product_id ??
      (info.pid as number | undefined);

    const likelyHeltec = connType === 'usb' && isLikelyHeltecDevice(vid);

    devices.push({
      path,
      description: _buildLabel(path, vid, pid, connType),
      type: connType,
      isLikelyHeltec: likelyHeltec,
      vid,
      pid,
    });
  }

  // Sort: Heltec-detected first, then USB, then BT, then rest; stable within groups
  return devices.sort((a, b) => {
    if (a.isLikelyHeltec !== b.isLikelyHeltec) return a.isLikelyHeltec ? -1 : 1;
    const typeOrder = { usb: 0, bluetooth: 1, unknown: 2 };
    const ta = typeOrder[a.type] ?? 2;
    const tb = typeOrder[b.type] ?? 2;
    if (ta !== tb) return ta - tb;
    return a.path.localeCompare(b.path);
  });
}

function _buildLabel(
  path: string,
  vid?: number,
  pid?: number,
  type?: string,
): string {
  if (vid != null && pid != null) {
    const v = vid.toString(16).padStart(4, '0').toUpperCase();
    const p = pid.toString(16).padStart(4, '0').toUpperCase();
    return `${path}  (VID:${v} PID:${p})`;
  }
  if (type === 'bluetooth') return `${path}  (Bluetooth)`;
  return path;
}

// ─── Connection ───────────────────────────────────────────────────────────────

/**
 * Connect to a device.
 *
 * Desktop: delegates to the existing `connect_port` Rust command, which opens
 * the serial port, runs the firmware/settings handshake, and emits events.
 *
 * Android: opens the USB serial port via the JS plugin, tells the Rust backend
 * it is connecting, then sends the two-packet connect sequence. The board's
 * responses arrive as bytes via the listener → `mobile_push_bytes` → Rust
 * emits the same "board-sync" / "connection-status" events the frontend already
 * handles for desktop connections.
 */
export async function connect(devicePath: string): Promise<void> {
  if (await isAndroid()) {
    return _connectAndroid(devicePath);
  }
  await invoke('connect_port', { port: devicePath });
}

async function _connectAndroid(devicePath: string): Promise<void> {
  // Tear down any previous session cleanly
  await _disconnectAndroid(/* notifyRust */ false);

  // Tell the Rust backend we are opening a connection so it emits
  // "connection-status: connecting" (same event shape as desktop).
  await invoke('mobile_set_connected', { devicePath });

  // Open the physical port — this triggers Android's USB permission dialog
  // the first time a new device is attached.
  try {
    activePort = new SerialPort({ path: devicePath, baudRate: 115200 });
    await activePort.open();
  } catch (e) {
    await invoke('mobile_set_disconnected');
    throw new Error(`USB serial open failed: ${e}`);
  }

  // Register a data listener. Every chunk of received bytes is fed into the
  // Rust accumulator which handles framing, packet parsing, and event emission.
  try {
    const teardown = await activePort.listen((data: Uint8Array | number[]) => {
      const bytes = Array.isArray(data) ? data : Array.from(data);
      invoke('mobile_push_bytes', { bytes, rssi: 0 }).catch(
        (err) => console.warn('[bridge] mobile_push_bytes error:', err),
      );
    });
    activeListenerTeardown = typeof teardown === 'function' ? teardown : null;
  } catch (e) {
    await _closePort();
    await invoke('mobile_set_disconnected');
    throw new Error(`USB listener setup failed: ${e}`);
  }

  // Listen for the mobile firmware-version event so we can surface it if
  // it arrives before the GET_SETTINGS response (which triggers "connected").
  const unlistenFw = await listen<string>('mobile-firmware-version', (ev) => {
    // Expose via a custom DOM event — ConnectionPanel picks this up
    window.dispatchEvent(new CustomEvent('radiodoge:mobile-fw', { detail: ev.payload }));
  });
  sessionUnlistens.push(unlistenFw);

  // Also forward mobile DOGE TX notifications to the notification system
  const unlistenTx = await listen<{ body: string }>('mobile-doge-tx-received', (ev) => {
    window.dispatchEvent(new CustomEvent('radiodoge:doge-tx', { detail: ev.payload.body }));
  });
  sessionUnlistens.push(unlistenTx);

  // Send the connect handshake: GET_FIRMWARE_VERSION + GET_SETTINGS
  // The board's replies arrive async via the listener above.
  const initPackets = await invoke<number[][]>('mobile_build_connect_queries');
  for (let i = 0; i < initPackets.length; i++) {
    await activePort.write(new Uint8Array(initPackets[i]));
    // Small gap so the board has time to process and respond before the next query
    if (i < initPackets.length - 1) {
      await _sleep(80);
    }
  }
}

/** Disconnect from the current device. */
export async function disconnect(): Promise<void> {
  if (await isAndroid()) {
    return _disconnectAndroid(/* notifyRust */ true);
  }
  await invoke('disconnect_port');
}

async function _disconnectAndroid(notifyRust: boolean): Promise<void> {
  // Cancel all session-scoped event listeners
  for (const unlisten of sessionUnlistens) {
    try { unlisten(); } catch { /* ignore */ }
  }
  sessionUnlistens = [];

  // Tear down the serial listener before closing the port
  if (activeListenerTeardown) {
    try { activeListenerTeardown(); } catch { /* ignore */ }
    activeListenerTeardown = null;
  }

  await _closePort();

  if (notifyRust) {
    await invoke('mobile_set_disconnected').catch(() => { /* best-effort */ });
    await invoke('mobile_clear_accumulator').catch(() => { /* best-effort */ });
  }
}

async function _closePort(): Promise<void> {
  if (activePort) {
    try { await activePort.close(); } catch { /* ignore */ }
    activePort = null;
  }
}

// ─── Outgoing packet helpers (Android only) ───────────────────────────────────

/**
 * Send a PING to the board on Android.
 * Rust builds the correct byte sequence; JS writes it to the open serial port.
 */
export async function mobilePing(): Promise<void> {
  _requireActivePort();
  const bytes = await invoke<number[]>('mobile_build_ping');
  await activePort!.write(new Uint8Array(bytes));
}

/**
 * Send a Dogecoin transaction on Android.
 * Handles single and multipart payloads transparently.
 */
export async function mobileSendTransaction(tx: TransactionRequest): Promise<void> {
  _requireActivePort();
  const packets = await invoke<number[][]>('mobile_build_tx_packets', { tx });
  for (let i = 0; i < packets.length; i++) {
    await activePort!.write(new Uint8Array(packets[i]));
    if (packets.length > 1 && i < packets.length - 1) {
      // Inter-packet delay for multipart sequences — gives the board
      // time to store each part before the next arrives.
      await _sleep(120);
    }
  }
}

/**
 * Send a LoRa settings update packet on Android.
 */
export async function mobileSendLoraSettings(settings: LoraSettings): Promise<void> {
  _requireActivePort();
  const bytes = await invoke<number[]>('mobile_build_lora_settings_packet', { settings });
  await activePort!.write(new Uint8Array(bytes));
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

function _requireActivePort(): void {
  if (!activePort) throw new Error('No active Android serial connection');
}

function _sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// ─── Public API ──────────────────────────────────────────────────────────────

export const bridge = {
  isAndroid,
  getPlatform,
  listDevices,
  connect,
  disconnect,
  mobilePing,
  mobileSendTransaction,
  mobileSendLoraSettings,
};
