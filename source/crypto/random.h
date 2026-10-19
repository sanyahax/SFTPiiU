#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto {

// ChaCha20-based generator with fast key erasure. Every caller of bytes() gets
// output derived from a key that is replaced immediately afterwards, so a
// later memory disclosure does not reveal earlier outputs. Entropy from the
// platform layer is hashed into the key with add_entropy().
void rng_add_entropy(const void *data, size_t len);
void rng_bytes(void *out, size_t len);

} // namespace crypto
