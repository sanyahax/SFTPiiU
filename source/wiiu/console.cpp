#include "console.h"
#include "../crypto/bytes.h"
#include "../crypto/random.h"
#include "../crypto/sha2.h"
#include "../platform.h"
#include "../util/identity.h"

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <mocha/mocha.h>
#include <mocha/otp.h>
#include <nn/ac.h>

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace {

struct MountSpec {
    const char *name;
    const char *device;
    const char *path;
    bool slc;
    bool disc;
};

// Same set ftpiiu exposes with "Allow access to system files".
const MountSpec kSystemMounts[] = {
        {"storage_mlc", nullptr, "/vol/storage_mlc01", false, false},
        {"storage_usb", nullptr, "/vol/storage_usb01", false, false},
        {"storage_slc", "/dev/slc01", "/vol/storage_slc01", true, false},
        {"slccmpt01", "/dev/slccmpt01", "/vol/storage_slccmpt01", true, false},
        {"storage_odd_tickets", nullptr, "/vol/storage_odd01", false, true},
        {"storage_odd_updates", nullptr, "/vol/storage_odd02", false, true},
        {"storage_odd_content", nullptr, "/vol/storage_odd03", false, true},
        {"storage_odd_content2", nullptr, "/vol/storage_odd04", false, true},
};

std::vector<std::string> gMounted;

bool mount(const MountSpec &m) {
    MochaUtilsStatus res = Mocha_MountFS(m.name, m.device, m.path);
    if (res == MOCHA_RESULT_ALREADY_EXISTS) {
        res = Mocha_MountFS(m.name, nullptr, m.path);
    }
    if (res != MOCHA_RESULT_SUCCESS) {
        LOG("mount %s failed: %s", m.name, Mocha_GetStatusStr(res));
        return false;
    }
    gMounted.emplace_back(m.name);
    return true;
}

} // namespace

std::string wiiu_seed_rng(bool mochaAvailable, const std::string &storedSeedHex) {
    crypto::Sha512 pool;
    auto addValue = [&](uint64_t v) { pool.update(&v, sizeof(v)); };

    pool.update(storedSeedHex.data(), storedSeedHex.size());
    addValue((uint64_t) OSGetTime());
    addValue((uint64_t) OSGetSystemTime());

    if (mochaAvailable) {
        uint64_t t0 = OSGetSystemTick();
        WiiUConsoleOTP otp;
        if (Mocha_ReadOTP(&otp) == MOCHA_RESULT_SUCCESS) {
            pool.update(&otp, sizeof(otp));
        }
        crypto::secure_zero(&otp, sizeof(otp));
        addValue(OSGetSystemTick() - t0);
        uint8_t seeprom[0x200];
        if (Mocha_SEEPROMRead(seeprom, 0, sizeof(seeprom)) == MOCHA_RESULT_SUCCESS) {
            pool.update(seeprom, sizeof(seeprom));
        }
        crypto::secure_zero(seeprom, sizeof(seeprom));
        addValue(OSGetSystemTick() - t0);
    }

    uint32_t jitter[1024];
    for (int i = 0; i < 1024; i++) {
        uint32_t a = OSGetSystemTick();
        if (i % 4 == 0) {
            OSYieldThread();
        }
        jitter[i] = OSGetSystemTick() - a;
    }
    pool.update(jitter, sizeof(jitter));
    addValue((uint64_t) OSGetTime());

    uint8_t digest[64];
    pool.final(digest);
    crypto::rng_add_entropy(digest, sizeof(digest));
    crypto::secure_zero(digest, sizeof(digest));

    uint8_t next[32];
    crypto::rng_bytes(next, sizeof(next));
    std::string hex = util::to_hex(next, sizeof(next));
    crypto::secure_zero(next, sizeof(next));
    return hex;
}

void wiiu_mount_all(Vfs &vfs, bool mochaAvailable, bool systemAccess, bool slcWritable) {
    vfs.add("sd", kSdRoot, false);
    if (systemAccess && mochaAvailable) {
        for (const auto &m : kSystemMounts) {
            if (mount(m)) {
                bool readOnly = m.disc || (m.slc && !slcWritable);
                vfs.add(m.name, std::string(m.name) + ":", readOnly);
            }
        }
    }
    mkdir("fs:/vol/external01/wiiu", 0777);
    mkdir(kConfigDir, 0777);
}

void wiiu_unmount_all() {
    for (const auto &name : gMounted) {
        Mocha_UnmountFS(name.c_str());
    }
    gMounted.clear();
}

std::string wiiu_own_ip() {
    uint32_t ip = 0;
    nn::ac::GetAssignedAddress(&ip);
    if (ip == 0) {
        return {};
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (unsigned) (ip >> 24) & 0xff, (unsigned) (ip >> 16) & 0xff,
             (unsigned) (ip >> 8) & 0xff, (unsigned) ip & 0xff);
    return buf;
}

int64_t wiiu_boot_time() {
    // OSGetSystemTime counts from power-on, OSGetTime from 2000.
    return (int64_t) ((OSGetTime() - OSGetSystemTime()) / (OSTime) OSSecondsToTicks(1));
}
