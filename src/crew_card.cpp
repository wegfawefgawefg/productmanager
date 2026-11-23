#include "crew_card.hpp"

#include <algorithm>
#include <cmath>

#include "icons.hpp"
#include "look.hpp"
#include "shapes.hpp"
#include "thumbs.hpp"
#include "tiles.hpp"
#include "transcript.hpp"

static const SDL_Color strip_bg = {16, 16, 16, 255};
static const SDL_Color text_main = look::text;
static const SDL_Color text_body = look::body;
static const SDL_Color text_dim = look::dim;
static const SDL_Color amber = look::warn;
static const SDL_Color red = look::fail;
// an image stays as the card's picture this long after it was read
static const double image_show_for = 40;
// silence this long from a working agent is worth a look
static const double quiet_warn_after = 5 * 60;
// a new tool call flashes the card edge this long
static const double flash_for = 0.6;

void draw_history(SDL_Renderer* r, const Tail& tail, float x, float y, float w, float h, double t0,
                  double t1, double now) {
    fill_rect(r, x, y, w, h, strip_bg);
    if (t1 <= t0) {
        return;
    }
    float scale = w / (float)(t1 - t0);
    for (size_t i = 0; i < tail.ticks.size(); i++) {
        const Tick& tick = tail.ticks[i];
        double end = i + 1 < tail.ticks.size() ? tail.ticks[i + 1].at : std::min(t1, now);
        if (end < t0 || tick.at > t1) {
            continue;
        }
        float x0 = x + (float)(std::max(tick.at, t0) - t0) * scale;
        float x1 = x + (float)(std::min(end, t1) - t0) * scale;
        // at least a sliver, so quick calls still show
        x1 = std::max(x1, x0 + 1.5f);
        fill_rect(r, x0, y, std::min(x1, x + w) - x0, h, with_alpha(act_color(tick.act), 0.85f));
        if (tick.failed) {
            fill_rect(r, x0, y, std::max(2.f, x1 - x0), 3, red);
        }
    }
}

static bool image_is_fresh(const Tail& tail, double now) {
    return !tail.image_path.empty() && now - tail.image_at < image_show_for;
}

std::string trouble_text(const Agent& agent, const CardContext& ctx) {
    if (agent.finished) {
        return "";
    }
    const Tail& tail = agent.tail;
    if (tail.error_streak >= 3) {
        return std::to_string(tail.error_streak) + " failed calls in a row";
    }
    if (tail.act != Act::Waiting && ctx.now - tail.last_line_at > quiet_warn_after) {
        return "silent for " + elapsed_text(ctx.now - tail.last_line_at);
    }
    if (!ctx.clash.empty()) {
        return "same file as " + ctx.clash;
    }
    return "";
}

static float line_h(const Font& font) {
    return font.size * 1.25f;
}

// the last few tool calls under the current one, newest first
static void draw_recent_calls(SDL_Renderer* r, const CrewFonts& fonts, const Tail& tail, double now,
                              float x, float y, float w, float h) {
    const float lh = line_h(fonts.tiny);
    const float box = fonts.tiny.size * 0.7f;
    int rows = (int)(h / lh);
    int shown = 0;
    // the newest tick is the current action, already shown above
    for (int i = (int)tail.ticks.size() - 2; i >= 0 && shown < rows; i--, shown++) {
        const Tick& tick = tail.ticks[i];
        float ly = y + shown * lh;
        float fade = std::max(0.35f, 1.0f - 0.08f * shown);
        fill_rect(r, x, ly + (lh - box) * 0.5f, box, box, with_alpha(act_color(tick.act), fade));
        if (tick.failed) {
            fill_rect(r, x, ly + (lh - box) * 0.5f, box, 3, red);
        }
        std::string age = elapsed_text(now - tick.at);
        float age_w = text_width(fonts.tiny, age);
        draw_text(r, fonts.tiny, x + w - age_w, ly, age, text_dim);
        std::string what = std::string(act_name(tick.act)) + "  " + tick.detail;
        draw_text(r, fonts.tiny, x + box + 6, ly,
                  fit_text(fonts.tiny, what, w - box - 14 - age_w), with_alpha(text_body, fade));
    }
}

