#include "sftp.h"
#include "platform.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/statvfs.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

namespace {

enum : uint8_t {
    FXP_INIT           = 1,
    FXP_VERSION        = 2,
    FXP_OPEN           = 3,
    FXP_CLOSE          = 4,
    FXP_READ           = 5,
    FXP_WRITE          = 6,
    FXP_LSTAT          = 7,
    FXP_FSTAT          = 8,
    FXP_SETSTAT        = 9,
    FXP_FSETSTAT       = 10,
    FXP_OPENDIR        = 11,
    FXP_READDIR        = 12,
    FXP_REMOVE         = 13,
    FXP_MKDIR          = 14,
    FXP_RMDIR          = 15,
    FXP_REALPATH       = 16,
    FXP_STAT           = 17,
    FXP_RENAME         = 18,
    FXP_READLINK       = 19,
    FXP_SYMLINK        = 20,
    FXP_STATUS         = 101,
    FXP_HANDLE         = 102,
    FXP_DATA           = 103,
    FXP_NAME           = 104,
    FXP_ATTRS          = 105,
    FXP_EXTENDED       = 200,
    FXP_EXTENDED_REPLY = 201,
};

enum : uint32_t {
    FX_OK                = 0,
    FX_EOF               = 1,
    FX_NO_SUCH_FILE      = 2,
    FX_PERMISSION_DENIED = 3,
    FX_FAILURE           = 4,
    FX_BAD_MESSAGE       = 5,
    FX_OP_UNSUPPORTED    = 8,
};

enum : uint32_t {
    ATTR_SIZE        = 0x1,
    ATTR_UIDGID      = 0x2,
    ATTR_PERMISSIONS = 0x4,
    ATTR_ACMODTIME   = 0x8,
    ATTR_EXTENDED    = 0x80000000u,
};

enum : uint32_t {
    PF_READ   = 0x1,
    PF_WRITE  = 0x2,
    PF_APPEND = 0x4,
    PF_CREAT  = 0x8,
    PF_TRUNC  = 0x10,
    PF_EXCL   = 0x20,
};

constexpr uint32_t kModeDir  = 0040000;
constexpr uint32_t kModeFile = 0100000;
constexpr size_t kMaxHandles = 64;

size_t begin(ssh::Writer &out, uint8_t type, uint32_t id) {
    size_t start = out.size();
    out.u32(0);
    out.u8(type);
    out.u32(id);
    return start;
}

void finish(ssh::Writer &out, size_t start) {
    out.patch_u32(start, (uint32_t) (out.size() - start - 4));
}

void status(ssh::Writer &out, uint32_t id, uint32_t code, const char *msg) {
    size_t s = begin(out, FXP_STATUS, id);
    out.u32(code);
    out.str(msg);
    out.str("en");
    finish(out, s);
}

void status_errno(ssh::Writer &out, uint32_t id, int err) {
    uint32_t code;
    switch (err) {
        case ENOENT:
        case ENOTDIR:
            code = FX_NO_SUCH_FILE;
            break;
        case EACCES:
        case EPERM:
        case EROFS:
            code = FX_PERMISSION_DENIED;
            break;
        default:
            code = FX_FAILURE;
            break;
    }
    status(out, id, code, strerror(err));
}

void denied(ssh::Writer &out, uint32_t id) {
    status(out, id, FX_PERMISSION_DENIED, "read-only location");
}

struct Attrs {
    uint32_t flags = 0;
    uint64_t size  = 0;
};

Attrs parse_attrs(ssh::Reader &r) {
    Attrs a;
    a.flags = r.u32();
    if (a.flags & ATTR_SIZE) {
        a.size = r.u64();
    }
    if (a.flags & ATTR_UIDGID) {
        r.u32();
        r.u32();
    }
    if (a.flags & ATTR_PERMISSIONS) {
        r.u32();
    }
    if (a.flags & ATTR_ACMODTIME) {
        r.u32();
        r.u32();
    }
    if (a.flags & ATTR_EXTENDED) {
        uint32_t n = r.u32();
        for (uint32_t i = 0; i < n && r.ok; i++) {
            r.str();
            r.str();
        }
    }
    return a;
}

uint32_t mode_of(bool dir, bool readOnly) {
    uint32_t perms = dir ? 0777 : 0666;
    if (readOnly) {
        perms &= ~0222u;
    }
    return (dir ? kModeDir : kModeFile) | perms;
}

void put_attrs(ssh::Writer &out, const struct stat &st, bool readOnly) {
    bool dir = S_ISDIR(st.st_mode);
    out.u32(ATTR_SIZE | ATTR_PERMISSIONS | ATTR_ACMODTIME);
    out.u64(dir ? 0 : (uint64_t) st.st_size);
    out.u32(mode_of(dir, readOnly));
    uint32_t t = st.st_mtime > 0 ? (uint32_t) st.st_mtime : 0;
    out.u32(t);
    out.u32(t);
}

void put_dir_attrs(ssh::Writer &out, bool readOnly) {
    out.u32(ATTR_SIZE | ATTR_PERMISSIONS | ATTR_ACMODTIME);
    out.u64(0);
    out.u32(mode_of(true, readOnly));
    out.u32(0);
    out.u32(0);
}

void civil_from_days(int64_t z, int64_t &y, unsigned &m, unsigned &d) {
    z += 719468;
    const int64_t era  = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned) (z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp  = (5 * doy + 2) / 153;
    d                  = doy - (153 * mp + 2) / 5 + 1;
    m                  = mp < 10 ? mp + 3 : mp - 9;
    y                  = (int64_t) yoe + era * 400 + (m <= 2);
}

// "ls -l" style line; only OpenSSH's sftp shows it, other clients use attrs.
std::string long_name(const std::string &name, bool dir, bool readOnly, uint64_t size, int64_t mtime) {
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char modeStr[11];
    uint32_t mode = mode_of(dir, readOnly);
    modeStr[0]    = dir ? 'd' : '-';
    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) {
        modeStr[1 + i] = (mode & (0400u >> i)) ? rwx[i] : '-';
    }
    modeStr[10] = 0;

