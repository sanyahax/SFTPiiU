#include "render.h"
#include "platform.h"

#include <coreinit/memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

constexpr int kFontSizes[F_COUNT] = {18, 20, 24, 24, 28, 28, 36, 58};
constexpr bool kFontBold[F_COUNT] = {false, false, false, true, false, true, true, true};

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

bool in_rect(float u, float v, float x0, float y0, float x1, float y1) {
    return u >= x0 && u <= x1 && v >= y0 && v <= y1;
}

bool in_circle(float u, float v, float cx, float cy, float r) {
    float dx = u - cx, dy = v - cy;
    return dx * dx + dy * dy <= r * r;
}

bool in_ring(float u, float v, float cx, float cy, float r0, float r1) {
    float dx = u - cx, dy = v - cy, d2 = dx * dx + dy * dy;
    return d2 >= r0 * r0 && d2 <= r1 * r1;
}

bool in_rrect(float u, float v, float x0, float y0, float x1, float y1, float r) {
    if (!in_rect(u, v, x0, y0, x1, y1)) {
        return false;
    }
    float cx = clampf(u, x0 + r, x1 - r), cy = clampf(v, y0 + r, y1 - r);
    return in_circle(u, v, cx, cy, r);
}

bool in_tri(float u, float v, float ax, float ay, float bx, float by, float cx, float cy) {
    auto side = [](float px, float py, float x1, float y1, float x2, float y2) {
        return (px - x2) * (y1 - y2) - (x1 - x2) * (py - y2);
    };
    float d1 = side(u, v, ax, ay, bx, by), d2 = side(u, v, bx, by, cx, cy), d3 = side(u, v, cx, cy, ax, ay);
    bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(neg && pos);
}

bool in_seg(float u, float v, float ax, float ay, float bx, float by, float hw) {
    float vx = bx - ax, vy = by - ay;
    float t  = clampf(((u - ax) * vx + (v - ay) * vy) / (vx * vx + vy * vy), 0, 1);
    float dx = u - (ax + t * vx), dy = v - (ay + t * vy);
    return dx * dx + dy * dy <= hw * hw;
}

