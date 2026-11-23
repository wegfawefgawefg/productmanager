#include "tiles.hpp"

#include <algorithm>
#include <cmath>

#include "icons.hpp"
#include "look.hpp"
#include "shapes.hpp"
#include "thumbs.hpp"
#include "transcript.hpp"

static const float pad = 6;
static const float gap = 4;
static const float leader_w = 250;
static const float agent_w = 190;
static const float tile_h = 58;
static const int agents_per_line = 4;
static const float chip_h = 20;
static const float footer_h = 18;
// an image stays as the tile's picture this long after it was read
static const double image_show_for = 25;

static const SDL_Color text_main = look::text;
static const SDL_Color text_dim = look::dim;
static const SDL_Color window_bg = look::bg;

static std::string upper(std::string s) {
    for (char& c : s) {
        c = (char)toupper((unsigned char)c);
    }
    return s;
}

std::string tokens_text(int64_t tokens) {
    char buf[32];
    if (tokens < 1000) {
        snprintf(buf, sizeof buf, "%d", (int)tokens);
    } else if (tokens < 1000000) {
        snprintf(buf, sizeof buf, "%.0fk", tokens / 1000.0);
    } else {
        snprintf(buf, sizeof buf, "%.1fM", tokens / 1000000.0);
    }
    return buf;
}

std::string elapsed_text(double seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    char buf[32];
    if (seconds < 60) {
        snprintf(buf, sizeof buf, "%ds", (int)seconds);
    } else if (seconds < 3600) {
        snprintf(buf, sizeof buf, "%dm", (int)(seconds / 60));
    } else if (seconds < 86400) {
        snprintf(buf, sizeof buf, "%dh", (int)(seconds / 3600));
    } else {
        snprintf(buf, sizeof buf, "%dd", (int)(seconds / 86400));
    }
    return buf;
}

static std::string act_line(const Tail& tail) {
    if (tail.detail.empty()) {
        return act_name(tail.act);
    }
    return std::string(act_name(tail.act)) + ": " + tail.detail;
}

static bool is_busy(Act act) {
    return act != Act::Idle && act != Act::NeedsYou && act != Act::Done && act != Act::Waiting;
}

// the crew window keeps finished agents for an hour; the strip only briefly
static bool shows_in_strip(const Agent& agent, double now) {
    return !agent.finished || now - agent.finished_at < 90;
}

static int strip_agent_count(const Chat& chat, double now) {
    int n = 0;
    for (const Agent& agent : chat.agents) {
        n += shows_in_strip(agent, now) ? 1 : 0;
    }
    return n;
}

static bool is_quiet(const Chat& chat, double now) {
    return chat.tail.act == Act::Idle && strip_agent_count(chat, now) == 0;
}

static bool hovered(const State& state, float x, float y, float w, float h) {
    return state.mouse_x >= x && state.mouse_x < x + w && state.mouse_y >= y &&
           state.mouse_y < y + h;
}

static bool has_viewer(const State& state, int pid) {
    for (const ViewInfo& view : state.views) {
        for (int shown : view.shows) {
            if (shown == pid) {
                return true;
            }
        }
    }
    return false;
}

// a thumbnail of the last image read, or the activity glyph
static void draw_picture(SDL_Renderer* r, const State& state, const Tail& tail, float x, float y,
                         float size) {
    bool fresh_image = !tail.image_path.empty() && state.now - tail.image_at < image_show_for &&
                       tail.act != Act::Done;
    SDL_Texture* thumb = fresh_image ? thumb_for(r, tail.image_path, state.now) : nullptr;
    if (!thumb) {
        draw_icon(r, tail.act, x, y, size, state.now);
        return;
    }
    float tw, th;
    SDL_GetTextureSize(thumb, &tw, &th);
    float scale = std::min(size / tw, size / th);
    SDL_FRect dst = {x + (size - tw * scale) * 0.5f, y + (size - th * scale) * 0.5f, tw * scale,
                     th * scale};
    SDL_RenderTexture(r, thumb, nullptr, &dst);
    rect_outline(r, dst.x - 1, dst.y - 1, dst.w + 2, dst.h + 2, 1, look::text);
}

// the image being looked at, cropped to fill the tile, kept dark under the text
static void draw_image_backdrop(SDL_Renderer* r, const State& state, const Tail& tail, float x,
                                float y, float w, float h) {
    if (tail.image_path.empty() || state.now - tail.image_at >= image_show_for ||
        tail.act == Act::Done) {
        return;
    }
    SDL_Texture* thumb = thumb_for(r, tail.image_path, state.now);
    if (!thumb) {
        return;
    }
    float tw, th;
    SDL_GetTextureSize(thumb, &tw, &th);
    float scale = std::max(w / tw, h / th);
    SDL_FRect src = {(tw - w / scale) * 0.5f, (th - h / scale) * 0.5f, w / scale, h / scale};
    SDL_FRect dst = {x + 3, y + 2, w - 5, h - 4};
    SDL_SetTextureAlphaMod(thumb, 70);
    SDL_RenderTexture(r, thumb, &src, &dst);
    SDL_SetTextureAlphaMod(thumb, 255);
}

