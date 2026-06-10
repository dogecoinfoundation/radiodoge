//! Pure Rust Dogecoin wallet implementation.
//!
//! Dogecoin uses the same elliptic curve cryptography as Bitcoin (secp256k1),
//! with different network version bytes:
//!   - Mainnet P2PKH address version: 0x1E  → addresses start with "D"
//!   - Mainnet WIF private key version: 0x9E → compressed WIF starts with "Q"
//!
//! No FFI to libdogecoin is needed — everything is done in pure Rust.

use anyhow::Result;
use argon2::{Argon2, Algorithm, Version, Params};
use chacha20poly1305::{ChaCha20Poly1305, Key, Nonce, KeyInit, aead::AeadInPlace};
use rand::{rngs::OsRng, RngCore};
use ripemd::{Digest as RipemdDigest, Ripemd160};
use secp256k1::{PublicKey, Secp256k1, SecretKey};
use sha2::Sha256;

use crate::types::WalletInfo;

// Dogecoin mainnet network bytes
const DOGE_MAINNET_ADDR_VERSION: u8 = 0x1E; // 30 decimal — P2PKH "D..." addresses
const DOGE_MAINNET_WIF_VERSION: u8 = 0x9E;  // 158 decimal — compressed WIF "Q..."

/// Generate a brand new Dogecoin keypair.
///
/// Uses OS entropy via `OsRng` for cryptographically secure key generation.
/// Returns a `WalletInfo` containing the address, compressed public key hex,
/// and WIF-encoded private key.
pub fn generate_keypair() -> Result<WalletInfo> {
    let secp = Secp256k1::new();
    let (secret_key, public_key) = secp.generate_keypair(&mut OsRng);

    let address = pubkey_to_address(&public_key)?;
    let public_key_hex = hex::encode(public_key.serialize()); // 33-byte compressed
    let private_key_wif = secret_key_to_wif(&secret_key)?;

    Ok(WalletInfo {
        address,
        public_key_hex,
        private_key_wif,
    })
}

/// Derive a Dogecoin P2PKH address from a compressed public key.
///
/// Algorithm:
/// 1. SHA256(compressed_pubkey)
/// 2. RIPEMD160(step1)  → pubkey hash (20 bytes)
/// 3. Prepend version byte 0x1E
/// 4. Base58Check encode
fn pubkey_to_address(public_key: &PublicKey) -> Result<String> {
    let compressed = public_key.serialize(); // 33 bytes, compressed

    // Step 1: SHA256 of the compressed public key
    let sha256_hash = Sha256::digest(compressed);

    // Step 2: RIPEMD160 of the SHA256 hash
    let mut ripemd = Ripemd160::new();
    ripemd.update(sha256_hash);
    let pubkey_hash = ripemd.finalize();

    // Step 3: Prepend the Dogecoin mainnet version byte
    let mut payload = Vec::with_capacity(21);
    payload.push(DOGE_MAINNET_ADDR_VERSION);
    payload.extend_from_slice(&pubkey_hash);

    // Step 4: Base58Check encode (adds 4-byte SHA256d checksum + encodes)
    let address = bs58::encode(payload)
        .with_check()
        .into_string();

    Ok(address)
}

/// Encode a secret key as Wallet Import Format (WIF).
///
/// WIF format for Dogecoin compressed keys:
/// 1. Prepend version byte 0x9E
/// 2. Append 32-byte raw secret key bytes
/// 3. Append 0x01 (compressed flag)
/// 4. Base58Check encode
fn secret_key_to_wif(secret_key: &SecretKey) -> Result<String> {
    let mut payload = Vec::with_capacity(34);
    payload.push(DOGE_MAINNET_WIF_VERSION);
    payload.extend_from_slice(&secret_key.secret_bytes());
    payload.push(0x01); // compressed public key flag

    Ok(bs58::encode(payload).with_check().into_string())
}

/// v0.3.6 — Import an existing wallet from a WIF-encoded private key.
///
/// Validates version byte, length, and compression flag.
/// Returns a `WalletInfo` with derived address + public key.
pub fn import_wif(wif: &str) -> Result<WalletInfo> {
    let decoded = bs58::decode(wif)
        .with_check(None)
        .into_vec()
        .map_err(|e| anyhow::anyhow!("Invalid WIF encoding: {}", e))?;

    if decoded.len() != 34 {
        anyhow::bail!(
            "Invalid WIF length: expected 34 bytes, got {}. Only compressed Dogecoin keys (starting with 'Q') are supported.",
            decoded.len()
        );
    }
    if decoded[0] != DOGE_MAINNET_WIF_VERSION {
        anyhow::bail!(
            "Not a Dogecoin mainnet WIF key (expected version 0x{:02X}, got 0x{:02X}). Keys should start with 'Q'.",
            DOGE_MAINNET_WIF_VERSION, decoded[0]
        );
    }
    if decoded[33] != 0x01 {
        anyhow::bail!("Only compressed keys are supported (compression flag must be 0x01).");
    }

    let secp = Secp256k1::new();
    let secret_key = SecretKey::from_slice(&decoded[1..33])
        .map_err(|e| anyhow::anyhow!("Invalid secret key bytes: {}", e))?;
    let public_key = PublicKey::from_secret_key(&secp, &secret_key);

    let address = pubkey_to_address(&public_key)?;
    let public_key_hex = hex::encode(public_key.serialize());
    let private_key_wif = secret_key_to_wif(&secret_key)?;

    Ok(WalletInfo { address, public_key_hex, private_key_wif })
}