static void draw_image_fit(SDL_Renderer* r, SDL_Texture* image, float x, float y, float w, float h,
                           float alpha) {
    float tw, th;
    SDL_GetTextureSize(image, &tw, &th);
    float scale = std::min(w / tw, h / th);
    SDL_FRect dst = {x + (w - tw * scale) * 0.5f, y + (h - th * scale) * 0.5f, tw * scale,
                     th * scale};
    SDL_SetTextureAlphaMod(image, (Uint8)(255 * alpha));
    SDL_RenderTexture(r, image, nullptr, &dst);
    SDL_SetTextureAlphaMod(image, 255);
    rect_outline(r, dst.x - 1, dst.y - 1, dst.w + 2, dst.h + 2, 1, with_alpha(look::line, alpha));
}

// the picture area: a freshly read image fills it; otherwise the current
// action big, the calls before it underneath, and the last image it looked
// at kept small on the right
static void draw_picture_area(SDL_Renderer* r, const CrewFonts& fonts, const Tail& tail,
                              double now, float x, float y, float w, float h) {
    SDL_Texture* image =
        tail.image_path.empty() ? nullptr : thumb_for(r, tail.image_path, now);
    if (image && image_is_fresh(tail, now)) {
        draw_image_fit(r, image, x, y, w, h, 1.0f);
        std::string what = std::string("looking: ") + tail.detail;
        float lh = line_h(fonts.tiny);
        float tw = std::min(text_width(fonts.tiny, what) + 10, w);
        fill_rect(r, x, y + h - lh, tw, lh, with_alpha(strip_bg, 0.8f));
        draw_text(r, fonts.tiny, x + 5, y + h - lh, fit_text(fonts.tiny, what, w - 10), text_main);
        return;
    }
    float left_w = w;
    // the last image stays beside the feed only where the feed still has room
    if (image && w >= 420) {
        float side = std::min(w * 0.38f, h);
        left_w = w - side - 10;
        draw_image_fit(r, image, x + w - side, y, side, h, 0.55f);
    }
    float icon = fonts.name.size * 2.2f;
    if (h < icon + 4) {
        // too short for the big form: glyph, action and detail on one line
        float small = line_h(fonts.body);
        draw_icon(r, tail.act, x, y, small, now);
        std::string line = std::string(act_name(tail.act)) + "  " + tail.detail;
        draw_text(r, fonts.body, x + small + 6, y, fit_text(fonts.body, line, left_w - small - 6),
                  act_ink(tail.act));
        return;
    }
    draw_icon(r, tail.act, x, y + 2, icon, now);
    float tx = x + icon + 12;
    float tw = left_w - icon - 12;
    std::string doing = act_name(tail.act);
    std::string since = elapsed_text(now - tail.act_since);
    draw_text(r, fonts.name, tx, y, doing, act_ink(tail.act));
    draw_text(r, fonts.tiny, tx + text_width(fonts.name, doing) + 8,
              y + (fonts.name.size - fonts.tiny.size) * 0.6f, since, text_dim);
    draw_text(r, fonts.body, tx, y + line_h(fonts.name), fit_text(fonts.body, tail.detail, tw),
              text_body);
    float list_y = y + std::max(icon, line_h(fonts.name) + line_h(fonts.body)) + 12;
    fill_rect(r, x, list_y - 6, left_w, 1, SDL_Color{40, 43, 54, 255});
    draw_recent_calls(r, fonts, tail, now, x, list_y, left_w, y + h - list_y);
}

static void draw_card_edge(SDL_Renderer* r, const Agent& agent, const CardContext& ctx,
                           const std::string& trouble, float x, float y, float w, float h) {
    const Tail& tail = agent.tail;
    look_box(r, x, y, w, h, ctx.hover);
    if (!trouble.empty()) {
        float pulse = 0.55f + 0.3f * (float)std::sin(ctx.now * 2.5);
        rect_outline(r, x, y, w, h, 2, with_alpha(amber, pulse));
        return;
    }
    double since_tick = tail.ticks.empty() ? 1e9 : ctx.now - tail.ticks.back().at;
    if (since_tick < flash_for) {
        float fade = 1.0f - (float)(since_tick / flash_for);
        // a new tool call flashes the edge white
        rect_outline(r, x, y, w, h, 1, with_alpha(look::text, fade));
    }
}

