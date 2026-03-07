<script lang="ts">
  /**
   * Doge-themed loading spinner.
   * Shows a spinning 🐕 emoji or a golden coin animation.
   */

  interface Props {
    size?: 'sm' | 'md' | 'lg';
    message?: string;
  }

  let { size = 'md', message = 'Loading...' }: Props = $props();

  const fontSize = $derived(() => {
    switch (size) {
      case 'sm': return '1.5rem';
      case 'lg': return '3rem';
      default:   return '2rem';
    }
  });
</script>

<div style="display: flex; flex-direction: column; align-items: center; gap: 8px;">
  <span
    style="
      font-size: {fontSize()};
      animation: spin-doge 1.5s linear infinite;
      display: inline-block;
      line-height: 1;
    "
    aria-label="Loading"
  >🐕</span>
  {#if message}
    <span style="color: var(--doge-muted); font-size: 0.8rem;">{message}</span>
  {/if}
</div>
