#pragma once

#include <SDL3/SDL.h>
#include <string>

// save the current render target as a PNG (for checking the look headless)
bool save_frame_png(SDL_Renderer* renderer, const std::string& path);
