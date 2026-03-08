<script lang="ts">
  /**
   * Animated signal strength bars — like a mobile signal indicator.
   *
   * Shows 1-5 bars based on RSSI value:
   *   5 bars: ≥ -50 dBm  (excellent)
   *   4 bars: ≥ -65 dBm  (good)
   *   3 bars: ≥ -80 dBm  (fair)
   *   2 bars: ≥ -95 dBm  (weak)
   *   1 bar:  < -95 dBm  (very weak)
   */

  interface Props {
    rssi?: number;       // dBm, e.g. -70
    connected?: boolean; // shows grey when not connected
    size?: 'sm' | 'md' | 'lg';
  }

  let { rssi = -120, connected = false, size = 'md' }: Props = $props();

  // Compute active bar count from RSSI
  const activeBars = $derived(() => {
    if (!connected) return 0;
    if (rssi >= -50) return 5;
    if (rssi >= -65) return 4;
    if (rssi >= -80) return 3;
    if (rssi >= -95) return 2;
    return 1;
  });

  // Color based on signal strength
  const barColor = $derived(() => {
    const bars = activeBars();
    if (bars >= 4) return 'var(--doge-neon)';
    if (bars === 3) return 'var(--doge-yellow)';
    if (bars === 2) return 'var(--doge-orange)';
    return 'var(--doge-red)';
  });

  const dims = $derived(() => {
    switch (size) {
      case 'sm': return { width: 4, gap: 2, heights: [4, 6, 8, 10, 12] };
      case 'lg': return { width: 8, gap: 4, heights: [10, 15, 20, 25, 30] };
      default:   return { width: 6, gap: 3, heights: [6, 9, 12, 15, 18] };
    }
  });
</script>

<div
  class="signal-bars {connected ? 'signal-bars-breathing' : ''}"
  title={connected ? `RSSI: ${rssi} dBm (${activeBars()} bars)` : 'Not connected'}
  style="display: flex; align-items: flex-end; gap: {dims().gap}px;"
>
  {#each dims().heights as height, i}
    {@const active = i < activeBars()}
    <div
      style="
        width: {dims().width}px;
        height: {height}px;
        border-radius: 2px;
        background: {active ? barColor() : 'var(--doge-subtle)'};
        opacity: {active ? 1 : 0.3};
        transition: all 0.4s ease;
        animation: {active && connected ? `bar-grow 0.3s ease ${i * 0.05}s both` : 'none'};
        transform-origin: bottom;
      "
    ></div>
  {/each}
</div>
