/*
 * Hybrid Key Exchange Implementation: X25519 + ML-KEM-768
 * ========================================================
 *
 * This C program demonstrates a hybrid key exchange combining:
 *   - X25519 (classical elliptic curve, RFC 7748)
 *   - ML-KEM-768 (post-quantum lattice-based KEM, FIPS 203)
 *
 * The program compares performance and output sizes between:
 *   1. Pure classical (X25519)
 *   2. Pure quantum-safe (ML-KEM-768)
 *   3. Hybrid (X25519 + ML-KEM-768)
 *
 * Hybrid approach ensures that:
 *   - If either algorithm is broken, the session is still secure
 *   - Modern networks handle the larger PQC sizes
 *   - Gradual transition to post-quantum is supported
 *
 * Compilation:
 *   gcc -o hybrid_kex hybrid_key_exchange.c \
 *       -loqs -lssl -lcrypto -lm -Wall -O2
 *
 * Or with clang:
 *   clang -o hybrid_kex hybrid_key_exchange.c \
 *       -loqs -lssl -lcrypto -lm -Wall -O2
 *
 * Requirements:
 *   - liboqs (Open Quantum Safe)
 *   - OpenSSL 1.1.1+ (for X25519)
 *   - C99 or later compiler
 *
 * Author: PQC Implementation Lab
 * Date: February 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <oqs/oqs.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

/* Color output for terminal */
#define COLOR_RESET     "\x1b[0m"
#define COLOR_GREEN     "\x1b[32m"
#define COLOR_YELLOW    "\x1b[33m"
#define COLOR_CYAN      "\x1b[36m"
#define COLOR_BOLD      "\x1b[1m"

/* Performance measurement structure */
typedef struct {
    char algorithm[64];
    double keygen_time_ms;
    double encaps_time_ms;
    double decaps_time_ms;
    double total_time_ms;
    size_t public_key_size;
    size_t private_key_size;
    size_t ciphertext_size;
    size_t shared_secret_size;
    size_t total_handshake_size;
} KeyExchangeMetrics;

/* High-precision timer using clock_gettime */
typedef struct {
    struct timespec start;
    struct timespec end;
} PrecisionTimer;

/* ============================================================================
 * Timer Utilities
 * ============================================================================ */

PrecisionTimer timer_start(void) {
    PrecisionTimer t;
    clock_gettime(CLOCK_MONOTONIC, &t.start);
    return t;
}

double timer_elapsed_ms(PrecisionTimer t) {
    clock_gettime(CLOCK_MONOTONIC, &t.end);
    double elapsed = (t.end.tv_sec - t.start.tv_sec) * 1000.0;
    elapsed += (t.end.tv_nsec - t.start.tv_nsec) / 1000000.0;
    return elapsed;
}

/* ============================================================================
 * Classical X25519 Key Exchange
 * ============================================================================ */

typedef struct {
    unsigned char private_key[32];
    unsigned char public_key[32];
} X25519_KeyPair;

/**
 * Generate X25519 key pair
 * Returns: 1 on success, 0 on failure
 */
