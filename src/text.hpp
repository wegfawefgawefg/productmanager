#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "stb_truetype.h"

// a font baked once into a texture atlas; ASCII only
struct Font {
    SDL_Texture* atlas = nullptr;
    stbtt_packedchar chars[96];
    float size = 0;
    float ascent = 0;
};

bool load_font(SDL_Renderer* renderer, Font& font, const std::string& path, float size);
float text_width(const Font& font, const std::string& text);
// cut the text to fit max_w, ending with ".." when it had to be cut
std::string fit_text(const Font& font, const std::string& text, float max_w);
// break text into lines no wider than max_w; the last kept line ends ".."
// when text was left over
std::vector<std::string> wrap_text(const Font& font, const std::string& text, float max_w,
                                   int max_lines);
// y is the top of the line
void draw_text(SDL_Renderer* renderer, const Font& font, float x, float y, const std::string& text,
               SDL_Color color);