// shared frame of every tile: square box, white pulse when it needs you,
// a thin sliding line while busy
static void draw_frame(SDL_Renderer* r, const State& state, const Tail& tail, float x, float y,
                       float w, float h, bool hover) {
    SDL_Color color = look::text;
    look_box(r, x, y, w, h, hover);
    draw_image_backdrop(r, state, tail, x, y, w, h);
    if (tail.act == Act::NeedsYou) {
        float pulse = 0.5f + 0.5f * (float)std::sin(state.now * 3.0);
        rect_outline(r, x, y, w, h, 1, with_alpha(color, pulse));
    }
    if (is_busy(tail.act)) {
        float bar_w = w * 0.25f;
        float phase = (float)std::fmod(state.now * 0.6, 1.0);
        float bx = x + (w + bar_w) * phase - bar_w;
        float left = std::max(bx, x + 4);
        float right = std::min(bx + bar_w, x + w - 4);
        if (right > left) {
            fill_rect(r, left, y + h - 3, right - left, 1, with_alpha(color, 0.8f));
        }
    }
}

static void draw_leader(SDL_Renderer* r, State& state, const Fonts& fonts, const Chat& chat,
                        float x, float y) {
    const Tail& tail = chat.tail;
    bool hover = hovered(state, x, y, leader_w, tile_h);
    draw_frame(r, state, tail, x, y, leader_w, tile_h, hover);
    draw_picture(r, state, tail, x + 10, y + 7, 44);

    float tx = x + 62;
    float text_w = leader_w - 62 - 8;
    std::string when = elapsed_text(state.now - tail.act_since);
    float when_w = text_width(fonts.tiny, when);
    draw_text(r, fonts.tiny, x + leader_w - 8 - when_w, y + 7, when, text_dim);
    std::string name = chat.name.empty() ? std::to_string(chat.pid) : chat.name;
    draw_text(r, fonts.name, tx, y + 5, fit_text(fonts.name, upper(name), text_w - when_w - 6),
              text_main);
    draw_text(r, fonts.small, tx, y + 24, fit_text(fonts.small, act_line(tail), text_w),
              act_ink(tail.act));
    std::string folder = chat.cwd.substr(chat.cwd.find_last_of('/') + 1);
    std::string sub = chat.kind == Kind::Codex ? "codex  " + folder : folder;
    int shown = strip_agent_count(chat, state.now);
    if (shown > 0) {
        sub += "   " + std::to_string(shown) + " agent" + (shown == 1 ? "" : "s");
    }
    draw_text(r, fonts.tiny, tx, y + 40, fit_text(fonts.tiny, sub, text_w - 26), text_dim);

    // + opens a big viewer for this chat, x closes the ones showing it
    bool viewed = has_viewer(state, chat.pid);
    float bw = 20, bh = 15;
    float bx = x + leader_w - bw - 5;
    float by = y + tile_h - bh - 5;
    bool button_hover = hovered(state, bx, by, bw, bh);
    look_box(r, bx, by, bw, bh, button_hover || viewed);
    const char* mark = viewed ? "x" : "+";
    draw_text(r, fonts.tiny, bx + (bw - text_width(fonts.tiny, mark)) * 0.5f, by + 1, mark,
              text_main);
    state.hits.push_back({bx, by, bw, bh, chat.pid, "", false, viewed ? 2 : 1});
    state.hits.push_back({x, y, leader_w, tile_h, chat.pid, ""});
}

