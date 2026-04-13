<script lang="ts">
  /**
   * Connection panel — the first tab the user sees.
   * Handles COM port selection (desktop) and Android USB/BT device picker.
   *
   * v0.3.0:  Uses list_ports_detailed for rich USB device info.
   * v0.3.10: Android USB serial support via tauri-plugin-serialplugin.
   *          Connection method tabs (USB / Bluetooth), status indicator badge,
   *          and platform-aware connect flow via connection-bridge.ts.
   */

  import { invoke } from '@tauri-apps/api/core';
  import { listen } from '@tauri-apps/api/event';
  import { open } from '@tauri-apps/plugin-shell';
  import {
    connection,
    setAvailablePorts,
    setConnecting,
    setDisconnected,
    setError,
    setMobileSearching,
    setMobileConnecting,
    setMobileConnected,
    setMobileFailed,
    setMobileIdle,
  } from '$lib/stores/connection.svelte';
  import SignalBars from './SignalBars.svelte';
  import DogeSpinner from './DogeSpinner.svelte';
  import { nodeAddressToString, type PortInfo, type MobileDeviceInfo } from '$lib/types';
  import { bridge } from '$lib/device/connection-bridge';

  /** Copy text to clipboard and briefly show confirmation. */
  let copiedField = $state<string | null>(null);
  async function copyToClipboard(text: string, field: string) {
    await navigator.clipboard.writeText(text);
    copiedField = field;
    setTimeout(() => { copiedField = null; }, 1500);
  }

  let selectedPort = $state('');
  let isRefreshing = $state(false);
  let ports = $state<PortInfo[]>([]);

  // Ping state
  let isPinging = $state(false);
  let pingResult = $state<{ success: boolean; message: string } | null>(null);

  // Firmware re-query state
  let isQueryingFw = $state(false);

  // localStorage key for remembering last COM port
  const LAST_PORT_KEY = 'radiodoge-last-port';

  /** Refresh the port list using the detailed IPC command. */
  async function refreshPorts() {
    isRefreshing = true;
    pingResult = null;
    try {
      ports = await invoke<PortInfo[]>('list_ports_detailed');
      // Keep the simple string array in the store (for compatibility)
      setAvailablePorts(ports.map(p => p.name));

      // Auto-select: prefer last-used port (if still present), then Heltec, then USB, then first
      if (ports.length > 0) {
        const lastUsed = localStorage.getItem(LAST_PORT_KEY);
        const lastUsedPresent = lastUsed ? ports.find(p => p.name === lastUsed) : null;
        const heltec = ports.find(p => p.isLikelyHeltec);
        const usb    = ports.find(p => p.isUsb);
        const auto   = lastUsedPresent ?? heltec ?? usb ?? ports[0];
        // Only auto-select if user hasn't already chosen something still present
        const currentStillPresent = ports.some(p => p.name === selectedPort);
        if (!currentStillPresent) {
          selectedPort = auto.name;
        }
      } else {
        selectedPort = '';
      }
    } catch (e) {
      console.error('Failed to list ports:', e);
    } finally {
      isRefreshing = false;
    }
  }

  async function connect() {
    if (!selectedPort) return;
    setConnecting(selectedPort);
    pingResult = null;
    try {
      await invoke('connect_port', { port: selectedPort });
      // Remember this port for next session
      localStorage.setItem(LAST_PORT_KEY, selectedPort);
      // Status update comes via the "connection-status" event in +page.svelte
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : String(e);
      setError(msg);
    }
  }

  /** Manually re-query firmware version from the device. */
  async function queryFirmware() {
    isQueryingFw = true;
    try {
      const fw = await invoke<string | null>('query_firmware_version');
      if (fw) {
        connection.firmwareVersion = fw;
      }
    } catch (e) {
      console.error('Firmware query failed:', e);
    } finally {
      isQueryingFw = false;
    }
  }

  async function disconnect() {
    pingResult = null;
    try {
      await invoke('disconnect_port');
    } catch (e) {
      console.error('Disconnect error:', e);
      setDisconnected();
    }
  }

  /** Send a PING to the Heltec and show the round-trip result. */
  async function pingDevice() {
    isPinging = true;
    pingResult = null;
    const start = performance.now();
    try {
      const ok = await invoke<boolean>('ping_device');
      const ms = Math.round(performance.now() - start);
      pingResult = ok
        ? { success: true, message: `Pong! Device responded in ~${ms} ms 🐕` }
        : { success: false, message: `No response within 500 ms. Is the firmware running?` };
    } catch (e) {
      pingResult = { success: false, message: String(e) };
    } finally {
      isPinging = false;
    }
  }

  // ── v0.3.10: Android mobile state ─────────────────────────────────────────

  /** True when running on Android (detected once on mount). */
  let isAndroidPlatform = $state(false);

  /** Connection method selected in the tab picker. */
  let mobileConnectTab = $state<'usb' | 'bluetooth'>('usb');

  /** Discovered Android devices (populated by mobileRefresh). */
  let mobileDevices = $state<MobileDeviceInfo[]>([]);

  /** Path of the device the user has selected in the mobile picker. */
  let selectedMobileDevice = $state('');

  /** True while the Android device scan is running. */
  let mobileRefreshing = $state(false);

  /** Firmware version received from the board during Android connect handshake. */
  let mobileFirmwareVersion = $state<string | null>(null);

  /** Scan for Android devices and update mobileDevices. */
  async function mobileRefresh() {
    mobileRefreshing = true;
    setMobileSearching(mobileConnectTab);
    try {
      let found: MobileDeviceInfo[];
      if (mobileConnectTab === 'bluetooth') {
        // BLE scan via tauri-plugin-blec — runs for 5 s then returns results.
        found = await bridge.bleScan(5000);
      } else {
        // USB-OTG device list via tauri-plugin-serialplugin
        const all = await bridge.listDevices();
        found = all.filter(d => d.type !== 'bluetooth');
      }
      mobileDevices = found;

      if (mobileDevices.length > 0) {
        const heltec = mobileDevices.find(d => d.isLikelyHeltec);
        if (!mobileDevices.some(d => d.path === selectedMobileDevice)) {
          selectedMobileDevice = (heltec ?? mobileDevices[0]).path;
        }
      } else {
        selectedMobileDevice = '';
      }
    } catch (e) {
      console.error('[ConnectionPanel] mobileRefresh error:', e);
    } finally {
      mobileRefreshing = false;
      setMobileIdle();
    }
  }

  /** Connect on Android via the connection bridge. */
  async function mobileConnect() {
    if (!selectedMobileDevice) return;
    setMobileConnecting(mobileConnectTab);
    mobileFirmwareVersion = null;
    pingResult = null;
    try {
      // Pass the connection type so the bridge opens USB-OTG or BLE GATT accordingly.
      await bridge.connect(selectedMobileDevice, mobileConnectTab === 'bluetooth' ? 'bluetooth' : 'usb');
      // "connection-status: connected" event arrives async via mobile_push_bytes
      // when the board responds to GET_SETTINGS.  The +page.svelte listener calls
      // setConnected() which drives the main UI — we mirror that into mobile state here.
      setMobileConnected();
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : String(e);
      setMobileFailed(msg);
      setError(msg);
    }
  }

  /** Disconnect on Android. */
  async function mobileDisconnect() {
    pingResult = null;
    try {
      await bridge.disconnect();
    } catch (e) {
      console.error('[ConnectionPanel] mobileDisconnect error:', e);
      setDisconnected();
    }
    setMobileIdle();
  }

  /** Send a PING via the Android bridge and wait for the board's PONG reply. */
  async function mobilePingDevice() {
    isPinging = true;
    pingResult = null;
    const start = performance.now();
    try {
      // mobilePingWait() registers a radio-packet listener BEFORE writing the
      // PING bytes, then waits up to 500 ms for CMD_PING (0x02) to come back.
      // This mirrors the desktop ping_device Rust command exactly.
      const ok = await bridge.mobilePingWait(500);
      const ms = Math.round(performance.now() - start);
      pingResult = ok
        ? { success: true,  message: `Pong! Device responded in ~${ms} ms 🐕` }
        : { success: false, message: `No response within 500 ms. Is the firmware running?` };
    } catch (e) {
      pingResult = { success: false, message: String(e) };
    } finally {
      isPinging = false;
    }
  }

  // ── v0.3.10: derived helper for the selected mobile device info ────────────
  const selectedMobileDeviceInfo = $derived(
    mobileDevices.find(d => d.path === selectedMobileDevice) ?? null
  );

  // ── Platform detection + event wiring on mount ─────────────────────────────

  // Refresh ports on mount
  $effect(() => {
    refreshPorts();
  });

  /** Helper: pick the right badge label for a port. */
  function portBadge(p: PortInfo): string {
    if (p.isLikelyHeltec) return '🟢 Heltec';
    if (p.isUsb) return '🔵 USB';
    return '⚪ COM';
  }

  /** Truncate long port descriptions so the <select> never overflows the card. */
  function portLabel(p: PortInfo): string {
    const badge = portBadge(p);
    const desc  = p.description.length > 42
      ? p.description.slice(0, 39) + '…'
      : p.description;
    return `${badge} ${desc}`;
  }

  /** The selected PortInfo object (or null). */
  const selectedPortInfo = $derived(ports.find(p => p.name === selectedPort) ?? null);

  /** True if selected port is not a recognized USB serial device. */
  const selectedIsNonUsb = $derived(selectedPort !== '' && selectedPortInfo !== null && !selectedPortInfo.isUsb);

  /** True when error looks like a soft timeout vs a hard access/driver failure. */
  const errorIsSoft = $derived(
    !!connection.error &&
    !connection.error.includes('Access') &&
    !connection.error.includes('driver') &&
    !connection.error.includes('Permission')
  );

  // ── Detect platform once; set up mobile event listeners ───────────────────
  $effect(() => {
    let cleanup: (() => void)[] = [];

    (async () => {
      isAndroidPlatform = await bridge.isAndroid();

      if (isAndroidPlatform) {
        // Firmware version arrives from Rust as a Tauri event; surface it in the UI
        const unlisten = await listen<string>('mobile-firmware-version', (ev) => {
          mobileFirmwareVersion = ev.payload
            .replace(/^RadioDoge\s+/i, '').trim() || ev.payload;
          // Also sync into the shared connection store so the NavBar badge works
          connection.firmwareVersion = mobileFirmwareVersion;
        });
        cleanup.push(unlisten);

        // When board sync arrives → mark mobile as connected
        const unlistenSync = await listen('board-sync', () => {
          setMobileConnected();
        });
        cleanup.push(unlistenSync);

        // Show an in-app toast for incoming DOGE transactions
        const handler = (e: Event) => {
          const body = (e as CustomEvent<string>).detail;
          console.info('[DOGE TX received on mobile]', body);
          // You could trigger a Svelte toast/notification here if desired
        };
        window.addEventListener('radiodoge:doge-tx', handler);
        cleanup.push(() => window.removeEventListener('radiodoge:doge-tx', handler));
      }
    })();

    return () => cleanup.forEach(fn => fn());
  });
