#pragma once
// Wii U pieces shared by the plugin and the app: RNG seeding, device mounts,
// the console's own address.

#include "../vfs.h"

#include <cstdint>
#include <string>

// No OS random source is available, so the generator is seeded from several
// independent inputs hashed together:
//  - a seed persisted from the previous run (storedSeedHex, may be empty),
//  - the console's OTP and SEEPROM (per-console secrets, via MochaPayload),
//  - clock values and scheduler/IPC timing jitter.
// Each connection additionally mixes in its own timing. Returns a fresh seed
// (hex) that the caller should persist for the next run.
std::string wiiu_seed_rng(bool mochaAvailable, const std::string &storedSeedHex);

// Adds /sd and, with Mocha and systemAccess, MLC/USB/SLC/disc mounts.
// SLC mounts are read-only unless slcWritable, disc mounts always are.
void wiiu_mount_all(Vfs &vfs, bool mochaAvailable, bool systemAccess, bool slcWritable);
void wiiu_unmount_all();

// "192.168.0.23", or empty if the console has no address yet.
std::string wiiu_own_ip();

// When the console started, in seconds since 2000. The same value for the
// whole session, so the plugin can leave it behind as proof that it was
// loaded and the app can compare it with its own.
int64_t wiiu_boot_time();

constexpr const char kSdRoot[]         = "fs:/vol/external01";
constexpr const char kConfigDir[]      = "fs:/vol/external01/wiiu/sftpiiu";
constexpr const char kAuthorizedKeys[] = "fs:/vol/external01/wiiu/sftpiiu/authorized_keys";