/// Validate that a string is a valid Dogecoin address.
///
/// Checks:
/// 1. Base58Check decodes successfully
/// 2. Version byte is 0x1E (mainnet P2PKH)
/// 3. Total decoded length is 21 bytes (1 version + 20 hash)
pub fn is_valid_address(address: &str) -> bool {
    match bs58::decode(address).with_check(None).into_vec() {
        Ok(decoded) => {
            decoded.len() == 21 && decoded[0] == DOGE_MAINNET_ADDR_VERSION
        }
        Err(_) => false,
    }
}

/// Encode a transaction as a binary payload for the LoRa packet.
///
/// Format (little-endian where applicable):
///   [8 bytes]  amount in koinus (1 DOGE = 1e8 koinus)
///   [1 byte]   address length
///   [N bytes]  recipient address (ASCII)
///   [1 byte]   memo length (0 if no memo)
///   [M bytes]  memo (UTF-8, optional)
///
/// This matches the packet structure expected by RadioDogeSharp's DogePacketGenerator.
pub fn encode_transaction_payload(
    to_address: &str,
    amount_doge: f64,
    memo: Option<&str>,
) -> Result<Vec<u8>> {
    // Convert DOGE to koinus (satoshi equivalent), round to nearest.
    // Guard against NaN/Infinity before cast — while the frontend validates and Tauri IPC
    // rejects non-finite JSON floats, the backend should not trust the caller.
    if !amount_doge.is_finite() || amount_doge < 0.0 {
        anyhow::bail!("Invalid amount: {}", amount_doge);
    }
    let koinus: u64 = (amount_doge * 1e8).round() as u64;

    let addr_bytes = to_address.as_bytes();
    if addr_bytes.len() > 255 {
        anyhow::bail!("Address too long");
    }

    let memo_bytes = memo.unwrap_or("").as_bytes();
    let memo_len = memo_bytes.len().min(255);

    let mut payload = Vec::new();
    payload.extend_from_slice(&koinus.to_le_bytes()); // 8 bytes
    payload.push(addr_bytes.len() as u8);
    payload.extend_from_slice(addr_bytes);
    payload.push(memo_len as u8);
    if memo_len > 0 {
        payload.extend_from_slice(&memo_bytes[..memo_len]);
    }

    Ok(payload)
}

// ─── Real P2PKH transaction signing + broadcast ───────────────────────────────

const BLOCKBOOK_BASE: &str = "https://doge1.trezor.io/api/v2";

/// Default transaction fee.  1 DOGE covers any plausible tx size on the Dogecoin network.
pub const DEFAULT_TX_FEE_DOGE: f64 = 1.0;

#[derive(serde::Deserialize)]
struct UtxoEntry {
    txid: String,
    vout: u32,
    value: String, // koinus as string (avoids f64 precision loss for large values)
}

fn encode_varint(n: u64) -> Vec<u8> {
    if n < 0xfd {
        vec![n as u8]
    } else if n <= 0xffff {
        let mut v = vec![0xfd_u8];
        v.extend_from_slice(&(n as u16).to_le_bytes());
        v
    } else if n <= 0xffff_ffff {
        let mut v = vec![0xfe_u8];
        v.extend_from_slice(&(n as u32).to_le_bytes());
        v
    } else {
        let mut v = vec![0xff_u8];
        v.extend_from_slice(&n.to_le_bytes());
        v
    }
}

fn sha256d(data: &[u8]) -> [u8; 32] {
    let first = Sha256::digest(data);
    Sha256::digest(first).into()
}

/// P2PKH scriptPubKey for a Dogecoin address (25 bytes).
/// Layout: OP_DUP OP_HASH160 <20-byte pubkey hash> OP_EQUALVERIFY OP_CHECKSIG
fn p2pkh_script(address: &str) -> Result<Vec<u8>> {
    let decoded = bs58::decode(address)
        .with_check(None)
        .into_vec()
        .map_err(|e| anyhow::anyhow!("Invalid address '{}': {}", address, e))?;
    if decoded.len() != 21 || decoded[0] != DOGE_MAINNET_ADDR_VERSION {
        anyhow::bail!("Not a valid Dogecoin mainnet P2PKH address: {}", address);
    }
    let mut s = Vec::with_capacity(25);
    s.push(0x76_u8); // OP_DUP
    s.push(0xa9_u8); // OP_HASH160
    s.push(0x14_u8); // push 20 bytes
    s.extend_from_slice(&decoded[1..21]);
    s.push(0x88_u8); // OP_EQUALVERIFY
    s.push(0xac_u8); // OP_CHECKSIG
    Ok(s)
}

