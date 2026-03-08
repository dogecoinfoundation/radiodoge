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
  import { radio, updateSettings } from '$lib/stores/radio.svelte';
  import { connection } from '$lib/stores/connection.svelte';
  import DogeSpinner from './DogeSpinner.svelte';

  let isSaving = $state(false);
  let saveSuccess = $state(false);
  let saveVerified = $state<boolean | null>(null);
  let saveError = $state<string | null>(null);

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
