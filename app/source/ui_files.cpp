// File browser, file actions, text editor, hex and image viewer.

#include "format.h"
#include "platform.h"
#include "ui.h"
#include "ui_style.h"

#include <SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cstring>

using namespace ui;

namespace {

bool has_ext(const std::string &name, std::initializer_list<const char *> exts) {
    size_t dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = name.substr(dot + 1);
    for (auto &c : ext) c = (char) std::tolower((unsigned char) c);
    for (const char *e : exts) {
        if (ext == e) return true;
    }
    return false;
}

bool is_image_name(const std::string &name) {
    return has_ext(name, {"png", "jpg", "jpeg", "bmp", "gif", "tga", "webp"});
}

bool is_text_name(const std::string &name) {
    return has_ext(name, {"txt", "ini", "cfg", "conf", "json", "xml", "log", "md", "csv", "yml", "yaml", "toml", "lua",
                          "js", "py", "sh", "bat", "c", "h", "cpp", "hpp", "html", "htm", "css", "nfo", "srt", "inf"});
}

Icon entry_icon(const Explorer::Entry &e) {
    if (e.dir) return Icon::Folder;
    if (is_image_name(e.name)) return Icon::Picture;
    if (is_text_name(e.name)) return Icon::Doc;
    return Icon::File;
}

// Tabs are shown as spaces; everything else as-is.
std::string display_line(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '\t') out += "    ";
        else out += c;
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------- state helpers

bool Ui::currentWritable() const {
    const Mount *m = explorer_.mount();
    return m && !m->readOnly;
}

bool Ui::docEditable() const {
    return settings_.fileActions && currentWritable();
}

void Ui::closeDocument() {
    view_ = View::Browser;
    doc_  = TextDoc();
    hex_.clear();
    freeImage();
    docPath_.clear();
    docReal_.clear();
}

void Ui::freeImage() {
    if (image_) {
        SDL_DestroyTexture(image_);
        image_ = nullptr;
    }
    imageIndex_ = -1;
}

void Ui::openImage(int index) {
    const auto &entries = explorer_.entries();
    if (index < 0 || index >= (int) entries.size()) return;
    const auto &e = entries[index];
    if (e.size > kMaxImageSize) {
        toast(trf(S_TOO_LARGE, fmt_bytes(kMaxImageSize)), true);
        return;
    }
    std::string vpath = explorer_.childPath(e);
    std::string real  = explorer_.realPath(vpath);
    SDL_Surface *surf = IMG_Load(real.c_str());
    if (!surf) {
        LOG("IMG_Load %s: %s", real.c_str(), IMG_GetError());
        toast(tr(S_CANNOT_READ), true);
        return;
    }
    SDL_Texture *tex = nullptr;
    if ((uint64_t) surf->w * (uint64_t) surf->h <= 4096ull * 4096ull) {
        tex = SDL_CreateTextureFromSurface(sdl_, surf);
    }
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) {
        toast(tr(S_CANNOT_READ), true);
        return;
    }
    freeImage();
    image_      = tex;
    imageW_     = w;
    imageH_     = h;
    imageIndex_ = index;
    explorer_.selected = index;
    docPath_    = vpath;
    docReal_    = real;
    view_       = View::Image;
}

void Ui::openEntry() {
    const auto &entries = explorer_.entries();
    if (entries.empty()) return;
    const auto &e = entries[explorer_.selected];
    if (e.dir) {
        pendingOpen_ = explorer_.childPath(e);
        pendingUp_   = false;
        return;
    }
    std::string vpath = explorer_.childPath(e);
    std::string real  = explorer_.realPath(vpath);
    FileKind kind     = classify_file(e.name, real);
    if (kind == FileKind::Image) {
        openImage(explorer_.selected);
        return;
    }
    if (kind == FileKind::Text && e.size <= kMaxTextSize) {
        int err = doc_.load(real);
        if (err) {
            toast(tr(S_CANNOT_READ), true);
            return;
        }
        view_      = View::Text;
        docPath_   = vpath;
        docReal_   = real;
        docSel_    = 0;
        docScroll_ = 0;
        return;
    }
    if (kind == FileKind::Text) {
        toast(trf(S_TOO_LARGE, fmt_bytes(kMaxTextSize)));
    }
    if (load_hex(real, hex_, hexTotal_) != 0) {
        toast(tr(S_CANNOT_READ), true);
        return;
    }
    view_      = View::Hex;
    docPath_   = vpath;
    docReal_   = real;
    docScroll_ = 0;
}