/// SIGHASH_ALL preimage for one input.
/// All inputs are assumed to be from the same P2PKH address (`utxo_script`).
fn sighash_preimage(
    inputs: &[([u8; 32], u32)], // (txid LE, vout)
    outputs: &[(u64, &[u8])],   // (koinus, scriptPubKey)
    signing_idx: usize,
    utxo_script: &[u8],
) -> Vec<u8> {
    let mut pre = Vec::new();
    pre.extend_from_slice(&1u32.to_le_bytes()); // version
    pre.extend_from_slice(&encode_varint(inputs.len() as u64));
    for (i, (txid_le, vout)) in inputs.iter().enumerate() {
        pre.extend_from_slice(txid_le);
        pre.extend_from_slice(&vout.to_le_bytes());
        if i == signing_idx {
            pre.extend_from_slice(&encode_varint(utxo_script.len() as u64));
            pre.extend_from_slice(utxo_script);
        } else {
            pre.push(0x00); // empty scriptSig for all other inputs
        }
        pre.extend_from_slice(&0xffff_ffffu32.to_le_bytes()); // sequence
    }
    pre.extend_from_slice(&encode_varint(outputs.len() as u64));
    for (value, script) in outputs {
        pre.extend_from_slice(&value.to_le_bytes());
        pre.extend_from_slice(&encode_varint(script.len() as u64));
        pre.extend_from_slice(script);
    }
    pre.extend_from_slice(&0u32.to_le_bytes()); // locktime
    pre.extend_from_slice(&1u32.to_le_bytes()); // SIGHASH_ALL = 1
    pre
}

async fn fetch_utxos_blockbook(address: &str) -> Result<Vec<UtxoEntry>> {
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(20))
        .build()
        .map_err(|e| anyhow::anyhow!("HTTP client init failed: {}", e))?;
    let url = format!("{}/utxo/{}", BLOCKBOOK_BASE, address);
    let utxos: Vec<UtxoEntry> = client
        .get(&url)
        .send()
        .await
        .map_err(|e| anyhow::anyhow!("UTXO fetch failed: {}", e))?
        .json()
        .await
        .map_err(|e| anyhow::anyhow!("UTXO response parse failed: {}", e))?;
    Ok(utxos)
}

