#pragma once

#include "monitor.h"
#include "platform.h"
#include "vfs.h"

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

struct ServerSettings {
    uint16_t port     = 22;
    bool passwordAuth = true;
    std::string password;
    // Real (device) path of an OpenSSH authorized_keys file, may be empty.
    std::string authorizedKeysPath;
    uint8_t hostPublic[32] = {};
    uint8_t hostSecret[64] = {};
    // Called from session threads; keep them short and thread-safe.
    void (*onLogin)(const char *ip, const char *method)  = nullptr;
    void (*onAuthFailure)(const char *ip, bool lockedOut) = nullptr;
    // Optional live status for a GUI.
    Monitor *monitor = nullptr;
    // The SFTPiiU app asks a running plugin server to step aside by sending
    // kYieldLine instead of an SSH version line. Only honoured if this returns
    // true for the peer (the plugin accepts the console's own addresses).
    bool (*allowYield)(const char *ip) = nullptr;
    void (*onYield)()                  = nullptr;
};

constexpr const char kYieldLine[] = "SFTPIIU-YIELD";

class SshServer {
public:
    static constexpr int kMaxSessions = 4;

    SshServer(const ServerSettings &settings, Vfs vfs);
    ~SshServer();

    bool start();
    void stop();
    // Non-blocking: lets the accept loop wind down and release the port.
    void requestStop() { stop_ = true; }

    const ServerSettings &settings() const { return settings_; }
    const Vfs &vfs() const { return vfs_; }
    const std::atomic<bool> &stopping() const { return stop_; }

    // Global password brute-force guard shared by all sessions.
    bool passwordLoginBlocked();
    // Returns true if this failure triggered a lockout.
    bool notePasswordFailure();
    void notePasswordSuccess();

private:
    struct SessionSlot {
        std::thread thread;
        std::atomic<bool> done{false};
    };

    void acceptLoop();
    void reapSessions(bool all);

    ServerSettings settings_;
    Vfs vfs_;
    std::atomic<bool> stop_{false};
    plat::Socket listener_ = plat::INVALID_SOCKET_;
    std::thread acceptThread_;
    std::list<std::unique_ptr<SessionSlot>> sessions_;

    std::mutex guardMutex_;
    int recentFailures_       = 0;
    uint64_t firstFailureMs_  = 0;
    uint64_t lockedUntilMs_   = 0;
};