</script>

<div style="max-width: 620px; margin: 0 auto; padding: clamp(12px, 3.5vw, 32px) clamp(12px, 4vw, 24px);">

  <!-- ── Hero ─────────────────────────────────────────────────────────────── -->
  <div style="text-align: center; margin-bottom: clamp(20px, 5vw, 40px);">
    <div style="
      margin-bottom: clamp(12px, 3vw, 20px);
      display: inline-block;
      animation: bounce-doge 3s ease-in-out infinite;
      filter: drop-shadow(0 0 24px rgba(245, 197, 24, 0.45));
    ">
      <img
        src="/doge-radio.png"
        alt="Doge holding a boombox radio with glowing Dogecoins"
        style="
          border-radius: 24px;
          display: block;
          width: clamp(80px, 28vw, 180px);
          height: auto;
        "
      />
    </div>
    <h1 style="
      font-size: clamp(1.4rem, 5vw, 2rem);
      font-weight: 800;
      background: linear-gradient(135deg, var(--doge-yellow), var(--doge-orange));
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
      margin: 0 0 6px 0;
    ">
      RadioDoge
    </h1>
    <p style="color: var(--doge-muted); margin: 0; font-size: clamp(0.8rem, 2.5vw, 0.95rem);">
      Wireless P2P Dogecoin — no internet required 🌐✖️
    </p>
    <p style="color: var(--doge-subtle); margin: 4px 0 0 0; font-size: clamp(0.68rem, 2vw, 0.78rem); font-style: italic;">
      Much radio. Very LoRa. Such decentralize. Wow. 🐕🌙
    </p>
  </div>

  <!-- ── Android USB/BT connection card (v0.3.10) ─────────────────────────── -->
  {#if isAndroidPlatform}
    <div class="card-doge {connection.isConnected ? 'card-connected' : ''}" style="margin-bottom: 16px;">
      <h2 style="margin: 0 0 16px 0; font-size: 1.1rem; font-weight: 600; color: var(--doge-text);">
        📱 Connect to Heltec Device
      </h2>

      <!-- ── Status indicator badge ──────────────────────────────────────── -->
      {#if connection.mobileStatusText}
        <div style="
          margin-bottom: 14px;
          padding: 8px 14px;
          border-radius: 8px;
          font-size: 0.83rem;
          font-weight: 600;
          display: flex;
          align-items: center;
          gap: 8px;
          {connection.mobileStatus === 'connected'
            ? 'background: rgba(0,255,136,0.08); border: 1px solid rgba(0,255,136,0.3); color: var(--doge-neon);'
            : connection.mobileStatus === 'failed'
            ? 'background: rgba(255,68,68,0.08); border: 1px solid rgba(255,68,68,0.3); color: var(--doge-red);'
            : 'background: rgba(245,197,24,0.07); border: 1px solid rgba(245,197,24,0.2); color: var(--doge-yellow);'}
        "
          role="status"
          aria-live="polite"
        >
          {#if connection.mobileStatus === 'searching' || connection.mobileStatus === 'connecting'}
            <span style="animation: spin-doge 1.2s linear infinite; display: inline-block;">📡</span>
          {:else if connection.mobileStatus === 'connected'}
            <span>✅</span>
          {:else if connection.mobileStatus === 'failed'}
            <span>❌</span>
          {/if}
          {connection.mobileStatusText}
        </div>
      {/if}

      <!-- ── Connection method tabs: USB / Bluetooth ─────────────────────── -->
      {#if !connection.isConnected}
        <div style="
          display: flex;
          gap: 0;
          border: 1px solid var(--doge-border);
          border-radius: 8px;
          overflow: hidden;
          margin-bottom: 14px;
        ">
          {#each [
            { id: 'usb' as const,       label: '🔌 USB-C',    subtitle: 'CP2102 / CH340' },
            { id: 'bluetooth' as const, label: '📶 Bluetooth', subtitle: 'BLE' },
          ] as tab}
            <button
              onclick={() => { mobileConnectTab = tab.id; selectedMobileDevice = ''; mobileDevices = []; setMobileIdle(); }}
              style="
                flex: 1;
                padding: 10px 8px;
                background: {mobileConnectTab === tab.id ? 'rgba(245,197,24,0.12)' : 'transparent'};
                border: none;
                border-right: {tab.id === 'usb' ? '1px solid var(--doge-border)' : 'none'};
                color: {mobileConnectTab === tab.id ? 'var(--doge-yellow)' : 'var(--doge-muted)'};
                cursor: pointer;
                transition: background 0.15s;
                font-size: 0.85rem;
                font-weight: {mobileConnectTab === tab.id ? 600 : 400};
                line-height: 1.4;
              "
            >
              <div>{tab.label}</div>
              <div style="font-size: 0.65rem; opacity: 0.7; margin-top: 2px;">{tab.subtitle}</div>
            </button>
          {/each}
        </div>

        {#if mobileConnectTab === 'bluetooth'}
          <div style="
            padding: 12px 14px;
            background: rgba(255,140,0,0.06);
            border: 1px solid rgba(255,140,0,0.2);
            border-radius: 8px;
            font-size: 0.8rem;
            color: var(--doge-muted);
            margin-bottom: 12px;
            line-height: 1.6;
          ">
            📶 <strong style="color: var(--doge-orange);">Bluetooth BLE</strong> — tap
            <strong>⟳ Scan</strong> to discover nearby Heltec boards. Make sure BLE serial is
            enabled in the firmware. USB-C is more reliable for development use.
          </div>
        {/if}

        <!-- ── Device list ─────────────────────────────────────────────── -->
        <div style="margin-bottom: 14px;">
          <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;">
            <div style="font-size: 0.8rem; color: var(--doge-muted); text-transform: uppercase; letter-spacing: 0.05em;">
              {mobileConnectTab === 'bluetooth' ? 'Bluetooth Devices' : 'USB Serial Devices'}
            </div>
            <button
              onclick={mobileRefresh}
              disabled={mobileRefreshing}
              class="btn-ghost"
              title="Scan for devices"
              style="padding: 6px 12px; font-size: 0.78rem;"
            >
              {#if mobileRefreshing}
                <span style="animation: spin-doge 1s linear infinite; display: inline-block;">⟳</span>
                {mobileConnectTab === 'bluetooth' ? 'Scanning BLE… (5 s)' : 'Scanning…'}
              {:else}
                ⟳ Scan
              {/if}
            </button>
          </div>

          {#if mobileDevices.length === 0 && !mobileRefreshing}
            <div style="
              padding: 14px;
              background: rgba(245,197,24,0.04);
              border: 1px dashed rgba(245,197,24,0.2);
              border-radius: 8px;
              font-size: 0.82rem;
              color: var(--doge-subtle);
              text-align: center;
              line-height: 1.7;
            ">
              {#if mobileConnectTab === 'usb'}
                No USB serial devices found.<br>
                Plug in the Heltec via USB-C OTG cable, then tap <strong>⟳ Scan</strong>.<br>
                <span style="font-size: 0.7rem; opacity: 0.75; margin-top: 4px; display: inline-block;">
                  Android will prompt for USB permission on first connect.
                </span>
              {:else}
                No Bluetooth devices found.<br>
                Make sure the Heltec is <strong>powered on</strong>, BLE is enabled
                in firmware, and the board is within ~10 m.<br>
                <span style="font-size: 0.7rem; opacity: 0.75; margin-top: 4px; display: inline-block;">
                  Board advertises as "RadioDoge-X.X.X" (Nordic UART Service).
                </span>
              {/if}
            </div>
          {:else if mobileDevices.length > 0}
            <div style="display: flex; flex-direction: column; gap: 6px;">
              {#each mobileDevices as device}
                <button
                  onclick={() => { selectedMobileDevice = device.path; }}
                  style="
                    display: flex; align-items: center; gap: 10px;
                    padding: 10px 12px;
                    background: {selectedMobileDevice === device.path
                      ? 'rgba(245,197,24,0.1)' : 'rgba(255,255,255,0.03)'};
                    border: 1px solid {selectedMobileDevice === device.path
                      ? 'rgba(245,197,24,0.4)' : 'var(--doge-border)'};
                    border-radius: 8px; cursor: pointer; text-align: left;
                    width: 100%; transition: all 0.15s;
                  "
                >
                  <span style="font-size: 1.1rem; flex-shrink: 0;">
                    {device.isLikelyHeltec ? '🟢' : device.type === 'bluetooth' ? '📶' : '🔵'}
                  </span>
                  <div style="min-width: 0; flex: 1;">
                    <div style="font-size: 0.82rem; font-family: var(--font-mono); color: var(--doge-text); white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
                      {device.path}
                    </div>
                    <div style="font-size: 0.7rem; color: var(--doge-muted); margin-top: 2px;">
                      {device.isLikelyHeltec ? '✅ Heltec-compatible USB adapter' : device.description}
                    </div>
                  </div>
                  {#if selectedMobileDevice === device.path}
                    <span style="color: var(--doge-yellow); flex-shrink: 0;">✓</span>
                  {/if}
                </button>
              {/each}
            </div>
          {/if}
        </div>

        <!-- ── Connect button ────────────────────────────────────────────── -->
        <button
          onclick={mobileConnect}
          disabled={!selectedMobileDevice || connection.mobileStatus === 'connecting'}
          class="btn-doge"
          style="width: 100%; padding: 14px; font-size: 1rem;"
        >
          {#if connection.mobileStatus === 'connecting'}
            <DogeSpinner size="sm" message="" />
            <span style="margin-left: 8px;">Connecting… such patience 🐕</span>
          {:else if mobileConnectTab === 'bluetooth'}
            📶 Connect via Bluetooth
          {:else}
            🔌 Connect via USB-C
          {/if}
        </button>

      {:else}
        <!-- ── Connected view (Android) ────────────────────────────────── -->
        <div style="margin-bottom: 4px;">
          <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 14px;">
            <div style="display: flex; align-items: center; gap: 10px;">
              <h3 style="margin: 0; font-size: 1rem; color: var(--doge-neon);">✅ Connected!</h3>
              {#if mobileFirmwareVersion ?? connection.firmwareVersion}
                <span style="
                  background: rgba(0,255,136,0.1); border: 1px solid rgba(0,255,136,0.3);
                  border-radius: 12px; padding: 2px 10px;
                  font-size: 0.72rem; font-family: var(--font-mono); color: var(--doge-neon);
                ">
                  {mobileFirmwareVersion ?? connection.firmwareVersion}
                </span>
              {/if}
            </div>
            <SignalBars rssi={connection.stats?.rssi ?? -120} connected={true} size="md" />
          </div>

          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 14px;">
            <div>
              <div class="stat-label">Device</div>
              <div style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 600; font-size: 0.78rem; word-break: break-all;">
                {selectedMobileDevice || connection.portName}
              </div>
            </div>
            {#if connection.nodeAddress}
              <div>
                <div class="stat-label">Node Address</div>
                <div style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 600;">
                  {nodeAddressToString(connection.nodeAddress)}
                </div>
              </div>
            {/if}
            <div>
              <div class="stat-label">Transport</div>
              <div style="color: var(--doge-text); font-size: 0.82rem;">
                {connection.mobileConnectionType === 'bluetooth' ? '📶 Bluetooth' : '🔌 USB-C (OTG)'}
              </div>
            </div>
          </div>

          <button
            onclick={mobilePingDevice}
            disabled={isPinging}
            class="btn-ghost"
            style="width: 100%; padding: 10px; font-size: 0.9rem; margin-bottom: {pingResult ? '10px' : '8px'};"
          >
            {#if isPinging}
              <span style="animation: spin-doge 1s linear infinite; display: inline-block;">📡</span>
              Pinging…
            {:else}
              📡 Ping Device
            {/if}
          </button>
          {#if pingResult}
            <div style="
              padding: 10px 14px; border-radius: 8px; font-size: 0.82rem; margin-bottom: 10px;
              {pingResult.success
                ? 'background: rgba(0,255,136,0.07); border: 1px solid rgba(0,255,136,0.25); color: var(--doge-neon);'
                : 'background: rgba(255,68,68,0.07); border: 1px solid rgba(255,68,68,0.25); color: var(--doge-red);'}
            ">
              {pingResult.success ? '✅' : '❌'} {pingResult.message}
            </div>
          {/if}

          <button
            onclick={mobileDisconnect}
            style="
              width: 100%;
              background: rgba(255,68,68,0.1); color: var(--doge-red);
              border: 1px solid rgba(255,68,68,0.3); border-radius: 12px;
              padding: 12px; font-size: 0.95rem; font-weight: 700; cursor: pointer;
            "
          >
            🔌 Disconnect
          </button>
        </div>
      {/if}

      <!-- Error (Android) -->
      {#if connection.error && !connection.isConnected}
        <div style="
          margin-top: 10px; padding: 10px 14px;
          background: rgba(255,68,68,0.08); border: 1px solid rgba(255,68,68,0.3);
          border-radius: 8px; color: var(--doge-red); font-size: 0.8rem;
          display: flex; align-items: flex-start; gap: 8px;
        ">
          <span style="flex-shrink:0;">❌</span>
          <span style="flex:1; word-break: break-word; white-space: pre-line;">{connection.error}</span>
          <button
            onclick={() => { connection.error = null; }}
            style="background:none;border:none;cursor:pointer;color:inherit;opacity:0.5;font-size:0.85rem;padding:0;flex-shrink:0;"
          >✕</button>
        </div>
      {/if}
    </div>
  {/if}

  <!-- ── Connection card (Desktop) ──────────────────────────────────────────── -->
  {#if !isAndroidPlatform}
  <div class="card-doge {connection.isConnected ? 'card-connected' : ''}">
    <h2 style="margin: 0 0 20px 0; font-size: 1.1rem; font-weight: 600; color: var(--doge-text);">
      🔌 Connect to Heltec Device
    </h2>

    <!-- Port selection -->
    <div style="margin-bottom: 16px;">
      <label for="serial-port-select" style="display: block; font-size: 0.8rem; color: var(--doge-muted); margin-bottom: 8px; text-transform: uppercase; letter-spacing: 0.05em;">
        Serial Port
      </label>
      <div style="display: flex; gap: 8px; min-width: 0;">
        <select
          id="serial-port-select"
          bind:value={selectedPort}
          disabled={connection.isConnected || connection.isConnecting}
          title={selectedPortInfo ? `${selectedPortInfo.description} (${selectedPortInfo.name})` : 'Select a serial port'}
          style="
            flex: 1;
            min-width: 0;
            background: var(--doge-dark);
            border: 1px solid var(--doge-border);
            border-radius: 8px;
            color: {selectedPort ? 'var(--doge-text)' : 'var(--doge-subtle)'};
            padding: 10px 12px;
            font-size: 0.85rem;
            outline: none;
            font-family: var(--font-mono);
            cursor: pointer;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            max-width: 100%;
          "
        >
          {#if ports.length === 0}
            <option value="">No ports found — plug in Heltec!</option>
          {:else}
            <option value="" disabled>Select a port...</option>
            {#each ports as p}
              <option value={p.name} title={p.description}>
                {portLabel(p)}
              </option>
            {/each}
          {/if}
        </select>

        <!-- Refresh button -->
        <button
          onclick={refreshPorts}
          disabled={isRefreshing}
          class="btn-ghost"
          title="Refresh port list"
          style="padding: 10px 14px; flex-shrink: 0;"
        >
          {#if isRefreshing}
            <span style="animation: spin-doge 1s linear infinite; display: inline-block;">⟳</span>
          {:else}
            ⟳
          {/if}
        </button>
      </div>

      <!-- Selected port detail badge -->
      {#if selectedPortInfo}
        <div style="
          margin-top: 8px;
          display: flex;
          align-items: center;
          gap: 8px;
          font-size: 0.76rem;
          color: var(--doge-muted);
        ">
          {#if selectedPortInfo.isLikelyHeltec}
            <span style="color: var(--doge-neon);">✅ Heltec-compatible USB adapter detected</span>
          {:else if selectedPortInfo.isUsb}
            <span style="color: var(--doge-yellow);">🔵 USB serial adapter</span>
          {:else}
            <span style="color: var(--doge-subtle);">⚪ Native COM port — may not be your Heltec</span>
          {/if}
          {#if selectedPortInfo.product}
            <span class="mono">({selectedPortInfo.product})</span>
          {/if}
        </div>
      {/if}

      <!-- No ports found: helpful message with driver links -->
      {#if ports.length === 0 && !isRefreshing}
        <div style="
          margin-top: 10px;
          padding: 12px 14px;
          background: rgba(245, 197, 24, 0.05);
          border: 1px solid rgba(245, 197, 24, 0.15);
          border-radius: 8px;
          font-size: 0.8rem;
          color: var(--doge-muted);
          line-height: 1.7;
        ">
          💡 <strong>No serial ports detected.</strong> Try these steps:
          <ol style="margin: 6px 0 0 0; padding-left: 20px;">
            <li>Plug the Heltec into a USB port and click ⟳</li>
            <li>If still empty, install the <strong>CP210x driver</strong> (most Heltec boards):
              <br>
              <button
                onclick={() => open('https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers')}
                class="btn-ghost"
                style="font-size: 0.72rem; padding: 2px 8px; margin-top: 4px; text-decoration: underline; color: var(--doge-yellow);"
              >
                🔗 silabs.com — CP210x USB Driver
              </button>
            </li>
            <li>Or the <strong>CH340 driver</strong> for some boards:
              <br>
              <button
                onclick={() => open('https://www.wch-ic.com/products/CH340.html')}
                class="btn-ghost"
                style="font-size: 0.72rem; padding: 2px 8px; margin-top: 4px; text-decoration: underline; color: var(--doge-yellow);"
              >
                🔗 wch-ic.com — CH340 USB Driver
              </button>
            </li>
            <li>Check Device Manager for a ⚠️ yellow warning on the port</li>
          </ol>
        </div>
      {/if}

      <!-- Non-USB port selected: warn user -->
      {#if selectedIsNonUsb}
        <p style="color: var(--doge-orange); font-size: 0.76rem; margin: 6px 0 0 0;">
          ⚠️ This doesn't look like a USB serial port. Your Heltec should appear as a USB device (CP210x / CH340).
        </p>
      {/if}
    </div>

    <!-- Connect / Disconnect / Connecting / Reconnecting buttons -->
    {#if connection.isConnected}
      <button
        onclick={disconnect}
        style="
          width: 100%;
          background: rgba(255, 68, 68, 0.1);
          color: var(--doge-red);
          border: 1px solid rgba(255, 68, 68, 0.3);
          border-radius: 12px;
          padding: 14px;
          font-size: 1rem;
          font-weight: 700;
          cursor: pointer;
          transition: all 0.2s;
        "
      >
        🔌 Disconnect
      </button>
    {:else if connection.isReconnecting}
      <button disabled class="btn-doge" style="width: 100%; padding: 14px; font-size: 1rem; opacity: 0.85;">
        <DogeSpinner size="sm" message="" />
        <span style="margin-left: 8px;">🔄 Reconnecting... such retry 🐕</span>
      </button>
    {:else if connection.isConnecting}
      <button disabled class="btn-doge" style="width: 100%; padding: 14px; font-size: 1rem;">
        <DogeSpinner size="sm" message="" />
        <span style="margin-left: 8px;">Connecting... such patience 🐕</span>
      </button>
    {:else}
      <button
        onclick={connect}
        disabled={!selectedPort}
        class="btn-doge"
        style="width: 100%; padding: 14px; font-size: 1.05rem;"
      >
        🔌 Connect to Heltec
      </button>
    {/if}

    <!-- Error message — compact yellow for soft errors, red for hard failures -->
    {#if connection.error}
      <div style="
        margin-top: 10px;
        padding: {errorIsSoft ? '7px 12px' : '10px 14px'};
        background: {errorIsSoft ? 'rgba(245,197,24,0.07)' : 'rgba(255,68,68,0.08)'};
        border: 1px solid {errorIsSoft ? 'rgba(245,197,24,0.25)' : 'rgba(255,68,68,0.3)'};
        border-radius: 8px;
        color: {errorIsSoft ? 'var(--doge-yellow)' : 'var(--doge-red)'};
        font-size: {errorIsSoft ? '0.76rem' : '0.8rem'};
        line-height: 1.55;
        white-space: pre-line;
        display: flex;
        align-items: flex-start;
        gap: 8px;
      ">
        <span style="flex-shrink:0;">{errorIsSoft ? '⚠️' : '❌'}</span>
        <span style="flex:1; word-break: break-word;">{connection.error}</span>
        <button
          onclick={() => { connection.error = null; }}
          title="Dismiss"
          style="background:none;border:none;cursor:pointer;color:inherit;opacity:0.5;font-size:0.85rem;padding:0;flex-shrink:0;line-height:1;"
        >✕</button>
      </div>
    {/if}
  </div>
  {/if}<!-- end desktop-only -->

  <!-- ── Connected status card (desktop only) ──────────────────────────────── -->
  {#if !isAndroidPlatform && connection.isConnected}
    <div class="card-doge card-connected" style="margin-top: 16px; animation: slide-up 0.3s ease;">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px;">
        <div style="display: flex; align-items: center; gap: 10px;">
          <h3 style="margin: 0; font-size: 1rem; color: var(--doge-neon);">✅ Connected!</h3>
          {#if connection.firmwareVersion}
            <span
              title="Firmware version reported by the Heltec device"
              style="
                background: rgba(0,255,136,0.1);
                border: 1px solid rgba(0,255,136,0.3);
                border-radius: 12px;
                padding: 2px 10px;
                font-size: 0.72rem;
                font-family: var(--font-mono);
                color: var(--doge-neon);
              "
            >
              {connection.firmwareVersion}
            </span>
          {:else}
            <span
              title="Firmware version not yet received — click ⟳ Query FW to retry"
              style="
                background: rgba(245,197,24,0.07);
                border: 1px solid rgba(245,197,24,0.2);
                border-radius: 12px;
                padding: 2px 10px;
                font-size: 0.72rem;
                font-family: var(--font-mono);
                color: var(--doge-subtle);
              "
            >
              FW …
            </span>
            <button
              onclick={queryFirmware}
              disabled={isQueryingFw}
              class="btn-ghost"
              title="Re-query firmware version from device"
              style="padding: 2px 8px; font-size: 0.68rem;"
            >
              {#if isQueryingFw}
                <span style="animation: spin-doge 1s linear infinite; display: inline-block;">⟳</span>
              {:else}
                ⟳ Query FW
              {/if}
            </button>
          {/if}
        </div>
        <SignalBars rssi={connection.stats?.rssi ?? -120} connected={true} size="md" />
      </div>

      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px;">
        <div>
          <div class="stat-label">Port</div>
          <div style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 600;">
            {connection.portName}
          </div>
        </div>
        {#if connection.nodeAddress}
          <div>
            <div class="stat-label">Node Address</div>
            <div style="display: flex; align-items: center; gap: 6px;">
              <span style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 600;">
                {nodeAddressToString(connection.nodeAddress)}
              </span>
              <button
                onclick={() => copyToClipboard(nodeAddressToString(connection.nodeAddress!), 'nodeAddr')}
                class="btn-ghost"
                title="Copy node address"
                style="padding: 2px 6px; font-size: 0.7rem; flex-shrink: 0;"
              >
                {copiedField === 'nodeAddr' ? '✅' : '📋'}
              </button>
            </div>
          </div>
        {/if}
        {#if connection.stats}
          <div>
            <div class="stat-label">RSSI</div>
            <div style="font-family: var(--font-mono); color: var(--doge-text);">
              {connection.stats.rssi} dBm
            </div>
          </div>
          <div>
            <div class="stat-label">SNR</div>
            <div style="font-family: var(--font-mono); color: var(--doge-text);">
              {connection.stats.snr.toFixed(1)} dB
            </div>
          </div>
        {/if}
      </div>

      <!-- Ping Device button -->
      <button
        onclick={pingDevice}
        disabled={isPinging}
        class="btn-ghost"
        style="width: 100%; padding: 10px; font-size: 0.9rem; margin-bottom: {pingResult ? '10px' : '0'};"
      >
        {#if isPinging}
          <span style="animation: spin-doge 1s linear infinite; display: inline-block;">📡</span>
          Pinging...
        {:else}
          📡 Ping Device
        {/if}
      </button>

      <!-- Ping result -->
      {#if pingResult}
        <div style="
          padding: 10px 14px;
          border-radius: 8px;
          font-size: 0.82rem;
          {pingResult.success
            ? 'background: rgba(0,255,136,0.07); border: 1px solid rgba(0,255,136,0.25); color: var(--doge-neon);'
            : 'background: rgba(255,68,68,0.07); border: 1px solid rgba(255,68,68,0.25); color: var(--doge-red);'}
        ">
          {pingResult.success ? '✅' : '❌'} {pingResult.message}
        </div>
      {/if}

      <p style="
        margin: 14px 0 0 0;
        padding: 10px 14px;
        background: rgba(0, 255, 136, 0.05);
        border-radius: 8px;
        font-size: 0.82rem;
        color: var(--doge-muted);
        font-style: italic;
      ">
        🐕 Such connection. Very radio. Wow. Head to Dashboard →
      </p>
    </div>
  {/if}

  <!-- ── How it works ─────────────────────────────────────────────────────── -->
  {#if !connection.isConnected}
    <div style="margin-top: 32px;">
      <h3 style="color: var(--doge-muted); font-size: 0.85rem; text-transform: uppercase; letter-spacing: 0.08em; margin-bottom: 16px;">
        How it works
      </h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        {#each (isAndroidPlatform ? [
          { icon: '1️⃣', text: 'Flash the Heltec firmware (see docs/) and power the board' },
          { icon: '2️⃣', text: 'Plug the Heltec into your phone via USB-C OTG cable, then tap ⟳ Scan' },
          { icon: '3️⃣', text: 'Select the device, tap Connect — Android will ask for USB permission' },
          { icon: '4️⃣', text: 'Generate a Dogecoin wallet in the Wallet tab and send DOGE over LoRa! 🚀' },
        ] : [
          { icon: '1️⃣', text: 'Flash Heltec firmware (see docs/), connect via USB-C' },
          { icon: '2️⃣', text: 'Select the COM port above (🟢 green = Heltec detected!) and click Connect' },
          { icon: '3️⃣', text: 'Generate a Dogecoin wallet in the Wallet tab' },
          { icon: '4️⃣', text: 'Send DOGE over LoRa radio — no internet needed! 🚀' },
        ]) as step}
          <div style="display: flex; align-items: flex-start; gap: 12px; color: var(--doge-muted); font-size: 0.88rem;">
            <span style="font-size: 1.1rem; flex-shrink: 0;">{step.icon}</span>
            <span>{step.text}</span>
          </div>
        {/each}
      </div>
    </div>
  {/if}
</div>