// ---------------------------------------------------------------- input

void Ui::filesPress(Button b, std::vector<UiAction> &actions) {
    const auto &entries = explorer_.entries();
    int n               = (int) entries.size();
    auto move           = [&](int d) {
        if (n) explorer_.selected = std::clamp(explorer_.selected + d, 0, n - 1);
    };
    switch (b) {
        case B_UP: move(-1); break;
        case B_DOWN: move(1); break;
        case B_LEFT:
        case B_ZL: move(-kFileRows); break;
        case B_RIGHT:
        case B_ZR: move(kFileRows); break;
        case B_A: openEntry(); break;
        case B_B:
            if (!explorer_.atRoot()) {
                pendingUp_   = true;
                pendingOpen_ = explorer_.path();
            }
            break;
        case B_X:
            if (explorer_.atRoot()) break;
            if (!settings_.fileActions) {
                toast(tr(S_HINT_ENABLE_IN_SETTINGS));
            } else {
                openActionsMenu();
            }
            break;
        default:
            break;
    }
    (void) actions;
}

void Ui::viewerPress(Button b, std::vector<UiAction> &actions) {
    if (view_ == View::Image) {
        const auto &entries = explorer_.entries();
        if (b == B_LEFT || b == B_RIGHT) {
            int dir = b == B_RIGHT ? 1 : -1;
            for (int i = imageIndex_ + dir; i >= 0 && i < (int) entries.size(); i += dir) {
                if (!entries[i].dir && is_image_name(entries[i].name)) {
                    openImage(i);
                    break;
                }
            }
        } else if (b == B_B) {
            closeDocument();
        }
        return;
    }
    if (view_ == View::Hex) {
        int rows = (int) ((hex_.size() + 15) / 16);
        if (b == B_UP) docScroll_--;
        if (b == B_DOWN) docScroll_++;
        if (b == B_LEFT || b == B_ZL) docScroll_ -= 12;
        if (b == B_RIGHT || b == B_ZR) docScroll_ += 12;
        docScroll_ = std::clamp(docScroll_, 0, std::max(0, rows - 12));
        if (b == B_B) closeDocument();
        return;
    }

    // Text editor.
    int n = (int) doc_.lines().size();
    switch (b) {
        case B_UP: docSel_--; break;
        case B_DOWN: docSel_++; break;
        case B_LEFT:
        case B_ZL: docSel_ -= kDocRows; break;
        case B_RIGHT:
        case B_ZR: docSel_ += kDocRows; break;
        case B_A:
            if (docEditable()) {
                runCommand(CMD_EDIT_LINE, actions);
            } else {
                toast(tr(settings_.fileActions ? S_ERR_READONLY : S_HINT_ENABLE_IN_SETTINGS));
            }
            break;
        case B_Y:
            if (docEditable()) runCommand(CMD_INSERT_LINE, actions);
            break;
        case B_X: {
            bool ed = docEditable();
            openMenu(basename_of(docPath_), {
                                                    {CMD_EDIT_LINE, tr(S_EDIT_LINE), ed},
                                                    {CMD_INSERT_LINE, tr(S_INSERT_LINE), ed},
                                                    {CMD_DUP_LINE, tr(S_DUP_LINE), ed},
                                                    {CMD_DELETE_LINE, tr(S_DELETE_LINE), ed, true},
                                                    {CMD_UNDO, tr(S_UNDO), ed && doc_.canUndo()},
                                                    {CMD_SAVE, tr(S_SAVE), ed && doc_.modified()},
                                                    {CMD_CLOSE_DOC, tr(S_CLOSE)},
                                            });
            break;
        }
        case B_B:
            runCommand(CMD_CLOSE_DOC, actions);
            break;
        default:
            break;
    }
    docSel_ = std::clamp(docSel_, 0, std::max(0, n - 1));
}

