<script lang="ts">
  /**
   * RadioDoge GUI — Main SPA page.
   *
   * This is the root of the single-page application. It:
   *   1. Sets up Tauri event listeners (connection status, radio packets, stats)
   *   2. Manages tab state
   *   3. Renders the NavBar and the active tab content
   */

  import '../app.css';
  import { onMount } from 'svelte';
  import { listen } from '@tauri-apps/api/event';

  import NavBar from '$lib/components/NavBar.svelte';
  import ConnectionPanel from '$lib/components/ConnectionPanel.svelte';
  import Dashboard from '$lib/components/Dashboard.svelte';
  import WalletTab from '$lib/components/WalletTab.svelte';
  import SendTab from '$lib/components/SendTab.svelte';
  import ReceiveTab from '$lib/components/ReceiveTab.svelte';
  import HistoryTab from '$lib/components/HistoryTab.svelte';
  import MeshTab from '$lib/components/MeshTab.svelte';
  import AddressBook from '$lib/components/AddressBook.svelte';
  import SettingsTab from '$lib/components/SettingsTab.svelte';
  import DebugConsole from '$lib/components/DebugConsole.svelte';
  import Confetti from '$lib/components/Confetti.svelte';

  import {
    connection,
    setConnected,
    setConnecting,
    setReconnecting,
    setDisconnected,
    setError,
    updateStats,
    applyBoardSync,
    setGatewayOnline,
    setAddrConflict,
  } from '$lib/stores/connection.svelte';

  import { radio, addPacket, addSentPacket } from '$lib/stores/radio.svelte';

  import type {
    BoardSettings,
    ConnectionStatusEvent,
    IncomingPacket,
    RadioStats,
  } from '$lib/types';

  // ── Tab State ──────────────────────────────────────────────────────────────
  let activeTab = $state('connect');

  function handleTabChange(tab: string) {
    activeTab = tab;
  }

  // ── Debug Console ─────────────────────────────────────────────────────────
  let debugVisible = $state(false);

  function toggleDebugConsole() {
    debugVisible = !debugVisible;
  }

  // ── Confetti ───────────────────────────────────────────────────────────────
  let confetti: Confetti;

  function triggerConfetti() {
    confetti?.trigger();
  }


  // ── Easter Egg: Konami Code 🐕🌙 ──────────────────────────────────────────
  // ↑ ↑ ↓ ↓ ← → ← → B A — much secret, very hidden, wow
  const KONAMI = ['ArrowUp','ArrowUp','ArrowDown','ArrowDown','ArrowLeft','ArrowRight','ArrowLeft','ArrowRight','b','a'];
  let konamiBuffer = $state<string[]>([]);
  let konamiActivated = $state(false);
  let konamiMessage = $state('');

  function handleKeydown(e: KeyboardEvent) {
    // Ctrl+Shift+D → toggle Debug Console
    if (e.ctrlKey && e.shiftKey && e.key === 'D') {
      e.preventDefault();
      toggleDebugConsole();
      return;
    }

    // Konami code detection
    konamiBuffer = [...konamiBuffer, e.key].slice(-KONAMI.length);
    if (konamiBuffer.join(',') === KONAMI.join(',')) {
      konamiActivated = true;
      konamiBuffer = [];
      const msgs = [
        '🐕 Such secret! Very Konami. Much wow! 🌙',
        '🎮 You found the Easter egg! Very gamer. Such doge.',
        '🌙 To the moon! 🚀 Much hidden. Very code. Wow.',
        '🐾 Shiba detected! You are truly one of us. Such radio.',
      ];
      konamiMessage = msgs[Math.floor(Math.random() * msgs.length)];
      confetti?.trigger();
      setTimeout(() => { konamiActivated = false; konamiMessage = ''; }, 3500);
    }
  }

  // ── Tauri Event Listeners ─────────────────────────────────────────────────
  onMount(() => {
    const unlisteners: Array<() => void> = [];

    // Connection status changes from Rust backend
    listen<ConnectionStatusEvent>('connection-status', (event) => {
      const status = event.payload;
      switch (status.status) {
        case 'connecting':
          setConnecting(status.port ?? '');
          break;
        case 'connected':
          setConnected(status.port ?? '', status.nodeAddress ?? null, status.firmwareVersion);
          // Auto-navigate to Dashboard on connect
          if (activeTab === 'connect') {
            activeTab = 'dashboard';
          }
          break;
        case 'reconnecting':
          setReconnecting(status.port ?? '');
          break;
        case 'disconnected':
          setDisconnected();
          activeTab = 'connect';
          break;
        case 'error':
          setError(status.message ?? 'Unknown connection error');
          break;
      }
    }).then(fn => unlisteners.push(fn));

    // Incoming LoRa packets
    listen<IncomingPacket>('radio-packet', (event) => {
      addPacket(event.payload);
    }).then(fn => unlisteners.push(fn));

    // Outgoing (TX) LoRa packets — emitted by Rust when we send a transaction
    listen<IncomingPacket>('radio-packet-tx', (event) => {
      addSentPacket(event.payload);
    }).then(fn => unlisteners.push(fn));

    // Radio statistics updates (every 2s when connected)
    listen<RadioStats>('radio-stats-update', (event) => {
      updateStats(event.payload);
    }).then(fn => unlisteners.push(fn));

    // Transaction sent success (triggers confetti from Rust)
    listen<string>('transaction-sent', () => {
      triggerConfetti();
    }).then(fn => unlisteners.push(fn));

    // v0.3.6 — Board-sync: board is source of truth.
    // App queries CMD_GET_SETTINGS (0x22) on connect; board replies with node addr + gateway_mode.
    listen<BoardSettings>('board-sync', (event) => {
      applyBoardSync(event.payload);
    }).then(fn => unlisteners.push(fn));

    // v0.3.6 — Gateway daemon status (radiodoge-cli daemon spawned/stopped)
    listen<{ online: boolean }>('gateway-status', (event) => {
      setGatewayOnline(event.payload.online);
    }).then(fn => unlisteners.push(fn));

    // v0.3.7 — Address conflict notification from board (0x25)
    listen<{ detected: boolean }>('addr-conflict', (event) => {
      if (event.payload.detected) {
        setAddrConflict(true);
        // Auto-switch to Mesh tab so user sees the warning
        activeTab = 'mesh';
      }
    }).then(fn => unlisteners.push(fn));

    // Keyboard listener (Ctrl+Shift+D + Konami code)
    window.addEventListener('keydown', handleKeydown);

    // Cleanup all listeners on component destroy
    return () => {
      unlisteners.forEach(fn => fn());
      window.removeEventListener('keydown', handleKeydown);
    };
  });
