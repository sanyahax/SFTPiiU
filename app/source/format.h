#pragma once
// Number, size and time formatting that follows the UI language.

#include <cstdint>
#include <string>

std::string fmt_bytes(uint64_t bytes);
std::string fmt_speed(double bytesPerSecond);
std::string fmt_clock(uint64_t ms);    // 12:34 or 1:02:03
std::string fmt_duration(uint64_t ms); // 4.2 s or 1:23 min
std::string fmt_ago(uint64_t ms);      // "5 s ago", localized
std::string fmt_date(int64_t unixTime); // 2026-10-03 09:20