static void draw_agent(SDL_Renderer* r, State& state, const Fonts& fonts, const Chat& chat,
                       const Agent& agent, float x, float y) {
    const Tail& tail = agent.tail;
    bool hover = hovered(state, x, y, agent_w, tile_h);
    draw_frame(r, state, tail, x, y, agent_w, tile_h, hover);
    draw_picture(r, state, tail, x + 9, y + 11, 36);

    float tx = x + 52;
    float text_w = agent_w - 52 - 6;
    double since = agent.finished ? state.now - agent.finished_at : state.now - tail.act_since;
    std::string when = elapsed_text(since);
    float when_w = text_width(fonts.tiny, when);
    draw_text(r, fonts.tiny, x + agent_w - 6 - when_w, y + 24, when, text_dim);
    std::string desc = agent.description.empty() ? agent.id : agent.description;
    draw_text(r, fonts.small, tx, y + 5, fit_text(fonts.small, desc, text_w),
              agent.finished ? text_dim : text_main);
    draw_text(r, fonts.tiny, tx, y + 24, fit_text(fonts.tiny, act_name(tail.act), text_w - when_w),
              act_ink(tail.act));
    draw_text(r, fonts.tiny, tx, y + 38, fit_text(fonts.tiny, tail.detail, text_w), text_dim);
    if (agent.finished) {
        // fade the whole tile out over its last seconds on screen
        fill_rect(r, x, y, agent_w, tile_h, with_alpha(window_bg, 0.45f));
    }
    state.hits.push_back({x, y, agent_w, tile_h, chat.pid, agent.id});
}


// quiet chats sit at the bottom as small name chips
static float draw_chips(SDL_Renderer* r, State& state, const Fonts& fonts, float y, float max_w) {
    float x = pad;
    bool any = false;
    for (const Chat& chat : state.chats) {
        if (!is_quiet(chat, state.now)) {
            continue;
        }
        any = true;
        std::string name = chat.name.empty() ? std::to_string(chat.pid) : chat.name;
        float w = text_width(fonts.tiny, name) + 16;
        if (x + w > max_w - pad && x > pad) {
            x = pad;
            y += chip_h + gap;
        }
        bool hover = hovered(state, x, y, w, chip_h);
        look_box(r, x, y, w, chip_h, hover);
        draw_text(r, fonts.tiny, x + 8, y + 3, name, hover ? text_main : text_dim);
        state.hits.push_back({x, y, w, chip_h, chat.pid, "", true});
        x += w + gap;
    }
    return any ? y + chip_h + gap : y;
}

static std::string hover_text(const State& state) {
    for (const HitBox& hit : state.hits) {
        if (!hovered(state, hit.x, hit.y, hit.w, hit.h)) {
            continue;
        }
        for (const Chat& chat : state.chats) {
            if (chat.pid != hit.chat_pid) {
                continue;
            }
            if (hit.button == 1) {
                return "open a big viewer for " + chat.name;
            }
            if (hit.button == 2) {
                return "close the big viewer showing " + chat.name;
            }
            if (hit.agent_id.empty()) {
                return chat.name + "  " + act_line(chat.tail) + "  (" + chat.cwd + ")";
            }
            for (const Agent& agent : chat.agents) {
                if (agent.id == hit.agent_id) {
                    return agent.description + "  [" + agent.model + "]  " + act_line(agent.tail);
                }
            }
        }
    }
    int working = 0;
    int agents = 0;
    for (const Chat& chat : state.chats) {
        working += is_busy(chat.tail.act) ? 1 : 0;
        for (const Agent& agent : chat.agents) {
            agents += agent.finished ? 0 : 1;
        }
    }
    return std::to_string(state.chats.size()) + " chats, " + std::to_string(working) +
           " working, " + std::to_string(agents) + " agents";
}

void draw_tiles(SDL_Renderer* r, State& state, const Fonts& fonts) {
    state.hits.clear();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, window_bg.r, window_bg.g, window_bg.b, 255);
    SDL_RenderClear(r);

    float y = pad;
    float want_w = leader_w + pad * 2;
    for (const Chat& chat : state.chats) {
        if (is_quiet(chat, state.now)) {
            continue;
        }
        draw_leader(r, state, fonts, chat, pad, y);
        float ax = pad + leader_w + gap;
        int on_line = 0;
        for (const Agent& agent : chat.agents) {
            if (!shows_in_strip(agent, state.now)) {
                continue;
            }
            if (on_line == agents_per_line) {
                on_line = 0;
                ax = pad + leader_w + gap;
                y += tile_h + gap;
            }
            draw_agent(r, state, fonts, chat, agent, ax, y);
            ax += agent_w + gap;
            want_w = std::max(want_w, ax - gap + pad);
            on_line++;
        }
        y += tile_h + gap;
    }
    want_w = std::max(want_w, 380.f);
    y = draw_chips(r, state, fonts, y, want_w);

    std::string footer = fit_text(fonts.tiny, hover_text(state), want_w - pad * 2);
    draw_text(r, fonts.tiny, pad, y + 1, footer, text_dim);
    y += footer_h;

    state.win_w = (int)want_w;
    state.win_h = (int)(y + pad - gap);
    // the panel edge, like the machine's other OSDs
    int out_w, out_h;
    SDL_GetRenderOutputSize(r, &out_w, &out_h);
    rect_outline(r, 0, 0, (float)out_w, (float)out_h, 1, look::text);
}
