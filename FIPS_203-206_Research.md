# FIPS 203–206 Implementation: Post-Quantum Cryptography Standards

## Executive Summary

This document provides a comprehensive analysis of NIST's newly finalized post-quantum cryptography (PQC) standards: FIPS 203 (ML-KEM), FIPS 204 (ML-DSA), FIPS 205 (SLH-DSA), and the emerging FIPS 206 (FN-DSA). These standards represent the first officially approved cryptographic algorithms designed to resist attacks from both classical and quantum computers.

---

## 1. COMPARATIVE ANALYSIS: FIPS 203-206 Standards

### 1.1 ML-KEM (FIPS 203): Module-Lattice-Based Key-Encapsulation Mechanism

**Purpose:** Key exchange (KEM) for establishing shared secrets  
**Mathematical Basis:** Module Learning with Errors (MLWE) problem  
**Predecessor:** CRYSTALS-Kyber  
**Status:** Finalized August 2024

| Parameter Set | Public Key (bytes) | Secret Key (bytes) | Ciphertext (bytes) | Security Level | Notes |
|---|---|---|---|---|---|
| ML-KEM-512 | 800 | 1,632 | 768 | 1 (AES-128) | Fast, baseline security |
| ML-KEM-768 | 1,184 | 2,400 | 1,088 | 3 (AES-192) | **Recommended** for general use |
| ML-KEM-1024 | 1,568 | 3,168 | 1,568 | 5 (AES-256) | Highest security, larger overhead |

**Key Features:**
- Fixed 32-byte (256-bit) shared secret output
- Non-interactive key establishment (requires interaction for ephemeral use)
- Relatively fast performance comparable to classical ECDH
- Significantly larger key and ciphertext sizes than X25519
- All variants produce 32-byte shared secrets regardless of security level

**Total Handshake Size (public key + ciphertext):**
- ML-KEM-512: 1,568 bytes
- ML-KEM-768: 2,272 bytes
- ML-KEM-1024: 3,136 bytes

---

### 1.2 ML-DSA (FIPS 204): Module-Lattice-Based Digital Signature Algorithm

**Purpose:** Digital signatures for authentication and non-repudiation  
**Mathematical Basis:** Module-Learning-with-Rounding (MLWR) problem  
**Predecessor:** CRYSTALS-Dilithium  
**Status:** Finalized August 2024

| Parameter Set | Public Key (bytes) | Private Key (bytes) | Signature (bytes) | Security Level | Use Case |
|---|---|---|---|---|---|
| ML-DSA-44 | 1,312 | 2,560 | 2,420 | 2 (≈AES-128) | Lower security requirements |
| ML-DSA-65 | 1,952 | 4,032 | 3,309 | 3 (≈AES-192) | **General-purpose** (recommended) |
| ML-DSA-87 | 2,592 | 4,896 | 4,627 | 5 (≈AES-256) | High-security applications |

**Key Features:**
- Drop-in replacement for RSA-PSS and ECDSA
- Deterministic signing (no randomness needed)
- Fast verification relative to signing
- Signatures 30-50× larger than ECDSA
- Hedged signing mode available for additional randomness injection

**Comparison with Classical Signatures:**
- Ed25519: 32-byte public key, 64-byte signature
- ML-DSA-65: 1,952-byte public key, 3,309-byte signature
- Size overhead: ~60-50× larger

---

### 1.3 SLH-DSA (FIPS 205): Stateless Hash-Based Digital Signature Algorithm

**Purpose:** Conservative, hash-function-only digital signatures  
**Mathematical Basis:** Merkle tree construction over SHA-2 or SHAKE  
**Predecessor:** SPHINCS+  
**Status:** Finalized August 2024

| Parameter Set | Public Key (bytes) | Signature (bytes) | Hash Function | Security Level |
|---|---|---|---|---|
| SLH-DSA-SHA2-128s | 32 | 7,856 | SHA-2 | 1 (≈AES-128) |
| SLH-DSA-SHA2-128f | 32 | 17,088 | SHA-2 | 1 (small or fast?) |
| SLH-DSA-SHA2-192s | 48 | 17,136 | SHA-2 | 3 (≈AES-192) |
| SLH-DSA-SHA2-192f | 48 | 35,664 | SHA-2 | 3 |
| SLH-DSA-SHA2-256s | 64 | 29,856 | SHA-2 | 5 (≈AES-256) |
| SLH-DSA-SHA2-256f | 64 | 49,856 | SHA-2 | 5 |
| SLH-DSA-SHAKE-128s | 32 | 7,856 | SHAKE256 | 1 |
| SLH-DSA-SHAKE-128f | 32 | 17,088 | SHAKE256 | 1 |
| SLH-DSA-SHAKE-192s | 48 | 17,136 | SHAKE256 | 3 |
| SLH-DSA-SHAKE-192f | 48 | 35,664 | SHAKE256 | 3 |
| SLH-DSA-SHAKE-256s | 64 | 29,856 | SHAKE256 | 5 |
| SLH-DSA-SHAKE-256f | 64 | 49,856 | SHAKE256 | 5 |

