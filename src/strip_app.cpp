#include <SDL3/SDL.h>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>

#include "apps.hpp"
#include "capture.hpp"
#include "focus.hpp"
#include "paths.hpp"
#include "sessions.hpp"
#include "state.hpp"
#include "tiles.hpp"
#include "views.hpp"

static Chat* chat_by_pid(State& state, int pid) {
    for (Chat& chat : state.chats) {
        if (chat.pid == pid) {
            return &chat;
        }
    }
    return nullptr;
}

// viewers launched in the last few seconds that have not registered yet
struct Pending {
    int chat_pid;
    double launched_at;
};
static std::vector<Pending> pending;
// x pressed while a viewer was still starting: close it once it registers
static std::vector<Pending> closing;

static void open_viewer(State& state, int chat_pid) {
    launch_program({own_exe(), "--view", "--pin-pid", std::to_string(chat_pid)});
    pending.push_back({chat_pid, state.now});
    state.views.push_back({-1, chat_pid, {chat_pid}});
}

// registered viewers, plus launches still starting up, so the button
// does not flip back to + (and open a second viewer) while one starts
static void refresh_views(State& state) {
    state.views = read_views();
    std::erase_if(closing, [&](const Pending& c) {
        for (const ViewInfo& view : state.views) {
            if (view.pinned == c.chat_pid && view.pid > 0) {
                kill(view.pid, SIGTERM);
                return true;
            }
        }
        return state.now - c.launched_at > 8;
    });
    std::erase_if(pending, [&](const Pending& p) {
        if (state.now - p.launched_at > 8) {
            return true;
        }
        for (const ViewInfo& view : state.views) {
            if (view.pinned == p.chat_pid) {
                return true;
            }
        }
        return false;
    });
    for (const Pending& p : pending) {
        state.views.push_back({-1, p.chat_pid, {p.chat_pid}});
    }
}

static void close_viewers(State& state, int chat_pid) {
    for (const ViewInfo& view : state.views) {
        for (int shown : view.shows) {
            if (shown == chat_pid && view.pid > 0) {
                kill(view.pid, SIGTERM);
            }
        }
    }
    for (const Pending& p : pending) {
        if (p.chat_pid == chat_pid) {
            closing.push_back({chat_pid, p.launched_at});
        }
    }
    std::erase_if(pending, [chat_pid](const Pending& p) { return p.chat_pid == chat_pid; });
    std::erase_if(state.views, [chat_pid](const ViewInfo& v) {
        return std::find(v.shows.begin(), v.shows.end(), chat_pid) != v.shows.end();
    });
}

// the viewer buttons, then tiles: a leader or chip jumps to its chat; an
// agent tile opens a viewer for its chat
static void click(State& state, float x, float y) {
    for (const HitBox& hit : state.hits) {
        if (x < hit.x || x >= hit.x + hit.w || y < hit.y || y >= hit.y + hit.h) {
            continue;
        }
        if (hit.button == 1 || (!hit.agent_id.empty() && hit.button == 0)) {
            bool viewed = false;
            for (const ViewInfo& view : state.views) {
                viewed |= std::find(view.shows.begin(), view.shows.end(), hit.chat_pid) !=
                          view.shows.end();
            }
            if (!viewed) {
                open_viewer(state, hit.chat_pid);
            }
        } else if (hit.button == 2) {
            close_viewers(state, hit.chat_pid);
        } else if (Chat* chat = chat_by_pid(state, hit.chat_pid)) {
            focus_chat(*chat);
        }
        return;
    }
}

// first placement only: the chosen corner of the screen the window opened on
static void place_window(SDL_Window* window, const State& state) {
    SDL_Rect bounds;
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!SDL_GetDisplayUsableBounds(display ? display : SDL_GetPrimaryDisplay(), &bounds)) {
        return;
    }
    const int margin = 8;
    int x = bounds.x + margin;
    int y = bounds.y + margin;
    if (state.corner == 1 || state.corner == 2) {
        x = bounds.x + bounds.w - state.win_w - margin;
    }
    if (state.corner == 2 || state.corner == 3) {
        y = bounds.y + bounds.h - state.win_h - margin;
    }
    SDL_SetWindowSize(window, state.win_w, state.win_h);
    SDL_SetWindowPosition(window, x, y);
}

// later size changes keep the window's top-right corner where it is now
// (where it was placed, or wherever it was dragged to), growing left and down
static void resize_in_place(SDL_Window* window, int old_w, const State& state) {
    int x, y;
    SDL_GetWindowPosition(window, &x, &y);
    SDL_SetWindowSize(window, state.win_w, state.win_h);
    SDL_SetWindowPosition(window, x + old_w - state.win_w, y);
}

int run_strip(int argc, char** argv) {
    std::string shot_path;
    State state;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
            shot_path = argv[++i];
        } else if (!strcmp(argv[i], "--corner") && i + 1 < argc) {
            state.corner = atoi(argv[++i]) & 3;
        }
    }
    state.home = home_dir();
    std::string pid_file = runtime_dir() + "/strip.pid";
    if (shot_path.empty()) {
        std::ofstream(pid_file) << getpid() << "\n";
    }

    // window: borderless, on top, never takes keyboard focus
    SDL_SetHint(SDL_HINT_APP_ID, app_name);
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetAppMetadata(app_name, "0.3", app_name);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP |
                            SDL_WINDOW_UTILITY | SDL_WINDOW_NOT_FOCUSABLE;
    // the i3 rule floats exactly this title
    SDL_Window* window = SDL_CreateWindow(app_name, state.win_w, state.win_h, flags);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        SDL_Log("window: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    Fonts fonts;
    std::string assets = assets_dir();
    load_font(renderer, fonts.name, assets + "/mono_bold.ttf", 14);
    load_font(renderer, fonts.small, assets + "/mono.ttf", 13);
    load_font(renderer, fonts.tiny, assets + "/mono.ttf", 11);

    int frames = 0;
    bool running = true;
    while (running) {
        state.now = wall_now();

        // pump input
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                state.mouse_x = event.motion.x;
                state.mouse_y = event.motion.y;
            } else if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
                state.mouse_x = -1;
                state.mouse_y = -1;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    click(state, event.button.x, event.button.y);
                }
            }
        }

        // follow the sessions, their logs, and the big viewers
        if (state.now >= state.next_session_scan) {
            scan_sessions(state);
            refresh_views(state);
            state.next_session_scan = state.now + 1.0;
        }
        if (state.now >= state.next_tail_poll) {
            poll_all_tails(state);
            state.next_tail_poll = state.now + 0.25;
        }

        // draw frame
        int old_w = state.win_w;
        int old_h = state.win_h;
        draw_tiles(renderer, state, fonts);
        frames++;
        if (!shot_path.empty() && frames == 3) {
            // the first frame sizes the window; this one is drawn at full size
            save_frame_png(renderer, shot_path);
            running = false;
        }
        SDL_RenderPresent(renderer);
        if (frames == 1) {
            place_window(window, state);
        } else if (state.win_w != old_w || state.win_h != old_h) {
            resize_in_place(window, old_w, state);
        }
        SDL_Delay(40);
    }

    if (shot_path.empty()) {
        unlink(pid_file.c_str());
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
