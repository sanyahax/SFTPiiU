#include "random.h"
#include "bytes.h"
#include "chacha.h"
#include "sha2.h"

#include <cstring>
#include <mutex>

namespace crypto {

static std::mutex rngMutex;
static uint8_t rngKey[32];
static uint64_t rngCounter;

void rng_add_entropy(const void *data, size_t len) {
    std::lock_guard<std::mutex> lock(rngMutex);
    Sha256 h;
    h.update(rngKey, sizeof(rngKey));
    h.update(data, len);
    h.final(rngKey);
}

void rng_bytes(void *out, size_t len) {
    std::lock_guard<std::mutex> lock(rngMutex);
    ChaCha20 c;
    c.set_key(rngKey);
    uint8_t nonce[8];
    store_be64(nonce, rngCounter++);

    // First block: new key, then the requested output.
    uint8_t next[32] = {0};
    c.crypt(nonce, 0, next, next, sizeof(next));
    memset(out, 0, len);
    c.crypt(nonce, 1, (uint8_t *) out, (uint8_t *) out, len);
    memcpy(rngKey, next, sizeof(rngKey));
    secure_zero(next, sizeof(next));
    secure_zero(&c, sizeof(c));
}

} // namespace crypto
