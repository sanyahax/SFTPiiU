#pragma once
// Live view of the server for the GUI app: connected clients, running
// transfers, recently finished transfers and an event log. Sessions report
// into it from their threads; the GUI copies a snapshot a few times per second.

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

// Events are stored as kind + arguments so the GUI can show them in the
// console's language.
enum class EventKind {
    ServerStarted,      // a = port
    Login,              // a = peer, b = method ("password" / "publickey")
    Disconnected,       // a = peer
    WrongPassword,      // a = peer
    LockedOut,          // a = peer
    TooManyConnections, // a = peer
    NewPassword,
};

class Monitor {
public:
    struct Transfer {
        uint32_t id     = 0;
        std::string path;
        bool upload     = false; // client -> console
        uint64_t bytes  = 0;     // moved through this handle so far
        uint64_t size   = 0;     // file size for downloads, highest written offset for uploads
        uint64_t startMs = 0;
        uint64_t lastMs  = 0;
    };

    struct Session {
        uint32_t id = 0;
        std::string peer;
        std::string client;
        std::string method; // "password" / "publickey", empty until logged in
        std::string cipher;
        uint64_t connectedMs = 0;
        uint64_t bytesUp     = 0;
        uint64_t bytesDown   = 0;
        std::vector<Transfer> transfers;
    };

    struct Finished {
        std::string path;
        bool upload         = false;
        uint64_t bytes      = 0;
        uint64_t durationMs = 0;
        uint64_t endMs      = 0;
    };

    struct Event {
        uint64_t ms    = 0;
        EventKind kind = EventKind::ServerStarted;
        std::string a, b;
        bool error = false;
    };

    struct Snapshot {
        std::vector<Session> sessions;
        std::deque<Finished> history; // newest first
        std::deque<Event> events;     // newest first
        uint64_t totalUp   = 0;
        uint64_t totalDown = 0;
    };

    uint32_t sessionOpened(const std::string &peer);
    void sessionClient(uint32_t sid, const std::string &client);
    void sessionCipher(uint32_t sid, const std::string &cipher);
    void sessionAuthed(uint32_t sid, const std::string &method);
    void sessionClosed(uint32_t sid);

    uint32_t transferStart(uint32_t sid, const std::string &path, bool upload, uint64_t size);
    void transferProgress(uint32_t sid, uint32_t tid, uint64_t bytes, uint64_t endOffset);
    void transferEnd(uint32_t sid, uint32_t tid);

    void event(EventKind kind, const std::string &a = {}, const std::string &b = {});

    Snapshot snapshot();

private:
    Session *find(uint32_t sid);
    void addEvent(EventKind kind, const std::string &a, const std::string &b);

    std::mutex mutex_;
    Snapshot state_;
    uint32_t nextId_ = 1;
};
