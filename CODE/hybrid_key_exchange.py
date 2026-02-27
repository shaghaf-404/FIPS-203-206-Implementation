#!/usr/bin/env python3
"""
Hybrid Key Exchange Implementation: X25519 + ML-KEM-768
=========================================================

This script demonstrates a hybrid key exchange combining:
  - X25519 (classical elliptic curve, NIST SP 800-56A)
  - ML-KEM-768 (post-quantum lattice-based KEM, FIPS 203)

The script compares performance and output sizes between:
  1. Pure classical (X25519)
  2. Pure quantum-safe (ML-KEM-768)
  3. Hybrid (X25519 + ML-KEM-768)

Hybrid approach ensures that:
  - If either algorithm is broken, the session is still secure
  - Modern networks handle the larger PQC sizes
  - Gradual transition to post-quantum is supported

Requirements:
    pip install liboqs cryptography psutil

Author: PQC Implementation Lab
Date: 2026
"""

import json
import os
import time
import sys
import hashlib
from typing import Dict, Tuple, Any
from dataclasses import dataclass, asdict
import traceback

try:
    from cryptography.hazmat.primitives.asymmetric import x25519
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.backends import default_backend
    import oqs
except ImportError as e:
    print(f"ERROR: Missing required package: {e}")
    print("\nInstall with: pip install liboqs cryptography")
    sys.exit(1)


@dataclass
class KeyExchangeMetrics:
    """Container for key exchange performance metrics"""
    algorithm: str
    keygen_time_ms: float
    encaps_time_ms: float
    decaps_time_ms: float
    total_time_ms: float
    public_key_size: int
    private_key_size: int
    ciphertext_size: int
    shared_secret_size: int
    total_handshake_size: int  # public_key + ciphertext


class ClassicalX25519KeyExchange:
    """Pure classical key exchange using X25519"""
    
    NAME = "X25519 (Classical)"
    
    @staticmethod
    def keygen() -> Tuple[bytes, bytes]:
        """Generate X25519 key pair
        
        Returns:
            (private_key_bytes, public_key_bytes)
        """
        private_key = x25519.X25519PrivateKey.generate()
        public_key = private_key.public_key()
        
        private_bytes = private_key.private_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PrivateFormat.Raw,
            encryption_algorithm=serialization.NoEncryption()
        )
        
        public_bytes = public_key.public_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PublicFormat.Raw
        )
        
        return private_bytes, public_bytes
    
    @staticmethod
    def exchange(public_key_bytes: bytes) -> Tuple[bytes, bytes]:
        """Perform X25519 key exchange
        
        Args:
            public_key_bytes: Peer's public key
            
        Returns:
            (private_key_bytes, shared_secret)
        """
        peer_public = x25519.X25519PublicKey.from_public_bytes(public_key_bytes)
        private_key = x25519.X25519PrivateKey.generate()
        shared_secret = private_key.exchange(peer_public)
        
        private_bytes = private_key.private_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PrivateFormat.Raw,
            encryption_algorithm=serialization.NoEncryption()
        )
        
        return private_bytes, shared_secret


class QuantumSafeMLKEM768KeyExchange:
    """Pure PQC key exchange using ML-KEM-768"""
    
    NAME = "ML-KEM-768 (Quantum-Safe)"
    ALGORITHM = "ML-KEM-768"
    
    @staticmethod
    def keygen() -> Tuple[bytes, bytes]:
        """Generate ML-KEM-768 key pair
        
        Returns:
            (private_key, public_key)
        """
        kem = oqs.KeyEncapsulation(QuantumSafeMLKEM768KeyExchange.ALGORITHM)
        public_key = kem.generate_keypair()
        private_key = kem.export_secret_key()
        kem.free()
        return private_key, public_key
    
    @staticmethod
    def exchange(public_key: bytes) -> Tuple[bytes, bytes]:
        """Perform ML-KEM-768 encapsulation
        
        Args:
            public_key: Peer's public key
            
        Returns:
            (ciphertext, shared_secret)
        """
        kem = oqs.KeyEncapsulation(QuantumSafeMLKEM768KeyExchange.ALGORITHM,
                                   secret_key=b'')  # Encapsulation mode
        ciphertext, shared_secret = kem.encap_secret(public_key)
        kem.free()
        return ciphertext, shared_secret


