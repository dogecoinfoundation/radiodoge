<script lang="ts">
  /**
   * Send tab — craft and broadcast a Dogecoin transaction over LoRa.
   */

  import { invoke } from '@tauri-apps/api/core';
  import { onMount } from 'svelte';
  import { connection } from '$lib/stores/connection.svelte';
  import { wallet } from '$lib/stores/wallet.svelte';
  import DogeSpinner from './DogeSpinner.svelte';
  interface Props {
    onTriggerConfetti: () => void;
  }
  let { onTriggerConfetti }: Props = $props();

  let toAddress = $state('');
  let amountDoge = $state('');
  let memo = $state('');
  let isSending = $state(false);
  let success = $state<string | null>(null);
  let error = $state<string | null>(null);

  // Timer used to clear the form + success message 5s after a successful send.
  // Tracked so it can be cancelled if the user starts a new transaction before
  // the 5s window elapses (otherwise their freshly-typed inputs would be wiped).
  let _successClearTimer: ReturnType<typeof setTimeout> | null = null;
  onMount(() => () => { if (_successClearTimer) clearTimeout(_successClearTimer); });

  // Validation
  const isValidAddress = $derived(() => {
    return toAddress.length > 25 && toAddress.startsWith('D');
  });

  const isValidAmount = $derived(() => {
    const n = parseFloat(amountDoge);
    return !isNaN(n) && n > 0;
  });

  /** True only when ALL conditions are met — including a loaded wallet. */
  const canSend = $derived(() =>
    connection.isConnected &&
    wallet.isGenerated &&
    isValidAddress() &&
    isValidAmount() &&
    !isSending
  );

  async function sendTransaction() {
    // Cancel any pending form-clear timer so a new transaction doesn't wipe
    // inputs the user has already started filling in.
    if (_successClearTimer !== null) {
      clearTimeout(_successClearTimer);
      _successClearTimer = null;
    }

    // Hard gate: never invoke the backend without a "From" wallet.
    // The button is already disabled, but guard here too in case of keyboard/script triggers.
    if (!connection.isConnected) {
      error = '🔌 Not connected — plug in your Heltec device first.';
      return;
    }
    if (!wallet.isGenerated) {
      error = '👛 No sender wallet loaded — go to the Wallet tab to generate or import one first.';
      return;
    }
    if (!canSend()) return;

    isSending = true;
    success = null;
    error = null;

    try {
      const result = await invoke<string>('send_transaction', {
        tx: {
          toAddress: toAddress.trim(),
          amountDoge: parseFloat(amountDoge),
          memo: memo.trim() || undefined,
          fromPrivateKeyWif: wallet.privateKeyWif || undefined,
        }
      });

      success = result;
      onTriggerConfetti(); // 🎉 BOOM — confetti!

      // Clear form 5s after success — gives user time to screenshot/note the result.
      // The timer ID is stored so it can be cancelled if the user starts a new
      // transaction during the 5s window (preventing their new inputs from being wiped).
      _successClearTimer = setTimeout(() => {
        toAddress = '';
        amountDoge = '';
        memo = '';
        success = null;
        _successClearTimer = null;
      }, 5000);

    } catch (e: unknown) {
      error = e instanceof Error ? e.message : String(e);
    } finally {
      isSending = false;
    }
  }

  async function pasteAddress() {
    try {
      toAddress = await navigator.clipboard.readText();
    } catch {
      // No clipboard permission
    }
  }
</script>

