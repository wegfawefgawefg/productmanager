#include "shapes.hpp"

#include "look.hpp"

#include <cmath>
#include <vector>

static SDL_FColor to_f(SDL_Color c) {
    return SDL_FColor{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
}

SDL_Color with_alpha(SDL_Color c, float alpha) {
    c.a = (Uint8)(c.a * alpha);
    return c;
}

SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
    auto m = [t](Uint8 x, Uint8 y) { return (Uint8)(x + (y - x) * t); };
    return SDL_Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

void fill_rect(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void line(SDL_Renderer* r, float x0, float y0, float x1, float y1, float width, SDL_Color c) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) {
        return;
    }
    // quad around the segment, plus round-ish caps from discs
    float nx = -dy / len * width * 0.5f;
    float ny = dx / len * width * 0.5f;
    SDL_FColor fc = to_f(c);
    SDL_Vertex v[4] = {
        {{x0 + nx, y0 + ny}, fc, {0, 0}},
        {{x1 + nx, y1 + ny}, fc, {0, 0}},
        {{x1 - nx, y1 - ny}, fc, {0, 0}},
        {{x0 - nx, y0 - ny}, fc, {0, 0}},
    };
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
    if (width >= 2.5f) {
        disc(r, x0, y0, width * 0.5f, c);
        disc(r, x1, y1, width * 0.5f, c);
    }
}

void ring(SDL_Renderer* r, float cx, float cy, float radius, float width, SDL_Color c) {
    const int steps = 32;
    SDL_FColor fc = to_f(c);
    std::vector<SDL_Vertex> v;
    std::vector<int> idx;
    float inner = radius - width * 0.5f;
    float outer = radius + width * 0.5f;
    for (int i = 0; i <= steps; i++) {
        float a = (float)i / steps * 6.2831853f;
        float ca = std::cos(a);
        float sa = std::sin(a);
        v.push_back({{cx + ca * inner, cy + sa * inner}, fc, {0, 0}});
        v.push_back({{cx + ca * outer, cy + sa * outer}, fc, {0, 0}});
    }
    for (int i = 0; i < steps; i++) {
        int a = i * 2;
        idx.insert(idx.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
    }
    SDL_RenderGeometry(r, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void disc(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c) {
    const int steps = 24;
    SDL_FColor fc = to_f(c);
    SDL_Vertex v[steps + 1];
    int idx[steps * 3];
    v[0] = {{cx, cy}, fc, {0, 0}};
    for (int i = 0; i < steps; i++) {
        float a = (float)i / steps * 6.2831853f;
        v[i + 1] = {{cx + std::cos(a) * radius, cy + std::sin(a) * radius}, fc, {0, 0}};
        idx[i * 3 + 0] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = (i + 1) % steps + 1;
    }
    SDL_RenderGeometry(r, nullptr, v, steps + 1, idx, steps * 3);
}

void rounded_rect(SDL_Renderer* r, float x, float y, float w, float h, float radius, SDL_Color c) {
    fill_rect(r, x + radius, y, w - radius * 2, h, c);
    fill_rect(r, x, y + radius, radius, h - radius * 2, c);
    fill_rect(r, x + w - radius, y + radius, radius, h - radius * 2, c);
    disc(r, x + radius, y + radius, radius, c);
    disc(r, x + w - radius, y + radius, radius, c);
    disc(r, x + radius, y + h - radius, radius, c);
    disc(r, x + w - radius, y + h - radius, radius, c);
}

void rect_outline(SDL_Renderer* r, float x, float y, float w, float h, float width, SDL_Color c) {
    fill_rect(r, x, y, w, width, c);
    fill_rect(r, x, y + h - width, w, width, c);
    fill_rect(r, x, y + width, width, h - width * 2, c);
    fill_rect(r, x + w - width, y + width, width, h - width * 2, c);
}

void look_box(SDL_Renderer* r, float x, float y, float w, float h, bool hover) {
    fill_rect(r, x, y, w, h, hover ? look::bg_hover : look::bg);
    rect_outline(r, x, y, w, h, 1, hover ? look::text : look::line);
}