void Ui::openActionsMenu() {
    const auto &entries = explorer_.entries();
    bool w              = currentWritable();
    bool hasSel         = !entries.empty();
    std::string title   = hasSel ? entries[explorer_.selected].name : explorer_.path();
    if (!w) {
        title += "  (" + std::string(tr(S_READ_ONLY)) + ")";
    }
    std::string paste = tr(S_ACT_PASTE);
    if (!clipPath_.empty()) {
        paste += "  · " + basename_of(clipPath_);
    }
    openMenu(title, {
                            {CMD_RENAME, tr(S_ACT_RENAME), w && hasSel},
                            {CMD_COPY, tr(S_ACT_COPY), hasSel},
                            {CMD_CUT, tr(S_ACT_CUT), w && hasSel},
                            {CMD_PASTE, paste, w && !clipPath_.empty()},
                            {CMD_NEWFOLDER, tr(S_ACT_NEWFOLDER), w},
                            {CMD_NEWFILE, tr(S_ACT_NEWFILE), w},
                            {CMD_DELETE, tr(S_ACT_DELETE), w && hasSel, true},
                    });
}

// ---------------------------------------------------------------- commands

void Ui::runCommand(int cmd, std::vector<UiAction> &actions) {
    const auto &entries        = explorer_.entries();
    const Explorer::Entry *sel = entries.empty() ? nullptr : &entries[explorer_.selected];
    switch (cmd) {
        case CMD_CANCEL:
            if (modal_.kind != ModalKind::Progress) closeModal();
            break;

        // ---- file actions
        case CMD_RENAME:
            closeModal();
            if (sel) askText(TXT_RENAME, sel->name, tr(S_NEW_NAME));
            break;
        case CMD_DELETE:
            if (!sel) break;
            openConfirm(trf(S_CONFIRM_DELETE, sel->name), tr(sel->dir ? S_CONFIRM_DELETE_DIR : S_CONFIRM_DELETE_FILE),
                        {{CMD_CANCEL, tr(S_CANCEL)}, {CMD_DELETE_CONFIRMED, tr(S_ACT_DELETE), true, true}});
            break;
        case CMD_DELETE_CONFIRMED: {
            closeModal();
            if (!sel || !currentWritable()) break;
            std::string vpath = explorer_.childPath(*sel);
            if (!clipPath_.empty() && (clipPath_ == vpath || clipPath_.rfind(vpath + "/", 0) == 0)) {
                clipPath_.clear();
            }
            jobReload_.clear();
            if (job_.start(fileops::Job::Kind::Delete, explorer_.realPath(vpath), {}, true)) {
                modal_.kind  = ModalKind::Progress;
                modal_.title = tr(S_DELETING);
            }
            break;
        }
        case CMD_COPY:
        case CMD_CUT:
            closeModal();
            if (!sel) break;
            clipPath_ = explorer_.childPath(*sel);
            clipCut_  = cmd == CMD_CUT;
            toast(trf(S_CLIPBOARD, sel->name));
            break;
        case CMD_PASTE:
            closeModal();
            startPaste();
            break;
        case CMD_NEWFOLDER:
            closeModal();
            askText(TXT_NEWFOLDER, tr(S_ACT_NEWFOLDER), tr(S_ACT_NEWFOLDER));
            break;
        case CMD_NEWFILE:
            closeModal();
            askText(TXT_NEWFILE, std::string(tr(S_TEXT_FILE)) + ".txt", tr(S_ACT_NEWFILE));
            break;

        // ---- editor
        case CMD_EDIT_LINE:
            closeModal();
            askText(TXT_EDIT_LINE, doc_.lines()[docSel_], "");
            break;
        case CMD_INSERT_LINE:
            closeModal();
            askText(TXT_INSERT_LINE, "", "");
            break;
        case CMD_DUP_LINE:
            closeModal();
            doc_.insertLine(docSel_ + 1, doc_.lines()[docSel_]);
            docSel_++;
            break;
        case CMD_DELETE_LINE:
            closeModal();
            doc_.deleteLine(docSel_);
            docSel_ = std::min(docSel_, (int) doc_.lines().size() - 1);
            break;
        case CMD_UNDO: {
            closeModal();
            int line = doc_.undo();
            if (line >= 0) docSel_ = line;
            break;
        }
        case CMD_SAVE:
        case CMD_SAVE_AND_CLOSE: {
            closeModal();
            if (!docEditable()) break;
            int err = doc_.save(docReal_);
            if (err) {
                toast(trf(S_ERR_FAILED, strerror(err)), true);
                break;
            }
            toast(tr(S_SAVED));
            if (cmd == CMD_SAVE_AND_CLOSE) {
                std::string name = basename_of(docPath_);
                closeDocument();
                explorer_.reload(name);
            }
            break;
        }
        case CMD_CLOSE_DOC:
            closeModal();
            if (view_ == View::Text && doc_.modified()) {
                openConfirm(tr(S_UNSAVED_TITLE), trf(S_UNSAVED_MSG, basename_of(docPath_)),
                            {{CMD_CANCEL, tr(S_CANCEL)},
                             {CMD_DISCARD_AND_CLOSE, tr(S_DISCARD), true, true},
                             {CMD_SAVE_AND_CLOSE, tr(S_SAVE)}});
            } else {
                closeDocument();
            }
            break;
        case CMD_DISCARD_AND_CLOSE:
            closeModal();
            closeDocument();
            break;

        // ---- settings
        case CMD_ENABLE_FILEOPS:
            closeModal();
            settings_.fileActions = true;
            actions.push_back(UiAction::SettingsChanged);
            break;
        case CMD_INSTALL_PLUGIN:
            closeModal();
            actions.push_back(UiAction::BackgroundOn);
            break;
        default:
            closeModal();
            break;
    }
}

