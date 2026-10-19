#pragma once
// Helpers for the server identity shared by the plugin and the app.

#include <cstddef>
#include <cstdint>
#include <string>

namespace util {

std::string to_hex(const uint8_t *p, size_t n);
bool from_hex(const std::string &s, uint8_t *out, size_t n);

// 12 characters from 31 unambiguous symbols (about 59 bits), as xxxx-xxxx-xxxx.
std::string generate_password();

// OpenSSH-style "SHA256:..." fingerprint of an ssh-ed25519 public key.
std::string host_fingerprint(const uint8_t publicKey[32]);

} // namespace util
