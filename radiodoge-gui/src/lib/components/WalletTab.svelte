<script lang="ts">
  /**
   * Wallet tab — generate Dogecoin keypairs, display address, show QR code.
   *
   * v0.3.1 improvements:
   *   - Dedicated "Show Address QR" button (address QR is separated from key reveal)
   *   - Private key reveal is completely independent of QR display
   *   - Copy-to-clipboard on ALL fields (address, pubkey, privkey)
   *   - Rich tooltips on every action button
   *   - Accessible ARIA labels
   *   - Fun Doge tone throughout
   */

  import { invoke } from '@tauri-apps/api/core';
  import { wallet, loadWallet, togglePrivateKeyVisibility } from '$lib/stores/wallet.svelte';
  import QRCode from './QRCode.svelte';
  import DogeSpinner from './DogeSpinner.svelte';

  let isGenerating = $state(false);
  let copyFeedback = $state<Record<string, boolean>>({});

  /** Whether the Address QR modal/panel is visible. */
  let showQR = $state(false);

  async function generateWallet() {
    isGenerating = true;
    wallet.error = null;
    showQR = false; // close QR on new wallet
    try {
      const info = await invoke<{ address: string; publicKeyHex: string; privateKeyWif: string }>('generate_wallet');
      loadWallet(info);
    } catch (e: unknown) {
      wallet.error = e instanceof Error ? e.message : String(e);
    } finally {
      isGenerating = false;
    }
  }

  async function copyToClipboard(text: string, key: string) {
    try {
      await navigator.clipboard.writeText(text);
      copyFeedback[key] = true;
      setTimeout(() => { copyFeedback[key] = false; }, 2000);
    } catch {
      // Browser may block in some sandboxed envs
    }
  }
</script>

