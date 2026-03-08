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
  } from '$lib/stores/connection.svelte';

  import { radio, addPacket, addSentPacket } from '$lib/stores/radio.svelte';

  import type {
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
<div style="
  display: flex;
  flex-direction: column;
  height: 100vh;
  background: var(--doge-dark);
  overflow: hidden;
">
  <!-- Navigation bar (sticky top) -->
  <NavBar {activeTab} onTabChange={handleTabChange} />

  <!-- Tab content area (scrollable) -->
  <main style="flex: 1; overflow-y: auto; overflow-x: hidden;">
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
    {:else if activeTab === 'settings'}
      <SettingsTab />
    {/if}
  </main>

  <!-- Status bar (bottom) -->
  <footer style="
    height: 28px;
    background: #080808;
    border-top: 1px solid var(--doge-border);
    display: flex;
    align-items: center;
    padding: 0 16px;
    gap: 16px;
    font-size: 0.7rem;
    color: var(--doge-subtle);
    flex-shrink: 0;
  ">
    <span>RadioDoge v0.3.5</span>
    <span>|</span>
    {#if connection.isConnected}
      <span style="color: var(--doge-neon);">
        🟢 {connection.portName} — Node {connection.nodeAddress?.region ?? '?'}.{connection.nodeAddress?.community ?? '?'}.{connection.nodeAddress?.node ?? '?'}
      </span>
      <span>|</span>
      <span>
        ↑ {connection.stats?.packetsSent ?? 0} sent
      </span>
      <span>
        ↓ {connection.stats?.packetsReceived ?? 0} received
      </span>
    {:else}
      <span>🔴 Not connected</span>
    {/if}
    <span style="flex: 1;"></span>
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
        padding: 1px 6px;
        font-family: var(--font-mono);
        transition: all 0.15s;
      "
    >
      🐛
    </button>
    <span style="color: var(--doge-yellow); font-style: italic;">
      such decentralize. very LoRa. wow 🐕
    </span>
  </footer>
</div>
