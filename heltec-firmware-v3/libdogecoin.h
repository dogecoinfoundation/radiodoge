/*
 * libdogecoin ESP32 - Compatible API
 * 
 * This is an ESP32-native implementation of libdogecoin's address generation API.
 * It uses ESP32's hardware RNG and mbedTLS for cryptographic operations.
 * 
 * API matches: https://github.com/dogecoinfoundation/libdogecoin
 */

#ifndef LIBDOGECOIN_ESP32_H
#define LIBDOGECOIN_ESP32_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Key length constants (matching libdogecoin)
#define PRIVKEYWIFLEN 53    // WIF private key length + null terminator
#define P2PKHLEN 35         // P2PKH address length + null terminator
#define PUBKEYHEXLEN 67     // Hex public key length + null terminator

// Type definition for compatibility
typedef uint8_t dogecoin_bool;

/**
 * @brief Initialize the ECC context. Must be called before any key operations.
 */
void dogecoin_ecc_start(void);

/**
 * @brief Destroy the ECC context. Call when done with key operations.
 */
void dogecoin_ecc_stop(void);

/**
 * @brief Generate a new private/public key pair.
 * 
 * @param wif_privkey Output buffer for WIF-encoded private key (min PRIVKEYWIFLEN bytes)
 * @param p2pkh_pubkey Output buffer for P2PKH address (min P2PKHLEN bytes)
 * @param is_testnet true for testnet, false for mainnet
 * @return 1 on success, 0 on failure
 */
int generatePrivPubKeypair(char* wif_privkey, char* p2pkh_pubkey, dogecoin_bool is_testnet);

/**
 * @brief Verify that a private key and address match.
 * 
 * @param wif_privkey The WIF-encoded private key
 * @param p2pkh_pubkey The P2PKH address to verify
 * @param is_testnet true for testnet, false for mainnet
 * @return 1 if they match, 0 if they don't
 */
int verifyPrivPubKeypair(const char* wif_privkey, const char* p2pkh_pubkey, dogecoin_bool is_testnet);

/**
 * @brief Verify that an address is valid.
 * 
 * @param p2pkh_pubkey The P2PKH address to verify
 * @param len Length of the address string
 * @return 1 if valid, 0 if invalid
 */
int verifyP2pkhAddress(const char* p2pkh_pubkey, size_t len);

/**
 * @brief Derive a Dogecoin address from a WIF private key.
 * 
 * @param wif_privkey The WIF-encoded private key
 * @param p2pkh_pubkey Output buffer for P2PKH address (min P2PKHLEN bytes)
 * @param is_testnet true for testnet, false for mainnet
 * @return 1 on success, 0 on failure
 */
int deriveAddressFromWIF(const char* wif_privkey, char* p2pkh_pubkey, dogecoin_bool is_testnet);

/**
 * @brief Get the moon string (fun function from libdogecoin)
 * @return The moon string
 */
const char* moon(void);

/**
 * @brief Get the moon emoji string
 * @return The moon emoji string
 */
const char* moonEmoji(void);

/**
 * @brief Get the current moon phase (0-7)
 * @return Moon phase number (0=new moon, 4=full moon, etc.)
 */
int moonPhase(void);

#ifdef __cplusplus
}
#endif

#endif // LIBDOGECOIN_ESP32_H

