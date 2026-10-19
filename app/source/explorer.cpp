#include "explorer.h"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>

namespace {

// Natural, case-insensitive order: "Screenshot_2" before "Screenshot_10".
bool less_name(const std::string &a, const std::string &b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        unsigned char ca = (unsigned char) a[i], cb = (unsigned char) b[j];
        if (std::isdigit(ca) && std::isdigit(cb)) {
            size_t ei = i, ej = j;
            while (ei < a.size() && std::isdigit((unsigned char) a[ei])) ei++;
            while (ej < b.size() && std::isdigit((unsigned char) b[ej])) ej++;
            // Compare numerically: skip leading zeros, then by length, then digits.
            size_t si = i, sj = j;
            while (si + 1 < ei && a[si] == '0') si++;
            while (sj + 1 < ej && b[sj] == '0') sj++;
            if (ei - si != ej - sj) {
                return ei - si < ej - sj;
            }
            int c = a.compare(si, ei - si, b, sj, ej - sj);
            if (c != 0) {
                return c < 0;
            }
            i = ei;
            j = ej;
            continue;
        }
        int la = std::tolower(ca), lb = std::tolower(cb);
        if (la != lb) {
            return la < lb;
        }
        i++;
        j++;
    }
    return a.size() - i < b.size() - j;
}

} // namespace

const Mount *Explorer::mount() const {
    if (!vfs_ || atRoot()) {
        return nullptr;
    }
    return vfs_->resolve(path_).mount;
}

std::string Explorer::childPath(const Entry &e) const {
    return atRoot() ? "/" + e.name : path_ + "/" + e.name;
}

bool Explorer::open(const std::string &rawPath) {
    std::string path = Vfs::normalize(rawPath);
    entries_.clear();
    failed_    = false;
    truncated_ = false;
    selected   = 0;
    scroll     = 0;
    path_      = path;
    if (!vfs_) {
        failed_ = true;
        return false;
    }

    Vfs::Resolved res = vfs_->resolve(path);
    if (res.kind == Vfs::Kind::Root) {
        for (const auto &m : vfs_->mounts()) {
            Entry e;
            e.name   = m.name;
            e.dir    = true;
            e.statOk = true;
            entries_.push_back(e);
        }
        return true;
    }
    if (res.kind == Vfs::Kind::NotFound) {
        failed_ = true;
        return false;
    }

    DIR *dir = opendir(res.real.c_str());
    if (!dir) {
        failed_ = true;
        return false;
    }
    std::string base = res.real;
    if (base.empty() || base.back() != '/') {
        base += '/';
    }
    while (struct dirent *d = readdir(dir)) {
        if (d->d_name[0] == '.' && (d->d_name[1] == 0 || (d->d_name[1] == '.' && d->d_name[2] == 0))) {
            continue;
        }
        if (d->d_name[0] == '.' && !showHidden_) {
            continue;
        }
        if (entries_.size() >= kMaxEntries) {
            truncated_ = true;
            break;
        }
        Entry e;
        e.name = d->d_name;
        struct stat st;
        if (stat((base + e.name).c_str(), &st) == 0) {
            e.statOk = true;
            e.dir    = S_ISDIR(st.st_mode);
            e.size   = e.dir ? 0 : (uint64_t) st.st_size;
            e.mtime  = (int64_t) st.st_mtime;
        } else {
            e.dir = d->d_type == DT_DIR;
        }
        entries_.push_back(e);
    }
    closedir(dir);

    std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) {
        if (a.dir != b.dir) {
            return a.dir;
        }
        return less_name(a.name, b.name);
    });
    return true;
}

std::string Explorer::realPath(const std::string &virtualPath) const {
    if (!vfs_) {
        return {};
    }
    Vfs::Resolved r = vfs_->resolve(Vfs::normalize(virtualPath));
    return r.kind == Vfs::Kind::Inside || r.kind == Vfs::Kind::MountRoot ? r.real : std::string();
}

void Explorer::reload(const std::string &name) {
    int keep        = selected;
    std::string cur = path_;
    open(cur);
    selected = std::min(keep, std::max(0, (int) entries_.size() - 1));
    if (!name.empty()) {
        for (size_t i = 0; i < entries_.size(); i++) {
            if (entries_[i].name == name) {
                selected = (int) i;
                break;
            }
        }
    }
}

void Explorer::up() {
    if (atRoot()) {
        return;
    }
    size_t slash     = path_.rfind('/');
    std::string from = path_.substr(slash + 1);
    open(slash == 0 ? "/" : path_.substr(0, slash));
    for (size_t i = 0; i < entries_.size(); i++) {
        if (entries_[i].name == from) {
            selected = (int) i;
            break;
        }
    }
}
