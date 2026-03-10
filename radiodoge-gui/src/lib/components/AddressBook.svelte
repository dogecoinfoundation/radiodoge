<script lang="ts">
  /**
   * AddressBook tab — save and manage labelled Dogecoin addresses locally.
   * v0.3.8: Entries persisted via Tauri to address_book.json in app data dir.
   */
  import { invoke } from '@tauri-apps/api/core';
  import type { AddressBookEntry } from '$lib/types';

  let entries = $state<AddressBookEntry[]>([]);
  let isLoading = $state(true);

  // ── Add entry form ─────────────────────────────────────────────────────────
  let showForm = $state(false);
  let newLabel = $state('');
  let newAddress = $state('');
  let newNotes = $state('');
  let formError = $state<string | null>(null);
  let isSaving = $state(false);

  let copiedId = $state<string | null>(null);

  async function loadBook() {
    isLoading = true;
    try {
      const raw = await invoke<{ id: string; label: string; address: string; createdAt: number; notes?: string }[]>('load_address_book');
      entries = raw as AddressBookEntry[];
    } catch { entries = []; } finally { isLoading = false; }
  }

  async function persist() {
    await invoke('save_address_book', { entries }).catch(() => {});
  }

  async function addEntry() {
    const addr = newAddress.trim();
    const label = newLabel.trim();
    if (!label) { formError = 'Label is required.'; return; }
    if (!addr.startsWith('D') || addr.length < 25 || addr.length > 35) {
      formError = 'Invalid Dogecoin address (must start with "D").';
      return;
    }
    isSaving = true;
    formError = null;
    const entry: AddressBookEntry = {
      id: String(Date.now()),
      label,
      address: addr,
      createdAt: Math.floor(Date.now() / 1000),
      notes: newNotes.trim() || undefined,
    };
    entries = [entry, ...entries];
    await persist();
    newLabel = ''; newAddress = ''; newNotes = '';
    showForm = false;
    isSaving = false;
  }

  async function removeEntry(id: string) {
    entries = entries.filter(e => e.id !== id);
    await persist();
  }

  async function copyAddress(address: string, id: string) {
    try {
      await navigator.clipboard.writeText(address);
      copiedId = id;
      setTimeout(() => { copiedId = null; }, 2000);
    } catch {}
  }

  $effect(() => { loadBook(); });
</script>