void Ui::onText(int purpose, const std::string &text) {
    switch (purpose) {
        case TXT_EDIT_LINE:
            doc_.setLine(docSel_, text);
            return;
        case TXT_INSERT_LINE:
            doc_.insertLine(docSel_ + 1, text);
            docSel_++;
            return;
        default:
            break;
    }

    // Names: rename, new folder, new file.
    if (!currentWritable()) {
        toast(tr(S_ERR_READONLY), true);
        return;
    }
    if (!fileops::valid_name(text)) {
        toast(tr(S_ERR_INVALID_NAME), true);
        return;
    }
    const auto &entries = explorer_.entries();
    std::string dirReal = explorer_.realPath(explorer_.path());
    std::string target  = fileops::join(dirReal, text);
    if (purpose == TXT_RENAME && !entries.empty() && entries[explorer_.selected].name == text) {
        return; // unchanged
    }
    if (fileops::exists(target)) {
        toast(tr(S_ERR_EXISTS), true);
        return;
    }
    int err = 0;
    if (purpose == TXT_RENAME && !entries.empty()) {
        std::string oldV = explorer_.childPath(entries[explorer_.selected]);
        err              = fileops::rename_path(explorer_.realPath(oldV), target);
        if (!err && clipPath_ == oldV) {
            clipPath_ = dirname_of(oldV) == "/" ? "/" + text : explorer_.path() + "/" + text;
        }
    } else if (purpose == TXT_NEWFOLDER) {
        err = fileops::make_dir(target);
    } else if (purpose == TXT_NEWFILE) {
        err = fileops::create_file(target);
    }
    if (err) {
        toast(trf(S_ERR_FAILED, strerror(err)), true);
        return;
    }
    explorer_.reload(text);
    toast(tr(S_DONE));
    if (purpose == TXT_NEWFILE) {
        openEntry(); // straight into the editor
    }
}

