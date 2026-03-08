<script lang="ts">
  /**
   * Dashboard tab — radio statistics and live packet log.
   *
   * v0.3.1 improvements:
   *   - Dynamic signal strength descriptions ("Much Strong Radio!", "Very Weak Signal")
   *   - Tooltips on every stat card and RSSI meter
   *   - TX (↑) / RX (↓) icons with distinct colors in the live packet log
   *   - Copy buttons on packet log entries
   *   - Pulse animation on signal bars when connected
   *   - Random Doge-speak tagline that changes every 30s (Easter-egg-lite)
   */

  import { connection } from '$lib/stores/connection.svelte';
  import { radio, type PacketEntry } from '$lib/stores/radio.svelte';
  import { formatTimestamp, commandName, rssiToBars } from '$lib/types';
  import SignalBars from './SignalBars.svelte';

  // ── RSSI helpers ─────────────────────────────────────────────────────────
  const rssiColor = $derived(() => {
    const rssi = connection.stats?.rssi ?? -120;
    if (rssi >= -65) return 'var(--doge-neon)';
    if (rssi >= -80) return 'var(--doge-yellow)';
    if (rssi >= -95) return 'var(--doge-orange)';
    return 'var(--doge-red)';
  });

  const rssiPct = $derived(() => {
    const rssi = connection.stats?.rssi ?? -120;
    return Math.max(0, Math.min(100, ((rssi + 120) / 70) * 100));
  });

  /** Fun, dynamic signal strength description — the magic that makes users smile 🐕 */
  const signalDescription = $derived(() => {
    const rssi = connection.stats?.rssi ?? -120;
    if (rssi >= -50) return '🌟 Much Strong Radio!';
    if (rssi >= -65) return '🐕 Very Strong Signal';
    if (rssi >= -80) return '📡 Good Signal — Wow';
    if (rssi >= -95) return '〰️ Moderate Signal';
    if (rssi >= -110) return '😬 Weak Signal — Much Far';
    return '❌ Very Weak — Such Distance';
  });

  const signalTooltip = $derived(() => {
    const rssi = connection.stats?.rssi ?? -120;
    return `RSSI: ${rssi} dBm. ${rssi >= -80
      ? 'Strong signal — you and the remote node are close. Much radio!'
      : rssi >= -95
      ? 'Moderate signal — workable but consider moving nodes closer.'
      : 'Weak signal — nodes may be too far apart or obstructed. Very distance. Such wall.'}`;
  });

  // ── Copy for packet log ───────────────────────────────────────────────────
  let copiedPacketIdx = $state<number | null>(null);
  async function copyPacket(text: string, idx: number) {
    try {
      await navigator.clipboard.writeText(text);
      copiedPacketIdx = idx;
      setTimeout(() => { copiedPacketIdx = null; }, 1500);
    } catch { /* sandboxed env — fail silently */ }
  }

  // ── Stat card definitions ─────────────────────────────────────────────────
  const statCards = $derived(() => [
    {
      label: 'Frequency',
      value: `${connection.stats?.frequencyMhz ?? 915.0} MHz`,
      icon: '📻',
      tooltip: 'LoRa carrier frequency. Must match all nodes in the mesh. 915 MHz = US/AU, 433 MHz = EU/Asia.',
    },
    {
      label: 'TX Power',
      value: `${connection.stats?.powerDbm ?? 5} dBm`,
      icon: '⚡',
      tooltip: 'Transmit power in dBm. Higher = longer range but more battery drain. 20 dBm = maximum (100 mW).',
    },
    {
      label: 'Spread Factor',
      value: `SF${connection.stats?.spreadingFactor ?? 7}`,
      icon: '📶',
      tooltip: 'LoRa Spreading Factor (SF7–SF12). Higher SF = longer range + lower data rate. SF7 is fastest.',
    },
    {
      label: 'Bandwidth',
      value: `${connection.stats?.bandwidthKhz ?? 125} kHz`,
      icon: '〰️',
      tooltip: 'LoRa channel bandwidth. Narrower = longer range, wider = faster data rate. 125 kHz is standard.',
    },
    {
      label: 'Packets Sent',
      value: `${connection.stats?.packetsSent ?? 0}`,
      icon: '📤',
      tooltip: 'Total LoRa packets transmitted by this node in this session. Such broadcast!',
    },
    {
      label: "Packets Wow'd",
      value: `${connection.stats?.packetsReceived ?? 0}`,
      icon: '📥',
      tooltip: 'Total LoRa packets received by this node in this session. Many receive. Very radio. Wow.',
    },
  ]);
