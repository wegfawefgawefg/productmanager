#pragma once

#include <SDL3/SDL.h>
#include <string>

// small cache of image textures for the "looking at" thumbnails; returns
// nullptr when the file can't be loaded
SDL_Texture* thumb_for(SDL_Renderer* renderer, const std::string& path, double now);
// drop every texture made for this renderer, before it is destroyed
void forget_thumbs(SDL_Renderer* renderer);
