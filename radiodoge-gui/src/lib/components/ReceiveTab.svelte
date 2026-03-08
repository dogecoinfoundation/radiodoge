<script lang="ts">
  /**
   * Receive tab — live packet monitor for ALL LoRa traffic (TX + RX).
   *
   * v0.3.1 improvements:
   *   - Clear TX (↑ green) / RX (↓ blue) direction labels with icons
   *   - TX/RX filter toggle buttons (show all / TX only / RX only)
   *   - Distinct colors and tooltips per direction and command type
   *   - Copy-to-clipboard on every packet row
   *   - "Much radio" Doge flair throughout
   */

  import { connection } from '$lib/stores/connection.svelte';
  import { radio, clearPackets, type PacketEntry } from '$lib/stores/radio.svelte';
  import { formatTimestamp, commandName } from '$lib/types';

  // ── Filter state ─────────────────────────────────────────────────────────
  type Filter = 'all' | 'tx' | 'rx';
  let filter = $state<Filter>('all');

  let showRaw = $state(false);

  // ── Computed filtered list ────────────────────────────────────────────────
  const filteredPackets = $derived(() => {
    if (filter === 'tx') return radio.packets.filter(p => p.direction === 'TX');
    if (filter === 'rx') return radio.packets.filter(p => p.direction === 'RX');
    return radio.packets;
  });

  // ── Command badge helpers ─────────────────────────────────────────────────
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

  function commandTooltip(cmd: number): string {
    const map: Record<number, string> = {
      0x00: 'GET ADDR — query device mesh address',
      0x01: 'SET ADDR — update device mesh address',
      0x02: 'PING — health check (much echo, very pong)',
      0x03: 'MESSAGE — direct text to a node',
      0x04: 'BROADCAST — text to ALL nodes in range',
      0x05: 'MULTIPART — large payload split across packets',
      0x10: 'DOGE TX — Dogecoin transaction over LoRa 🐕',
      0x11: 'BALANCE — UTXO balance request',
      0x20: 'FW VERSION — firmware version query',
    };
    return map[cmd] ?? `Unknown command 0x${cmd.toString(16).toUpperCase()}`;
  }

  // ── Copy state ───────────────────────────────────────────────────────────
  let copiedIndex = $state<number | null>(null);
  async function copyPacket(text: string, idx: number) {
    try {
      await navigator.clipboard.writeText(text);
      copiedIndex = idx;
      setTimeout(() => { copiedIndex = null; }, 1500);
    } catch {
      // Browser may block in some sandboxed envs — fail silently
    }
  }
</script>

