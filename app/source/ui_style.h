#pragma once
// Shared look of the app's screens (Wii U Menu inspired palette and layout).

#include "i18n.h"
#include "render.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace ui {

constexpr int kW = 1280;
constexpr int kH = 720;

constexpr uint32_t kBgTop     = 0xF6F7F9FF;
constexpr uint32_t kBgBottom  = 0xDFE5ECFF;
constexpr uint32_t kWhite     = 0xFFFFFFFF;
constexpr uint32_t kText      = 0x323A45FF;
constexpr uint32_t kDim       = 0x87909BFF;
constexpr uint32_t kBlue      = 0x009AC7FF;
constexpr uint32_t kGlow      = 0x2EC5F000; // alpha added at draw time
constexpr uint32_t kBlueSoft  = 0xE1F4FBFF;
constexpr uint32_t kOrange    = 0xF39200FF;
constexpr uint32_t kGreen     = 0x3FAE3AFF;
constexpr uint32_t kGreenSoft = 0xE4F5E2FF;
constexpr uint32_t kRed       = 0xE5413AFF;
constexpr uint32_t kRedSoft   = 0xFCE6E5FF;
constexpr uint32_t kTrack     = 0xE6EAEFFF;
constexpr uint32_t kFooter    = 0xEEF1F4FF;
constexpr uint32_t kLine      = 0xD8DEE5FF;
constexpr uint32_t kGrey      = 0x9AA3ADFF;

constexpr int kContentTop = 168;
constexpr int kFooterY    = 668;

inline uint32_t with_alpha(uint32_t rgb, int a) {
    return (rgb & 0xFFFFFF00u) | (uint32_t) std::clamp(a, 0, 255);
}

inline std::string basename_of(const std::string &path) {
    size_t s = path.rfind('/');
    return s == std::string::npos ? path : path.substr(s + 1);
}

inline std::string dirname_of(const std::string &path) {
    size_t s = path.rfind('/');
    return s == std::string::npos || s == 0 ? "/" : path.substr(0, s);
}

Icon drive_icon(const std::string &mountName);
std::string drive_name(const std::string &mountName);

// Touch targets registered while drawing.
enum HitKind { HIT_TAB, HIT_BUTTON, HIT_DRIVE, HIT_FILE, HIT_PASSWORD, HIT_BREADCRUMB, HIT_MODAL, HIT_SETTING, HIT_LINE };

// Commands behind menu entries and dialog buttons.
enum Cmd {
    CMD_CANCEL = 1,
    CMD_RENAME,
    CMD_DELETE,
    CMD_DELETE_CONFIRMED,
    CMD_COPY,
    CMD_CUT,
    CMD_PASTE,
    CMD_NEWFOLDER,
    CMD_NEWFILE,
    CMD_EDIT_LINE,
    CMD_INSERT_LINE,
    CMD_DELETE_LINE,
    CMD_DUP_LINE,
    CMD_UNDO,
    CMD_SAVE,
    CMD_CLOSE_DOC,
    CMD_SAVE_AND_CLOSE,
    CMD_DISCARD_AND_CLOSE,
    CMD_ENABLE_FILEOPS,
    CMD_JOB_CANCEL,
    CMD_INSTALL_PLUGIN,
};

// What a software keyboard result is for.
enum TextPurpose { TXT_RENAME = 1, TXT_NEWFOLDER, TXT_NEWFILE, TXT_EDIT_LINE, TXT_INSERT_LINE };

constexpr int kFileRows = 9;
constexpr int kFileRowH = 44;
constexpr int kDocRows  = 11;
constexpr int kDocRowH  = 30;

} // namespace ui
