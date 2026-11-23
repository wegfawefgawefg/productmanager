#pragma once

#include <SDL3/SDL.h>

#include "state.hpp"

// muted color per activity, for history barcodes and feed squares only
SDL_Color act_color(Act act);
// glyph and text color: white while working, grey when idle or done
SDL_Color act_ink(Act act);
// a glyph for the activity inside the square x,y,size; t animates
void draw_icon(SDL_Renderer* r, Act act, float x, float y, float size, double t);