    int64_t y;
    unsigned m, d;
    int64_t days = mtime >= 0 ? mtime / 86400 : 0;
    civil_from_days(days, y, m, d);
    int64_t secs = mtime >= 0 ? mtime % 86400 : 0;
    char when[32];
    int64_t now = (int64_t) time(nullptr);
    if (mtime > now - 180LL * 86400 && mtime <= now + 86400) {
        snprintf(when, sizeof(when), "%s %2u %02u:%02u", months[m - 1], d, (unsigned) (secs / 3600), (unsigned) (secs / 60 % 60));
    } else {
        snprintf(when, sizeof(when), "%s %2u  %4lld", months[m - 1], d, (long long) y);
    }
    char line[96];
    snprintf(line, sizeof(line), "%s    1 wiiu     wiiu     %12llu %s ", modeStr, (unsigned long long) size, when);
    return std::string(line) + name;
}

int make_dir(const char *path) {
#ifdef _WIN32
    return ::mkdir(path);
#else
    return ::mkdir(path, 0777);
#endif
}

int sync_fd(int fd) {
#ifdef _WIN32
    return _commit(fd);
#else
    return ::fsync(fd);
#endif
}

std::string join(const std::string &dir, const char *name) {
    if (!dir.empty() && dir.back() == '/') {
        return dir + name;
    }
    return dir + "/" + name;
}

} // namespace

SftpSession::SftpSession(const Vfs &vfs, Monitor *monitor, uint32_t sessionId)
    : vfs_(vfs), monitor_(monitor), sessionId_(sessionId) {
    ioRaw_.reset(new uint8_t[kMaxIo + 64]);
    uintptr_t p = (uintptr_t) ioRaw_.get();
    io_         = (uint8_t *) ((p + 63) & ~(uintptr_t) 63);
}

SftpSession::~SftpSession() {
    closeAll();
}

void SftpSession::closeAll() {
    for (auto &kv : handles_) {
        Handle &h = kv.second;
        untrack(h);
        if (h.fd >= 0) {
            ::close(h.fd);
        }
        if (h.dir) {
            ::closedir(h.dir);
        }
    }
    handles_.clear();
}

