<script lang="ts">
  /**
   * Settings tab — configure LoRa parameters and node address.
   *
   * v0.3.1 improvements:
   *   - Tooltips on every slider, button, and input field
   *   - Improved "Save to Device" feedback: green verified ✓ / red error
   *   - Keyboard-accessible SF/BW/CR buttons
   *   - Node address copy-to-clipboard
   *   - Fun Doge tone throughout
   */

  import { invoke } from '@tauri-apps/api/core';
  import { listen } from '@tauri-apps/api/event';
  import { radio, updateSettings } from '$lib/stores/radio.svelte';
  import { connection, setBoardMac } from '$lib/stores/connection.svelte';
  import { bridge } from '$lib/device/connection-bridge';
  import DogeSpinner from './DogeSpinner.svelte';

  // ── Platform detection ────────────────────────────────────────────────────
  // Use null as "not yet resolved" to prevent fetchMac() from taking the wrong
  // code path (desktop query_mac) on Android before the Promise settles.
  let isAndroidPlatform = $state<boolean | null>(null);
  $effect(() => {
    bridge.isAndroid().then(v => { isAndroidPlatform = v; });
  });

  let isSaving = $state(false);
  let saveSuccess = $state(false);
  let saveVerified = $state<boolean | null>(null);
  let saveError = $state<string | null>(null);

  // ── v0.3.6 — Connection type (USB / BLE) ─────────────────────────────────
  let connType = $state<'usb' | 'ble'>(connection.connectionType);
  async function applyConnType(type: 'usb' | 'ble') {
    connType = type;
    connection.connectionType = type;
    await invoke('set_connection_type', { connType: type }).catch(() => {});
  }

  // ── v0.3.8 — Light/dark theme ─────────────────────────────────────────────
  let isDarkTheme = $state(true); // dark is default
  function toggleTheme() {
    isDarkTheme = !isDarkTheme;
    document.documentElement.setAttribute('data-theme', isDarkTheme ? 'dark' : 'light');
    try { localStorage.setItem('rd-theme', isDarkTheme ? 'dark' : 'light'); } catch {}
  }
  // Restore saved theme on mount
  $effect(() => {
    try {
      const saved = localStorage.getItem('rd-theme');
      if (saved === 'light') { isDarkTheme = false; document.documentElement.setAttribute('data-theme', 'light'); }
    } catch {}
  });

  // ── v0.3.8 — Board MAC address ───────────────────────────────────────────
  let isQueryingMac = $state(false);
  async function fetchMac() {
    if (!connection.isConnected) return;
    // isAndroidPlatform is null until the platform Promise resolves — wait for it.
    if (isAndroidPlatform === null) return;
    isQueryingMac = true;
    try {
      if (isAndroidPlatform) {
        // Android: write GET_MAC bytes via bridge; response arrives as 'mobile-board-mac' event
        const bytes = await invoke<number[]>('mobile_build_get_mac');
        await bridge.mobileSendBytes(new Uint8Array(bytes));
        // isQueryingMac cleared by the mobile-board-mac listener below (or timeout)
        setTimeout(() => { isQueryingMac = false; }, 1200);
      } else {
        const mac = await invoke<string | null>('query_mac');
        setBoardMac(mac ?? null);
        isQueryingMac = false;
      }
    } catch { isQueryingMac = false; }
  }
  // Listen for MAC arriving from board on Android (mobile_push_bytes → "mobile-board-mac" event)
  $effect(() => {
    if (!isAndroidPlatform) return;
    let unlisten: (() => void) | undefined;
    listen<string>('mobile-board-mac', (ev) => {
      setBoardMac(ev.payload);
      isQueryingMac = false;
    }).then(fn => { unlisten = fn; });
    return () => { unlisten?.(); };
  });
  // Auto-fetch MAC only once platform is known (isAndroidPlatform !== null) — prevents
  // the desktop query_mac path from running on Android before the Promise settles.
  $effect(() => {
    if (connection.isConnected && !connection.boardMac && isAndroidPlatform !== null) {
      fetchMac();
    }
  });

  // ── v0.3.7 — WiFi toggle ─────────────────────────────────────────────────
  let isTogglingWifi = $state(false);
  let wifiError = $state<string | null>(null);
  async function toggleWifi() {
    if (!connection.isConnected) return;
    isTogglingWifi = true;
    wifiError = null;
    try {
      const nextState = !connection.wifiEnabled;
      if (isAndroidPlatform) {
        // Android: write WIFI_TOGGLE bytes via bridge; board-sync event confirms
        const bytes = await invoke<number[]>('mobile_build_wifi_toggle', { enable: nextState });
        await bridge.mobileSendBytes(new Uint8Array(bytes));
        connection.wifiEnabled = nextState; // optimistic update
      } else {
        const confirmed = await invoke<boolean>('set_wifi_enabled', { enable: nextState });
        connection.wifiEnabled = confirmed; // use confirmed value directly — fixes board-sync race
      }
    } catch (e: unknown) {
      wifiError = e instanceof Error ? e.message : String(e);
    } finally {
      isTogglingWifi = false;
    }
  }

  // ── v0.3.6 — Gateway mode (board-persistent) ─────────────────────────────
  let isSettingGateway = $state(false);
  let gatewayError = $state<string | null>(null);
  let isStartingDaemon = $state(false);
  let daemonError = $state<string | null>(null);

  // ── v0.3.16 — BLE Advertising toggle ────────────────────────────────────
  // Tracks local UI state only — board state persists independently in NVS.
  // Assumes BLE advertising is ON by default (board always starts advertising).
  let bleAdvertisingEnabled = $state(true);
  let isTogglingBle = $state(false);
  let bleError = $state<string | null>(null);
  // Reset BLE toggle to default when the board disconnects so reconnect shows the
  // correct initial state (board always restarts with BLE advertising enabled).
  $effect(() => {
    if (!connection.isConnected) {
      bleAdvertisingEnabled = true;
      bleError = null;
    }
  });
  async function toggleBleAdvertising() {
    if (!connection.isConnected) return;
    isTogglingBle = true;
    bleError = null;
    try {
      const nextState = !bleAdvertisingEnabled;
      if (isAndroidPlatform) {
        const bytes = await invoke<number[]>('mobile_build_ble_toggle', { enable: nextState });
        await bridge.mobileSendBytes(new Uint8Array(bytes));
      } else {
        await invoke('set_ble_enabled', { enable: nextState });
      }
      bleAdvertisingEnabled = nextState;
    } catch (e: unknown) {
      bleError = e instanceof Error ? e.message : String(e);
    } finally {
      isTogglingBle = false;
    }
  }

  async function toggleGatewayMode() {
    if (!connection.isConnected) return;
    isSettingGateway = true;
    gatewayError = null;
    try {
      const nextState = !connection.gatewayMode;
      if (isAndroidPlatform) {
        // Android: write SET_GATEWAY bytes via bridge; board-sync event confirms
        const bytes = await invoke<number[]>('mobile_build_set_gateway', { enable: nextState });
        await bridge.mobileSendBytes(new Uint8Array(bytes));
        connection.gatewayMode = nextState; // optimistic update; board-sync will confirm
      } else {
        // Desktop: Rust command sends + confirms.  Use returned value to update immediately
        // (fixes the "double-press" bug where board-sync event races with UI re-render).
        const confirmed = await invoke<boolean>('set_gateway_mode', { enable: nextState });
        connection.gatewayMode = confirmed;
      }
    } catch (e: unknown) {
      gatewayError = e instanceof Error ? e.message : String(e);
    } finally {
      isSettingGateway = false;
    }
  }

  async function startGatewayDaemon() {
    const port = connection.portName;
    if (!port) return;
    isStartingDaemon = true;
    daemonError = null;
    try {
      await invoke('start_gateway', { port });
    } catch (e: unknown) {
      daemonError = e instanceof Error ? e.message : String(e);
    } finally {
      isStartingDaemon = false;
    }
  }

  async function stopGatewayDaemon() {
    try { await invoke('stop_gateway'); } catch (_) {}
  }

  // ── v0.3.6 — Manual board sync ───────────────────────────────────────────
  let isSyncing = $state(false);
  let syncResult = $state<string | null>(null);
  async function syncFromBoard() {
    if (!connection.isConnected) return;
    isSyncing = true;
    syncResult = null;
    try {
      const bs = await invoke<{ nodeAddress: { region: number; community: number; node: number }; gatewayMode: boolean } | null>('get_board_settings');
      if (bs) {
        syncResult = `✅ Board: ${bs.nodeAddress.region}.${bs.nodeAddress.community}.${bs.nodeAddress.node} | GW: ${bs.gatewayMode ? 'ON' : 'OFF'}`;
      } else {
        syncResult = '⚠️ No response from board — try again.';
      }
    } catch (e: unknown) {
      syncResult = '❌ ' + (e instanceof Error ? e.message : String(e));
    } finally {
      isSyncing = false;
      setTimeout(() => { syncResult = null; }, 5000);
    }
  }

  // Local copy for editing
  let settings = $state({ ...radio.settings });
  let nodeAddr = $state({ ...radio.settings.nodeAddress });

  const SPREADING_FACTORS = [7, 8, 9, 10, 11, 12];
  const BANDWIDTHS = [125, 250, 500];
  const CODING_RATES = ['4/5', '4/6', '4/7', '4/8'];

  const SF_TOOLTIPS: Record<number, string> = {
    7:  'SF7 — Fastest, shortest range. Good for testing nearby. Much speed!',
    8:  'SF8 — Slightly longer range than SF7, modest speed drop.',
    9:  'SF9 — Good balance of range and speed.',
    10: 'SF10 — Long range, noticeable slower data rate.',
    11: 'SF11 — Very long range. Use for distant nodes.',
    12: 'SF12 — Maximum range! Extremely slow. Such distance. Very patience.',
  };

  const BW_TOOLTIPS: Record<number, string> = {
    125: '125 kHz — Standard LoRa bandwidth. Best range, used worldwide.',
    250: '250 kHz — Wider channel, 2× data rate, slightly shorter range.',
    500: '500 kHz — Fastest but shortest range. Use only for nearby nodes.',
  };

  const CR_TOOLTIPS: Record<string, string> = {
    '4/5': '4/5 — Minimal redundancy. Fastest. Good for clean RF environments.',
    '4/6': '4/6 — Light error correction. Good default with some noise tolerance.',
    '4/7': '4/7 — Moderate error correction. Better in noisy RF.',
    '4/8': '4/8 — Maximum error correction. Slowest but most robust.',
  };

  async function saveSettings() {
    isSaving = true;
    saveSuccess = false;
    saveVerified = null;
    saveError = null;

    const fullSettings = { ...settings, nodeAddress: nodeAddr };

    try {
      const verified = await invoke<boolean>('update_lora_settings', { settings: fullSettings });
      updateSettings(fullSettings);
      saveSuccess = true;
      saveVerified = verified;
      setTimeout(() => { saveSuccess = false; saveVerified = null; }, 5000);
    } catch (e: unknown) {
      saveError = e instanceof Error ? e.message : String(e);
    } finally {
      isSaving = false;
    }
  }

  function resetDefaults() {
    settings = {
      frequencyMhz: 915.0,
      powerDbm: 5,
      spreadingFactor: 7,
      bandwidthKhz: 125.0,
      codingRate: '4/5',
      nodeAddress: { region: 10, community: 0, node: 1 },
    };
    nodeAddr = { ...settings.nodeAddress };
  }

  // Copy node address to clipboard
  let copiedNodeAddr = $state(false);
  async function copyNodeAddr() {
    await navigator.clipboard.writeText(`${nodeAddr.region}.${nodeAddr.community}.${nodeAddr.node}`);
    copiedNodeAddr = true;
    setTimeout(() => { copiedNodeAddr = false; }, 1500);
  }
