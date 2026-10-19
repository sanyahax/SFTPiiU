#pragma once
// Virtual file tree: "/" lists the mounted devices, "/<mount>/..." maps onto
// the device path that newlib/devoptab understands (e.g. "storage_mlc:/sys").

#include <string>
#include <string_view>
#include <vector>

struct Mount {
    std::string name;
    std::string root; // device prefix without trailing slash, e.g. "fs:/vol/external01"
    bool readOnly;
};

class Vfs {
public:
    enum class Kind { Root, MountRoot, Inside, NotFound };

    struct Resolved {
        Kind kind          = Kind::NotFound;
        const Mount *mount = nullptr;
        std::string real;
    };

    void add(std::string name, std::string root, bool readOnly) {
        mounts_.push_back({std::move(name), std::move(root), readOnly});
    }

    const std::vector<Mount> &mounts() const { return mounts_; }

    // Turns any client path into "/a/b" form; relative paths start at "/".
    static std::string normalize(std::string_view path);

    // Expects a normalized path.
    Resolved resolve(const std::string &path) const;

private:
    std::vector<Mount> mounts_;
};
