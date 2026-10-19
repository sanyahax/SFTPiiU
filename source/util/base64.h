#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace util {

std::string base64_encode(const uint8_t *data, size_t len, bool pad = true);
bool base64_decode(std::string_view in, std::vector<uint8_t> &out);

} // namespace util