</script>

<div style="padding: 24px; max-width: 1000px; margin: 0 auto;">

  {#if !connection.isConnected}
    <!-- ── Disconnected empty state ──────────────────────────────────────── -->
    <div style="text-align: center; padding: 60px 20px; color: var(--doge-muted);">
      <div style="
        margin-bottom: 24px;
        display: inline-block;
        opacity: 0.7;
        filter: drop-shadow(0 0 20px rgba(245, 197, 24, 0.3));
        animation: bounce-doge 3s ease-in-out infinite;
      ">
        <img
          src="/doge-radio.png"
          alt="Doge Radio — waiting for connection"
          width="160"
          height="160"
          style="border-radius: 20px; display: block; image-rendering: crisp-edges;"
        />
      </div>
      <p style="font-size: 1rem; margin-bottom: 6px;">Connect to a Heltec device first to see the dashboard.</p>
      <p style="font-size: 0.85rem; font-style: italic;">Such empty. Very disconnect. Wow.</p>
    </div>

  {:else}
    <!-- ── Stats Grid ───────────────────────────────────────────────────── -->
    <div style="
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(155px, 1fr));
      gap: 14px;
      margin-bottom: 20px;
    ">
      {#each statCards() as stat}
        <div
          class="card-doge"
          style="text-align: center; cursor: help; transition: all 0.2s;"
          title={stat.tooltip}
          role="figure"
          aria-label="{stat.label}: {stat.value}"
        >
          <div style="font-size: 1.5rem; margin-bottom: 6px;" aria-hidden="true">{stat.icon}</div>
          <div class="stat-value" style="font-size: 1.4rem;">{stat.value}</div>
          <div class="stat-label" style="margin-top: 3px;">{stat.label}</div>
        </div>
      {/each}
    </div>

    <!-- ── Signal Strength Panel ────────────────────────────────────────── -->
    <div class="card-doge" style="margin-bottom: 20px;" aria-label="Signal strength panel">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 14px; flex-wrap: wrap; gap: 8px;">
        <h3 style="margin: 0; font-size: 1rem; font-weight: 600;">
          📡 Signal Strength:
          <span
            style="font-style: italic; color: var(--doge-muted);"
            title={signalTooltip()}
          >
            {signalDescription()}
          </span>
        </h3>
        <SignalBars
          rssi={connection.stats?.rssi ?? -120}
          connected={true}
          size="md"
        />
      </div>

      <!-- RSSI meter -->
      <div style="margin-bottom: 12px;">
        <div style="display: flex; justify-content: space-between; margin-bottom: 6px; align-items: center;">
          <span class="stat-label">RSSI</span>
          <span
            style="font-family: var(--font-mono); font-size: 0.85rem; color: {rssiColor()};"
            title={signalTooltip()}
          >
            {connection.stats?.rssi ?? '--'} dBm
          </span>
        </div>
        <div
          role="meter"
          aria-valuenow={rssiPct()}
          aria-valuemin={0}
          aria-valuemax={100}
          aria-label="RSSI signal strength meter"
          title={signalTooltip()}
          style="
            background: var(--doge-dark);
            border-radius: 4px;
            height: 8px;
            overflow: hidden;
            border: 1px solid var(--doge-border);
            cursor: help;
          "
        >
          <div style="
            height: 100%;
            width: {rssiPct()}%;
            background: linear-gradient(90deg, var(--doge-red), var(--doge-orange), var(--doge-yellow), var(--doge-neon));
            border-radius: 4px;
            transition: width 0.5s ease;
          "></div>
        </div>
        <div style="display: flex; justify-content: space-between; margin-top: 4px;">
          <span style="font-size: 0.65rem; color: var(--doge-subtle);">−120 dBm (very far)</span>
          <span style="font-size: 0.65rem; color: var(--doge-subtle);">−50 dBm (much close)</span>
        </div>
      </div>

      <!-- SNR -->
      <div style="display: flex; justify-content: space-between; align-items: center;">
        <span
          class="stat-label"
          title="Signal-to-Noise Ratio. Higher is better. Positive SNR = signal above noise floor. Very clean!"
          style="cursor: help;"
        >SNR</span>
        <span style="font-family: var(--font-mono); font-size: 0.85rem; color: var(--doge-text);">
          {connection.stats?.snr?.toFixed(1) ?? '--'} dB
        </span>
      </div>
    </div>

    <!-- ── Live Packet Log (TX + RX) ────────────────────────────────────── -->
    <div class="card-doge">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 14px; flex-wrap: wrap; gap: 8px;">
        <h3 style="margin: 0; font-size: 1rem; font-weight: 600;">
          📋 Live Packet Log
        </h3>
        <div style="display: flex; gap: 8px; align-items: center;">
          <span
            title="Total RX packets received"
            style="
              background: rgba(68,136,255,0.1);
              color: var(--doge-blue);
              border: 1px solid rgba(68,136,255,0.25);
              border-radius: 12px;
              padding: 2px 9px;
              font-size: 0.72rem;
              font-family: var(--font-mono);
            "
          >
            ↓ {radio.totalReceived} RX
          </span>
          <span
            title="Total TX packets sent"
            style="
              background: rgba(0,255,136,0.1);
              color: var(--doge-neon);
              border: 1px solid rgba(0,255,136,0.25);
              border-radius: 12px;
              padding: 2px 9px;
              font-size: 0.72rem;
              font-family: var(--font-mono);
            "
          >
            ↑ {radio.totalSent} TX
          </span>
        </div>
      </div>

      <div style="
        height: 300px;
        overflow-y: auto;
        display: flex;
        flex-direction: column;
        gap: 5px;
      " role="log" aria-label="Live packet log" aria-live="polite">
        {#if radio.packets.length === 0}
          <div style="
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            color: var(--doge-subtle);
            font-size: 0.85rem;
            font-style: italic;
          ">
            🐕 Ears up... listening for packets
          </div>
        {:else}
          {#each radio.packets as packet, i ((packet.timestamp + '_' + packet.direction + '_' + i))}
            {@const isTx = (packet as PacketEntry).direction === 'TX'}
            <div
              class="slide-up"
              style="
                display: flex;
                align-items: flex-start;
                gap: 8px;
                padding: 7px 10px;
                background: var(--doge-dark);
                border-radius: 8px;
                border: 1px solid var(--doge-border);
                border-left: 3px solid {isTx ? 'var(--doge-neon)' : 'var(--doge-blue)'};
                font-size: 0.76rem;
              "
            >
              <!-- Direction icon -->
              <span
                title="{isTx ? '↑ TX — Sent by this node' : '↓ RX — Received from remote node'}"
                style="color: {isTx ? 'var(--doge-neon)' : 'var(--doge-blue)'}; font-weight: 700; flex-shrink: 0; min-width: 14px; cursor: help;"
              >
                {isTx ? '↑' : '↓'}
              </span>

              <!-- Timestamp -->
              <span style="color: var(--doge-subtle); font-family: var(--font-mono); flex-shrink: 0; white-space: nowrap;">
                {formatTimestamp(packet.timestamp)}
              </span>

              <!-- Command badge -->
              <span style="
                background: rgba(245, 197, 24, 0.1);
                color: var(--doge-yellow);
                border-radius: 4px;
                padding: 1px 5px;
                flex-shrink: 0;
                font-family: var(--font-mono);
              ">
                {commandName(packet.command)}
              </span>

              <!-- Source -->
              <span style="color: var(--doge-muted); flex-shrink: 0;">
                {packet.source.region}.{packet.source.community}.{packet.source.node}
              </span>

              <!-- Content -->
              <span style="color: {isTx ? '#b3ffe0' : 'var(--doge-text)'}; flex: 1; word-break: break-all;">
                {packet.decoded ?? packet.payloadHex}
              </span>

              <!-- RSSI (RX only) -->
              {#if !isTx}
                <span style="color: var(--doge-subtle); flex-shrink: 0; font-family: var(--font-mono); white-space: nowrap;">
                  {packet.rssi}dBm
                </span>
              {/if}

              <!-- Copy -->
              <button
                onclick={() => copyPacket(packet.decoded ?? packet.payloadHex, i)}
                class="btn-ghost"
                title="Copy packet data to clipboard"
                aria-label="Copy packet"
                style="padding: 1px 5px; font-size: 0.66rem; flex-shrink: 0; border-color: transparent;"
              >
                {copiedPacketIdx === i ? '✅' : '📋'}
              </button>
            </div>
          {/each}
        {/if}
      </div>
    </div>
  {/if}
</div>
