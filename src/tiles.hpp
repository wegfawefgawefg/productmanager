#pragma once

#include <SDL3/SDL.h>

#include "state.hpp"
#include "text.hpp"

struct Fonts {
    Font name; // bold, chat names
    Font small; // details
    Font tiny; // times, chips
};

std::string elapsed_text(double seconds);
std::string tokens_text(int64_t tokens);

// lay out and draw every chat; sets state.win_w / win_h to the size the
// content wants and refills state.hits for clicks
void draw_tiles(SDL_Renderer* r, State& state, const Fonts& fonts);
