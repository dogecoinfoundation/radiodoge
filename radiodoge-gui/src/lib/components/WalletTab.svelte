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
  import { listen } from '@tauri-apps/api/event';
  import { onMount, onDestroy } from 'svelte';
  import { wallet, loadWallet, togglePrivateKeyVisibility } from '$lib/stores/wallet.svelte';
  import QRCode from './QRCode.svelte';
  import DogeSpinner from './DogeSpinner.svelte';

  // Register the gateway-balance listener unconditionally so any wallet (saved or freshly
  // generated) receives BAL responses from the LoRa gateway.  Uses the Promise-capture
  // pattern so cleanup always cancels the listener regardless of mount timing.
  const _balanceListenPromise = listen<{ decoded?: string }>('radio-packet', (ev) => {
    const decoded = ev.payload?.decoded ?? '';
    const match = decoded.match(/^💰 Balance: ([\d.]+) DOGE$/);
    if (match) {
      const amount = parseFloat(match[1]);
      if (isFinite(amount)) {
        balance = amount;
        balanceSource = 'gateway';
        balanceError = null;
      }
    }
  });
  onDestroy(() => { _balanceListenPromise.then(fn => fn()); });

  let isGenerating = $state(false);
  let copyFeedback = $state<Record<string, boolean>>({});

  /** Whether the Address QR modal/panel is visible. */
  let showQR = $state(false);

  // ── v0.3.6 — WIF Import ────────────────────────────────────────────────
  let showImport = $state(false);
  let importWif = $state('');
  let isImporting = $state(false);
  let importError = $state<string | null>(null);

  // ── v0.3.16 — Mnemonic import ──────────────────────────────────────────
  let showMnemonicImport = $state(false);
  let importMnemonicPhrase = $state('');
  let isImportingMnemonic = $state(false);
  let importMnemonicError = $state<string | null>(null);

  // ── v0.3.16 — Mnemonic backup modal (shown after generating a new wallet)
  let showMnemonicBackup = $state(false);
  let generatedMnemonic = $state<string | null>(null);

  async function importWallet() {
    if (!importWif.trim()) {
      importError = 'Enter a WIF private key (starts with "Q" for Dogecoin mainnet).';
      return;
    }
    isImporting = true;
    importError = null;
    wallet.error = null;
    showQR = false;
    try {
      const info = await invoke<{ address: string; publicKeyHex: string; privateKeyWif: string }>('import_wif', { wif: importWif.trim() });
      loadWallet(info);
      importWif = '';
      showImport = false;
    } catch (e: unknown) {
      importError = e instanceof Error ? e.message : String(e);
    } finally {
      isImporting = false;
    }
  }

  async function importMnemonicWallet() {
    if (!importMnemonicPhrase.trim()) {
      importMnemonicError = 'Enter your 12 or 24 word recovery phrase.';
      return;
    }
    isImportingMnemonic = true;
    importMnemonicError = null;
    wallet.error = null;
    showQR = false;
    try {
      const info = await invoke<{ address: string; publicKeyHex: string; privateKeyWif: string }>(
        'import_mnemonic_wallet', { phrase: importMnemonicPhrase.trim() }
      );
      loadWallet(info);
      importMnemonicPhrase = '';
      showMnemonicImport = false;
    } catch (e: unknown) {
      importMnemonicError = e instanceof Error ? e.message : String(e);
    } finally {
      isImportingMnemonic = false;
    }
  }

  async function generateWallet() {
    isGenerating = true;
    wallet.error = null;
    showQR = false;
    try {
      const result = await invoke<{ mnemonic: string; address: string; publicKeyHex: string; privateKeyWif: string }>(
        'generate_mnemonic_wallet'
      );
      loadWallet({ address: result.address, publicKeyHex: result.publicKeyHex, privateKeyWif: result.privateKeyWif });
      generatedMnemonic = result.mnemonic;
      showMnemonicBackup = true;
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

  // ── v0.3.16 — Balance query ────────────────────────────────────────────────
  let balance = $state<number | null>(null);
  let balanceSource = $state<'blockbook' | 'gateway' | null>(null);
  let isFetchingBalance = $state(false);
  let balanceError = $state<string | null>(null);

  async function checkBalance() {
    if (!wallet.address) return;
    isFetchingBalance = true;
    balanceError = null;
    try {
      balance = await invoke<number>('get_balance', { address: wallet.address });
      balanceSource = 'blockbook';
    } catch (e: unknown) {
      balanceError = e instanceof Error ? e.message : String(e);
    } finally {
      isFetchingBalance = false;
    }
  }



  // ── v0.3.16 — Encrypted persistent wallet ────────────────────────────────
  let showSaveModal = $state(false);
  let savePassphrase = $state('');
  let savePassphraseConfirm = $state('');
  let isSavingWallet = $state(false);
  let saveWalletError = $state<string | null>(null);
  let walletSaved = $state(false);
  // Passphrase unlock modal (shown on startup when an encrypted wallet exists)
  let showUnlockModal = $state(false);
  let unlockPassphrase = $state('');
  let isUnlocking = $state(false);
  let unlockError = $state<string | null>(null);
  // True if a legacy plaintext wallet was loaded — nudge user to re-encrypt
  let walletIsLegacy = $state(false);

  async function openSaveModal() {
    savePassphrase = '';
    savePassphraseConfirm = '';
    saveWalletError = null;
    showSaveModal = true;
  }

  async function confirmSaveWallet() {
    if (savePassphrase.length < 8) {
      saveWalletError = 'Passphrase must be at least 8 characters.';
      return;
    }
    if (savePassphrase !== savePassphraseConfirm) {
      saveWalletError = 'Passphrases do not match.';
      return;
    }
    if (!wallet.isGenerated) return;
    isSavingWallet = true;
    saveWalletError = null;
    try {
      await invoke('save_wallet', {
        walletInfo: {
          address: wallet.address,
          publicKeyHex: wallet.publicKeyHex,
          privateKeyWif: wallet.privateKeyWif,
        },
        passphrase: savePassphrase,
      });
      walletSaved = true;
      walletIsLegacy = false;
      showSaveModal = false;
    } catch (e: unknown) {
      saveWalletError = e instanceof Error ? e.message : String(e);
    } finally {
      isSavingWallet = false;
    }
  }

  async function deleteSavedWallet() {
    await invoke('delete_saved_wallet').catch(() => {});
    walletSaved = false;
    walletIsLegacy = false;
  }

  async function unlockWallet() {
    if (!unlockPassphrase) {
      unlockError = 'Enter your passphrase.';
      return;
    }
    isUnlocking = true;
    unlockError = null;
    try {
      const w = await invoke<{ address: string; publicKeyHex: string; privateKeyWif: string } | null>(
        'load_saved_wallet', { passphrase: unlockPassphrase }
      );
      if (w) {
        loadWallet(w);
        walletSaved = true;
        showUnlockModal = false;
        unlockPassphrase = '';
      }
    } catch (e: unknown) {
      unlockError = e instanceof Error ? e.message : String(e);
    } finally {
      isUnlocking = false;
    }
  }

  // On mount: check for saved wallet, prompt for passphrase if encrypted.
  // Must use onMount (not $effect) — $effect re-runs each time the component mounts,
  // which happens on every tab switch ({#if activeTab === 'wallet'} unmounts on exit).
  onMount(async () => {
    const hasWallet = await invoke<boolean>('wallet_needs_passphrase').catch(() => false);
    if (!hasWallet) return;
    // Try loading without passphrase — succeeds for legacy plaintext wallets.
    try {
      const w = await invoke<{ address: string; publicKeyHex: string; privateKeyWif: string } | null>(
        'load_saved_wallet', { passphrase: null }
      );
      if (w) {
        loadWallet(w);
        walletSaved = true;
        walletIsLegacy = true; // legacy plaintext — nudge user to re-encrypt
      }
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : String(e);
      if (msg === 'passphrase_required') {
        showUnlockModal = true;
      }
    }

  });
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

    <div style="display: flex; gap: 8px; flex-wrap: wrap;">
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
      <button
        onclick={() => { showImport = !showImport; importError = null; showMnemonicImport = false; }}
        class="btn-ghost"
        title="Import an existing wallet from a WIF private key (starts with 'Q'). Hot wallet — test amounts only!"
        style="display: flex; align-items: center; gap: 6px;"
      >
        📥 Import WIF
      </button>
      <button
        onclick={() => { showMnemonicImport = !showMnemonicImport; importMnemonicError = null; showImport = false; }}
        class="btn-ghost"
        title="Restore a wallet from a 12 or 24-word BIP39 recovery phrase. Derives m/44'/3'/0'/0/0."
        style="display: flex; align-items: center; gap: 6px;"
      >
        🌱 Mnemonic
      </button>
    </div>
  </div>

  <!-- ── v0.3.6 — Import WIF panel ─────────────────────────────────────────── -->
  {#if showImport}
    <div
      class="card-doge slide-up"
      style="
        margin-bottom: 20px;
        border-color: rgba(255, 68, 68, 0.4);
        background: rgba(255, 68, 68, 0.04);
      "
      role="region"
      aria-label="Import WIF private key"
    >
      <div style="margin-bottom: 12px;">
        <div style="font-weight: 600; font-size: 0.9rem; margin-bottom: 6px;">📥 Import WIF Private Key</div>
        <!-- Prominent hot-wallet warning -->
        <div style="
          padding: 10px 14px;
          background: rgba(255, 68, 68, 0.12);
          border: 1px solid rgba(255, 68, 68, 0.5);
          border-radius: 8px;
          font-size: 0.8rem;
          color: var(--doge-red);
          line-height: 1.6;
          margin-bottom: 12px;
        " role="alert">
          🔥 <strong>Hot wallet warning!</strong> Only use test amounts — never reuse this key on mainnet.
          Never share your WIF key. If exposed, funds are gone. Such danger. Very caution. Wow.
        </div>
        <textarea
          bind:value={importWif}
          placeholder="Paste your Dogecoin WIF private key here (starts with 'Q')..."
          rows="3"
          class="input-doge"
          style="width: 100%; resize: vertical; font-family: var(--font-mono); font-size: 0.8rem;"
          aria-label="WIF private key input"
          spellcheck="false"
          autocomplete="off"
        ></textarea>
        {#if importError}
          <div style="
            margin-top: 8px;
            padding: 8px 12px;
            background: rgba(255, 68, 68, 0.1);
            border: 1px solid rgba(255, 68, 68, 0.3);
            border-radius: 6px;
            color: var(--doge-red);
            font-size: 0.78rem;
          " role="alert">
            ❌ {importError}
          </div>
        {/if}
        <div style="display: flex; gap: 8px; margin-top: 10px; flex-wrap: wrap;">
          <button
            onclick={importWallet}
            disabled={isImporting || !importWif.trim()}
            class="btn-doge"
            style="display: flex; align-items: center; gap: 8px; flex: 1; justify-content: center; min-width: 140px;"
            title="Derive Dogecoin address from this WIF key. Much cryptography."
          >
            {#if isImporting}
              <DogeSpinner size="sm" message="" />
              Importing...
            {:else}
              🔑 Import Key
            {/if}
          </button>
          <button
            onclick={() => { showImport = false; importWif = ''; importError = null; }}
            class="btn-ghost"
            style="padding: 8px 16px;"
          >
            Cancel
          </button>
        </div>
      </div>
    </div>
  {/if}

  <!-- ── v0.3.16 — Import Mnemonic panel ──────────────────────────────────── -->
  {#if showMnemonicImport}
    <div
      class="card-doge slide-up"
      style="
        margin-bottom: 20px;
        border-color: rgba(0, 200, 100, 0.4);
        background: rgba(0, 200, 100, 0.04);
      "
      role="region"
      aria-label="Import from mnemonic phrase"
    >
      <div style="font-weight: 600; font-size: 0.9rem; margin-bottom: 10px;">🌱 Restore from Recovery Phrase</div>
      <p style="margin: 0 0 12px 0; font-size: 0.8rem; color: var(--doge-muted); line-height: 1.6;">
        Enter your 12 or 24-word BIP39 recovery phrase (space-separated).
        Derives key at <span class="mono" style="font-size: 0.75rem;">m/44'/3'/0'/0/0</span> (Dogecoin BIP44).
      </p>
      <textarea
        bind:value={importMnemonicPhrase}
        placeholder="word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12"
        rows="3"
        class="input-doge"
        style="width: 100%; resize: vertical; font-family: var(--font-mono); font-size: 0.8rem;"
        aria-label="BIP39 mnemonic phrase input"
        spellcheck="false"
        autocomplete="off"
      ></textarea>
      {#if importMnemonicError}
        <div style="
          margin-top: 8px;
          padding: 8px 12px;
          background: rgba(255, 68, 68, 0.1);
          border: 1px solid rgba(255, 68, 68, 0.3);
          border-radius: 6px;
          color: var(--doge-red);
          font-size: 0.78rem;
        " role="alert">
          ❌ {importMnemonicError}
        </div>
      {/if}
      <div style="display: flex; gap: 8px; margin-top: 10px; flex-wrap: wrap;">
        <button
          onclick={importMnemonicWallet}
          disabled={isImportingMnemonic || !importMnemonicPhrase.trim()}
          class="btn-doge"
          style="display: flex; align-items: center; gap: 8px; flex: 1; justify-content: center; min-width: 140px;"
          title="Derive Dogecoin wallet from this recovery phrase at m/44'/3'/0'/0/0"
        >
          {#if isImportingMnemonic}
            <DogeSpinner size="sm" message="" />
            Restoring...
          {:else}
            🌱 Restore Wallet
          {/if}
        </button>
        <button
          onclick={() => { showMnemonicImport = false; importMnemonicPhrase = ''; importMnemonicError = null; }}
          class="btn-ghost"
          style="padding: 8px 16px;"
        >
          Cancel
        </button>
      </div>
    </div>
  {/if}

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

      <!-- Balance -->
      <div class="card-doge" aria-label="Wallet balance">
        <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; flex-wrap: wrap; gap: 8px;">
          <div class="stat-label">💰 Balance</div>
          <button
            onclick={checkBalance}
            disabled={isFetchingBalance}
            class="btn-ghost"
            style="padding: 4px 12px; font-size: 0.78rem;"
            title="Query confirmed balance from Blockbook (Trezor)"
          >
            {isFetchingBalance ? '⏳ Fetching…' : '🔄 Check Balance'}
          </button>
        </div>
        {#if balance !== null}
          <div style="
            font-size: 1.5rem; font-weight: 700;
            color: var(--doge-yellow);
            font-family: var(--font-mono);
            letter-spacing: -0.02em;
          ">
            {balance.toFixed(8)} DOGE
          </div>
          <div style="font-size: 0.75rem; color: var(--doge-muted); margin-top: 4px;">
            {balanceSource === 'gateway' ? '📡 Relayed by gateway over LoRa' : '🌐 Confirmed balance from Trezor Blockbook'}
          </div>
        {:else if balanceError}
          <div style="font-size: 0.8rem; color: #ff6060;">{balanceError}</div>
        {:else}
          <div style="font-size: 0.82rem; color: var(--doge-muted);">Click "Check Balance" to query the Dogecoin network.</div>
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
        💡 <strong style="color: var(--doge-yellow);">Tip:</strong> Each generated wallet comes with a 12-word recovery phrase — write it down offline.
        The phrase is shown once and never stored. The encrypted save below protects your private key with a passphrase,
        but does <em>not</em> store the recovery phrase. Much wallet. Very self-custody. Wow.
      </div>

      <!-- v0.3.16 — Encrypted persistent wallet section -->
      <div style="
        padding: 14px 16px;
        background: rgba(255,196,0,0.04);
        border: 1px solid rgba(255,196,0,0.2);
        border-radius: 8px;
        font-size: 0.82rem;
        line-height: 1.6;
      ">
        <p style="margin: 0 0 8px 0; font-weight: 700; color: var(--doge-yellow); font-size: 0.88rem;">
          🔐 Encrypted Wallet Storage
        </p>
        <p style="margin: 0 0 10px 0; color: var(--doge-muted);">
          Save your wallet to local storage, protected with a passphrase (ChaCha20-Poly1305 + argon2id).
          The passphrase is never stored — if you forget it, you cannot recover the saved wallet.
        </p>
        {#if walletIsLegacy}
          <p style="margin: 0 0 10px 0; font-size: 0.78rem; color: #ff8080;">
            ⚠️ Your saved wallet uses an old unencrypted format. Click "Save Encrypted" to upgrade it.
          </p>
        {/if}
        <div style="display: flex; gap: 8px; flex-wrap: wrap; align-items: center;">
          {#if walletSaved && !walletIsLegacy}
            <span style="font-size: 0.78rem; color: var(--doge-neon);">🔐 Wallet saved (encrypted)</span>
            <button
              onclick={openSaveModal}
              style="padding: 6px 12px; border: 1px solid rgba(255,196,0,0.3); background: rgba(255,196,0,0.06); color: var(--doge-yellow); border-radius: 6px; cursor: pointer; font-size: 0.78rem;"
            >
              🔑 Change passphrase
            </button>
            <button
              onclick={deleteSavedWallet}
              style="padding: 6px 12px; border: 1px solid rgba(255,60,60,0.4); background: rgba(255,60,60,0.08); color: #ff8080; border-radius: 6px; cursor: pointer; font-size: 0.78rem;"
            >
              🗑️ Remove saved wallet
            </button>
          {:else}
            <button
              onclick={openSaveModal}
              class="btn-doge"
              style="padding: 6px 14px; font-size: 0.8rem;"
            >
              🔐 Save Encrypted
            </button>
          {/if}
        </div>
      </div>
    </div>
  {/if}

  <!-- v0.3.16 — Passphrase-protected wallet save modal -->
  {#if showSaveModal}
    <div
      style="
        position: fixed; inset: 0; background: rgba(0,0,0,0.85);
        display: flex; align-items: center; justify-content: center;
        z-index: 9999; padding: 24px;
      "
      role="dialog" aria-modal="true" aria-label="Save wallet with passphrase"
    >
      <div style="
        background: var(--doge-card);
        border: 2px solid rgba(255,196,0,0.4);
        border-radius: 16px;
        padding: 28px;
        max-width: 440px;
        width: 100%;
        box-shadow: 0 0 40px rgba(255,196,0,0.12);
      ">
        <h3 style="margin: 0 0 10px 0; color: var(--doge-yellow); font-size: 1.05rem;">🔐 Encrypt &amp; Save Wallet</h3>
        <p style="margin: 0 0 16px 0; font-size: 0.83rem; color: var(--doge-muted); line-height: 1.6;">
          Your private key will be encrypted with <strong style="color:var(--doge-text);">ChaCha20-Poly1305</strong> using a passphrase you choose.
          Pick something strong — there is no recovery option if you forget it.
        </p>
        <label for="save-passphrase" style="display:block; font-size:0.8rem; color:var(--doge-muted); margin-bottom:4px;">Passphrase (min. 8 chars)</label>
        <input
          id="save-passphrase"
          type="password"
          placeholder="Enter passphrase…"
          bind:value={savePassphrase}
          autocomplete="new-password"
          style="
            width: 100%; padding: 10px 14px; box-sizing: border-box;
            background: var(--doge-dark); border: 1px solid rgba(255,196,0,0.3);
            border-radius: 8px; color: var(--doge-text); font-size: 0.9rem;
            margin-bottom: 10px; outline: none;
          "
        />
        <label for="save-passphrase-confirm" style="display:block; font-size:0.8rem; color:var(--doge-muted); margin-bottom:4px;">Confirm passphrase</label>
        <input
          id="save-passphrase-confirm"
          type="password"
          placeholder="Repeat passphrase…"
          bind:value={savePassphraseConfirm}
          autocomplete="new-password"
          style="
            width: 100%; padding: 10px 14px; box-sizing: border-box;
            background: var(--doge-dark); border: 1px solid rgba(255,196,0,0.3);
            border-radius: 8px; color: var(--doge-text); font-size: 0.9rem;
            margin-bottom: 10px; outline: none;
          "
        />
        {#if saveWalletError}
          <p style="margin: 0 0 10px 0; font-size: 0.78rem; color: #ff6060;">{saveWalletError}</p>
        {/if}
        <div style="display: flex; gap: 8px; justify-content: flex-end;">
          <button
            onclick={() => { showSaveModal = false; savePassphrase = ''; savePassphraseConfirm = ''; saveWalletError = null; }}
            style="padding: 8px 18px; border: 1px solid var(--doge-border); background: transparent; color: var(--doge-muted); border-radius: 8px; cursor: pointer; font-size: 0.85rem;"
          >
            Cancel
          </button>
          <button
            onclick={confirmSaveWallet}
            disabled={isSavingWallet}
            class="btn-doge"
            style="padding: 8px 18px; font-size: 0.85rem; opacity: {isSavingWallet ? 0.6 : 1};"
          >
            {isSavingWallet ? '⏳ Encrypting…' : '🔐 Save Encrypted'}
          </button>
        </div>
      </div>
    </div>
  {/if}

  <!-- v0.3.16 — Recovery phrase backup modal (shown after generating a new wallet) -->
  {#if showMnemonicBackup && generatedMnemonic}
    <div
      style="
        position: fixed; inset: 0; background: rgba(0,0,0,0.92);
        display: flex; align-items: center; justify-content: center;
        z-index: 9999; padding: 24px;
      "
      role="dialog" aria-modal="true" aria-label="Back up your recovery phrase"
    >
      <div style="
        background: var(--doge-card);
        border: 2px solid rgba(0, 220, 110, 0.45);
        border-radius: 16px;
        padding: 28px;
        max-width: 500px;
        width: 100%;
        box-shadow: 0 0 48px rgba(0,220,110,0.12);
      ">
        <div style="font-size: 2.2rem; text-align: center; margin-bottom: 10px;">🌱</div>
        <h3 style="margin: 0 0 6px 0; color: #00dc6e; font-size: 1.05rem; text-align: center;">Save Your Recovery Phrase</h3>
        <p style="margin: 0 0 18px 0; font-size: 0.82rem; color: var(--doge-muted); text-align: center; line-height: 1.6;">
          Write these 12 words down on paper and store them offline.
          Anyone with this phrase can access your wallet.
          <strong style="color: var(--doge-text);">Never share it.</strong>
        </p>
        <!-- 12-word grid -->
        <div style="
          display: grid;
          grid-template-columns: repeat(3, 1fr);
          gap: 8px;
          margin-bottom: 18px;
          padding: 16px;
          background: var(--doge-dark);
          border: 1px solid rgba(0,220,110,0.2);
          border-radius: 12px;
        ">
          {#each generatedMnemonic.split(' ') as word, i}
            <div style="
              display: flex;
              align-items: center;
              gap: 6px;
              padding: 6px 10px;
              background: rgba(0,220,110,0.06);
              border: 1px solid rgba(0,220,110,0.15);
              border-radius: 6px;
              font-size: 0.82rem;
            ">
              <span style="color: var(--doge-subtle); font-size: 0.7rem; min-width: 18px;">{i + 1}.</span>
              <span class="mono" style="color: var(--doge-text);">{word}</span>
            </div>
          {/each}
        </div>
        <button
          onclick={() => copyToClipboard(generatedMnemonic ?? '', 'mnemonic')}
          class="btn-ghost"
          style="width: 100%; margin-bottom: 14px; font-size: 0.82rem;"
        >
          {copyFeedback.mnemonic ? '✅ Copied!' : '📋 Copy phrase to clipboard'}
        </button>
        <div style="
          padding: 10px 14px;
          background: rgba(255, 140, 0, 0.08);
          border: 1px solid rgba(255, 140, 0, 0.3);
          border-radius: 8px;
          font-size: 0.78rem;
          color: var(--doge-orange);
          margin-bottom: 16px;
          line-height: 1.6;
        " role="alert">
          ⚠️ This phrase will not be shown again. Store it offline before continuing.
        </div>
        <button
          onclick={() => { showMnemonicBackup = false; generatedMnemonic = null; }}
          class="btn-doge"
          style="width: 100%; padding: 12px; font-size: 0.9rem;"
        >
          ✅ I've saved my recovery phrase
        </button>
      </div>
    </div>
  {/if}

  <!-- v0.3.16 — Passphrase unlock modal (shown on startup when encrypted wallet exists) -->
  {#if showUnlockModal}
    <div
      style="
        position: fixed; inset: 0; background: rgba(0,0,0,0.9);
        display: flex; align-items: center; justify-content: center;
        z-index: 9999; padding: 24px;
      "
      role="dialog" aria-modal="true" aria-label="Unlock saved wallet"
    >
      <div style="
        background: var(--doge-card);
        border: 2px solid rgba(255,196,0,0.4);
        border-radius: 16px;
        padding: 28px;
        max-width: 380px;
        width: 100%;
        box-shadow: 0 0 40px rgba(255,196,0,0.15);
        text-align: center;
      ">
        <div style="font-size: 2.5rem; margin-bottom: 10px;">🔐</div>
        <h3 style="margin: 0 0 8px 0; color: var(--doge-yellow); font-size: 1.05rem;">Unlock Saved Wallet</h3>
        <p style="margin: 0 0 16px 0; font-size: 0.83rem; color: var(--doge-muted); line-height: 1.5;">
          An encrypted wallet was found on disk. Enter your passphrase to load it.
        </p>
        <input
          type="password"
          placeholder="Enter passphrase…"
          bind:value={unlockPassphrase}
          autocomplete="current-password"
          onkeydown={(e) => { if (e.key === 'Enter') unlockWallet(); }}
          style="
            width: 100%; padding: 10px 14px; box-sizing: border-box;
            background: var(--doge-dark); border: 1px solid rgba(255,196,0,0.35);
            border-radius: 8px; color: var(--doge-text); font-size: 0.9rem;
            margin-bottom: 10px; outline: none; text-align: left;
          "
        />
        {#if unlockError}
          <p style="margin: 0 0 10px 0; font-size: 0.78rem; color: #ff6060;">{unlockError}</p>
        {/if}
        <div style="display: flex; gap: 8px; justify-content: center;">
          <button
            onclick={() => { showUnlockModal = false; unlockPassphrase = ''; unlockError = null; }}
            style="padding: 8px 18px; border: 1px solid var(--doge-border); background: transparent; color: var(--doge-muted); border-radius: 8px; cursor: pointer; font-size: 0.85rem;"
          >
            Skip
          </button>
          <button
            onclick={unlockWallet}
            disabled={isUnlocking}
            class="btn-doge"
            style="padding: 8px 18px; font-size: 0.85rem; opacity: {isUnlocking ? 0.6 : 1};"
          >
            {isUnlocking ? '⏳ Unlocking…' : '🔓 Unlock'}
          </button>
        </div>
      </div>
    </div>
  {/if}
</div>
