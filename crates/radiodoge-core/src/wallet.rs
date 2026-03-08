//! Pure Rust Dogecoin wallet implementation.
//!
//! Dogecoin uses the same elliptic curve cryptography as Bitcoin (secp256k1),
//! with different network version bytes:
//!   - Mainnet P2PKH address version: 0x1E  → addresses start with "D"
//!   - Mainnet WIF private key version: 0x9E → compressed WIF starts with "Q"
//!
//! No FFI to libdogecoin is needed — everything is done in pure Rust.

use anyhow::Result;
use rand::rngs::OsRng;
use ripemd::{Digest as RipemdDigest, Ripemd160};
use secp256k1::{PublicKey, Secp256k1, SecretKey};
use sha2::{Digest as Sha2Digest, Sha256};

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
    // Convert DOGE to koinus (satoshi equivalent), round to nearest
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
}