class HybridX25519ML_KEM768KeyExchange:
    """Hybrid key exchange combining X25519 and ML-KEM-768"""
    
    NAME = "X25519 + ML-KEM-768 (Hybrid)"
    
    @staticmethod
    def keygen() -> Tuple[Dict[str, bytes], Dict[str, bytes]]:
        """Generate hybrid key pair (X25519 + ML-KEM-768)
        
        Returns:
            (private_keys_dict, public_keys_dict)
        """
        # X25519
        x25519_priv, x25519_pub = ClassicalX25519KeyExchange.keygen()
        
        # ML-KEM-768
        mlkem_priv, mlkem_pub = QuantumSafeMLKEM768KeyExchange.keygen()
        
        private_keys = {
            'x25519': x25519_priv,
            'ml_kem_768': mlkem_priv
        }
        
        public_keys = {
            'x25519': x25519_pub,
            'ml_kem_768': mlkem_pub
        }
        
        return private_keys, public_keys
    
    @staticmethod
    def exchange(public_keys: Dict[str, bytes]) -> Tuple[Dict[str, bytes], bytes]:
        """Perform hybrid key exchange
        
        Args:
            public_keys: Dict with 'x25519' and 'ml_kem_768' public keys
            
        Returns:
            (ephemeral_private_keys, combined_shared_secret)
        """
        # X25519 exchange
        x25519_priv, x25519_secret = ClassicalX25519KeyExchange.exchange(
            public_keys['x25519']
        )
        
        # ML-KEM-768 encapsulation
        mlkem_ct, mlkem_secret = QuantumSafeMLKEM768KeyExchange.exchange(
            public_keys['ml_kem_768']
        )
        
        # Combine shared secrets using KDF (HKDF-like construction)
        # In production, use HKDF-SHA256 or similar
        combined_secret = hashlib.sha256(
            b'HYBRID_KDF' + x25519_secret + mlkem_secret
        ).digest()
        
        ephemeral_private = {
            'x25519': x25519_priv,
            'ml_kem_768_ciphertext': mlkem_ct
        }
        
        return ephemeral_private, combined_secret


def benchmark_classical() -> KeyExchangeMetrics:
    """Benchmark pure classical X25519 exchange"""
    print("\n" + "="*70)
    print("BENCHMARK: Classical X25519 Key Exchange")
    print("="*70)
    
    # Keygen
    start = time.perf_counter()
    alice_priv, alice_pub = ClassicalX25519KeyExchange.keygen()
    bob_priv, bob_pub = ClassicalX25519KeyExchange.keygen()
    keygen_time = (time.perf_counter() - start) * 1000  # Convert to ms
    
    # Exchange (Alice encapsulates for Bob)
    start = time.perf_counter()
    bob_eph_priv, bob_secret = ClassicalX25519KeyExchange.exchange(alice_pub)
    encaps_time = (time.perf_counter() - start) * 1000
    
    # Exchange (Bob decapsulates)
    start = time.perf_counter()
    alice_secret = x25519.X25519PrivateKey.from_private_bytes(
        alice_priv
    ).exchange(x25519.X25519PublicKey.from_public_bytes(bob_eph_priv[:32]))
    decaps_time = (time.perf_counter() - start) * 1000
    
    total_time = keygen_time + encaps_time + decaps_time
    
    metrics = KeyExchangeMetrics(
        algorithm=ClassicalX25519KeyExchange.NAME,
        keygen_time_ms=keygen_time,
        encaps_time_ms=encaps_time,
        decaps_time_ms=decaps_time,
        total_time_ms=total_time,
        public_key_size=len(alice_pub),
        private_key_size=len(alice_priv),
        ciphertext_size=len(bob_eph_priv),
        shared_secret_size=len(bob_secret),
        total_handshake_size=len(alice_pub) + len(bob_eph_priv)
    )
    
    print(f"Keygen Time:           {keygen_time:>8.3f} ms (both parties)")
    print(f"Encapsulation Time:    {encaps_time:>8.3f} ms")
    print(f"Decapsulation Time:    {decaps_time:>8.3f} ms")
    print(f"Total Round Trip:      {total_time:>8.3f} ms")
    print(f"\nPublic Key Size:       {len(alice_pub):>8} bytes")
    print(f"Private Key Size:      {len(alice_priv):>8} bytes")
    print(f"Ephemeral Secret Size: {len(bob_eph_priv):>8} bytes")
    print(f"Shared Secret Size:    {len(bob_secret):>8} bytes")
    print(f"Total Handshake Size:  {metrics.total_handshake_size:>8} bytes")
    
    return metrics


