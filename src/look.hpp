#pragma once

#include <SDL3/SDL.h>

// the look: black, white text, grey for the quiet parts, 1px borders,
// square corners, monospace
namespace look {
inline const SDL_Color bg = {0, 0, 0, 255};
inline const SDL_Color bg_hover = {24, 24, 24, 255};
inline const SDL_Color text = {255, 255, 255, 255};
inline const SDL_Color body = {221, 221, 221, 255};
inline const SDL_Color dim = {153, 153, 153, 255};
inline const SDL_Color line = {85, 85, 85, 255};
inline const SDL_Color faint = {51, 51, 51, 255};
// the one warning color, for agents in trouble
inline const SDL_Color warn = {255, 170, 40, 255};
inline const SDL_Color fail = {255, 70, 70, 255};
} // namespace look

// a square box: black fill, grey 1px border, white border and lifted fill on hover
void look_box(SDL_Renderer* r, float x, float y, float w, float h, bool hover);
