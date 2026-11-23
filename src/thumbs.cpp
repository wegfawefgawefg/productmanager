#include "thumbs.hpp"

#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#include "stb_image.h"

struct Thumb {
    SDL_Renderer* renderer = nullptr; // textures belong to one renderer
    std::string path;
    SDL_Texture* texture = nullptr; // nullptr when loading failed
    double used_at = 0;
};

static std::vector<Thumb> cache;
static const size_t cache_max = 48;
// longest edge kept; big screenshots are shrunk on load
static const int thumb_edge = 480;

static SDL_Texture* load_thumb(SDL_Renderer* renderer, const std::string& path) {
    int w, h, n;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!pixels) {
        return nullptr;
    }
    SDL_Surface* full =
        SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * 4);
    float scale = (float)thumb_edge / (float)(w > h ? w : h);
    SDL_Texture* texture = nullptr;
    if (scale < 1.0f) {
        SDL_Surface* small = SDL_ScaleSurface(full, (int)(w * scale) + 1, (int)(h * scale) + 1,
                                              SDL_SCALEMODE_LINEAR);
        texture = SDL_CreateTextureFromSurface(renderer, small);
        SDL_DestroySurface(small);
    } else {
        texture = SDL_CreateTextureFromSurface(renderer, full);
    }
    SDL_DestroySurface(full);
    stbi_image_free(pixels);
    return texture;
}

SDL_Texture* thumb_for(SDL_Renderer* renderer, const std::string& path, double now) {
    for (Thumb& thumb : cache) {
        if (thumb.renderer == renderer && thumb.path == path) {
            thumb.used_at = now;
            return thumb.texture;
        }
    }
    if (cache.size() >= cache_max) {
        size_t oldest = 0;
        for (size_t i = 1; i < cache.size(); i++) {
            if (cache[i].used_at < cache[oldest].used_at) {
                oldest = i;
            }
        }
        if (cache[oldest].texture) {
            SDL_DestroyTexture(cache[oldest].texture);
        }
        cache.erase(cache.begin() + (long)oldest);
    }
    Thumb thumb;
    thumb.renderer = renderer;
    thumb.path = path;
    thumb.texture = load_thumb(renderer, path);
    thumb.used_at = now;
    cache.push_back(thumb);
    return thumb.texture;
}

void forget_thumbs(SDL_Renderer* renderer) {
    for (Thumb& thumb : cache) {
        if (thumb.renderer == renderer && thumb.texture) {
            SDL_DestroyTexture(thumb.texture);
        }
    }
    std::erase_if(cache, [renderer](const Thumb& t) { return t.renderer == renderer; });
}
