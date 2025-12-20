/*
 * libdogecoin ESP32 - Native Implementation
 * 
 * Uses ESP32's hardware RNG and mbedTLS for secp256k1 operations.
 * Implements the same API as the official libdogecoin library.
 */

#include "libdogecoin.h"
#include <Arduino.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <esp_random.h>
#include <string.h>
#include <time.h>

// Dogecoin network version bytes
#define DOGECOIN_MAINNET_PUBKEY_VERSION 0x1E   // 'D' prefix
#define DOGECOIN_MAINNET_PRIVKEY_VERSION 0x9E  // WIF prefix
#define DOGECOIN_TESTNET_PUBKEY_VERSION 0x71   // 'n' prefix  
#define DOGECOIN_TESTNET_PRIVKEY_VERSION 0xF1  // Testnet WIF prefix

// Base58 alphabet
static const char BASE58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Global ECC context
static bool ecc_initialized = false;

// ============================================================================
// RIPEMD-160 Implementation (not in mbedTLS on ESP32)
// ============================================================================

static const uint32_t RIPEMD160_K[5] = {0x00000000, 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xA953FD4E};
static const uint32_t RIPEMD160_KP[5] = {0x50A28BE6, 0x5C4DD124, 0x6D703EF3, 0x7A6D76E9, 0x00000000};
static const int RIPEMD160_R[80] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5, 2, 14, 11, 8,
    3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12,
    1, 9, 11, 10, 0, 8, 12, 4, 13, 3, 7, 15, 14, 5, 6, 2,
    4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13
};
static const int RIPEMD160_RP[80] = {
    5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12,
    6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12, 4, 9, 1, 2,
    15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13,
    8, 6, 4, 1, 3, 11, 15, 0, 5, 12, 2, 13, 9, 7, 10, 14,
    12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11
};
static const int RIPEMD160_S[80] = {
    11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8,
    7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15, 9, 11, 7, 13, 12,
    11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5,
    11, 12, 14, 15, 14, 15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12,
    9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6
};
static const int RIPEMD160_SP[80] = {
    8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6,
    9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12, 7, 6, 15, 13, 11,
    9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5,
    15, 5, 8, 11, 14, 14, 6, 14, 6, 9, 12, 9, 12, 5, 15, 8,
    8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11
};

