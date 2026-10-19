#include "textdoc.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string lower_ext(const std::string &name) {
    size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) {
        return {};
    }
    std::string ext = name.substr(dot + 1);
    for (auto &c : ext) {
        c = (char) std::tolower((unsigned char) c);
    }
    return ext;
}

bool read_all(const std::string &real, uint64_t limit, std::string &out, uint64_t &size, int &err) {
    struct stat st;
    if (stat(real.c_str(), &st) != 0) {
        err = errno;
        return false;
    }
    size = (uint64_t) st.st_size;
    if (size > limit) {
        err = EFBIG;
        return false;
    }
    int fd = open(real.c_str(), O_RDONLY);
    if (fd < 0) {
        err = errno;
        return false;
    }
    out.resize((size_t) size);
    size_t got = 0;
    while (got < out.size()) {
        ssize_t n = read(fd, &out[got], out.size() - got);
        if (n < 0) {
            err = errno;
            close(fd);
            return false;
        }
        if (n == 0) {
            break;
        }
        got += (size_t) n;
    }
    close(fd);
    out.resize(got);
    return true;
}

} // namespace

FileKind classify_file(const std::string &name, const std::string &real) {
    static const char *images[] = {"png", "jpg", "jpeg", "bmp", "gif", "tga", "webp"};
    static const char *texts[]  = {"txt", "ini", "cfg", "conf", "json", "xml", "log", "md", "csv", "yml",
                                   "yaml", "toml", "lua", "js", "py", "sh", "bat", "c", "h", "cpp", "hpp",
                                   "html", "htm", "css", "nfo", "srt", "properties", "inf", "list", "meta"};
    std::string ext = lower_ext(name);
    for (const char *e : images) {
        if (ext == e) return FileKind::Image;
    }
    bool textExt = false;
    for (const char *e : texts) {
        if (ext == e) textExt = true;
    }
    // Sniff the start of the file: NUL bytes mean binary.
    int fd = open(real.c_str(), O_RDONLY);
    if (fd < 0) {
        return textExt ? FileKind::Text : FileKind::Binary;
    }
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n < 0) {
        return textExt ? FileKind::Text : FileKind::Binary;
    }
    return memchr(buf, 0, (size_t) n) ? FileKind::Binary : FileKind::Text;
}

int TextDoc::load(const std::string &real) {
    std::string data;
    uint64_t size;
    int err = 0;
    if (!read_all(real, kMaxTextSize, data, size, err)) {
        return err ? err : EIO;
    }
    bom_ = data.size() >= 3 && (uint8_t) data[0] == 0xEF && (uint8_t) data[1] == 0xBB && (uint8_t) data[2] == 0xBF;
    if (bom_) {
        data.erase(0, 3);
    }
    crlf_         = data.find("\r\n") != std::string::npos;
    finalNewline_ = !data.empty() && data.back() == '\n';

    lines_.clear();
    size_t start = 0;
    while (start <= data.size()) {
        size_t end = data.find('\n', start);
        if (end == std::string::npos) {
            end = data.size();
        }
        std::string line = data.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines_.push_back(line);
        start = end + 1;
    }
    if (finalNewline_ && lines_.size() > 1) {
        lines_.pop_back(); // the empty piece after the last newline
    }
    if (lines_.empty()) {
        lines_.emplace_back();
    }
    undo_.clear();
    modified_   = false;
    savedDepth_ = 0;
    return 0;
}

int TextDoc::save(const std::string &real) {
    std::string data = bom_ ? "\xEF\xBB\xBF" : "";
    const char *eol  = crlf_ ? "\r\n" : "\n";
    for (size_t i = 0; i < lines_.size(); i++) {
        data += lines_[i];
        if (i + 1 < lines_.size() || finalNewline_) {
            data += eol;
        }
    }
    // Write next to the original first, so a failed write never destroys it.
    std::string tmp = real + ".sftpiiu-tmp";
    int fd          = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return errno;
    }
    size_t off = 0;
    while (off < data.size()) {
        ssize_t n = write(fd, data.data() + off, data.size() - off);
        if (n <= 0) {
            int err = n < 0 ? errno : ENOSPC;
            close(fd);
            unlink(tmp.c_str());
            return err;
        }
        off += (size_t) n;
    }
    if (close(fd) != 0) {
        int err = errno;
        unlink(tmp.c_str());
        return err;
    }
    // The Wii U file systems do not replace on rename, so remove first.
    unlink(real.c_str());
    if (rename(tmp.c_str(), real.c_str()) != 0) {
        return errno; // the new content is still in the .sftpiiu-tmp file
    }
    modified_   = false;
    savedDepth_ = undo_.size();
    return 0;
}

std::string TextDoc::format() const {
    std::string f = bom_ ? "UTF-8 BOM" : "UTF-8";
    f += crlf_ ? " · CRLF" : " · LF";
    return f;
}

void TextDoc::push(Op op) {
    if (undo_.size() >= 500) {
        undo_.erase(undo_.begin());
        savedDepth_ = savedDepth_ ? savedDepth_ - 1 : (size_t) -1;
    }
    undo_.push_back(std::move(op));
    modified_ = true;
}

void TextDoc::setLine(int i, const std::string &text) {
    if (i < 0 || i >= (int) lines_.size() || lines_[i] == text) {
        return;
    }
    push({Op::Set, i, lines_[i]});
    lines_[i] = text;
}

void TextDoc::insertLine(int i, const std::string &text) {
    i = std::clamp(i, 0, (int) lines_.size());
    push({Op::Insert, i, {}});
    lines_.insert(lines_.begin() + i, text);
}

void TextDoc::deleteLine(int i) {
    if (i < 0 || i >= (int) lines_.size()) {
        return;
    }
    Op op{Op::Delete, i, lines_[i]};
    lines_.erase(lines_.begin() + i);
    if (lines_.empty()) {
        lines_.emplace_back(); // a document always has one line
        op.emptied = true;
    }
    push(op);
}

int TextDoc::undo() {
    if (undo_.empty()) {
        return -1;
    }
    Op op = undo_.back();
    undo_.pop_back();
    switch (op.type) {
        case Op::Set:
            lines_[op.index] = op.text;
            break;
        case Op::Insert:
            lines_.erase(lines_.begin() + op.index);
            if (lines_.empty()) lines_.emplace_back();
            break;
        case Op::Delete:
            if (op.emptied) {
                lines_[0] = op.text;
            } else {
                lines_.insert(lines_.begin() + op.index, op.text);
            }
            break;
    }
    modified_ = undo_.size() != savedDepth_;
    return std::min(op.index, (int) lines_.size() - 1);
}

int load_hex(const std::string &real, std::vector<uint8_t> &out, uint64_t &total) {
    struct stat st;
    if (stat(real.c_str(), &st) != 0) {
        return errno;
    }
    total  = (uint64_t) st.st_size;
    int fd = open(real.c_str(), O_RDONLY);
    if (fd < 0) {
        return errno;
    }
    out.resize((size_t) std::min<uint64_t>(total, kMaxHexBytes));
    size_t got = 0;
    while (got < out.size()) {
        ssize_t n = read(fd, out.data() + got, out.size() - got);
        if (n <= 0) {
            break;
        }
        got += (size_t) n;
    }
    close(fd);
    out.resize(got);
    return 0;
}
