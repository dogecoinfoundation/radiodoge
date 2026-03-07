/**
 * Wallet state store using Svelte 5 runes.
 *
 * Holds the currently generated Dogecoin keypair.
 * In a production app this would be persisted to an encrypted keystore —
 * for MVP it's in-memory only (user must save private key manually).
 */

import type { WalletInfo } from '$lib/types';

export const wallet = $state({
  /** Whether a wallet has been generated this session */
  isGenerated: false,
  /** Dogecoin address (starts with "D" on mainnet) */
  address: '',
  /** Compressed public key, hex-encoded */
  publicKeyHex: '',
  /** Private key in WIF format (starts with "Q") */
  privateKeyWif: '',
  /** Whether the private key is currently visible in the UI */
  privateKeyVisible: false,
  /** Whether we're currently generating (brief loading state) */
  isGenerating: false,
  /** Any error from the last generation attempt */
  error: null as string | null,
});

/** Load generated wallet info into state */
export function loadWallet(info: WalletInfo) {
  wallet.address = info.address;
  wallet.publicKeyHex = info.publicKeyHex;
  wallet.privateKeyWif = info.privateKeyWif;
  wallet.isGenerated = true;
  wallet.privateKeyVisible = false;
  wallet.error = null;
}

/** Clear wallet state */
export function clearWallet() {
  wallet.address = '';
  wallet.publicKeyHex = '';
  wallet.privateKeyWif = '';
  wallet.isGenerated = false;
  wallet.privateKeyVisible = false;
}

/** Toggle private key visibility */
export function togglePrivateKeyVisibility() {
  wallet.privateKeyVisible = !wallet.privateKeyVisible;
}
