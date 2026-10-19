// PC test harness: runs the same SSH/SFTP core as the plugin, serving local
// folders that stand in for the Wii U devices. Builds natively (Windows) or as
// wasm32-wasi (see wasm/run_wasm.py).
//
//   --selftest
//   --port 2222 --root testroot --password secret [--keys authorized_keys]

#include "../source/crypto/curve25519.h"
#include "../source/crypto/random.h"
#include "../source/crypto/sha2.h"
#include "../source/selftest.h"
#include "../source/server.h"
#include "../source/util/base64.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void os_random(uint8_t *out, size_t len) {
#ifdef _WIN32
    BCryptGenRandom(nullptr, out, (ULONG) len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    while (len) {
        size_t n = len > 256 ? 256 : len;
        getentropy(out, n);
        out += n;
        len -= n;
    }
#endif
}

int main(int argc, char **argv) {
    uint16_t port    = 2222;
    std::string root = "testroot";
    std::string password;
    std::string keys;
    std::string hostKeyFile = "host_ed25519.seed";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next     = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--selftest") {
            const char *fail = crypto_selftest();
            printf("selftest: %s\n", fail ? fail : "OK");
            return fail ? 1 : 0;
        } else if (a == "--port") {
            port = (uint16_t) atoi(next().c_str());
        } else if (a == "--root") {
            root = next();
        } else if (a == "--password") {
            password = next();
        } else if (a == "--keys") {
            keys = next();
        } else if (a == "--hostkey") {
            hostKeyFile = next();
        }
    }

    if (const char *fail = crypto_selftest()) {
        fprintf(stderr, "crypto selftest failed: %s\n", fail);
        return 1;
    }

    uint8_t seedEntropy[64];
    os_random(seedEntropy, sizeof(seedEntropy));
    crypto::rng_add_entropy(seedEntropy, sizeof(seedEntropy));

    ServerSettings s;
    s.port               = port;
    s.password           = password;
    s.passwordAuth       = !password.empty();
    s.authorizedKeysPath = keys;

    uint8_t seed[32];
    FILE *f = fopen(hostKeyFile.c_str(), "rb");
    if (!f || fread(seed, 1, 32, f) != 32) {
        crypto::rng_bytes(seed, 32);
        if (f) fclose(f);
        f = fopen(hostKeyFile.c_str(), "wb");
        if (f) fwrite(seed, 1, 32, f);
    }
    if (f) fclose(f);
    crypto::ed25519_keypair_from_seed(seed, s.hostPublic, s.hostSecret);

    uint8_t blob[51] = {0, 0, 0, 11};
    memcpy(blob + 4, "ssh-ed25519", 11);
    blob[18] = 32;
    memcpy(blob + 19, s.hostPublic, 32);
    uint8_t fp[32];
    crypto::sha256(blob, sizeof(blob), fp);
    printf("host key: ssh-ed25519 %s\n", util::base64_encode(blob, sizeof(blob)).c_str());
    printf("fingerprint: SHA256:%s\n", util::base64_encode(fp, 32, false).c_str());

    s.onLogin = [](const char *ip, const char *how) {
        printf("LOGIN %s via %s\n", ip, how);
        fflush(stdout);
    };
    s.onAuthFailure = [](const char *ip, bool locked) {
        printf("AUTHFAIL %s%s\n", ip, locked ? " (locked)" : "");
        fflush(stdout);
    };

    Monitor monitor;
    s.monitor    = &monitor;
    s.allowYield = [](const char *ip) { return strcmp(ip, "127.0.0.1") == 0; };
    s.onYield    = []() {
        printf("YIELD requested\n");
        fflush(stdout);
    };

    Vfs vfs;
    vfs.add("sd", root + "/sd", false);
    vfs.add("storage_mlc", root + "/mlc", false);
    vfs.add("storage_slc", root + "/slc", true);

    plat::net_init();
    SshServer server(s, vfs);
    printf("starting on port %u\n", (unsigned) port);
    fflush(stdout);
    // In the single-threaded wasm build start() serves clients and only
    // returns once the server was asked to stop (yield).
    if (!server.start()) {
        return 1;
    }
    while (!server.stopping()) {
        plat::sleep_ms(500);
        if (FILE *stop = fopen("STOP", "rb")) {
            fclose(stop);
            remove("STOP");
            break;
        }
    }
    server.stop();

    Monitor::Snapshot snap = monitor.snapshot();
    printf("monitor: %zu sessions, up %llu, down %llu\n", snap.sessions.size(), (unsigned long long) snap.totalUp,
           (unsigned long long) snap.totalDown);
    for (const auto &f : snap.history) {
        printf("  history: %s %s %llu bytes\n", f.upload ? "UP  " : "DOWN", f.path.c_str(), (unsigned long long) f.bytes);
    }
    for (const auto &e : snap.events) {
        printf("  event%s: kind %d %s %s\n", e.error ? " (error)" : "", (int) e.kind, e.a.c_str(), e.b.c_str());
    }
    printf("stopped\n");
    return 0;
}