/// Build and sign a Dogecoin P2PKH transaction, returning the raw serialized bytes.
///
/// Fetches UTXOs from Trezor Blockbook, selects coins with a greedy algorithm
/// (largest first), builds inputs and outputs (including change), signs each
/// input with SIGHASH_ALL.
///
/// Pass `wallet::DEFAULT_TX_FEE_DOGE` (1.0) for the fee.
/// To broadcast the result call `wallet::broadcast_raw_tx(&hex::encode(&tx))`.
pub async fn build_signed_transaction(
    from_wif: &str,
    to_address: &str,
    amount_doge: f64,
    fee_doge: f64,
) -> Result<Vec<u8>> {
    if !amount_doge.is_finite() || amount_doge <= 0.0 {
        anyhow::bail!("Invalid send amount: {}", amount_doge);
    }

    // Decode WIF → secret key + compressed pubkey + from_address
    let decoded = bs58::decode(from_wif)
        .with_check(None)
        .into_vec()
        .map_err(|e| anyhow::anyhow!("Invalid WIF encoding: {}", e))?;
    if decoded.len() != 34 || decoded[0] != DOGE_MAINNET_WIF_VERSION || decoded[33] != 0x01 {
        anyhow::bail!("Invalid Dogecoin compressed WIF key (must start with 'Q')");
    }
    let secp = Secp256k1::new();
    let secret_key = SecretKey::from_slice(&decoded[1..33])
        .map_err(|e| anyhow::anyhow!("Invalid key bytes: {}", e))?;
    let public_key = PublicKey::from_secret_key(&secp, &secret_key);
    let compressed_pubkey = public_key.serialize(); // 33 bytes
    let from_address = pubkey_to_address(&public_key)?;

    let amount_koinus: u64 = (amount_doge * 1e8).round() as u64;
    let fee_koinus: u64 = (fee_doge.abs() * 1e8).round() as u64;
    let total_needed = amount_koinus
        .checked_add(fee_koinus)
        .ok_or_else(|| anyhow::anyhow!("Amount + fee overflow"))?;

    // Fetch UTXOs and greedily select coins (largest first)
    let raw_utxos = fetch_utxos_blockbook(&from_address).await?;
    if raw_utxos.is_empty() {
        anyhow::bail!("No UTXOs found for {}. Balance may be zero.", from_address);
    }
    let mut utxos: Vec<(String, u32, u64)> = raw_utxos
        .into_iter()
        .filter_map(|u| u.value.parse::<u64>().ok().map(|v| (u.txid, u.vout, v)))
        .collect();
    utxos.sort_by(|a, b| b.2.cmp(&a.2)); // largest first

    let mut selected: Vec<(String, u32)> = Vec::new();
    let mut selected_sum: u64 = 0;
    for (txid, vout, value) in &utxos {
        selected.push((txid.clone(), *vout));
        selected_sum = selected_sum.saturating_add(*value);
        if selected_sum >= total_needed {
            break;
        }
    }
    if selected_sum < total_needed {
        anyhow::bail!(
            "Insufficient funds: need {:.8} DOGE, spendable {:.8} DOGE",
            total_needed as f64 / 1e8,
            selected_sum as f64 / 1e8
        );
    }

    // Build output and signing scripts
    let to_script = p2pkh_script(to_address)?;
    let from_script = p2pkh_script(&from_address)?; // also used as UTXO script for signing

    let change = selected_sum - total_needed;
    let mut outputs: Vec<(u64, Vec<u8>)> = vec![(amount_koinus, to_script)];
    if change > 0 {
        outputs.push((change, from_script.clone()));
    }

    // Decode txids from display hex (big-endian) to wire format (little-endian)
    let mut input_txids: Vec<([u8; 32], u32)> = Vec::with_capacity(selected.len());
    for (txid_hex, vout) in &selected {
        let mut b = hex::decode(txid_hex)
            .map_err(|e| anyhow::anyhow!("Invalid txid '{}': {}", txid_hex, e))?;
        if b.len() != 32 {
            anyhow::bail!("Malformed txid length: {}", txid_hex);
        }
        b.reverse(); // display is big-endian; wire format is little-endian
        let mut arr = [0u8; 32];
        arr.copy_from_slice(&b);
        input_txids.push((arr, *vout));
    }

    // Sign each input (SIGHASH_ALL)
    let outputs_ref: Vec<(u64, &[u8])> =
        outputs.iter().map(|(v, s)| (*v, s.as_slice())).collect();
    let mut script_sigs: Vec<Vec<u8>> = Vec::with_capacity(input_txids.len());
    for i in 0..input_txids.len() {
        let preimage = sighash_preimage(&input_txids, &outputs_ref, i, &from_script);
        let hash = sha256d(&preimage);
        let msg = secp256k1::Message::from_digest(hash);
        let sig = secp.sign_ecdsa(&msg, &secret_key);
        let mut der_sig = sig.serialize_der().to_vec();
        der_sig.push(0x01); // SIGHASH_ALL type byte

        // scriptSig: <push sig+hashtype> <push compressed pubkey>
        let mut ss = Vec::new();
        ss.push(der_sig.len() as u8); // push opcode = byte length (always ≤73)
        ss.extend_from_slice(&der_sig);
        ss.push(compressed_pubkey.len() as u8); // 0x21 = 33 bytes
        ss.extend_from_slice(&compressed_pubkey);
        script_sigs.push(ss);
    }

    // Serialize the final transaction
    let mut tx = Vec::new();
    tx.extend_from_slice(&1u32.to_le_bytes()); // version = 1
    tx.extend_from_slice(&encode_varint(input_txids.len() as u64));
    for (i, (txid_le, vout)) in input_txids.iter().enumerate() {
        tx.extend_from_slice(txid_le);
        tx.extend_from_slice(&vout.to_le_bytes());
        let ss = &script_sigs[i];
        tx.extend_from_slice(&encode_varint(ss.len() as u64));
        tx.extend_from_slice(ss);
        tx.extend_from_slice(&0xffff_ffffu32.to_le_bytes()); // sequence = MAX (not RBF)
    }
    tx.extend_from_slice(&encode_varint(outputs.len() as u64));
    for (value, script) in &outputs {
        tx.extend_from_slice(&value.to_le_bytes());
        tx.extend_from_slice(&encode_varint(script.len() as u64));
        tx.extend_from_slice(script);
    }
    tx.extend_from_slice(&0u32.to_le_bytes()); // locktime = 0

    Ok(tx)
}

/// Broadcast a raw signed Dogecoin transaction to the network via Trezor Blockbook.
/// `raw_hex` is the hex-encoded serialized transaction (output of
/// `hex::encode(build_signed_transaction(...))`).
/// Returns the txid string on success.
pub async fn broadcast_raw_tx(raw_hex: &str) -> Result<String> {
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(20))
        .build()
        .map_err(|e| anyhow::anyhow!("HTTP client init failed: {}", e))?;
    let url = format!("{}/sendtx/", BLOCKBOOK_BASE);
    let resp: serde_json::Value = client
        .post(&url)
        .header("Content-Type", "text/plain")
        .body(raw_hex.to_string())
        .send()
        .await
        .map_err(|e| anyhow::anyhow!("Broadcast request failed: {}", e))?
        .json()
        .await
        .map_err(|e| anyhow::anyhow!("Broadcast response parse failed: {}", e))?;

    if let Some(txid) = resp.get("result").and_then(|v| v.as_str()) {
        Ok(txid.to_string())
    } else if let Some(err) = resp.get("error").and_then(|v| v.as_str()) {
        anyhow::bail!("Network rejected transaction: {}", err)
    } else {
        anyhow::bail!("Unexpected broadcast response: {}", resp)
    }
}

// ─── Wallet encryption at rest ────────────────────────────────────────────────

/// Encrypted wallet file written to `wallet.json` (v0.3.16+).
/// The WIF private key is encrypted with ChaCha20-Poly1305 (16-byte auth tag included),
/// key-derived from the user's passphrase via argon2id.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct EncryptedWalletFile {
    pub address: String,
    pub public_key_hex: String,
    /// Ciphertext + 16-byte Poly1305 authentication tag, hex-encoded.
    pub encrypted_wif: String,
    /// Argon2id salt (16 bytes), hex-encoded.
    pub salt: String,
    /// ChaCha20-Poly1305 nonce (12 bytes), hex-encoded.
    pub nonce: String,
}