void Ui::startPaste() {
    if (clipPath_.empty() || !explorer_.vfs()) return;
    const Vfs &vfs          = *explorer_.vfs();
    Vfs::Resolved src       = vfs.resolve(clipPath_);
    Vfs::Resolved dstDir    = vfs.resolve(explorer_.path());
    std::string name        = basename_of(clipPath_);
    if (src.kind != Vfs::Kind::Inside || !fileops::exists(src.real)) {
        toast(tr(S_CANNOT_READ), true);
        clipPath_.clear();
        return;
    }
    if (!dstDir.mount || dstDir.mount->readOnly || (clipCut_ && src.mount->readOnly)) {
        toast(tr(S_ERR_READONLY), true);
        return;
    }
    bool srcDir = fileops::is_dir(src.real);
    if (srcDir && (explorer_.path() == clipPath_ || explorer_.path().rfind(clipPath_ + "/", 0) == 0)) {
        toast(tr(S_ERR_INTO_ITSELF), true);
        return;
    }
    if (clipCut_ && dirname_of(clipPath_) == explorer_.path()) {
        clipPath_.clear(); // moving into the same folder: nothing to do
        return;
    }
    std::string dstName = fileops::unique_name(dstDir.real, name);
    std::string dstReal = fileops::join(dstDir.real, dstName);
    auto kind           = clipCut_ ? fileops::Job::Kind::Move : fileops::Job::Kind::Copy;
    if (job_.start(kind, src.real, dstReal, src.mount == dstDir.mount)) {
        jobReload_   = dstName;
        modal_       = Modal();
        modal_.kind  = ModalKind::Progress;
        modal_.title = tr(clipCut_ ? S_MOVING : S_COPYING);
        if (clipCut_) clipPath_.clear();
    }
}

void Ui::pollJob() {
    if (!job_.active() || !job_.collect()) {
        return;
    }
    if (modal_.kind == ModalKind::Progress) {
        closeModal();
    }
    if (job_.error()) {
        toast(trf(S_ERR_FAILED, strerror(job_.error())), true);
    } else if (job_.canceled()) {
        toast(tr(S_CANCELED));
    } else {
        toast(tr(S_DONE));
    }
    explorer_.reload(jobReload_);
}

// ---------------------------------------------------------------- drawing

