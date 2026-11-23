#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "state.hpp"
#include "text.hpp"

struct CrewFonts {
    Font title; // leader name
    Font name; // agent descriptions
    Font body; // actions, remarks
    Font tiny; // stats, tags
};

// what a card needs besides the agent itself
struct CardContext {
    double now = 0;
    std::string parent_name; // empty for agents the leader launched
    std::string clash; // another live agent editing the same file
    bool hover = false;
};

// colored barcode of every tool call between t0 and t1; failed calls get a
// red mark on top
void draw_history(SDL_Renderer* r, const Tail& tail, float x, float y, float w, float h, double t0,
                  double t1, double now);

// a working agent: big picture, current action, last remark, history
void draw_live_card(SDL_Renderer* r, const CrewFonts& fonts, const Agent& agent,
                    const CardContext& ctx, float x, float y, float w, float h);

// the chat's leader as a tile: name, what it does, its last remark, the
// agents that finished this hour, and its last hour as a barcode
void draw_leader_card(SDL_Renderer* r, const CrewFonts& fonts, const Chat& chat,
                      const std::vector<const Agent*>& finished, int working, double now,
                      bool hover, float x, float y, float w, float h);

// "why this card is amber": failures in a row, a long silence, a shared file
std::string trouble_text(const Agent& agent, const CardContext& ctx);