void SftpSession::track(Handle &h, bool upload, uint64_t bytes, uint64_t endOffset) {
    if (!monitor_) {
        return;
    }
    if (!h.transfer) {
        uint64_t size = 0;
        struct stat st;
        if (!upload && ::fstat(h.fd, &st) == 0) {
            size = (uint64_t) st.st_size;
        }
        h.transfer = monitor_->transferStart(sessionId_, h.path, upload, size);
    }
    monitor_->transferProgress(sessionId_, h.transfer, bytes, endOffset);
}

void SftpSession::untrack(Handle &h) {
    if (monitor_ && h.transfer) {
        monitor_->transferEnd(sessionId_, h.transfer);
        h.transfer = 0;
    }
}

SftpSession::Handle *SftpSession::lookup(std::string_view handle) {
    if (handle.size() != 4) {
        return nullptr;
    }
    const uint8_t *b = ssh::u8p(handle);
    uint32_t key     = ((uint32_t) b[0] << 24) | ((uint32_t) b[1] << 16) | ((uint32_t) b[2] << 8) | b[3];
    auto it          = handles_.find(key);
    return it == handles_.end() ? nullptr : &it->second;
}

static void send_handle(ssh::Writer &out, uint32_t id, uint32_t key) {
    size_t s = begin(out, FXP_HANDLE, id);
    uint8_t b[4] = {(uint8_t) (key >> 24), (uint8_t) (key >> 16), (uint8_t) (key >> 8), (uint8_t) key};
    out.str(b, 4);
    finish(out, s);
}

bool SftpSession::handle(const uint8_t *pkt, size_t len, ssh::Writer &out) {
    ssh::Reader r(pkt, len);
    uint8_t type = r.u8();
    if (!r.ok) {
        return false;
    }

    if (type == FXP_INIT) {
        r.u32(); // client version, we always answer with 3
        size_t start = out.size();
        out.u32(0);
        out.u8(FXP_VERSION);
        out.u32(3);
        out.str("posix-rename@openssh.com");
        out.str("1");
        out.str("fsync@openssh.com");
        out.str("1");
        out.str("statvfs@openssh.com");
        out.str("2");
        out.str("limits@openssh.com");
        out.str("1");
        finish(out, start);
        return true;
    }

    uint32_t id = r.u32();
    if (!r.ok) {
        return false;
    }

    switch (type) {
        case FXP_OPEN:
            opOpen(id, r, out);
            break;
        case FXP_CLOSE:
            opClose(id, r, out);
            break;
        case FXP_READ:
            opRead(id, r, out);
            break;
        case FXP_WRITE:
            opWrite(id, r, out);
            break;
        case FXP_LSTAT:
        case FXP_STAT:
            opStat(id, r, out);
            break;
        case FXP_FSTAT:
            opFstat(id, r, out);
            break;
        case FXP_SETSTAT:
            opSetstat(id, r, out, false);
            break;
        case FXP_FSETSTAT:
            opSetstat(id, r, out, true);
            break;
        case FXP_OPENDIR:
            opOpendir(id, r, out);
            break;
        case FXP_READDIR:
            opReaddir(id, r, out);
            break;
        case FXP_REMOVE:
            opRemove(id, r, out);
            break;
        case FXP_MKDIR:
            opMkdir(id, r, out);
            break;
        case FXP_RMDIR:
            opRmdir(id, r, out);
            break;
        case FXP_REALPATH:
            opRealpath(id, r, out);
            break;
        case FXP_RENAME: {
            std::string_view from = r.str();
            std::string_view to   = r.str();
            if (!r.ok) {
                status(out, id, FX_BAD_MESSAGE, "malformed request");
                break;
            }
            opRename(id, from, to, false, out);
            break;
        }
        case FXP_EXTENDED:
            opExtended(id, r, out);
            break;
        case FXP_READLINK:
        case FXP_SYMLINK:
        default:
            status(out, id, FX_OP_UNSUPPORTED, "operation not supported");
            break;
    }
    return true;
}

