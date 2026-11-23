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
// productmanager start       start the strip and the phone page unless they run (login)
// productmanager stop        close the strip, every viewer and the phone page
// productmanager big [NAME]  open a big viewer: this workspace's chats, or one chat
// productmanager serve       start only the phone page (127.0.0.1:8651)
// (pm is a link to the same program)
//
// Inside, the strip runs as --strip, viewers as --view and the phone page as
// --serve; all detach from the terminal that started them.

static int running(const char* pid_name) {
    std::ifstream in(runtime_dir() + "/" + pid_name);
    int pid = 0;
    in >> pid;
    return is_ours(pid) ? pid : 0;
}

static int running_strip() {
    return running("strip.pid");
}

static void start_serve() {
    if (!running("serve.pid")) {
        launch_program({own_exe(), "--serve"});
    }
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
    if (int pid = running("serve.pid")) {
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
    if (cmd == "--serve") {
        return run_serve(argc, argv);
    }
    if (cmd == "serve") {
        start_serve();
        return 0;
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
        start_serve();
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
            "usage: %s [start | stop | big [NAME] | serve]\n"
            "  no argument shows the strip, or hides it when it is showing\n",
            app_name);
    return 1;
}
