#include "vfs.h"

std::string Vfs::normalize(std::string_view path) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string_view::npos) {
            end = path.size();
        }
        std::string_view part = path.substr(start, end - start);
        if (part.empty() || part == ".") {
            // skip
        } else if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else {
            parts.push_back(part);
        }
        start = end + 1;
    }
    std::string out;
    for (auto p : parts) {
        out += '/';
        out += p;
    }
    if (out.empty()) {
        out = "/";
    }
    return out;
}

Vfs::Resolved Vfs::resolve(const std::string &path) const {
    Resolved r;
    if (path == "/") {
        r.kind = Kind::Root;
        return r;
    }
    size_t slash          = path.find('/', 1);
    std::string_view head = std::string_view(path).substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
    for (const auto &m : mounts_) {
        if (m.name == head) {
            r.mount = &m;
            if (slash == std::string::npos) {
                r.kind = Kind::MountRoot;
                r.real = m.root + "/";
            } else {
                r.kind = Kind::Inside;
                r.real = m.root + path.substr(slash);
            }
            return r;
        }
    }
    return r;
}
