/**
 * RadioDoge connection bridge — v0.3.16
 *
 * Platform-aware abstraction over serial communication:
 *
 *   Desktop (Windows / Linux / macOS)
 *     → The Rust `SerialManager` owns the port.
 *       All operations go through existing `invoke('connect_port', …)` etc.
 *
 *   Android USB-OTG (CP2102 / CH340 on a Heltec ESP32)
 *     → `tauri-plugin-serialplugin` owns the port via `usb-serial-for-android`.
 *       Physical I/O stays in JS; protocol encoding/decoding is delegated back
 *       to Rust via the `mobile_*` commands so there is zero duplication.
 *
 *   Android Bluetooth BLE (Heltec ESP32 firmware BLE serial service)
 *     → `@mnlphlp/plugin-blec` (tauri-plugin-blec) handles GATT scan/connect/write.
 *       Incoming characteristic notifications feed into `mobile_push_bytes` via the
 *       same accumulator pipeline as USB — zero protocol duplication between transports.
 *       GATT UUIDs are Nordic UART Service (NUS) — matching heltec-firmware-v3.
 *
 * Data pipeline (both USB and BLE on Android):
 *   Raw bytes → invoke('mobile_push_bytes') → Rust accumulator + parser
 *     → "radio-packet" / "board-sync" / "connection-status" Tauri events
 *     → existing frontend stores and UI — exactly like desktop.
 *
 * Usage:
 *   import { bridge } from '$lib/device/connection-bridge';
 *   const devices = await bridge.listDevices();          // USB devices
 *   const bleDevs = await bridge.bleScan(5000);          // BLE scan (5 s)
 *   await bridge.connect(device.path, 'usb');            // USB-OTG
 *   await bridge.connect(device.path, 'bluetooth');      // BLE
 *   await bridge.disconnect();
 */

import { invoke } from '@tauri-apps/api/core';
import { platform } from '@tauri-apps/plugin-os';
import { listen } from '@tauri-apps/api/event';
import type { UnlistenFn } from '@tauri-apps/api/event';
import { SerialPort } from 'tauri-plugin-serialplugin';
import type { LoraSettings, TransactionRequest } from '$lib/types';
import { setConnected, setDisconnected } from '$lib/stores/connection.svelte';

// ─── Types ────────────────────────────────────────────────────────────────────

/** A serial/Bluetooth device discovered on this platform. */
export interface MobileDeviceInfo {
  /** OS path or BT MAC address, e.g. "/dev/bus/usb/001/002" or "AA:BB:CC:DD:EE:FF" */
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
  /** RSSI at scan time (BLE only) */
  rssi?: number;
}

/** Lifecycle status of the Android mobile connection. */
export type MobileConnectionStatus =
  | 'idle'        // Nothing happening
  | 'searching'   // Device scan in progress
  | 'connecting'  // Opening port / GATT connect, waiting for board response
  | 'connected'   // Board responded to GET_SETTINGS — fully ready
  | 'failed';     // Open or handshake failed

// ─── BLE GATT UUIDs — Nordic UART Service (NUS) ──────────────────────────────
//
// The firmware in heltec-firmware-v3/heltec-firmware.ino implements a Nordic
// UART Service (NUS).  These UUIDs are registered with the Bluetooth SIG and
// must match exactly what the board advertises:
//
//   Service          — 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
//   RX characteristic — 6E400002  (WRITE, host → board; "RX" from board's POV)
//   TX characteristic — 6E400003  (NOTIFY, board → host; "TX" from board's POV)
//
// From the host's perspective:
//   BLE_WRITE_CHAR_UUID  = NUS RX char  (we write to it)
//   BLE_NOTIFY_CHAR_UUID = NUS TX char  (we subscribe for notifications)

const BLE_SERVICE_UUID     = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E'; // NUS service
const BLE_WRITE_CHAR_UUID  = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E'; // NUS RX — host writes
const BLE_NOTIFY_CHAR_UUID = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E'; // NUS TX — board notifies

// ─── USB VID/PID recognition ─────────────────────────────────────────────────

const HELTEC_USB_VIDS = new Set([
  0x10C4, // Silicon Labs CP210x (most Heltec V3)
  0x1A86, // WCH CH340 / CH9102
  0x0403, // FTDI
  0x303A, // Espressif native USB (ESP32-S3 CDC)
]);

