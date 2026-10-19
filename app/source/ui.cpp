#include "ui.h"
#include "format.h"
#include "platform.h"
#include "ui_style.h"

#include <SDL_image.h>
#include <SDL_syswm.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sys/statvfs.h>

using namespace ui;

namespace {

constexpr size_t kGraphSamples = 120; // 60 s at 2 samples per second
constexpr uint64_t kSampleMs   = 500;

std::string event_text(const Monitor::Event &e) {
    switch (e.kind) {
        case EventKind::ServerStarted:
            return trf(S_E_STARTED, e.a);
        case EventKind::Login:
            return trf(e.b == "password" ? S_E_LOGIN_PW : S_E_LOGIN_KEY, e.a);
        case EventKind::Disconnected:
            return trf(S_E_DISCONNECTED, e.a);
        case EventKind::WrongPassword:
            return trf(S_E_WRONG_PW, e.a);
        case EventKind::LockedOut:
            return tr(S_E_LOCKED);
        case EventKind::TooManyConnections:
            return trf(S_E_TOO_MANY, e.a);
        case EventKind::NewPassword:
            return tr(S_E_NEW_PW);
    }
    return {};
}

} // namespace

namespace ui {

Icon drive_icon(const std::string &name) {
    if (name == "sd") return Icon::SdCard;
    if (name == "storage_mlc") return Icon::Chip;
    if (name == "storage_usb") return Icon::Drive;
    if (name.rfind("storage_odd", 0) == 0) return Icon::Disc;
    return Icon::Lock; // SLC / vWii system areas
}

std::string drive_name(const std::string &name) {
    if (name == "sd") return tr(S_DRIVE_SD);
    if (name == "storage_mlc") return tr(S_DRIVE_MLC);
    if (name == "storage_usb") return tr(S_DRIVE_USB);
    if (name == "storage_slc") return tr(S_DRIVE_SLC);
    if (name == "slccmpt01") return tr(S_DRIVE_SLCCMPT);
    if (name.rfind("storage_odd_", 0) == 0) return std::string(tr(S_DRIVE_DISC)) + " (" + name.substr(12) + ")";
    return name;
}

} // namespace ui

// ---------------------------------------------------------------- lifecycle

bool Ui::init() {
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        LOG("SDL_Init: %s", SDL_GetError());
        return false;
    }
    // The software keyboard reports OK/cancel through SysWM events.
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    window_ = SDL_CreateWindow("SFTPiiU", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, kW, kH, 0);
    if (!window_) {
        return false;
    }
    sdl_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_) {
        return false;
    }
    SDL_RenderSetLogicalSize(sdl_, kW, kH);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
    if (TTF_Init() != 0 || !r_.init(sdl_)) {
        return false;
    }
    applyLanguage();
    return fontType_ >= 0;
}

void Ui::applyLanguage() {
    setLanguage(settings_.language < 0 ? console_language() : (Lang) settings_.language);
}

void Ui::setLanguage(Lang lang) {
    int type = font_type_for(lang);
    if (type != fontType_ && !r_.loadFonts(type)) {
        // The language's font is missing: use the standard font and English.
        lang = Lang::EN;
        type = font_type_for(Lang::EN);
        if (type != fontType_ && !r_.loadFonts(type)) {
            set_language(lang);
            return;
        }
    }
    fontType_ = type;
    set_language(lang);
}

