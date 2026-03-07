<script lang="ts">
  /**
   * Navigation bar with tab switching and Doge branding.
   *
   * Tabs: Connect | Dashboard | Wallet | Send | Receive | Settings
   */

  import { connection } from '$lib/stores/connection.svelte';
  import SignalBars from './SignalBars.svelte';

  interface Props {
    activeTab: string;
    onTabChange: (tab: string) => void;
  }

  let { activeTab, onTabChange }: Props = $props();

  const tabs = [
    { id: 'connect',   icon: '🔌', label: 'Connect'   },
    { id: 'dashboard', icon: '📡', label: 'Dashboard'  },
    { id: 'wallet',    icon: '👛', label: 'Wallet'     },
    { id: 'send',      icon: '📤', label: 'Send'       },
    { id: 'receive',   icon: '📥', label: 'Receive'    },
    { id: 'settings',  icon: '⚙️', label: 'Settings'   },
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
      >DOGE RADIO</div>
      <div style="font-size: 0.6rem; color: var(--doge-subtle); letter-spacing: 0.1em;">
        v0.3.0 · WIRELESS P2P
      </div>
    </div>
  </div>

  <!-- Tab buttons -->
  {#each tabs as tab}
    {@const isActive = activeTab === tab.id}
    {@const isDisabled = tab.id !== 'connect' && tab.id !== 'settings' && !connection.isConnected}
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
      title={isDisabled ? 'Connect to Heltec first' : tab.label}
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
        🟢 {connection.portName}
      </span>
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
