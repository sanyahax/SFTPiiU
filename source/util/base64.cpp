#include "base64.h"

namespace util {

static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const uint8_t *data, size_t len, bool pad) {
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 3 <= len; i += 3) {
        uint32_t v = ((uint32_t) data[i] << 16) | ((uint32_t) data[i + 1] << 8) | data[i + 2];
        out += kAlphabet[(v >> 18) & 63];
        out += kAlphabet[(v >> 12) & 63];
        out += kAlphabet[(v >> 6) & 63];
        out += kAlphabet[v & 63];
    }
    size_t rest = len - i;
    if (rest == 1) {
        uint32_t v = (uint32_t) data[i] << 16;
        out += kAlphabet[(v >> 18) & 63];
        out += kAlphabet[(v >> 12) & 63];
        if (pad) {
            out += "==";
        }
    } else if (rest == 2) {
        uint32_t v = ((uint32_t) data[i] << 16) | ((uint32_t) data[i + 1] << 8);
        out += kAlphabet[(v >> 18) & 63];
        out += kAlphabet[(v >> 12) & 63];
        out += kAlphabet[(v >> 6) & 63];
        if (pad) {
            out += '=';
        }
    }
    return out;
}

static int value_of(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool base64_decode(std::string_view in, std::vector<uint8_t> &out) {
    out.clear();
    uint32_t acc = 0;
    int bits     = 0;
    for (char ch : in) {
        unsigned char c = (unsigned char) ch;
        if (c == '=') {
            break;
        }
        int v = value_of(c);
        if (v < 0) {
            return false;
        }
        acc = (acc << 6) | (uint32_t) v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t) (acc >> bits));
        }
    }
    return true;
}

} // namespace util
