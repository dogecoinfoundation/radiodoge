<script lang="ts">
  /**
   * Settings tab — configure LoRa parameters and node address.
   * Changes are sent to the Heltec device via serial.
   */

  import { invoke } from '@tauri-apps/api/core';
  import { radio, updateSettings } from '$lib/stores/radio.svelte';
  import { connection } from '$lib/stores/connection.svelte';
  import DogeSpinner from './DogeSpinner.svelte';

  let isSaving = $state(false);
  let saveSuccess = $state(false);
  let saveError = $state<string | null>(null);

  // Local copy for editing
  let settings = $state({ ...radio.settings });
  let nodeAddr = $state({ ...radio.settings.nodeAddress });

  const SPREADING_FACTORS = [7, 8, 9, 10, 11, 12];
  const BANDWIDTHS = [125, 250, 500];
  const CODING_RATES = ['4/5', '4/6', '4/7', '4/8'];

  async function saveSettings() {
    isSaving = true;
    saveSuccess = false;
    saveError = null;

    const fullSettings = {
      ...settings,
      nodeAddress: nodeAddr,
    };

    try {
      await invoke('update_lora_settings', { settings: fullSettings });
      updateSettings(fullSettings);
      saveSuccess = true;
      setTimeout(() => { saveSuccess = false; }, 3000);
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
</script>

<div style="padding: 24px; max-width: 700px; margin: 0 auto;">
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 24px;">
    <div>
      <h2 style="margin: 0 0 4px 0; font-size: 1.3rem; font-weight: 700;">⚙️ LoRa Settings</h2>
      <p style="margin: 0; color: var(--doge-muted); font-size: 0.85rem;">
        Configure radio parameters — sent to Heltec device
      </p>
    </div>
    <button onclick={resetDefaults} class="btn-ghost" style="font-size: 0.8rem;">
      ↺ Defaults
    </button>
  </div>

  <div style="display: flex; flex-direction: column; gap: 16px;">

    <!-- Frequency -->
    <div class="card-doge">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 12px;">
        <label style="font-weight: 600; font-size: 0.9rem;">📻 Frequency</label>
        <span style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 700;">
          {settings.frequencyMhz} MHz
        </span>
      </div>
      <input
        type="number"
        bind:value={settings.frequencyMhz}
        min="433"
        max="915"
        step="0.1"
        class="input-doge"
      />
      <p style="margin: 6px 0 0 0; font-size: 0.75rem; color: var(--doge-muted);">
        433 MHz (EU/Asia) or 915 MHz (US/AU). Match your Heltec firmware setting.
      </p>
    </div>

    <!-- TX Power -->
    <div class="card-doge">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 12px;">
        <label style="font-weight: 600; font-size: 0.9rem;">⚡ TX Power</label>
        <span style="font-family: var(--font-mono); color: var(--doge-yellow); font-weight: 700;">
          {settings.powerDbm} dBm
        </span>
      </div>
      <input
        type="range"
        bind:value={settings.powerDbm}
        min="5"
        max="20"
        step="1"
        style="
          width: 100%;
          accent-color: var(--doge-yellow);
          cursor: pointer;
        "
      />
      <div style="display: flex; justify-content: space-between; font-size: 0.72rem; color: var(--doge-subtle); margin-top: 4px;">
        <span>5 dBm (low power)</span>
        <span>20 dBm (maximum range)</span>
      </div>
    </div>

    <!-- Spreading Factor -->
    <div class="card-doge">
      <label style="display: block; font-weight: 600; font-size: 0.9rem; margin-bottom: 12px;">
        📶 Spreading Factor
      </label>
      <div style="display: grid; grid-template-columns: repeat(6, 1fr); gap: 8px;">
        {#each SPREADING_FACTORS as sf}
          <button
            onclick={() => settings.spreadingFactor = sf}
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
        Higher SF = longer range, lower data rate. Default: SF7 (fastest).
      </p>
    </div>

    <!-- Bandwidth -->
    <div class="card-doge">
      <label style="display: block; font-weight: 600; font-size: 0.9rem; margin-bottom: 12px;">
        〰️ Bandwidth
      </label>
      <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px;">
        {#each BANDWIDTHS as bw}
          <button
            onclick={() => settings.bandwidthKhz = bw}
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

    <!-- Coding Rate -->
    <div class="card-doge">
      <label style="display: block; font-weight: 600; font-size: 0.9rem; margin-bottom: 12px;">
        🔢 Coding Rate
      </label>
      <div style="display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px;">
        {#each CODING_RATES as cr}
          <button
            onclick={() => settings.codingRate = cr}
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

    <!-- Node Address -->
    <div class="card-doge">
      <label style="display: block; font-weight: 600; font-size: 0.9rem; margin-bottom: 4px;">
        📍 Node Address
      </label>
      <p style="margin: 0 0 12px 0; font-size: 0.75rem; color: var(--doge-muted);">
        Region.Community.Node — identifies your device on the mesh
      </p>
      <div style="display: flex; align-items: center; gap: 8px;">
        {#each [
          { label: 'Region', key: 'region' as const },
          { label: 'Community', key: 'community' as const },
          { label: 'Node', key: 'node' as const },
        ] as field, i}
          {#if i > 0}
            <span style="color: var(--doge-muted); font-weight: 700;">.</span>
          {/if}
          <div style="flex: 1;">
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
              style="text-align: center; font-family: var(--font-mono);"
            />
          </div>
        {/each}
        <div style="color: var(--doge-yellow); font-family: var(--font-mono); font-weight: 700; padding-top: 18px; flex-shrink: 0;">
          = {nodeAddr.region}.{nodeAddr.community}.{nodeAddr.node}
        </div>
      </div>
    </div>

    <!-- Save button -->
    <button
      onclick={saveSettings}
      disabled={isSaving}
      class="btn-doge"
      style="width: 100%; padding: 14px; font-size: 1rem;"
    >
      {#if isSaving}
        <DogeSpinner size="sm" message="" />
        <span style="margin-left: 8px;">Saving to device...</span>
      {:else}
        📡 Save to Device
      {/if}
    </button>

    {#if saveSuccess}
      <div class="slide-up" style="
        padding: 12px 16px;
        background: rgba(0, 255, 136, 0.1);
        border: 1px solid rgba(0, 255, 136, 0.3);
        border-radius: 8px;
        color: var(--doge-neon);
        font-size: 0.85rem;
        text-align: center;
      ">
        ✅ Settings saved! Much configure. Very radio. Wow.
        {#if !connection.isConnected}
          <span style="display: block; font-size: 0.75rem; color: var(--doge-muted); margin-top: 4px;">
            (Settings stored locally — connect a device to apply them)
          </span>
        {/if}
      </div>
    {/if}

    {#if saveError}
      <div class="slide-up" style="
        padding: 12px 16px;
        background: rgba(255, 68, 68, 0.1);
        border: 1px solid rgba(255, 68, 68, 0.3);
        border-radius: 8px;
        color: var(--doge-red);
        font-size: 0.85rem;
      ">
        ❌ {saveError}
      </div>
    {/if}
  </div>
</div>