<div style="padding: 0 0 24px 0;">
  <!-- ── Header ──────────────────────────────────────────────────────────── -->
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 18px; flex-wrap: wrap; gap: 10px;">
    <div>
      <h2 style="margin: 0; font-size: 1.1rem; font-weight: 700;">📒 Address Book</h2>
      <p style="margin: 4px 0 0 0; font-size: 0.75rem; color: var(--doge-muted);">
        Save labelled Dogecoin addresses · stored locally
      </p>
    </div>
    <button
      onclick={() => { showForm = !showForm; formError = null; }}
      style="padding: 7px 16px; border: 1px solid var(--doge-yellow); background: rgba(245,197,24,0.08); color: var(--doge-yellow); border-radius: 8px; cursor: pointer; font-size: 0.82rem;"
    >
      {showForm ? '✕ Cancel' : '+ Add Address'}
    </button>
  </div>

  <!-- ── Add form ───────────────────────────────────────────────────────── -->
  {#if showForm}
    <div class="card-doge" style="margin-bottom: 18px;">
      <h3 style="margin: 0 0 14px 0; font-size: 0.95rem; font-weight: 700; color: var(--doge-yellow);">Add New Address</h3>
      <div style="display: flex; flex-direction: column; gap: 10px;">
        <input
          type="text"
          placeholder="Label (e.g. 'My main wallet')"
          bind:value={newLabel}
          style="padding: 9px 12px; background: var(--doge-dark); border: 1px solid var(--doge-border); border-radius: 8px; color: var(--doge-text); font-size: 0.88rem; outline: none;"
        />
        <input
          type="text"
          placeholder="Dogecoin address (starts with D)"
          bind:value={newAddress}
          style="padding: 9px 12px; background: var(--doge-dark); border: 1px solid var(--doge-border); border-radius: 8px; color: var(--doge-text); font-size: 0.88rem; font-family: var(--font-mono); outline: none;"
        />
        <input
          type="text"
          placeholder="Notes (optional)"
          bind:value={newNotes}
          style="padding: 9px 12px; background: var(--doge-dark); border: 1px solid var(--doge-border); border-radius: 8px; color: var(--doge-text); font-size: 0.88rem; outline: none;"
        />
        {#if formError}
          <p style="margin: 0; font-size: 0.78rem; color: #ff6060;">{formError}</p>
        {/if}
        <button
          onclick={addEntry}
          disabled={isSaving}
          style="align-self: flex-start; padding: 8px 18px; border: 1px solid var(--doge-yellow); background: rgba(245,197,24,0.1); color: var(--doge-yellow); border-radius: 8px; cursor: pointer; font-size: 0.85rem;"
        >
          {isSaving ? '⏳ Saving…' : '💾 Save'}
        </button>
      </div>
    </div>
  {/if}

  <!-- ── Entries ────────────────────────────────────────────────────────── -->
  {#if isLoading}
    <div class="card-doge" style="text-align: center; padding: 28px; color: var(--doge-muted);">⏳ Loading…</div>
  {:else if entries.length === 0}
    <div class="card-doge" style="text-align: center; padding: 32px; color: var(--doge-muted);">
      <p style="font-size: 1.2rem; margin: 0 0 8px 0;">📭</p>
      <p style="margin: 0; font-size: 0.9rem;">No saved addresses yet.</p>
      <p style="margin: 6px 0 0 0; font-size: 0.75rem; opacity: 0.7;">Click "+ Add Address" to save a Dogecoin address with a label.</p>
    </div>
  {:else}
    <div class="card-doge" style="padding: 0; overflow: hidden;">
      <table style="width: 100%; border-collapse: collapse;">
        <thead>
          <tr style="border-bottom: 1px solid var(--doge-border); background: rgba(255,255,255,0.02);">
            <th style="padding: 10px 14px; text-align: left; font-size: 0.75rem; color: var(--doge-muted); font-weight: 600;">Label</th>
            <th style="padding: 10px 14px; text-align: left; font-size: 0.75rem; color: var(--doge-muted); font-weight: 600;">Address</th>
            <th style="padding: 10px 14px; text-align: left; font-size: 0.75rem; color: var(--doge-muted); font-weight: 600;">Notes</th>
            <th style="padding: 10px 14px; text-align: left; font-size: 0.75rem; color: var(--doge-muted); font-weight: 600;"></th>
          </tr>
        </thead>
        <tbody>
          {#each entries as entry (entry.id)}
            <tr style="border-bottom: 1px solid rgba(255,255,255,0.04);">
              <td style="padding: 10px 14px; font-size: 0.88rem; font-weight: 600; color: var(--doge-text);">{entry.label}</td>
              <td style="padding: 10px 14px;">
                <div style="display: flex; align-items: center; gap: 6px;">
                  <span style="font-family: var(--font-mono); font-size: 0.8rem; color: var(--doge-yellow);">
                    {entry.address.slice(0,10)}…{entry.address.slice(-6)}
                  </span>
                  <button
                    onclick={() => copyAddress(entry.address, entry.id)}
                    style="padding: 2px 7px; border: 1px solid var(--doge-border); background: transparent; color: var(--doge-muted); border-radius: 4px; cursor: pointer; font-size: 0.7rem;"
                    title="Copy full address"
                  >
                    {copiedId === entry.id ? '✅' : '📋'}
                  </button>
                </div>
              </td>
              <td style="padding: 10px 14px; font-size: 0.78rem; color: var(--doge-muted);">{entry.notes ?? '—'}</td>
              <td style="padding: 10px 14px;">
                <button
                  onclick={() => removeEntry(entry.id)}
                  style="padding: 4px 9px; border: 1px solid rgba(255,60,60,0.3); background: transparent; color: #ff8080; border-radius: 5px; cursor: pointer; font-size: 0.72rem;"
                  title="Remove entry"
                >
                  🗑️
                </button>
              </td>
            </tr>
          {/each}
        </tbody>
      </table>
    </div>
  {/if}
</div>