void SftpSession::opOpen(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    std::string path = Vfs::normalize(r.str());
    uint32_t pflags  = r.u32();
    parse_attrs(r);
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    Vfs::Resolved res = vfs_.resolve(path);
    if (res.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_NO_SUCH_FILE, "no such file");
        return;
    }
    if (res.kind != Vfs::Kind::Inside) {
        status(out, id, FX_FAILURE, "is a directory");
        return;
    }
    bool wantsWrite = pflags & (PF_WRITE | PF_APPEND | PF_CREAT | PF_TRUNC);
    if (wantsWrite && res.mount->readOnly) {
        denied(out, id);
        return;
    }
    if (handles_.size() >= kMaxHandles) {
        status(out, id, FX_FAILURE, "too many open handles");
        return;
    }

    // The Wii U file systems only offer fopen-style modes ("r", "r+", "w",
    // "w+", "a", "a+"), and their devoptabs reject other flag combinations
    // (e.g. O_WRONLY alone, which FileZilla uses to resume). Pick an
    // equivalent combination that maps onto one of those modes.
    bool rd     = pflags & PF_READ;
    bool creat  = pflags & PF_CREAT;
    bool trunc  = pflags & PF_TRUNC;
    bool append = (pflags & PF_APPEND) && !trunc; // writes carry offsets anyway
    int flags;
    if (!wantsWrite) {
        flags = O_RDONLY;
    } else if (append || trunc) {
        flags = (creat && rd ? O_RDWR : O_WRONLY) | (append ? O_APPEND : O_TRUNC) | (creat ? O_CREAT : 0);
    } else if (creat) {
        flags = (rd ? O_RDWR : O_WRONLY) | O_CREAT;
    } else {
        flags = O_RDWR;
    }
    if (creat && (pflags & PF_EXCL)) {
        flags |= O_EXCL;
    }

    int fd = ::open(res.real.c_str(), flags | O_BINARY, 0666);
    if (fd < 0) {
        status_errno(out, id, errno);
        return;
    }
    uint32_t key = nextHandle_++;
    Handle &h    = handles_[key];
    h.fd         = fd;
    h.mount      = res.mount;
    h.real       = res.real;
    h.path       = path;
    h.pos        = 0;
    h.posKnown   = !append;
    send_handle(out, id, key);
}

void SftpSession::opClose(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    std::string_view hs = r.str();
    Handle *h           = lookup(hs);
    if (!h) {
        status(out, id, FX_FAILURE, "invalid handle");
        return;
    }
    untrack(*h);
    int rc = 0;
    int err = 0;
    if (h->fd >= 0) {
        rc  = ::close(h->fd);
        err = errno;
    }
    if (h->dir) {
        ::closedir(h->dir);
    }
    const uint8_t *b = ssh::u8p(hs);
    handles_.erase(((uint32_t) b[0] << 24) | ((uint32_t) b[1] << 16) | ((uint32_t) b[2] << 8) | b[3]);
    if (rc != 0) {
        status_errno(out, id, err);
    } else {
        status(out, id, FX_OK, "");
    }
}

void SftpSession::opRead(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    Handle *h       = lookup(r.str());
    uint64_t offset = r.u64();
    uint32_t want   = r.u32();
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    if (!h || h->fd < 0) {
        status(out, id, FX_FAILURE, "invalid handle");
        return;
    }
    if (want > kMaxIo) {
        want = kMaxIo;
    }
    if (!h->posKnown || h->pos != offset) {
        if (::lseek(h->fd, (off_t) offset, SEEK_SET) < 0) {
            h->posKnown = false;
            status_errno(out, id, errno);
            return;
        }
        h->pos      = offset;
        h->posKnown = true;
    }
    size_t got = 0;
    while (got < want) {
        ssize_t n = ::read(h->fd, io_ + got, want - got);
        if (n < 0) {
            int err     = errno;
            h->posKnown = false;
            if (got == 0) {
                status_errno(out, id, err);
                return;
            }
            break;
        }
        if (n == 0) {
            break;
        }
        got += (size_t) n;
    }
    h->pos += got;
    if (got == 0) {
        status(out, id, FX_EOF, "end of file");
        return;
    }
    track(*h, false, got, offset + got);
    size_t s = begin(out, FXP_DATA, id);
    out.str(io_, got);
    finish(out, s);
}

