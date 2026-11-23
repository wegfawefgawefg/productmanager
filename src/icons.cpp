#include "icons.hpp"

#include <algorithm>
#include <cmath>

#include "shapes.hpp"

SDL_Color act_ink(Act act) {
    if (act == Act::Idle || act == Act::Done || act == Act::Waiting) {
        return {153, 153, 153, 255};
    }
    return {255, 255, 255, 255};
}

SDL_Color act_color(Act act) {
    switch (act) {
    case Act::Thinking:
        return {110, 110, 110, 255};
    case Act::Reading:
        return {110, 150, 190, 255};
    case Act::Searching:
        return {100, 170, 170, 255};
    case Act::Editing:
        return {130, 180, 120, 255};
    case Act::Running:
        return {200, 140, 90, 255};
    case Act::Building:
        return {200, 175, 90, 255};
    case Act::Looking:
        return {190, 120, 160, 255};
    case Act::Web:
        return {90, 170, 150, 255};
    case Act::Spawning:
        return {160, 140, 190, 255};
    case Act::Talking:
        return {140, 160, 190, 255};
    case Act::Publishing:
        return {160, 180, 100, 255};
    case Act::Waiting:
        return {70, 70, 70, 255};
    case Act::NeedsYou:
        return {255, 255, 255, 255};
    case Act::Idle:
        return {51, 51, 51, 255};
    case Act::Done:
        return {85, 85, 85, 255};
    }
    return {200, 200, 200, 255};
}

// all glyphs are drawn in a 0..1 box and scaled by s from (x, y)
struct Pen {
    SDL_Renderer* r;
    float x, y, s;
    SDL_Color c;
    void seg(float x0, float y0, float x1, float y1, float w) const {
        line(r, x + x0 * s, y + y0 * s, x + x1 * s, y + y1 * s, w * s, c);
    }
    void circle(float cx, float cy, float rad, float w) const {
        ring(r, x + cx * s, y + cy * s, rad * s, w * s, c);
    }
    void dot(float cx, float cy, float rad) const {
        disc(r, x + cx * s, y + cy * s, rad * s, c);
    }
    void box(float x0, float y0, float w, float h, float lw) const {
        rect_outline(r, x + x0 * s, y + y0 * s, w * s, h * s, lw * s, c);
    }
};

static void icon_page(const Pen& p) {
    p.box(0.22f, 0.12f, 0.56f, 0.76f, 0.07f);
    for (int i = 0; i < 4; i++) {
        float y = 0.3f + i * 0.13f;
        p.seg(0.33f, y, i == 3 ? 0.52f : 0.67f, y, 0.05f);
    }
}

static void icon_magnifier(const Pen& p) {
    p.circle(0.42f, 0.42f, 0.22f, 0.08f);
    p.seg(0.58f, 0.58f, 0.82f, 0.82f, 0.11f);
}

static void icon_pencil(const Pen& p, double t) {
    float wob = (float)std::sin(t * 9.0) * 0.03f;
    p.seg(0.25f + wob, 0.75f, 0.72f + wob, 0.28f, 0.16f);
    p.seg(0.2f + wob, 0.8f, 0.25f + wob, 0.75f, 0.08f);
    p.seg(0.12f, 0.9f, 0.6f, 0.9f, 0.05f);
}

static void icon_terminal(const Pen& p, double t) {
    p.box(0.1f, 0.18f, 0.8f, 0.64f, 0.06f);
    p.seg(0.24f, 0.38f, 0.36f, 0.5f, 0.06f);
    p.seg(0.36f, 0.5f, 0.24f, 0.62f, 0.06f);
    if (std::fmod(t, 1.0) < 0.6) {
        p.seg(0.45f, 0.64f, 0.62f, 0.64f, 0.06f);
    }
}

static void icon_gear(const Pen& p, double t) {
    float spin = (float)t * 1.5f;
    for (int i = 0; i < 8; i++) {
        float a = spin + i * 0.785398f;
        p.seg(0.5f + std::cos(a) * 0.3f, 0.5f + std::sin(a) * 0.3f, 0.5f + std::cos(a) * 0.43f,
              0.5f + std::sin(a) * 0.43f, 0.13f);
    }
    p.circle(0.5f, 0.5f, 0.25f, 0.11f);
    p.dot(0.5f, 0.5f, 0.07f);
}

static void icon_eye(const Pen& p) {
    p.seg(0.1f, 0.5f, 0.3f, 0.33f, 0.06f);
    p.seg(0.3f, 0.33f, 0.7f, 0.33f, 0.06f);
    p.seg(0.7f, 0.33f, 0.9f, 0.5f, 0.06f);
    p.seg(0.1f, 0.5f, 0.3f, 0.67f, 0.06f);
    p.seg(0.3f, 0.67f, 0.7f, 0.67f, 0.06f);
    p.seg(0.7f, 0.67f, 0.9f, 0.5f, 0.06f);
    p.dot(0.5f, 0.5f, 0.11f);
}