<div style="padding: 24px; max-width: 960px; margin: 0 auto;">

  <!-- ── Header ──────────────────────────────────────────────────────────── -->
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 20px; flex-wrap: wrap; gap: 12px;">
    <div>
      <h2 style="margin: 0 0 4px 0; font-size: 1.3rem; font-weight: 700;">📡 Live Packet Log</h2>
      <p style="margin: 0; color: var(--doge-muted); font-size: 0.85rem;">
        All LoRa traffic — TX (↑ sent) and RX (↓ received)
      </p>
    </div>

    <div style="display: flex; gap: 8px; align-items: center; flex-wrap: wrap;">

      <!-- TX/RX filter pills -->
      <div
        role="group"
        aria-label="Filter packets by direction"
        style="display: flex; background: var(--doge-dark); border: 1px solid var(--doge-border); border-radius: 10px; overflow: hidden;"
      >
        {#each ([['all','All'],['tx','↑ TX'],['rx','↓ RX']] as const) as [val, label]}
          <button
            onclick={() => filter = val}
            aria-pressed={filter === val}
            title="Show {val === 'all' ? 'all packets' : val === 'tx' ? 'sent (TX) packets only' : 'received (RX) packets only'}"
            style="
              padding: 5px 14px;
              border: none;
              background: {filter === val ? 'rgba(245,197,24,0.15)' : 'transparent'};
              color: {filter === val ? 'var(--doge-yellow)' : 'var(--doge-muted)'};
              font-size: 0.78rem;
              font-weight: {filter === val ? 700 : 400};
              cursor: pointer;
              transition: all 0.15s;
              border-right: 1px solid var(--doge-border);
            "
          >{label}</button>
        {/each}
      </div>

      <!-- Raw hex toggle -->
      <label
        title="Toggle between human-readable decoded text and raw hex bytes"
        style="display: flex; align-items: center; gap: 6px; cursor: pointer; font-size: 0.8rem; color: var(--doge-muted);"
      >
        <input
          type="checkbox"
          bind:checked={showRaw}
          style="accent-color: var(--doge-yellow);"
          aria-label="Show raw hex"
        />
        Hex
      </label>

      <!-- Clear button -->
      {#if radio.packets.length > 0}
        <button
          onclick={clearPackets}
          class="btn-ghost"
          title="Clear all packets from the log"
          style="padding: 5px 14px; font-size: 0.8rem;"
          aria-label="Clear packet log"
        >
          🗑️ Clear
        </button>
      {/if}

      <!-- Stats badges -->
      <span
        title="Total RX packets received since connect"
        style="
          background: rgba(68, 136, 255, 0.1);
          color: var(--doge-blue);
          border: 1px solid rgba(68, 136, 255, 0.25);
          border-radius: 12px;
          padding: 4px 10px;
          font-size: 0.75rem;
          font-family: var(--font-mono);
          white-space: nowrap;
        "
      >
        ↓ {radio.totalReceived} RX
      </span>
      <span
        title="Total TX packets sent since connect"
        style="
          background: rgba(0, 255, 136, 0.1);
          color: var(--doge-neon);
          border: 1px solid rgba(0, 255, 136, 0.25);
          border-radius: 12px;
          padding: 4px 10px;
          font-size: 0.75rem;
          font-family: var(--font-mono);
          white-space: nowrap;
        "
      >
        ↑ {radio.totalSent} TX
      </span>
    </div>
  </div>

  <!-- ── Empty / disconnected states ────────────────────────────────────── -->
  {#if !connection.isConnected}
    <div style="text-align: center; padding: 60px 20px; color: var(--doge-muted);">
      <div style="font-size: 3rem; margin-bottom: 16px; animation: bounce-doge 2s ease-in-out infinite; display: inline-block;">🐕</div>
      <h3 style="margin: 0 0 8px 0; color: var(--doge-text);">Ears down</h3>
      <p style="margin: 0; font-size: 0.9rem;">
        Connect to a Heltec device to monitor LoRa traffic.<br/>
        <span style="font-style: italic;">Such listen. Very wait. Wow.</span>
      </p>
    </div>

  {:else if filteredPackets().length === 0}
    <div style="text-align: center; padding: 60px 20px; color: var(--doge-muted);">
      <div style="font-size: 3rem; margin-bottom: 16px; display: inline-block; animation: bounce-doge 1.5s ease-in-out infinite;">🐕</div>
      <h3 style="margin: 0 0 8px 0; color: var(--doge-text);">
        {filter === 'all' ? 'Listening for packets...' : filter === 'tx' ? 'No TX packets yet' : 'No RX packets yet'}
      </h3>
      <p style="margin: 0; font-size: 0.9rem;">
        {#if filter === 'tx'}
          Send a transaction to see TX packets here. 🐕
        {:else if filter === 'rx'}
          🐕 Ears up! Waiting for incoming LoRa transmissions.<br/>
          <span style="font-style: italic;">Such patience. Very radio. Wow.</span>
        {:else}
          🐕 Ears up! Waiting for LoRa traffic...<br/>
          <span style="font-style: italic;">Such patience. Very radio. Wow.</span>
        {/if}
      </p>
    </div>

  {:else}
    <!-- ── Packet list ────────────────────────────────────────────────── -->
    <div style="display: flex; flex-direction: column; gap: 6px;" role="log" aria-label="Packet log" aria-live="polite">
      {#each filteredPackets() as packet, i (packet.timestamp + '_' + i + '_' + packet.direction)}
        {@const isTx = packet.direction === 'TX'}
        <div
          class="slide-up card-doge"
          style="
            padding: 10px 14px;
            display: flex;
            gap: 10px;
            align-items: flex-start;
            border-left: 3px solid {isTx ? 'var(--doge-neon)' : 'var(--doge-blue)'};
          "
          aria-label="{isTx ? 'Sent' : 'Received'} packet at {formatTimestamp(packet.timestamp)}"
        >
          <!-- Direction indicator with icon + tooltip -->
          <span
            title="{isTx ? '↑ TX — Sent by this node over LoRa. Such broadcast, very radio.' : '↓ RX — Received from remote node. Much incoming, very packet.'}"
            style="
              font-size: 0.85rem;
              font-weight: 700;
              flex-shrink: 0;
              color: {isTx ? 'var(--doge-neon)' : 'var(--doge-blue)'};
              min-width: 28px;
              text-align: center;
              padding-top: 1px;
            "
            aria-label="{isTx ? 'Transmitted' : 'Received'}"
          >
            {isTx ? '↑' : '↓'}
          </span>

          <!-- Timestamp -->
          <span style="
            font-family: var(--font-mono);
            font-size: 0.72rem;
            color: var(--doge-subtle);
            flex-shrink: 0;
            padding-top: 2px;
            white-space: nowrap;
          ">
            {formatTimestamp(packet.timestamp)}
          </span>

          <!-- Command badge with tooltip -->
          <span
            title={commandTooltip(packet.command)}
            style="
              background: {commandColor(packet.command)};
              color: {commandTextColor(packet.command)};
              border-radius: 4px;
              padding: 2px 7px;
              font-size: 0.7rem;
              font-family: var(--font-mono);
              font-weight: 600;
              flex-shrink: 0;
              white-space: nowrap;
              cursor: help;
            "
          >
            {commandName(packet.command)}
          </span>

          <!-- Source address -->
          <span
            title="Source node address (Region.Community.Node)"
            style="
              font-family: var(--font-mono);
              font-size: 0.72rem;
              color: var(--doge-muted);
              flex-shrink: 0;
              padding-top: 2px;
              cursor: help;
            "
          >
            {packet.source.region}.{packet.source.community}.{packet.source.node}
          </span>

          <!-- Decoded / raw content -->
          <span style="
            flex: 1;
            font-size: 0.82rem;
            color: {isTx ? '#b3ffe0' : 'var(--doge-text)'};
            word-break: break-all;
            line-height: 1.4;
          ">
            {#if showRaw || !packet.decoded}
              <span style="font-family: var(--font-mono); color: var(--doge-muted); font-size: 0.73rem;">
                {packet.payloadHex}
              </span>
            {:else}
              {packet.decoded}
            {/if}
          </span>

          <!-- RSSI (only meaningful for RX packets) -->
          {#if !isTx}
            <span
              title="Received Signal Strength Indicator — {packet.rssi >= -80 ? 'Strong signal! Much wow.' : packet.rssi >= -100 ? 'Moderate signal' : 'Weak signal — move nodes closer!'}"
              style="
                font-family: var(--font-mono);
                font-size: 0.7rem;
                color: {packet.rssi >= -80 ? 'var(--doge-neon)' : packet.rssi >= -100 ? 'var(--doge-yellow)' : 'var(--doge-red)'};
                flex-shrink: 0;
                white-space: nowrap;
                cursor: help;
              "
            >
              {packet.rssi} dBm
            </span>
          {:else}
            <span style="font-size: 0.7rem; color: var(--doge-subtle); flex-shrink: 0; white-space: nowrap;">
              sent ↑
            </span>
          {/if}

          <!-- Copy button -->
          <button
            onclick={() => copyPacket(
              showRaw || !packet.decoded ? packet.payloadHex : (packet.decoded ?? packet.payloadHex),
              i
            )}
            class="btn-ghost"
            title="Copy {showRaw || !packet.decoded ? 'raw hex' : 'decoded text'} to clipboard"
            aria-label="Copy packet data"
            style="padding: 2px 6px; font-size: 0.7rem; flex-shrink: 0;"
          >
            {copiedIndex === i ? '✅' : '📋'}
          </button>
        </div>
      {/each}
    </div>

    <!-- Footer summary -->
    <p style="
      margin-top: 14px;
      text-align: center;
      color: var(--doge-subtle);
      font-size: 0.73rem;
    ">
      Showing {filteredPackets().length} of {radio.packets.length} packets
      {filter !== 'all' ? `(${filter.toUpperCase()} filter active)` : ''}
      · {radio.totalReceived} total received · {radio.totalSent} total sent
    </p>
  {/if}
</div>