**Key Features:**
- **Smallest public keys** (32-64 bytes)
- Very large signatures (7-50 KB range)
- "s" = small signatures, "f" = fast signatures
- No state management required (unlike older LMS/XMSS)
- Based only on well-established hash functions (SHA-2, SHAKE256)
- Slowest among the three signature schemes
- Best for long-term archival requiring high confidence

**Unique Advantage:** Only relies on the security of SHA-256 or SHAKE256, making it the most conservative choice with decades of cryptanalytic scrutiny.

---

### 1.4 FN-DSA (FIPS 206): Fast Fourier Transform over NTRU Lattice-Based Digital Signature

**Purpose:** Compact digital signatures optimized for bandwidth-constrained environments  
**Mathematical Basis:** NTRU lattice problem  
**Predecessor:** FALCON  
**Status:** Draft (expected finalization late 2026/early 2027)

| Parameter Set | Public Key (bytes) | Private Key (bytes) | Signature (bytes) | Security Level | Status |
|---|---|---|---|---|---|
| FN-DSA-512 | 897 | 1,281 | 666 | 1 (≈AES-128) | IPD Draft |
| FN-DSA-1024 | 1,793 | 2,305 | 1,280 | 5 (≈AES-256) | IPD Draft |

**Key Features:**
- **Significantly smaller signatures** than ML-DSA (1/3 to 1/5 size)
- **More complex implementation** requiring floating-point arithmetic
- Slower than ML-DSA but much faster than SLH-DSA
- **Non-deterministic signing** (requires careful randomness handling)
- Excellent for certificate chains (DNS, PKI)
- Intended for intermediate/root certificates rather than leaf certificates
- Difficult to implement securely; floating-point precision is critical

**Comparison with ML-DSA:**
- FN-DSA-512 signature: 666 bytes
- ML-DSA-65 signature: 3,309 bytes
- Reduction: **80% smaller** than ML-DSA

**Trade-offs:**
- Pro: Much smaller sizes, closer to ECC compactness
- Con: Complex implementation, non-deterministic, slower, still in draft

---

## 2. Classical vs. PQC Algorithm Comparison

### 2.1 X25519 vs. ML-KEM-768 (Key Exchange)

| Metric | X25519 (Classical) | ML-KEM-768 (PQC) | Ratio |
|---|---|---|---|
| **Public Key** | 32 bytes | 1,184 bytes | 37× larger |
| **Ciphertext** | 32 bytes | 1,088 bytes | 34× larger |
| **Total Handshake** | 64 bytes | 2,272 bytes | 35× larger |
| **Shared Secret** | 32 bytes | 32 bytes | Same |
| **KeyGen Speed** | ~10 μs | ~50 μs | 5× slower |
| **Encaps Speed** | ~10 μs | ~50 μs | 5× slower |
| **Decaps Speed** | ~10 μs | ~50 μs | 5× slower |
| **Quantum Resistant** | ✗ No | ✓ Yes | — |
| **Classical Security** | ~128-bits | ~128-bits | Equivalent |

**Conclusion:** ML-KEM-768 is 5–6× slower than X25519 but 35× larger. For hybrid approaches (X25519 + ML-KEM-768), the 1,216-byte public key and 1,120-byte ciphertext add ~2 KB to handshakes—acceptable for most modern networks.

---

### 2.2 Ed25519 vs. ML-DSA-65 (Digital Signatures)

| Metric | Ed25519 (Classical) | ML-DSA-65 (PQC) | Ratio |
|---|---|---|---|
| **Public Key** | 32 bytes | 1,952 bytes | 61× larger |
| **Private Key** | 32 bytes | 4,032 bytes | 126× larger |
| **Signature** | 64 bytes | 3,309 bytes | 52× larger |
| **Signing Speed** | ~10 μs | ~100 μs | 10× slower |
| **Verification Speed** | ~30 μs | ~200 μs | 7× slower |
| **Quantum Resistant** | ✗ No | ✓ Yes | — |
| **Classical Security** | ~128-bits | ~192-bits | Higher |

