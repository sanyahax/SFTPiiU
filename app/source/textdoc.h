#pragma once
// Documents for the built-in viewers: line-based text editing with undo,
// a hex dump for binary files, and file type detection.

#include <cstdint>
#include <string>
#include <vector>

enum class FileKind { Text, Image, Binary };

constexpr uint64_t kMaxTextSize  = 1024 * 1024;
constexpr uint64_t kMaxHexBytes  = 64 * 1024;
constexpr uint64_t kMaxImageSize = 16 * 1024 * 1024;

// By extension; unknown types are sniffed (no NUL bytes in the first 4 KB
// means text).
FileKind classify_file(const std::string &name, const std::string &real);

class TextDoc {
public:
    // errno-style results (0 on success, EFBIG if larger than kMaxTextSize).
    int load(const std::string &real);
    int save(const std::string &real);

    const std::vector<std::string> &lines() const { return lines_; }
    bool modified() const { return modified_; }
    bool canUndo() const { return !undo_.empty(); }
    std::string format() const; // e.g. "UTF-8 · CRLF"

    void setLine(int i, const std::string &text);
    void insertLine(int i, const std::string &text);
    void deleteLine(int i);
    // Returns the line to select after undoing, or -1 if nothing to undo.
    int undo();

private:
    struct Op {
        enum Type { Set, Insert, Delete } type;
        int index;
        std::string text;     // previous text for Set/Delete
        bool emptied = false; // Delete removed the last line (a blank one was added)
    };
    void push(Op op);

    std::vector<std::string> lines_{""};
    std::vector<Op> undo_;
    bool modified_     = false;
    bool crlf_         = false;
    bool bom_          = false;
    bool finalNewline_ = false;
    size_t savedDepth_ = 0; // undo depth at the last save
};

// First kMaxHexBytes bytes of a file; total is the full size.
int load_hex(const std::string &real, std::vector<uint8_t> &out, uint64_t &total);
