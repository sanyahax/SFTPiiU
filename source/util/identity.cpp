#include "identity.h"
#include "../crypto/random.h"
#include "../crypto/sha2.h"
#include "base64.h"

#include <cstring>

namespace util {

std::string to_hex(const uint8_t *p, size_t n) {
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s += digits[p[i] >> 4];
        s += digits[p[i] & 15];
    }
    return s;
}

bool from_hex(const std::string &s, uint8_t *out, size_t n) {
    if (s.size() != n * 2) {
        return false;
    }
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < n; i++) {
        int hi = nib(s[2 * i]), lo = nib(s[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t) ((hi << 4) | lo);
    }
    return true;
}

std::string generate_password() {
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    const unsigned n             = sizeof(alphabet) - 1;
    std::string pw;
    while (pw.size() < 14) {
        if (pw.size() == 4 || pw.size() == 9) {
            pw += '-';
            continue;
        }
        uint8_t b;
        crypto::rng_bytes(&b, 1);
        if (b >= 256 - (256 % n)) {
            continue; // rejection sampling keeps the distribution uniform
        }
        pw += alphabet[b % n];
    }
    return pw;
}

std::string host_fingerprint(const uint8_t publicKey[32]) {
    uint8_t blob[51] = {0, 0, 0, 11};
    memcpy(blob + 4, "ssh-ed25519", 11);
    blob[18] = 32;
    memcpy(blob + 19, publicKey, 32);
    uint8_t h[32];
    crypto::sha256(blob, sizeof(blob), h);
    return "SHA256:" + base64_encode(h, sizeof(h), false);
}

} // namespace util