void Ui::drawFiles(const AppState &st) {
    if (view_ == View::Text) {
        drawTextView();
        return;
    }
    if (view_ == View::Hex) {
        drawHexView();
        return;
    }
    if (view_ == View::Image) {
        drawImageView();
        return;
    }

    const auto &entries = explorer_.entries();
    const Mount *mount  = explorer_.mount();

    // Breadcrumb: tap to go up.
    {
        const int x = 40, y = kContentTop, w = 1200, h = 56;
        r_.card(x, y, w, h, 16);
        addHit(x, y, w, h, HIT_BREADCRUMB, 0);
        r_.icon(explorer_.atRoot() ? Icon::Drive : Icon::Folder, x + 18, y + 13, 30, kBlue);
        std::string shown = explorer_.atRoot() ? std::string("/") : explorer_.path();
        r_.text(F_BODY_B, shown, x + 60, y + (h - r_.lineHeight(F_BODY_B)) / 2, kText, A_LEFT, 760, true);
        std::string count = entries.size() == 1 ? tr(S_ITEMS_ONE) : trf(S_ITEMS, std::to_string(entries.size()));
        if (explorer_.truncated()) count = trf(S_TRUNCATED, std::to_string(entries.size()));
        int cw = r_.text(F_SMALL, count, x + w - 24, y + (h - r_.lineHeight(F_SMALL)) / 2, kDim, A_RIGHT);
        if (mount && mount->readOnly) {
            std::string ro = tr(S_READ_ONLY);
            int bw         = r_.measure(F_TINY, ro) + 24;
            int bx         = x + w - 24 - cw - 16 - bw;
            r_.round(bx, y + 14, bw, 28, 14, kTrack);
            r_.text(F_TINY, ro, bx + bw / 2, y + 14 + (28 - r_.lineHeight(F_TINY)) / 2, kDim, A_CENTER);
        }
    }

    // List.
    const int lx = 40, ly = kContentTop + 68, lw = 800, lh = 420;
    r_.card(lx, ly, lw, lh);
    int &sel    = explorer_.selected;
    int &scroll = explorer_.scroll;
    if (!entries.empty()) {
        sel = std::clamp(sel, 0, (int) entries.size() - 1);
        if (sel < scroll) scroll = sel;
        if (sel >= scroll + kFileRows) scroll = sel - kFileRows + 1;
        scroll = std::clamp(scroll, 0, std::max(0, (int) entries.size() - kFileRows));
    }
    if (pendingOpen_.size()) {
        r_.text(F_MEDIUM, tr(S_LOADING), lx + lw / 2, ly + lh / 2 - 16, kDim, A_CENTER);
    } else if (explorer_.failed()) {
        r_.icon(Icon::Folder, lx + lw / 2 - 40, ly + 120, 80, kTrack);
        r_.text(F_MEDIUM, tr(S_CANNOT_OPEN), lx + lw / 2, ly + 220, kDim, A_CENTER);
    } else if (entries.empty()) {
        r_.icon(Icon::Folder, lx + lw / 2 - 40, ly + 120, 80, kTrack);
        r_.text(F_MEDIUM, tr(S_EMPTY_FOLDER), lx + lw / 2, ly + 220, kDim, A_CENTER);
    } else {
        for (int i = 0; i < kFileRows && scroll + i < (int) entries.size(); i++) {
            int idx       = scroll + i;
            const auto &e = entries[idx];
            int ry        = ly + 12 + i * kFileRowH;
            if (idx == sel) {
                drawGlow(lx + 10, ry, lw - 40, kFileRowH - 4, 10);
                r_.round(lx + 10, ry, lw - 40, kFileRowH - 4, 10, kBlueSoft);
            }
            addHit(lx, ry, lw - 20, kFileRowH, HIT_FILE, idx);
            Icon ic = explorer_.atRoot() ? drive_icon(e.name) : entry_icon(e);
            r_.icon(ic, lx + 22, ry + 6, 28, e.dir ? kBlue : kGrey);
            std::string name = explorer_.atRoot() ? drive_name(e.name) : e.name;
            bool clipped     = !clipPath_.empty() && explorer_.childPath(e) == clipPath_;
            r_.text(F_BODY, name, lx + 62, ry + (kFileRowH - 4 - r_.lineHeight(F_BODY)) / 2,
                    clipped && clipCut_ ? kDim : kText, A_LEFT, 520);
            std::string right = e.dir ? "›" : fmt_bytes(e.size);
            r_.text(F_SMALL, right, lx + lw - 46, ry + (kFileRowH - 4 - r_.lineHeight(F_SMALL)) / 2, kDim, A_RIGHT);
        }
        // Scrollbar.
        if ((int) entries.size() > kFileRows) {
            int th = std::max(30, (lh - 24) * kFileRows / (int) entries.size());
            int ty = ly + 12 + (lh - 24 - th) * scroll / std::max(1, (int) entries.size() - kFileRows);
            r_.round(lx + lw - 20, ly + 12, 6, lh - 24, 3, kTrack);
            r_.round(lx + lw - 20, ty, 6, th, 3, kBlue);
        }
    }

    // Details of the selected entry.
    const int dx = 856, dy = ly, dw = 384, dh = lh;
    r_.card(dx, dy, dw, dh);
    if (entries.empty() || pendingOpen_.size()) {
        return;
    }
    const auto &e = entries[sel];
    Icon ic       = explorer_.atRoot() ? drive_icon(e.name) : entry_icon(e);
    r_.round(dx + dw / 2 - 54, dy + 22, 108, 108, 54, kBlueSoft);
    r_.icon(ic, dx + dw / 2 - 36, dy + 40, 72, e.dir ? kBlue : kGrey);
    std::string name = explorer_.atRoot() ? drive_name(e.name) : e.name;
    r_.text(F_MEDIUM_B, name, dx + dw / 2, dy + 144, kText, A_CENTER, dw - 40);

    struct Row {
        Str label;
        std::string value;
    };
    std::vector<Row> rows;
    Str type = e.dir ? S_FOLDER : (is_image_name(e.name) ? S_IMAGE : (is_text_name(e.name) ? S_TEXT_FILE : S_FILE));
    rows.push_back({S_TYPE, tr(type)});
    if (!e.dir) rows.push_back({S_SIZE, fmt_bytes(e.size)});
    if (e.statOk && e.mtime > 0) rows.push_back({S_MODIFIED, fmt_date(e.mtime)});
    rows.push_back({S_LOCATION, explorer_.childPath(e)});
    const Mount *m = explorer_.atRoot() && st.vfs ? st.vfs->resolve("/" + e.name).mount : mount;
    if (m) rows.push_back({S_ACCESS_RIGHTS, tr(m->readOnly ? S_READ_ONLY : S_READ_WRITE)});
    int ry = dy + 196;
    for (const auto &row : rows) {
        r_.fill(dx + 20, ry - 8, dw - 40, 1, kTrack);
        r_.text(F_SMALL, tr(row.label), dx + 24, ry, kDim, A_LEFT, 130);
        r_.text(F_SMALL, row.value, dx + dw - 24, ry, kText, A_RIGHT, dw - 180, true);
        ry += 42;
    }
}