/// Encrypt a wallet for storage on disk.
/// Uses argon2id (64 MiB, 2 iterations) for key derivation and ChaCha20-Poly1305 for AEAD.
/// Passphrase must be at least 1 byte; enforce a minimum length in the calling layer.
pub fn encrypt_wallet(wallet: &WalletInfo, passphrase: &str) -> Result<EncryptedWalletFile> {
    let mut salt = [0u8; 16];
    let mut nonce_bytes = [0u8; 12];
    OsRng.fill_bytes(&mut salt);
    OsRng.fill_bytes(&mut nonce_bytes);

    let key = derive_encryption_key(passphrase, &salt)?;
    let cipher = ChaCha20Poly1305::new(Key::from_slice(&key));
    let nonce = Nonce::from_slice(&nonce_bytes);
    let mut buffer: Vec<u8> = wallet.private_key_wif.as_bytes().to_vec();
    cipher.encrypt_in_place(nonce, b"", &mut buffer)
        .map_err(|_| anyhow::anyhow!("encryption failed"))?;

    Ok(EncryptedWalletFile {
        address: wallet.address.clone(),
        public_key_hex: wallet.public_key_hex.clone(),
        encrypted_wif: hex::encode(&buffer),
        salt: hex::encode(salt),
        nonce: hex::encode(nonce_bytes),
    })
}

/// Decrypt a wallet loaded from disk.
/// Returns `Err` with a user-facing message if the passphrase is wrong or the file is corrupted.
pub fn decrypt_wallet(encrypted: &EncryptedWalletFile, passphrase: &str) -> Result<WalletInfo> {
    let salt = hex::decode(&encrypted.salt)
        .map_err(|_| anyhow::anyhow!("corrupted wallet: invalid salt encoding"))?;
    let nonce_bytes = hex::decode(&encrypted.nonce)
        .map_err(|_| anyhow::anyhow!("corrupted wallet: invalid nonce encoding"))?;
    let mut buffer = hex::decode(&encrypted.encrypted_wif)
        .map_err(|_| anyhow::anyhow!("corrupted wallet: invalid ciphertext encoding"))?;

    let key = derive_encryption_key(passphrase, &salt)?;
    let cipher = ChaCha20Poly1305::new(Key::from_slice(&key));
    let nonce = Nonce::from_slice(&nonce_bytes);
    cipher.decrypt_in_place(nonce, b"", &mut buffer)
        .map_err(|_| anyhow::anyhow!("Wrong passphrase or corrupted wallet file."))?;

    let wif = String::from_utf8(buffer)
        .map_err(|_| anyhow::anyhow!("corrupted wallet: decrypted data is not valid UTF-8"))?;
    let info = import_wif(&wif)?;
    if info.address != encrypted.address {
        anyhow::bail!("Wallet integrity check failed: decrypted key does not match stored address.");
    }
    Ok(info)
}

fn derive_encryption_key(passphrase: &str, salt: &[u8]) -> Result<[u8; 32]> {
    let params = Params::new(65536, 2, 1, Some(32))
        .map_err(|e| anyhow::anyhow!("argon2 params error: {}", e))?;
    let argon2 = Argon2::new(Algorithm::Argon2id, Version::V0x13, params);
    let mut key = [0u8; 32];
    argon2.hash_password_into(passphrase.as_bytes(), salt, &mut key)
        .map_err(|e| anyhow::anyhow!("key derivation failed: {}", e))?;
    Ok(key)
}

/// Return true if a CMD_DOGE_TX payload looks like a raw signed Dogecoin transaction
/// rather than the legacy stub format (amount + address + memo bytes).
/// A valid Dogecoin v1 tx starts with `[0x01, 0x00, 0x00, 0x00]` and is ≥ 100 bytes.
pub fn is_signed_tx_payload(payload: &[u8]) -> bool {
    payload.len() >= 100 && payload.starts_with(&[0x01, 0x00, 0x00, 0x00])
}

// ─── Incoming transaction verification ────────────────────────────────────────

/// Parse a varint from `buf` at position `pos`, advancing `pos`.
fn parse_varint_at(buf: &[u8], pos: &mut usize) -> Option<u64> {
    let first = *buf.get(*pos)?;
    *pos += 1;
    match first {
        0..=0xfc => Some(first as u64),
        0xfd => {
            let bytes: [u8; 2] = buf.get(*pos..*pos + 2)?.try_into().ok()?;
            *pos += 2;
            Some(u16::from_le_bytes(bytes) as u64)
        }
        0xfe => {
            let bytes: [u8; 4] = buf.get(*pos..*pos + 4)?.try_into().ok()?;
            *pos += 4;
            Some(u32::from_le_bytes(bytes) as u64)
        }
        _ => {
            let bytes: [u8; 8] = buf.get(*pos..*pos + 8)?.try_into().ok()?;
            *pos += 8;
            Some(u64::from_le_bytes(bytes))
        }
    }
}