// Icon shapes in unit coordinates (u right, v down).
bool icon_shape(Icon icon, float u, float v) {
    switch (icon) {
        case Icon::Gauge:
            return (in_ring(u, v, 0.5f, 0.6f, 0.29f, 0.40f) && v <= 0.72f) ||
                   in_seg(u, v, 0.5f, 0.6f, 0.68f, 0.40f, 0.045f) || in_circle(u, v, 0.5f, 0.6f, 0.08f);
        case Icon::Key:
            return in_ring(u, v, 0.30f, 0.5f, 0.10f, 0.20f) || in_rect(u, v, 0.47f, 0.455f, 0.90f, 0.545f) ||
                   in_rect(u, v, 0.70f, 0.545f, 0.77f, 0.68f) || in_rect(u, v, 0.82f, 0.545f, 0.89f, 0.64f);
        case Icon::Drive:
            return in_rrect(u, v, 0.10f, 0.30f, 0.90f, 0.70f, 0.10f) && !in_circle(u, v, 0.76f, 0.5f, 0.05f) &&
                   !in_rect(u, v, 0.20f, 0.47f, 0.56f, 0.53f);
        case Icon::Folder:
            return in_rrect(u, v, 0.08f, 0.20f, 0.46f, 0.40f, 0.05f) || in_rrect(u, v, 0.08f, 0.30f, 0.92f, 0.82f, 0.07f);
        case Icon::File:
            return in_rrect(u, v, 0.22f, 0.10f, 0.78f, 0.90f, 0.05f) && !(u > 0.56f && v < 0.10f + (u - 0.56f));
        case Icon::SdCard:
            return in_rrect(u, v, 0.24f, 0.10f, 0.76f, 0.90f, 0.05f) && (u - 0.24f) + (v - 0.10f) >= 0.16f &&
                   !in_rect(u, v, 0.37f, 0.15f, 0.43f, 0.31f) && !in_rect(u, v, 0.49f, 0.15f, 0.55f, 0.31f) &&
                   !in_rect(u, v, 0.61f, 0.15f, 0.67f, 0.31f);
        case Icon::Disc:
            return in_ring(u, v, 0.5f, 0.5f, 0.08f, 0.42f);
        case Icon::ArrowDown:
            return in_rect(u, v, 0.40f, 0.12f, 0.60f, 0.55f) || in_tri(u, v, 0.16f, 0.50f, 0.84f, 0.50f, 0.50f, 0.88f);
        case Icon::ArrowUp:
            return in_rect(u, v, 0.40f, 0.45f, 0.60f, 0.88f) || in_tri(u, v, 0.16f, 0.50f, 0.84f, 0.50f, 0.50f, 0.12f);
        case Icon::Lock:
            return in_rrect(u, v, 0.20f, 0.44f, 0.80f, 0.88f, 0.07f) ||
                   (in_ring(u, v, 0.5f, 0.44f, 0.13f, 0.21f) && v <= 0.44f);
        case Icon::User:
            return in_circle(u, v, 0.5f, 0.33f, 0.17f) || (in_circle(u, v, 0.5f, 1.02f, 0.42f) && v <= 0.88f);
        case Icon::Chip: {
            if (in_rrect(u, v, 0.25f, 0.25f, 0.75f, 0.75f, 0.06f)) {
                return !in_rrect(u, v, 0.36f, 0.36f, 0.64f, 0.64f, 0.03f);
            }
            for (float p : {0.35f, 0.5f, 0.65f}) {
                if (in_rect(u, v, p - 0.03f, 0.12f, p + 0.03f, 0.25f) || in_rect(u, v, p - 0.03f, 0.75f, p + 0.03f, 0.88f) ||
                    in_rect(u, v, 0.12f, p - 0.03f, 0.25f, p + 0.03f) || in_rect(u, v, 0.75f, p - 0.03f, 0.88f, p + 0.03f)) {
                    return true;
                }
            }
            return false;
        }
        case Icon::Clock:
            return in_ring(u, v, 0.5f, 0.5f, 0.31f, 0.41f) || in_seg(u, v, 0.5f, 0.5f, 0.5f, 0.27f, 0.04f) ||
                   in_seg(u, v, 0.5f, 0.5f, 0.66f, 0.58f, 0.04f);
        case Icon::Gear: {
            if (in_circle(u, v, 0.5f, 0.5f, 0.12f)) return false;
            if (in_circle(u, v, 0.5f, 0.5f, 0.30f)) return true;
            for (int k = 0; k < 8; k++) {
                float a = k * 0.785398f;
                if (in_circle(u, v, 0.5f + 0.34f * std::cos(a), 0.5f + 0.34f * std::sin(a), 0.085f)) return true;
            }
            return false;
        }
        case Icon::Picture:
            return (in_rrect(u, v, 0.10f, 0.20f, 0.90f, 0.80f, 0.06f) && !in_rrect(u, v, 0.17f, 0.27f, 0.83f, 0.73f, 0.03f)) ||
                   in_tri(u, v, 0.20f, 0.70f, 0.42f, 0.40f, 0.62f, 0.70f) ||
                   in_tri(u, v, 0.50f, 0.70f, 0.66f, 0.50f, 0.80f, 0.70f) || in_circle(u, v, 0.68f, 0.38f, 0.065f);
        case Icon::Doc:
            return icon_shape(Icon::File, u, v) && !in_rect(u, v, 0.32f, 0.38f, 0.68f, 0.43f) &&
                   !in_rect(u, v, 0.32f, 0.52f, 0.68f, 0.57f) && !in_rect(u, v, 0.32f, 0.66f, 0.58f, 0.71f);
        case Icon::Globe: {
            if (!in_circle(u, v, 0.5f, 0.5f, 0.41f)) return false;
            if (!in_circle(u, v, 0.5f, 0.5f, 0.33f)) return true;
            float dx = u - 0.5f, dy = v - 0.5f;
            float outer = dx * dx / (0.21f * 0.21f) + dy * dy / (0.41f * 0.41f);
            float inner = dx * dx / (0.13f * 0.13f) + dy * dy / (0.41f * 0.41f);
            return (outer <= 1 && inner >= 1) || std::fabs(dy) <= 0.04f;
        }
        case Icon::ChevronLeft:
            return in_seg(u, v, 0.62f, 0.18f, 0.32f, 0.5f, 0.08f) || in_seg(u, v, 0.32f, 0.5f, 0.62f, 0.82f, 0.08f);
        case Icon::ChevronRight:
            return in_seg(u, v, 0.38f, 0.18f, 0.68f, 0.5f, 0.08f) || in_seg(u, v, 0.68f, 0.5f, 0.38f, 0.82f, 0.08f);
        default:
            return false;
    }
}

uint32_t icon_pixel(float u, float v, void *ctx) {
    return icon_shape(*(Icon *) ctx, u, v) ? 0xFFFFFFFFu : 0u;
}