void SftpSession::opWrite(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    Handle *h             = lookup(r.str());
    uint64_t offset       = r.u64();
    std::string_view data = r.str();
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    if (!h || h->fd < 0) {
        status(out, id, FX_FAILURE, "invalid handle");
        return;
    }
    if (h->mount->readOnly) {
        denied(out, id);
        return;
    }
    if (!h->posKnown || h->pos != offset) {
        if (::lseek(h->fd, (off_t) offset, SEEK_SET) < 0) {
            h->posKnown = false;
            status_errno(out, id, errno);
            return;
        }
        h->pos      = offset;
        h->posKnown = true;
    }
    const uint8_t *src = ssh::u8p(data);
    size_t left        = data.size();
    while (left) {
        size_t chunk = left < kMaxIo ? left : kMaxIo;
        memcpy(io_, src, chunk);
        size_t done = 0;
        while (done < chunk) {
            ssize_t n = ::write(h->fd, io_ + done, chunk - done);
            if (n <= 0) {
                int err     = n < 0 ? errno : ENOSPC;
                h->posKnown = false;
                status_errno(out, id, err);
                return;
            }
            done += (size_t) n;
        }
        h->pos += chunk;
        src += chunk;
        left -= chunk;
    }
    track(*h, true, data.size(), offset + data.size());
    status(out, id, FX_OK, "");
}

void SftpSession::opStat(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    std::string path = Vfs::normalize(r.str());
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    Vfs::Resolved res = vfs_.resolve(path);
    if (res.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_NO_SUCH_FILE, "no such file");
        return;
    }
    if (res.kind == Vfs::Kind::Root || res.kind == Vfs::Kind::MountRoot) {
        size_t s = begin(out, FXP_ATTRS, id);
        put_dir_attrs(out, res.kind == Vfs::Kind::Root || res.mount->readOnly);
        finish(out, s);
        return;
    }
    struct stat st;
    if (::stat(res.real.c_str(), &st) != 0) {
        status_errno(out, id, errno);
        return;
    }
    size_t s = begin(out, FXP_ATTRS, id);
    put_attrs(out, st, res.mount->readOnly);
    finish(out, s);
}

void SftpSession::opFstat(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    Handle *h = lookup(r.str());
    if (!h) {
        status(out, id, FX_FAILURE, "invalid handle");
        return;
    }
    if (h->rootListing) {
        size_t s = begin(out, FXP_ATTRS, id);
        put_dir_attrs(out, true);
        finish(out, s);
        return;
    }
    struct stat st;
    int rc = h->fd >= 0 ? ::fstat(h->fd, &st) : ::stat(h->real.c_str(), &st);
    if (rc != 0) {
        if (h->isDir) {
            size_t s = begin(out, FXP_ATTRS, id);
            put_dir_attrs(out, h->mount->readOnly);
            finish(out, s);
            return;
        }
        status_errno(out, id, errno);
        return;
    }
    size_t s = begin(out, FXP_ATTRS, id);
    put_attrs(out, st, h->mount->readOnly);
    finish(out, s);
}

void SftpSession::opSetstat(uint32_t id, ssh::Reader &r, ssh::Writer &out, bool byHandle) {
    Handle *h = nullptr;
    Vfs::Resolved res;
    if (byHandle) {
        h = lookup(r.str());
    } else {
        res = vfs_.resolve(Vfs::normalize(r.str()));
    }
    Attrs a = parse_attrs(r);
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    if (byHandle && !h) {
        status(out, id, FX_FAILURE, "invalid handle");
        return;
    }
    if (!byHandle && res.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_NO_SUCH_FILE, "no such file");
        return;
    }
    // Permissions and timestamps cannot be changed on the Wii U file systems;
    // accept them silently so clients do not abort uploads. Size changes are
    // real and honoured.
    if (a.flags & ATTR_SIZE) {
        const Mount *m = byHandle ? h->mount : res.mount;
        if (!m || m->readOnly || (!byHandle && res.kind != Vfs::Kind::Inside)) {
            denied(out, id);
            return;
        }
        int rc;
        if (byHandle && h->fd >= 0) {
            rc = ::ftruncate(h->fd, (off_t) a.size);
        } else {
            int fd = ::open((byHandle ? h->real : res.real).c_str(), O_RDWR | O_BINARY);
            if (fd < 0) {
                status_errno(out, id, errno);
                return;
            }
            rc = ::ftruncate(fd, (off_t) a.size);
            ::close(fd);
        }
        if (rc != 0) {
            status_errno(out, id, errno);
            return;
        }
        if (h) {
            h->posKnown = false;
        }
    }
    status(out, id, FX_OK, "");
}

