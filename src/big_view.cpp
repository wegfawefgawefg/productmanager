#include "big_view.hpp"

#include <algorithm>
#include <functional>

#include "focus.hpp"
#include "look.hpp"
#include "paths.hpp"
#include "shapes.hpp"
#include "text.hpp"
#include "thumbs.hpp"
#include "tile_layout.hpp"

static const float pad = 6;
static const float gap = 6;

static Chat* find_chat(State& state, int pid) {
    for (Chat& chat : state.chats) {
        if (chat.pid == pid) {
            return &chat;
        }
    }
    return nullptr;
}

static bool is_shown(const BigView& big, const Chat& chat) {
    return std::find(big.shown.begin(), big.shown.end(), chat.pid) != big.shown.end();
}

void mark_shown_chats(const BigView& big, State& state) {
    for (Chat& chat : state.chats) {
        bool want = big.shown.empty() || is_shown(big, chat);
        if (want && !chat.crew_open) {
            state.next_session_scan = 0; // pick up the last hour's agents now
        }
        chat.crew_open = want;
    }
}

static void load_fonts(SDL_Renderer* r, CrewFonts& f, float title, float name, float body,
                       float tiny) {
    std::string assets = assets_dir();
    load_font(r, f.title, assets + "/mono_bold.ttf", title);
    load_font(r, f.name, assets + "/mono_bold.ttf", name);
    load_font(r, f.body, assets + "/mono.ttf", body);
    load_font(r, f.tiny, assets + "/mono.ttf", tiny);
}