uint32_t circle_pixel(float u, float v, void *) {
    return in_circle(u, v, 0.5f, 0.5f, 0.5f) ? 0xFFFFFFFFu : 0u;
}

uint32_t shadow_pixel(float u, float v, void *) {
    float d = std::sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)) * 2.0f;
    float a = clampf(1.0f - d, 0, 1);
    a       = a * a * (3 - 2 * a);
    return 0xFFFFFF00u | (uint32_t) (a * 255.0f);
}

// The app logo: folder with an orange lock badge on a blue tile.
uint32_t logo_pixel(float u, float v, void *) {
    if (!in_rrect(u, v, 0, 0, 1, 1, 0.2f)) {
        return 0;
    }
    auto rgb = [](int r, int g, int b) { return ((uint32_t) r << 24) | ((uint32_t) g << 16) | ((uint32_t) b << 8) | 0xFF; };
    float d = std::sqrt((u - 0.70f) * (u - 0.70f) + (v - 0.70f) * (v - 0.70f));
    if (d <= 0.19f) {
        if (d >= 0.17f) return rgb(15, 30, 56);
        if (in_rrect(u, v, 0.615f, 0.675f, 0.785f, 0.80f, 0.02f)) return rgb(16, 31, 58);
        float ring = std::sqrt((u - 0.70f) * (u - 0.70f) + (v - 0.675f) * (v - 0.675f));
        if (v <= 0.675f && ring >= 0.038f && ring <= 0.062f) return rgb(16, 31, 58);
        return rgb(255, 165, 61);
    }
    if (in_rrect(u, v, 0.17f, 0.25f, 0.47f, 0.40f, 0.04f)) return rgb(196, 208, 226);
    if (in_rrect(u, v, 0.17f, 0.33f, 0.83f, 0.79f, 0.06f)) return rgb(232, 238, 247);
    int r = (int) (27 + (15 - 27) * v), g = (int) (58 + (30 - 58) * v), b = (int) (107 + (56 - 107) * v);
    return rgb(r, g, b);
}

void set_draw_color(SDL_Renderer *r, uint32_t rgba) {
    SDL_SetRenderDrawColor(r, rgba >> 24, (rgba >> 16) & 0xff, (rgba >> 8) & 0xff, rgba & 0xff);
}

void set_mod(SDL_Texture *t, uint32_t rgba) {
    SDL_SetTextureColorMod(t, rgba >> 24, (rgba >> 16) & 0xff, (rgba >> 8) & 0xff);
    SDL_SetTextureAlphaMod(t, rgba & 0xff);
}

} // namespace

SDL_Texture *Renderer::makeTexture(int size, uint32_t (*pixel)(float, float, void *), void *ctx, int samples) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) {
        return nullptr;
    }
    SDL_LockSurface(s);
    for (int y = 0; y < size; y++) {
        uint8_t *row = (uint8_t *) s->pixels + y * s->pitch;
        for (int x = 0; x < size; x++) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int sy = 0; sy < samples; sy++) {
                for (int sx = 0; sx < samples; sx++) {
                    float u    = (x + (sx + 0.5f) / samples) / size;
                    float v    = (y + (sy + 0.5f) / samples) / size;
                    uint32_t p = pixel(u, v, ctx);
                    float pa   = (p & 0xff) / 255.0f;
                    r += (p >> 24) * pa;
                    g += ((p >> 16) & 0xff) * pa;
                    b += ((p >> 8) & 0xff) * pa;
                    a += pa;
                }
            }
            uint8_t *px = row + x * 4; // RGBA32 is byte order R, G, B, A on every host
            if (a > 0) {
                px[0] = (uint8_t) (r / a);
                px[1] = (uint8_t) (g / a);
                px[2] = (uint8_t) (b / a);
            } else {
                px[0] = px[1] = px[2] = 255;
            }
            px[3] = (uint8_t) (a / (samples * samples) * 255.0f + 0.5f);
        }
    }
    SDL_UnlockSurface(s);
    SDL_Texture *t = SDL_CreateTextureFromSurface(sdl_, s);
    SDL_FreeSurface(s);
    if (t) {
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return t;
}

bool Renderer::init(SDL_Renderer *renderer) {
    sdl_ = renderer;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    circle_ = makeTexture(128, circle_pixel, nullptr, 4);
    shadow_ = makeTexture(128, shadow_pixel, nullptr, 1);
    logo_   = makeTexture(192, logo_pixel, nullptr, 3);
    for (int i = 0; i < (int) Icon::Count; i++) {
        Icon id    = (Icon) i;
        icons_[i] = makeTexture(96, icon_pixel, &id, 4);
    }
    return circle_ && shadow_ && logo_;
}

