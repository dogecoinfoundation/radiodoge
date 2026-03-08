<script lang="ts">
  /**
   * Debug Console — hidden-by-default raw serial traffic viewer.
   *
   * Toggle via:
   *   - Ctrl+Shift+D keyboard shortcut
   *   - 🐛 button in the status bar footer
   *
   * Features:
   *   - Real-time raw serial traffic (TX/RX) with timestamps
   *   - Raw hex bytes + parsed command labels
   *   - Auto-scroll (toggleable)
   *   - Clear buffer
   *   - Export to .txt
   *   - Copy individual lines
   *   - Professional dark terminal styling (Meshtastic-inspired)
   */

  import { listen } from '@tauri-apps/api/event';
  import { onMount } from 'svelte';

  // ── Props ────────────────────────────────────────────────────────────────
  interface Props {
    visible?: boolean;
  }
  let { visible = $bindable(false) }: Props = $props();

  // ── State ────────────────────────────────────────────────────────────────
  interface DebugEntry {
    timestamp: number;
    direction: 'TX' | 'RX';
    rawHex: string;
    parsed: string;
  }

  let entries = $state<DebugEntry[]>([]);
  let autoScroll = $state(true);
  let copiedIdx = $state<number | null>(null);
  let scrollContainer: HTMLDivElement | undefined = $state();

  const MAX_ENTRIES = 500;

  // ── Auto-scroll effect ──────────────────────────────────────────────────
  $effect(() => {
    if (autoScroll && scrollContainer && entries.length > 0) {
      // Trigger on entries.length change
      requestAnimationFrame(() => {
        scrollContainer?.scrollTo({ top: scrollContainer.scrollHeight, behavior: 'smooth' });
      });
    }
  });

  // ── Listen for debug traffic events ─────────────────────────────────────
  onMount(() => {
    const unlisten = listen<DebugEntry>('debug-serial-traffic', (event) => {
      entries = [...entries, event.payload].slice(-MAX_ENTRIES);
    });

    return () => { unlisten.then(fn => fn()); };
  });

  // ── Actions ─────────────────────────────────────────────────────────────
  function clearConsole() {
    entries = [];
  }

  function formatTime(ts: number): string {
    const d = new Date(ts);
    return d.toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })
      + '.' + String(d.getMilliseconds()).padStart(3, '0');
  }

  function entryToText(e: DebugEntry): string {
    return `[${formatTime(e.timestamp)}] ${e.direction} | ${e.rawHex || '(empty)'} | ${e.parsed}`;
  }

  async function copyLine(idx: number) {
    try {
      await navigator.clipboard.writeText(entryToText(entries[idx]));
      copiedIdx = idx;
      setTimeout(() => { copiedIdx = null; }, 1200);
    } catch { /* sandboxed — fail silently */ }
  }

  function exportToTxt() {
    const header = `RadioDoge Debug Console Export\nGenerated: ${new Date().toISOString()}\n${'─'.repeat(72)}\n\n`;
    const text = header + entries.map(entryToText).join('\n');
    const blob = new Blob([text], { type: 'text/plain' });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href     = url;
    a.download = `radiodoge-debug-${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.txt`;
    // Must be in the DOM for Tauri's WebView to honour the download attribute
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  /** Format hex into spaced byte groups for readability */
  function formatHex(hex: string): string {
    if (!hex) return '';
    return hex.match(/.{1,2}/g)?.join(' ') ?? hex;
  }
</script>

{#if visible}
  <div
    class="debug-console"
    style="
      position: fixed;
      bottom: 28px;
      left: 0;
      right: 0;
      height: 280px;
      background: #0c0c0c;
      border-top: 2px solid var(--doge-yellow);
      z-index: 8000;
      display: flex;
      flex-direction: column;
      font-family: var(--font-mono);
      font-size: 0.74rem;
      animation: slide-up 0.2s ease;
    "
    role="log"
    aria-label="Debug serial console"
  >
    <!-- Toolbar -->
    <div style="
      display: flex;
      align-items: center;
      gap: 10px;
      padding: 6px 12px;
      background: #111;
      border-bottom: 1px solid #2a2a2a;
      flex-shrink: 0;
    ">
      <span style="color: var(--doge-yellow); font-weight: 700; font-size: 0.78rem;">
        🐛 Debug Console
      </span>
      <span style="color: var(--doge-subtle); font-size: 0.68rem;">
        {entries.length} entries
      </span>

      <span style="flex: 1;"></span>

      <!-- Auto-scroll toggle -->
      <label
        style="display: flex; align-items: center; gap: 4px; color: var(--doge-muted); cursor: pointer; font-size: 0.7rem;"
        title="Auto-scroll to newest entry"
      >
        <input type="checkbox" bind:checked={autoScroll} style="accent-color: var(--doge-yellow);" />
        Auto-scroll
      </label>

      <!-- Export -->
      <button
        onclick={exportToTxt}
        class="btn-ghost"
        title="Export debug log to .txt file"
        style="padding: 2px 8px; font-size: 0.68rem; border-color: var(--doge-border);"
        disabled={entries.length === 0}
      >
        💾 Export
      </button>

      <!-- Clear -->
      <button
        onclick={clearConsole}
        class="btn-ghost"
        title="Clear all debug entries"
        style="padding: 2px 8px; font-size: 0.68rem; border-color: var(--doge-border);"
      >
        🗑️ Clear
      </button>

      <!-- Close -->
      <button
        onclick={() => visible = false}
        class="btn-ghost"
        title="Close debug console (Ctrl+Shift+D)"
        style="padding: 2px 8px; font-size: 0.72rem; border-color: var(--doge-border); color: var(--doge-red);"
      >
        ✕
      </button>
    </div>

    <!-- Entries -->
    <div
      bind:this={scrollContainer}
      style="
        flex: 1;
        overflow-y: auto;
        overflow-x: hidden;
        padding: 4px 0;
      "
    >
      {#if entries.length === 0}
        <div style="
          display: flex;
          align-items: center;
          justify-content: center;
          height: 100%;
          color: var(--doge-subtle);
          font-style: italic;
        ">
          🐕 Waiting for serial traffic... such debug. very console. wow.
        </div>
      {:else}
        {#each entries as entry, i (entry.timestamp + '_' + entry.direction + '_' + i)}
          {@const isTx = entry.direction === 'TX'}
          <div
            style="
              display: flex;
              align-items: flex-start;
              gap: 8px;
              padding: 3px 12px;
              border-bottom: 1px solid #1a1a1a;
              line-height: 1.5;
              transition: background 0.1s;
            "
            class:debug-line-hover={true}
          >
            <!-- Timestamp -->
            <span style="color: #666; flex-shrink: 0; min-width: 85px;">
              {formatTime(entry.timestamp)}
            </span>

            <!-- Direction badge -->
            <span style="
              flex-shrink: 0;
              min-width: 24px;
              font-weight: 700;
              color: {isTx ? 'var(--doge-neon)' : 'var(--doge-blue)'};
            ">
              {isTx ? '↑TX' : '↓RX'}
            </span>

            <!-- Raw hex -->
            <span
              style="color: #888; flex-shrink: 0; max-width: 320px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;"
              title={formatHex(entry.rawHex)}
            >
              {formatHex(entry.rawHex) || '—'}
            </span>

            <!-- Parsed label -->
            <span style="color: {isTx ? '#b3ffe0' : '#b3d4ff'}; flex: 1; word-break: break-all;">
              {entry.parsed}
            </span>

            <!-- Copy button -->
            <button
              onclick={() => copyLine(i)}
              class="btn-ghost"
              title="Copy this debug line"
              style="padding: 0 4px; font-size: 0.62rem; flex-shrink: 0; border-color: transparent; opacity: 0.5;"
            >
              {copiedIdx === i ? '✅' : '📋'}
            </button>
          </div>
        {/each}
      {/if}
    </div>
  </div>
{/if}

<style>
  .debug-line-hover:hover {
    background: rgba(245, 197, 24, 0.03);
  }
</style>
