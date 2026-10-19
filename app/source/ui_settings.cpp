// Settings page: background server, file changes, hidden files, language.

#include "platform.h"
#include "ui.h"
#include "ui_style.h"

#include <algorithm>

using namespace ui;

namespace {

enum Row { ROW_BACKGROUND, ROW_FILEOPS, ROW_HIDDEN, ROW_LANGUAGE, kRows };

// Each language in its own name and script.
const char *const kLangNames[(int) Lang::Count] = {"日本語",     "English",   "Français", "Deutsch",
                                                    "Italiano",   "Español",   "简体中文",  "한국어",
                                                    "Nederlands", "Português", "Русский",  "繁體中文"};

} // namespace

void Ui::changeSetting(int row, int mode, std::vector<UiAction> &actions) {
    // mode: 0 = activate (toggle / next), -1 = off / previous, +1 = on / next
    switch (row) {
        case ROW_BACKGROUND: {
            bool on   = bg_.on();
            bool want = mode == 0 ? !on : mode > 0;
            if (want && !on) {
                if (bg_.installed) {
                    actions.push_back(UiAction::BackgroundOn);
                } else {
                    // Installing a plugin is a bigger step: say what happens.
                    openConfirm(tr(S_BG_INSTALL_TITLE), tr(S_BG_INSTALL_MSG),
                                {{CMD_CANCEL, tr(S_CANCEL)}, {CMD_INSTALL_PLUGIN, tr(S_INSTALL)}});
                }
            } else if (!want && on) {
                actions.push_back(UiAction::BackgroundOff);
            }
            break;
        }
        case ROW_FILEOPS: {
            bool want = mode == 0 ? !settings_.fileActions : mode > 0;
            if (want && !settings_.fileActions) {
                openConfirm(tr(S_FILEOPS_WARN_TITLE), tr(S_FILEOPS_WARN),
                            {{CMD_CANCEL, tr(S_CANCEL)}, {CMD_ENABLE_FILEOPS, tr(S_ENABLE)}});
            } else if (!want && settings_.fileActions) {
                settings_.fileActions = false;
                actions.push_back(UiAction::SettingsChanged);
            }
            break;
        }
        case ROW_HIDDEN: {
            bool want = mode == 0 ? !settings_.showHidden : mode > 0;
            if (want != settings_.showHidden) {
                settings_.showHidden = want;
                explorer_.setShowHidden(want);
                if (explorerOpened_ && view_ == View::Browser) {
                    explorer_.reload();
                }
                actions.push_back(UiAction::SettingsChanged);
            }
            break;
        }
        case ROW_LANGUAGE: {
            int step = mode < 0 ? -1 : 1;
            // Cycle through "automatic" (-1) and the twelve languages.
            int v               = settings_.language + 1 + step;
            int count           = (int) Lang::Count + 1;
            settings_.language  = ((v % count) + count) % count - 1;
            applyLanguage();
            actions.push_back(UiAction::SettingsChanged);
            break;
        }
        default:
            break;
    }
}

void Ui::settingsPress(Button b, std::vector<UiAction> &actions) {
    switch (b) {
        case B_UP: settingsSel_ = std::max(0, settingsSel_ - 1); break;
        case B_DOWN: settingsSel_ = std::min(kRows - 1, settingsSel_ + 1); break;
        case B_A: changeSetting(settingsSel_, 0, actions); break;
        case B_LEFT: changeSetting(settingsSel_, -1, actions); break;
        case B_RIGHT: changeSetting(settingsSel_, 1, actions); break;
        default: break;
    }
}

