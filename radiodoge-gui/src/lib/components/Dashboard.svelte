<script lang="ts">
  /**
   * Dashboard tab — shows radio statistics and a live packet log.
   */

  import { connection } from '$lib/stores/connection.svelte';
  import { radio } from '$lib/stores/radio.svelte';
  import { formatTimestamp, commandName, rssiToBars } from '$lib/types';
  import SignalBars from './SignalBars.svelte';

  // Computed RSSI color
  const rssiColor = $derived(() => {
    const rssi = connection.stats?.rssi ?? -120;
    if (rssi >= -65) return 'var(--doge-neon)';
    if (rssi >= -80) return 'var(--doge-yellow)';
    if (rssi >= -95) return 'var(--doge-orange)';
    return 'var(--doge-red)';
  });

  // RSSI bar width (0-100%)
  const rssiPct = $derived(() => {
    const rssi = connection.stats?.rssi ?? -120;
    return Math.max(0, Math.min(100, ((rssi + 120) / 70) * 100));
  });
</script>

<div style="padding: 24px; max-width: 1000px; margin: 0 auto;">
  {#if !connection.isConnected}
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
          style="border-radius: 20px; display: block;"
        />
      </div>
      <p style="font-size: 1rem; margin-bottom: 6px;">Connect to a Heltec device first to see the dashboard.</p>
      <p style="font-size: 0.85rem; font-style: italic;">Such empty. Very disconnect. Wow.</p>
    </div>
  {:else}
    <!-- Stats Grid -->
    <div style="
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
      gap: 16px;
      margin-bottom: 24px;
    ">
      {#each [
        { label: 'Frequency', value: `${connection.stats?.frequencyMhz ?? 915.0} MHz`, icon: '📻' },
        { label: 'TX Power', value: `${connection.stats?.powerDbm ?? 5} dBm`, icon: '⚡' },
        { label: 'Spread Factor', value: `SF${connection.stats?.spreadingFactor ?? 7}`, icon: '📶' },
        { label: 'Bandwidth', value: `${connection.stats?.bandwidthKhz ?? 125} kHz`, icon: '〰️' },
        { label: 'Packets Sent', value: `${connection.stats?.packetsSent ?? 0}`, icon: '📤' },
        { label: "Packets Wow'd", value: `${connection.stats?.packetsReceived ?? 0}`, icon: '📥' },
      ] as stat}
        <div class="card-doge" style="text-align: center;">
          <div style="font-size: 1.5rem; margin-bottom: 8px;">{stat.icon}</div>
          <div class="stat-value" style="font-size: 1.5rem;">{stat.value}</div>
          <div class="stat-label" style="margin-top: 4px;">{stat.label}</div>
        </div>
      {/each}
    </div>

    <!-- Signal Strength Panel -->
    <div class="card-doge" style="margin-bottom: 24px;">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px;">
        <h3 style="margin: 0; font-size: 1rem; font-weight: 600;">
          📡 Signal Strength: <span style="font-style: italic; color: var(--doge-muted);">Much Radio</span>
        </h3>
        <SignalBars rssi={connection.stats?.rssi ?? -120} connected={true} size="md" />
      </div>

      <!-- RSSI meter -->
      <div style="margin-bottom: 12px;">
        <div style="display: flex; justify-content: space-between; margin-bottom: 6px;">
          <span class="stat-label">RSSI</span>
          <span style="font-family: var(--font-mono); font-size: 0.85rem; color: {rssiColor()};">
            {connection.stats?.rssi ?? '--'} dBm
          </span>
        </div>
        <div style="
          background: var(--doge-dark);
          border-radius: 4px;
          height: 8px;
          overflow: hidden;
          border: 1px solid var(--doge-border);
        ">
          <div style="
            height: 100%;
            width: {rssiPct()}%;
            background: linear-gradient(90deg, var(--doge-red), var(--doge-orange), var(--doge-yellow), var(--doge-neon));
            border-radius: 4px;
            transition: width 0.5s ease;
          "></div>
        </div>
        <div style="display: flex; justify-content: space-between; margin-top: 4px;">
          <span style="font-size: 0.65rem; color: var(--doge-subtle);">-120 dBm</span>
          <span style="font-size: 0.65rem; color: var(--doge-subtle);">-50 dBm</span>
        </div>
      </div>

      <!-- SNR -->
      <div style="display: flex; justify-content: space-between; align-items: center;">
        <span class="stat-label">SNR</span>
        <span style="font-family: var(--font-mono); font-size: 0.85rem; color: var(--doge-text);">
          {connection.stats?.snr?.toFixed(1) ?? '--'} dB
        </span>
      </div>
    </div>

    <!-- Live Packet Log -->
    <div class="card-doge">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 16px;">
        <h3 style="margin: 0; font-size: 1rem; font-weight: 600;">
          📋 Live Packet Log
        </h3>
        <span style="
          background: rgba(245, 197, 24, 0.1);
          color: var(--doge-yellow);
          border-radius: 12px;
          padding: 2px 10px;
          font-size: 0.75rem;
          font-family: var(--font-mono);
        ">
          {radio.packets.length} packets
        </span>
      </div>

      <div style="
        height: 300px;
        overflow-y: auto;
        display: flex;
        flex-direction: column;
        gap: 6px;
      ">
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
          {#each radio.packets as packet (packet.timestamp + packet.source.node)}
            <div class="slide-up" style="
              display: flex;
              align-items: flex-start;
              gap: 10px;
              padding: 8px 12px;
              background: var(--doge-dark);
              border-radius: 8px;
              border: 1px solid var(--doge-border);
              font-size: 0.78rem;
            ">
              <span style="color: var(--doge-subtle); font-family: var(--font-mono); flex-shrink: 0;">
                {formatTimestamp(packet.timestamp)}
              </span>
              <span style="
                background: rgba(245, 197, 24, 0.1);
                color: var(--doge-yellow);
                border-radius: 4px;
                padding: 1px 6px;
                flex-shrink: 0;
                font-family: var(--font-mono);
              ">
                {commandName(packet.command)}
              </span>
              <span style="color: var(--doge-muted); flex-shrink: 0;">
                {packet.source.region}.{packet.source.community}.{packet.source.node}
              </span>
              <span style="color: var(--doge-text); flex: 1; word-break: break-all;">
                {packet.decoded ?? packet.payloadHex}
              </span>
              <span style="color: var(--doge-subtle); flex-shrink: 0; font-family: var(--font-mono);">
                {packet.rssi}dBm
              </span>
            </div>
          {/each}
        {/if}
      </div>
    </div>
  {/if}
</div>
