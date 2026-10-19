#include "aes.h"
#include "bytes.h"

#include <mutex>

namespace crypto {

static uint8_t sbox[256];
static uint32_t Te0[256], Te1[256], Te2[256], Te3[256];
static std::once_flag tablesOnce;

static inline uint8_t rotl8(uint8_t x, int s) {
    return (uint8_t) ((x << s) | (x >> (8 - s)));
}

static inline uint8_t xtime(uint8_t x) {
    return (uint8_t) ((x << 1) ^ ((x & 0x80) ? 0x1b : 0));
}

static void build_tables() {
    // S-box from multiplicative inverses in GF(2^8) plus the affine transform.
    uint8_t p = 1, q = 1;
    do {
        p = (uint8_t) (p ^ (p << 1) ^ ((p & 0x80) ? 0x1b : 0));
        q ^= (uint8_t) (q << 1);
        q ^= (uint8_t) (q << 2);
        q ^= (uint8_t) (q << 4);
        if (q & 0x80) {
            q ^= 0x09;
        }
        uint8_t x = (uint8_t) (q ^ rotl8(q, 1) ^ rotl8(q, 2) ^ rotl8(q, 3) ^ rotl8(q, 4));
        sbox[p]   = x ^ 0x63;
    } while (p != 1);
    sbox[0] = 0x63;

    for (int i = 0; i < 256; i++) {
        uint8_t s  = sbox[i];
        uint8_t s2 = xtime(s);
        uint8_t s3 = s2 ^ s;
        uint32_t t = ((uint32_t) s2 << 24) | ((uint32_t) s << 16) | ((uint32_t) s << 8) | s3;
        Te0[i]     = t;
        Te1[i]     = rotr32(t, 8);
        Te2[i]     = rotr32(t, 16);
        Te3[i]     = rotr32(t, 24);
    }
}

static uint32_t sub_word(uint32_t w) {
    return ((uint32_t) sbox[w >> 24] << 24) | ((uint32_t) sbox[(w >> 16) & 0xff] << 16) |
           ((uint32_t) sbox[(w >> 8) & 0xff] << 8) | sbox[w & 0xff];
}

void AesCtr::set_key(const uint8_t *key, size_t keylen, const uint8_t iv[16]) {
    std::call_once(tablesOnce, build_tables);
    int nk = (int) keylen / 4;
    rounds = nk + 6;
    int total = 4 * (rounds + 1);
    for (int i = 0; i < nk; i++) {
        rk[i] = load_be32(key + 4 * i);
    }
    uint8_t rcon = 1;
    for (int i = nk; i < total; i++) {
        uint32_t t = rk[i - 1];
        if (i % nk == 0) {
            t    = sub_word((t << 8) | (t >> 24)) ^ ((uint32_t) rcon << 24);
            rcon = xtime(rcon);
        } else if (nk > 6 && i % nk == 4) {
            t = sub_word(t);
        }
        rk[i] = rk[i - nk] ^ t;
    }
    for (int i = 0; i < 16; i++) {
        ctr[i] = iv[i];
    }
    ks_used = 16;
}

void AesCtr::encrypt_block(const uint32_t *rk, int rounds, const uint8_t in[16], uint8_t out[16]) {
    uint32_t s0 = load_be32(in) ^ rk[0];
    uint32_t s1 = load_be32(in + 4) ^ rk[1];
    uint32_t s2 = load_be32(in + 8) ^ rk[2];
    uint32_t s3 = load_be32(in + 12) ^ rk[3];
    for (int r = 1; r < rounds; r++) {
        const uint32_t *k = rk + 4 * r;
        uint32_t t0 = Te0[s0 >> 24] ^ Te1[(s1 >> 16) & 0xff] ^ Te2[(s2 >> 8) & 0xff] ^ Te3[s3 & 0xff] ^ k[0];
        uint32_t t1 = Te0[s1 >> 24] ^ Te1[(s2 >> 16) & 0xff] ^ Te2[(s3 >> 8) & 0xff] ^ Te3[s0 & 0xff] ^ k[1];
        uint32_t t2 = Te0[s2 >> 24] ^ Te1[(s3 >> 16) & 0xff] ^ Te2[(s0 >> 8) & 0xff] ^ Te3[s1 & 0xff] ^ k[2];
        uint32_t t3 = Te0[s3 >> 24] ^ Te1[(s0 >> 16) & 0xff] ^ Te2[(s1 >> 8) & 0xff] ^ Te3[s2 & 0xff] ^ k[3];
        s0 = t0;
        s1 = t1;
        s2 = t2;
        s3 = t3;
    }
    const uint32_t *k = rk + 4 * rounds;
    uint32_t o0 = ((uint32_t) sbox[s0 >> 24] << 24) ^ ((uint32_t) sbox[(s1 >> 16) & 0xff] << 16) ^
                  ((uint32_t) sbox[(s2 >> 8) & 0xff] << 8) ^ sbox[s3 & 0xff] ^ k[0];
    uint32_t o1 = ((uint32_t) sbox[s1 >> 24] << 24) ^ ((uint32_t) sbox[(s2 >> 16) & 0xff] << 16) ^
                  ((uint32_t) sbox[(s3 >> 8) & 0xff] << 8) ^ sbox[s0 & 0xff] ^ k[1];
    uint32_t o2 = ((uint32_t) sbox[s2 >> 24] << 24) ^ ((uint32_t) sbox[(s3 >> 16) & 0xff] << 16) ^
                  ((uint32_t) sbox[(s0 >> 8) & 0xff] << 8) ^ sbox[s1 & 0xff] ^ k[2];
    uint32_t o3 = ((uint32_t) sbox[s3 >> 24] << 24) ^ ((uint32_t) sbox[(s0 >> 16) & 0xff] << 16) ^
                  ((uint32_t) sbox[(s1 >> 8) & 0xff] << 8) ^ sbox[s2 & 0xff] ^ k[3];
    store_be32(out, o0);
    store_be32(out + 4, o1);
    store_be32(out + 8, o2);
    store_be32(out + 12, o3);
}

void AesCtr::crypt(uint8_t *data, size_t len) {
    while (len) {
        if (ks_used == 16) {
            encrypt_block(rk, rounds, ctr, ks);
            for (int i = 15; i >= 0; i--) {
                if (++ctr[i] != 0) {
                    break;
                }
            }
            ks_used = 0;
        }
        size_t n = 16 - ks_used;
        if (n > len) {
            n = len;
        }
        for (size_t i = 0; i < n; i++) {
            data[i] ^= ks[ks_used + i];
        }
        ks_used += n;
        data += n;
        len -= n;
    }
}

} // namespace crypto