void Ui::drawSettings(const AppState &st) {
    settingsSel_ = std::clamp(settingsSel_, 0, kRows - 1);
    const int x = 40, y = kContentTop, w = 1200, rowH = 90;
    r_.card(x, y, w, 20 + kRows * rowH);

    struct RowInfo {
        Icon icon;
        Str title, desc;
    } rows[kRows] = {
            {Icon::Clock, S_OPT_BACKGROUND, S_OPT_BACKGROUND_DESC},
            {Icon::Lock, S_OPT_FILEOPS, S_OPT_FILEOPS_DESC},
            {Icon::Folder, S_OPT_HIDDEN, S_OPT_HIDDEN_DESC},
            {Icon::Globe, S_OPT_LANGUAGE, S_OPT_LANGUAGE_DESC},
    };
    for (int i = 0; i < kRows; i++) {
        int ry   = y + 10 + i * rowH;
        bool sel = i == settingsSel_;
        if (i > 0) r_.fill(x + 24, ry, w - 48, 1, kTrack);
        if (sel) {
            drawGlow(x + 10, ry + 5, w - 20, rowH - 10, 14);
            r_.round(x + 10, ry + 5, w - 20, rowH - 10, 14, kBlueSoft);
        }
        addHit(x, ry, w, rowH, HIT_SETTING, i);
        r_.round(x + 30, ry + 15, 60, 60, 30, sel ? kWhite : kBlueSoft);
        r_.icon(rows[i].icon, x + 42, ry + 27, 36, kBlue);

        bool isSwitch = i != ROW_LANGUAGE;
        bool on       = i == ROW_BACKGROUND ? bg_.on()
                        : i == ROW_FILEOPS  ? settings_.fileActions
                                            : settings_.showHidden;
        const int cx = x + w - 40; // right edge of the control
        int controlLeft;
        if (isSwitch) {
            controlLeft = cx - 76 - 18 - r_.measure(F_BODY_B, tr(on ? S_ON : S_OFF));
        } else {
            controlLeft = cx - std::clamp(r_.measure(F_BODY, tr(S_LANG_AUTO)) + 104, 280, 460);
        }

        // The background row shows what the switch currently means.
        std::string desc = tr(rows[i].desc);
        uint32_t descC   = kDim;
        if (i == ROW_BACKGROUND && bg_.on()) {
            desc  = tr(bg_.loaded ? S_BG_ACTIVE : S_BG_AFTER_REBOOT);
            descC = bg_.loaded ? kGreen : kOrange;
        }
        // Text gets the room up to the control; long descriptions use the
        // smaller font before they are shortened.
        const int textW = controlLeft - 24 - (x + 112);
        Font descFont   = r_.measure(F_SMALL, desc) > textW ? F_TINY : F_SMALL;
        r_.text(F_MEDIUM_B, tr(rows[i].title), x + 112, ry + 13, kText, A_LEFT, textW);
        r_.text(descFont, desc, x + 112, ry + 51, descC, A_LEFT, textW);

        if (isSwitch) {
            // Wii U style switch with the state written next to it.
            const int tw = 76, th = 40, ty = ry + 25;
            r_.round(cx - tw, ty, tw, th, th / 2, on ? kGreen : 0xC9D0D8FF);
            r_.shadow(on ? cx - th + 4 : cx - tw + 4, ty + 6, th - 8, th - 8, (th - 8) / 2, 3, 60);
            r_.round(on ? cx - th + 4 : cx - tw + 4, ty + 4, th - 8, th - 8, (th - 8) / 2, kWhite);
            r_.text(F_BODY_B, tr(on ? S_ON : S_OFF), cx - tw - 18, ty + (th - r_.lineHeight(F_BODY_B)) / 2,
                    on ? kGreen : kDim, A_RIGHT);
        } else {
            // Language: < value >, wide enough for "automatic" in any language.
            int lang        = settings_.language;
            const int pillH = 44, py = ry + 23, chev = 22;
            const int pillW = cx - controlLeft;
            r_.round(cx - pillW, py, pillW, pillH, pillH / 2, sel ? kWhite : kTrack);
            r_.icon(Icon::ChevronLeft, cx - pillW + 16, py + (pillH - chev) / 2, chev, kBlue);
            r_.icon(Icon::ChevronRight, cx - 16 - chev, py + (pillH - chev) / 2, chev, kBlue);
            int ty = py + (pillH - r_.lineHeight(F_BODY)) / 2;
            if (lang < 0) {
                r_.text(F_BODY, tr(S_LANG_AUTO), cx - pillW / 2, ty, kText, A_CENTER);
            } else {
                r_.textInFont(font_type_for((Lang) lang), kLangNames[lang], cx - pillW / 2, ty, kText, A_CENTER);
            }
        }
    }

    // About / where the server settings live.
    const int iy = y + 20 + kRows * rowH + 14, ih = 94;
    r_.card(x, iy, w, ih);
    r_.logo(x + 30, iy + 19, 56);
    r_.text(F_MEDIUM_B, "SFTPiiU 1.3.0", x + 112, iy + 12, kText);
    r_.text(F_SMALL, "made by sanyahax (github @sanyahax) - ajk.dev", x + w - 30, iy + 18, kDim, A_RIGHT);
    r_.text(F_SMALL, tr(st.fromPlugin ? S_SETTINGS_SERVER_PLUGIN : S_SETTINGS_SERVER_APP), x + 112, iy + 54, kDim,
            A_LEFT, w - 150);
}