void draw_live_card(SDL_Renderer* r, const CrewFonts& fonts, const Agent& agent,
                    const CardContext& ctx, float x, float y, float w, float h) {
    const Tail& tail = agent.tail;
    std::string trouble = trouble_text(agent, ctx);
    draw_card_edge(r, agent, ctx, trouble, x, y, w, h);
    const float m = std::max(10.f, fonts.body.size * 0.7f); // margin
    float inner = w - m * 2;
    float cy = y + m * 0.8f;

    // header: who it is, how long it has run, what it costs
    std::string age = elapsed_text(ctx.now - tail.started_at);
    float age_w = text_width(fonts.tiny, age);
    draw_text(r, fonts.tiny, x + w - m - age_w, cy + 3, age, text_dim);
    std::string desc = agent.description.empty() ? agent.id : agent.description;
    draw_text(r, fonts.name, x + m, cy, fit_text(fonts.name, desc, inner - age_w - 8), text_main);
    cy += line_h(fonts.name);
    std::string stats = agent.model.empty() ? agent.type : agent.model + "  " + agent.type;
    stats += "   " + tokens_text(tail.output_tokens) + " out   " +
             tokens_text(tail.context_tokens) + " ctx   " + std::to_string(tail.tools_used) +
             " calls";
    draw_text(r, fonts.tiny, x + m, cy, fit_text(fonts.tiny, stats, inner), text_dim);
    cy += line_h(fonts.tiny);
    if (!ctx.parent_name.empty()) {
        draw_text(r, fonts.tiny, x + m, cy,
                  fit_text(fonts.tiny, "under " + ctx.parent_name, inner), text_dim);
        cy += line_h(fonts.tiny);
    }

    // bottom up: history barcode, then what it last said; the picture gets the rest
    float strip_h = std::max(12.f, fonts.tiny.size);
    float strip_y = y + h - strip_h - m * 0.8f;
    int remark_lines = h > 400 ? 4 : 3;
    std::vector<std::string> said = wrap_text(fonts.body, tail.remark, inner, remark_lines);
    float remark_h = said.size() * line_h(fonts.body);
    float remark_y = strip_y - 8 - remark_h;
    float pic_y = cy + 6;
    float pic_h = remark_y - 8 - pic_y;
    if (pic_h > 20) {
        draw_picture_area(r, fonts, tail, ctx.now, x + m, pic_y, inner, pic_h);
    }
    for (size_t i = 0; i < said.size(); i++) {
        draw_text(r, fonts.body, x + m, remark_y + i * line_h(fonts.body), said[i], text_body);
    }
    draw_history(r, tail, x + m, strip_y, inner, strip_h, tail.started_at, ctx.now, ctx.now);
    if (!trouble.empty()) {
        float lh = line_h(fonts.tiny);
        float tw = text_width(fonts.tiny, trouble) + 10;
        fill_rect(r, x + w - m - tw, strip_y - lh - 2, tw, lh, with_alpha(amber, 0.9f));
        draw_text(r, fonts.tiny, x + w - m - tw + 5, strip_y - lh - 2, trouble, strip_bg);
    }
}

// one finished agent as a line in the leader tile: check, name, times, a thin barcode
static void draw_done_line(SDL_Renderer* r, const CrewFonts& fonts, const Agent& agent, double now,
                           float x, float y, float w) {
    const Tail& tail = agent.tail;
    float lh = line_h(fonts.tiny);
    draw_icon(r, Act::Done, x, y, lh, now);
    double ran = std::max(0.0, tail.last_line_at - tail.started_at);
    std::string right = "ran " + elapsed_text(ran) + ", done " +
                        elapsed_text(now - agent.finished_at) + " ago";
    float right_w = text_width(fonts.tiny, right);
    draw_text(r, fonts.tiny, x + w - right_w, y, right, text_dim);
    std::string desc = agent.description.empty() ? agent.id : agent.description;
    draw_text(r, fonts.tiny, x + lh + 6, y, fit_text(fonts.tiny, desc, w - lh - 14 - right_w),
              text_body);
    draw_history(r, tail, x + lh + 6, y + lh, w - lh - 6, 3, tail.started_at, tail.last_line_at,
                 tail.last_line_at);
}