def benchmark_quantum_safe() -> KeyExchangeMetrics:
    """Benchmark pure ML-KEM-768 exchange"""
    print("\n" + "="*70)
    print("BENCHMARK: ML-KEM-768 (Quantum-Safe) Key Exchange")
    print("="*70)
    
    # Keygen
    start = time.perf_counter()
    alice_priv, alice_pub = QuantumSafeMLKEM768KeyExchange.keygen()
    bob_priv, bob_pub = QuantumSafeMLKEM768KeyExchange.keygen()
    keygen_time = (time.perf_counter() - start) * 1000
    
    # Encapsulation (Bob encapsulates for Alice)
    start = time.perf_counter()
    bob_ct, bob_secret = QuantumSafeMLKEM768KeyExchange.exchange(alice_pub)
    encaps_time = (time.perf_counter() - start) * 1000
    
    # Decapsulation (Alice decapsulates)
    start = time.perf_counter()
    kem = oqs.KeyEncapsulation("ML-KEM-768", secret_key=alice_priv)
    alice_secret = kem.decap_secret(bob_ct)
    kem.free()
    decaps_time = (time.perf_counter() - start) * 1000
    
    total_time = keygen_time + encaps_time + decaps_time
    
    metrics = KeyExchangeMetrics(
        algorithm=QuantumSafeMLKEM768KeyExchange.NAME,
        keygen_time_ms=keygen_time,
        encaps_time_ms=encaps_time,
        decaps_time_ms=decaps_time,
        total_time_ms=total_time,
        public_key_size=len(alice_pub),
        private_key_size=len(alice_priv),
        ciphertext_size=len(bob_ct),
        shared_secret_size=len(bob_secret),
        total_handshake_size=len(alice_pub) + len(bob_ct)
    )
    
    print(f"Keygen Time:           {keygen_time:>8.3f} ms (both parties)")
    print(f"Encapsulation Time:    {encaps_time:>8.3f} ms")
    print(f"Decapsulation Time:    {decaps_time:>8.3f} ms")
    print(f"Total Round Trip:      {total_time:>8.3f} ms")
    print(f"\nPublic Key Size:       {len(alice_pub):>8} bytes")
    print(f"Private Key Size:      {len(alice_priv):>8} bytes")
    print(f"Ciphertext Size:       {len(bob_ct):>8} bytes")
    print(f"Shared Secret Size:    {len(bob_secret):>8} bytes")
    print(f"Total Handshake Size:  {metrics.total_handshake_size:>8} bytes")
    
    return metrics


