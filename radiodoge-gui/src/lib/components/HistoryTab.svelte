<script lang="ts">
  /**
   * History tab — show last 50 transaction history entries.
   * v0.3.6: Loaded from local JSON via Rust `get_history` command.
   * Much history. Very blockchain. Wow.
   */

  import { invoke } from '@tauri-apps/api/core';
  import { onMount } from 'svelte';
  import type { TxHistoryEntry } from '$lib/types';
  import { formatTimestamp } from '$lib/types';

  let history = $state<TxHistoryEntry[]>([]);
  let isLoading = $state(false);
  let loadError = $state<string | null>(null);

  async function loadHistory() {
    isLoading = true;
    loadError = null;
    try {
      history = await invoke<TxHistoryEntry[]>('get_history');
    } catch (e: unknown) {
      loadError = e instanceof Error ? e.message : String(e);
    } finally {
      isLoading = false;
    }
  }

  onMount(() => {
    loadHistory();
  });

  function statusColor(status: string): string {
    return status === 'sent' ? 'var(--doge-neon)' : 'var(--doge-red)';
  }

  function statusIcon(status: string): string {
    return status === 'sent' ? '✅' : '❌';
  }

  let copiedTx = $state<Record<number, boolean>>({});
  async function copyAddr(addr: string, idx: number) {
    try {
      await navigator.clipboard.writeText(addr);
      copiedTx[idx] = true;
      setTimeout(() => { copiedTx[idx] = false; }, 1500);
    } catch { /* sandboxed / permission denied — fail silently */ }
  }
</script>

<div style="padding: 24px; max-width: 860px; margin: 0 auto;">

  <!-- ── Header ──────────────────────────────────────────────────────────── -->
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 24px; flex-wrap: wrap; gap: 12px;">
    <div>
      <h2 style="margin: 0 0 4px 0; font-size: 1.3rem; font-weight: 700;">📜 Transaction History</h2>
      <p style="margin: 0; color: var(--doge-muted); font-size: 0.85rem;">
        Last {history.length} transactions sent from this device. Much records. Very immutable.
      </p>
    </div>
    <button
      onclick={loadHistory}
      disabled={isLoading}
      class="btn-ghost"
      title="Refresh transaction history from local storage"
      style="font-size: 0.8rem;"
    >
      {isLoading ? '⏳ Loading...' : '↺ Refresh'}
    </button>
  </div>

  <!-- ── Error ─────────────────────────────────────────────────────────────── -->
  {#if loadError}
    <div style="
      padding: 12px 16px;
      background: rgba(255, 68, 68, 0.1);
      border: 1px solid rgba(255, 68, 68, 0.3);
      border-radius: 8px;
      color: var(--doge-red);
      font-size: 0.85rem;
      margin-bottom: 20px;
    " role="alert">
      ❌ {loadError}
    </div>
  {/if}

  <!-- ── Empty state ───────────────────────────────────────────────────────── -->
  {#if !isLoading && history.length === 0}
    <div style="
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 60px 20px;
      text-align: center;
      color: var(--doge-muted);
    ">
      <div style="font-size: 4rem; margin-bottom: 16px;" aria-hidden="true">📭</div>
      <h3 style="margin: 0 0 8px 0; color: var(--doge-text);">No transactions yet</h3>
      <p style="margin: 0; font-size: 0.9rem;">
        Send your first Dogecoin over LoRa from the Send tab!<br/>
        Much potential. Very future. Wow.
      </p>
    </div>

  {:else}
    <!-- ── Transaction list ──────────────────────────────────────────────── -->
    <div style="display: flex; flex-direction: column; gap: 10px;">
      {#each history as entry, i}
        <div
          class="card-doge"
          style="padding: 14px 16px;"
          aria-label="Transaction to {entry.toAddress}"
        >
          <div style="display: flex; align-items: flex-start; justify-content: space-between; gap: 12px; flex-wrap: wrap;">

            <!-- Left: status + amount -->
            <div style="display: flex; align-items: center; gap: 10px; flex-shrink: 0;">
              <span style="font-size: 1.1rem;" title="Status: {entry.status}">{statusIcon(entry.status)}</span>
              <div>
                <div style="font-family: var(--font-mono); font-weight: 700; font-size: 1rem; color: var(--doge-yellow);">
                  {entry.amountDoge.toFixed(8)} DOGE
                </div>
                <div style="font-size: 0.72rem; color: {statusColor(entry.status)}; font-weight: 600; text-transform: uppercase;">
                  {entry.status}
                </div>
              </div>
            </div>

            <!-- Right: timestamp -->
            <div style="font-size: 0.72rem; color: var(--doge-subtle); text-align: right; flex-shrink: 0;">
              {formatTimestamp(entry.timestamp)}
            </div>
          </div>

          <!-- Recipient address -->
          <div style="
            display: flex;
            align-items: center;
            gap: 6px;
            margin-top: 10px;
            background: var(--doge-dark);
            border: 1px solid var(--doge-border);
            border-radius: 6px;
            padding: 6px 10px;
          ">
            <span style="font-size: 0.7rem; color: var(--doge-muted); flex-shrink: 0;">→</span>
            <span
              class="mono"
              style="flex: 1; font-size: 0.75rem; color: var(--doge-text); overflow: hidden; text-overflow: ellipsis; white-space: nowrap;"
              title={entry.toAddress}
            >
              {entry.toAddress}
            </span>
            <button
              onclick={() => copyAddr(entry.toAddress, i)}
              class="btn-ghost"
              title="Copy recipient address"
              style="padding: 2px 8px; font-size: 0.7rem; flex-shrink: 0;"
            >
              {copiedTx[i] ? '✅' : '📋'}
            </button>
          </div>

          <!-- Memo (if any) -->
          {#if entry.memo}
            <div style="
              margin-top: 6px;
              font-size: 0.75rem;
              color: var(--doge-muted);
              font-style: italic;
              padding-left: 4px;
            ">
              💬 {entry.memo}
            </div>
          {/if}
        </div>
      {/each}
    </div>

    <p style="text-align: center; color: var(--doge-subtle); font-size: 0.75rem; margin-top: 16px;">
      Showing {history.length} most recent transactions · Stored locally · Much transparent. Such doge.
    </p>
  {/if}
</div>