static inline uint32_t rol(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static uint32_t f_rmd(int j, uint32_t x, uint32_t y, uint32_t z) {
    if (j < 16) return x ^ y ^ z;
    if (j < 32) return (x & y) | (~x & z);
    if (j < 48) return (x | ~y) ^ z;
    if (j < 64) return (x & z) | (y & ~z);
    return x ^ (y | ~z);
}

static void ripemd160_hash(const uint8_t* data, size_t len, uint8_t* digest) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    
    size_t padLen = ((len + 8) / 64 + 1) * 64;
    uint8_t* padded = (uint8_t*)calloc(padLen, 1);
    if (!padded) return;
    
    memcpy(padded, data, len);
    padded[len] = 0x80;
    uint64_t bitLen = len * 8;
    memcpy(padded + padLen - 8, &bitLen, 8);
    
    for (size_t i = 0; i < padLen; i += 64) {
        uint32_t X[16];
        for (int j = 0; j < 16; j++) {
            X[j] = padded[i + j*4] | (padded[i + j*4 + 1] << 8) |
                   (padded[i + j*4 + 2] << 16) | (padded[i + j*4 + 3] << 24);
        }
        
        uint32_t A = h0, B = h1, C = h2, D = h3, E = h4;
        uint32_t Ap = h0, Bp = h1, Cp = h2, Dp = h3, Ep = h4;
        
        for (int j = 0; j < 80; j++) {
            uint32_t T = rol(A + f_rmd(j, B, C, D) + X[RIPEMD160_R[j]] + RIPEMD160_K[j/16], RIPEMD160_S[j]) + E;
            A = E; E = D; D = rol(C, 10); C = B; B = T;
            
            T = rol(Ap + f_rmd(79-j, Bp, Cp, Dp) + X[RIPEMD160_RP[j]] + RIPEMD160_KP[j/16], RIPEMD160_SP[j]) + Ep;
            Ap = Ep; Ep = Dp; Dp = rol(Cp, 10); Cp = Bp; Bp = T;
        }
        
        uint32_t T = h1 + C + Dp;
        h1 = h2 + D + Ep; h2 = h3 + E + Ap; h3 = h4 + A + Bp; h4 = h0 + B + Cp; h0 = T;
    }
    free(padded);
    
    for (int i = 0; i < 4; i++) {
        digest[i] = (h0 >> (i*8)) & 0xFF;
        digest[i+4] = (h1 >> (i*8)) & 0xFF;
        digest[i+8] = (h2 >> (i*8)) & 0xFF;
        digest[i+12] = (h3 >> (i*8)) & 0xFF;
        digest[i+16] = (h4 >> (i*8)) & 0xFF;
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

static void sha256_hash(const uint8_t* data, size_t len, uint8_t* hash) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
}

static void double_sha256(const uint8_t* data, size_t len, uint8_t* hash) {
    uint8_t temp[32];
    sha256_hash(data, len, temp);
    sha256_hash(temp, 32, hash);
}

static void hash160(const uint8_t* data, size_t len, uint8_t* hash) {
    uint8_t sha[32];
    sha256_hash(data, len, sha);
    ripemd160_hash(sha, 32, hash);
}

static bool base58_check_encode(const uint8_t* data, size_t len, char* output, size_t outLen) {
    // Add 4-byte checksum
    size_t totalLen = len + 4;
    uint8_t* withChecksum = (uint8_t*)malloc(totalLen);
    if (!withChecksum) return false;
    
    memcpy(withChecksum, data, len);
    uint8_t hash[32];
    double_sha256(data, len, hash);
    memcpy(withChecksum + len, hash, 4);
    
    // Count leading zeros
    int zeros = 0;
    for (size_t i = 0; i < totalLen && withChecksum[i] == 0; i++) zeros++;
    
    // Convert to base58
    size_t size = totalLen * 138 / 100 + 1;
    uint8_t* b58 = (uint8_t*)calloc(size, 1);
    if (!b58) { free(withChecksum); return false; }
    
    for (size_t i = 0; i < totalLen; i++) {
        int carry = withChecksum[i];
        for (int j = size - 1; j >= 0; j--) {
            carry += 256 * b58[j];
            b58[j] = carry % 58;
            carry /= 58;
        }
    }
    
    int start = 0;
    while (start < (int)size && b58[start] == 0) start++;
    
    int idx = 0;
    for (int i = 0; i < zeros && idx < (int)outLen - 1; i++) output[idx++] = '1';
    for (int i = start; i < (int)size && idx < (int)outLen - 1; i++) output[idx++] = BASE58_ALPHABET[b58[i]];
    output[idx] = '\0';
    
    free(withChecksum);
    free(b58);
    return true;
}

static int base58_decode(const char* input, uint8_t* output, size_t* outLen) {
    size_t inLen = strlen(input);
    
    // Count leading '1's (zeros in output)
    int zeros = 0;
    while (zeros < (int)inLen && input[zeros] == '1') zeros++;
    
    // Allocate enough space
    size_t size = inLen * 733 / 1000 + 1;
    uint8_t* b256 = (uint8_t*)calloc(size, 1);
    if (!b256) return 0;
    
    for (size_t i = 0; i < inLen; i++) {
        const char* p = strchr(BASE58_ALPHABET, input[i]);
        if (!p) { free(b256); return 0; }
        int carry = (int)(p - BASE58_ALPHABET);
        for (int j = size - 1; j >= 0; j--) {
            carry += 58 * b256[j];
            b256[j] = carry % 256;
            carry /= 256;
        }
    }
    
    int start = 0;
    while (start < (int)size && b256[start] == 0) start++;
    
    size_t resultLen = zeros + (size - start);
    if (resultLen > *outLen) { free(b256); return 0; }
    
    memset(output, 0, zeros);
    memcpy(output + zeros, b256 + start, size - start);
    *outLen = resultLen;
    
    free(b256);
    return 1;
}

// ============================================================================
// Public API Implementation
// ============================================================================

extern "C" {

void dogecoin_ecc_start(void) {
    ecc_initialized = true;
    Serial.println("[libdogecoin] ECC context initialized");
}

void dogecoin_ecc_stop(void) {
    ecc_initialized = false;
    Serial.println("[libdogecoin] ECC context stopped");
}

int generatePrivPubKeypair(char* wif_privkey, char* p2pkh_pubkey, dogecoin_bool is_testnet) {
    if (!ecc_initialized) {
        Serial.println("[libdogecoin] Error: ECC not initialized");
        return 0;
    }
    
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_mpi d;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&d);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    const char *pers = "dogecoin_keygen";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        Serial.printf("[libdogecoin] Failed to seed RNG: %d\n", ret);
        goto cleanup;
    }
    
    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1);
    if (ret != 0) {
        Serial.printf("[libdogecoin] Failed to load secp256k1: %d\n", ret);
        goto cleanup;
    }
    
    // Generate a valid private key using mbedTLS (properly within curve order)
    ret = mbedtls_ecp_gen_privkey(&grp, &d, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        Serial.printf("[libdogecoin] Failed to generate private key: %d\n", ret);
        goto cleanup;
    }
    
    // Export private key to bytes for WIF encoding
    {
        uint8_t privkey[32];
        ret = mbedtls_mpi_write_binary(&d, privkey, 32);
        if (ret != 0) {
            Serial.printf("[libdogecoin] Failed to export private key: %d\n", ret);
            goto cleanup;
        }
        
        // Create WIF private key
        uint8_t wifData[34];
        wifData[0] = is_testnet ? DOGECOIN_TESTNET_PRIVKEY_VERSION : DOGECOIN_MAINNET_PRIVKEY_VERSION;
        memcpy(wifData + 1, privkey, 32);
        wifData[33] = 0x01; // Compressed flag
        
        if (!base58_check_encode(wifData, 34, wif_privkey, PRIVKEYWIFLEN)) {
            Serial.println("[libdogecoin] Failed to encode WIF");
            ret = -1;
            goto cleanup;
        }
    }
    
    // Derive public key: Q = d * G
    ret = mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        Serial.printf("[libdogecoin] Failed to derive public key: %d\n", ret);
        goto cleanup;
    }
    
    // Export compressed public key and create address
    {
        uint8_t pubkey[33];
        size_t olen;
        ret = mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_COMPRESSED, &olen, pubkey, 33);
        if (ret != 0 || olen != 33) {
            Serial.printf("[libdogecoin] Failed to export public key: %d\n", ret);
            goto cleanup;
        }
        
        // Hash160 of public key
        uint8_t pubkeyHash[20];
        hash160(pubkey, 33, pubkeyHash);
        
        // Create address with version byte
        uint8_t versioned[21];
        versioned[0] = is_testnet ? DOGECOIN_TESTNET_PUBKEY_VERSION : DOGECOIN_MAINNET_PUBKEY_VERSION;
        memcpy(versioned + 1, pubkeyHash, 20);
        
        if (!base58_check_encode(versioned, 21, p2pkh_pubkey, P2PKHLEN)) {
            Serial.println("[libdogecoin] Failed to encode address");
            ret = -1;
            goto cleanup;
        }
    }
    
    ret = 0; // Success
    Serial.println("[libdogecoin] Keypair generated successfully");
    
