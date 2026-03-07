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
  import Confetti from '$lib/components/Confetti.svelte';

  import {
    connection,
    setConnected,
    setConnecting,
    setDisconnected,
    setError,
    updateStats,
  } from '$lib/stores/connection.svelte';

  import { radio, addPacket } from '$lib/stores/radio.svelte';

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

  // ── Confetti ───────────────────────────────────────────────────────────────
  let confetti: Confetti;

  function triggerConfetti() {
    confetti?.trigger();
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
          setConnected(status.port ?? '', status.nodeAddress ?? null);
          // Auto-navigate to Dashboard on connect
          if (activeTab === 'connect') {
            activeTab = 'dashboard';
          }
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

    // Radio statistics updates (every 2s when connected)
    listen<RadioStats>('radio-stats-update', (event) => {
      updateStats(event.payload);
    }).then(fn => unlisteners.push(fn));

    // Transaction sent success (triggers confetti from Rust)
    listen<string>('transaction-sent', () => {
      triggerConfetti();
    }).then(fn => unlisteners.push(fn));

    // Cleanup all listeners on component destroy
    return () => {
      unlisteners.forEach(fn => fn());
    };
  });
</script>

<!-- Confetti overlay (renders above everything else) -->
<Confetti bind:this={confetti} />

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
    <span>RadioDoge v0.2.0</span>
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
    <span style="color: var(--doge-yellow); font-style: italic;">
      such decentralize. very LoRa. wow 🐕
    </span>
  </footer>
</div>