void SftpSession::opOpendir(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    std::string path = Vfs::normalize(r.str());
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    Vfs::Resolved res = vfs_.resolve(path);
    if (res.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_NO_SUCH_FILE, "no such directory");
        return;
    }
    if (handles_.size() >= kMaxHandles) {
        status(out, id, FX_FAILURE, "too many open handles");
        return;
    }
    Handle h;
    h.isDir = true;
    if (res.kind == Vfs::Kind::Root) {
        h.rootListing = true;
    } else {
        h.dir = ::opendir(res.real.c_str());
        if (!h.dir) {
            status_errno(out, id, errno);
            return;
        }
        h.mount = res.mount;
        h.real  = res.real;
    }
    uint32_t key  = nextHandle_++;
    handles_[key] = h;
    send_handle(out, id, key);
}

void SftpSession::opReaddir(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    Handle *h = lookup(r.str());
    if (!h || !h->isDir) {
        status(out, id, FX_FAILURE, "invalid handle");
        return;
    }
    size_t s        = begin(out, FXP_NAME, id);
    size_t countPos = out.size();
    out.u32(0);
    uint32_t count = 0;

    if (h->rootListing) {
        const auto &mounts = vfs_.mounts();
        for (; h->rootIndex < mounts.size(); h->rootIndex++) {
            const Mount &m = mounts[h->rootIndex];
            out.str(m.name);
            out.str(long_name(m.name, true, m.readOnly, 0, 0));
            put_dir_attrs(out, m.readOnly);
            count++;
        }
    } else {
        while (count < 128 && out.size() - s < 48 * 1024) {
            errno           = 0;
            struct dirent *e = ::readdir(h->dir);
            if (!e) {
                break;
            }
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
                continue;
            }
            std::string name = e->d_name;
            struct stat st;
            bool haveStat = ::stat(join(h->real, e->d_name).c_str(), &st) == 0;
            bool dir      = haveStat ? S_ISDIR(st.st_mode) : false;
            uint64_t size = haveStat && !dir ? (uint64_t) st.st_size : 0;
            int64_t mtime = haveStat ? (int64_t) st.st_mtime : 0;
            out.str(name);
            out.str(long_name(name, dir, h->mount->readOnly, size, mtime));
            if (haveStat) {
                put_attrs(out, st, h->mount->readOnly);
            } else {
                out.u32(0);
            }
            count++;
        }
    }

    if (count == 0) {
        out.buf.resize(s);
        status(out, id, FX_EOF, "end of directory");
        return;
    }
    out.patch_u32(countPos, count);
    finish(out, s);
}

void SftpSession::opRemove(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    Vfs::Resolved res = vfs_.resolve(Vfs::normalize(r.str()));
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    if (res.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_NO_SUCH_FILE, "no such file");
        return;
    }
    if (res.kind != Vfs::Kind::Inside || res.mount->readOnly) {
        denied(out, id);
        return;
    }
    if (::unlink(res.real.c_str()) != 0) {
        status_errno(out, id, errno);
        return;
    }
    status(out, id, FX_OK, "");
}

void SftpSession::opMkdir(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    Vfs::Resolved res = vfs_.resolve(Vfs::normalize(r.str()));
    parse_attrs(r);
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    if (res.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_PERMISSION_DENIED, "cannot create directories in /");
        return;
    }
    if (res.kind != Vfs::Kind::Inside) {
        status(out, id, FX_FAILURE, "already exists");
        return;
    }
    if (res.mount->readOnly) {
        denied(out, id);
        return;
    }
    if (make_dir(res.real.c_str()) != 0) {
        status_errno(out, id, errno);
        return;
    }
    status(out, id, FX_OK, "");
}

void SftpSession::opRmdir(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    Vfs::Resolved res = vfs_.resolve(Vfs::normalize(r.str()));
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    if (res.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_NO_SUCH_FILE, "no such directory");
        return;
    }
    if (res.kind != Vfs::Kind::Inside || res.mount->readOnly) {
        denied(out, id);
        return;
    }
    if (::rmdir(res.real.c_str()) != 0) {
        status_errno(out, id, errno);
        return;
    }
    status(out, id, FX_OK, "");
}

