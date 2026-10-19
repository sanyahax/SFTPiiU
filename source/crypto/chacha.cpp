#include "chacha.h"
#include "bytes.h"

namespace crypto {

#define QR(a, b, c, d)                \
    a += b;                           \
    d = rotl32(d ^ a, 16);            \
    c += d;                           \
    b = rotl32(b ^ c, 12);            \
    a += b;                           \
    d = rotl32(d ^ a, 8);             \
    c += d;                           \
    b = rotl32(b ^ c, 7)

static void chacha_block(const uint32_t in[16], uint8_t out[64]) {
    uint32_t x[16];
    for (int i = 0; i < 16; i++) {
        x[i] = in[i];
    }
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; i++) {
        store_le32(out + 4 * i, x[i] + in[i]);
    }
}

void ChaCha20::set_key(const uint8_t k[32]) {
    for (int i = 0; i < 8; i++) {
        key[i] = load_le32(k + 4 * i);
    }
}

void ChaCha20::crypt(const uint8_t nonce[8], uint64_t counter, const uint8_t *in, uint8_t *out, size_t len) const {
    uint32_t st[16];
    st[0] = 0x61707865;
    st[1] = 0x3320646e;
    st[2] = 0x79622d32;
    st[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) {
        st[4 + i] = key[i];
    }
    st[14] = load_le32(nonce);
    st[15] = load_le32(nonce + 4);
    uint8_t ks[64];
    while (len) {
        st[12] = (uint32_t) counter;
        st[13] = (uint32_t) (counter >> 32);
        chacha_block(st, ks);
        size_t n = len < 64 ? len : 64;
        for (size_t i = 0; i < n; i++) {
            out[i] = in[i] ^ ks[i];
        }
        in += n;
        out += n;
        len -= n;
        counter++;
    }
    secure_zero(ks, sizeof(ks));
}

