#pragma once
// File operations for the app's file browser. Copy, move and delete run on a
// worker thread so the UI stays responsive and can show progress and cancel.
// All paths here are real (device) paths, e.g. "fs:/vol/external01/foo".

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace fileops {

bool exists(const std::string &real);
bool is_dir(const std::string &real);
// Rejects empty names, "." / "..", path separators and characters FAT32 forbids.
bool valid_name(const std::string &name);
// "name.txt" -> "name (2).txt" etc. until nothing with that name exists in dir.
std::string unique_name(const std::string &dirReal, const std::string &name);
std::string join(const std::string &dir, const std::string &name);
// errno-style result (0 on success).
int make_dir(const std::string &real);
int create_file(const std::string &real);
int rename_path(const std::string &from, const std::string &to);

class Job {
public:
    enum class Kind { Copy, Move, Delete };

    ~Job();

    // Copy/Move: dst is the full destination path and must not exist yet; a
    // canceled or failed copy removes it again. sameDevice lets Move use a
    // plain rename. Returns false if a job is already running.
    bool start(Kind kind, const std::string &src, const std::string &dst, bool sameDevice);
    void cancel() { cancel_ = true; }
    bool active() const { return active_; }
    // True once after the worker finished; joins the thread.
    bool collect();

    Kind kind() const { return kind_; }
    uint64_t bytesDone();
    uint64_t bytesTotal();
    int itemsDone() const { return itemsDone_; }
    int itemsTotal() const { return itemsTotal_; }
    std::string current();
    int error() const { return error_; } // errno of the first failure, 0 if none
    bool canceled() const { return cancel_; }

private:
    void run();
    void scan(const std::string &path);
    bool copyTree(const std::string &src, const std::string &dst);
    bool copyFile(const std::string &src, const std::string &dst);
    bool removeTree(const std::string &path);
    void setCurrent(const std::string &path);
    void addBytes(uint64_t done, uint64_t total);
    bool fail(int err);

    Kind kind_ = Kind::Copy;
    std::string src_, dst_;
    bool sameDevice_ = false;
    std::thread thread_;
    std::atomic<bool> active_{false};
    std::atomic<bool> done_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<int> itemsDone_{0}, itemsTotal_{0};
    std::atomic<int> error_{0};
    // PowerPC 32 has no 64-bit atomics, so these share the mutex.
    std::mutex mutex_;
    uint64_t bytesDone_  = 0;
    uint64_t bytesTotal_ = 0;
    std::string current_;
};

} // namespace fileops
