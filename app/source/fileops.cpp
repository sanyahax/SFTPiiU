#include "fileops.h"
#include "platform.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fileops {

namespace {

constexpr size_t kCopyBuffer = 256 * 1024;

std::vector<std::string> list_dir(const std::string &dir, int &err) {
    std::vector<std::string> names;
    err      = 0;
    DIR *d   = opendir(dir.c_str());
    if (!d) {
        err = errno ? errno : EIO;
        return names;
    }
    // Collect first and close the handle before recursing: the Wii U file
    // systems allow only a few open directories at once.
    while (struct dirent *e = readdir(d)) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        names.emplace_back(e->d_name);
    }
    closedir(d);
    return names;
}

// Best-effort recursive delete, used to clean up an incomplete copy.
void remove_all(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        int err;
        for (const auto &n : list_dir(path, err)) {
            remove_all(path + "/" + n);
        }
        rmdir(path.c_str());
    } else {
        unlink(path.c_str());
    }
}

} // namespace

bool exists(const std::string &real) {
    struct stat st;
    return stat(real.c_str(), &st) == 0;
}

bool is_dir(const std::string &real) {
    struct stat st;
    return stat(real.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool valid_name(const std::string &name) {
    if (name.empty() || name == "." || name == ".." || name.size() > 255) {
        return false;
    }
    for (unsigned char c : name) {
        if (c < 0x20 || strchr("/\\:*?\"<>|", c)) {
            return false;
        }
    }
    return name.back() != ' ' && name.back() != '.';
}

std::string join(const std::string &dir, const std::string &name) {
    if (!dir.empty() && dir.back() == '/') {
        return dir + name;
    }
    return dir + "/" + name;
}

std::string unique_name(const std::string &dirReal, const std::string &name) {
    if (!exists(join(dirReal, name))) {
        return name;
    }
    size_t dot      = name.rfind('.');
    bool hasExt     = dot != std::string::npos && dot > 0;
    std::string base = hasExt ? name.substr(0, dot) : name;
    std::string ext  = hasExt ? name.substr(dot) : "";
    for (int i = 2; i < 1000; i++) {
        std::string candidate = base + " (" + std::to_string(i) + ")" + ext;
        if (!exists(join(dirReal, candidate))) {
            return candidate;
        }
    }
    return name;
}

int make_dir(const std::string &real) {
    return mkdir(real.c_str(), 0777) == 0 ? 0 : errno;
}

int create_file(const std::string &real) {
    int fd = open(real.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return errno;
    }
    close(fd);
    return 0;
}

int rename_path(const std::string &from, const std::string &to) {
    return rename(from.c_str(), to.c_str()) == 0 ? 0 : errno;
}

// ---------------------------------------------------------------- Job

Job::~Job() {
    cancel_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool Job::start(Kind kind, const std::string &src, const std::string &dst, bool sameDevice) {
    if (active_) {
        return false;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    kind_       = kind;
    src_        = src;
    dst_        = dst;
    sameDevice_ = sameDevice;
    cancel_     = false;
    done_       = false;
    error_      = 0;
    itemsDone_ = itemsTotal_ = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bytesDone_ = bytesTotal_ = 0;
        current_.clear();
    }
    active_ = true;
    thread_ = std::thread(&Job::run, this);
    plat::tune_thread(thread_, "SFTPiiU file job");
    return true;
}

bool Job::collect() {
    if (!active_ || !done_) {
        return false;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    active_ = false;
    return true;
}

std::string Job::current() {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

uint64_t Job::bytesDone() {
    std::lock_guard<std::mutex> lock(mutex_);
    return bytesDone_;
}

uint64_t Job::bytesTotal() {
    std::lock_guard<std::mutex> lock(mutex_);
    return bytesTotal_;
}

void Job::addBytes(uint64_t done, uint64_t total) {
    std::lock_guard<std::mutex> lock(mutex_);
    bytesDone_ += done;
    bytesTotal_ += total;
}

void Job::setCurrent(const std::string &path) {
    size_t slash = path.rfind('/');
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = slash == std::string::npos ? path : path.substr(slash + 1);
}

bool Job::fail(int err) {
    if (error_ == 0) {
        error_ = err ? err : EIO;
    }
    return false;
}

void Job::scan(const std::string &path) {
    if (cancel_) {
        return;
    }
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return;
    }
    itemsTotal_++;
    if (S_ISDIR(st.st_mode)) {
        int err;
        for (const auto &n : list_dir(path, err)) {
            scan(join(path, n));
        }
    } else {
        addBytes(0, (uint64_t) st.st_size);
    }
}

void Job::run() {
    if (kind_ == Kind::Move && sameDevice_) {
        // Same file system: a rename is instant and atomic.
        itemsTotal_ = 1;
        setCurrent(src_);
        int err = rename_path(src_, dst_);
        if (err) {
            fail(err);
        }
        itemsDone_ = 1;
    } else {
        scan(src_);
        if (kind_ == Kind::Delete) {
            removeTree(src_);
        } else if (!copyTree(src_, dst_)) {
            // The destination did not exist before (the UI picks a free
            // name), so an incomplete copy can go entirely.
            setCurrent(dst_);
            remove_all(dst_);
        } else if (kind_ == Kind::Move && !cancel_) {
            // Only remove the source once the copy is complete.
            itemsDone_ = 0;
            removeTree(src_);
        }
    }
    done_ = true;
}

bool Job::copyFile(const std::string &src, const std::string &dst) {
    setCurrent(src);
    int in = open(src.c_str(), O_RDONLY);
    if (in < 0) {
        return fail(errno);
    }
    int out = open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) {
        int err = errno;
        close(in);
        return fail(err);
    }
    // 64-byte aligned buffer lets the Wii U file system skip a bounce copy.
    std::unique_ptr<uint8_t[]> raw(new uint8_t[kCopyBuffer + 64]);
    uint8_t *buf = (uint8_t *) (((uintptr_t) raw.get() + 63) & ~(uintptr_t) 63);
    bool ok      = true;
    for (;;) {
        if (cancel_) {
            ok = false;
            break;
        }
        ssize_t n = read(in, buf, kCopyBuffer);
        if (n < 0) {
            ok = fail(errno);
            break;
        }
        if (n == 0) {
            break;
        }
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, (size_t) (n - off));
            if (w <= 0) {
                ok = fail(w < 0 ? errno : ENOSPC);
                break;
            }
            off += w;
        }
        if (!ok) {
            break;
        }
        addBytes((uint64_t) n, 0);
    }
    close(in);
    if (close(out) != 0 && ok) {
        ok = fail(errno);
    }
    if (!ok) {
        unlink(dst.c_str()); // no half-written files
        return false;
    }
    itemsDone_++;
    return true;
}

bool Job::copyTree(const std::string &src, const std::string &dst) {
    if (cancel_) {
        return false;
    }
    struct stat st;
    if (stat(src.c_str(), &st) != 0) {
        return fail(errno);
    }
    if (!S_ISDIR(st.st_mode)) {
        return copyFile(src, dst);
    }
    setCurrent(src);
    if (!is_dir(dst) && mkdir(dst.c_str(), 0777) != 0) {
        return fail(errno);
    }
    itemsDone_++;
    int err;
    std::vector<std::string> names = list_dir(src, err);
    if (err) {
        return fail(err);
    }
    for (const auto &n : names) {
        if (!copyTree(join(src, n), join(dst, n))) {
            return false;
        }
    }
    return true;
}

bool Job::removeTree(const std::string &path) {
    if (cancel_) {
        return false;
    }
    setCurrent(path);
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return fail(errno);
    }
    if (S_ISDIR(st.st_mode)) {
        int err;
        std::vector<std::string> names = list_dir(path, err);
        if (err) {
            return fail(err);
        }
        for (const auto &n : names) {
            if (!removeTree(join(path, n))) {
                return false;
            }
        }
        if (rmdir(path.c_str()) != 0) {
            return fail(errno);
        }
    } else if (unlink(path.c_str()) != 0) {
        return fail(errno);
    }
    itemsDone_++;
    return true;
}

} // namespace fileops