cleanup:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    
    return (ret == 0) ? 1 : 0;
}

int verifyPrivPubKeypair(const char* wif_privkey, const char* p2pkh_pubkey, dogecoin_bool is_testnet) {
    if (!ecc_initialized) return 0;
    
    // Decode WIF
    uint8_t decoded[128];
    size_t decodedLen = sizeof(decoded);
    if (!base58_decode(wif_privkey, decoded, &decodedLen)) return 0;
    
    // Remove checksum (last 4 bytes) and verify
    if (decodedLen < 5) return 0;
    decodedLen -= 4;
    
    uint8_t hash[32];
    double_sha256(decoded, decodedLen, hash);
    if (memcmp(hash, decoded + decodedLen, 4) != 0) return 0;
    
    // Extract private key (skip version byte, maybe skip compression flag)
    uint8_t privkey[32];
    if (decodedLen == 34) {
        memcpy(privkey, decoded + 1, 32);
    } else if (decodedLen == 33) {
        memcpy(privkey, decoded + 1, 32);
    } else {
        return 0;
    }
    
    // Derive public key and compare address
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_mpi d;
    
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&d);
    
    int result = 0;
    
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) != 0) goto verify_cleanup;
    if (mbedtls_mpi_read_binary(&d, privkey, 32) != 0) goto verify_cleanup;
    if (mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, NULL, NULL) != 0) goto verify_cleanup;
    
    {
        uint8_t pubkey[33];
        size_t olen;
        if (mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_COMPRESSED, &olen, pubkey, 33) != 0) goto verify_cleanup;
        
        uint8_t pubkeyHash[20];
        hash160(pubkey, 33, pubkeyHash);
        
        uint8_t versioned[21];
        versioned[0] = is_testnet ? DOGECOIN_TESTNET_PUBKEY_VERSION : DOGECOIN_MAINNET_PUBKEY_VERSION;
        memcpy(versioned + 1, pubkeyHash, 20);
        
        char derivedAddr[P2PKHLEN];
        if (!base58_check_encode(versioned, 21, derivedAddr, P2PKHLEN)) goto verify_cleanup;
        
        result = (strcmp(derivedAddr, p2pkh_pubkey) == 0) ? 1 : 0;
    }
    