/// Extract (sig_der, hashtype, compressed_pubkey) from a P2PKH scriptSig.
fn parse_p2pkh_scriptsig(ss: &[u8]) -> Result<(Vec<u8>, u8, [u8; 33])> {
    if ss.len() < 4 {
        anyhow::bail!("scriptSig too short");
    }
    let sig_push = ss[0] as usize;
    if ss.len() < 1 + sig_push + 2 {
        anyhow::bail!("scriptSig truncated at signature");
    }
    let sig_with_ht = &ss[1..1 + sig_push];
    if sig_with_ht.is_empty() {
        anyhow::bail!("empty signature push");
    }
    let hashtype = *sig_with_ht.last().unwrap();
    let sig_der = sig_with_ht[..sig_with_ht.len() - 1].to_vec();

    let pubkey_push = ss[1 + sig_push] as usize;
    if pubkey_push != 33 {
        anyhow::bail!("expected 33-byte compressed pubkey, got {} bytes", pubkey_push);
    }
    if ss.len() < 1 + sig_push + 1 + 33 {
        anyhow::bail!("scriptSig truncated at pubkey");
    }
    let mut pubkey = [0u8; 33];
    pubkey.copy_from_slice(&ss[1 + sig_push + 1..1 + sig_push + 1 + 33]);
    Ok((sig_der, hashtype, pubkey))
}

/// Derive a P2PKH scriptPubKey directly from a public key (for sighash construction).
fn p2pkh_script_from_pubkey(pubkey: &PublicKey) -> Vec<u8> {
    let compressed = pubkey.serialize();
    let sha_hash = Sha256::digest(compressed);
    let mut ripemd = Ripemd160::new();
    ripemd.update(sha_hash);
    let pubkey_hash = ripemd.finalize();
    let mut s = Vec::with_capacity(25);
    s.push(0x76); // OP_DUP
    s.push(0xa9); // OP_HASH160
    s.push(0x14); // push 20 bytes
    s.extend_from_slice(&pubkey_hash);
    s.push(0x88); // OP_EQUALVERIFY
    s.push(0xac); // OP_CHECKSIG
    s
}

/// Extract a Dogecoin address from a standard P2PKH scriptPubKey.
fn p2pkh_address_from_script(script: &[u8]) -> Option<String> {
    if script.len() == 25
        && script[0] == 0x76
        && script[1] == 0xa9
        && script[2] == 0x14
        && script[23] == 0x88
        && script[24] == 0xac
    {
        let mut payload = Vec::with_capacity(21);
        payload.push(DOGE_MAINNET_ADDR_VERSION);
        payload.extend_from_slice(&script[3..23]);
        Some(bs58::encode(payload).with_check().into_string())
    } else {
        None
    }
}

