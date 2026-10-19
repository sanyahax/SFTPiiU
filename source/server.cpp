#include "server.h"
#include "ssh/session.h"

namespace {
constexpr int kLockoutFailures     = 10;
constexpr uint64_t kFailureWindow  = 10 * 60 * 1000;
constexpr uint64_t kLockoutMs      = 5 * 60 * 1000;
} // namespace

SshServer::SshServer(const ServerSettings &settings, Vfs vfs) : settings_(settings), vfs_(std::move(vfs)) {
}

SshServer::~SshServer() {
    stop();
}

bool SshServer::start() {
    if (acceptThread_.joinable()) {
        return true;
    }
    listener_ = plat::tcp_listen(settings_.port);
    if (listener_ == plat::INVALID_SOCKET_) {
        LOG("cannot listen on port %u", (unsigned) settings_.port);
        return false;
    }
    stop_         = false;
    acceptThread_ = std::thread(&SshServer::acceptLoop, this);
    plat::tune_thread(acceptThread_, "SFTPiiU listener");
    LOG("listening on port %u", (unsigned) settings_.port);
    return true;
}

void SshServer::stop() {
    stop_ = true;
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    if (listener_ != plat::INVALID_SOCKET_) {
        plat::tcp_close(listener_);
        listener_ = plat::INVALID_SOCKET_;
    }
}

void SshServer::reapSessions(bool all) {
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (all || (*it)->done) {
            if ((*it)->thread.joinable()) {
                (*it)->thread.join();
            }
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void SshServer::acceptLoop() {
    while (!stop_) {
        reapSessions(false);
        std::string peer;
        plat::Socket s = plat::tcp_accept(listener_, peer, 250);
        if (s == plat::INVALID_SOCKET_) {
            continue;
        }
        if (sessions_.size() >= (size_t) kMaxSessions) {
            LOG("[%s] rejected: too many sessions", peer.c_str());
            if (settings_.monitor) {
                settings_.monitor->event(EventKind::TooManyConnections, peer);
            }
            plat::tcp_close(s);
            continue;
        }
        auto slot          = std::make_unique<SessionSlot>();
        SessionSlot *raw   = slot.get();
        raw->thread        = std::thread([this, s, peer, raw]() {
            {
                SshSession session(*this, s, peer);
                session.run();
            }
            raw->done = true;
        });
        plat::tune_thread(raw->thread, "SFTPiiU session");
        sessions_.push_back(std::move(slot));
    }
    reapSessions(true);
    // Free the port right away, so another server (the app) can bind it.
    plat::tcp_close(listener_);
    listener_ = plat::INVALID_SOCKET_;
}

bool SshServer::passwordLoginBlocked() {
    std::lock_guard<std::mutex> lock(guardMutex_);
    return plat::now_ms() < lockedUntilMs_;
}

bool SshServer::notePasswordFailure() {
    std::lock_guard<std::mutex> lock(guardMutex_);
    uint64_t now = plat::now_ms();
    if (recentFailures_ == 0 || now - firstFailureMs_ > kFailureWindow) {
        recentFailures_ = 0;
        firstFailureMs_ = now;
    }
    if (++recentFailures_ >= kLockoutFailures) {
        recentFailures_ = 0;
        lockedUntilMs_  = now + kLockoutMs;
        LOG("password login locked for %u minutes", (unsigned) (kLockoutMs / 60000));
        return true;
    }
    return false;
}

void SshServer::notePasswordSuccess() {
    std::lock_guard<std::mutex> lock(guardMutex_);
    recentFailures_ = 0;
}