// poly1305-donna, 32-bit limbs (26 bits each).
void poly1305(uint8_t mac[16], const uint8_t *m, size_t len, const uint8_t key[32]) {
    const uint32_t r0 = (load_le32(key + 0)) & 0x3ffffff;
    const uint32_t r1 = (load_le32(key + 3) >> 2) & 0x3ffff03;
    const uint32_t r2 = (load_le32(key + 6) >> 4) & 0x3ffc0ff;
    const uint32_t r3 = (load_le32(key + 9) >> 6) & 0x3f03fff;
    const uint32_t r4 = (load_le32(key + 12) >> 8) & 0x00fffff;
    const uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;

    uint8_t last[16];
    while (len) {
        const uint8_t *blk;
        uint32_t hibit;
        if (len >= 16) {
            blk   = m;
            hibit = 1u << 24;
            m += 16;
            len -= 16;
        } else {
            size_t i = 0;
            for (; i < len; i++) {
                last[i] = m[i];
            }
            last[i++] = 1;
            for (; i < 16; i++) {
                last[i] = 0;
            }
            blk   = last;
            hibit = 0;
            len   = 0;
        }

        h0 += (load_le32(blk + 0)) & 0x3ffffff;
        h1 += (load_le32(blk + 3) >> 2) & 0x3ffffff;
        h2 += (load_le32(blk + 6) >> 4) & 0x3ffffff;
        h3 += (load_le32(blk + 9) >> 6) & 0x3ffffff;
        h4 += (load_le32(blk + 12) >> 8) | hibit;

        uint64_t d0 = (uint64_t) h0 * r0 + (uint64_t) h1 * s4 + (uint64_t) h2 * s3 + (uint64_t) h3 * s2 + (uint64_t) h4 * s1;
        uint64_t d1 = (uint64_t) h0 * r1 + (uint64_t) h1 * r0 + (uint64_t) h2 * s4 + (uint64_t) h3 * s3 + (uint64_t) h4 * s2;
        uint64_t d2 = (uint64_t) h0 * r2 + (uint64_t) h1 * r1 + (uint64_t) h2 * r0 + (uint64_t) h3 * s4 + (uint64_t) h4 * s3;
        uint64_t d3 = (uint64_t) h0 * r3 + (uint64_t) h1 * r2 + (uint64_t) h2 * r1 + (uint64_t) h3 * r0 + (uint64_t) h4 * s4;
        uint64_t d4 = (uint64_t) h0 * r4 + (uint64_t) h1 * r3 + (uint64_t) h2 * r2 + (uint64_t) h3 * r1 + (uint64_t) h4 * r0;

        uint32_t c;
        c  = (uint32_t) (d0 >> 26);
        h0 = (uint32_t) d0 & 0x3ffffff;
        d1 += c;
        c  = (uint32_t) (d1 >> 26);
        h1 = (uint32_t) d1 & 0x3ffffff;
        d2 += c;
        c  = (uint32_t) (d2 >> 26);
        h2 = (uint32_t) d2 & 0x3ffffff;
        d3 += c;
        c  = (uint32_t) (d3 >> 26);
        h3 = (uint32_t) d3 & 0x3ffffff;
        d4 += c;
        c  = (uint32_t) (d4 >> 26);
        h4 = (uint32_t) d4 & 0x3ffffff;
        h0 += c * 5;
        c  = h0 >> 26;
        h0 = h0 & 0x3ffffff;
        h1 += c;
    }

    uint32_t c;
    c  = h1 >> 26;
    h1 = h1 & 0x3ffffff;
    h2 += c;
    c  = h2 >> 26;
    h2 = h2 & 0x3ffffff;
    h3 += c;
    c  = h3 >> 26;
    h3 = h3 & 0x3ffffff;
    h4 += c;
    c  = h4 >> 26;
    h4 = h4 & 0x3ffffff;
    h0 += c * 5;
    c  = h0 >> 26;
    h0 = h0 & 0x3ffffff;
    h1 += c;

    uint32_t g0 = h0 + 5;
    c           = g0 >> 26;
    g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c;
    c           = g1 >> 26;
    g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c;
    c           = g2 >> 26;
    g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c;
    c           = g3 >> 26;
    g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1u << 26);

    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask;
    g1 &= mask;
    g2 &= mask;
    g3 &= mask;
    g4 &= mask;
    mask = ~mask;
    h0   = (h0 & mask) | g0;
    h1   = (h1 & mask) | g1;
    h2   = (h2 & mask) | g2;
    h3   = (h3 & mask) | g3;
    h4   = (h4 & mask) | g4;

    h0 = (h0) | (h1 << 26);
    h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 << 8);

    uint64_t f;
    f  = (uint64_t) h0 + load_le32(key + 16);
    h0 = (uint32_t) f;
    f  = (uint64_t) h1 + load_le32(key + 20) + (f >> 32);
    h1 = (uint32_t) f;
    f  = (uint64_t) h2 + load_le32(key + 24) + (f >> 32);
    h2 = (uint32_t) f;
    f  = (uint64_t) h3 + load_le32(key + 28) + (f >> 32);
    h3 = (uint32_t) f;

    store_le32(mac + 0, h0);
    store_le32(mac + 4, h1);
    store_le32(mac + 8, h2);
    store_le32(mac + 12, h3);
}

void ChaChaPolySsh::set_key(const uint8_t key[64]) {
    main.set_key(key);
    header.set_key(key + 32);
}

uint32_t ChaChaPolySsh::decrypt_length(uint32_t seq, const uint8_t enc[4]) const {
    uint8_t nonce[8];
    store_be64(nonce, seq);
    uint8_t plain[4];
    header.crypt(nonce, 0, enc, plain, 4);
    return load_be32(plain);
}

void ChaChaPolySsh::seal(uint32_t seq, uint8_t *buf, size_t len, uint8_t tag[16]) const {
    uint8_t nonce[8];
    store_be64(nonce, seq);
    uint8_t polykey[32] = {0};
    main.crypt(nonce, 0, polykey, polykey, 32);
    header.crypt(nonce, 0, buf, buf, 4);
    main.crypt(nonce, 1, buf + 4, buf + 4, len - 4);
    poly1305(tag, buf, len, polykey);
    secure_zero(polykey, sizeof(polykey));
}

bool ChaChaPolySsh::open(uint32_t seq, uint8_t *buf, size_t len, const uint8_t tag[16]) const {
    uint8_t nonce[8];
    store_be64(nonce, seq);
    uint8_t polykey[32] = {0};
    main.crypt(nonce, 0, polykey, polykey, 32);
    uint8_t expect[16];
    poly1305(expect, buf, len, polykey);
    secure_zero(polykey, sizeof(polykey));
    if (!ct_equal(expect, tag, 16)) {
        return false;
    }
    header.crypt(nonce, 0, buf, buf, 4);
    main.crypt(nonce, 1, buf + 4, buf + 4, len - 4);
    return true;
}

} // namespace crypto
