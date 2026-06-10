// Minimal type declaration for the 'qrcode' package.
// The package ships CommonJS without bundled TS types; this shim provides
// the subset used by QRCode.svelte so svelte-check doesn't error.
declare module 'qrcode' {
  interface QRCodeToDataURLOptions {
    width?: number;
    margin?: number;
    color?: { dark?: string; light?: string };
    errorCorrectionLevel?: 'L' | 'M' | 'Q' | 'H';
  }
  export function toDataURL(text: string, opts?: QRCodeToDataURLOptions): Promise<string>;
}