</script>

<!-- Confetti overlay (renders above everything else) -->
<Confetti bind:this={confetti} />

<!-- Debug Console (hidden by default, toggle via Ctrl+Shift+D or footer button) -->
<DebugConsole bind:visible={debugVisible} />

<!-- Konami code Easter egg toast 🐕 -->
{#if konamiActivated}
  <div
    class="konami-activated"
    style="
      position: fixed;
      bottom: 48px;
      left: 50%;
      transform: translateX(-50%);
      background: linear-gradient(135deg, var(--doge-yellow), var(--doge-orange));
      color: #000;
      font-weight: 700;
      padding: 14px 28px;
      border-radius: 50px;
      font-size: 1rem;
      z-index: 9999;
      box-shadow: 0 4px 24px rgba(245, 197, 24, 0.5);
      white-space: nowrap;
      animation: slide-up 0.3s ease;
    "
    role="status"
    aria-live="assertive"
  >
    {konamiMessage}
  </div>
{/if}

<!-- App shell -->
<div class="app-shell">
  <!-- Navigation bar (sticky top) -->
  <NavBar {activeTab} onTabChange={handleTabChange} />

  <!-- Tab content area (scrollable) -->
  <main class="app-main">
    {#if activeTab === 'connect'}
      <ConnectionPanel />
    {:else if activeTab === 'dashboard'}
      <Dashboard />
    {:else if activeTab === 'wallet'}
      <WalletTab />
    {:else if activeTab === 'send'}
      <SendTab onTriggerConfetti={triggerConfetti} />
    {:else if activeTab === 'receive'}
      <ReceiveTab />
    {:else if activeTab === 'history'}
      <HistoryTab />
    {:else if activeTab === 'mesh'}
      <MeshTab />
    {:else if activeTab === 'addrbook'}
      <div style="padding: 24px; max-width: 900px; margin: 0 auto;">
        <AddressBook />
      </div>
    {:else if activeTab === 'settings'}
      <SettingsTab />
    {/if}
  </main>

  <!-- Status bar (bottom) — responsive, truncates long device paths -->
  <footer class="app-footer">
    <!-- Left: version + connection info (truncates to fit) -->
    <div class="footer-left">
      <span class="footer-version">RadioDoge v0.3.15</span>
      <span class="footer-sep" aria-hidden="true">|</span>
      {#if connection.isConnected}
        <span class="footer-port">
          🟢 {connection.portName}{connection.nodeAddress != null
            ? ` · ${connection.nodeAddress.region}.${connection.nodeAddress.community}.${connection.nodeAddress.node}`
            : ''}
        </span>
        <span class="footer-stats">↑{connection.stats?.packetsSent ?? 0} ↓{connection.stats?.packetsReceived ?? 0}</span>
      {:else}
        <span class="footer-disconnected">🔴 Not connected</span>
      {/if}
    </div>
    <!-- Right: debug toggle + slogan (slogan hidden on narrow screens) -->
    <div class="footer-right">
      <button
        onclick={toggleDebugConsole}
        title="Toggle Debug Console (Ctrl+Shift+D)"
        style="
          background: {debugVisible ? 'rgba(245, 197, 24, 0.15)' : 'transparent'};
          border: 1px solid {debugVisible ? 'var(--doge-yellow)' : 'transparent'};
          border-radius: 4px;
          color: {debugVisible ? 'var(--doge-yellow)' : 'var(--doge-subtle)'};
          cursor: pointer;
          font-size: 0.68rem;
          padding: 2px 7px;
          font-family: var(--font-mono);
          transition: all 0.15s;
          flex-shrink: 0;
        "
      >
        🐛
      </button>
      <span class="footer-slogan">such decentralize. wow 🐕</span>
    </div>
  </footer>
</div>

<style>
  /* ── App shell ───────────────────────────────────────────────────────────── */
  .app-shell {
    display: flex;
    flex-direction: column;
    /* 100svh = small viewport height: accounts for dynamic browser chrome on
       mobile while falling back to 100vh on older engines. Tauri Android fills
       the full window so both values are equivalent, but svh is explicit. */
    height: 100vh;
    height: 100svh;
    height: 100dvh; /* dynamic viewport height: adjusts as Android chrome shows/hides */
    background: var(--doge-dark);
    overflow: hidden;
  }

  .app-main {
    flex: 1;
    overflow-y: auto;
    overflow-x: hidden;
    /* Ensure -webkit-overflow-scrolling: touch for smooth inertia on Android */
    -webkit-overflow-scrolling: touch;
  }

  /* ── Bottom status bar ───────────────────────────────────────────────────── */
  .app-footer {
    display: flex;
    align-items: center;
    justify-content: space-between;
    min-height: 28px;
    height: auto;
    background: #080808;
    border-top: 1px solid var(--doge-border);
    padding: 3px clamp(8px, 3vw, 16px);
    padding-bottom: max(3px, env(safe-area-inset-bottom, 0px));
    gap: clamp(4px, 1.5vw, 8px);
    font-size: clamp(0.6rem, 1.8vw, 0.7rem);
    color: var(--doge-subtle);
    flex-shrink: 0;
    overflow: hidden; /* never let footer grow and push content out */
  }

  /* Left group takes all available space; overflowing text gets ellipsis */
  .footer-left {
    display: flex;
    align-items: center;
    gap: clamp(4px, 1.5vw, 8px);
    flex: 1 1 0;
    min-width: 0; /* critical: allows flex children to shrink below content size */
    overflow: hidden;
  }

  .footer-version {
    white-space: nowrap;
    flex-shrink: 0;
  }

  .footer-sep {
    flex-shrink: 0;
    opacity: 0.4;
  }

  /* The device path span — this is the one that must truncate */
  .footer-port {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    min-width: 0;
    flex: 1 1 0;
    color: var(--doge-neon);
  }

  .footer-stats {
    white-space: nowrap;
    flex-shrink: 0;
    opacity: 0.8;
  }

  .footer-disconnected {
    white-space: nowrap;
    flex-shrink: 0;
  }

  /* Right group: debug button + slogan — shrinks but never overflows left */
  .footer-right {
    display: flex;
    align-items: center;
    gap: clamp(4px, 1.5vw, 8px);
    flex-shrink: 0;
    margin-left: clamp(4px, 1.5vw, 8px);
  }

  .footer-slogan {
    white-space: nowrap;
    color: var(--doge-yellow);
    font-style: italic;
  }

  /* Hide slogan on phones narrower than 520px — it's just flavour text */
  @media (max-width: 520px) {
    .footer-slogan { display: none; }
  }
</style>
