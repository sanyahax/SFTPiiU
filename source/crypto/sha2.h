#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto {

struct Sha256 {
    uint32_t h[8];
    uint64_t total;
    uint8_t buf[64];
    size_t used;

    Sha256() { init(); }
    void init();
    void update(const void *data, size_t len);
    void final(uint8_t out[32]);
};

void sha256(const void *data, size_t len, uint8_t out[32]);

struct Sha512 {
    uint64_t h[8];
    uint64_t total;
    uint8_t buf[128];
    size_t used;

    Sha512() { init(); }
    void init();
    void update(const void *data, size_t len);
    void final(uint8_t out[64]);
};

void sha512(const void *data, size_t len, uint8_t out[64]);

// HMAC-SHA256 with the padded key states precomputed, so per-packet MACs only
// copy two contexts instead of re-hashing the key.
struct HmacSha256Key {
    Sha256 inner;
    Sha256 outer;

    void set_key(const uint8_t *key, size_t len);
    void mac(const uint8_t *a, size_t alen, const uint8_t *b, size_t blen, uint8_t out[32]) const;
};

} // namespace crypto
