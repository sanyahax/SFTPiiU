#include "monitor.h"
#include "platform.h"

namespace {
constexpr size_t kMaxHistory = 20;
constexpr size_t kMaxEvents  = 50;
} // namespace

Monitor::Session *Monitor::find(uint32_t sid) {
    for (auto &s : state_.sessions) {
        if (s.id == sid) {
            return &s;
        }
    }
    return nullptr;
}

void Monitor::addEvent(EventKind kind, const std::string &a, const std::string &b) {
    Event e;
    e.ms    = plat::now_ms();
    e.kind  = kind;
    e.a     = a;
    e.b     = b;
    e.error = kind == EventKind::WrongPassword || kind == EventKind::LockedOut ||
              kind == EventKind::TooManyConnections;
    state_.events.push_front(e);
    if (state_.events.size() > kMaxEvents) {
        state_.events.pop_back();
    }
}

uint32_t Monitor::sessionOpened(const std::string &peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    Session s;
    s.id          = nextId_++;
    s.peer        = peer;
    s.connectedMs = plat::now_ms();
    state_.sessions.push_back(s);
    return s.id;
}

void Monitor::sessionClient(uint32_t sid, const std::string &client) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Session *s = find(sid)) {
        s->client = client;
    }
}

void Monitor::sessionCipher(uint32_t sid, const std::string &cipher) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Session *s = find(sid)) {
        s->cipher = cipher;
    }
}

void Monitor::sessionAuthed(uint32_t sid, const std::string &method) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Session *s = find(sid)) {
        s->method = method;
        addEvent(EventKind::Login, s->peer, method);
    }
}

void Monitor::sessionClosed(uint32_t sid) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = state_.sessions.begin(); it != state_.sessions.end(); ++it) {
        if (it->id == sid) {
            if (!it->method.empty()) {
                addEvent(EventKind::Disconnected, it->peer, {});
            }
            state_.sessions.erase(it);
            return;
        }
    }
}

uint32_t Monitor::transferStart(uint32_t sid, const std::string &path, bool upload, uint64_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    Session *s = find(sid);
    if (!s) {
        return 0;
    }
    Transfer t;
    t.id      = nextId_++;
    t.path    = path;
    t.upload  = upload;
    t.size    = size;
    t.startMs = t.lastMs = plat::now_ms();
    s->transfers.push_back(t);
    return t.id;
}

void Monitor::transferProgress(uint32_t sid, uint32_t tid, uint64_t bytes, uint64_t endOffset) {
    std::lock_guard<std::mutex> lock(mutex_);
    Session *s = find(sid);
    if (!s) {
        return;
    }
    for (auto &t : s->transfers) {
        if (t.id == tid) {
            t.bytes += bytes;
            t.lastMs = plat::now_ms();
            if (t.upload) {
                if (endOffset > t.size) {
                    t.size = endOffset;
                }
                s->bytesUp += bytes;
                state_.totalUp += bytes;
            } else {
                s->bytesDown += bytes;
                state_.totalDown += bytes;
            }
            return;
        }
    }
}

void Monitor::transferEnd(uint32_t sid, uint32_t tid) {
    std::lock_guard<std::mutex> lock(mutex_);
    Session *s = find(sid);
    if (!s) {
        return;
    }
    for (auto it = s->transfers.begin(); it != s->transfers.end(); ++it) {
        if (it->id == tid) {
            if (it->bytes > 0) {
                Finished f;
                f.path       = it->path;
                f.upload     = it->upload;
                f.bytes      = it->bytes;
                f.endMs      = plat::now_ms();
                f.durationMs = f.endMs - it->startMs;
                state_.history.push_front(f);
                if (state_.history.size() > kMaxHistory) {
                    state_.history.pop_back();
                }
            }
            s->transfers.erase(it);
            return;
        }
    }
}

void Monitor::event(EventKind kind, const std::string &a, const std::string &b) {
    std::lock_guard<std::mutex> lock(mutex_);
    addEvent(kind, a, b);
}

Monitor::Snapshot Monitor::snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}
