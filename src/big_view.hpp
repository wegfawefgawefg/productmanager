#pragma once

#include <SDL3/SDL.h>
#include <vector>

#include "crew_card.hpp"
#include "state.hpp"

// a big viewer: the chats it shows, each as its leader tile plus a tile per
// working agent, the tiles filling the whole window
struct BigView {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    CrewFonts fonts[3]; // picked by tile size: normal, large, huge
    CrewFonts small_fonts;
    std::vector<int> shown; // chat pids; empty shows every working chat
    std::vector<HitBox> hits;
    float mouse_x = -1;
    float mouse_y = -1;
};

bool open_big(BigView& big, const char* title);
void close_big(BigView& big);
// shown chats keep their finished agents for an hour
void mark_shown_chats(const BigView& big, State& state);
// draws into the viewer's renderer; the caller presents
void draw_big(BigView& big, State& state);
// mouse events: a click on a tile jumps to its chat
void big_event(BigView& big, State& state, const SDL_Event& event);
