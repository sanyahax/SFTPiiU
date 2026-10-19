#include "format.h"
#include "i18n.h"

#include <cstdio>

namespace {

std::string localize(std::string s) {
    char sep = decimal_separator();
    for (auto &c : s) {
        if (c == '.') {
            c = sep;
        }
    }
    return s;
}

} // namespace

std::string fmt_bytes(uint64_t bytes) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = (double) bytes;
    int u    = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        u++;
    }
    char buf[32];
    if (u == 0) {
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long) bytes);
    } else {
        snprintf(buf, sizeof(buf), v < 100 ? "%.1f %s" : "%.0f %s", v, units[u]);
    }
    return localize(buf);
}

std::string fmt_speed(double bytesPerSecond) {
    return fmt_bytes(bytesPerSecond < 1 ? 0 : (uint64_t) bytesPerSecond) + "/s";
}

std::string fmt_clock(uint64_t ms) {
    unsigned s = (unsigned) (ms / 1000);
    char buf[32];
    if (s >= 3600) {
        snprintf(buf, sizeof(buf), "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60);
    } else {
        snprintf(buf, sizeof(buf), "%02u:%02u", s / 60, s % 60);
    }
    return buf;
}

std::string fmt_duration(uint64_t ms) {
    char buf[32];
    if (ms < 60000) {
        snprintf(buf, sizeof(buf), "%.1f s", ms / 1000.0);
        return localize(buf);
    }
    return fmt_clock(ms) + " min";
}

std::string fmt_ago(uint64_t ms) {
    unsigned s = (unsigned) (ms / 1000);
    if (s < 60) {
        return trf(S_AGO_S, std::to_string(s));
    }
    if (s < 3600) {
        return trf(S_AGO_MIN, std::to_string(s / 60));
    }
    return trf(S_AGO_H, std::to_string(s / 3600));
}

std::string fmt_date(int64_t t) {
    if (t <= 0) {
        return "–";
    }
    // Civil date from days since 1970 (Howard Hinnant's algorithm).
    int64_t z          = t / 86400 + 719468;
    int64_t secs       = t % 86400;
    const int64_t era  = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned) (z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp  = (5 * doy + 2) / 153;
    unsigned d         = doy - (153 * mp + 2) / 5 + 1;
    unsigned m         = mp < 10 ? mp + 3 : mp - 9;
    int64_t y          = (int64_t) yoe + era * 400 + (m <= 2);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04lld-%02u-%02u %02u:%02u", (long long) y, m, d, (unsigned) (secs / 3600),
             (unsigned) (secs / 60 % 60));
    return buf;
}