void SftpSession::opRealpath(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    std::string path = Vfs::normalize(r.str());
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    size_t s = begin(out, FXP_NAME, id);
    out.u32(1);
    out.str(path);
    out.str(path);
    out.u32(0);
    finish(out, s);
}

void SftpSession::opRename(uint32_t id, std::string_view from, std::string_view to, bool overwrite, ssh::Writer &out) {
    Vfs::Resolved a = vfs_.resolve(Vfs::normalize(from));
    Vfs::Resolved b = vfs_.resolve(Vfs::normalize(to));
    if (a.kind == Vfs::Kind::NotFound) {
        status(out, id, FX_NO_SUCH_FILE, "no such file");
        return;
    }
    if (a.kind != Vfs::Kind::Inside || b.kind != Vfs::Kind::Inside || a.mount->readOnly) {
        denied(out, id);
        return;
    }
    if (a.mount != b.mount) {
        status(out, id, FX_FAILURE, "cannot move between devices");
        return;
    }
    struct stat st;
    if (::stat(b.real.c_str(), &st) == 0) {
        if (!overwrite || S_ISDIR(st.st_mode)) {
            status(out, id, FX_FAILURE, "target exists");
            return;
        }
        if (::unlink(b.real.c_str()) != 0) {
            status_errno(out, id, errno);
            return;
        }
    }
    if (::rename(a.real.c_str(), b.real.c_str()) != 0) {
        status_errno(out, id, errno);
        return;
    }
    status(out, id, FX_OK, "");
}

void SftpSession::opExtended(uint32_t id, ssh::Reader &r, ssh::Writer &out) {
    std::string_view name = r.str();
    if (!r.ok) {
        status(out, id, FX_BAD_MESSAGE, "malformed request");
        return;
    }
    if (name == "posix-rename@openssh.com") {
        std::string_view from = r.str();
        std::string_view to   = r.str();
        if (!r.ok) {
            status(out, id, FX_BAD_MESSAGE, "malformed request");
            return;
        }
        opRename(id, from, to, true, out);
    } else if (name == "fsync@openssh.com") {
        Handle *h = lookup(r.str());
        if (!h || h->fd < 0) {
            status(out, id, FX_FAILURE, "invalid handle");
            return;
        }
        sync_fd(h->fd);
        status(out, id, FX_OK, "");
    } else if (name == "statvfs@openssh.com") {
        Vfs::Resolved res = vfs_.resolve(Vfs::normalize(r.str()));
        if (!r.ok) {
            status(out, id, FX_BAD_MESSAGE, "malformed request");
            return;
        }
        if (res.kind != Vfs::Kind::MountRoot && res.kind != Vfs::Kind::Inside) {
            status(out, id, FX_OP_UNSUPPORTED, "not a device");
            return;
        }
#ifdef _WIN32
        status(out, id, FX_OP_UNSUPPORTED, "statvfs not available");
#else
        struct statvfs sv;
        if (::statvfs(res.real.c_str(), &sv) != 0) {
            status_errno(out, id, errno);
            return;
        }
        size_t s = begin(out, FXP_EXTENDED_REPLY, id);
        out.u64(sv.f_bsize);
        out.u64(sv.f_frsize);
        out.u64(sv.f_blocks);
        out.u64(sv.f_bfree);
        out.u64(sv.f_bavail);
        out.u64(sv.f_files);
        out.u64(sv.f_ffree);
        out.u64(sv.f_favail);
        out.u64(sv.f_fsid);
        out.u64(res.mount->readOnly ? 1 : 0); // SSH2_FXE_STATVFS_ST_RDONLY
        out.u64(sv.f_namemax);
        finish(out, s);
#endif
    } else if (name == "limits@openssh.com") {
        size_t s = begin(out, FXP_EXTENDED_REPLY, id);
        out.u64(kMaxRequest);
        out.u64(kMaxIo);
        out.u64(kMaxIo);
        out.u64(kMaxHandles);
        finish(out, s);
    } else {
        status(out, id, FX_OP_UNSUPPORTED, "extension not supported");
    }
}
