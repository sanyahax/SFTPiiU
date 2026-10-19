#pragma once
// Read-only browser over the same virtual tree the SFTP server exposes:
// "/" lists the drives, "/sd/..." etc. map to the device paths.

#include "vfs.h"

#include <cstdint>
#include <string>
#include <vector>

class Explorer {
public:
    static constexpr size_t kMaxEntries = 3000;

    struct Entry {
        std::string name;
        bool dir      = false;
        bool statOk   = false;
        uint64_t size = 0;
        int64_t mtime = 0;
    };

    explicit Explorer(const Vfs *vfs = nullptr) : vfs_(vfs) {}
    void setVfs(const Vfs *vfs) { vfs_ = vfs; }
    const Vfs *vfs() const { return vfs_; }
    void setShowHidden(bool show) { showHidden_ = show; }

    // Loads a directory (blocking). Returns false if it cannot be opened.
    bool open(const std::string &path);
    // Goes to the parent directory and selects the folder we came from.
    void up();
    // Reloads the current directory and selects `name` if it still exists
    // (otherwise keeps the position).
    void reload(const std::string &name = {});

    // Real (device) path of an entry or of the current directory.
    std::string realPath(const std::string &virtualPath) const;

    const std::string &path() const { return path_; }
    const std::vector<Entry> &entries() const { return entries_; }
    bool failed() const { return failed_; }
    bool truncated() const { return truncated_; }
    bool atRoot() const { return path_ == "/"; }
    // Mount of the current directory, or nullptr at the root.
    const Mount *mount() const;
    std::string childPath(const Entry &e) const;

    int selected = 0;
    int scroll   = 0;

private:
    const Vfs *vfs_;
    std::string path_ = "/";
    std::vector<Entry> entries_;
    bool failed_     = false;
    bool truncated_  = false;
    bool showHidden_ = false;
};