bool Renderer::loadFonts(int sharedFontType) {
    void *font    = nullptr;
    uint32_t size = 0;
    if (!OSGetSharedData((OSSharedDataType) sharedFontType, 0, &font, &size) || !font || !size) {
        LOG("shared font %d not available", sharedFontType);
        return false;
    }
    clearTextCache();
    for (int i = 0; i < F_COUNT; i++) {
        if (fonts_[i]) {
            TTF_CloseFont(fonts_[i]);
        }
        fonts_[i] = TTF_OpenFontRW(SDL_RWFromConstMem(font, (int) size), 1, kFontSizes[i]);
        if (!fonts_[i]) {
            LOG("TTF_OpenFontRW: %s", TTF_GetError());
            return false;
        }
        if (kFontBold[i]) {
            TTF_SetFontStyle(fonts_[i], TTF_STYLE_BOLD);
        }
    }
    fontType_ = sharedFontType;
    return true;
}

int Renderer::textInFont(int type, const std::string &s, int x, int y, uint32_t rgba, Align align) {
    if (type == fontType_ || type < 0 || type >= 4) {
        return text(F_BODY, s, x, y, rgba, align);
    }
    if (!altFonts_[type]) {
        void *font    = nullptr;
        uint32_t size = 0;
        if (!OSGetSharedData((OSSharedDataType) type, 0, &font, &size) || !font) {
            return text(F_BODY, s, x, y, rgba, align);
        }
        altFonts_[type] = TTF_OpenFontRW(SDL_RWFromConstMem(font, (int) size), 1, kFontSizes[F_BODY]);
        if (!altFonts_[type]) {
            return text(F_BODY, s, x, y, rgba, align);
        }
    }
    char prefix[24];
    snprintf(prefix, sizeof(prefix), "alt%d%08x|", type, (unsigned) rgba);
    std::string key = prefix + s;
    auto it         = cache_.find(key);
    if (it == cache_.end()) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(altFonts_[type], s.c_str(),
                                                   SDL_Color{(Uint8) (rgba >> 24), (Uint8) (rgba >> 16),
                                                             (Uint8) (rgba >> 8), (Uint8) rgba});
        if (!surf) {
            return 0;
        }
        CachedText ct;
        ct.tex = SDL_CreateTextureFromSurface(sdl_, surf);
        ct.w   = surf->w;
        ct.h   = surf->h;
        SDL_FreeSurface(surf);
        if (!ct.tex) {
            return 0;
        }
        it = cache_.emplace(key, ct).first;
    }
    CachedText &ct = it->second;
    ct.lastFrame   = frame_;
    int dx         = align == A_RIGHT ? x - ct.w : (align == A_CENTER ? x - ct.w / 2 : x);
    SDL_Rect dst{dx, y, ct.w, ct.h};
    SDL_RenderCopy(sdl_, ct.tex, nullptr, &dst);
    return ct.w;
}

void Renderer::clearTextCache() {
    for (auto &kv : cache_) {
        SDL_DestroyTexture(kv.second.tex);
    }
    cache_.clear();
    wrapCache_.clear();
}

void Renderer::shutdown() {
    clearTextCache();
    for (auto *&f : fonts_) {
        if (f) {
            TTF_CloseFont(f);
            f = nullptr;
        }
    }
    for (auto *&f : altFonts_) {
        if (f) {
            TTF_CloseFont(f);
            f = nullptr;
        }
    }
    for (SDL_Texture *t : {circle_, shadow_, logo_}) {
        if (t) {
            SDL_DestroyTexture(t);
        }
    }
    for (auto *&t : icons_) {
        if (t) {
            SDL_DestroyTexture(t);
            t = nullptr;
        }
    }
    circle_ = shadow_ = logo_ = nullptr;
}