verify_cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    
    return result;
}

int verifyP2pkhAddress(const char* p2pkh_pubkey, size_t len) {
    if (len < 26 || len > 35) return 0;
    
    // Decode base58
    uint8_t decoded[128];
    size_t decodedLen = sizeof(decoded);
    if (!base58_decode(p2pkh_pubkey, decoded, &decodedLen)) return 0;
    
    // Should be 25 bytes (1 version + 20 hash + 4 checksum)
    if (decodedLen != 25) return 0;
    
    // Verify checksum
    uint8_t hash[32];
    double_sha256(decoded, 21, hash);
    if (memcmp(hash, decoded + 21, 4) != 0) return 0;
    
    // Check version byte
    if (decoded[0] != DOGECOIN_MAINNET_PUBKEY_VERSION && 
        decoded[0] != DOGECOIN_TESTNET_PUBKEY_VERSION) return 0;
    
    return 1;
}

int deriveAddressFromWIF(const char* wif_privkey, char* p2pkh_pubkey, dogecoin_bool is_testnet) {
    if (!ecc_initialized) return 0;
    
    // Decode WIF
    uint8_t decoded[128];
    size_t decodedLen = sizeof(decoded);
    if (!base58_decode(wif_privkey, decoded, &decodedLen)) return 0;
    
    // Remove checksum (last 4 bytes) and verify
    if (decodedLen < 5) return 0;
    decodedLen -= 4;
    
    uint8_t hash[32];
    double_sha256(decoded, decodedLen, hash);
    if (memcmp(hash, decoded + decodedLen, 4) != 0) return 0;
    
    // Extract private key (skip version byte)
    uint8_t privkey[32];
    if (decodedLen == 34) {
        memcpy(privkey, decoded + 1, 32);
    } else if (decodedLen == 33) {
        memcpy(privkey, decoded + 1, 32);
    } else {
        return 0;
    }
    
    // Derive public key
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_mpi d;
    
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&d);
    
    int result = 0;
    
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) != 0) goto derive_cleanup;
    if (mbedtls_mpi_read_binary(&d, privkey, 32) != 0) goto derive_cleanup;
    if (mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, NULL, NULL) != 0) goto derive_cleanup;
    
    {
        uint8_t pubkey[33];
        size_t olen;
        if (mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_COMPRESSED, &olen, pubkey, 33) != 0) goto derive_cleanup;
        
        uint8_t pubkeyHash[20];
        hash160(pubkey, 33, pubkeyHash);
        
        uint8_t versioned[21];
        versioned[0] = is_testnet ? DOGECOIN_TESTNET_PUBKEY_VERSION : DOGECOIN_MAINNET_PUBKEY_VERSION;
        memcpy(versioned + 1, pubkeyHash, 20);
        
        if (!base58_check_encode(versioned, 21, p2pkh_pubkey, P2PKHLEN)) goto derive_cleanup;
        
        result = 1;
    }
    
