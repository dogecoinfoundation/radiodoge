<script lang="ts">
  /**
   * Navigation bar with tab switching and Doge branding.
   *
   * Tabs: Connect | Dashboard | Wallet | Send | Receive | Settings
   */

  import { connection } from '$lib/stores/connection.svelte';

  // History tab is always accessible (even disconnected — you have past history)
  import SignalBars from './SignalBars.svelte';

  interface Props {
    activeTab: string;
    onTabChange: (tab: string) => void;
  }

  let { activeTab, onTabChange }: Props = $props();

  const tabs = [
    { id: 'connect',   icon: '🔌', label: 'Connect',   tooltip: 'Connect to your Heltec LoRa device via USB serial or BLE' },
    { id: 'dashboard', icon: '📡', label: 'Dashboard',  tooltip: 'Live radio stats: RSSI, SNR, packets sent/received. Much data!' },
    { id: 'wallet',    icon: '👛', label: 'Wallet',     tooltip: 'Generate or import a Dogecoin keypair — pure Rust crypto, no cloud. Very secure.' },
    { id: 'send',      icon: '📤', label: 'Send',       tooltip: 'Broadcast a Dogecoin transaction over LoRa radio. Such transaction!' },
    { id: 'receive',   icon: '📥', label: 'Receive',    tooltip: 'Live TX/RX packet log — see all incoming and outgoing LoRa traffic.' },
    { id: 'history',   icon: '📜', label: 'History',    tooltip: 'Transaction history — last 50 sends. Much records. Very blockchain.' },
    { id: 'mesh',      icon: '🕸️', label: 'Mesh',       tooltip: 'Mesh neighbors — nodes heard on the LoRa network. Such decentralize!' },
    { id: 'settings',  icon: '⚙️', label: 'Settings',   tooltip: 'Configure LoRa parameters, gateway mode, WiFi toggle. Such configure.' },
  ];
</script>

<nav
  style="
    display: flex;
    align-items: center;
    background: #0D0D0D;
    border-bottom: 1px solid var(--doge-border);
    padding: 0 16px;
    height: 56px;
    gap: 4px;
    position: sticky;
    top: 0;
    z-index: 100;
  "
>
  <!-- Logo / Branding -->
  <div
    style="
      display: flex;
      align-items: center;
      gap: 10px;
      margin-right: 24px;
      flex-shrink: 0;
    "
  >
    <img
      src="/doge-radio.png"
      alt="RadioDoge logo"
      width="32"
      height="32"
      style="
        border-radius: 8px;
        animation: bounce-doge 2s ease-in-out infinite;
        display: inline-block;
        filter: drop-shadow(0 0 6px rgba(245, 197, 24, 0.5));
      "
    />
    <div>
      <div
        style="
          font-weight: 800;
          font-size: 0.95rem;
          background: linear-gradient(90deg, var(--doge-yellow), var(--doge-orange));
          -webkit-background-clip: text;
          -webkit-text-fill-color: transparent;
          background-clip: text;
          line-height: 1.1;
          letter-spacing: 0.05em;
        "
      >RadioDoge</div>
      <div style="font-size: 0.6rem; color: var(--doge-subtle); letter-spacing: 0.1em;">
        WIRELESS P2P
      </div>
    </div>
  </div>

  <!-- Tab buttons -->
  {#each tabs as tab}
    {@const isActive = activeTab === tab.id}
    {@const isDisabled = tab.id !== 'connect' && tab.id !== 'settings' && tab.id !== 'history' && tab.id !== 'mesh' && !connection.isConnected}
    <button
      onclick={() => !isDisabled && onTabChange(tab.id)}
      style="
        display: flex;
        align-items: center;
        gap: 6px;
        padding: 8px 14px;
        border: none;
        background: {isActive ? 'rgba(245, 197, 24, 0.1)' : 'transparent'};
        color: {isActive ? 'var(--doge-yellow)' : isDisabled ? 'var(--doge-subtle)' : 'var(--doge-muted)'};
        border-radius: 8px;
        cursor: {isDisabled ? 'not-allowed' : 'pointer'};
        font-size: 0.85rem;
        font-weight: {isActive ? 600 : 400};
        position: relative;
        transition: all 0.15s ease;
        border-bottom: 2px solid {isActive ? 'var(--doge-yellow)' : 'transparent'};
        border-radius: 8px 8px 0 0;
      "
      title={isDisabled ? 'Connect to Heltec first to unlock this tab' : tab.tooltip}
    >
      <span style="font-size: 1rem; line-height: 1;">{tab.icon}</span>
      <span style="white-space: nowrap;">{tab.label}</span>
    </button>
  {/each}

  <!-- Spacer -->
  <div style="flex: 1;"></div>

  <!-- Connection status indicator (right side) -->
  <div style="display: flex; align-items: center; gap: 10px; margin-left: 8px;">
    {#if connection.isConnected}
      <SignalBars rssi={connection.stats?.rssi ?? -120} connected={true} size="sm" />
      <span
        style="
          background: rgba(0, 255, 136, 0.1);
          color: var(--doge-neon);
          border: 1px solid rgba(0, 255, 136, 0.3);
          border-radius: 20px;
          padding: 3px 10px;
          font-size: 0.72rem;
          font-weight: 600;
          white-space: nowrap;
        "
      >
        {connection.connectionType === 'ble' ? '📶' : '🟢'} {connection.portName}
      </span>
      {#if connection.gatewayMode}
        <span
          style="
            background: rgba(0, 255, 136, 0.08);
            color: var(--doge-neon);
            border: 1px solid rgba(0, 255, 136, 0.25);
            border-radius: 20px;
            padding: 3px 10px;
            font-size: 0.68rem;
            font-weight: 700;
            white-space: nowrap;
            letter-spacing: 0.05em;
          "
          title="Board is in gateway mode (stored in NVS)"
        >
          🌐 GATEWAY
        </span>
      {/if}
      {#if connection.gatewayOnline}
        <span
          style="
            background: rgba(245, 197, 24, 0.08);
            color: var(--doge-yellow);
            border: 1px solid rgba(245, 197, 24, 0.25);
            border-radius: 20px;
            padding: 3px 10px;
            font-size: 0.68rem;
            font-weight: 700;
            white-space: nowrap;
          "
          title="radiodoge-cli daemon is running"
        >
          ▶ Daemon
        </span>
      {/if}
    {:else if connection.isConnecting}
      <span
        style="
          color: var(--doge-yellow);
          font-size: 0.75rem;
          animation: pulse-glow 1s ease-in-out infinite;
        "
      >
        🟡 Connecting...
      </span>
    {:else}
      <span style="color: var(--doge-subtle); font-size: 0.75rem;">🔴 Disconnected</span>
    {/if}
  </div>
</nav>
