#pragma once
// SFTP protocol version 3 (draft-ietf-secsh-filexfer-02), the version every
// common client speaks, plus a few OpenSSH extensions.

#include "monitor.h"
#include "ssh/buffer.h"
#include "vfs.h"

#include <cstdint>
#include <dirent.h>
#include <map>
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

class SftpSession {
public:
    static constexpr size_t kMaxIo      = 64 * 1024;
    static constexpr size_t kMaxRequest = kMaxIo + 4096;

    // monitor may be null; sessionId identifies this connection in it.
    SftpSession(const Vfs &vfs, Monitor *monitor = nullptr, uint32_t sessionId = 0);
    ~SftpSession();

    // Handles one complete SFTP packet (without the 4-byte length) and appends
    // the framed response to out. Returns false on a protocol violation.
    bool handle(const uint8_t *pkt, size_t len, ssh::Writer &out);

private:
    struct Handle {
        bool isDir         = false;
        int fd             = -1;
        DIR *dir           = nullptr;
        bool rootListing   = false;
        size_t rootIndex   = 0;
        const Mount *mount = nullptr;
        std::string real; // for directories: path to prefix entry names with
        std::string path; // virtual path, for the monitor
        uint64_t pos      = 0;
        bool posKnown     = false;
        uint32_t transfer = 0; // monitor id, assigned on first read/write
    };

    void closeAll();
    Handle *lookup(std::string_view handle);
    void track(Handle &h, bool upload, uint64_t bytes, uint64_t endOffset);
    void untrack(Handle &h);

    void opOpen(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opClose(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opRead(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opWrite(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opStat(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opFstat(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opSetstat(uint32_t id, ssh::Reader &r, ssh::Writer &out, bool byHandle);
    void opOpendir(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opReaddir(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opRemove(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opMkdir(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opRmdir(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opRealpath(uint32_t id, ssh::Reader &r, ssh::Writer &out);
    void opRename(uint32_t id, std::string_view from, std::string_view to, bool overwrite, ssh::Writer &out);
    void opExtended(uint32_t id, ssh::Reader &r, ssh::Writer &out);

    const Vfs &vfs_;
    Monitor *monitor_;
    uint32_t sessionId_;
    std::map<uint32_t, Handle> handles_;
    uint32_t nextHandle_ = 1;
    std::unique_ptr<uint8_t[]> ioRaw_;
    uint8_t *io_; // 64-byte aligned, the Wii U FS layer prefers that
};
