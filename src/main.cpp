#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "apps.hpp"
#include "focus.hpp"
#include "paths.hpp"
#include "views.hpp"

// productmanager             show the strip, or hide it when it is showing
// productmanager start       show the strip unless it already runs (login autostart)
// productmanager stop        close the strip and every viewer
// productmanager big [NAME]  open a big viewer: this workspace's chats, or one chat
// (pm is a link to the same program)
//
// Inside, the strip runs as --strip and viewers as --view; both detach from
// the terminal that started them.

static int running_strip() {
    std::ifstream in(runtime_dir() + "/strip.pid");
    int pid = 0;
    in >> pid;
    return is_ours(pid) ? pid : 0;
}

static void start_strip() {
    if (!running_strip()) {
        launch_program({own_exe(), "--strip"});
    }
}

static void stop_all() {
    if (int pid = running_strip()) {
        kill(pid, SIGTERM);
    }
    for (const ViewInfo& view : read_views()) {
        kill(view.pid, SIGTERM);
    }
}

int main(int argc, char** argv) {
    std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "--strip" || cmd == "--shot") {
        return run_strip(argc, argv);
    }
    if (cmd == "--view") {
        return run_view(argc, argv);
    }
    if (cmd == "") {
        if (int pid = running_strip()) {
            kill(pid, SIGTERM);
        } else {
            start_strip();
        }
        return 0;
    }
    if (cmd == "start") {
        start_strip();
        return 0;
    }
    if (cmd == "stop") {
        stop_all();
        return 0;
    }
    if (cmd == "big") {
        std::vector<std::string> args = {own_exe(), "--view"};
        if (argc > 2) {
            args.push_back("--pin");
            args.push_back(argv[2]);
        }
        launch_program(args);
        return 0;
    }
    fprintf(stderr,
            "usage: %s [start | stop | big [NAME]]\n"
            "  no argument shows the strip, or hides it when it is showing\n",
            app_name);
    return 1;
}