void Renderer::endFrame() {
    frame_++;
    if (frame_ % 60 == 0) {
        for (auto it = cache_.begin(); it != cache_.end();) {
            if (frame_ - it->second.lastFrame > 180) {
                SDL_DestroyTexture(it->second.tex);
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// ---------------------------------------------------------------- shapes

void Renderer::fill(int x, int y, int w, int h, uint32_t rgba) {
    set_draw_color(sdl_, rgba);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(sdl_, &r);
}

void Renderer::gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    auto color = [](uint32_t c) { return SDL_Color{(Uint8) (c >> 24), (Uint8) (c >> 16), (Uint8) (c >> 8), (Uint8) c}; };
    SDL_Vertex v[4] = {
            {{(float) x, (float) y}, color(top), {0, 0}},
            {{(float) (x + w), (float) y}, color(top), {0, 0}},
            {{(float) (x + w), (float) (y + h)}, color(bottom), {0, 0}},
            {{(float) x, (float) (y + h)}, color(bottom), {0, 0}},
    };
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(sdl_, nullptr, v, 4, idx, 6);
}

// Draws a texture as a nine-slice: corners from its quadrants, edges and the
// center stretched from its middle row/column.
void Renderer::nine(SDL_Texture *tex, int x, int y, int w, int h, int c, uint32_t rgba) {
    if (!tex || w <= 0 || h <= 0) {
        return;
    }
    c = std::min({c, w / 2, h / 2});
    const int half = 64;
    set_mod(tex, rgba);
    auto copy = [&](int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
        if (dw <= 0 || dh <= 0) return;
        SDL_Rect src{sx, sy, sw, sh}, dst{dx, dy, dw, dh};
        SDL_RenderCopy(sdl_, tex, &src, &dst);
    };
    copy(0, 0, half, half, x, y, c, c);
    copy(half, 0, half, half, x + w - c, y, c, c);
    copy(0, half, half, half, x, y + h - c, c, c);
    copy(half, half, half, half, x + w - c, y + h - c, c, c);
    copy(half, 0, 1, half, x + c, y, w - 2 * c, c);
    copy(half, half, 1, half, x + c, y + h - c, w - 2 * c, c);
    copy(0, half, half, 1, x, y + c, c, h - 2 * c);
    copy(half, half, half, 1, x + w - c, y + c, c, h - 2 * c);
    copy(half, half, 1, 1, x + c, y + c, w - 2 * c, h - 2 * c);
}

void Renderer::round(int x, int y, int w, int h, int r, uint32_t rgba) {
    nine(circle_, x, y, w, h, r, rgba);
}

void Renderer::shadow(int x, int y, int w, int h, int r, int spread, uint8_t alpha) {
    nine(shadow_, x - spread, y - spread, w + 2 * spread, h + 2 * spread, r + spread, 0x1E2A3800u | alpha);
}

void Renderer::card(int x, int y, int w, int h, int r, uint32_t rgba) {
    shadow(x, y + 4, w, h, r, 10, 40);
    round(x, y, w, h, r, rgba);
}

void Renderer::icon(Icon icon, int x, int y, int size, uint32_t rgba) {
    SDL_Texture *t = icons_[(int) icon];
    if (!t) {
        return;
    }
    set_mod(t, rgba);
    SDL_Rect dst{x, y, size, size};
    SDL_RenderCopy(sdl_, t, nullptr, &dst);
}

void Renderer::logo(int x, int y, int size) {
    SDL_Rect dst{x, y, size, size};
    SDL_SetTextureColorMod(logo_, 255, 255, 255);
    SDL_SetTextureAlphaMod(logo_, 255);
    SDL_RenderCopy(sdl_, logo_, nullptr, &dst);
}

int Renderer::button(const char *label, int x, int y, int size) {
    int len = 0;
    while (label[len]) len++;
    // Shoulder buttons are wider pills, face buttons circles, longer labels
    // (e.g. the D-pad arrows) pills sized to fit.
    int w = size;
    if (len == 1 && (label[0] == 'L' || label[0] == 'R')) {
        w = size * 3 / 2;
    } else if (len > 1) {
        w = std::max(size, measure(F_SMALL, label) + 16);
    }
    round(x, y, w, size, size / 2, 0x50565EFF);
    int th = lineHeight(F_SMALL);
    text(F_SMALL, label, x + w / 2, y + (size - th) / 2, 0xFFFFFFFF, A_CENTER);
    return w;
}

// ---------------------------------------------------------------- text

int Renderer::lineHeight(Font f) {
    return fonts_[f] ? TTF_FontHeight(fonts_[f]) : kFontSizes[f];
}

int Renderer::measure(Font f, const std::string &s) {
    int w = 0, h = 0;
    if (s.empty() || !fonts_[f] || TTF_SizeUTF8(fonts_[f], s.c_str(), &w, &h) != 0) {
        return 0;
    }
    return w;
}

std::string Renderer::fit(Font f, const std::string &s, int maxWidth, bool keepEnd) {
    if (measure(f, s) <= maxWidth) {
        return s;
    }
    std::vector<size_t> cuts; // UTF-8 character boundaries
    for (size_t i = 1; i < s.size(); i++) {
        if (((unsigned char) s[i] & 0xC0) != 0x80) {
            cuts.push_back(i);
        }
    }
    const std::string ell = "…";
    auto candidate        = [&](size_t i) { return keepEnd ? ell + s.substr(cuts[i]) : s.substr(0, cuts[i]) + ell; };
    size_t lo = 0, hi = cuts.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        bool fits  = measure(f, candidate(mid)) <= maxWidth;
        if (keepEnd ? fits : !fits) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if (keepEnd) {
        return lo < cuts.size() ? candidate(lo) : ell;
    }
    return lo > 0 ? candidate(lo - 1) : ell;
}

// Greedy line breaking: words first, single characters when a word (or a
// whole CJK sentence) does not fit.
std::vector<std::string> Renderer::wrap(Font f, const std::string &s, int maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    auto flushWord = [&](const std::string &word) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (measure(f, candidate) <= maxWidth) {
            line = candidate;
            return;
        }
        if (!line.empty()) {
            lines.push_back(line);
            line.clear();
        }
        size_t start = 0;
        while (start < word.size()) {
            size_t end = start, best = start;
            while (end < word.size()) {
                size_t next = end + 1;
                while (next < word.size() && ((unsigned char) word[next] & 0xC0) == 0x80) next++;
                if (measure(f, word.substr(start, next - start)) > maxWidth && best > start) break;
                best = end = next;
            }
            std::string piece = word.substr(start, best - start);
            start             = best;
            if (start < word.size()) {
                lines.push_back(piece);
            } else {
                line = piece;
            }
        }
    };
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t sp = s.find(' ', pos);
        if (sp == std::string::npos) sp = s.size();
        if (sp > pos) flushWord(s.substr(pos, sp - pos));
        pos = sp + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

int Renderer::textWrapped(Font f, const std::string &s, int x, int y, int maxWidth, int maxLines, int lineStep,
                          uint32_t rgba, Align align) {
    char prefix[24];
    snprintf(prefix, sizeof(prefix), "%d|%d|", (int) f, maxWidth);
    std::string key = prefix + s;
    auto cached     = wrapCache_.find(key);
    if (cached == wrapCache_.end()) {
        if (wrapCache_.size() > 256) {
            wrapCache_.clear();
        }
        cached = wrapCache_.emplace(key, wrap(f, s, maxWidth)).first;
    }
    const std::vector<std::string> &lines = cached->second;
    int drawn                             = 0;
    for (size_t i = 0; i < lines.size() && drawn < maxLines; i++, drawn++) {
        std::string shown = lines[i];
        if (drawn == maxLines - 1 && i + 1 < lines.size()) {
            for (size_t k = i + 1; k < lines.size(); k++) shown += " " + lines[k];
            shown = fit(f, shown, maxWidth, false);
        }
        text(f, shown, x, y + drawn * lineStep, rgba, align);
    }
    return drawn;
}

int Renderer::text(Font f, const std::string &s, int x, int y, uint32_t rgba, Align align, int maxWidth, bool keepEnd) {
    if (s.empty() || !fonts_[f]) {
        return 0;
    }
    std::string shown = maxWidth > 0 ? fit(f, s, maxWidth, keepEnd) : s;
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%d%08x|", (int) f, (unsigned) rgba);
    std::string key = prefix + shown;
    auto it         = cache_.find(key);
    if (it == cache_.end()) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(fonts_[f], shown.c_str(),
                                                   SDL_Color{(Uint8) (rgba >> 24), (Uint8) (rgba >> 16),
                                                             (Uint8) (rgba >> 8), (Uint8) rgba});
        if (!surf) {
            return 0;
        }
        CachedText ct;
        ct.tex = SDL_CreateTextureFromSurface(sdl_, surf);
        ct.w   = surf->w;
        ct.h   = surf->h;
        SDL_FreeSurface(surf);
        if (!ct.tex) {
            return 0;
        }
        it = cache_.emplace(key, ct).first;
    }
    CachedText &ct = it->second;
    ct.lastFrame   = frame_;
    int dx         = align == A_RIGHT ? x - ct.w : (align == A_CENTER ? x - ct.w / 2 : x);
    SDL_Rect dst{dx, y, ct.w, ct.h};
    SDL_RenderCopy(sdl_, ct.tex, nullptr, &dst);
    return ct.w;
}
