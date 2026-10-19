#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto {

// AES encryption (128/256-bit keys) used in CTR mode for aes*-ctr.
struct AesCtr {
    uint32_t rk[60];
    int rounds = 0;
    uint8_t ctr[16];
    uint8_t ks[16];
    size_t ks_used = 16;

    void set_key(const uint8_t *key, size_t keylen, const uint8_t iv[16]);
    void crypt(uint8_t *data, size_t len);

    static void encrypt_block(const uint32_t *rk, int rounds, const uint8_t in[16], uint8_t out[16]);
};

} // namespace crypto