static void icon_globe(const Pen& p, double t) {
    p.circle(0.5f, 0.5f, 0.34f, 0.06f);
    p.seg(0.16f, 0.5f, 0.84f, 0.5f, 0.05f);
    float sway = (float)std::sin(t * 2.0) * 0.14f;
    p.seg(0.5f + sway, 0.17f, 0.5f - sway, 0.83f, 0.05f);
    p.seg(0.24f, 0.32f, 0.76f, 0.32f, 0.04f);
    p.seg(0.24f, 0.68f, 0.76f, 0.68f, 0.04f);
}

static void icon_spawn(const Pen& p) {
    p.dot(0.5f, 0.22f, 0.1f);
    p.seg(0.5f, 0.3f, 0.22f, 0.72f, 0.05f);
    p.seg(0.5f, 0.3f, 0.5f, 0.72f, 0.05f);
    p.seg(0.5f, 0.3f, 0.78f, 0.72f, 0.05f);
    p.dot(0.22f, 0.76f, 0.08f);
    p.dot(0.5f, 0.76f, 0.08f);
    p.dot(0.78f, 0.76f, 0.08f);
}

static void icon_envelope(const Pen& p) {
    p.box(0.14f, 0.26f, 0.72f, 0.48f, 0.06f);
    p.seg(0.16f, 0.3f, 0.5f, 0.54f, 0.06f);
    p.seg(0.5f, 0.54f, 0.84f, 0.3f, 0.06f);
}

static void icon_upload(const Pen& p) {
    p.seg(0.5f, 0.22f, 0.5f, 0.66f, 0.08f);
    p.seg(0.5f, 0.2f, 0.3f, 0.4f, 0.08f);
    p.seg(0.5f, 0.2f, 0.7f, 0.4f, 0.08f);
    p.seg(0.2f, 0.82f, 0.8f, 0.82f, 0.07f);
}

static void icon_hourglass(const Pen& p) {
    p.seg(0.28f, 0.16f, 0.72f, 0.16f, 0.07f);
    p.seg(0.28f, 0.84f, 0.72f, 0.84f, 0.07f);
    p.seg(0.32f, 0.18f, 0.68f, 0.82f, 0.06f);
    p.seg(0.68f, 0.18f, 0.32f, 0.82f, 0.06f);
}

static void icon_dots(const Pen& p, double t) {
    for (int i = 0; i < 3; i++) {
        float lift = (float)std::max(0.0, std::sin(t * 5.0 - i * 0.9)) * 0.12f;
        p.dot(0.25f + i * 0.25f, 0.55f - lift, 0.08f);
    }
}

static void icon_bang(const Pen& p, double t) {
    float pulse = 0.9f + 0.1f * (float)std::sin(t * 4.0);
    p.circle(0.5f, 0.5f, 0.36f * pulse, 0.07f);
    p.seg(0.5f, 0.3f, 0.5f, 0.56f, 0.1f);
    p.dot(0.5f, 0.7f, 0.06f);
}

static void icon_moon(const Pen& p) {
    p.seg(0.32f, 0.36f, 0.52f, 0.36f, 0.05f);
    p.seg(0.52f, 0.36f, 0.32f, 0.6f, 0.05f);
    p.seg(0.32f, 0.6f, 0.52f, 0.6f, 0.05f);
    p.seg(0.58f, 0.2f, 0.7f, 0.2f, 0.04f);
    p.seg(0.7f, 0.2f, 0.58f, 0.34f, 0.04f);
    p.seg(0.58f, 0.34f, 0.7f, 0.34f, 0.04f);
}

static void icon_check(const Pen& p) {
    p.seg(0.22f, 0.52f, 0.42f, 0.72f, 0.1f);
    p.seg(0.42f, 0.72f, 0.8f, 0.3f, 0.1f);
}

void draw_icon(SDL_Renderer* r, Act act, float x, float y, float size, double t) {
    Pen p{r, x, y, size, act_ink(act)};
    switch (act) {
    case Act::Thinking:
        icon_dots(p, t);
        break;
    case Act::Reading:
        icon_page(p);
        break;
    case Act::Searching:
        icon_magnifier(p);
        break;
    case Act::Editing:
        icon_pencil(p, t);
        break;
    case Act::Running:
        icon_terminal(p, t);
        break;
    case Act::Building:
        icon_gear(p, t);
        break;
    case Act::Looking:
        icon_eye(p);
        break;
    case Act::Web:
        icon_globe(p, t);
        break;
    case Act::Spawning:
        icon_spawn(p);
        break;
    case Act::Talking:
        icon_envelope(p);
        break;
    case Act::Publishing:
        icon_upload(p);
        break;
    case Act::Waiting:
        icon_hourglass(p);
        break;
    case Act::NeedsYou:
        icon_bang(p, t);
        break;
    case Act::Idle:
        icon_moon(p);
        break;
    case Act::Done:
        icon_check(p);
        break;
    }
}
