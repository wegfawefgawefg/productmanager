#include "capture.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool save_frame_png(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* frame = SDL_RenderReadPixels(renderer, nullptr);
    if (!frame) {
        SDL_Log("read pixels failed: %s", SDL_GetError());
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(frame, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(frame);
    if (!rgba) {
        return false;
    }
    int ok = stbi_write_png(path.c_str(), rgba->w, rgba->h, 4, rgba->pixels, rgba->pitch);
    SDL_DestroySurface(rgba);
    return ok != 0;
}