bool open_big(BigView& big, const char* title) {
    big.window = SDL_CreateWindow(title, 1080, 960, SDL_WINDOW_RESIZABLE);
    big.renderer = big.window ? SDL_CreateRenderer(big.window, nullptr) : nullptr;
    if (!big.renderer) {
        SDL_Log("viewer window: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(big.renderer, 1);
    load_fonts(big.renderer, big.fonts[0], 22, 16, 14, 12);
    load_fonts(big.renderer, big.fonts[1], 26, 20, 16, 14);
    load_fonts(big.renderer, big.fonts[2], 30, 25, 19, 16);
    load_fonts(big.renderer, big.small_fonts, 20, 17, 14, 12);
    return true;
}

void close_big(BigView& big) {
    if (big.renderer) {
        forget_thumbs(big.renderer);
        SDL_DestroyRenderer(big.renderer);
    }
    if (big.window) {
        SDL_DestroyWindow(big.window);
    }
    big.window = nullptr;
    big.renderer = nullptr;
}

static const Agent* agent_by_id(const Chat& chat, const std::string& id) {
    for (const Agent& agent : chat.agents) {
        if (agent.id == id) {
            return &agent;
        }
    }
    return nullptr;
}

// depth first from the leader's own agents, oldest first, so an agent's
// agents sit right after it
static std::vector<const Agent*> tree_order(const Chat& chat) {
    std::vector<const Agent*> sorted;
    for (const Agent& agent : chat.agents) {
        sorted.push_back(&agent);
    }
    std::sort(sorted.begin(), sorted.end(), [](const Agent* a, const Agent* b) {
        return a->tail.started_at < b->tail.started_at;
    });
    std::vector<const Agent*> out;
    std::function<void(const std::string&)> add_children = [&](const std::string& parent) {
        for (const Agent* agent : sorted) {
            bool is_root = agent->parent_id.empty() || !agent_by_id(chat, agent->parent_id);
            bool match = parent.empty() ? is_root : agent->parent_id == parent;
            if (match) {
                out.push_back(agent);
                add_children(agent->id);
            }
        }
    };
    add_children("");
    return out;
}

static CardContext card_context(const Chat& chat, const Agent* agent, double now) {
    CardContext ctx;
    ctx.now = now;
    if (!agent->parent_id.empty()) {
        const Agent* parent = agent_by_id(chat, agent->parent_id);
        ctx.parent_name = parent ? parent->description : "another agent";
    }
    // two working agents editing the same file is worth knowing about
    if (!agent->finished && agent->tail.act == Act::Editing) {
        for (const Agent& other : chat.agents) {
            if (&other != agent && !other.finished && other.tail.act == Act::Editing &&
                !agent->tail.edit_path.empty() && other.tail.edit_path == agent->tail.edit_path) {
                ctx.clash = other.description;
            }
        }
    }
    return ctx;
}

static bool hovered(const BigView& big, float x, float y, float w, float h) {
    return big.mouse_x >= x && big.mouse_x < x + w && big.mouse_y >= y && big.mouse_y < y + h;
}

// a chat to show: its working agents in tree order, its finished ones newest first
struct Group {
    Chat* chat;
    std::vector<const Agent*> live;
    std::vector<const Agent*> done;
};

// one tile: a chat's leader (agent == nullptr) or one of its agents
struct TileRef {
    const Group* group;
    const Agent* agent;
};

static const CrewFonts& fonts_for(const BigView& big, float card_w, float card_h) {
    if (card_w >= 700 && card_h >= 420) {
        return big.fonts[2];
    }
    if (card_w >= 460 && card_h >= 300) {
        return big.fonts[1];
    }
    return big.fonts[0];
}

static Group make_group(Chat& chat) {
    Group g{&chat, {}, {}};
    for (const Agent* agent : tree_order(chat)) {
        (agent->finished ? g.done : g.live).push_back(agent);
    }
    std::sort(g.done.begin(), g.done.end(),
              [](const Agent* a, const Agent* b) { return a->finished_at > b->finished_at; });
    return g;
}

// the viewer's own chats; with none, every chat with agents working, or
// failing that every chat whose leader is working
static std::vector<Group> shown_groups(const BigView& big, State& state) {
    std::vector<Group> groups;
    for (Chat& chat : state.chats) {
        if (is_shown(big, chat)) {
            groups.push_back(make_group(chat));
        }
    }
    if (!groups.empty()) {
        return groups;
    }
    for (Chat& chat : state.chats) {
        Group g = make_group(chat);
        if (!g.live.empty()) {
            groups.push_back(g);
        }
    }
    if (!groups.empty()) {
        return groups;
    }
    for (Chat& chat : state.chats) {
        if (chat.tail.act != Act::Idle && chat.tail.act != Act::NeedsYou) {
            groups.push_back(make_group(chat));
        }
    }
    return groups;
}

void draw_big(BigView& big, State& state) {
    if (!big.renderer) {
        return;
    }
    SDL_Renderer* r = big.renderer;
    big.hits.clear();
    int out_w, out_h;
    SDL_GetRenderOutputSize(r, &out_w, &out_h);
    float w = (float)out_w;
    float h = (float)out_h;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, look::bg.r, look::bg.g, look::bg.b, 255);
    SDL_RenderClear(r);

    // every tile: each chat's leader, then its working agents
    std::vector<Group> groups = shown_groups(big, state);
    std::vector<TileRef> tiles;
    for (const Group& g : groups) {
        tiles.push_back({&g, nullptr});
        for (const Agent* agent : g.live) {
            tiles.push_back({&g, agent});
        }
    }
    if (tiles.empty()) {
        draw_text(r, big.small_fonts.name, pad + 8, pad + 8, "NOTHING WORKING", look::dim);
    }
    std::vector<Slot> slots = layout_tiles((int)tiles.size(), pad, pad, w - pad * 2,
                                           h - pad * 2, gap, groups.size() == 1);
    for (size_t i = 0; i < tiles.size(); i++) {
        const Slot& s = slots[i];
        const TileRef& t = tiles[i];
        bool hover = hovered(big, s.x, s.y, s.w, s.h);
        const CrewFonts& fonts = fonts_for(big, s.w, s.h);
        if (!t.agent) {
            draw_leader_card(r, fonts, *t.group->chat, t.group->done, (int)t.group->live.size(),
                             state.now, hover, s.x, s.y, s.w, s.h);
        } else {
            CardContext ctx = card_context(*t.group->chat, t.agent, state.now);
            ctx.hover = hover;
            draw_live_card(r, fonts, *t.agent, ctx, s.x, s.y, s.w, s.h);
        }
        big.hits.push_back({s.x, s.y, s.w, s.h, t.group->chat->pid, t.agent ? t.agent->id : ""});
    }
}

void big_event(BigView& big, State& state, const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        big.mouse_x = -1;
        big.mouse_y = -1;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        big.mouse_x = event.motion.x;
        big.mouse_y = event.motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button != SDL_BUTTON_LEFT) {
            break;
        }
        for (const HitBox& hit : big.hits) {
            if (hovered(big, hit.x, hit.y, hit.w, hit.h)) {
                if (Chat* chat = find_chat(state, hit.chat_pid)) {
                    focus_chat(*chat);
                }
                break;
            }
        }
        break;
    default:
        break;
    }
}