derive_cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    
    return result;
}

/**
 * @brief Calculate and return the current moon phase
 * Based on original libdogecoin moon.c by qlpqlp
 * @return Moon phase string (emoji or text for OLED compatibility)
 */
const char* moon(void) {
    int D, c, e;
    double jd;  // Julian days
    
    // Get current time
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    
    if (tm_info == NULL) {
        return "Moon: ???";
    }
    
    int y = tm_info->tm_year + 1900;  // Year since 1900
    int m = tm_info->tm_mon;           // Month (0-11)
    int d = tm_info->tm_mday - 2;      // Day (adjusted -2 for accuracy)
    
    if (m < 3) {
        y--;
        m += 12;
    }
    ++m;
    
    c = 365.25 * y;           // Total days in year
    e = 30.6 * m;
    jd = c + e + d - 694039.09;  // Julian days
    jd /= 29.53;              // Moon cycle is 29.53 days
    D = (int)jd;
    jd -= D;
    D = (int)(jd * 8 + 1.5);  // Get fraction 0-8
    
    // Return moon phase (OLED-compatible text with emoji)
    switch (D & 7) {
        case 0: return "New Moon";           // 🌑
        case 1: return "Waxing Crescent";    // 🌒
        case 2: return "First Quarter";      // 🌓
        case 3: return "Waxing Gibbous";     // 🌔
        case 4: return "Full Moon!";         // 🌕
        case 5: return "Waning Gibbous";     // 🌖
        case 6: return "Third Quarter";      // 🌗
        case 7: return "Waning Crescent";    // 🌘
    }
    return "Moon: ???";
}

/**
 * @brief Get moon phase as emoji (for Unicode-capable displays)
 * @return Moon emoji character
 */
const char* moonEmoji(void) {
    switch (moonPhase()) {
        case 0: return "🌑";  // New Moon
        case 1: return "🌒";  // Waxing Crescent
        case 2: return "🌓";  // First Quarter
        case 3: return "🌔";  // Waxing Gibbous
        case 4: return "🌕";  // Full Moon
        case 5: return "🌖";  // Waning Gibbous
        case 6: return "🌗";  // Third Quarter
        case 7: return "🌘";  // Waning Crescent
    }
    return "?";
}

/**
 * @brief Get the current moon phase index (0-7)
 * @return Phase index for use with XBM images
 */
int moonPhase(void) {
    int D, c, e;
    double jd;
    
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    
    if (tm_info == NULL) return 0;
    
    int y = tm_info->tm_year + 1900;
    int m = tm_info->tm_mon;
    int d = tm_info->tm_mday - 2;
    
    if (m < 3) { y--; m += 12; }
    ++m;
    
    c = 365.25 * y;
    e = 30.6 * m;
    jd = c + e + d - 694039.09;
    jd /= 29.53;
    D = (int)jd;
    jd -= D;
    D = (int)(jd * 8 + 1.5);
    
    return D & 7;
}

} // extern "C"