void Ui::drawTextView() {
    const auto &lines = doc_.lines();
    int n             = (int) lines.size();
    docSel_           = std::clamp(docSel_, 0, std::max(0, n - 1));
    if (docSel_ < docScroll_) docScroll_ = docSel_;
    if (docSel_ >= docScroll_ + kDocRows) docScroll_ = docSel_ - kDocRows + 1;
    docScroll_ = std::clamp(docScroll_, 0, std::max(0, n - kDocRows));

    // Title bar: name, modified marker, position and format.
    {
        const int x = 40, y = kContentTop, w = 1200, h = 56;
        r_.card(x, y, w, h, 16);
        r_.icon(Icon::Doc, x + 18, y + 13, 30, kBlue);
        int nw = r_.text(F_BODY_B, basename_of(docPath_), x + 60, y + (h - r_.lineHeight(F_BODY_B)) / 2, kText,
                         A_LEFT, 520);
        if (doc_.modified()) {
            r_.round(x + 72 + nw, y + 23, 10, 10, 5, kOrange);
        }
        std::string info = trf(S_LINE_OF, std::to_string(docSel_ + 1), std::to_string(n)) + "  ·  " + doc_.format();
        int iw           = r_.text(F_SMALL, info, x + w - 24, y + (h - r_.lineHeight(F_SMALL)) / 2, kDim, A_RIGHT);
        if (!docEditable()) {
            std::string ro = tr(S_READ_ONLY);
            int bw         = r_.measure(F_TINY, ro) + 24;
            int bx         = x + w - 24 - iw - 16 - bw;
            r_.round(bx, y + 14, bw, 28, 14, kTrack);
            r_.text(F_TINY, ro, bx + bw / 2, y + 14 + (28 - r_.lineHeight(F_TINY)) / 2, kDim, A_CENTER);
        }
    }

    // Lines with numbers.
    const int x = 40, y = kContentTop + 68, w = 1200, h = 18 + kDocRows * kDocRowH;
    r_.card(x, y, w, h);
    r_.fill(x + 92, y + 10, 1, h - 20, kTrack);
    for (int i = 0; i < kDocRows && docScroll_ + i < n; i++) {
        int idx = docScroll_ + i;
        int ry  = y + 9 + i * kDocRowH;
        if (idx == docSel_) {
            drawGlow(x + 100, ry, w - 136, kDocRowH, 8);
            r_.round(x + 100, ry, w - 136, kDocRowH, 8, kBlueSoft);
        }
        addHit(x, ry, w - 20, kDocRowH, HIT_LINE, idx);
        int ty = ry + (kDocRowH - r_.lineHeight(F_SMALL)) / 2;
        r_.text(F_SMALL, std::to_string(idx + 1), x + 78, ty, idx == docSel_ ? kBlue : kGrey, A_RIGHT);
        r_.text(F_SMALL, display_line(lines[idx]), x + 112, ty, kText, A_LEFT, w - 160);
    }
    if (n > kDocRows) {
        int th = std::max(24, (h - 20) * kDocRows / n);
        int ty = y + 10 + (h - 20 - th) * docScroll_ / std::max(1, n - kDocRows);
        r_.round(x + w - 18, y + 10, 6, h - 20, 3, kTrack);
        r_.round(x + w - 18, ty, 6, th, 3, kBlue);
    }

    // Full text of the selected line.
    const int py = y + h + 12, ph = kFooterY - 12 - py;
    r_.card(x, py, w, ph, 16);
    const std::string &cur = lines[docSel_];
    if (cur.empty()) {
        r_.text(F_SMALL, tr(S_EMPTY_LINE), x + 24, py + 8, kGrey);
    } else {
        r_.textWrapped(F_SMALL, display_line(cur), x + 24, py + 6, w - 48, 2, 24, kText);
    }
}