def benchmark_hybrid() -> KeyExchangeMetrics:
    """Benchmark hybrid X25519 + ML-KEM-768 exchange"""
    print("\n" + "="*70)
    print("BENCHMARK: Hybrid (X25519 + ML-KEM-768) Key Exchange")
    print("="*70)
    
    # Keygen
    start = time.perf_counter()
    alice_privs, alice_pubs = HybridX25519ML_KEM768KeyExchange.keygen()
    bob_privs, bob_pubs = HybridX25519ML_KEM768KeyExchange.keygen()
    keygen_time = (time.perf_counter() - start) * 1000
    
    # Exchange
    start = time.perf_counter()
    bob_ephemerals, bob_secret = HybridX25519ML_KEM768KeyExchange.exchange(
        alice_pubs
    )
    exchange_time = (time.perf_counter() - start) * 1000
    
    # Decapsulation (Alice decapsulates)
    start = time.perf_counter()
    # X25519 part
    alice_x25519_secret = x25519.X25519PrivateKey.from_private_bytes(
        alice_privs['x25519']
    ).exchange(x25519.X25519PublicKey.from_public_bytes(bob_ephemerals['x25519'][:32]))
    
    # ML-KEM part
    kem = oqs.KeyEncapsulation("ML-KEM-768", secret_key=alice_privs['ml_kem_768'])
    alice_mlkem_secret = kem.decap_secret(bob_ephemerals['ml_kem_768_ciphertext'])
    kem.free()
    
    # Combine
    alice_secret = hashlib.sha256(
        b'HYBRID_KDF' + alice_x25519_secret + alice_mlkem_secret
    ).digest()
    decaps_time = (time.perf_counter() - start) * 1000
    
    total_time = keygen_time + exchange_time + decaps_time
    
    total_handshake = (
        len(alice_pubs['x25519']) +
        len(alice_pubs['ml_kem_768']) +
        len(bob_ephemerals['x25519']) +
        len(bob_ephemerals['ml_kem_768_ciphertext'])
    )
    
    metrics = KeyExchangeMetrics(
        algorithm=HybridX25519ML_KEM768KeyExchange.NAME,
        keygen_time_ms=keygen_time,
        encaps_time_ms=exchange_time,
        decaps_time_ms=decaps_time,
        total_time_ms=total_time,
        public_key_size=len(alice_pubs['x25519']) + len(alice_pubs['ml_kem_768']),
        private_key_size=len(alice_privs['x25519']) + len(alice_privs['ml_kem_768']),
        ciphertext_size=len(bob_ephemerals['x25519']) + len(bob_ephemerals['ml_kem_768_ciphertext']),
        shared_secret_size=len(bob_secret),
        total_handshake_size=total_handshake
    )
    
    print(f"Keygen Time:           {keygen_time:>8.3f} ms (both parties)")
    print(f"Exchange Time:         {exchange_time:>8.3f} ms")
    print(f"Decapsulation Time:    {decaps_time:>8.3f} ms")
    print(f"Total Round Trip:      {total_time:>8.3f} ms")
    print(f"\nCombined Public Key:   {metrics.public_key_size:>8} bytes")
    print(f"  X25519:              {len(alice_pubs['x25519']):>8} bytes")
    print(f"  ML-KEM-768:          {len(alice_pubs['ml_kem_768']):>8} bytes")
    print(f"\nCombined Ciphertext:   {metrics.ciphertext_size:>8} bytes")
    print(f"  X25519 Ephemeral:    {len(bob_ephemerals['x25519']):>8} bytes")
    print(f"  ML-KEM Ciphertext:   {len(bob_ephemerals['ml_kem_768_ciphertext']):>8} bytes")
    print(f"Shared Secret Size:    {len(bob_secret):>8} bytes")
    print(f"Total Handshake Size:  {metrics.total_handshake_size:>8} bytes")
    
    return metrics


def print_comparison_table(metrics_list: list):
    """Print comparison table of all benchmarks"""
    print("\n" + "="*70)
    print("COMPARATIVE SUMMARY")
    print("="*70)
    print()
    
    headers = [
        "Metric",
        "Classical X25519",
        "Quantum-Safe ML-KEM",
        "Hybrid X25519+ML-KEM"
    ]
    
    print(f"{headers[0]:<30} {headers[1]:>15} {headers[2]:>15} {headers[3]:>18}")
    print("-" * 80)
    
    metrics_data = [m for m in metrics_list]
    
    # Performance metrics
    print("PERFORMANCE (milliseconds):")
    print(f"{'Keygen Time':<30} {metrics_data[0].keygen_time_ms:>15.3f} {metrics_data[1].keygen_time_ms:>15.3f} {metrics_data[2].keygen_time_ms:>18.3f}")
    print(f"{'Encaps/Exchange Time':<30} {metrics_data[0].encaps_time_ms:>15.3f} {metrics_data[1].encaps_time_ms:>15.3f} {metrics_data[2].encaps_time_ms:>18.3f}")
    print(f"{'Decaps Time':<30} {metrics_data[0].decaps_time_ms:>15.3f} {metrics_data[1].decaps_time_ms:>15.3f} {metrics_data[2].decaps_time_ms:>18.3f}")
    print(f"{'Total Round Trip':<30} {metrics_data[0].total_time_ms:>15.3f} {metrics_data[1].total_time_ms:>15.3f} {metrics_data[2].total_time_ms:>18.3f}")
    
    print("\nSIZES (bytes):")
    print(f"{'Public Key Size':<30} {metrics_data[0].public_key_size:>15} {metrics_data[1].public_key_size:>15} {metrics_data[2].public_key_size:>18}")
    print(f"{'Private Key Size':<30} {metrics_data[0].private_key_size:>15} {metrics_data[1].private_key_size:>15} {metrics_data[2].private_key_size:>18}")
    print(f"{'Ciphertext/Ephemeral':<30} {metrics_data[0].ciphertext_size:>15} {metrics_data[1].ciphertext_size:>15} {metrics_data[2].ciphertext_size:>18}")
    print(f"{'Shared Secret':<30} {metrics_data[0].shared_secret_size:>15} {metrics_data[1].shared_secret_size:>15} {metrics_data[2].shared_secret_size:>18}")
    print(f"{'Total Handshake Size':<30} {metrics_data[0].total_handshake_size:>15} {metrics_data[1].total_handshake_size:>15} {metrics_data[2].total_handshake_size:>18}")
    
    print("\nOVERHEAD RELATIVE TO X25519:")
    for i, metric_name in enumerate([
        "Total Round Trip",
        "Public Key Size",
        "Ciphertext Size",
        "Handshake Size"
    ]):
        if i == 0:
            base = metrics_data[0].total_time_ms
            val1 = metrics_data[1].total_time_ms
            val2 = metrics_data[2].total_time_ms
            unit = "×"
        elif i == 1:
            base = metrics_data[0].public_key_size
            val1 = metrics_data[1].public_key_size
            val2 = metrics_data[2].public_key_size
            unit = "×"
        elif i == 2:
            base = metrics_data[0].ciphertext_size
            val1 = metrics_data[1].ciphertext_size
            val2 = metrics_data[2].ciphertext_size
            unit = "×"
        else:
            base = metrics_data[0].total_handshake_size
            val1 = metrics_data[1].total_handshake_size
            val2 = metrics_data[2].total_handshake_size
            unit = "×"
        
        ratio1 = val1 / base if base > 0 else 0
        ratio2 = val2 / base if base > 0 else 0
        
        print(f"{metric_name:<30} {ratio1:>15.1f}{unit} {ratio2:>18.1f}{unit}")


