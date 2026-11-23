#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "apps.hpp"
#include "big_view.hpp"
#include "capture.hpp"
#include "focus.hpp"
#include "paths.hpp"
#include "sessions.hpp"
#include "state.hpp"
#include "views.hpp"
#include "wm.hpp"

// productmanager --view                 the chats whose terminals share its workspace
// productmanager --view --pin NAME      one chat by name
// productmanager --view --pin-pid PID   one chat by process (the strip's + button);
//                                       the viewer moves to that chat's workspace
// productmanager --view --shot PATH     headless capture
// productmanager --view --size WxH      start at this window size

// chat terminal windows barely move, so their ids are remembered for a while
struct WindowCache {
    std::map<int, std::string> by_pid;
    double refreshed_at = 0;
};

static long own_window(SDL_Window* window) {
    return (long)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
                                       SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
}

static std::string cached_window(WindowCache& cache, const Chat& chat, double now) {
    if (now - cache.refreshed_at > 30) {
        cache.by_pid.clear();
        cache.refreshed_at = now;
    }
    auto it = cache.by_pid.find(chat.pid);
    if (it != cache.by_pid.end()) {
        return it->second;
    }
    return cache.by_pid[chat.pid] = chat_window(chat);
}

// the chats whose terminals sit on the same i3 workspace as this window
static std::vector<int> chats_on_my_workspace(State& state, WindowCache& cache, long me) {
    std::map<long, std::string> spaces = window_workspaces();
    auto mine = spaces.find(me);
    std::vector<int> out;
    if (mine == spaces.end()) {
        return out;
    }
    for (const Chat& chat : state.chats) {
        std::string win = cached_window(cache, chat, state.now);
        if (win.empty()) {
            continue;
        }
        auto there = spaces.find(atol(win.c_str()));
        if (there != spaces.end() && there->second == mine->second) {
            out.push_back(chat.pid);
        }
    }
    return out;
}

static int chat_pid_by_name(const State& state, const std::string& name) {
    for (const Chat& chat : state.chats) {
        if (chat.name == name) {
            return chat.pid;
        }
    }
    return 0;
}

// a pinned viewer goes to its chat's workspace once, then stays where it is put
static bool move_beside_chat(State& state, WindowCache& cache, SDL_Window* window, int pid) {
    for (const Chat& chat : state.chats) {
        if (chat.pid != pid) {
            continue;
        }
        std::string win = cached_window(cache, chat, state.now);
        std::map<long, std::string> spaces = window_workspaces();
        auto there = spaces.find(atol(win.c_str()));
        long me = own_window(window);
        if (there == spaces.end() || me == 0 || spaces.find(me) == spaces.end()) {
            return false;
        }
        move_to_workspace(me, there->second);
        return true;
    }
    return false;
}

int run_view(int argc, char** argv) {
    std::string shot_path;
    std::string pin_name;
    int pinned = 0;
    int want_w = 0;
    int want_h = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
            shot_path = argv[++i];
        } else if (!strcmp(argv[i], "--pin") && i + 1 < argc) {
            pin_name = argv[++i];
        } else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            sscanf(argv[++i], "%dx%d", &want_w, &want_h);
        } else if (!strcmp(argv[i], "--pin-pid") && i + 1 < argc) {
            pinned = atoi(argv[++i]);
        }
    }
    State state;
    state.home = home_dir();
    bool move_once = pinned != 0 && shot_path.empty();

    SDL_SetHint(SDL_HINT_APP_ID, app_name);
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetAppMetadata(app_name, "0.3", app_name);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    // a normal window: i3 tiles it (only the strip's exact title floats)
    BigView big;
    std::string base_title = std::string(app_name) + " view";
    if (!open_big(big, base_title.c_str())) {
        return 1;
    }
    // register at once, so the strip shows x for this chat right away
    if (shot_path.empty()) {
        write_view(pinned, pinned ? std::vector<int>{pinned} : std::vector<int>{});
    }
    if (want_w > 0 && want_h > 0) {
        SDL_SetWindowSize(big.window, want_w, want_h);
    }
    WindowCache cache;
    double next_workspace_check = 0;
    double give_up_moving_at = wall_now() + 10;
    std::string title;

    int frames = 0;
    bool running = true;
    while (running) {
        state.now = wall_now();

        // pump input
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else {
                big_event(big, state, event);
            }
        }

        // follow the sessions; work out which chats this viewer shows
        if (state.now >= state.next_session_scan) {
            scan_sessions(state);
            state.next_session_scan = state.now + 1.0;
            if (!pin_name.empty() && pinned == 0) {
                pinned = chat_pid_by_name(state, pin_name);
            }
            if (pinned != 0) {
                big.shown = {pinned};
            } else if (pin_name.empty() && state.now >= next_workspace_check) {
                big.shown = chats_on_my_workspace(state, cache, own_window(big.window));
                next_workspace_check = state.now + 3;
            }
            mark_shown_chats(big, state);
            if (shot_path.empty()) {
                write_view(pinned, big.shown);
            }
            if (move_once && state.now < give_up_moving_at) {
                move_once = !move_beside_chat(state, cache, big.window, pinned);
            }
            std::string want = base_title;
            for (const Chat& chat : state.chats) {
                if (big.shown.size() == 1 && chat.pid == big.shown[0]) {
                    want += ": " + chat.name;
                }
            }
            if (want != title) {
                title = want;
                SDL_SetWindowTitle(big.window, title.c_str());
            }
        }
        if (state.now >= state.next_tail_poll) {
            poll_all_tails(state);
            state.next_tail_poll = state.now + 0.25;
        }

        // draw frame
        draw_big(big, state);
        frames++;
        if (!shot_path.empty() && frames == 10) {
            save_frame_png(big.renderer, shot_path);
            running = false;
        }
        SDL_RenderPresent(big.renderer);
        SDL_Delay(40);
    }

    if (shot_path.empty()) {
        remove_view();
    }
    close_big(big);
    SDL_Quit();
    return 0;
}
