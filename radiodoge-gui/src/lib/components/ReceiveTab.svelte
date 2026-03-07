<script lang="ts">
  /**
   * Receive tab — live packet monitor showing all incoming LoRa packets.
   */

  import { connection } from '$lib/stores/connection.svelte';
  import { radio, clearPackets } from '$lib/stores/radio.svelte';
  import { formatTimestamp, commandName } from '$lib/types';

  // Command type → badge color
  function commandColor(cmd: number): string {
    switch (cmd) {
      case 0x02: return 'rgba(68, 136, 255, 0.2)';  // PING — blue
      case 0x03: return 'rgba(245, 197, 24, 0.15)'; // MESSAGE — yellow
      case 0x04: return 'rgba(255, 140, 0, 0.15)';  // BROADCAST — orange
      case 0x10: return 'rgba(0, 255, 136, 0.15)';  // DOGE TX — green
      default:   return 'rgba(136, 136, 136, 0.15)';
    }
  }

  function commandTextColor(cmd: number): string {
    switch (cmd) {
      case 0x02: return 'var(--doge-blue)';
      case 0x03: return 'var(--doge-yellow)';
      case 0x04: return 'var(--doge-orange)';
      case 0x10: return 'var(--doge-neon)';
      default:   return 'var(--doge-muted)';
    }
  }

  let showRaw = $state(false);
</script>

<div style="padding: 24px; max-width: 900px; margin: 0 auto;">
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 24px;">
    <div>
      <h2 style="margin: 0 0 4px 0; font-size: 1.3rem; font-weight: 700;">📥 Live Packet Monitor</h2>
      <p style="margin: 0; color: var(--doge-muted); font-size: 0.85rem;">
        Incoming LoRa packets — decoded in real-time
      </p>
    </div>

    <div style="display: flex; gap: 8px; align-items: center;">
      <!-- Raw / Decoded toggle -->
      <label style="display: flex; align-items: center; gap: 6px; cursor: pointer; font-size: 0.8rem; color: var(--doge-muted);">
        <input
          type="checkbox"
          bind:checked={showRaw}
          style="accent-color: var(--doge-yellow);"
        />
        Show raw hex
      </label>

      <!-- Clear button -->
      {#if radio.packets.length > 0}
        <button onclick={clearPackets} class="btn-ghost" style="padding: 6px 14px; font-size: 0.8rem;">
          🗑️ Clear
        </button>
      {/if}

      <!-- Stats badge -->
      <span style="
        background: rgba(245, 197, 24, 0.1);
        color: var(--doge-yellow);
        border: 1px solid rgba(245, 197, 24, 0.3);
        border-radius: 12px;
        padding: 4px 12px;
        font-size: 0.78rem;
        font-family: var(--font-mono);
        white-space: nowrap;
      ">
        {radio.totalReceived} total
      </span>
    </div>
  </div>

  {#if !connection.isConnected}
    <!-- Not connected state -->
    <div style="
      text-align: center;
      padding: 60px 20px;
      color: var(--doge-muted);
    ">
      <div style="font-size: 3rem; margin-bottom: 16px; animation: bounce-doge 2s ease-in-out infinite; display: inline-block;">
        🐕
      </div>
      <h3 style="margin: 0 0 8px 0; color: var(--doge-text);">Ears down</h3>
      <p style="margin: 0; font-size: 0.9rem;">
        Connect to a Heltec device to monitor incoming packets.<br/>
        <span style="font-style: italic;">Such listen. Very wait. Wow.</span>
      </p>
    </div>
  {:else if radio.packets.length === 0}
    <!-- Connected but no packets yet -->
    <div style="
      text-align: center;
      padding: 60px 20px;
      color: var(--doge-muted);
    ">
      <div style="
        font-size: 3rem;
        margin-bottom: 16px;
        display: inline-block;
        animation: bounce-doge 1.5s ease-in-out infinite;
      ">
        🐕
      </div>
      <h3 style="margin: 0 0 8px 0; color: var(--doge-text);">Listening for packets...</h3>
      <p style="margin: 0; font-size: 0.9rem;">
        🐕 Ears up! Waiting for incoming LoRa transmissions.<br/>
        <span style="font-style: italic;">Such patience. Very radio. Wow.</span>
      </p>
    </div>
  {:else}
    <!-- Packet list -->
    <div style="display: flex; flex-direction: column; gap: 8px;">
      {#each radio.packets as packet, i (packet.timestamp + '_' + i)}
        <div
          class="slide-up card-doge"
          style="padding: 12px 16px; display: flex; gap: 12px; align-items: flex-start;"
        >
          <!-- Timestamp -->
          <span style="
            font-family: var(--font-mono);
            font-size: 0.72rem;
            color: var(--doge-subtle);
            flex-shrink: 0;
            padding-top: 2px;
          ">
            {formatTimestamp(packet.timestamp)}
          </span>

          <!-- Command badge -->
          <span style="
            background: {commandColor(packet.command)};
            color: {commandTextColor(packet.command)};
            border-radius: 4px;
            padding: 2px 8px;
            font-size: 0.72rem;
            font-family: var(--font-mono);
            font-weight: 600;
            flex-shrink: 0;
            white-space: nowrap;
          ">
            {commandName(packet.command)}
          </span>

          <!-- Source address -->
          <span style="
            font-family: var(--font-mono);
            font-size: 0.75rem;
            color: var(--doge-muted);
            flex-shrink: 0;
            padding-top: 2px;
          ">
            {packet.source.region}.{packet.source.community}.{packet.source.node}
          </span>

          <!-- Decoded / raw content -->
          <span style="
            flex: 1;
            font-size: 0.83rem;
            color: var(--doge-text);
            word-break: break-all;
            line-height: 1.4;
          ">
            {#if showRaw || !packet.decoded}
              <span style="font-family: var(--font-mono); color: var(--doge-muted); font-size: 0.75rem;">
                {packet.payloadHex}
              </span>
            {:else}
              {packet.decoded}
            {/if}
          </span>

          <!-- RSSI -->
          <span style="
            font-family: var(--font-mono);
            font-size: 0.72rem;
            color: {packet.rssi >= -80 ? 'var(--doge-neon)' : packet.rssi >= -100 ? 'var(--doge-yellow)' : 'var(--doge-red)'};
            flex-shrink: 0;
            white-space: nowrap;
          ">
            {packet.rssi} dBm
          </span>
        </div>
      {/each}
    </div>

    <!-- Footer note -->
    <p style="
      margin-top: 16px;
      text-align: center;
      color: var(--doge-subtle);
      font-size: 0.75rem;
    ">
      Showing last {radio.packets.length} of {radio.totalReceived} total packets received
    </p>
  {/if}
</div>