**Conclusion:** ML-DSA trades significant size and speed overhead for quantum resistance. The 3.3 KB signature is acceptable for most applications but problematic for bandwidth-constrained systems (e.g., embedded, IoT, DNS).

---

## 3. Alternative PQC Algorithm: CRYSTALS-Kyber (Pre-Standardization)

### Context
While ML-KEM is the standardized version of Kyber, the research phase used CRYSTALS-Kyber, which influenced ML-KEM but differs in implementation details. Another interesting alternative outside NIST's primary selection is **Lattice-based KEMs from Round 4 candidates**:

### HQC (Hamming Quasi-Cyclic): Code-Based KEM

**Status:** Selected for standardization in March 2025 (pending draft)  
**Mathematical Basis:** Syndrome decoding problem over Hamming codes  

| Metric | HQC-128 | HQC-192 | HQC-256 | Notes |
|---|---|---|---|---|
| **Public Key** | 2,249 bytes | 4,522 bytes | 7,245 bytes | Small-ish |
| **Ciphertext** | 2,273 bytes | 4,562 bytes | 7,289 bytes | Near-identical to PK |
| **Performance** | Good | Good | Good | Code-based, not lattice |

**Advantages over ML-KEM:**
- Different mathematical hardness (diversity in portfolio)
- Potentially smaller keys than ML-KEM at some security levels
- Code-based problems have longer research history than lattices

**Disadvantages:**
- Slower than lattice-based (but acceptable)
- Still emerging; less implementation experience

---

## 4. Security Strength Mapping

All NIST standards map to NIST Security Levels:

| Level | Equivalent Classical Strength | FIPS 203 KEM | FIPS 204 Signature | FIPS 205 Signature |
|---|---|---|---|---|
| **1** | AES-128 / RSA-2048 | ML-KEM-512 | ML-DSA-44 | SLH-DSA-SHA2-128s/f |
| **2** | — | — | — | — |
| **3** | AES-192 / RSA-3072 | ML-KEM-768 | ML-DSA-65 | SLH-DSA-SHA2-192s/f |
| **4** | — | — | — | — |
| **5** | AES-256 / RSA-15360+ | ML-KEM-1024 | ML-DSA-87 | SLH-DSA-SHA2-256s/f |

---

## 5. Practical Deployment Recommendations

### Immediate Actions (2025-2026):
1. **Key Exchange Priority:** Deploy X25519 + ML-KEM-768 hybrid in TLS 1.3
   - Already supported: Chrome, Firefox, Edge, Brave, Opera
   - Network impact: +1-2 ms latency
   - Data overhead: ~2 KB per handshake

2. **Signature Deployment:** Use ML-DSA-65 for new certificates
   - Avoid for frequently-signed leaf certificates
   - Suitable for root/intermediate certificates
   - Plan for larger certificate chains

3. **Conservative Backup:** Maintain knowledge of SLH-DSA-SHA2-256s
   - For high-assurance, long-term security requirements
   - Archive and legal document signing

### Future Actions (2027+):
1. Evaluate FN-DSA once FIPS 206 is finalized
2. Plan HQC evaluation once standardization completes
3. Implement crypto-agility for algorithmic switching

---

## 6. Key Takeaways

| Aspect | Finding |
|---|---|
| **KEM Selection** | ML-KEM-768 for nearly all applications; hybrid X25519-ML-KEM-768 for transition |
| **Signature Selection** | ML-DSA-65 for general use; SLH-DSA for conservative long-term use |
| **Certificate Chains** | Watch FN-DSA (FIPS 206) for significant reduction in chain size |
| **Size Overhead** | Expect 30–50× larger signatures, 35× larger ephemeral public keys |
| **Performance Impact** | 5–10× slower than classical, still acceptable for most networks |
| **Quantum Timeline** | Not yet "cryptographically relevant" but harvest-now-decrypt-later risk is real |
| **Recommendations** | Begin hybrid deployments now; plan full transition within 3–5 years |

---

## References

- NIST FIPS 203: Module-Lattice-Based Key-Encapsulation Mechanism Standard (August 2024)
- NIST FIPS 204: Module-Lattice-Based Digital Signature Standard (August 2024)
- NIST FIPS 205: Stateless Hash-Based Digital Signature Algorithm (August 2024)
- NIST FIPS 206: FN-DSA (Initial Public Draft, August 2025, pending finalization)
- Open Quantum Safe (OQS) Project: https://openquantumsafe.org
- NIST Post-Quantum Cryptography Standardization: https://csrc.nist.gov/projects/post-quantum-cryptography

