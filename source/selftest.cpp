#include "selftest.h"
#include "crypto/aes.h"
#include "crypto/bytes.h"
#include "crypto/chacha.h"
#include "crypto/curve25519.h"
#include "crypto/sha2.h"

#include <cstring>
#include <vector>

namespace {

std::vector<uint8_t> hex(const char *s) {
    std::vector<uint8_t> out;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return 0;
    };
    for (; s[0] && s[1]; s += 2) {
        out.push_back((uint8_t) ((nib(s[0]) << 4) | nib(s[1])));
    }
    return out;
}

bool eq(const uint8_t *a, const char *hexExpected) {
    auto e = hex(hexExpected);
    return memcmp(a, e.data(), e.size()) == 0;
}

} // namespace

const char *crypto_selftest() {
    uint8_t out[64];

    crypto::sha256("abc", 3, out);
    if (!eq(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) return "sha256";

    {
        // 1,000,000 x 'a' exercises multi-block updates and the length field.
        crypto::Sha256 h;
        uint8_t a[1000];
        memset(a, 'a', sizeof(a));
        for (int i = 0; i < 1000; i++) h.update(a, sizeof(a));
        h.final(out);
        if (!eq(out, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")) return "sha256-long";
    }

    crypto::sha512("abc", 3, out);
    if (!eq(out, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
                 "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"))
        return "sha512";

    {
        uint8_t key[20];
        memset(key, 0x0b, sizeof(key));
        crypto::HmacSha256Key k;
        k.set_key(key, sizeof(key));
        k.mac((const uint8_t *) "Hi ", 3, (const uint8_t *) "There", 5, out);
        if (!eq(out, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7")) return "hmac-sha256";
    }

    {
        auto a  = hex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
        auto b  = hex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
        uint8_t pa[32], pb[32], s1[32], s2[32];
        crypto::x25519_base(pa, a.data());
        crypto::x25519_base(pb, b.data());
        if (!eq(pa, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a")) return "x25519-pub";
        if (!eq(pb, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f")) return "x25519-pub2";
        crypto::x25519(s1, a.data(), pb);
        crypto::x25519(s2, b.data(), pa);
        if (!eq(s1, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742")) return "x25519-shared";
        if (memcmp(s1, s2, 32) != 0) return "x25519-agree";
    }

    {
        auto seed = hex("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
        uint8_t pk[32], sk[64], sig[64];
        crypto::ed25519_keypair_from_seed(seed.data(), pk, sk);
        if (!eq(pk, "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c")) return "ed25519-pub";
        uint8_t msg = 0x72;
        crypto::ed25519_sign(sig, &msg, 1, sk);
        if (!eq(sig, "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
                     "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"))
            return "ed25519-sign";
        if (!crypto::ed25519_verify(sig, &msg, 1, pk)) return "ed25519-verify";
        sig[10] ^= 1;
        if (crypto::ed25519_verify(sig, &msg, 1, pk)) return "ed25519-reject";
    }

    {
        uint8_t key[32] = {0}, nonce[8] = {0}, ks[64] = {0};
        crypto::ChaCha20 c;
        c.set_key(key);
        c.crypt(nonce, 0, ks, ks, sizeof(ks));
        if (!eq(ks, "76b8e0ada0f13d90405d6ae55386bd28bdd219b8a08ded1aa836efcc8b770dc7"
                    "da41597c5157488d7724e03fb8d84a376a43b8f41518a11cc387b669b2ee6586"))
            return "chacha20";
    }

    {
        auto key        = hex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b");
        const char *msg = "Cryptographic Forum Research Group";
        crypto::poly1305(out, (const uint8_t *) msg, strlen(msg), key.data());
        if (!eq(out, "a8061dc1305136c6c22b8baf0c0127a9")) return "poly1305";
    }

    {
        auto pt = hex("00112233445566778899aabbccddeeff");
        uint8_t key[32], iv[16] = {0};
        for (int i = 0; i < 32; i++) key[i] = (uint8_t) i;
        crypto::AesCtr a;
        a.set_key(key, 16, iv);
        crypto::AesCtr::encrypt_block(a.rk, a.rounds, pt.data(), out);
        if (!eq(out, "69c4e0d86a7b0430d8cdb78070b4c55a")) return "aes128";
        a.set_key(key, 24, iv);
        crypto::AesCtr::encrypt_block(a.rk, a.rounds, pt.data(), out);
        if (!eq(out, "dda97ca4864cdfe06eaf70a0ec0d7191")) return "aes192";
        a.set_key(key, 32, iv);
        crypto::AesCtr::encrypt_block(a.rk, a.rounds, pt.data(), out);
        if (!eq(out, "8ea2b7ca516745bfeafc49904b496089")) return "aes256";
    }

    {
        // chacha20-poly1305@openssh.com round trip, including tamper detection.
        uint8_t key[64];
        for (int i = 0; i < 64; i++) key[i] = (uint8_t) (i * 7);
        crypto::ChaChaPolySsh cp;
        cp.set_key(key);
        uint8_t pkt[4 + 32], tag[16];
        crypto::store_be32(pkt, 32);
        for (int i = 0; i < 32; i++) pkt[4 + i] = (uint8_t) i;
        cp.seal(5, pkt, sizeof(pkt), tag);
        if (cp.decrypt_length(5, pkt) != 32) return "chachapoly-len";
        uint8_t copy[sizeof(pkt)];
        memcpy(copy, pkt, sizeof(pkt));
        if (!cp.open(5, pkt, sizeof(pkt), tag) || pkt[4 + 31] != 31) return "chachapoly-open";
        copy[8] ^= 1;
        if (cp.open(5, copy, sizeof(copy), tag)) return "chachapoly-tamper";
    }

    return nullptr;
}
