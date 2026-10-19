#pragma once
// Drawing layer for the app: anti-aliased rounded shapes and soft shadows
// (nine-sliced from generated textures), generated monochrome icons, button
// glyphs and cached TTF text using the console's shared system font.

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class Icon {
    Gauge,
    Key,
    Drive,
    Folder,
    File,
    SdCard,
    Disc,
    ArrowDown,
    ArrowUp,
    Lock,
    User,
    Chip,
    Clock,
    Gear,
    Picture,
    Doc,
    Globe,
    ChevronLeft,
    ChevronRight,
    Count
};

enum Font { F_TINY, F_SMALL, F_BODY, F_BODY_B, F_MEDIUM, F_MEDIUM_B, F_LARGE_B, F_HUGE_B, F_COUNT };

enum Align { A_LEFT, A_RIGHT, A_CENTER };

class Renderer {
public:
    bool init(SDL_Renderer *renderer);
    // (Re)loads all font sizes from the given OSSharedDataType font.
    bool loadFonts(int sharedFontType);
    void shutdown();

    SDL_Renderer *sdl() const { return sdl_; }
    void endFrame();

    void fill(int x, int y, int w, int h, uint32_t rgba);
    void gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
    void round(int x, int y, int w, int h, int r, uint32_t rgba);
    void shadow(int x, int y, int w, int h, int r, int spread, uint8_t alpha);
    // White card with a soft drop shadow.
    void card(int x, int y, int w, int h, int r = 18, uint32_t rgba = 0xFFFFFFFF);
    void icon(Icon icon, int x, int y, int size, uint32_t rgba);
    void logo(int x, int y, int size);
    // Wii U style button glyph ("A", "B", "X", "Y", "L", "R", "+"), returns its width.
    int button(const char *label, int x, int y, int size);

    // Draws text with its top at y and returns the drawn width. maxWidth > 0
    // shortens it with "…", keeping the end (paths) or the start.
    int text(Font f, const std::string &s, int x, int y, uint32_t rgba, Align align = A_LEFT, int maxWidth = 0,
             bool keepEnd = false);
    // Word-wraps (or breaks between characters for scripts without spaces)
    // into at most maxLines lines; returns the number of lines drawn.
    int textWrapped(Font f, const std::string &s, int x, int y, int maxWidth, int maxLines, int lineStep,
                    uint32_t rgba, Align align = A_LEFT);
    int wrappedLines(Font f, const std::string &s, int maxWidth) { return (int) wrap(f, s, maxWidth).size(); }
    // Body-sized text in another shared system font (e.g. a language's own
    // name in its script); falls back to the current font.
    int textInFont(int sharedFontType, const std::string &s, int x, int y, uint32_t rgba, Align align = A_LEFT);
    int measure(Font f, const std::string &s);
    int lineHeight(Font f);
    std::string fit(Font f, const std::string &s, int maxWidth, bool keepEnd);

private:
    struct CachedText {
        SDL_Texture *tex = nullptr;
        int w = 0, h = 0;
        uint32_t lastFrame = 0;
    };

    void nine(SDL_Texture *tex, int x, int y, int w, int h, int corner, uint32_t rgba);
    std::vector<std::string> wrap(Font f, const std::string &s, int maxWidth);
    void clearTextCache();
    SDL_Texture *makeTexture(int size, uint32_t (*pixel)(float u, float v, void *ctx), void *ctx, int samples);

    SDL_Renderer *sdl_ = nullptr;
    SDL_Texture *circle_ = nullptr;
    SDL_Texture *shadow_ = nullptr;
    SDL_Texture *logo_   = nullptr;
    SDL_Texture *icons_[(int) Icon::Count] = {};
    TTF_Font *fonts_[F_COUNT] = {};
    int fontType_             = -1;
    TTF_Font *altFonts_[4]    = {}; // one body-sized font per shared font type
    std::unordered_map<std::string, CachedText> cache_;
    std::unordered_map<std::string, std::vector<std::string>> wrapCache_;
    uint32_t frame_ = 0;
};
