#include "text.hpp"

#include <cstdio>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static const int atlas_size = 512;

static std::vector<unsigned char> read_bytes(const std::string& path) {
    std::vector<unsigned char> out;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return out;
    }
    fseek(f, 0, SEEK_END);
    out.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    size_t got = fread(out.data(), 1, out.size(), f);
    out.resize(got);
    fclose(f);
    return out;
}

bool load_font(SDL_Renderer* renderer, Font& font, const std::string& path, float size) {
    std::vector<unsigned char> ttf = read_bytes(path);
    if (ttf.empty()) {
        SDL_Log("font missing: %s", path.c_str());
        return false;
    }
    std::vector<unsigned char> alpha(atlas_size * atlas_size);
    stbtt_pack_context pack;
    stbtt_PackBegin(&pack, alpha.data(), atlas_size, atlas_size, 0, 1, nullptr);
    stbtt_PackSetOversampling(&pack, 2, 2);
    stbtt_PackFontRange(&pack, ttf.data(), 0, size, 32, 96, font.chars);
    stbtt_PackEnd(&pack);

    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf.data(), 0);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    font.ascent = ascent * stbtt_ScaleForPixelHeight(&info, size);
    font.size = size;

    // white pixels carrying the glyph coverage as alpha, so color mod tints it
    std::vector<uint32_t> rgba(atlas_size * atlas_size);
    for (int i = 0; i < atlas_size * atlas_size; i++) {
        rgba[i] = ((uint32_t)alpha[i] << 24) | 0x00ffffff;
    }
    SDL_Surface* surface = SDL_CreateSurfaceFrom(atlas_size, atlas_size, SDL_PIXELFORMAT_ARGB8888,
                                                 rgba.data(), atlas_size * 4);
    font.atlas = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    SDL_SetTextureBlendMode(font.atlas, SDL_BLENDMODE_BLEND);
    return font.atlas != nullptr;
}

float text_width(const Font& font, const std::string& text) {
    float x = 0;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) {
            c = '?';
        }
        x += font.chars[c - 32].xadvance;
    }
    return x;
}

std::string fit_text(const Font& font, const std::string& text, float max_w) {
    if (text_width(font, text) <= max_w) {
        return text;
    }
    std::string out = text;
    while (!out.empty() && text_width(font, out + "..") > max_w) {
        out.pop_back();
    }
    return out + "..";
}

void draw_text(SDL_Renderer* renderer, const Font& font, float x, float y, const std::string& text,
               SDL_Color color) {
    if (!font.atlas) {
        return;
    }
    SDL_SetTextureColorMod(font.atlas, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(font.atlas, color.a);
    float pen_x = x;
    float pen_y = y + font.ascent;
    for (unsigned char c : text) {
        if (c < 32 || c >= 128) {
            c = '?';
        }
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(font.chars, atlas_size, atlas_size, c - 32, &pen_x, &pen_y, &q, 0);
        SDL_FRect src = {q.s0 * atlas_size, q.t0 * atlas_size, (q.s1 - q.s0) * atlas_size,
                         (q.t1 - q.t0) * atlas_size};
        SDL_FRect dst = {q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0};
        SDL_RenderTexture(renderer, font.atlas, &src, &dst);
    }
}

std::vector<std::string> wrap_text(const Font& font, const std::string& text, float max_w,
                                   int max_lines) {
    std::vector<std::string> lines;
    std::string line;
    size_t i = 0;
    while (i < text.size()) {
        size_t space = text.find(' ', i);
        std::string word = text.substr(i, space == std::string::npos ? std::string::npos : space - i);
        i = space == std::string::npos ? text.size() : space + 1;
        std::string tried = line.empty() ? word : line + " " + word;
        if (text_width(font, tried) <= max_w || line.empty()) {
            line = fit_text(font, tried, max_w);
            continue;
        }
        lines.push_back(line);
        line = word;
        if ((int)lines.size() == max_lines) {
            lines.back() = fit_text(font, lines.back() + " " + word + "...", max_w);
            return lines;
        }
    }
    if (!line.empty()) {
        lines.push_back(fit_text(font, line, max_w));
    }
    return lines;
}