void Ui::shutdown() {
    freeImage();
    r_.shutdown();
    if (TTF_WasInit()) {
        TTF_Quit();
    }
    IMG_Quit();
    for (auto *p : pads_) {
        SDL_GameControllerClose(p);
    }
    pads_.clear();
    if (sdl_) SDL_DestroyRenderer(sdl_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

// ---------------------------------------------------------------- input

void Ui::pollEvents(std::vector<UiAction> &actions, bool &quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_SYSWMEVENT:
                if (e.syswm.msg && e.syswm.msg->subsystem == SDL_SYSWM_WIIU && swkbdActive_) {
                    unsigned ev = e.syswm.msg->msg.wiiu.event;
                    if (ev == SDL_WIIU_SYSWM_SWKBD_OK_START_EVENT) {
                        swkbdText_.clear();
                    } else if (ev == SDL_WIIU_SYSWM_SWKBD_OK_FINISH_EVENT) {
                        SDL_StopTextInput();
                        swkbdActive_ = false;
                        onText(swkbdPurpose_, swkbdText_);
                    } else if (ev == SDL_WIIU_SYSWM_SWKBD_CANCEL_EVENT) {
                        SDL_StopTextInput();
                        swkbdActive_ = false;
                    }
                }
                break;
            case SDL_TEXTINPUT:
                if (swkbdActive_) {
                    swkbdText_ += e.text.text;
                }
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (SDL_GameController *c = SDL_GameControllerOpen(e.cdevice.which)) {
                    pads_.push_back(c);
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                bool down = e.type == SDL_CONTROLLERBUTTONDOWN;
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_A: setHeld(B_A, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_B: setHeld(B_B, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_X: setHeld(B_X, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_Y: setHeld(B_Y, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: setHeld(B_L, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: setHeld(B_R, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_START: setHeld(B_PLUS, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: setHeld(B_UP, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: setHeld(B_DOWN, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: setHeld(B_LEFT, down, actions); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: setHeld(B_RIGHT, down, actions); break;
                    default: break;
                }
                break;
            }
            case SDL_CONTROLLERAXISMOTION: {
                int v = e.caxis.value;
                switch (e.caxis.axis) {
                    case SDL_CONTROLLER_AXIS_LEFTY: {
                        int dir = v < -20000 ? -1 : (v > 20000 ? 1 : 0);
                        if (dir != stickY_) {
                            if (stickY_) setHeld(stickY_ < 0 ? B_UP : B_DOWN, false, actions);
                            stickY_ = dir;
                            if (dir) setHeld(dir < 0 ? B_UP : B_DOWN, true, actions);
                        }
                        break;
                    }
                    case SDL_CONTROLLER_AXIS_LEFTX: {
                        int dir = v < -20000 ? -1 : (v > 20000 ? 1 : 0);
                        if (dir != stickX_) {
                            if (stickX_) setHeld(stickX_ < 0 ? B_LEFT : B_RIGHT, false, actions);
                            stickX_ = dir;
                            if (dir) setHeld(dir < 0 ? B_LEFT : B_RIGHT, true, actions);
                        }
                        break;
                    }
                    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                        if ((v > 16000) != trigL_) {
                            trigL_ = v > 16000;
                            setHeld(B_ZL, trigL_, actions);
                        }
                        break;
                    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                        if ((v > 16000) != trigR_) {
                            trigR_ = v > 16000;
                            setHeld(B_ZR, trigR_, actions);
                        }
                        break;
                    default:
                        break;
                }
                break;
            }
            case SDL_FINGERDOWN:
                if (!swkbdActive_) {
                    touch((int) (e.tfinger.x * kW), (int) (e.tfinger.y * kH), actions);
                }
                break;
            default:
                break;
        }
    }

    // Auto-repeat for held directions, as in the Wii U menus.
    uint64_t now = plat::now_ms();
    for (Button b : {B_UP, B_DOWN, B_LEFT, B_RIGHT, B_ZL, B_ZR}) {
        if (held_[b] && now >= nextRepeat_[b]) {
            press(b, actions);
            nextRepeat_[b] = now + 70;
        }
    }
}

void Ui::setHeld(Button b, bool down, std::vector<UiAction> &actions) {
    if (down && !held_[b]) {
        press(b, actions);
        nextRepeat_[b] = plat::now_ms() + 350;
    }
    held_[b] = down;
}

void Ui::press(Button b, std::vector<UiAction> &actions) {
    if (loadingNow_ || swkbdActive_) {
        return;
    }
    if (modal_.kind != ModalKind::None) {
        modalPress(b, actions);
        return;
    }
    if (b == B_L || b == B_R) {
        page_         = (Page) ((page_ + (b == B_R ? 1 : kPageCount - 1)) % kPageCount);
        confirmUntil_ = 0;
        return;
    }
    if (b == B_PLUS) {
        actions.push_back(UiAction::Quit);
        return;
    }
    uint64_t now = plat::now_ms();
    switch (page_) {
        case kAccess:
            if (b == B_X) {
                showPassword_ = !showPassword_;
            } else if (b == B_Y && st_ && st_->canNewPassword && st_->running) {
                if (now < confirmUntil_) {
                    confirmUntil_ = 0;
                    showPassword_ = true;
                    actions.push_back(UiAction::NewPassword);
                } else {
                    confirmUntil_ = now + 3000;
                }
            }
            break;
        case kDrives: {
            int n = st_ && st_->vfs ? (int) st_->vfs->mounts().size() : 0;
            if (n == 0) break;
            if (b == B_LEFT || b == B_RIGHT) driveSel_ ^= 1;
            if (b == B_UP) driveSel_ -= 2;
            if (b == B_DOWN) driveSel_ += 2;
            driveSel_ = std::clamp(driveSel_, 0, n - 1);
            if (b == B_A) {
                page_ = kFiles;
                closeDocument();
                pendingOpen_ = "/" + st_->vfs->mounts()[driveSel_].name;
                pendingUp_   = false;
            }
            break;
        }
        case kFiles:
            if (view_ == View::Browser) {
                filesPress(b, actions);
            } else {
                viewerPress(b, actions);
            }
            break;
        case kSettings:
            settingsPress(b, actions);
            break;
        default:
            break;
    }
}

void Ui::touch(int x, int y, std::vector<UiAction> &actions) {
    bool modal = modal_.kind != ModalKind::None;
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) { // topmost first
        const Hit &h = *it;
        if (x < h.rect.x || y < h.rect.y || x >= h.rect.x + h.rect.w || y >= h.rect.y + h.rect.h) {
            continue;
        }
        if (modal && h.kind != HIT_MODAL) {
            return; // a dialog blocks everything behind it
        }
        switch (h.kind) {
            case HIT_TAB:
                page_         = (Page) h.value;
                confirmUntil_ = 0;
                break;
            case HIT_BUTTON:
                press((Button) h.value, actions);
                break;
            case HIT_PASSWORD:
                showPassword_ = !showPassword_;
                break;
            case HIT_DRIVE:
                if (driveSel_ == h.value) {
                    press(B_A, actions);
                } else {
                    driveSel_ = h.value;
                }
                break;
            case HIT_FILE:
                if (explorer_.selected == h.value) {
                    press(B_A, actions);
                } else {
                    explorer_.selected = h.value;
                }
                break;
            case HIT_LINE:
                if (docSel_ == h.value) {
                    press(B_A, actions);
                } else {
                    docSel_ = h.value;
                }
                break;
            case HIT_BREADCRUMB:
                press(B_B, actions);
                break;
            case HIT_SETTING:
                settingsSel_ = h.value;
                changeSetting(h.value, 0, actions);
                break;
            case HIT_MODAL:
                if (h.value < 0) {
                    // Tapping outside closes a menu; dialogs need an answer.
                    if (modal_.kind == ModalKind::Menu) runCommand(CMD_CANCEL, actions);
                } else if (h.value < (int) modal_.items.size() && modal_.items[h.value].enabled) {
                    modal_.sel = h.value;
                    modalPress(B_A, actions);
                }
                break;
        }
        return;
    }
}

void Ui::addHit(int x, int y, int w, int h, int kind, int value) {
    hits_.push_back({SDL_Rect{x, y, w, h}, kind, value});
}

// ---------------------------------------------------------------- dialogs

void Ui::openMenu(const std::string &title, std::vector<MenuItem> items) {
    modal_       = Modal();
    modal_.kind  = ModalKind::Menu;
    modal_.title = title;
    modal_.items = std::move(items);
    for (size_t i = 0; i < modal_.items.size(); i++) {
        if (modal_.items[i].enabled) {
            modal_.sel = (int) i;
            break;
        }
    }
}

void Ui::openConfirm(const std::string &title, const std::string &message, std::vector<MenuItem> buttons) {
    modal_         = Modal();
    modal_.kind    = ModalKind::Confirm;
    modal_.title   = title;
    modal_.message = message;
    modal_.items   = std::move(buttons);
    modal_.sel     = 0; // the first button (Cancel) is the safe default
}

void Ui::modalPress(Button b, std::vector<UiAction> &actions) {
    if (modal_.kind == ModalKind::Progress) {
        if (b == B_B) {
            job_.cancel();
        }
        return;
    }
    int n = (int) modal_.items.size();
    if (b == B_B) {
        runCommand(CMD_CANCEL, actions);
        return;
    }
    bool vertical = modal_.kind == ModalKind::Menu;
    int dir       = 0;
    if ((vertical && b == B_UP) || (!vertical && b == B_LEFT)) dir = -1;
    if ((vertical && b == B_DOWN) || (!vertical && b == B_RIGHT)) dir = 1;
    if (dir && n > 0) {
        for (int i = 1; i <= n; i++) {
            int cand = ((modal_.sel + dir * i) % n + n) % n;
            if (modal_.items[cand].enabled) {
                modal_.sel = cand;
                break;
            }
        }
        return;
    }
    if (b == B_A && modal_.sel >= 0 && modal_.sel < n && modal_.items[modal_.sel].enabled) {
        runCommand(modal_.items[modal_.sel].cmd, actions);
    }
}

void Ui::toast(const std::string &text, bool error) {
    toast_      = text;
    toastError_ = error;
    toastUntil_ = plat::now_ms() + 2800;
}

void Ui::askText(int purpose, const std::string &initial, const std::string &hint) {
    swkbdPurpose_ = purpose;
    swkbdText_.clear();
    swkbdActive_ = true;
    SDL_WiiUSetSWKBDInitialText(initial.c_str());
    SDL_WiiUSetSWKBDHintText(hint.c_str());
    SDL_WiiUSetSWKBDShowWordSuggestions(SDL_FALSE);
    SDL_StartTextInput();
}

void Ui::drawModal() {
    if (modal_.kind == ModalKind::None) {
        return;
    }
    r_.fill(0, 0, kW, kH, 0x1E2A3870);
    addHit(0, 0, kW, kH, HIT_MODAL, -2); // swallow taps outside the dialog
    float t   = (float) (plat::now_ms() % 100000) / 1000.0f;
    int pulse = 150 + (int) (70 * std::sin(t * 3.5f));

    if (modal_.kind == ModalKind::Menu) {
        const int w = 560, rowH = 56;
        int h       = 84 + (int) modal_.items.size() * rowH + 16;
        int x = (kW - w) / 2, y = (kH - h) / 2;
        r_.card(x, y, w, h, 22);
        r_.text(F_MEDIUM_B, modal_.title, x + 28, y + 24, kText, A_LEFT, w - 56);
        int ry = y + 84;
        for (size_t i = 0; i < modal_.items.size(); i++) {
            const MenuItem &it = modal_.items[i];
            bool sel           = (int) i == modal_.sel;
            if (sel) {
                r_.round(x + 14, ry - 5, w - 28, rowH - 2, 14, with_alpha(kGlow, pulse));
                r_.round(x + 18, ry - 1, w - 36, rowH - 10, 12, kBlueSoft);
            }
            uint32_t color = !it.enabled ? 0xC3C9D0FF : (it.danger ? kRed : kText);
            r_.text(F_BODY, it.label, x + 36, ry + (rowH - 10 - r_.lineHeight(F_BODY)) / 2, color, A_LEFT, w - 72);
            addHit(x, ry - 5, w, rowH, HIT_MODAL, (int) i);
            ry += rowH;
        }
        return;
    }

    if (modal_.kind == ModalKind::Confirm) {
        const int w = 680;
        int msgLines = modal_.message.empty() ? 0 : std::clamp(r_.wrappedLines(F_BODY, modal_.message, w - 64), 1, 5);
        int h        = 96 + msgLines * 30 + 104;
        int x = (kW - w) / 2, y = (kH - h) / 2;
        r_.card(x, y, w, h, 22);
        r_.text(F_MEDIUM_B, modal_.title, x + 32, y + 28, kText, A_LEFT, w - 64);
        if (msgLines) {
            r_.textWrapped(F_BODY, modal_.message, x + 32, y + 80, w - 64, msgLines, 30, kDim);
        }
        // Buttons right-aligned, like the Wii U system dialogs.
        int bx = x + w - 32;
        for (int i = (int) modal_.items.size() - 1; i >= 0; i--) {
            const MenuItem &it = modal_.items[i];
            int bw             = std::max(150, r_.measure(F_BODY_B, it.label) + 56);
            bx -= bw;
            int by   = y + h - 76;
            bool sel = i == modal_.sel;
            if (sel) r_.round(bx - 5, by - 5, bw + 10, 62, 31, with_alpha(kGlow, pulse));
            uint32_t fillC = sel ? (it.danger ? kRed : kBlue) : kTrack;
            r_.round(bx, by, bw, 52, 26, fillC);
            uint32_t textC = sel ? kWhite : (it.danger ? kRed : kText);
            r_.text(F_BODY_B, it.label, bx + bw / 2, by + (52 - r_.lineHeight(F_BODY_B)) / 2, textC, A_CENTER);
            addHit(bx, by, bw, 52, HIT_MODAL, i);
            bx -= 16;
        }
        return;
    }

    // Progress of a copy/move/delete job.
    const int w = 680, h = 236;
    int x = (kW - w) / 2, y = (kH - h) / 2;
    r_.card(x, y, w, h, 22);
    r_.text(F_MEDIUM_B, modal_.title, x + 32, y + 26, kText);
    r_.text(F_BODY, job_.current(), x + 32, y + 78, kDim, A_LEFT, w - 64, true);
    double frac = 0;
    std::string amount;
    if (job_.kind() != fileops::Job::Kind::Delete && job_.bytesTotal() > 0) {
        frac   = (double) job_.bytesDone() / (double) job_.bytesTotal();
        amount = fmt_bytes(job_.bytesDone()) + " / " + fmt_bytes(job_.bytesTotal());
    } else if (job_.itemsTotal() > 0) {
        frac   = (double) job_.itemsDone() / (double) job_.itemsTotal();
        amount = trf(S_PROGRESS_ITEMS, std::to_string(job_.itemsDone()), std::to_string(job_.itemsTotal()));
    }
    frac = std::clamp(frac, 0.0, 1.0);
    r_.round(x + 32, y + 124, w - 64, 14, 7, kTrack);
    r_.round(x + 32, y + 124, std::max(14, (int) ((w - 64) * frac)), 14, 7, kBlue);
    r_.text(F_SMALL, amount, x + 32, y + 150, kDim);
    int bw = r_.button("B", x + w - 32 - 40 - r_.measure(F_SMALL, tr(S_CANCEL)), y + h - 52, 30);
    r_.text(F_SMALL, tr(S_CANCEL), x + w - 32 - r_.measure(F_SMALL, tr(S_CANCEL)), y + h - 52 + (30 - r_.lineHeight(F_SMALL)) / 2, kText);
    addHit(x + w - 220, y + h - 64, 220, 56, HIT_BUTTON, B_B);
    (void) bw;
}

void Ui::drawToast() {
    uint64_t now = plat::now_ms();
    if (toast_.empty() || now >= toastUntil_) {
        return;
    }
    int w = std::min(1100, r_.measure(F_BODY, toast_) + 72);
    int x = (kW - w) / 2, y = kFooterY - 74;
    r_.card(x, y, w, 56, 28, toastError_ ? kRed : 0x3B444FFF);
    r_.text(F_BODY, toast_, kW / 2, y + (56 - r_.lineHeight(F_BODY)) / 2, kWhite, A_CENTER, w - 48);
}

// ---------------------------------------------------------------- data

void Ui::updateStats(const AppState &st) {
    uint64_t now = plat::now_ms();
    std::map<uint32_t, Speed> next;
    for (const auto &s : st.snap.sessions) {
        for (const auto &t : s.transfers) {
            auto it = speeds_.find(t.id);
            if (it == speeds_.end()) {
                next[t.id] = Speed{t.bytes, now, 0};
                continue;
            }
            Speed sp = it->second;
            if (now - sp.ms >= kSampleMs) {
                double inst = (double) (t.bytes - sp.bytes) * 1000.0 / (double) (now - sp.ms);
                sp.perSec   = sp.perSec == 0 ? inst : 0.5 * sp.perSec + 0.5 * inst;
                sp.bytes    = t.bytes;
                sp.ms       = now;
            }
            next[t.id] = sp;
        }
    }
    speeds_.swap(next);

    if (lastSampleMs_ == 0) {
        lastSampleMs_ = now;
        lastDown_     = st.snap.totalDown;
        lastUp_       = st.snap.totalUp;
        return;
    }
    if (now - lastSampleMs_ >= kSampleMs) {
        double dt   = (double) (now - lastSampleMs_) / 1000.0;
        double down = (double) (st.snap.totalDown - lastDown_) / dt;
        double up   = (double) (st.snap.totalUp - lastUp_) / dt;
        speedDown_  = 0.5 * speedDown_ + 0.5 * down;
        speedUp_    = 0.5 * speedUp_ + 0.5 * up;
        histDown_.push_back((float) down);
        histUp_.push_back((float) up);
        if (histDown_.size() > kGraphSamples) {
            histDown_.erase(histDown_.begin());
            histUp_.erase(histUp_.begin());
        }
        lastDown_     = st.snap.totalDown;
        lastUp_       = st.snap.totalUp;
        lastSampleMs_ = now;
    }
}

void Ui::refreshDrives(const AppState &st, bool force) {
    if (!st.vfs) {
        return;
    }
    uint64_t now       = plat::now_ms();
    const auto &mounts = st.vfs->mounts();
    if (!force && drives_.size() == mounts.size() && now - drivesAt_ < 15000) {
        return;
    }
    drives_.assign(mounts.size(), DriveInfo{});
    for (size_t i = 0; i < mounts.size(); i++) {
        struct statvfs sv;
        if (statvfs((mounts[i].root + "/").c_str(), &sv) == 0 && sv.f_blocks > 0) {
            uint64_t unit    = sv.f_frsize ? sv.f_frsize : sv.f_bsize;
            drives_[i].known = true;
            drives_[i].total = (uint64_t) sv.f_blocks * unit;
            drives_[i].free  = (uint64_t) sv.f_bavail * unit;
        }
    }
    drivesAt_ = now;
}

// ---------------------------------------------------------------- frame

void Ui::splash(float progress) {
    drawBackground();
    r_.logo(kW / 2 - 80, 170, 160);
    r_.text(F_LARGE_B, "SFTPiiU", kW / 2, 360, kText, A_CENTER);
    r_.text(F_BODY, tr(S_STARTING), kW / 2, 414, kDim, A_CENTER);
    r_.round(kW / 2 - 200, 470, 400, 10, 5, kTrack);
    r_.round(kW / 2 - 200, 470, std::max(10, (int) (400 * std::clamp(progress, 0.0f, 1.0f))), 10, 5, kBlue);
    SDL_RenderPresent(sdl_);
    r_.endFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            if (SDL_GameController *c = SDL_GameControllerOpen(e.cdevice.which)) {
                pads_.push_back(c);
            }
        }
    }
}

bool Ui::frame(const AppState &st, std::vector<UiAction> &actions) {
    st_       = &st;
    bg_       = st.bg;
    bool quit = false;
    explorer_.setVfs(st.vfs);
    explorer_.setShowHidden(settings_.showHidden);
    if (!explorerOpened_ && st.vfs) {
        explorer_.open("/");
        explorerOpened_ = true;
    }
    pollEvents(actions, quit);
    if (quit) {
        st_ = nullptr;
        return false;
    }
    pollJob();
    if (page_ == kDrives) {
        refreshDrives(st, false);
    }
    updateStats(st);

    hits_.clear();
    loadingNow_ = !pendingOpen_.empty();
    drawBackground();
    switch (page_) {
        case kDashboard: drawDashboard(st); break;
        case kAccess: drawAccess(st); break;
        case kDrives: drawDrives(st); break;
        case kFiles: drawFiles(st); break;
        case kSettings: drawSettings(st); break;
        default: break;
    }
    drawHeader(st);
    drawTabs();
    drawFooter(st);
    if (!st.error.empty()) {
        r_.card(40, 596, 1200, 52, 14, kRed);
        r_.text(F_BODY, st.error, 64, 596 + (52 - r_.lineHeight(F_BODY)) / 2, kWhite, A_LEFT, 1152);
    }
    drawToast();
    drawModal();
    SDL_RenderPresent(sdl_);
    r_.endFrame();

    // Directory loads happen after a frame that shows "Loading…".
    if (loadingNow_) {
        if (pendingUp_) {
            explorer_.up();
        } else {
            explorer_.open(pendingOpen_);
        }
        pendingOpen_.clear();
        pendingUp_  = false;
        loadingNow_ = false;
    }
    st_ = nullptr;
    return true;
}

// ---------------------------------------------------------------- chrome

void Ui::drawBackground() {
    r_.gradient(0, 0, kW, kH, kBgTop, kBgBottom);
    // Faint diagonal stripes, like the Wii U Menu backdrop.
    SDL_Vertex v[4];
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int x = -kH; x < kW; x += 56) {
        SDL_Color c{255, 255, 255, 70};
        v[0] = {{(float) x, (float) kH}, c, {0, 0}};
        v[1] = {{(float) (x + 22), (float) kH}, c, {0, 0}};
        v[2] = {{(float) (x + 22 + kH), 0}, c, {0, 0}};
        v[3] = {{(float) (x + kH), 0}, c, {0, 0}};
        SDL_RenderGeometry(sdl_, nullptr, v, 4, idx, 6);
    }
}

void Ui::drawHeader(const AppState &st) {
    r_.shadow(0, 0, kW, 84, 0, 8, 50);
    r_.fill(0, 0, kW, 84, kWhite);
    r_.logo(36, 18, 48);
    r_.text(F_LARGE_B, "SFTPiiU", 98, 20, kText);

    const std::string status = tr(st.running ? S_SERVER_RUNNING : S_SERVER_STOPPED);
    int sw                   = r_.measure(F_BODY, status) + 64;
    int sx                   = kW - 40 - sw;
    r_.round(sx, 22, sw, 40, 20, st.running ? kGreenSoft : kRedSoft);
    r_.round(sx + 18, 36, 12, 12, 6, st.running ? kGreen : kRed);
    r_.text(F_BODY, status, sx + 42, 22 + (40 - r_.lineHeight(F_BODY)) / 2, st.running ? kGreen : kRed);

    std::string addr = st.ip.empty() ? tr(S_NO_NETWORK) : "sftp://" + st.ip + ":" + std::to_string(st.port);
    r_.text(F_BODY, addr, sx - 24, 22 + (40 - r_.lineHeight(F_BODY)) / 2, kDim, A_RIGHT, sx - 300);
}

void Ui::drawTabs() {
    static const Str labels[kPageCount] = {S_TAB_DASHBOARD, S_TAB_ACCESS, S_TAB_DRIVES, S_TAB_FILES, S_SETTINGS};
    static const Icon icons[kPageCount] = {Icon::Gauge, Icon::Key, Icon::Drive, Icon::Folder, Icon::Gear};
    const int h = 52, y = 100, gap = 10;
    // Shrink the labels if a language needs more room than the bar has.
    Font f = F_BODY;
    int widths[kPageCount], total = 0;
    for (int pass = 0; pass < 2; pass++) {
        total = 0;
        for (int i = 0; i < kPageCount; i++) {
            widths[i] = 18 + 28 + 8 + r_.measure(f, tr(labels[i])) + 22;
            total += widths[i];
        }
        total += gap * (kPageCount - 1);
        if (total <= kW - 180 || f == F_SMALL) break;
        f = F_SMALL;
    }
    int x = (kW - total) / 2;
    r_.button("L", x - 66, y + 11, 30);
    addHit(x - 72, y, 60, h, HIT_BUTTON, B_L);
    for (int i = 0; i < kPageCount; i++) {
        bool sel = i == page_;
        r_.shadow(x, y + 3, widths[i], h, h / 2, 6, sel ? 60 : 30);
        r_.round(x, y, widths[i], h, h / 2, sel ? kBlue : kWhite);
        r_.icon(icons[i], x + 16, y + 12, 28, sel ? kWhite : kBlue);
        r_.text(f, tr(labels[i]), x + 54, y + (h - r_.lineHeight(f)) / 2, sel ? kWhite : kText);
        addHit(x, y, widths[i], h, HIT_TAB, i);
        x += widths[i] + gap;
    }
    r_.button("R", x + 20, y + 11, 30);
    addHit(x + 12, y, 60, h, HIT_BUTTON, B_R);
}

void Ui::drawFooter(const AppState &st) {
    r_.fill(0, kFooterY, kW, kH - kFooterY, kFooter);
    r_.fill(0, kFooterY, kW, 1, kLine);
    const int gy = kFooterY + 12, ty = kFooterY + (kH - kFooterY - r_.lineHeight(F_SMALL)) / 2;

    if (plat::now_ms() < confirmUntil_) {
        r_.text(F_SMALL, tr(S_HINT_CONFIRM_PW), kW / 2, ty, kOrange, A_CENTER);
        return;
    }

    struct HintItem {
        const char *glyph;
        std::string text;
        Button button;
    };
    std::vector<HintItem> left;
    if (modal_.kind == ModalKind::Menu || modal_.kind == ModalKind::Confirm) {
        left.push_back({"A", tr(S_HINT_SELECT), B_A});
        left.push_back({"B", tr(S_CANCEL), B_B});
    } else if (modal_.kind == ModalKind::None && !swkbdActive_) {
        switch (page_) {
            case kAccess:
                left.push_back({"X", tr(showPassword_ ? S_HINT_HIDE_PW : S_HINT_SHOW_PW), B_X});
                if (st.canNewPassword) left.push_back({"Y", tr(S_HINT_NEW_PW), B_Y});
                break;
            case kDrives:
                left.push_back({"A", tr(S_HINT_OPEN), B_A});
                break;
            case kFiles:
                if (view_ == View::Browser) {
                    left.push_back({"A", tr(S_HINT_OPEN), B_A});
                    if (!explorer_.atRoot()) left.push_back({"B", tr(S_HINT_BACK), B_B});
                    if (!explorer_.atRoot()) left.push_back({"X", tr(S_HINT_MENU), B_X});
                } else if (view_ == View::Text) {
                    if (docEditable()) {
                        left.push_back({"A", tr(S_HINT_EDIT), B_A});
                        left.push_back({"Y", tr(S_HINT_NEW_LINE), B_Y});
                    }
                    left.push_back({"X", tr(S_HINT_MENU), B_X});
                    left.push_back({"B", tr(S_CLOSE), B_B});
                } else if (view_ == View::Image) {
                    left.push_back({"◀▶", tr(S_HINT_PREV_NEXT), B_RIGHT});
                    left.push_back({"B", tr(S_CLOSE), B_B});
                } else {
                    left.push_back({"B", tr(S_CLOSE), B_B});
                }
                break;
            case kSettings:
                left.push_back({"A", tr(S_HINT_CHANGE), B_A});
                break;
            default:
                break;
        }
    }
    int x = 40;
    for (const auto &h : left) {
        int start = x;
        x += r_.button(h.glyph, x, gy, 30) + 10;
        x += r_.text(F_SMALL, h.text, x, ty, kText) + 30;
        addHit(start, kFooterY, x - start - 16, kH - kFooterY, HIT_BUTTON, h.button);
    }

    // Global hints on the right; the page hint is dropped if space is short.
    // A dialog takes all buttons, so they are hidden while one is open.
    if (modal_.kind != ModalKind::None || swkbdActive_) {
        return;
    }
    int qx = kW - 40 - r_.measure(F_SMALL, tr(S_HINT_QUIT));
    r_.text(F_SMALL, tr(S_HINT_QUIT), qx, ty, kText);
    qx -= 40;
    r_.button("+", qx, gy, 30);
    addHit(qx, kFooterY, kW - qx, kH - kFooterY, HIT_BUTTON, B_PLUS);
    int pageW = r_.measure(F_SMALL, tr(S_HINT_PAGE)) + 36 + 10 + 45 + 6 + 45;
    if (qx - pageW > x) {
        qx -= 36 + r_.measure(F_SMALL, tr(S_HINT_PAGE));
        r_.text(F_SMALL, tr(S_HINT_PAGE), qx, ty, kText);
        qx -= 10 + 45;
        r_.button("R", qx, gy, 30);
        qx -= 6 + 45;
        r_.button("L", qx, gy, 30);
    }
}

void Ui::drawGlow(int x, int y, int w, int h, int r) {
    float t = (float) (plat::now_ms() % 100000) / 1000.0f;
    int a   = 150 + (int) (70 * std::sin(t * 3.5f));
    r_.round(x - 5, y - 5, w + 10, h + 10, r + 5, with_alpha(kGlow, a));
}

// ---------------------------------------------------------------- dashboard

void Ui::drawDashboard(const AppState &st) {
    uint64_t now = plat::now_ms();

    // Speed card with a stacked 60 s graph.
    {
        const int x = 40, y = kContentTop, w = 760, h = 236;
        r_.card(x, y, w, h);
        r_.icon(Icon::Gauge, x + 24, y + 18, 32, kBlue);
        r_.text(F_MEDIUM_B, tr(S_SPEED), x + 66, y + 18, kText);
        r_.text(F_HUGE_B, fmt_speed(speedDown_ + speedUp_), x + 24, y + 56, kText);

        // Legend: the label gets whatever the value leaves free.
        const int lx = x + 440, lw = w - 440 - 24;
        std::string down = fmt_speed(speedDown_), up = fmt_speed(speedUp_);
        r_.round(lx, y + 70, 14, 14, 7, kBlue);
        r_.text(F_SMALL, tr(S_FROM_WIIU), lx + 22, y + 66, kDim, A_LEFT, lw - 34 - r_.measure(F_BODY_B, down));
        r_.text(F_BODY_B, down, lx + lw, y + 64, kBlue, A_RIGHT);
        r_.round(lx, y + 112, 14, 14, 7, kOrange);
        r_.text(F_SMALL, tr(S_TO_WIIU), lx + 22, y + 108, kDim, A_LEFT, lw - 34 - r_.measure(F_BODY_B, up));
        r_.text(F_BODY_B, up, lx + lw, y + 106, kOrange, A_RIGHT);

        const int gx = x + 24, gy = y + 148, gw = w - 48, gh = 58;
        r_.round(gx, gy, gw, gh, 10, kBlueSoft);
        float peak = 1024.0f * 1024.0f;
        for (size_t i = 0; i < histDown_.size(); i++) {
            peak = std::max(peak, histDown_[i] + histUp_[i]);
        }
        const int bar = (gw - 16) / (int) kGraphSamples;
        int bx        = gx + 8 + (gw - 16) - bar * (int) histDown_.size();
        for (size_t i = 0; i < histDown_.size(); i++) {
            int hd = (int) (histDown_[i] / peak * (gh - 10));
            int hu = (int) (histUp_[i] / peak * (gh - 10));
            int by = gy + gh - 5;
            if (hd > 0) r_.fill(bx, by - hd, std::max(bar - 1, 1), hd, kBlue);
            if (hu > 0) r_.fill(bx, by - hd - hu, std::max(bar - 1, 1), hu, kOrange);
            bx += bar;
        }
        r_.text(F_TINY, tr(S_LAST_60S), gx, gy + gh + 4, kDim);
    }

    // Status card.
    {
        const int x = 816, y = kContentTop, w = 424, h = 236;
        r_.card(x, y, w, h);
        r_.icon(Icon::User, x + 24, y + 18, 32, kBlue);
        r_.text(F_MEDIUM_B, tr(S_STATUS), x + 66, y + 18, kText);
        struct Row {
            Str label;
            std::string value;
            uint32_t color;
        } rows[4] = {
                {S_CONNECTIONS, std::to_string(st.snap.sessions.size()) + " / 4", kText},
                {S_UPTIME, st.running ? fmt_clock(now - st.serverStartMs) : "–", kText},
                {S_FROM_WIIU, fmt_bytes(st.snap.totalDown), kBlue},
                {S_TO_WIIU, fmt_bytes(st.snap.totalUp), kOrange},
        };
        int ry = y + 72;
        for (const auto &row : rows) {
            r_.text(F_BODY, tr(row.label), x + 24, ry, kDim, A_LEFT, 220);
            r_.text(F_BODY_B, row.value, x + w - 24, ry, row.color, A_RIGHT);
            ry += 38;
        }
    }

    // Transfers: running ones first, then the most recent finished ones.
    const int x = 40, y = kContentTop + 252, w = 1200, h = 236;
    r_.card(x, y, w, h);
    r_.icon(Icon::ArrowDown, x + 24, y + 18, 30, kBlue);
    r_.text(F_MEDIUM_B, tr(S_ACTIVE_TRANSFERS), x + 66, y + 18, kText);

    struct Active {
        const Monitor::Session *s;
        const Monitor::Transfer *t;
    };
    std::vector<Active> active;
    for (const auto &s : st.snap.sessions) {
        for (const auto &t : s.transfers) {
            active.push_back({&s, &t});
        }
    }
    std::sort(active.begin(), active.end(), [](const Active &a, const Active &b) { return a.t->lastMs > b.t->lastMs; });

    const int rows = 3, rowH = 56, top = y + 62;
    int row = 0;
    for (; row < rows && row < (int) active.size(); row++) {
        const auto &a  = active[row];
        int ry         = top + row * rowH;
        uint32_t color = a.t->upload ? kOrange : kBlue;
        if (row > 0) r_.fill(x + 24, ry - 4, w - 48, 1, kTrack);
        r_.icon(a.t->upload ? Icon::ArrowUp : Icon::ArrowDown, x + 24, ry + 8, 30, color);
        r_.text(F_BODY_B, basename_of(a.t->path), x + 70, ry, kText, A_LEFT, 440);
        r_.text(F_TINY, dirname_of(a.t->path) + " · " + a.s->peer, x + 70, ry + 28, kDim, A_LEFT, 440, true);

        const int bx = x + 540, bw = 360, by = ry + 18;
        r_.round(bx, by, bw, 12, 6, kTrack);
        std::string amount;
        if (!a.t->upload && a.t->size > 0) {
            double frac = std::min(1.0, (double) a.t->bytes / (double) a.t->size);
            r_.round(bx, by, std::max(12, (int) (bw * frac)), 12, 6, color);
            amount = std::to_string((int) (frac * 100)) + " % · " + fmt_bytes(a.t->bytes) + " / " + fmt_bytes(a.t->size);
        } else {
            // Uploads have no known size: an animated "busy" segment.
            int seg = 90, pos = (int) ((now / 6) % (uint64_t) (bw + seg)) - seg;
            int s0 = std::max(0, pos), s1 = std::min(bw, pos + seg);
            if (s1 - s0 > 12) r_.round(bx + s0, by, s1 - s0, 12, 6, color);
            amount = trf(S_RECEIVED, fmt_bytes(a.t->bytes));
        }
        auto sp = speeds_.find(a.t->id);
        r_.text(F_BODY_B, sp != speeds_.end() ? fmt_speed(sp->second.perSec) : "…", x + w - 24, ry, color, A_RIGHT);
        r_.text(F_TINY, amount, x + w - 24, ry + 28, kDim, A_RIGHT);
    }
    if (active.size() > (size_t) rows) {
        r_.text(F_SMALL, "+" + std::to_string(active.size() - rows), x + w - 24, y + 22, kDim, A_RIGHT);
    }

    // Idle sessions and recent transfers fill the remaining rows.
    for (size_t i = 0; row < rows && i < st.snap.sessions.size(); i++) {
        const auto &s = st.snap.sessions[i];
        if (!s.transfers.empty()) continue;
        int ry = top + row * rowH;
        if (row > 0) r_.fill(x + 24, ry - 4, w - 48, 1, kTrack);
        r_.icon(Icon::User, x + 24, ry + 8, 30, kGrey);
        r_.text(F_BODY, s.peer, x + 70, ry + 12, kText);
        std::string state = s.method.empty() ? tr(S_LOGGING_IN) : tr(S_IDLE);
        r_.text(F_SMALL, state + (s.client.empty() ? "" : " · " + s.client), x + w - 24, ry + 14, kDim, A_RIGHT, 600);
        row++;
    }
    for (size_t i = 0; row < rows && i < st.snap.history.size(); i++, row++) {
        const auto &f = st.snap.history[i];
        int ry        = top + row * rowH;
        if (row > 0) r_.fill(x + 24, ry - 4, w - 48, 1, kTrack);
        r_.icon(f.upload ? Icon::ArrowUp : Icon::ArrowDown, x + 24, ry + 8, 30, kGrey);
        r_.text(F_BODY, basename_of(f.path), x + 70, ry + 12, kDim, A_LEFT, 560);
        std::string right = trf(S_IN_TIME, fmt_bytes(f.bytes), fmt_duration(f.durationMs)) + " · " + fmt_ago(now - f.endMs);
        r_.text(F_SMALL, right, x + w - 24, ry + 14, kDim, A_RIGHT);
    }
    if (row == 0) {
        r_.text(F_MEDIUM, tr(S_NO_TRANSFERS), x + w / 2, y + 96, kDim, A_CENTER);
        std::string addr = st.ip.empty() ? tr(S_NO_NETWORK) : "sftp://" + st.ip + ":" + std::to_string(st.port);
        r_.textWrapped(F_BODY, trf(S_CONNECT_HINT, addr), x + w / 2, y + 140, w - 80, 2, 30, kDim, A_CENTER);
    }
}

// ---------------------------------------------------------------- credentials

void Ui::drawAccess(const AppState &st) {
    {
        const int x = 40, y = kContentTop, w = 700, h = 330;
        r_.card(x, y, w, h);
        r_.icon(Icon::Key, x + 24, y + 18, 32, kBlue);
        r_.text(F_MEDIUM_B, tr(S_CONNECTION_DATA), x + 66, y + 18, kText);
        const int lx = x + 28, vx = x + 290, vw = w - 290 - 28;
        int ry = y + 72;
        auto rowLine = [&](Str label, const std::string &value, uint32_t color) {
            r_.text(F_BODY, tr(label), lx, ry + 3, kDim, A_LEFT, 246);
            r_.text(F_MEDIUM, value, vx, ry, color, A_LEFT, vw);
            ry += 50;
        };
        rowLine(S_PROTOCOL, "SFTP (SSH)", kText);
        rowLine(S_ADDRESS, st.ip.empty() ? tr(S_NO_NETWORK) : st.ip, kText);
        rowLine(S_PORT, std::to_string(st.port), kText);
        rowLine(S_USERNAME, tr(S_ANY_USER), kText);

        // Password row: tap or X to reveal.
        r_.text(F_BODY, tr(S_PASSWORD), lx, ry + 3, kDim, A_LEFT, 246);
        if (!st.passwordLogin) {
            r_.text(F_MEDIUM, tr(S_PASSWORD_OFF), vx, ry, kDim, A_LEFT, vw);
        } else if (showPassword_) {
            bool confirming = plat::now_ms() < confirmUntil_;
            r_.round(vx - 12, ry - 6, r_.measure(F_MEDIUM_B, st.password) + 24, 44, 12, confirming ? 0xFDEBD3FF : kBlueSoft);
            r_.text(F_MEDIUM_B, st.password, vx, ry, confirming ? kOrange : kBlue);
        } else {
            int pw = r_.text(F_MEDIUM, "••••-••••-••••", vx, ry, kText);
            r_.button("X", vx + pw + 18, ry + 2, 30);
        }
        addHit(x, ry - 10, w, 50, HIT_PASSWORD, 0);
    }

    {
        const int x = 756, y = kContentTop, w = 484, h = 330;
        r_.card(x, y, w, h);
        r_.icon(Icon::Lock, x + 24, y + 18, 32, kBlue);
        r_.text(F_MEDIUM_B, tr(S_SECURITY), x + 66, y + 18, kText);
        int ry = y + 66;
        r_.text(F_SMALL, tr(S_HOSTKEY), x + 24, ry, kDim);
        std::string fp = st.fingerprint;
        size_t split   = fp.size() / 2;
        r_.text(F_BODY, fp.substr(0, split), x + 24, ry + 26, kText, A_LEFT, w - 48);
        r_.text(F_BODY, fp.substr(split), x + 24, ry + 54, kText, A_LEFT, w - 48);
        r_.textWrapped(F_TINY, tr(S_HOSTKEY_HINT), x + 24, ry + 86, w - 48, 2, 22, kDim);
        ry += 140;
        r_.text(F_SMALL, tr(S_KEY_LOGIN), x + 24, ry, kDim);
        r_.text(F_BODY, st.keyCount > 0 ? trf(S_KEYS_COUNT, std::to_string(st.keyCount)) : tr(S_KEYS_NONE), x + 24,
                ry + 24, kText, A_LEFT, w - 48);
        ry += 62;
        r_.text(F_SMALL, tr(S_SETTINGS), x + 24, ry, kDim);
        r_.text(F_SMALL, tr(st.fromPlugin ? S_FROM_PLUGIN : S_FROM_APP), x + 24, ry + 24, kText, A_LEFT, w - 48);
    }

    {
        const int x = 40, y = kContentTop + 346, w = 1200, h = 142;
        r_.card(x, y, w, h);
        r_.icon(Icon::User, x + 24, y + 16, 30, kBlue);
        r_.text(F_MEDIUM_B, tr(S_LOGINS), x + 66, y + 16, kText);
        uint64_t now = plat::now_ms();
        int ry       = y + 58;
        int shown    = 0;
        for (const auto &e : st.snap.events) {
            if (e.kind != EventKind::Login && e.kind != EventKind::WrongPassword && e.kind != EventKind::LockedOut) {
                continue;
            }
            r_.round(x + 28, ry + 7, 10, 10, 5, e.error ? kRed : kGreen);
            r_.text(F_SMALL, event_text(e), x + 50, ry, e.error ? kRed : kText, A_LEFT, 900);
            r_.text(F_SMALL, fmt_ago(now - e.ms), x + w - 24, ry, kDim, A_RIGHT);
            ry += 26;
            if (++shown == 3) break;
        }
        if (shown == 0) {
            r_.text(F_BODY, tr(S_NO_LOGINS), x + 28, ry + 10, kDim);
        }
    }
}

// ---------------------------------------------------------------- drives

void Ui::drawDrives(const AppState &st) {
    if (!st.vfs) {
        return;
    }
    const auto &mounts = st.vfs->mounts();
    driveSel_          = std::clamp(driveSel_, 0, std::max(0, (int) mounts.size() - 1));
    const int w = 594, h = 110;
    for (size_t i = 0; i < mounts.size() && i < 8; i++) {
        const Mount &m = mounts[i];
        int x = i % 2 == 0 ? 40 : 646, y = kContentTop + (int) (i / 2) * 122;
        bool sel = (int) i == driveSel_;
        if (sel) drawGlow(x, y, w, h, 18);
        r_.card(x, y, w, h);
        addHit(x, y, w, h, HIT_DRIVE, (int) i);

        r_.round(x + 20, y + 23, 64, 64, 32, kBlueSoft);
        r_.icon(drive_icon(m.name), x + 32, y + 35, 40, kBlue);

        std::string badge = tr(m.readOnly ? S_READ_ONLY : S_READ_WRITE);
        int bw            = r_.measure(F_TINY, badge) + 24;
        r_.round(x + w - 20 - bw, y + 16, bw, 28, 14, m.readOnly ? kTrack : kGreenSoft);
        r_.text(F_TINY, badge, x + w - 20 - bw / 2, y + 16 + (28 - r_.lineHeight(F_TINY)) / 2,
                m.readOnly ? kDim : kGreen, A_CENTER);
        r_.text(F_MEDIUM_B, drive_name(m.name), x + 104, y + 14, kText, A_LEFT, w - 104 - bw - 36);
        r_.text(F_TINY, "/" + m.name, x + 104, y + 50, kDim);

        const DriveInfo *d = i < drives_.size() ? &drives_[i] : nullptr;
        if (d && d->known) {
            double used = d->total ? 1.0 - (double) d->free / (double) d->total : 0;
            r_.round(x + 104, y + 82, 220, 10, 5, kTrack);
            r_.round(x + 104, y + 82, std::max(10, (int) (220 * used)), 10, 5, used > 0.9 ? kRed : kBlue);
            r_.text(F_TINY, trf(S_FREE_OF, fmt_bytes(d->free), fmt_bytes(d->total)), x + 340, y + 76, kDim, A_LEFT,
                    w - 360);
        } else {
            r_.text(F_TINY, tr(S_SPACE_UNKNOWN), x + 104, y + 76, kDim);
        }
    }
    if (!st.mocha) {
        int y = kContentTop + (int) ((mounts.size() + 1) / 2) * 122 + 10;
        r_.icon(Icon::Lock, 56, y + 2, 26, kDim);
        r_.text(F_BODY, tr(S_NEED_MOCHA), 92, y, kDim, A_LEFT, 1100);
    }
}

#ifdef SFTPIIU_UI_TOUR
void Ui::command(const std::string &cmd, std::vector<UiAction> &actions) {
    static const struct {
        const char *name;
        Button b;
    } buttons[] = {{"A", B_A},       {"B", B_B},         {"X", B_X},       {"Y", B_Y},         {"L", B_L},
                   {"R", B_R},       {"ZL", B_ZL},       {"ZR", B_ZR},     {"up", B_UP},       {"down", B_DOWN},
                   {"left", B_LEFT}, {"right", B_RIGHT}};
    if (cmd.rfind("tab ", 0) == 0) {
        page_ = (Page) std::clamp(atoi(cmd.c_str() + 4), 0, (int) kPageCount - 1);
    } else if (cmd.rfind("lang ", 0) == 0) {
        setLanguage((Lang) std::clamp(atoi(cmd.c_str() + 5), 0, (int) Lang::Count - 1));
    } else if (cmd.rfind("open ", 0) == 0) {
        page_ = kFiles;
        closeDocument();
        pendingOpen_ = cmd.substr(5);
        pendingUp_   = false;
    } else if (cmd.rfind("select ", 0) == 0) {
        explorer_.selected = atoi(cmd.c_str() + 7);
        driveSel_          = atoi(cmd.c_str() + 7);
        settingsSel_       = atoi(cmd.c_str() + 7);
        docSel_            = atoi(cmd.c_str() + 7);
    } else if (cmd == "pw") {
        showPassword_ = !showPassword_;
    } else if (cmd.rfind("press ", 0) == 0) {
        for (const auto &bt : buttons) {
            if (cmd.substr(6) == bt.name) press(bt.b, actions);
        }
    } else if (cmd.rfind("text ", 0) == 0 && swkbdActive_) {
        // Simulates confirming the software keyboard.
        SDL_StopTextInput();
        swkbdActive_ = false;
        onText(swkbdPurpose_, cmd.substr(5));
    }
}
#endif