function isLikelyHeltecDevice(vid: number | undefined): boolean {
  return vid != null && HELTEC_USB_VIDS.has(vid);
}

// ─── Lazy blec import (only loaded when BLE is actually used) ─────────────────
//
// Dynamic import prevents the blec module from running any top-level
// initialisation on desktop startup.  BLE code paths are only entered when
// isAndroidPlatform === true in the UI.

type BlecModule = typeof import('@mnlphlp/plugin-blec');
let _blecCache: BlecModule | null = null;

async function _blec(): Promise<BlecModule> {
  if (!_blecCache) {
    _blecCache = await import('@mnlphlp/plugin-blec');
  }
  return _blecCache;
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

// ─── Active connection state ──────────────────────────────────────────────────

/** Currently open USB serial port on Android (null when disconnected). */
let activePort: InstanceType<typeof SerialPort> | null = null;

/**
 * True while the USB polling read loop is running.
 * Set to false to stop the loop (on disconnect or error).
 */
let activeReadLoopRunning = false;

/** BLE device address (MAC) when connected over BLE (null when disconnected). */
let activeBleAddress: string | null = null;

/** Teardown function for the BLE characteristic notification subscription. */
let bleNotifyTeardown: (() => void) | null = null;

/** Tauri event unlisten handles registered during a session (both USB and BLE). */
let sessionUnlistens: UnlistenFn[] = [];

// ─── BLE device discovery (scan) ─────────────────────────────────────────────

/**
 * Scan for nearby BLE devices for `timeoutMs` milliseconds.
 *
 * Notifies Rust via `mobile_ble_scan` (state update + debug log) then calls
 * the blec plugin's `scan()` which runs a BLE discovery pass.
 * Returns discovered devices as `MobileDeviceInfo[]` with type='bluetooth'.
 */
export async function bleScan(timeoutMs = 5000): Promise<MobileDeviceInfo[]> {
  // Notify Rust: update connection_type, emit ble-scan-started event
  await invoke('mobile_ble_scan');

  const blec = await _blec();
  let rawDevices: Array<{ address: string; name?: string | null; rssi?: number | null }> = [];
  try {
    rawDevices = await blec.scan(timeoutMs);
  } catch (e) {
    console.warn('[bridge-ble] scan() failed:', e);
    return [];
  }

  return rawDevices.map(d => ({
    path: d.address,
    description: d.name ? `${d.name}  (${d.address})` : d.address,
    type: 'bluetooth' as const,
    isLikelyHeltec: _isLikelyHeltecBle(d.name ?? null),
    rssi: d.rssi ?? undefined,
  }));
}

/** Heuristic: device name starts with "Heltec" or "RadioDoge". */
function _isLikelyHeltecBle(name: string | null): boolean {
  if (!name) return false;
  const n = name.toLowerCase();
  return n.startsWith('heltec') || n.startsWith('radiodoge') || n.startsWith('esp32');
}

// ─── USB device discovery ─────────────────────────────────────────────────────

/**
 * List available USB serial devices.
 *
 * Desktop: proxies through `list_ports_detailed` Rust command.
 * Android: queries USB devices via tauri-plugin-serialplugin.
 * (Does NOT return BLE devices — use bleScan() for those.)
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
    const typeRaw = String((info.type ?? info.port_type ?? '')).toLowerCase();
    const isBluetooth = typeRaw.includes('bluetooth') || typeRaw.includes('bt');
    const connType: MobileDeviceInfo['type'] = isBluetooth ? 'bluetooth' : 'usb';

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

  return devices.sort((a, b) => {
    if (a.isLikelyHeltec !== b.isLikelyHeltec) return a.isLikelyHeltec ? -1 : 1;
    const typeOrder = { usb: 0, bluetooth: 1, unknown: 2 };
    return (typeOrder[a.type] ?? 2) - (typeOrder[b.type] ?? 2) || a.path.localeCompare(b.path);
  });
}

function _buildLabel(path: string, vid?: number, pid?: number, type?: string): string {
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
 * Desktop: delegates to the existing `connect_port` Rust command.
 *
 * Android USB: opens the serial port via tauri-plugin-serialplugin, feeds
 * received bytes into `mobile_push_bytes`.
 *
 * Android BLE: connects via tauri-plugin-blec, subscribes to GATT notify
 * characteristic, feeds received bytes into `mobile_push_bytes`.
 *
 * Both Android paths share the same downstream pipeline: mobile_push_bytes
 * → Rust accumulator → "radio-packet" / "board-sync" / "connection-status"
 * events → existing frontend stores.
 *
 * @param devicePath  USB device path or BLE MAC address
 * @param type        'usb' | 'bluetooth' (only used on Android; ignored on desktop)
 */
export async function connect(devicePath: string, type?: 'usb' | 'bluetooth'): Promise<void> {
  if (await isAndroid()) {
    if (type === 'bluetooth') {
      return _connectBluetoothAndroid(devicePath);
    }
    return _connectAndroid(devicePath);
  }
  await invoke('connect_port', { port: devicePath });
}

// ─── Raw-bytes extraction helper ─────────────────────────────────────────────
//
// Used by both the USB polling loop and the BLE onReceiveData path to normalise
// whatever shape the underlying transport delivers into a flat number[].
//
// USB polling path: invoke('plugin:serialplugin|read_binary', ...) returns
//   Vec<u8> from Rust, which Tauri IPC serialises to JSON number[].
// BLE notify path: blec.onReceiveData delivers Uint8Array.

/**
 * Extract raw bytes from whatever shape the transport layer delivers:
 *
 *   number[]           — standard Tauri IPC result for Vec<u8> (USB polling)
 *   Uint8Array         — typed array (BLE notify, some native paths)
 *   ArrayBuffer        — raw buffer (some native edge cases)
 *   { data: ... }      — wrapped form (defensive, some plugin versions)
 *
 * Returns an empty array for any unrecognised shape.
 */
function _extractBytes(raw: unknown): number[] {
  if (raw instanceof Uint8Array) return Array.from(raw);
  if (raw instanceof ArrayBuffer) return Array.from(new Uint8Array(raw));
  if (Array.isArray(raw)) return raw as number[];
  if (raw !== null && typeof raw === 'object') {
    const inner = (raw as Record<string, unknown>).data;
    if (inner instanceof Uint8Array) return Array.from(inner);
    if (inner instanceof ArrayBuffer) return Array.from(new Uint8Array(inner));
    if (Array.isArray(inner)) return inner as number[];
  }
  return [];
}

// ─── Android USB connect ──────────────────────────────────────────────────────

async function _connectAndroid(devicePath: string): Promise<void> {
  await _disconnectAndroid(/* notifyRust */ false);

  await invoke('mobile_set_connected', { devicePath });

  try {
    activePort = new SerialPort({ path: devicePath, baudRate: 115200 });
    await activePort.open();
  } catch (e) {
    await invoke('mobile_set_disconnected');
    throw new Error(`USB serial open failed: ${e}`);
  }

  // Kick off the polling read loop as a fire-and-forget task.
  // It runs until activeReadLoopRunning is set to false (disconnect / error).
  // We do NOT use startListening()+listen() here because the Android Kotlin code
  // emits "serialData" events (not the "plugin-serialplugin-read-*" name that
  // the JS listen() wrapper subscribes to) and encodes data as a UTF-8 string
  // (not a number[]), making the event-based path completely unusable.
  // Using read_binary() directly is the same IPC path as writeBinary() — known
  // to work — and is simpler, more reliable, and free of event-name fragility.
  _startReadLoop(devicePath).catch(
    (err) => console.warn('[bridge] USB read loop crashed unexpectedly:', err),
  );

  await _registerSessionListeners();

  // Immediately mark global connection state as connected so the Disconnect
  // button appears and isConnected-gated tabs/features become visible.
  // The board will respond to GET_SETTINGS asynchronously and emit a
  // "connection-status: connected" event which will re-call setConnected with
  // the real nodeAddress/firmwareVersion.
  setConnected(devicePath, null);

  const initPackets = await invoke<number[][]>('mobile_build_connect_queries');
  for (let i = 0; i < initPackets.length; i++) {
    await _usbWrite(new Uint8Array(initPackets[i]));
    if (i < initPackets.length - 1) await _sleep(80);
  }
}

// ─── Android BLE connect ──────────────────────────────────────────────────────

async function _connectBluetoothAndroid(address: string): Promise<void> {
  // Tear down any prior BLE session
  await _disconnectBluetoothAndroid(/* notifyRust */ false);

  // Notify Rust: sets ble_device_address, connection_type, emits "connecting"
  await invoke('mobile_ble_connect', { address });

  const blec = await _blec();

  // Connect via the blec plugin (triggers Android BLE GATT connect)
  try {
    await blec.connect(address, () => {
      // onDisconnect: board dropped the connection unexpectedly — reset global state
      console.warn('[bridge-ble] BLE device disconnected:', address);
      activeBleAddress = null;
      invoke('mobile_ble_disconnect').catch(() => {});
      invoke('mobile_clear_accumulator').catch(() => {});
      setDisconnected();
    });
  } catch (e) {
    await invoke('mobile_ble_disconnect').catch(() => {});
    throw new Error(`BLE connect failed: ${e}`);
  }

  activeBleAddress = address;

  // Subscribe to characteristic notifications (board → host data path).
  // Each notification chunk feeds directly into mobile_push_bytes, which runs
  // the identical accumulator + framing logic as the USB-OTG path.
  try {
    const teardown = await blec.onReceiveData(
      BLE_SERVICE_UUID,
      BLE_NOTIFY_CHAR_UUID,
      (data: unknown) => {
        // BLE notifications are typically Uint8Array but use _extractBytes()
        // for defensive compatibility with any wrapper format.
        const bytes = _extractBytes(data);
        if (bytes.length === 0) return;
        invoke('mobile_push_bytes', { bytes, rssi: 0 }).catch(
          (err) => console.warn('[bridge-ble] mobile_push_bytes error:', err),
        );
      },
    );
    bleNotifyTeardown = typeof teardown === 'function' ? teardown : null;
  } catch (e) {
    // Non-fatal: we can still send but won't receive from the board.
    console.warn('[bridge-ble] notification subscribe failed (UUID mismatch?):', e);
  }

  // Register the same Tauri event listeners as the USB path
  await _registerSessionListeners();

  // Mark global connection state as connected immediately after GATT is up.
  setConnected(address, null);

  // Send the connect handshake: GET_FIRMWARE_VERSION + GET_SETTINGS.
  // Board responses arrive as BLE notifications → mobile_push_bytes.
  const initPackets = await invoke<number[][]>('mobile_build_connect_queries');
  for (let i = 0; i < initPackets.length; i++) {
    await _bleWrite(new Uint8Array(initPackets[i]));
    if (i < initPackets.length - 1) await _sleep(80);
  }
}

// ─── Disconnect ───────────────────────────────────────────────────────────────

/** Disconnect from the current device (USB or BLE). */
export async function disconnect(): Promise<void> {
  if (await isAndroid()) {
    if (activeBleAddress) {
      return _disconnectBluetoothAndroid(/* notifyRust */ true);
    }
    return _disconnectAndroid(/* notifyRust */ true);
  }
  await invoke('disconnect_port');
}

async function _disconnectAndroid(notifyRust: boolean): Promise<void> {
  _clearSessionListeners();

  // Tell the polling read loop to stop.  It will exit within at most one
  // 100 ms read window (the current read_binary call).
  activeReadLoopRunning = false;

  await _closePort();

  if (notifyRust) {
    await invoke('mobile_set_disconnected').catch(() => {});
    await invoke('mobile_clear_accumulator').catch(() => {});
    setDisconnected();
  }
}

// ─── Android USB polling read loop ───────────────────────────────────────────
//
// Why polling instead of startListening()+listen()?
//
// The tauri-plugin-serialplugin Android (Kotlin) backend emits data via
// `trigger("serialData", { path, data: String(chunk), size })`.
// The JS plugin wrapper's listen() subscribes to
// `plugin-serialplugin-read-${encodedPath}` — a completely different event name.
// Even if the name matched, the payload.data is a UTF-8 encoded string
// (String(byteArray) in Kotlin) and `new Uint8Array(string)` throws a
// RangeError, which the plugin catches silently.
//
// read_binary() uses the same synchronous IPC path as writeBinary() (which is
// known to work): JS invoke → Rust command → run_mobile_plugin("readBinary")
// → Kotlin reads USB port → returns bytes → Rust → JS.  No events, no
// encoding ambiguity, no event-name fragility.
//
// Each loop iteration:
//   1. invoke('plugin:serialplugin|read_binary', { path, timeout: 100, size: 1024 })
//   2. bytes.length > 0 → invoke('mobile_push_bytes', { bytes, rssi: 0 })
//   3. "no data within N ms" timeout errors are silently skipped.
//   4. Any other error (device unplugged, port closed) exits the loop.

async function _startReadLoop(devicePath: string): Promise<void> {
  activeReadLoopRunning = true;
  while (activeReadLoopRunning) {
    try {
      const raw = await invoke('plugin:serialplugin|read_binary', {
        path: devicePath,
        timeout: 100,    // 100 ms read window per iteration
        size:    1024,
      });
      const bytes = _extractBytes(raw);
      if (bytes.length > 0) {
        invoke('mobile_push_bytes', { bytes, rssi: 0 }).catch(
          (err) => console.warn('[bridge] mobile_push_bytes error:', err),
        );
      }
    } catch (e: unknown) {
      if (!activeReadLoopRunning) break;  // Normal shutdown — silently exit
      const msg = String(e).toLowerCase();
      // A read timeout (no data in the 100 ms window) is normal and expected.
      if (
        msg.includes('no data') ||
        msg.includes('timeout') ||
        msg.includes('timed out') ||
        msg.includes('timedout')
      ) {
        continue;
      }
      // Any other error means the port is closed or device was unplugged.
      console.warn('[bridge] USB read loop: port error, stopping:', e);
      break;
    }
  }
  activeReadLoopRunning = false;
}

async function _disconnectBluetoothAndroid(notifyRust: boolean): Promise<void> {
  _clearSessionListeners();

  // Unsubscribe from BLE notifications
  if (bleNotifyTeardown) {
    try { bleNotifyTeardown(); } catch { /* ignore */ }
    bleNotifyTeardown = null;
  }

  // Disconnect GATT
  if (activeBleAddress) {
    try {
      const blec = await _blec();
      await blec.disconnect();
    } catch { /* ignore — device may already be gone */ }
    activeBleAddress = null;
  }

  if (notifyRust) {
    await invoke('mobile_ble_disconnect').catch(() => {});
    await invoke('mobile_clear_accumulator').catch(() => {});
    setDisconnected();
  }
}

// ─── Shared session listeners ─────────────────────────────────────────────────

/** Register Tauri event listeners that apply to both USB and BLE sessions. */
async function _registerSessionListeners(): Promise<void> {
  const unlistenFw = await listen<string>('mobile-firmware-version', (ev) => {
    window.dispatchEvent(new CustomEvent('radiodoge:mobile-fw', { detail: ev.payload }));
  });
  sessionUnlistens.push(unlistenFw);

  const unlistenTx = await listen<{ body: string }>('mobile-doge-tx-received', (ev) => {
    window.dispatchEvent(new CustomEvent('radiodoge:doge-tx', { detail: ev.payload.body }));
  });
  sessionUnlistens.push(unlistenTx);
}

function _clearSessionListeners(): void {
  for (const unlisten of sessionUnlistens) {
    try { unlisten(); } catch { /* ignore */ }
  }
  sessionUnlistens = [];
}

// ─── BLE write helper ─────────────────────────────────────────────────────────

/**
 * Write `data` to the BLE TX characteristic.
 *
 * Logs the bytes to the debug traffic stream via `mobile_ble_write_characteristic`
 * (visible in the Debug Console tab), then performs the GATT write via blec.
 */
async function _bleWrite(data: Uint8Array): Promise<void> {
  if (!activeBleAddress) throw new Error('No active BLE connection');
  // Debug logging (non-blocking — fire-and-forget is fine here)
  invoke('mobile_ble_write_characteristic', { data: Array.from(data) }).catch(() => {});
  const blec = await _blec();
  await blec.sendData(BLE_SERVICE_UUID, BLE_WRITE_CHAR_UUID, data);
}

// ─── Port helpers ─────────────────────────────────────────────────────────────

async function _closePort(): Promise<void> {
  if (activePort) {
    try { await activePort.close(); } catch { /* ignore */ }
    activePort = null;
  }
}

/**
 * Write `data` to the active USB serial port and emit TX debug traffic.
 *
 * Mirrors _bleWrite() for the USB path: the actual byte write goes via the
 * serialplugin's writeBinary, and a fire-and-forget invoke to mobile_emit_debug_tx
 * makes the bytes visible in the Debug Console.
 */
async function _usbWrite(data: Uint8Array): Promise<void> {
  _requireActivePort();
  await activePort!.writeBinary(data);
  invoke('mobile_emit_debug_tx', { bytes: Array.from(data) }).catch(() => {});
}

function _sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// ─── Outgoing packet helpers (Android — USB and BLE) ─────────────────────────

/**
 * Send a PING and wait up to `timeoutMs` ms for the board to echo it back.
 *
 * Mirrors the desktop `ping_device` Rust command which blocks until it receives
 * a CMD_PING (0x02) reply or times out after 500 ms.
 *
 * Implementation:
 *   1. Register a one-shot `radio-packet` listener BEFORE writing the PING
 *      bytes to avoid a race condition where the PONG arrives before we start
 *      listening.
 *   2. Write the PING via USB or BLE.
 *   3. Resolve true when CMD_PING arrives; resolve false on timeout (500 ms).
 *
 * Returns true if a PONG was received, false on timeout.
 */
export async function mobilePingWait(timeoutMs = 2000): Promise<boolean> {
  const pingBytes = await invoke<number[]>('mobile_build_ping');

  // Deferred-resolve pattern: create the promise first, capture its resolver.
  let resolvePong!: (v: boolean) => void;
  const pong = new Promise<boolean>((r) => { resolvePong = r; });
  let settled = false;
  let timerId: ReturnType<typeof setTimeout> | undefined;

  // Start as a no-op; overwritten once listen() resolves (before any event
  // can fire because events are only dispatched after the current microtask).
  let unlisten: () => void = () => {};

  const ul = await listen<{ command: number }>('radio-packet', (ev) => {
    if (!settled && ev.payload.command === 0x02 /* CMD_PING */) {
      settled = true;
      if (timerId !== undefined) clearTimeout(timerId);
      unlisten();
      resolvePong(true);
    }
  });
  unlisten = ul;

  timerId = setTimeout(() => {
    if (!settled) {
      settled = true;
      unlisten();
      resolvePong(false);
    }
  }, timeoutMs);

  // Send the PING only after the listener is active.
  try {
    if (activeBleAddress) {
      await _bleWrite(new Uint8Array(pingBytes));
    } else {
      await _usbWrite(new Uint8Array(pingBytes));
    }
  } catch (e) {
    if (!settled) {
      settled = true;
      if (timerId !== undefined) clearTimeout(timerId);
      unlisten();
      resolvePong(false);
    }
    throw e;
  }

  return pong;
}

/**
 * Send a PING to the board (fire-and-forget, no wait for PONG).
 *
 * Routes through USB serial or BLE write depending on which transport is active.
 * Rust builds the correct byte sequence so there is no protocol code in JS.
 * Use mobilePingWait() when a round-trip confirmation is needed.
 */
export async function mobilePing(): Promise<void> {
  const bytes = await invoke<number[]>('mobile_build_ping');
  if (activeBleAddress) {
    await _bleWrite(new Uint8Array(bytes));
  } else {
    await _usbWrite(new Uint8Array(bytes));
  }
}

/**
 * Send a Dogecoin transaction.
 *
 * Handles single and multipart payloads transparently.
 * Routes through USB or BLE depending on active transport.
 */
export async function mobileSendTransaction(tx: TransactionRequest): Promise<void> {
  const packets = await invoke<number[][]>('mobile_build_tx_packets', { tx });
  for (let i = 0; i < packets.length; i++) {
    const chunk = new Uint8Array(packets[i]);
    if (activeBleAddress) {
      await _bleWrite(chunk);
    } else {
      await _usbWrite(chunk);
    }
    if (packets.length > 1 && i < packets.length - 1) await _sleep(120);
  }
}

/**
 * Send a LoRa settings update packet.
 *
 * Routes through USB or BLE depending on active transport.
 */
export async function mobileSendLoraSettings(settings: LoraSettings): Promise<void> {
  const bytes = await invoke<number[]>('mobile_build_lora_settings_packet', { settings });
  const data = new Uint8Array(bytes);
  if (activeBleAddress) {
    await _bleWrite(data);
  } else {
    await _usbWrite(data);
  }
}

// ─── Guards ───────────────────────────────────────────────────────────────────

function _requireActivePort(): void {
  if (!activePort) throw new Error('No active Android USB serial connection');
}

// ─── Public API ──────────────────────────────────────────────────────────────

export const bridge = {
  isAndroid,
  getPlatform,
  listDevices,
  bleScan,
  connect,
  disconnect,
  mobilePing,
  mobilePingWait,
  mobileSendTransaction,
  mobileSendLoraSettings,
};