</script>

<div style="padding: 24px; max-width: 700px; margin: 0 auto;">

  <!-- ── Header ──────────────────────────────────────────────────────────── -->
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 24px; flex-wrap: wrap; gap: 12px;">
    <div>
      <h2 style="margin: 0 0 4px 0; font-size: 1.3rem; font-weight: 700;">⚙️ LoRa Settings</h2>
      <p style="margin: 0; color: var(--doge-muted); font-size: 0.85rem;">
        Configure radio parameters — sent to Heltec device on save
      </p>
    </div>
    <button
      onclick={resetDefaults}
      class="btn-ghost"
      title="Reset all settings to factory defaults (915 MHz, SF7, 125 kHz, 5 dBm). Much default. Very reset."
      aria-label="Reset all settings to defaults"
      style="font-size: 0.8rem;"
    >
      ↺ Defaults
    </button>
  </div>

  <div style="display: flex; flex-direction: column; gap: 16px;">

    <!-- ── Frequency ──────────────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="Frequency setting">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 12px;">
        <label
          for="settings-frequency"
          style="font-weight: 600; font-size: 0.9rem; cursor: pointer;"
          title="LoRa carrier frequency in MHz. Must match all nodes in your mesh network. 915 = US/AU, 433 = EU/Asia."
        >
          📻 Frequency
        </label>
        <span style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 700;">
          {settings.frequencyMhz} MHz
        </span>
      </div>
      <input
        id="settings-frequency"
        type="number"
        bind:value={settings.frequencyMhz}
        min="433"
        max="915"
        step="0.1"
        class="input-doge"
        title="Enter frequency in MHz. Valid range: 433–915 MHz. Must match your Heltec firmware setting and regional regulations."
        aria-describedby="freq-hint"
      />
      <p id="freq-hint" style="margin: 6px 0 0 0; font-size: 0.75rem; color: var(--doge-muted);">
        433 MHz (EU/Asia) · 915 MHz (US/AU). Must match all nodes in your mesh.
      </p>
    </div>

    <!-- ── TX Power ───────────────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="TX power setting">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 12px;">
        <label
          for="settings-tx-power"
          style="font-weight: 600; font-size: 0.9rem; cursor: pointer;"
          title="Transmit power in dBm. Higher = more range but more current draw. 20 dBm = 100 mW = maximum."
        >
          ⚡ TX Power
        </label>
        <span style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 700;">
          {settings.powerDbm} dBm
        </span>
      </div>
      <input
        id="settings-tx-power"
        type="range"
        bind:value={settings.powerDbm}
        min="5"
        max="20"
        step="1"
        aria-valuetext="{settings.powerDbm} dBm"
        title="Slide to adjust transmit power. Higher = more range but more battery drain. 20 dBm is maximum per Heltec specs."
        style="width: 100%; accent-color: var(--doge-yellow); cursor: pointer;"
      />
      <div style="display: flex; justify-content: space-between; font-size: 0.72rem; color: var(--doge-subtle); margin-top: 4px;">
        <span>5 dBm (low power, very nearby)</span>
        <span>20 dBm (max range, much power)</span>
      </div>
    </div>

    <!-- ── Spreading Factor ───────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="Spreading factor setting">
      <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 12px 0;"
         title="Spreading Factor controls range vs. data rate. SF7 = fastest, SF12 = longest range but very slow."
      >
        📶 Spreading Factor
      </p>
      <div style="display: grid; grid-template-columns: repeat(6, 1fr); gap: 8px;" role="group" aria-label="Spreading factor selection">
        {#each SPREADING_FACTORS as sf}
          <button
            onclick={() => settings.spreadingFactor = sf}
            aria-pressed={settings.spreadingFactor === sf}
            title={SF_TOOLTIPS[sf]}
            style="
              padding: 10px 0;
              border: 1px solid {settings.spreadingFactor === sf ? 'var(--doge-yellow)' : 'var(--doge-border)'};
              background: {settings.spreadingFactor === sf ? 'rgba(245, 197, 24, 0.15)' : 'transparent'};
              color: {settings.spreadingFactor === sf ? 'var(--doge-yellow)' : 'var(--doge-muted)'};
              border-radius: 8px;
              cursor: pointer;
              font-weight: {settings.spreadingFactor === sf ? 700 : 400};
              font-family: var(--font-mono);
              transition: all 0.15s;
            "
          >
            SF{sf}
          </button>
        {/each}
      </div>
      <p style="margin: 8px 0 0 0; font-size: 0.75rem; color: var(--doge-muted);">
        Higher SF = longer range, lower data rate. SF7 = fastest (default). Such tradeoff. Wow.
      </p>
    </div>

    <!-- ── Bandwidth ─────────────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="Bandwidth setting">
      <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 12px 0;"
         title="LoRa channel bandwidth. Narrower = longer range. 125 kHz is the standard worldwide."
      >
        〰️ Bandwidth
      </p>
      <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px;" role="group" aria-label="Bandwidth selection">
        {#each BANDWIDTHS as bw}
          <button
            onclick={() => settings.bandwidthKhz = bw}
            aria-pressed={settings.bandwidthKhz === bw}
            title={BW_TOOLTIPS[bw]}
            style="
              padding: 10px 0;
              border: 1px solid {settings.bandwidthKhz === bw ? 'var(--doge-yellow)' : 'var(--doge-border)'};
              background: {settings.bandwidthKhz === bw ? 'rgba(245, 197, 24, 0.15)' : 'transparent'};
              color: {settings.bandwidthKhz === bw ? 'var(--doge-yellow)' : 'var(--doge-muted)'};
              border-radius: 8px;
              cursor: pointer;
              font-weight: {settings.bandwidthKhz === bw ? 700 : 400};
              font-family: var(--font-mono);
              transition: all 0.15s;
            "
          >
            {bw} kHz
          </button>
        {/each}
      </div>
    </div>

    <!-- ── Coding Rate ────────────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="Coding rate setting">
      <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 12px 0;"
         title="Forward error correction ratio. Higher denominator = more redundancy = better noise tolerance but slower."
      >
        🔢 Coding Rate
      </p>
      <div style="display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px;" role="group" aria-label="Coding rate selection">
        {#each CODING_RATES as cr}
          <button
            onclick={() => settings.codingRate = cr}
            aria-pressed={settings.codingRate === cr}
            title={CR_TOOLTIPS[cr]}
            style="
              padding: 10px 0;
              border: 1px solid {settings.codingRate === cr ? 'var(--doge-yellow)' : 'var(--doge-border)'};
              background: {settings.codingRate === cr ? 'rgba(245, 197, 24, 0.15)' : 'transparent'};
              color: {settings.codingRate === cr ? 'var(--doge-yellow)' : 'var(--doge-muted)'};
              border-radius: 8px;
              cursor: pointer;
              font-weight: {settings.codingRate === cr ? 700 : 400};
              font-family: var(--font-mono);
              transition: all 0.15s;
            "
          >
            {cr}
          </button>
        {/each}
      </div>
    </div>

    <!-- ── Node Address ───────────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="Node address setting">
      <div style="display: flex; align-items: flex-start; justify-content: space-between; margin-bottom: 4px; flex-wrap: wrap; gap: 8px;">
        <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0;"
           title="This node's unique address on the RadioDoge mesh. Format: Region.Community.Node (each 0–254). Address 255.255.255 is broadcast (all nodes)."
        >
          📍 Node Address
        </p>
        <button
          onclick={copyNodeAddr}
          class="btn-ghost"
          title="Copy node address to clipboard"
          aria-label="Copy node address"
          style="padding: 3px 10px; font-size: 0.72rem;"
        >
          {copiedNodeAddr ? '✅ Copied!' : '📋 Copy'}
        </button>
      </div>
      <p style="margin: 0 0 12px 0; font-size: 0.75rem; color: var(--doge-muted);">
        Region.Community.Node — identifies your device on the mesh. Much address. Very unique.
      </p>
      <div style="display: flex; align-items: center; gap: 8px;">
        {#each [
          { label: 'Region', key: 'region' as const, tooltip: 'Geographic region (0–254). Nodes in different regions cannot communicate directly.' },
          { label: 'Community', key: 'community' as const, tooltip: 'Community within region (0–254). Groups nearby nodes.' },
          { label: 'Node', key: 'node' as const, tooltip: 'Individual node ID within community (0–254). Must be unique in your mesh.' },
        ] as field, i}
          {#if i > 0}
            <span style="color: var(--doge-muted); font-weight: 700; flex-shrink: 0;">.</span>
          {/if}
          <div style="flex: 1;" title={field.tooltip}>
            <div style="font-size: 0.7rem; color: var(--doge-muted); margin-bottom: 4px; text-align: center;">
              {field.label}
            </div>
            <input
              type="number"
              bind:value={nodeAddr[field.key]}
              min="0"
              max="254"
              step="1"
              class="input-doge"
              aria-label="{field.label} address byte (0–254)"
              title={field.tooltip}
              style="text-align: center; font-family: var(--font-mono);"
            />
          </div>
        {/each}
        <div
          style="color: var(--doge-yellow); font-family: var(--font-mono); font-weight: 700; padding-top: 18px; flex-shrink: 0;"
          title="Full node address: Region.Community.Node"
        >
          = {nodeAddr.region}.{nodeAddr.community}.{nodeAddr.node}
        </div>
      </div>
    </div>

    <!-- ── v0.3.6 — Connection Type (USB / BLE) ─────────────────────────── -->
    <div class="card-doge" aria-label="Connection type">
      <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 12px 0;"
         title="Choose how to connect to your Heltec board. USB = wired serial (default). BLE = wireless Bluetooth LE (Heltec V3 only)."
      >
        🔌 Connection Type
      </p>
      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px;" role="group" aria-label="Connection type">
        {#each [
          { id: 'usb' as const, label: '🔌 USB Serial', desc: 'Wired via USB cable (default, recommended)' },
          { id: 'ble' as const, label: '📶 Bluetooth LE', desc: 'Wireless BLE — Heltec V3 with BLE firmware required' },
        ] as opt}
          <button
            onclick={() => applyConnType(opt.id)}
            aria-pressed={connType === opt.id}
            title={opt.desc}
            style="
              padding: 12px 8px;
              border: 1px solid {connType === opt.id ? 'var(--doge-yellow)' : 'var(--doge-border)'};
              background: {connType === opt.id ? 'rgba(245, 197, 24, 0.15)' : 'transparent'};
              color: {connType === opt.id ? 'var(--doge-yellow)' : 'var(--doge-muted)'};
              border-radius: 8px;
              cursor: pointer;
              font-weight: {connType === opt.id ? 700 : 400};
              font-size: 0.85rem;
              transition: all 0.15s;
              display: flex; flex-direction: column; align-items: center; gap: 4px;
            "
          >
            <span>{opt.label}</span>
            {#if connType === opt.id && opt.id === 'ble'}
              <span style="font-size: 0.65rem; color: var(--doge-yellow); opacity: 0.8;">
                Scan for "RadioDoge-XX" devices
              </span>
            {/if}
          </button>
        {/each}
      </div>
      {#if connType === 'ble'}
        <p style="margin: 8px 0 0 0; font-size: 0.75rem; color: var(--doge-muted);">
          📶 BLE mode requires Heltec firmware v0.3.6+. Connect tab will scan for "RadioDoge-XX" devices.
          Such wireless. Very Bluetooth. Wow.
        </p>
      {/if}
    </div>

    <!-- ── v0.3.7 — WiFi Toggle ────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="WiFi toggle">
      <div style="display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px;">
        <div>
          <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 4px 0;">
            📡 WiFi Radio
          </p>
          <p style="margin: 0; font-size: 0.75rem; color: var(--doge-muted);">
            Disable to save power. Persisted in board NVS. Board is source of truth.
          </p>
        </div>
        {#if connection.isConnected}
          <span style="
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.72rem;
            font-weight: 700;
            background: {connection.wifiEnabled ? 'rgba(0,255,136,0.12)' : 'rgba(100,100,100,0.1)'};
            color: {connection.wifiEnabled ? 'var(--doge-neon)' : 'var(--doge-subtle)'};
            border: 1px solid {connection.wifiEnabled ? 'rgba(0,255,136,0.35)' : 'var(--doge-border)'};
          ">
            {connection.wifiEnabled ? '🟢 WiFi ON' : '⚫ WiFi OFF'}
          </span>
        {/if}
      </div>
      <div style="display: flex; gap: 8px; margin-top: 12px; flex-wrap: wrap; align-items: center;">
        <button
          onclick={toggleWifi}
          disabled={!connection.isConnected || isTogglingWifi}
          style="
            padding: 9px 18px;
            border: 1px solid {connection.wifiEnabled ? 'var(--doge-neon)' : 'var(--doge-border)'};
            background: {connection.wifiEnabled ? 'rgba(0,255,136,0.08)' : 'transparent'};
            color: {connection.wifiEnabled ? 'var(--doge-neon)' : 'var(--doge-muted)'};
            border-radius: 8px;
            cursor: pointer;
            font-size: 0.85rem;
            opacity: {!connection.isConnected || isTogglingWifi ? 0.5 : 1};
          "
        >
          {isTogglingWifi ? '⏳ Applying…' : connection.wifiEnabled ? '📴 Disable WiFi' : '📡 Enable WiFi'}
        </button>
        {#if wifiError}
          <span style="font-size: 0.75rem; color: #ff6060;">{wifiError}</span>
        {/if}
      </div>
    </div>

    <!-- ── v0.3.8 — Board MAC Address ───────────────────────────────────────── -->
    <div class="card-doge" aria-label="Board MAC address">
      <div style="display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px;">
        <div>
          <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 4px 0;">🔑 Board MAC Address</p>
          <p style="margin: 0; font-size: 0.75rem; color: var(--doge-muted);">WiFi station MAC address of the connected board.</p>
        </div>
        {#if connection.boardMac}
          <span style="font-family: var(--font-mono); font-size: 0.88rem; color: var(--doge-yellow); font-weight: 700; letter-spacing: 0.05em;">
            {connection.boardMac}
          </span>
        {:else}
          <button
            onclick={fetchMac}
            disabled={!connection.isConnected || isQueryingMac}
            style="padding: 6px 14px; border: 1px solid var(--doge-border); background: transparent; color: var(--doge-yellow); border-radius: 6px; cursor: pointer; font-size: 0.8rem; opacity: {!connection.isConnected || isQueryingMac ? 0.5 : 1};"
          >
            {isQueryingMac ? '⏳ Reading…' : '🔍 Read MAC'}
          </button>
        {/if}
      </div>
    </div>

    <!-- ── v0.3.8 — Light/Dark Theme ─────────────────────────────────────────── -->
    <div class="card-doge" aria-label="Theme toggle">
      <div style="display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px;">
        <div>
          <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 4px 0;">
            {isDarkTheme ? '🌙 Dark Theme' : '☀️ Light Theme'}
          </p>
          <p style="margin: 0; font-size: 0.75rem; color: var(--doge-muted);">Toggle between dark and light mode. Preference saved locally.</p>
        </div>
        <button
          onclick={toggleTheme}
          style="
            padding: 9px 18px;
            border: 1px solid var(--doge-border);
            background: transparent;
            color: var(--doge-yellow);
            border-radius: 8px;
            cursor: pointer;
            font-size: 0.85rem;
          "
        >
          {isDarkTheme ? '☀️ Switch to Light' : '🌙 Switch to Dark'}
        </button>
      </div>
    </div>

    <!-- ── v0.3.6 — Gateway Mode (board-persistent) ───────────────────────── -->
    <div class="card-doge" aria-label="Gateway mode">
      <div style="display: flex; align-items: flex-start; justify-content: space-between; margin-bottom: 8px; flex-wrap: wrap; gap: 8px;">
        <div>
          <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 4px 0;"
             title="Gateway mode routes LoRa traffic to the internet. Setting is stored on the board — survives power cycles. Such persist. Very NVS."
          >
            🌐 Gateway Mode
          </p>
          <p style="margin: 0; font-size: 0.75rem; color: var(--doge-muted);">
            Stored in board NVS — persists across power cycles. Board is source of truth.
          </p>
        </div>

        <!-- Gateway mode badge -->
        {#if connection.isConnected}
          <span style="
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.72rem;
            font-weight: 700;
            background: {connection.gatewayMode ? 'rgba(0,255,136,0.12)' : 'rgba(100,100,100,0.1)'};
            color: {connection.gatewayMode ? 'var(--doge-neon)' : 'var(--doge-subtle)'};
            border: 1px solid {connection.gatewayMode ? 'rgba(0,255,136,0.35)' : 'var(--doge-border)'};
            flex-shrink: 0;
          ">
            {connection.gatewayMode ? '🟢 Gateway ON' : '⚫ Gateway OFF'}
          </span>
        {/if}
      </div>

      <div style="display: flex; gap: 8px; flex-wrap: wrap; margin-top: 4px;">
        <!-- Toggle gateway_mode on board -->
        <button
          onclick={toggleGatewayMode}
          disabled={!connection.isConnected || isSettingGateway}
          title="{connection.isConnected
            ? (connection.gatewayMode ? 'Disable gateway mode — saves to board NVS' : 'Enable gateway mode — saves to board NVS')
            : 'Connect to board first to change gateway mode'}"
          style="
            padding: 9px 18px;
            border: 1px solid {connection.gatewayMode ? 'var(--doge-neon)' : 'var(--doge-border)'};
            background: {connection.gatewayMode ? 'rgba(0,255,136,0.1)' : 'transparent'};
            color: {connection.gatewayMode ? 'var(--doge-neon)' : 'var(--doge-muted)'};
            border-radius: 8px;
            cursor: {connection.isConnected ? 'pointer' : 'not-allowed'};
            font-size: 0.85rem;
            font-weight: 600;
            transition: all 0.15s;
            display: flex; align-items: center; gap: 6px;
          "
        >
          {#if isSettingGateway}
            <DogeSpinner size="sm" message="" />
            Saving...
          {:else if connection.gatewayMode}
            🟢 Stop Gateway
          {:else}
            🌐 Start Gateway
          {/if}
        </button>

        <!-- Spawn daemon process -->
        {#if !connection.gatewayOnline}
          <button
            onclick={startGatewayDaemon}
            disabled={!connection.portName || isStartingDaemon}
            title="Spawn radiodoge-cli daemon process for this port. Routes LoRa packets to internet. Such bridge. Very daemon."
            style="
              padding: 9px 18px;
              border: 1px solid var(--doge-border);
              background: transparent;
              color: var(--doge-muted);
              border-radius: 8px;
              cursor: pointer;
              font-size: 0.85rem;
              transition: all 0.15s;
              display: flex; align-items: center; gap: 6px;
            "
          >
            {#if isStartingDaemon}
              <DogeSpinner size="sm" message="" />
              Starting...
            {:else}
              ▶ Spawn Daemon
            {/if}
          </button>
        {:else}
          <button
            onclick={stopGatewayDaemon}
            title="Stop the background gateway daemon process"
            style="
              padding: 9px 18px;
              border: 1px solid rgba(0,255,136,0.35);
              background: rgba(0,255,136,0.08);
              color: var(--doge-neon);
              border-radius: 8px;
              cursor: pointer;
              font-size: 0.85rem;
              font-weight: 600;
              transition: all 0.15s;
            "
          >
            🟢 Gateway Online — Stop
          </button>
        {/if}
      </div>

      {#if gatewayError}
        <div style="margin-top: 8px; color: var(--doge-red); font-size: 0.78rem;" role="alert">❌ {gatewayError}</div>
      {/if}
      {#if daemonError}
        <div style="margin-top: 8px; color: var(--doge-red); font-size: 0.78rem;" role="alert">❌ Daemon: {daemonError}</div>
      {/if}
    </div>

    <!-- ── v0.3.16 — BLE Advertising toggle ──────────────────────────────── -->
    <div class="card-doge" aria-label="BLE advertising toggle">
      <div style="display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px;">
        <div>
          <p style="display: block; font-weight: 600; font-size: 0.9rem; margin: 0 0 4px 0;">
            📶 BLE Advertising
          </p>
          <p style="margin: 0; font-size: 0.75rem; color: var(--doge-muted);">
            Enable/disable board BLE advertising. Disable to save power. Requires firmware v0.3.16+.
          </p>
        </div>
        {#if connection.isConnected}
          <span style="
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.72rem;
            font-weight: 700;
            background: {bleAdvertisingEnabled ? 'rgba(0,255,136,0.12)' : 'rgba(100,100,100,0.1)'};
            color: {bleAdvertisingEnabled ? 'var(--doge-neon)' : 'var(--doge-subtle)'};
            border: 1px solid {bleAdvertisingEnabled ? 'rgba(0,255,136,0.35)' : 'var(--doge-border)'};
          ">
            {bleAdvertisingEnabled ? '🟢 BLE ON' : '⚫ BLE OFF'}
          </span>
        {/if}
      </div>
      <div style="display: flex; gap: 8px; margin-top: 12px; flex-wrap: wrap; align-items: center;">
        <button
          onclick={toggleBleAdvertising}
          disabled={!connection.isConnected || isTogglingBle}
          style="
            padding: 9px 18px;
            border: 1px solid {bleAdvertisingEnabled ? 'var(--doge-neon)' : 'var(--doge-border)'};
            background: {bleAdvertisingEnabled ? 'rgba(0,255,136,0.08)' : 'transparent'};
            color: {bleAdvertisingEnabled ? 'var(--doge-neon)' : 'var(--doge-muted)'};
            border-radius: 8px;
            cursor: pointer;
            font-size: 0.85rem;
            opacity: {!connection.isConnected || isTogglingBle ? 0.5 : 1};
          "
        >
          {isTogglingBle ? '⏳ Applying…' : bleAdvertisingEnabled ? '📴 Disable BLE' : '📶 Enable BLE'}
        </button>
        {#if bleError}
          <span style="font-size: 0.75rem; color: #ff6060;">{bleError}</span>
        {/if}
      </div>
    </div>

    <!-- ── v0.3.6 — Board Sync ────────────────────────────────────────────── -->
    <div class="card-doge" aria-label="Board state sync">
      <div style="display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px;">
        <div>
          <p style="font-weight: 600; font-size: 0.9rem; margin: 0 0 4px 0;">
            🔄 Board State Sync
          </p>
          <p style="margin: 0; font-size: 0.75rem; color: var(--doge-muted);">
            Board is source of truth. Query live state — if mismatch, board wins.
          </p>
        </div>
        <button
          onclick={syncFromBoard}
          disabled={!connection.isConnected || isSyncing}
          class="btn-ghost"
          title="Query CMD_GET_SETTINGS (0x22) from board — read live node address + gateway mode. Board state is authoritative."
          style="font-size: 0.8rem; display: flex; align-items: center; gap: 6px;"
        >
          {#if isSyncing}
            <DogeSpinner size="sm" message="" />
            Syncing...
          {:else}
            🔄 Sync from Board
          {/if}
        </button>
      </div>
      {#if syncResult}
        <div
          class="slide-up"
          style="
            margin-top: 10px;
            padding: 8px 12px;
            border-radius: 8px;
            font-size: 0.8rem;
            font-family: var(--font-mono);
            background: {syncResult.startsWith('✅') ? 'rgba(0,255,136,0.08)' : 'rgba(255,140,0,0.08)'};
            border: 1px solid {syncResult.startsWith('✅') ? 'rgba(0,255,136,0.25)' : 'rgba(255,140,0,0.25)'};
            color: {syncResult.startsWith('✅') ? 'var(--doge-neon)' : 'var(--doge-orange)'};
          "
        >{syncResult}</div>
      {/if}
    </div>

    <!-- ── Save to Device button ──────────────────────────────────────────── -->
    <button
      onclick={saveSettings}
      disabled={isSaving}
      class="btn-doge"
      title="{connection.isConnected
        ? 'Send new LoRa settings to the Heltec device and verify they were accepted (round-trip check). Much configure. Very verify.'
        : 'Save settings locally — connect a device to push settings to hardware.'}"
      aria-label="Save settings to device"
      style="width: 100%; padding: 14px; font-size: 1rem; display: flex; align-items: center; justify-content: center; gap: 8px;"
    >
      {#if isSaving}
        <DogeSpinner size="sm" message="" />
        <span>Saving to device... such patience</span>
      {:else}
        📡 Save to Device
      {/if}
    </button>

    <!-- ── Save success feedback ──────────────────────────────────────────── -->
    {#if saveSuccess}
      <div
        class="slide-up"
        role="status"
        aria-live="polite"
        style="
          padding: 12px 16px;
          background: {saveVerified ? 'rgba(0, 255, 136, 0.1)' : 'rgba(245, 197, 24, 0.08)'};
          border: 1px solid {saveVerified ? 'rgba(0, 255, 136, 0.35)' : 'rgba(245, 197, 24, 0.3)'};
          border-radius: 8px;
          color: {saveVerified ? 'var(--doge-neon)' : 'var(--doge-yellow)'};
          font-size: 0.85rem;
          text-align: center;
          line-height: 1.5;
        "
      >
        {#if !connection.isConnected}
          ✅ Settings stored locally — connect a device to push them to hardware.
        {:else if saveVerified}
          ✅ <strong>Verified ✓</strong> — Settings saved &amp; confirmed on device! Much configure. Very radio. Wow.
        {:else}
          ⚠️ Settings sent — device did not confirm within 1 s. Check connection &amp; try again.
        {/if}
      </div>
    {/if}

    <!-- ── Save error feedback ────────────────────────────────────────────── -->
    {#if saveError}
      <div
        class="slide-up"
        role="alert"
        style="
          padding: 12px 16px;
          background: rgba(255, 68, 68, 0.1);
          border: 1px solid rgba(255, 68, 68, 0.3);
          border-radius: 8px;
          color: var(--doge-red);
          font-size: 0.85rem;
          line-height: 1.5;
        "
      >
        ❌ {saveError}
      </div>
    {/if}
  </div>
</div>