/// Verify all input signatures in a raw Dogecoin P2PKH transaction (no network access).
/// Returns `(sender_address, recipients)` where recipients is a list of `(koinus, address)`.
/// All inputs are assumed to be P2PKH from the same address; the UTXO scriptPubKey is
/// reconstructed from the pubkey in each input's scriptSig.
pub fn verify_signed_tx(raw_tx: &[u8]) -> Result<(String, Vec<(u64, String)>)> {
    let mut pos = 0usize;

    // Version (4 bytes)
    if raw_tx.len() < 4 {
        anyhow::bail!("transaction too short");
    }
    pos += 4;

    // Inputs
    let input_count = parse_varint_at(raw_tx, &mut pos)
        .ok_or_else(|| anyhow::anyhow!("failed to parse input count"))? as usize;
    if input_count == 0 || input_count > 200 {
        anyhow::bail!("invalid input count: {}", input_count);
    }
    let mut inputs: Vec<([u8; 32], u32, Vec<u8>)> = Vec::with_capacity(input_count);
    for _ in 0..input_count {
        let txid: [u8; 32] = raw_tx
            .get(pos..pos + 32)
            .and_then(|s| s.try_into().ok())
            .ok_or_else(|| anyhow::anyhow!("truncated txid"))?;
        pos += 32;
        let vout = u32::from_le_bytes(
            raw_tx.get(pos..pos + 4)
                .and_then(|s| s.try_into().ok())
                .ok_or_else(|| anyhow::anyhow!("truncated vout"))?,
        );
        pos += 4;
        let ss_len = parse_varint_at(raw_tx, &mut pos)
            .ok_or_else(|| anyhow::anyhow!("failed to parse scriptSig length"))? as usize;
        let ss = raw_tx.get(pos..pos + ss_len)
            .ok_or_else(|| anyhow::anyhow!("truncated scriptSig"))?
            .to_vec();
        pos += ss_len;
        pos += 4; // sequence
        inputs.push((txid, vout, ss));
    }

    // Outputs
    let output_count = parse_varint_at(raw_tx, &mut pos)
        .ok_or_else(|| anyhow::anyhow!("failed to parse output count"))? as usize;
    if output_count == 0 || output_count > 200 {
        anyhow::bail!("invalid output count: {}", output_count);
    }
    let mut outputs: Vec<(u64, Vec<u8>)> = Vec::with_capacity(output_count);
    for _ in 0..output_count {
        let value = u64::from_le_bytes(
            raw_tx.get(pos..pos + 8)
                .and_then(|s| s.try_into().ok())
                .ok_or_else(|| anyhow::anyhow!("truncated output value"))?,
        );
        pos += 8;
        let spk_len = parse_varint_at(raw_tx, &mut pos)
            .ok_or_else(|| anyhow::anyhow!("failed to parse scriptPubKey length"))? as usize;
        let spk = raw_tx.get(pos..pos + spk_len)
            .ok_or_else(|| anyhow::anyhow!("truncated scriptPubKey"))?
            .to_vec();
        pos += spk_len;
        outputs.push((value, spk));
    }

    // Verify each input's ECDSA signature
    let secp = Secp256k1::verification_only();
    let mut sender_address: Option<String> = None;
    let all_inputs_for_sighash: Vec<([u8; 32], u32)> =
        inputs.iter().map(|(txid, vout, _)| (*txid, *vout)).collect();
    let outputs_for_sighash: Vec<(u64, &[u8])> =
        outputs.iter().map(|(v, s)| (*v, s.as_slice())).collect();

    for (i, (_, _, scriptsig)) in inputs.iter().enumerate() {
        let (sig_der, hashtype, pubkey_bytes) = parse_p2pkh_scriptsig(scriptsig)?;
        if hashtype != 0x01 {
            anyhow::bail!("input {} uses unsupported SIGHASH type 0x{:02x}", i, hashtype);
        }
        let pubkey = PublicKey::from_slice(&pubkey_bytes)
            .map_err(|e| anyhow::anyhow!("input {} invalid pubkey: {}", i, e))?;
        let utxo_script = p2pkh_script_from_pubkey(&pubkey);
        let preimage = sighash_preimage(&all_inputs_for_sighash, &outputs_for_sighash, i, &utxo_script);
        let hash = sha256d(&preimage);
        let msg = secp256k1::Message::from_digest(hash);
        let sig = secp256k1::ecdsa::Signature::from_der(&sig_der)
            .map_err(|e| anyhow::anyhow!("input {} invalid DER sig: {}", i, e))?;
        secp.verify_ecdsa(&msg, &sig, &pubkey)
            .map_err(|_| anyhow::anyhow!("input {} signature verification failed", i))?;
        if sender_address.is_none() {
            sender_address = pubkey_to_address(&pubkey).ok();
        }
    }

    let sender = sender_address.ok_or_else(|| anyhow::anyhow!("no verified sender"))?;
    let recipients: Vec<(u64, String)> = outputs
        .iter()
        .filter_map(|(v, spk)| p2pkh_address_from_script(spk).map(|addr| (*v, addr)))
        .filter(|(_, addr)| addr != &sender)
        .collect();

    Ok((sender, recipients))
}

/// Describe an incoming signed Dogecoin transaction payload for display.
/// Returns a human-readable string with verification status, amount, and addresses.
/// Falls back to an "unverified" message if the transaction cannot be parsed or verified.
pub fn describe_signed_tx(raw_tx: &[u8]) -> String {
    match verify_signed_tx(raw_tx) {
        Ok((sender, recipients)) => {
            if recipients.is_empty() {
                format!("✅ DOGE TX (verified) — {} → self/change only", sender)
            } else {
                let parts: Vec<String> = recipients
                    .iter()
                    .map(|(v, addr)| format!("{:.8} DOGE → {}", *v as f64 / 1e8, addr))
                    .collect();
                format!("✅ DOGE TX: {} [from {}]", parts.join(", "), sender)
            }
        }
        Err(_) => format!("⚠️ DOGE TX (unverified) — {} bytes", raw_tx.len()),
    }
}

