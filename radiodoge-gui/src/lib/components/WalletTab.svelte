<script lang="ts">
  /**
   * Wallet tab — generate Dogecoin keypairs, display address, show QR code.
   */

  import { invoke } from '@tauri-apps/api/core';
  import { wallet, loadWallet, togglePrivateKeyVisibility } from '$lib/stores/wallet.svelte';
  import QRCode from './QRCode.svelte';
  import DogeSpinner from './DogeSpinner.svelte';

  let isGenerating = $state(false);
  let copyFeedback = $state<Record<string, boolean>>({});

  async function generateWallet() {
    isGenerating = true;
    wallet.error = null;
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
      // Fallback
    }
  }
</script>

<div style="padding: 24px; max-width: 800px; margin: 0 auto;">
  <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 24px;">
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

  {#if wallet.error}
    <div style="
      padding: 12px 16px;
      background: rgba(255, 68, 68, 0.1);
      border: 1px solid rgba(255, 68, 68, 0.3);
      border-radius: 8px;
      color: var(--doge-red);
      font-size: 0.85rem;
      margin-bottom: 20px;
    ">
      ❌ {wallet.error}
    </div>
  {/if}

  {#if !wallet.isGenerated}
    <!-- Empty state -->
    <div style="
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 60px 20px;
      text-align: center;
      color: var(--doge-muted);
    ">
      <div style="font-size: 4rem; margin-bottom: 16px;">👛</div>
      <h3 style="margin: 0 0 8px 0; color: var(--doge-text);">No wallet yet</h3>
      <p style="margin: 0 0 24px 0; font-size: 0.9rem;">
        Click "Generate Wallet" to create a fresh Dogecoin address.<br/>
        Much security. Very cryptography. Wow.
      </p>
      <button onclick={generateWallet} class="btn-doge" style="font-size: 1.1rem; padding: 14px 32px;">
        🎲 Generate Wallet
      </button>
    </div>
  {:else}
    <!-- Wallet content -->
    <div style="display: grid; grid-template-columns: 1fr auto; gap: 24px; align-items: start;">
      <!-- Left: Address + Keys -->
      <div style="display: flex; flex-direction: column; gap: 16px;">

        <!-- Dogecoin Address -->
        <div class="card-doge">
          <div class="stat-label" style="margin-bottom: 8px;">📍 Dogecoin Address</div>
          <div style="
            display: flex;
            align-items: center;
            gap: 8px;
            background: var(--doge-dark);
            border: 1px solid var(--doge-border);
            border-radius: 8px;
            padding: 10px 14px;
          ">
            <span class="mono" style="flex: 1; color: var(--doge-yellow); font-size: 0.85rem;" data-selectable>
              {wallet.address}
            </span>
            <button
              onclick={() => copyToClipboard(wallet.address, 'addr')}
              class="btn-ghost"
              style="padding: 4px 10px; font-size: 0.78rem; flex-shrink: 0;"
            >
              {copyFeedback.addr ? '✅ Copied!' : '📋 Copy'}
            </button>
          </div>
        </div>

        <!-- Public Key -->
        <div class="card-doge">
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
            <span class="mono" style="flex: 1; color: var(--doge-text); font-size: 0.75rem; word-break: break-all;" data-selectable>
              {wallet.publicKeyHex}
            </span>
            <button
              onclick={() => copyToClipboard(wallet.publicKeyHex, 'pubkey')}
              class="btn-ghost"
              style="padding: 4px 10px; font-size: 0.78rem; flex-shrink: 0;"
            >
              {copyFeedback.pubkey ? '✅' : '📋'}
            </button>
          </div>
        </div>

        <!-- Private Key (hidden by default) -->
        <div class="card-doge" style="border-color: rgba(255, 140, 0, 0.4);">
          <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;">
            <div class="stat-label">🔐 Private Key (WIF)</div>
            <button
              onclick={togglePrivateKeyVisibility}
              class="btn-ghost"
              style="padding: 3px 10px; font-size: 0.75rem;"
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
              <span class="mono" style="flex: 1; color: var(--doge-orange); font-size: 0.8rem; word-break: break-all;" data-selectable>
                {wallet.privateKeyWif}
              </span>
              <button
                onclick={() => copyToClipboard(wallet.privateKeyWif, 'privkey')}
                class="btn-ghost"
                style="padding: 4px 10px; font-size: 0.78rem; flex-shrink: 0; border-color: rgba(255, 140, 0, 0.4);"
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
            ">
              ⚠️ <strong>Much security!</strong> Never share your private key.
              Save it somewhere safe — if you lose it, you lose your DOGE. Very permanent. Wow.
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
            ">
              ••••••••••••••••••••••••••••••••••••••••••••••••••••
            </div>
          {/if}
        </div>
      </div>

      <!-- Right: QR Code -->
      <div style="display: flex; flex-direction: column; align-items: center; gap: 12px;">
        <QRCode value={wallet.address} size={200} />
        <p style="color: var(--doge-muted); font-size: 0.75rem; text-align: center; margin: 0; max-width: 200px;">
          Share this address to receive DOGE
        </p>
      </div>
    </div>

    <!-- Regenerate warning -->
    <div style="
      margin-top: 20px;
      padding: 12px 16px;
      background: rgba(245, 197, 24, 0.05);
      border: 1px solid rgba(245, 197, 24, 0.2);
      border-radius: 8px;
      font-size: 0.8rem;
      color: var(--doge-muted);
    ">
      💡 <strong style="color: var(--doge-yellow);">Tip:</strong> Each wallet is independent.
      Save your private key before generating a new one — keys are not stored by RadioDoge.
      Much wallet. Very self-custody. Wow.
    </div>
  {/if}
</div>
