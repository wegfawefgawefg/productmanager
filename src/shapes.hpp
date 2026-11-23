#pragma once

#include <SDL3/SDL.h>

// thick lines and round things on top of SDL_RenderGeometry
void fill_rect(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c);
void line(SDL_Renderer* r, float x0, float y0, float x1, float y1, float width, SDL_Color c);
void ring(SDL_Renderer* r, float cx, float cy, float radius, float width, SDL_Color c);
void disc(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c);
void rounded_rect(SDL_Renderer* r, float x, float y, float w, float h, float radius, SDL_Color c);
void rect_outline(SDL_Renderer* r, float x, float y, float w, float h, float width, SDL_Color c);

SDL_Color with_alpha(SDL_Color c, float alpha);
SDL_Color mix(SDL_Color a, SDL_Color b, float t);