void draw_leader_card(SDL_Renderer* r, const CrewFonts& fonts, const Chat& chat,
                      const std::vector<const Agent*>& finished, int working, double now,
                      bool hover, float x, float y, float w, float h) {
    const Tail& tail = chat.tail;
    look_box(r, x, y, w, h, hover);
    if (tail.act == Act::NeedsYou) {
        float pulse = 0.5f + 0.5f * (float)std::sin(now * 3.0);
        rect_outline(r, x, y, w, h, 1, with_alpha(look::text, pulse));
    }
    const float m = std::max(10.f, fonts.body.size * 0.7f);
    float inner = w - m * 2;
    float cy = y + m * 0.8f;

    // who: the chat's name, its crew in numbers, where it works
    std::string counts = std::to_string(working) + " working   " +
                         std::to_string(finished.size()) + " done this hour";
    float counts_w = text_width(fonts.tiny, counts);
    draw_text(r, fonts.tiny, x + w - m - counts_w, cy + 4, counts, text_dim);
    std::string name;
    for (char c : chat.name) {
        name += (char)toupper((unsigned char)c);
    }
    draw_text(r, fonts.title, x + m, cy, fit_text(fonts.title, name, inner - counts_w - 10),
              text_main);
    cy += line_h(fonts.title);
    std::string where = chat.cwd + "   " + tokens_text(tail.context_tokens) + " ctx";
    draw_text(r, fonts.tiny, x + m, cy, fit_text(fonts.tiny, where, inner), text_dim);
    cy += line_h(fonts.tiny) + 4;

    // bottom up: last hour barcode, finished agents, what it last said
    float strip_h = std::max(10.f, fonts.tiny.size * 0.8f);
    float strip_y = y + h - strip_h - m * 0.8f;
    draw_history(r, tail, x + m, strip_y, inner, strip_h, now - 3600, now, now);
    draw_text(r, fonts.tiny, x + m, strip_y - line_h(fonts.tiny) - 2, "last hour", text_dim);
    float bottom = strip_y - line_h(fonts.tiny) - 8;

    float done_row = line_h(fonts.tiny) + 6;
    std::vector<std::string> said = wrap_text(fonts.body, tail.remark, inner, 3);
    float said_h = said.size() * line_h(fonts.body);
    // the leader's own action line always fits; finished agents take what is
    // left, up to 60% of the tile
    float pic_min = fonts.name.size * 2.2f + 12;
    float space = bottom - cy;
    float for_done = std::min(space * 0.6f, space - said_h - pic_min - line_h(fonts.tiny) - 14);
    int rows = std::min((int)finished.size(), std::max(0, (int)(for_done / done_row)));
    if (rows > 0) {
        float list_y = bottom - rows * done_row;
        for (int i = 0; i < rows; i++) {
            draw_done_line(r, fonts, *finished[i], now, x + m, list_y + i * done_row, inner);
        }
        bottom = list_y - line_h(fonts.tiny) - 6;
        std::string label = "finished";
        if ((int)finished.size() > rows) {
            label += " (" + std::to_string(finished.size() - rows) + " more)";
        }
        draw_text(r, fonts.tiny, x + m, bottom + 2, label, text_dim);
        bottom -= 6;
    }
    float remark_y = bottom - said_h;
    for (size_t i = 0; i < said.size(); i++) {
        draw_text(r, fonts.body, x + m, remark_y + i * line_h(fonts.body), said[i], text_body);
    }
    float pic_h = remark_y - 8 - cy;
    if (pic_h > 20) {
        draw_picture_area(r, fonts, tail, now, x + m, cy, inner, pic_h);
    }
}