def main():
    """Main benchmark orchestration"""
    print("\n")
    print("╔" + "="*68 + "╗")
    print("║" + " "*68 + "║")
    print("║" + "  FIPS 203 Hybrid Key Exchange Benchmarking".center(68) + "║")
    print("║" + "  Classical vs. Post-Quantum vs. Hybrid".center(68) + "║")
    print("║" + " "*68 + "║")
    print("╚" + "="*68 + "╝")
    
    try:
        # Run benchmarks
        metrics_classical = benchmark_classical()
        metrics_quantum = benchmark_quantum_safe()
        metrics_hybrid = benchmark_hybrid()
        
        # Print comparison
        print_comparison_table([metrics_classical, metrics_quantum, metrics_hybrid])
        
        # Save results to JSON
        results = {
            "classical": asdict(metrics_classical),
            "quantum_safe": asdict(metrics_quantum),
            "hybrid": asdict(metrics_hybrid),
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        
        output_file = "/home/claude/hybrid_kex_results.json"
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        
        print(f"\n✓ Results saved to: {output_file}")
        
        # Print key insights
        print("\n" + "="*70)
        print("KEY INSIGHTS")
        print("="*70)
        overhead_time = ((metrics_hybrid.total_time_ms / metrics_classical.total_time_ms) - 1) * 100
        overhead_size = ((metrics_hybrid.total_handshake_size / metrics_classical.total_handshake_size) - 1) * 100
        
        print(f"\n✓ Hybrid approach adds ~{overhead_time:.1f}% latency overhead")
        print(f"✓ Hybrid approach adds ~{overhead_size:.0f}% to handshake size")
        print(f"\n✓ Hybrid ensures security even if ONE algorithm is broken")
        print(f"✓ ML-KEM-768 is {metrics_quantum.total_time_ms/metrics_classical.total_time_ms:.1f}x slower than X25519")
        print(f"✓ ML-KEM-768 is {metrics_quantum.total_handshake_size/metrics_classical.total_handshake_size:.0f}x larger than X25519")
        print(f"\n✓ Deployment: Begin with X25519+ML-KEM-768 hybrid in TLS 1.3")
        print(f"✓ Timeline: Plan full migration to post-quantum by 2027-2030")
        
    except Exception as e:
        print(f"\n✗ ERROR: {e}")
        traceback.print_exc()
        sys.exit(1)
    
    print("\n")


if __name__ == "__main__":
    main()
