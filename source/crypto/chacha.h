#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto {

// Original ChaCha20 (64-bit nonce, 64-bit block counter), as used by
// chacha20-poly1305@openssh.com.
struct ChaCha20 {
    uint32_t key[8];

    void set_key(const uint8_t k[32]);
    // XORs the keystream for (nonce, counter...) into data. out may equal in.
    void crypt(const uint8_t nonce[8], uint64_t counter, const uint8_t *in, uint8_t *out, size_t len) const;
};

void poly1305(uint8_t mac[16], const uint8_t *msg, size_t len, const uint8_t key[32]);

// chacha20-poly1305@openssh.com (see OpenSSH PROTOCOL.chacha20poly1305).
struct ChaChaPolySsh {
    ChaCha20 main;   // K_2: first 32 bytes of key material
    ChaCha20 header; // K_1: second 32 bytes, encrypts the length field

    void set_key(const uint8_t key[64]);
    uint32_t decrypt_length(uint32_t seq, const uint8_t enc[4]) const;
    // buf holds length(4) || packet, encrypted in place; tag gets 16 bytes.
    void seal(uint32_t seq, uint8_t *buf, size_t len, uint8_t tag[16]) const;
    // Verifies the tag over the ciphertext, then decrypts in place.
    bool open(uint32_t seq, uint8_t *buf, size_t len, const uint8_t tag[16]) const;
};

} // namespace crypto