int x25519_keygen(X25519_KeyPair *keypair) {
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if (!pctx) return 0;

    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    /* Extract private and public keys */
    size_t priv_len = 32, pub_len = 32;
    if (EVP_PKEY_get_raw_private_key(pkey, keypair->private_key, &priv_len) <= 0 ||
        EVP_PKEY_get_raw_public_key(pkey, keypair->public_key, &pub_len) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    return 1;
}

/**
 * Perform X25519 key exchange
 * Returns: 1 on success, 0 on failure
 */
int x25519_exchange(const unsigned char *peer_public,
                   unsigned char *shared_secret,
                   unsigned char *ephemeral_private) {
    /* Generate ephemeral key pair */
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if (!pctx) return 0;

    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    EVP_PKEY *ephemeral_pkey = NULL;
    if (EVP_PKEY_keygen(pctx, &ephemeral_pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    /* Extract ephemeral private key */
    size_t priv_len = 32;
    if (EVP_PKEY_get_raw_private_key(ephemeral_pkey, ephemeral_private, &priv_len) <= 0) {
        EVP_PKEY_free(ephemeral_pkey);
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    /* Create peer public key object */
    EVP_PKEY *peer_pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL,
                                                       peer_public, 32);
    if (!peer_pkey) {
        EVP_PKEY_free(ephemeral_pkey);
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    /* Perform ECDH */
    EVP_PKEY_CTX *ectx = EVP_PKEY_CTX_new(ephemeral_pkey, NULL);
    if (!ectx) {
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(ephemeral_pkey);
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    if (EVP_PKEY_derive_init(ectx) <= 0) {
        EVP_PKEY_CTX_free(ectx);
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(ephemeral_pkey);
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    if (EVP_PKEY_derive_set_peer(ectx, peer_pkey) <= 0) {
        EVP_PKEY_CTX_free(ectx);
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(ephemeral_pkey);
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    size_t secret_len = 32;
    if (EVP_PKEY_derive(ectx, shared_secret, &secret_len) <= 0) {
        EVP_PKEY_CTX_free(ectx);
        EVP_PKEY_free(peer_pkey);
        EVP_PKEY_free(ephemeral_pkey);
        EVP_PKEY_CTX_free(pctx);
        return 0;
    }

    EVP_PKEY_CTX_free(ectx);
    EVP_PKEY_free(peer_pkey);
    EVP_PKEY_free(ephemeral_pkey);
    EVP_PKEY_CTX_free(pctx);
    return 1;
}

/* ============================================================================
 * Quantum-Safe ML-KEM-768 Key Exchange
 * ============================================================================ */

typedef struct {
    unsigned char *private_key;
    unsigned char *public_key;
    size_t private_key_size;
    size_t public_key_size;
} MLKem768_KeyPair;

/**
 * Generate ML-KEM-768 key pair
 * Returns: 1 on success, 0 on failure
 */
int mlkem768_keygen(MLKem768_KeyPair *keypair) {
    OQS_KEM *kem = OQS_KEM_new("ML-KEM-768");
    if (!kem) {
        fprintf(stderr, "Error: Failed to create ML-KEM-768 KEM\n");
        return 0;
    }

    keypair->public_key_size = kem->length_public_key;
    keypair->private_key_size = kem->length_secret_key;

    keypair->public_key = malloc(kem->length_public_key);
    keypair->private_key = malloc(kem->length_secret_key);

    if (!keypair->public_key || !keypair->private_key) {
        OQS_KEM_free(kem);
        return 0;
    }

    if (OQS_KEM_keypair(kem, keypair->public_key, keypair->private_key) != OQS_STATUS_SUCCESS) {
        OQS_KEM_free(kem);
        return 0;
    }

    OQS_KEM_free(kem);
    return 1;
}

/**
 * Perform ML-KEM-768 encapsulation
 * Returns: 1 on success, 0 on failure
 */
int mlkem768_encaps(const unsigned char *public_key,
                   size_t public_key_size,
                   unsigned char *ciphertext,
                   unsigned char *shared_secret,
                   size_t *ciphertext_size,
                   size_t *shared_secret_size) {
    OQS_KEM *kem = OQS_KEM_new("ML-KEM-768");
    if (!kem) return 0;

    *ciphertext_size = kem->length_ciphertext;
    *shared_secret_size = kem->length_shared_secret;

    if (OQS_KEM_encaps(kem, ciphertext, shared_secret, public_key) != OQS_STATUS_SUCCESS) {
        OQS_KEM_free(kem);
        return 0;
    }

    OQS_KEM_free(kem);
    return 1;
}

/**
 * Perform ML-KEM-768 decapsulation
 * Returns: 1 on success, 0 on failure
 */
int mlkem768_decaps(const unsigned char *private_key,
                   size_t private_key_size,
                   const unsigned char *ciphertext,
                   size_t ciphertext_size,
                   unsigned char *shared_secret) {
    OQS_KEM *kem = OQS_KEM_new("ML-KEM-768");
    if (!kem) return 0;

    if (OQS_KEM_decaps(kem, shared_secret, ciphertext, private_key) != OQS_STATUS_SUCCESS) {
        OQS_KEM_free(kem);
        return 0;
    }

    OQS_KEM_free(kem);
    return 1;
}

/* ============================================================================
 * Hybrid Key Exchange (X25519 + ML-KEM-768)
 * ============================================================================ */

/**
 * Combine X25519 and ML-KEM-768 shared secrets using SHA-256 KDF
 */
void hybrid_combine_secrets(const unsigned char *x25519_secret,
                           const unsigned char *mlkem_secret,
                           unsigned char *combined_secret) {
    SHA256_CTX sha_ctx;
    unsigned char label[] = "HYBRID_KDF";

    SHA256_Init(&sha_ctx);
    SHA256_Update(&sha_ctx, label, sizeof(label) - 1);
    SHA256_Update(&sha_ctx, x25519_secret, 32);
    SHA256_Update(&sha_ctx, mlkem_secret, 32);
    SHA256_Final(combined_secret, &sha_ctx);
}

/* ============================================================================
 * Benchmarking Functions
 * ============================================================================ */

void benchmark_classical_x25519(KeyExchangeMetrics *metrics) {
    printf("\n" COLOR_BOLD COLOR_CYAN "======================================================================\n");
    printf("BENCHMARK: Classical X25519 Key Exchange\n");
    printf("======================================================================" COLOR_RESET "\n\n");

    memset(metrics, 0, sizeof(KeyExchangeMetrics));
    strcpy(metrics->algorithm, "X25519 (Classical)");

    /* Keygen */
    PrecisionTimer t = timer_start();
    X25519_KeyPair alice_keypair, bob_keypair;
    x25519_keygen(&alice_keypair);
    x25519_keygen(&bob_keypair);
    metrics->keygen_time_ms = timer_elapsed_ms(t);

    /* Exchange (Bob encapsulates for Alice) */
    unsigned char bob_ephemeral[32];
    unsigned char bob_secret[32];
    t = timer_start();
    x25519_exchange(alice_keypair.public_key, bob_secret, bob_ephemeral);
    metrics->encaps_time_ms = timer_elapsed_ms(t);

    /* Exchange (Alice decapsulates) */
    unsigned char alice_secret[32];
    t = timer_start();
    x25519_exchange(bob_keypair.public_key, alice_secret, bob_ephemeral);
    metrics->decaps_time_ms = timer_elapsed_ms(t);

    metrics->total_time_ms = metrics->keygen_time_ms + metrics->encaps_time_ms + metrics->decaps_time_ms;
    metrics->public_key_size = 32;
    metrics->private_key_size = 32;
    metrics->ciphertext_size = 32;
    metrics->shared_secret_size = 32;
    metrics->total_handshake_size = metrics->public_key_size + metrics->ciphertext_size;

    printf("Keygen Time:           %8.3f ms (both parties)\n", metrics->keygen_time_ms);
    printf("Encapsulation Time:    %8.3f ms\n", metrics->encaps_time_ms);
    printf("Decapsulation Time:    %8.3f ms\n", metrics->decaps_time_ms);
    printf("Total Round Trip:      %8.3f ms\n\n", metrics->total_time_ms);

    printf("Public Key Size:       %8zu bytes\n", metrics->public_key_size);
    printf("Private Key Size:      %8zu bytes\n", metrics->private_key_size);
    printf("Ephemeral Secret Size: %8zu bytes\n", metrics->ciphertext_size);
    printf("Shared Secret Size:    %8zu bytes\n", metrics->shared_secret_size);
    printf("Total Handshake Size:  %8zu bytes\n", metrics->total_handshake_size);
}

void benchmark_quantum_safe(KeyExchangeMetrics *metrics) {
    printf("\n" COLOR_BOLD COLOR_CYAN "======================================================================\n");
    printf("BENCHMARK: ML-KEM-768 (Quantum-Safe) Key Exchange\n");
    printf("======================================================================" COLOR_RESET "\n\n");

    memset(metrics, 0, sizeof(KeyExchangeMetrics));
    strcpy(metrics->algorithm, "ML-KEM-768 (Quantum-Safe)");

    /* Keygen */
    PrecisionTimer t = timer_start();
    MLKem768_KeyPair alice_keypair, bob_keypair;
    mlkem768_keygen(&alice_keypair);
    mlkem768_keygen(&bob_keypair);
    metrics->keygen_time_ms = timer_elapsed_ms(t);

    /* Encapsulation */
    unsigned char *ciphertext = malloc(1088);
    unsigned char *bob_secret = malloc(32);
    size_t ct_size, ss_size;

    t = timer_start();
    mlkem768_encaps(alice_keypair.public_key, alice_keypair.public_key_size,
                   ciphertext, bob_secret, &ct_size, &ss_size);
    metrics->encaps_time_ms = timer_elapsed_ms(t);

    /* Decapsulation */
    unsigned char *alice_secret = malloc(32);
    t = timer_start();
    mlkem768_decaps(alice_keypair.private_key, alice_keypair.private_key_size,
                   ciphertext, ct_size, alice_secret);
    metrics->decaps_time_ms = timer_elapsed_ms(t);

    metrics->total_time_ms = metrics->keygen_time_ms + metrics->encaps_time_ms + metrics->decaps_time_ms;
    metrics->public_key_size = alice_keypair.public_key_size;
    metrics->private_key_size = alice_keypair.private_key_size;
    metrics->ciphertext_size = ct_size;
    metrics->shared_secret_size = ss_size;
    metrics->total_handshake_size = metrics->public_key_size + metrics->ciphertext_size;

    printf("Keygen Time:           %8.3f ms (both parties)\n", metrics->keygen_time_ms);
    printf("Encapsulation Time:    %8.3f ms\n", metrics->encaps_time_ms);
    printf("Decapsulation Time:    %8.3f ms\n", metrics->decaps_time_ms);
    printf("Total Round Trip:      %8.3f ms\n\n", metrics->total_time_ms);

    printf("Public Key Size:       %8zu bytes\n", metrics->public_key_size);
    printf("Private Key Size:      %8zu bytes\n", metrics->private_key_size);
    printf("Ciphertext Size:       %8zu bytes\n", metrics->ciphertext_size);
    printf("Shared Secret Size:    %8zu bytes\n", metrics->shared_secret_size);
    printf("Total Handshake Size:  %8zu bytes\n", metrics->total_handshake_size);

    free(ciphertext);
    free(bob_secret);
    free(alice_secret);
    free(alice_keypair.public_key);
    free(alice_keypair.private_key);
    free(bob_keypair.public_key);
    free(bob_keypair.private_key);
}

void benchmark_hybrid(KeyExchangeMetrics *metrics) {
    printf("\n" COLOR_BOLD COLOR_CYAN "======================================================================\n");
    printf("BENCHMARK: Hybrid (X25519 + ML-KEM-768) Key Exchange\n");
    printf("======================================================================" COLOR_RESET "\n\n");

    memset(metrics, 0, sizeof(KeyExchangeMetrics));
    strcpy(metrics->algorithm, "X25519 + ML-KEM-768 (Hybrid)");

    /* Keygen */
    PrecisionTimer t = timer_start();
    X25519_KeyPair alice_x25519, bob_x25519;
    MLKem768_KeyPair alice_mlkem, bob_mlkem;
    x25519_keygen(&alice_x25519);
    x25519_keygen(&bob_x25519);
    mlkem768_keygen(&alice_mlkem);
    mlkem768_keygen(&bob_mlkem);
    metrics->keygen_time_ms = timer_elapsed_ms(t);

    /* Exchange */
    unsigned char bob_x25519_eph[32];
    unsigned char bob_x25519_secret[32];
    unsigned char *bob_mlkem_ct = malloc(1088);
    unsigned char *bob_mlkem_secret = malloc(32);
    size_t ct_size, ss_size;

    t = timer_start();
    x25519_exchange(alice_x25519.public_key, bob_x25519_secret, bob_x25519_eph);
    mlkem768_encaps(alice_mlkem.public_key, alice_mlkem.public_key_size,
                   bob_mlkem_ct, bob_mlkem_secret, &ct_size, &ss_size);
    metrics->encaps_time_ms = timer_elapsed_ms(t);

    /* Decapsulation */
    unsigned char alice_x25519_secret[32];
    unsigned char *alice_mlkem_secret = malloc(32);
    unsigned char combined_secret[32];

    t = timer_start();
    x25519_exchange(bob_x25519.public_key, alice_x25519_secret, bob_x25519_eph);
    mlkem768_decaps(alice_mlkem.private_key, alice_mlkem.private_key_size,
                   bob_mlkem_ct, ct_size, alice_mlkem_secret);
    hybrid_combine_secrets(bob_x25519_secret, bob_mlkem_secret, combined_secret);
    metrics->decaps_time_ms = timer_elapsed_ms(t);

    metrics->total_time_ms = metrics->keygen_time_ms + metrics->encaps_time_ms + metrics->decaps_time_ms;
    metrics->public_key_size = 32 + alice_mlkem.public_key_size;
    metrics->private_key_size = 32 + alice_mlkem.private_key_size;
    metrics->ciphertext_size = 32 + ct_size;
    metrics->shared_secret_size = 32;
    metrics->total_handshake_size = 32 + alice_mlkem.public_key_size + 32 + ct_size;

    printf("Keygen Time:           %8.3f ms (both parties)\n", metrics->keygen_time_ms);
    printf("Exchange Time:         %8.3f ms\n", metrics->encaps_time_ms);
    printf("Decapsulation Time:    %8.3f ms\n", metrics->decaps_time_ms);
    printf("Total Round Trip:      %8.3f ms\n\n", metrics->total_time_ms);

    printf("Combined Public Key:   %8zu bytes\n", metrics->public_key_size);
    printf("  X25519:              %8d bytes\n", 32);
    printf("  ML-KEM-768:          %8zu bytes\n", alice_mlkem.public_key_size);
    printf("\nCombined Ciphertext:   %8zu bytes\n", metrics->ciphertext_size);
    printf("  X25519 Ephemeral:    %8d bytes\n", 32);
    printf("  ML-KEM Ciphertext:   %8zu bytes\n", ct_size);
    printf("Shared Secret Size:    %8zu bytes\n", 32);
    printf("Total Handshake Size:  %8zu bytes\n", metrics->total_handshake_size);

    free(bob_mlkem_ct);
    free(bob_mlkem_secret);
    free(alice_mlkem_secret);
    free(alice_mlkem.public_key);
    free(alice_mlkem.private_key);
    free(bob_mlkem.public_key);
    free(bob_mlkem.private_key);
}

/* ============================================================================
 * Output Formatting
 * ============================================================================ */

void print_comparison_table(KeyExchangeMetrics *metrics_array, int count) {
    printf("\n" COLOR_BOLD COLOR_CYAN "======================================================================\n");
    printf("COMPARATIVE SUMMARY\n");
    printf("======================================================================" COLOR_RESET "\n\n");

    printf("%-30s %18s %18s %18s\n", "Metric", "Classical X25519", "Quantum-Safe ML-KEM", "Hybrid X25519+ML-KEM");
    printf("-" "-----" "-" "-----" "-" "-----" "-" "-----" "-" "-----" "-" "-----" "-" "-----" "-" "-----\n");

    printf("\n" COLOR_BOLD "PERFORMANCE (milliseconds):" COLOR_RESET "\n");
    printf("%-30s %18.3f %18.3f %18.3f\n", "Keygen Time",
           metrics_array[0].keygen_time_ms, metrics_array[1].keygen_time_ms, metrics_array[2].keygen_time_ms);
    printf("%-30s %18.3f %18.3f %18.3f\n", "Encaps/Exchange Time",
           metrics_array[0].encaps_time_ms, metrics_array[1].encaps_time_ms, metrics_array[2].encaps_time_ms);
    printf("%-30s %18.3f %18.3f %18.3f\n", "Decaps Time",
           metrics_array[0].decaps_time_ms, metrics_array[1].decaps_time_ms, metrics_array[2].decaps_time_ms);
    printf("%-30s %18.3f %18.3f %18.3f\n", "Total Round Trip",
           metrics_array[0].total_time_ms, metrics_array[1].total_time_ms, metrics_array[2].total_time_ms);

    printf("\n" COLOR_BOLD "SIZES (bytes):" COLOR_RESET "\n");
    printf("%-30s %18zu %18zu %18zu\n", "Public Key Size",
           metrics_array[0].public_key_size, metrics_array[1].public_key_size, metrics_array[2].public_key_size);
    printf("%-30s %18zu %18zu %18zu\n", "Private Key Size",
           metrics_array[0].private_key_size, metrics_array[1].private_key_size, metrics_array[2].private_key_size);
    printf("%-30s %18zu %18zu %18zu\n", "Ciphertext/Ephemeral",
           metrics_array[0].ciphertext_size, metrics_array[1].ciphertext_size, metrics_array[2].ciphertext_size);
    printf("%-30s %18zu %18zu %18zu\n", "Shared Secret",
           metrics_array[0].shared_secret_size, metrics_array[1].shared_secret_size, metrics_array[2].shared_secret_size);
    printf("%-30s %18zu %18zu %18zu\n", "Total Handshake Size",
           metrics_array[0].total_handshake_size, metrics_array[1].total_handshake_size, metrics_array[2].total_handshake_size);

    printf("\n" COLOR_BOLD "OVERHEAD RELATIVE TO X25519:" COLOR_RESET "\n");
    printf("%-30s %18.1f× %18.1f× %18.1f×\n", "Total Round Trip",
           1.0,
           metrics_array[1].total_time_ms / metrics_array[0].total_time_ms,
           metrics_array[2].total_time_ms / metrics_array[0].total_time_ms);
    printf("%-30s %18.1f× %18.1f× %18.1f×\n", "Public Key Size",
           1.0,
           (double)metrics_array[1].public_key_size / metrics_array[0].public_key_size,
           (double)metrics_array[2].public_key_size / metrics_array[0].public_key_size);
    printf("%-30s %18.1f× %18.1f× %18.1f×\n", "Ciphertext Size",
           1.0,
           (double)metrics_array[1].ciphertext_size / metrics_array[0].ciphertext_size,
           (double)metrics_array[2].ciphertext_size / metrics_array[0].ciphertext_size);
    printf("%-30s %18.1f× %18.1f× %18.1f×\n", "Handshake Size",
           1.0,
           (double)metrics_array[1].total_handshake_size / metrics_array[0].total_handshake_size,
           (double)metrics_array[2].total_handshake_size / metrics_array[0].total_handshake_size);
}

void print_key_insights(KeyExchangeMetrics *metrics_array) {
    printf("\n" COLOR_BOLD COLOR_GREEN "======================================================================\n");
    printf("KEY INSIGHTS\n");
    printf("======================================================================" COLOR_RESET "\n\n");

    double overhead_time = ((metrics_array[2].total_time_ms / metrics_array[0].total_time_ms) - 1) * 100;
    double overhead_size = ((metrics_array[2].total_handshake_size / (double)metrics_array[0].total_handshake_size) - 1) * 100;

    printf("✓ Hybrid approach adds ~%.1f%% latency overhead\n", overhead_time);
    printf("✓ Hybrid approach adds ~%.0f%% to handshake size\n\n", overhead_size);

    printf("✓ Hybrid ensures security even if ONE algorithm is broken\n");
    printf("✓ ML-KEM-768 is %.1f× slower than X25519\n", metrics_array[1].total_time_ms / metrics_array[0].total_time_ms);
    printf("✓ ML-KEM-768 is %.0f× larger than X25519\n\n", (double)metrics_array[1].total_handshake_size / metrics_array[0].total_handshake_size);

    printf("✓ Deployment: Begin with X25519+ML-KEM-768 hybrid in TLS 1.3\n");
    printf("✓ Timeline: Plan full migration to post-quantum by 2027-2030\n");
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf("╔" "════════════════════════════════════════════════════════════════╗\n");
    printf("║" "                                                                ║\n");
    printf("║" "       FIPS 203 Hybrid Key Exchange Benchmarking (C)            ║\n");
    printf("║" "    Classical vs. Post-Quantum vs. Hybrid                       ║\n");
    printf("║" "                                                                ║\n");
    printf("╚" "════════════════════════════════════════════════════════════════╝\n");

    KeyExchangeMetrics metrics[3];

    /* Run benchmarks */
    benchmark_classical_x25519(&metrics[0]);
    benchmark_quantum_safe(&metrics[1]);
    benchmark_hybrid(&metrics[2]);

    /* Print comparison and insights */
    print_comparison_table(metrics, 3);
    print_key_insights(metrics);

    printf("\n");
    return 0;
}
