#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto {

// X25519 (RFC 7748). Returns false if the shared secret is all zero.
bool x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]);
void x25519_base(uint8_t out[32], const uint8_t scalar[32]);

// Ed25519 (RFC 8032). Secret key layout matches OpenSSH/NaCl: seed(32) || public(32).
void ed25519_keypair_from_seed(const uint8_t seed[32], uint8_t pk[32], uint8_t sk[64]);
void ed25519_sign(uint8_t sig[64], const uint8_t *msg, size_t len, const uint8_t sk[64]);
bool ed25519_verify(const uint8_t sig[64], const uint8_t *msg, size_t len, const uint8_t pk[32]);

} // namespace crypto
