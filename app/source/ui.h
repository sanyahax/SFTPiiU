#pragma once
// The app's screens: Dashboard, Credentials, Drives, Files (browser, text
// editor, hex and image viewer) and Settings. Wii U styled, 1280x720 on the TV
// and mirrored to the GamePad. Navigation: L/R switch pages, D-pad/stick
// select, A open, B back, X actions, + quit; touch works everywhere too.

#include "background.h"
#include "explorer.h"
#include "fileops.h"
#include "i18n.h"
#include "monitor.h"
#include "render.h"
#include "textdoc.h"

#include <SDL.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct AppState {
    bool running = false;
    std::string error;
    std::string ip; // empty without network
    uint16_t port = 22;
    std::string password;
    bool passwordLogin  = true;
    bool canNewPassword = false;
    bool fromPlugin     = false;
    std::string fingerprint;
    int keyCount           = -1; // -1: no authorized_keys file
    bool mocha             = false;
    uint64_t serverStartMs = 0;
    const Vfs *vfs         = nullptr;
    Monitor::Snapshot snap;
    background::Status bg; // the plugin that keeps the server running
};

// Settings the user changes in the app; main.cpp persists them.
struct UiSettings {
    bool fileActions = false;
    bool showHidden  = false;
    int language     = -1; // -1: follow the console
};

enum class UiAction { Quit, NewPassword, SettingsChanged, BackgroundOn, BackgroundOff };

class Ui {
public:
    explicit Ui(UiSettings &settings) : settings_(settings) {}

    bool init();
    void shutdown();
    // Applies settings_.language (or the console language).
    void applyLanguage();

    // Startup screen with a progress bar (0..1).
    void splash(float progress);
    // Handles input and draws one frame. Returns false once the app must exit.
    bool frame(const AppState &st, std::vector<UiAction> &actions);
    // Short message above the footer, e.g. the result of an action.
    void notify(const std::string &text, bool error = false) { toast(text, error); }
    bool onSettingsPage() const { return page_ == kSettings; }

#ifdef SFTPIIU_UI_TOUR
    // Test-only remote control, see main.cpp.
    void command(const std::string &cmd, std::vector<UiAction> &actions);
#endif

private:
    enum Page { kDashboard, kAccess, kDrives, kFiles, kSettings, kPageCount };
    enum Button { B_A, B_B, B_X, B_Y, B_L, B_R, B_ZL, B_ZR, B_PLUS, B_UP, B_DOWN, B_LEFT, B_RIGHT, B_COUNT };
    enum class View { Browser, Text, Hex, Image };
    enum class ModalKind { None, Menu, Confirm, Progress };

    struct Hit {
        SDL_Rect rect;
        int kind; // see ui.cpp
        int value;
    };

    struct DriveInfo {
        bool known     = false;
        uint64_t total = 0;
        uint64_t free  = 0;
    };

    struct MenuItem {
        int cmd;
        std::string label;
        bool enabled = true;
        bool danger  = false;
    };

    struct Modal {
        ModalKind kind = ModalKind::None;
        std::string title, message;
        std::vector<MenuItem> items; // menu entries or dialog buttons
        int sel = 0;
    };

    struct Speed {
        uint64_t bytes = 0;
        uint64_t ms    = 0;
        double perSec  = 0;
    };

    // input
    void pollEvents(std::vector<UiAction> &actions, bool &quit);
    void setHeld(Button b, bool down, std::vector<UiAction> &actions);
    void press(Button b, std::vector<UiAction> &actions);
    void touch(int x, int y, std::vector<UiAction> &actions);
    void addHit(int x, int y, int w, int h, int kind, int value);
    void setLanguage(Lang lang);

    // modal dialogs, toasts, software keyboard
    void openMenu(const std::string &title, std::vector<MenuItem> items);
    void openConfirm(const std::string &title, const std::string &message, std::vector<MenuItem> buttons);
    void closeModal() { modal_ = Modal(); }
    void modalPress(Button b, std::vector<UiAction> &actions);
    void runCommand(int cmd, std::vector<UiAction> &actions);
    void toast(const std::string &text, bool error = false);
    void askText(int purpose, const std::string &initial, const std::string &hint);
    void onText(int purpose, const std::string &text);
    void drawModal();
    void drawToast();

    // data
    void updateStats(const AppState &st);
    void refreshDrives(const AppState &st, bool force);

    // chrome and pages (ui.cpp)
    void drawBackground();
    void drawHeader(const AppState &st);
    void drawTabs();
    void drawFooter(const AppState &st);
    void drawDashboard(const AppState &st);
    void drawAccess(const AppState &st);
    void drawDrives(const AppState &st);
    void drawGlow(int x, int y, int w, int h, int r);

    // files, editor and viewers (ui_files.cpp)
    void filesPress(Button b, std::vector<UiAction> &actions);
    void viewerPress(Button b, std::vector<UiAction> &actions);
    void drawFiles(const AppState &st);
    void drawTextView();
    void drawHexView();
    void drawImageView();
    void openEntry();
    void openActionsMenu();
    void startPaste();
    void pollJob();
    void closeDocument();
    bool docEditable() const;
    bool currentWritable() const;
    void openImage(int index);
    void freeImage();

    // settings (ui_settings.cpp)
    void settingsPress(Button b, std::vector<UiAction> &actions);
    void drawSettings(const AppState &st);
    void changeSetting(int row, int dir, std::vector<UiAction> &actions);

    SDL_Window *window_ = nullptr;
    SDL_Renderer *sdl_  = nullptr;
    Renderer r_;
    int fontType_ = -1;
    std::vector<SDL_GameController *> pads_;
    UiSettings &settings_;

    const AppState *st_ = nullptr; // valid during frame()
    background::Status bg_;        // copy from the last frame, for input handling
    Page page_          = kDashboard;
    bool showPassword_  = false;
    uint64_t confirmUntil_ = 0;
    int driveSel_          = 0;
    int settingsSel_       = 0;
    std::vector<DriveInfo> drives_;
    uint64_t drivesAt_ = 0;
    std::vector<Hit> hits_;

    // file browser
    Explorer explorer_;
    bool explorerOpened_ = false;
    std::string pendingOpen_;
    bool pendingUp_  = false;
    bool loadingNow_ = false;
    std::string clipPath_; // virtual path of the copied/cut entry
    bool clipCut_ = false;
    fileops::Job job_;
    std::string jobReload_; // name to select after the job

    // viewers
    View view_ = View::Browser;
    std::string docPath_; // virtual path of the open file
    std::string docReal_;
    TextDoc doc_;
    int docSel_    = 0;
    int docScroll_ = 0;
    std::vector<uint8_t> hex_;
    uint64_t hexTotal_ = 0;
    SDL_Texture *image_ = nullptr;
    int imageW_ = 0, imageH_ = 0;
    int imageIndex_ = -1;

    // dialogs and text input
    Modal modal_;
    bool swkbdActive_ = false;
    int swkbdPurpose_ = 0;
    std::string swkbdText_;
    std::string toast_;
    bool toastError_     = false;
    uint64_t toastUntil_ = 0;

    // input repeat
    bool held_[B_COUNT]           = {};
    uint64_t nextRepeat_[B_COUNT] = {};
    int stickY_ = 0, stickX_ = 0;
    bool trigL_ = false, trigR_ = false;

    // statistics
    std::map<uint32_t, Speed> speeds_;
    std::vector<float> histDown_, histUp_;
    uint64_t lastDown_ = 0, lastUp_ = 0, lastSampleMs_ = 0;
    double speedDown_ = 0, speedUp_ = 0;
};