void Ui::drawHexView() {
    {
        const int x = 40, y = kContentTop, w = 1200, h = 56;
        r_.card(x, y, w, h, 16);
        r_.icon(Icon::File, x + 18, y + 13, 30, kGrey);
        r_.text(F_BODY_B, basename_of(docPath_), x + 60, y + (h - r_.lineHeight(F_BODY_B)) / 2, kText, A_LEFT, 460);
        std::string info = std::string(tr(S_HEX_VIEW)) + "  ·  " + fmt_bytes(hexTotal_);
        if (hexTotal_ > hex_.size()) info += "  ·  " + trf(S_HEX_LIMIT, fmt_bytes(hex_.size()));
        r_.text(F_SMALL, info, x + w - 24, y + (h - r_.lineHeight(F_SMALL)) / 2, kDim, A_RIGHT, 640);
    }
    const int x = 40, y = kContentTop + 68, w = 1200, h = 420, rowH = 33;
    r_.card(x, y, w, h);
    static const char digits[] = "0123456789ABCDEF";
    int rows                   = (int) ((hex_.size() + 15) / 16);
    for (int i = 0; i < 12 && docScroll_ + i < rows; i++) {
        int row = docScroll_ + i;
        int ry  = y + 12 + i * rowH;
        int ty  = ry + (rowH - r_.lineHeight(F_SMALL)) / 2;
        char off[16];
        snprintf(off, sizeof(off), "%08X", row * 16);
        r_.text(F_SMALL, off, x + 24, ty, kGrey);
        std::string ascii;
        for (int c = 0; c < 16; c++) {
            size_t pos = (size_t) row * 16 + c;
            if (pos >= hex_.size()) break;
            uint8_t b    = hex_[pos];
            char cell[3] = {digits[b >> 4], digits[b & 15], 0};
            int cx       = x + 150 + c * 36 + (c >= 8 ? 14 : 0);
            r_.text(F_SMALL, cell, cx, ty, b ? kText : kGrey);
            ascii += (b >= 0x20 && b < 0x7F) ? (char) b : '.';
        }
        r_.text(F_SMALL, ascii, x + 150 + 16 * 36 + 40, ty, kDim);
    }
}

void Ui::drawImageView() {
    {
        const int x = 40, y = kContentTop, w = 1200, h = 56;
        r_.card(x, y, w, h, 16);
        r_.icon(Icon::Picture, x + 18, y + 13, 30, kBlue);
        r_.text(F_BODY_B, basename_of(docPath_), x + 60, y + (h - r_.lineHeight(F_BODY_B)) / 2, kText, A_LEFT, 640);
        uint64_t size = imageIndex_ >= 0 && imageIndex_ < (int) explorer_.entries().size()
                                ? explorer_.entries()[imageIndex_].size
                                : 0;
        std::string info = std::to_string(imageW_) + " × " + std::to_string(imageH_) + "  ·  " + fmt_bytes(size);
        r_.text(F_SMALL, info, x + w - 24, y + (h - r_.lineHeight(F_SMALL)) / 2, kDim, A_RIGHT);
    }
    const int x = 40, y = kContentTop + 68, w = 1200, h = kFooterY - 12 - (kContentTop + 68);
    r_.card(x, y, w, h);
    if (!image_ || imageW_ <= 0 || imageH_ <= 0) {
        return;
    }
    // Fit inside the card, never upscale beyond 2x.
    const int pad = 14;
    double scale  = std::min({(double) (w - 2 * pad) / imageW_, (double) (h - 2 * pad) / imageH_, 2.0});
    int dw = (int) (imageW_ * scale), dh = (int) (imageH_ * scale);
    SDL_Rect dst{x + (w - dw) / 2, y + (h - dh) / 2, dw, dh};
    SDL_RenderCopy(sdl_, image_, nullptr, &dst);
}