<div style="padding: 24px; max-width: 600px; margin: 0 auto;">
  <div style="margin-bottom: 24px;">
    <h2 style="margin: 0 0 4px 0; font-size: 1.3rem; font-weight: 700;">📤 Send Dogecoin</h2>
    <p style="margin: 0; color: var(--doge-muted); font-size: 0.85rem;">
      Broadcast via LoRa radio — no internet required 🚀
    </p>
  </div>

  <!-- Not connected warning -->
  {#if !connection.isConnected}
    <div style="
      padding: 16px;
      background: rgba(255, 68, 68, 0.08);
      border: 1px solid rgba(255, 68, 68, 0.25);
      border-radius: 12px;
      color: var(--doge-red);
      font-size: 0.9rem;
      margin-bottom: 20px;
      text-align: center;
    ">
      🔌 Connect to a Heltec device first to send transactions.
    </div>
  {/if}

  <!-- Send form -->
  <div class="card-doge" style="display: flex; flex-direction: column; gap: 20px;">
    <!-- Recipient address -->
    <div>
      <label for="send-to-address" style="display: block; font-size: 0.8rem; color: var(--doge-muted); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px;">
        Recipient Address
      </label>
      <div style="display: flex; gap: 8px;">
        <input
          id="send-to-address"
          bind:value={toAddress}
          placeholder="DH5yaieqoZN36fDVciNyRueRGvGLR3mr7L"
          class="input-doge"
          style="flex: 1; border-color: {toAddress && !isValidAddress() ? 'var(--doge-red)' : 'var(--doge-border)'};"
        />
        <button onclick={pasteAddress} class="btn-ghost" style="padding: 10px 14px; flex-shrink: 0;">
          📋 Paste
        </button>
      </div>
      {#if toAddress && !isValidAddress()}
        <p style="color: var(--doge-red); font-size: 0.75rem; margin: 6px 0 0 0;">
          ⚠️ Dogecoin addresses start with "D" and are 25-34 characters
        </p>
      {:else if isValidAddress()}
        <p style="color: var(--doge-neon); font-size: 0.75rem; margin: 6px 0 0 0;">
          ✅ Valid Dogecoin address
        </p>
      {/if}
    </div>

    <!-- Amount -->
    <div>
      <label for="send-amount" style="display: block; font-size: 0.8rem; color: var(--doge-muted); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px;">
        Amount (DOGE)
      </label>
      <div style="position: relative;">
        <input
          id="send-amount"
          bind:value={amountDoge}
          type="number"
          min="0"
          step="0.00000001"
          placeholder="100.00"
          class="input-doge"
          style="padding-right: 70px;"
        />
        <span style="
          position: absolute;
          right: 14px;
          top: 50%;
          transform: translateY(-50%);
          color: var(--doge-yellow);
          font-weight: 700;
          font-size: 0.85rem;
        ">
          DOGE 🐕
        </span>
      </div>
      {#if amountDoge && !isValidAmount()}
        <p style="color: var(--doge-red); font-size: 0.75rem; margin: 6px 0 0 0;">
          ⚠️ Amount must be greater than 0
        </p>
      {/if}
    </div>

    <!-- Memo (optional) -->
    <div>
      <label for="send-memo" style="display: block; font-size: 0.8rem; color: var(--doge-muted); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px;">
        Memo <span style="color: var(--doge-subtle); text-transform: none; font-size: 0.75rem;">(optional)</span>
      </label>
      <input
        id="send-memo"
        bind:value={memo}
        placeholder="such payment, very thanks, wow"
        class="input-doge"
        maxlength="100"
      />
    </div>

    <!-- From wallet indicator — required; blocks send if missing -->
    {#if wallet.isGenerated}
      <div
        title="This address will sign the transaction"
        style="
          display: flex;
          align-items: center;
          gap: 10px;
          padding: 10px 14px;
          background: rgba(245, 197, 24, 0.05);
          border: 1px solid rgba(245, 197, 24, 0.2);
          border-radius: 8px;
          font-size: 0.8rem;
        "
      >
        <span style="color: var(--doge-muted); flex-shrink: 0;">From:</span>
        <span class="mono" style="color: var(--doge-yellow); flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">
          {wallet.address}
        </span>
        <span style="color: var(--doge-neon); font-size: 0.7rem; flex-shrink: 0;" title="Wallet loaded ✅">✅</span>
      </div>
    {:else}
      <div style="
        display: flex;
        align-items: center;
        gap: 10px;
        padding: 10px 14px;
        background: rgba(255, 68, 68, 0.06);
        border: 1px solid rgba(255, 68, 68, 0.25);
        border-radius: 8px;
        font-size: 0.8rem;
        color: var(--doge-red);
      ">
        <span style="flex-shrink: 0;">❌</span>
        <span style="flex: 1;">
          No sender wallet — <strong>required to sign transactions</strong>.
          Go to the <em>Wallet</em> tab to generate or import one.
        </span>
      </div>
    {/if}

    <!-- Fee summary (shown when wallet is loaded and amount is valid) -->
    {#if wallet.isGenerated && isValidAmount()}
      <div style="
        display: flex; align-items: center; justify-content: space-between;
        padding: 8px 14px;
        background: rgba(245, 197, 24, 0.04);
        border: 1px solid rgba(245, 197, 24, 0.15);
        border-radius: 8px;
        font-size: 0.78rem;
        color: var(--doge-muted);
      ">
        <span>Network fee</span>
        <span style="font-family: var(--font-mono); color: var(--doge-text);">1.00000000 DOGE (fixed)</span>
      </div>
    {/if}

    <!-- Send button -->
    <button
      onclick={sendTransaction}
      disabled={!canSend()}
      class="btn-doge"
      title={
        !connection.isConnected ? 'Connect to a Heltec device first' :
        !wallet.isGenerated    ? 'Generate or import a wallet in the Wallet tab first' :
        !isValidAddress()      ? 'Enter a valid Dogecoin recipient address' :
        !isValidAmount()       ? 'Enter an amount greater than 0' :
        'Sign & broadcast this transaction over LoRa 🚀'
      }
      style="
        width: 100%;
        padding: 16px;
        font-size: 1.1rem;
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 10px;
        {isSending ? 'animation: pulse-glow 1s ease-in-out infinite;' : ''}
      "
    >
      {#if isSending}
        <DogeSpinner size="sm" message="" />
        Beaming into the radio ether... 📡
      {:else}
        🚀 Sign & Broadcast via Radio
      {/if}
    </button>
  </div>

  <!-- Success message -->
  {#if success}
    <div
      class="slide-up"
      style="
        margin-top: 16px;
        padding: 16px 20px;
        background: rgba(0, 255, 136, 0.1);
        border: 1px solid rgba(0, 255, 136, 0.4);
        border-radius: 12px;
        text-align: center;
      "
    >
      <div style="font-size: 2.5rem; margin-bottom: 8px;">🎉</div>
      <div style="color: var(--doge-neon); font-weight: 700; font-size: 1rem; margin-bottom: 4px;">
        Transaction Sent! Much broadcast. Wow!
      </div>
      <div style="color: var(--doge-muted); font-size: 0.82rem;">
        {success}
      </div>
    </div>
  {/if}

  <!-- Error message -->
  {#if error}
    <div
      class="slide-up"
      style="
        margin-top: 16px;
        padding: 16px 20px;
        background: rgba(255, 68, 68, 0.1);
        border: 1px solid rgba(255, 68, 68, 0.3);
        border-radius: 12px;
      "
    >
      <div style="font-size: 1.5rem; margin-bottom: 8px;">😢</div>
      <div style="color: var(--doge-red); font-weight: 700; font-size: 0.9rem; margin-bottom: 4px;">
        Sad! Send failed.
      </div>
      <div style="color: var(--doge-muted); font-size: 0.82rem; font-family: var(--font-mono);">
        {error}
      </div>
    </div>
  {/if}

  <!-- Info footer -->
  <div style="
    margin-top: 24px;
    padding: 12px 16px;
    background: var(--doge-card);
    border-radius: 8px;
    font-size: 0.78rem;
    color: var(--doge-subtle);
    line-height: 1.6;
  ">
    📡 <strong>How it works:</strong> Your transaction is packaged into a RadioDoge LoRa packet
    and sent to the Heltec device via serial. The device broadcasts it over the LoRa mesh network.
    Any node within radio range can relay it to a gateway connected to the Dogecoin network.
    Very decentralize. Much wow.
  </div>
</div>
