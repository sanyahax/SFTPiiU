#pragma once
// SSH wire-format encoding (RFC 4251 section 5), always big-endian.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace ssh {

class Writer {
public:
    std::vector<uint8_t> buf;

    void u8(uint8_t v) { buf.push_back(v); }

    void boolean(bool v) { buf.push_back(v ? 1 : 0); }

    void u32(uint32_t v) {
        uint8_t b[4] = {(uint8_t) (v >> 24), (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v};
        buf.insert(buf.end(), b, b + 4);
    }

    void u64(uint64_t v) {
        u32((uint32_t) (v >> 32));
        u32((uint32_t) v);
    }

    void raw(const void *p, size_t n) {
        const uint8_t *b = (const uint8_t *) p;
        buf.insert(buf.end(), b, b + n);
    }

    void str(const void *p, size_t n) {
        u32((uint32_t) n);
        raw(p, n);
    }

    void str(std::string_view s) { str(s.data(), s.size()); }

    // Unsigned big-endian magnitude as mpint: strip leading zeros, add a zero
    // byte when the top bit is set.
    void mpint(const uint8_t *be, size_t n) {
        while (n && be[0] == 0) {
            be++;
            n--;
        }
        if (n && (be[0] & 0x80)) {
            u32((uint32_t) n + 1);
            u8(0);
            raw(be, n);
        } else {
            str(be, n);
        }
    }

    void patch_u32(size_t at, uint32_t v) {
        buf[at]     = (uint8_t) (v >> 24);
        buf[at + 1] = (uint8_t) (v >> 16);
        buf[at + 2] = (uint8_t) (v >> 8);
        buf[at + 3] = (uint8_t) v;
    }

    size_t size() const { return buf.size(); }
    const uint8_t *data() const { return buf.data(); }
    void clear() { buf.clear(); }
};

// Bounds-checked reader. Any overrun latches ok = false and yields zeros, so
// callers can parse a whole message and check ok once at the end.
class Reader {
public:
    Reader(const uint8_t *p, size_t n) : p_(p), n_(n) {}

    bool ok = true;

    size_t remaining() const { return n_ - pos_; }
    const uint8_t *cursor() const { return p_ + pos_; }

    uint8_t u8() {
        if (!need(1)) {
            return 0;
        }
        return p_[pos_++];
    }

    bool boolean() { return u8() != 0; }

    uint32_t u32() {
        if (!need(4)) {
            return 0;
        }
        const uint8_t *b = p_ + pos_;
        pos_ += 4;
        return ((uint32_t) b[0] << 24) | ((uint32_t) b[1] << 16) | ((uint32_t) b[2] << 8) | b[3];
    }

    uint64_t u64() {
        uint64_t hi = u32();
        return (hi << 32) | u32();
    }

    // Returns a view into the underlying buffer.
    std::string_view str() {
        uint32_t len = u32();
        if (!ok || !need(len)) {
            return {};
        }
        std::string_view v((const char *) p_ + pos_, len);
        pos_ += len;
        return v;
    }

    const uint8_t *bytes(size_t n) {
        if (!need(n)) {
            return nullptr;
        }
        const uint8_t *b = p_ + pos_;
        pos_ += n;
        return b;
    }

private:
    bool need(size_t n) {
        if (!ok || n > n_ - pos_) {
            ok = false;
            return false;
        }
        return true;
    }

    const uint8_t *p_;
    size_t n_;
    size_t pos_ = 0;
};

inline const uint8_t *u8p(std::string_view s) {
    return (const uint8_t *) s.data();
}

// True if name appears in a comma-separated name-list.
inline bool namelist_has(std::string_view list, std::string_view name) {
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find(',', start);
        if (end == std::string_view::npos) {
            end = list.size();
        }
        if (list.substr(start, end - start) == name) {
            return true;
        }
        start = end + 1;
    }
    return false;
}

// First entry of the client's list that the server also supports (RFC 4253 7.1).
inline std::string namelist_pick(std::string_view client, std::string_view server) {
    size_t start = 0;
    while (start <= client.size()) {
        size_t end = client.find(',', start);
        if (end == std::string_view::npos) {
            end = client.size();
        }
        std::string_view n = client.substr(start, end - start);
        if (!n.empty() && namelist_has(server, n)) {
            return std::string(n);
        }
        start = end + 1;
    }
    return {};
}

} // namespace ssh