<div style="padding: 24px; max-width: 800px; margin: 0 auto;">

  <!-- ── Header ──────────────────────────────────────────────────────────── -->
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 24px; flex-wrap: wrap; gap: 12px;">
    <div>
      <h2 style="margin: 0 0 4px 0; font-size: 1.3rem; font-weight: 700;">👛 Dogecoin Wallet</h2>
      <p style="margin: 0; color: var(--doge-muted); font-size: 0.85rem;">
        Generate keys, share your address, receive DOGE
      </p>
    </div>

    <button
      onclick={generateWallet}
      disabled={isGenerating}
      class="btn-doge"
      title="Generate a new Dogecoin keypair using cryptographically secure randomness. Much entropy, very secp256k1."
      aria-label="{wallet.isGenerated ? 'Generate a new Dogecoin wallet (replaces current)' : 'Generate a new Dogecoin wallet'}"
      style="display: flex; align-items: center; gap: 8px;"
    >
      {#if isGenerating}
        <DogeSpinner size="sm" message="" />
        Generating...
      {:else}
        🎲 {wallet.isGenerated ? 'New Wallet' : 'Generate Wallet'}
      {/if}
    </button>
  </div>

  <!-- ── Error display ────────────────────────────────────────────────────── -->
  {#if wallet.error}
    <div style="
      padding: 12px 16px;
      background: rgba(255, 68, 68, 0.1);
      border: 1px solid rgba(255, 68, 68, 0.3);
      border-radius: 8px;
      color: var(--doge-red);
      font-size: 0.85rem;
      margin-bottom: 20px;
    " role="alert">
      ❌ {wallet.error}
    </div>
  {/if}

  <!-- ── Empty state ──────────────────────────────────────────────────────── -->
  {#if !wallet.isGenerated}
    <div style="
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 60px 20px;
      text-align: center;
      color: var(--doge-muted);
    ">
      <div style="font-size: 4rem; margin-bottom: 16px;" aria-hidden="true">👛</div>
      <h3 style="margin: 0 0 8px 0; color: var(--doge-text);">No wallet yet</h3>
      <p style="margin: 0 0 24px 0; font-size: 0.9rem;">
        Click "Generate Wallet" to create a fresh Dogecoin address.<br/>
        Much security. Very cryptography. Wow.
      </p>
      <button
        onclick={generateWallet}
        class="btn-doge"
        title="Generate a new Dogecoin keypair — pure Rust secp256k1, no network, no cloud. Very private. Much wow."
        style="font-size: 1.1rem; padding: 14px 32px;"
      >
        🎲 Generate Wallet
      </button>
    </div>

  {:else}
    <!-- ── Wallet content ────────────────────────────────────────────────── -->
    <div style="display: flex; flex-direction: column; gap: 16px;">

      <!-- Dogecoin Address -->
      <div class="card-doge" aria-label="Dogecoin address">
        <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; flex-wrap: wrap; gap: 8px;">
          <div class="stat-label">📍 Dogecoin Address</div>
          <!-- "Show Address QR" button — clearly separate from private key reveal -->
          <button
            onclick={() => showQR = !showQR}
            class="btn-ghost"
            title="Show/hide a scannable QR code for this Dogecoin address. Share with senders — safe to show publicly!"
            aria-expanded={showQR}
            aria-controls="address-qr-panel"
            style="padding: 4px 12px; font-size: 0.78rem; display: flex; align-items: center; gap: 6px;"
          >
            {showQR ? '🙈 Hide QR' : '📷 Show Address QR'}
          </button>
        </div>

        <div style="
          display: flex;
          align-items: center;
          gap: 8px;
          background: var(--doge-dark);
          border: 1px solid var(--doge-border);
          border-radius: 8px;
          padding: 10px 14px;
        ">
          <span
            class="mono"
            style="flex: 1; color: var(--doge-yellow); font-size: 0.85rem;"
            data-selectable
            aria-label="Dogecoin address: {wallet.address}"
          >
            {wallet.address}
          </span>
          <button
            onclick={() => copyToClipboard(wallet.address, 'addr')}
            class="btn-ghost"
            title="Copy Dogecoin address to clipboard. Share this freely — it's your public receive address."
            aria-label="Copy address to clipboard"
            style="padding: 4px 10px; font-size: 0.78rem; flex-shrink: 0;"
          >
            {copyFeedback.addr ? '✅ Copied!' : '📋 Copy'}
          </button>
        </div>

        <!-- QR Code panel — toggled by "Show Address QR" button -->
        {#if showQR}
          <div
            id="address-qr-panel"
            class="slide-up"
            style="
              margin-top: 16px;
              display: flex;
              flex-direction: column;
              align-items: center;
              gap: 10px;
              padding: 20px;
              background: var(--doge-dark);
              border: 1px solid rgba(245, 197, 24, 0.25);
              border-radius: 12px;
            "
          >
            <QRCode value={wallet.address} size={200} />
            <p style="color: var(--doge-muted); font-size: 0.78rem; text-align: center; margin: 0; max-width: 240px;">
              📷 Share this QR code to receive DOGE — it is your <strong style="color: var(--doge-yellow);">public address</strong> only.<br/>
              <span style="font-style: italic;">Much scannable. Very safe. Wow.</span>
            </p>
            <button
              onclick={() => copyToClipboard(wallet.address, 'addrQR')}
              class="btn-ghost"
              title="Copy address to clipboard"
              aria-label="Copy address from QR panel"
              style="font-size: 0.78rem; padding: 5px 16px;"
            >
              {copyFeedback.addrQR ? '✅ Copied!' : '📋 Copy Address'}
            </button>
          </div>
        {/if}
      </div>

      <!-- Public Key -->
      <div class="card-doge" aria-label="Compressed public key">
        <div class="stat-label" style="margin-bottom: 8px;">🔑 Public Key (Compressed)</div>
        <div style="
          display: flex;
          align-items: center;
          gap: 8px;
          background: var(--doge-dark);
          border: 1px solid var(--doge-border);
          border-radius: 8px;
          padding: 10px 14px;
        ">
          <span
            class="mono"
            style="flex: 1; color: var(--doge-text); font-size: 0.75rem; word-break: break-all;"
            data-selectable
          >
            {wallet.publicKeyHex}
          </span>
          <button
            onclick={() => copyToClipboard(wallet.publicKeyHex, 'pubkey')}
            class="btn-ghost"
            title="Copy the 33-byte compressed secp256k1 public key (66 hex chars). Safe to share — derived from private key but not reversible."
            aria-label="Copy public key to clipboard"
            style="padding: 4px 10px; font-size: 0.78rem; flex-shrink: 0;"
          >
            {copyFeedback.pubkey ? '✅' : '📋'}
          </button>
        </div>
      </div>

      <!-- Private Key (separate from QR, hidden by default) -->
      <div class="card-doge" style="border-color: rgba(255, 140, 0, 0.4);" aria-label="Private key (WIF format)">
        <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;">
          <div class="stat-label">🔐 Private Key (WIF)</div>
          <button
            onclick={togglePrivateKeyVisibility}
            class="btn-ghost"
            title="{wallet.privateKeyVisible
              ? 'Hide private key — very important, such security!'
              : 'Reveal private key — make sure nobody is looking over your shoulder! Much secret.'}"
            aria-label="{wallet.privateKeyVisible ? 'Hide private key' : 'Reveal private key'}"
            aria-pressed={wallet.privateKeyVisible}
            style="padding: 3px 10px; font-size: 0.75rem; border-color: rgba(255,140,0,0.4);"
          >
            {wallet.privateKeyVisible ? '🙈 Hide' : '👁️ Reveal'}
          </button>
        </div>

        {#if wallet.privateKeyVisible}
          <div style="
            display: flex;
            align-items: center;
            gap: 8px;
            background: rgba(255, 140, 0, 0.05);
            border: 1px solid rgba(255, 140, 0, 0.3);
            border-radius: 8px;
            padding: 10px 14px;
          ">
            <span
              class="mono"
              style="flex: 1; color: var(--doge-orange); font-size: 0.8rem; word-break: break-all;"
              data-selectable
              aria-label="Private key in WIF format"
            >
              {wallet.privateKeyWif}
            </span>
            <button
              onclick={() => copyToClipboard(wallet.privateKeyWif, 'privkey')}
              class="btn-ghost"
              title="Copy WIF private key to clipboard. ⚠️ NEVER share this with anyone! Very secret. Much danger."
              aria-label="Copy private key to clipboard"
              style="padding: 4px 10px; font-size: 0.78rem; flex-shrink: 0; border-color: rgba(255,140,0,0.4);"
            >
              {copyFeedback.privkey ? '✅' : '📋'}
            </button>
          </div>

          <!-- Security warning -->
          <div style="
            margin-top: 10px;
            padding: 10px 14px;
            background: rgba(255, 140, 0, 0.08);
            border-radius: 8px;
            font-size: 0.78rem;
            color: var(--doge-orange);
            line-height: 1.6;
          " role="alert">
            ⚠️ <strong>Much security!</strong> Never share your private key with anyone, ever.
            Save it somewhere safe offline — if you lose it, you lose access to your DOGE forever.
            Very permanent. Such responsibility. Wow.
          </div>
        {:else}
          <div style="
            padding: 10px 14px;
            background: var(--doge-dark);
            border: 1px solid var(--doge-border);
            border-radius: 8px;
            color: var(--doge-subtle);
            font-size: 0.85rem;
            letter-spacing: 0.3em;
            font-family: var(--font-mono);
            user-select: none;
          " aria-label="Private key hidden for security">
            ••••••••••••••••••••••••••••••••••••••••••••••••••••
          </div>
        {/if}
      </div>

      <!-- Tip / warning about regeneration -->
      <div style="
        padding: 12px 16px;
        background: rgba(245, 197, 24, 0.05);
        border: 1px solid rgba(245, 197, 24, 0.2);
        border-radius: 8px;
        font-size: 0.8rem;
        color: var(--doge-muted);
        line-height: 1.6;
      ">
        💡 <strong style="color: var(--doge-yellow);">Tip:</strong> Each wallet is independent.
        Save your private key (WIF) before generating a new one — RadioDoge never stores keys to disk.
        Much wallet. Very self-custody. Wow.
      </div>
    </div>
  {/if}
</div>
