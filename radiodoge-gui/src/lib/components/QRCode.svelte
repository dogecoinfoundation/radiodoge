<script lang="ts">
  /**
   * QR code component — renders a Dogecoin address as a scannable QR code.
   * Uses the `qrcode` npm package to generate an SVG data URL.
   */

  import { onMount } from 'svelte';

  interface Props {
    value: string;      // The data to encode (Dogecoin address)
    size?: number;      // Size in pixels (default 200)
    label?: string;     // Caption below the QR code
  }

  let { value, size = 200, label = 'Scan to send DOGE here 🐕' }: Props = $props();

  let dataUrl = $state('');
  let error = $state('');

  async function generateQR(val: string) {
    if (!val) {
      dataUrl = '';
      return;
    }
    try {
      const QRCode = await import('qrcode');
      dataUrl = await QRCode.toDataURL(val, {
        width: size,
        margin: 2,
        color: {
          dark: '#000000',
          light: '#FFFFFF',
        },
        errorCorrectionLevel: 'M',
      });
      error = '';
    } catch (e) {
      error = 'Failed to generate QR code';
      dataUrl = '';
    }
  }

  // Re-generate whenever `value` changes
  $effect(() => {
    generateQR(value);
  });
</script>

<div style="display: flex; flex-direction: column; align-items: center; gap: 8px;">
  {#if error}
    <div style="color: var(--doge-red); font-size: 0.8rem;">{error}</div>
  {:else if dataUrl}
    <div
      style="
        padding: 12px;
        background: white;
        border-radius: 12px;
        box-shadow: var(--glow-yellow);
      "
    >
      <img src={dataUrl} alt="QR Code for {value}" width={size} height={size} />
    </div>
    {#if label}
      <p style="color: var(--doge-muted); font-size: 0.75rem; text-align: center; margin: 0;">
        {label}
      </p>
    {/if}
  {:else if value}
    <div style="
      width: {size}px;
      height: {size}px;
      background: var(--doge-card);
      border-radius: 12px;
      display: flex;
      align-items: center;
      justify-content: center;
      color: var(--doge-muted);
      font-size: 0.8rem;
    ">
      Generating QR...
    </div>
  {:else}
    <div style="
      width: {size}px;
      height: {size}px;
      background: var(--doge-card);
      border: 2px dashed var(--doge-border);
      border-radius: 12px;
      display: flex;
      align-items: center;
      justify-content: center;
      color: var(--doge-subtle);
      font-size: 2rem;
    ">
      📱
    </div>
  {/if}
</div>