/// Decode an incoming transaction payload from a LoRa packet.
/// Returns a human-readable description.
pub fn decode_transaction_payload(payload: &[u8]) -> Option<String> {
    if payload.len() < 10 {
        return None;
    }

    let koinus = u64::from_le_bytes(payload[..8].try_into().ok()?);
    let amount_doge = koinus as f64 / 1e8;

    let addr_len = payload[8] as usize;
    if payload.len() < 9 + addr_len {
        return None;
    }

    let addr = std::str::from_utf8(&payload[9..9 + addr_len]).ok()?;
    let rest = &payload[9 + addr_len..];

    if rest.is_empty() {
        Some(format!("DOGE TX: {:.8} DOGE → {}", amount_doge, addr))
    } else {
        let memo_len = rest[0] as usize;
        let memo = if memo_len > 0 && rest.len() > memo_len {
            std::str::from_utf8(&rest[1..1 + memo_len]).unwrap_or("?")
        } else {
            ""
        };
        if memo.is_empty() {
            Some(format!("DOGE TX: {:.8} DOGE → {}", amount_doge, addr))
        } else {
            Some(format!("DOGE TX: {:.8} DOGE → {} [{}]", amount_doge, addr, memo))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_keypair() {
        let wallet = generate_keypair().expect("keypair generation should succeed");
        // Dogecoin mainnet addresses start with "D"
        assert!(wallet.address.starts_with('D'), "address should start with D, got: {}", wallet.address);
        // Compressed WIF for Dogecoin mainnet starts with "Q"
        assert!(wallet.private_key_wif.starts_with('Q'), "WIF should start with Q, got: {}", wallet.private_key_wif);
        // Compressed pubkey hex = 33 bytes = 66 hex chars
        assert_eq!(wallet.public_key_hex.len(), 66);
    }

    #[test]
    fn test_address_validation() {
        let wallet = generate_keypair().expect("keypair generation should succeed");
        assert!(is_valid_address(&wallet.address));
        assert!(!is_valid_address("not-an-address"));
        assert!(!is_valid_address("1BitcoinAddress...")); // Bitcoin address, wrong version
    }

    #[test]
    fn test_encode_decode_transaction() {
        let payload = encode_transaction_payload(
            "DH5yaieqoZN36fDVciNyRueRGvGLR3mr7L",
            100.0,
            Some("much wow"),
        ).expect("encode should succeed");

        let decoded = decode_transaction_payload(&payload).expect("decode should succeed");
        assert!(decoded.contains("100.00000000"));
        assert!(decoded.contains("much wow"));
    }

    #[test]
    fn test_wallet_encrypt_decrypt_roundtrip() {
        let wallet = generate_keypair().expect("keypair generation should succeed");
        let passphrase = "test-passphrase-12345";
        let encrypted = encrypt_wallet(&wallet, passphrase).expect("encryption should succeed");
        // encrypted_wif should not contain the plaintext WIF
        assert!(!encrypted.encrypted_wif.contains(&wallet.private_key_wif));
        // Decryption with correct passphrase should recover the original wallet
        let recovered = decrypt_wallet(&encrypted, passphrase).expect("decryption should succeed");
        assert_eq!(recovered.address, wallet.address);
        assert_eq!(recovered.private_key_wif, wallet.private_key_wif);
        // Decryption with wrong passphrase should fail
        let bad = decrypt_wallet(&encrypted, "wrong-passphrase");
        assert!(bad.is_err(), "wrong passphrase should fail");
    }

    #[test]
    fn test_tx_verification_on_manually_built_tx() {
        // Build a self-contained signed transaction and verify it.
        let sender = generate_keypair().expect("keypair generation should succeed");
        let recipient = generate_keypair().expect("keypair generation should succeed");

        // Build a fake 1-input transaction manually (UTXO from self)
        let decoded_wif = bs58::decode(&sender.private_key_wif)
            .with_check(None).into_vec().unwrap();
        let secp = Secp256k1::new();
        let secret_key = SecretKey::from_slice(&decoded_wif[1..33]).unwrap();
        let public_key = secp256k1::PublicKey::from_secret_key(&secp, &secret_key);

        let utxo_script = p2pkh_script_from_pubkey(&public_key);
        let to_script = p2pkh_script(&recipient.address).unwrap();

        // Fake txid (32 bytes LE), vout 0
        let txid_le = [0xab_u8; 32];
        let inputs = vec![(txid_le, 0u32)];
        let amount_koinus = 100_000_000u64; // 1 DOGE
        let outputs: Vec<(u64, &[u8])> = vec![(amount_koinus, to_script.as_slice())];

        let preimage = sighash_preimage(&inputs, &outputs, 0, &utxo_script);
        let hash = sha256d(&preimage);
        let msg = secp256k1::Message::from_digest(hash);
        let sig = secp.sign_ecdsa(&msg, &secret_key);
        let mut der_sig = sig.serialize_der().to_vec();
        der_sig.push(0x01); // SIGHASH_ALL

        let compressed_pubkey = public_key.serialize();
        let mut scriptsig = Vec::new();
        scriptsig.push(der_sig.len() as u8);
        scriptsig.extend_from_slice(&der_sig);
        scriptsig.push(33u8);
        scriptsig.extend_from_slice(&compressed_pubkey);

        // Serialize the transaction
        let mut tx = Vec::new();
        tx.extend_from_slice(&1u32.to_le_bytes()); // version
        tx.extend_from_slice(&encode_varint(1)); // 1 input
        tx.extend_from_slice(&txid_le);
        tx.extend_from_slice(&0u32.to_le_bytes()); // vout
        tx.extend_from_slice(&encode_varint(scriptsig.len() as u64));
        tx.extend_from_slice(&scriptsig);
        tx.extend_from_slice(&0xffff_ffffu32.to_le_bytes()); // sequence
        tx.extend_from_slice(&encode_varint(1)); // 1 output
        tx.extend_from_slice(&amount_koinus.to_le_bytes());
        tx.extend_from_slice(&encode_varint(to_script.len() as u64));
        tx.extend_from_slice(&to_script);
        tx.extend_from_slice(&0u32.to_le_bytes()); // locktime

        let (verified_sender, recipients) = verify_signed_tx(&tx)
            .expect("verification should succeed for a properly signed transaction");
        assert_eq!(verified_sender, sender.address, "sender address should match");
        assert_eq!(recipients.len(), 1, "should have 1 recipient");
        assert_eq!(recipients[0].0, amount_koinus, "amount should match");
        assert_eq!(recipients[0].1, recipient.address, "recipient address should match");

        // describe_signed_tx should return a verified description
        let desc = describe_signed_tx(&tx);
        assert!(desc.starts_with("✅"), "should start with verified checkmark");
        assert!(desc.contains("1.00000000"), "should contain the amount");
    }
}
