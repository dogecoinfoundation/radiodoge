<script lang="ts">
  /**
   * Connection panel — the first tab the user sees.
   * Handles COM port selection and connecting to the Heltec device.
   */

  import { invoke } from '@tauri-apps/api/core';
  import { connection, setAvailablePorts, setConnecting, setDisconnected, setError } from '$lib/stores/connection.svelte';
  import SignalBars from './SignalBars.svelte';
  import DogeSpinner from './DogeSpinner.svelte';
  import { nodeAddressToString } from '$lib/types';

  let selectedPort = $state('');
  let isRefreshing = $state(false);

  async function refreshPorts() {
    isRefreshing = true;
    try {
      const ports = await invoke<string[]>('list_ports');
      setAvailablePorts(ports);
      if (ports.length > 0 && !selectedPort) {
        selectedPort = ports[0];
      }
      if (ports.length === 0) {
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
    try {
      await invoke('connect_port', { port: selectedPort });
      // Status update comes via the "connection-status" event in +page.svelte
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : String(e);
      setError(msg);
    }
  }

  async function disconnect() {
    try {
      await invoke('disconnect_port');
    } catch (e) {
      console.error('Disconnect error:', e);
      setDisconnected();
    }
  }

  // Refresh ports on mount
  $effect(() => {
    refreshPorts();
  });
</script>

<div style="max-width: 600px; margin: 0 auto; padding: 32px 24px;">
  <!-- Hero section -->
  <div style="text-align: center; margin-bottom: 40px;">
    <div style="font-size: 4rem; margin-bottom: 16px; animation: bounce-doge 2s ease-in-out infinite; display: inline-block;">
      🐕
    </div>
    <h1 style="
      font-size: 2rem;
      font-weight: 800;
      background: linear-gradient(135deg, var(--doge-yellow), var(--doge-orange));
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
      margin: 0 0 8px 0;
    ">
      RadioDoge
    </h1>
    <p style="color: var(--doge-muted); margin: 0; font-size: 0.95rem;">
      Wireless P2P Dogecoin — no internet required 🌐✖️
    </p>
  </div>

  <!-- Connection Card -->
  <div class="card-doge {connection.isConnected ? 'card-connected' : ''}">
    <h2 style="margin: 0 0 20px 0; font-size: 1.1rem; font-weight: 600; color: var(--doge-text);">
      🔌 Connect to Heltec Device
    </h2>

    <!-- Port selection -->
    <div style="margin-bottom: 16px;">
      <label style="display: block; font-size: 0.8rem; color: var(--doge-muted); margin-bottom: 8px; text-transform: uppercase; letter-spacing: 0.05em;">
        Serial Port
      </label>
      <div style="display: flex; gap: 8px;">
        <select
          bind:value={selectedPort}
          disabled={connection.isConnected || connection.isConnecting}
          style="
            flex: 1;
            background: var(--doge-dark);
            border: 1px solid var(--doge-border);
            border-radius: 8px;
            color: {selectedPort ? 'var(--doge-text)' : 'var(--doge-subtle)'};
            padding: 10px 14px;
            font-size: 0.9rem;
            outline: none;
            font-family: var(--font-mono);
            cursor: pointer;
          "
        >
          {#if connection.availablePorts.length === 0}
            <option value="">No ports found — plug in Heltec!</option>
          {:else}
            <option value="" disabled>Select a port...</option>
            {#each connection.availablePorts as port}
              <option value={port}>{port}</option>
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

      {#if connection.availablePorts.length === 0 && !isRefreshing}
        <p style="color: var(--doge-muted); font-size: 0.78rem; margin: 8px 0 0 0;">
          💡 Connect your Heltec ESP32 via USB, then click ⟳ to refresh
        </p>
      {/if}
    </div>

    <!-- Connect / Disconnect button -->
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

    <!-- Error message -->
    {#if connection.error}
      <div style="
        margin-top: 12px;
        padding: 10px 14px;
        background: rgba(255, 68, 68, 0.1);
        border: 1px solid rgba(255, 68, 68, 0.3);
        border-radius: 8px;
        color: var(--doge-red);
        font-size: 0.85rem;
      ">
        ❌ {connection.error}
      </div>
    {/if}
  </div>

  <!-- Connected status card -->
  {#if connection.isConnected}
    <div class="card-doge card-connected" style="margin-top: 16px; animation: slide-up 0.3s ease;">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px;">
        <h3 style="margin: 0; font-size: 1rem; color: var(--doge-neon);">✅ Connected!</h3>
        <SignalBars rssi={connection.stats?.rssi ?? -120} connected={true} size="md" />
      </div>

      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px;">
        <div>
          <div class="stat-label">Port</div>
          <div style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 600;">
            {connection.portName}
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

      <p style="
        margin: 16px 0 0 0;
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

  <!-- How it works section -->
  {#if !connection.isConnected}
    <div style="margin-top: 32px;">
      <h3 style="color: var(--doge-muted); font-size: 0.85rem; text-transform: uppercase; letter-spacing: 0.08em; margin-bottom: 16px;">
        How it works
      </h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        {#each [
          { icon: '1️⃣', text: 'Flash Heltec firmware (see docs/), connect via USB' },
          { icon: '2️⃣', text: 'Select the COM port above and click Connect' },
          { icon: '3️⃣', text: 'Generate a Dogecoin wallet in the Wallet tab' },
          { icon: '4️⃣', text: 'Send DOGE over LoRa radio — no internet needed! 🚀' },
        ] as step}
          <div style="display: flex; align-items: flex-start; gap: 12px; color: var(--doge-muted); font-size: 0.88rem;">
            <span style="font-size: 1.1rem; flex-shrink: 0;">{step.icon}</span>
            <span>{step.text}</span>
          </div>
        {/each}
      </div>
    </div>
  {/if}
</div>
