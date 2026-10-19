#pragma once
// Endian-neutral load/store helpers. The Wii U is big-endian, the test host is
// little-endian, so crypto code must never type-pun multi-byte integers.

#include <cstddef>
#include <cstdint>

namespace crypto {

inline uint32_t load_le32(const uint8_t *p) {
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

inline void store_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t) v;
    p[1] = (uint8_t) (v >> 8);
    p[2] = (uint8_t) (v >> 16);
    p[3] = (uint8_t) (v >> 24);
}

inline uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | (uint32_t) p[3];
}

inline void store_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t) (v >> 24);
    p[1] = (uint8_t) (v >> 16);
    p[2] = (uint8_t) (v >> 8);
    p[3] = (uint8_t) v;
}

inline uint64_t load_be64(const uint8_t *p) {
    return ((uint64_t) load_be32(p) << 32) | load_be32(p + 4);
}

inline void store_be64(uint8_t *p, uint64_t v) {
    store_be32(p, (uint32_t) (v >> 32));
    store_be32(p + 4, (uint32_t) v);
}

inline uint32_t rotl32(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

inline uint32_t rotr32(uint32_t v, int n) {
    return (v >> n) | (v << (32 - n));
}

inline uint64_t rotr64(uint64_t v, int n) {
    return (v >> n) | (v << (64 - n));
}

// Constant-time comparison, returns true when equal.
inline bool ct_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++) {
        d |= a[i] ^ b[i];
    }
    return d == 0;
}

inline void secure_zero(void *p, size_t n) {
    volatile uint8_t *v = (volatile uint8_t *) p;
    while (n--) {
        *v++ = 0;
    }
}

} // namespace crypto
