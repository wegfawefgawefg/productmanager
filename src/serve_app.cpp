// productmanager --serve: no window, just the phone page.
//
// Listens on 127.0.0.1:8651 only; Tailscale's "serve" puts it on the tailnet
// over HTTPS (https://<machine>.<tailnet>.ts.net:8443/), which also lets the
// phone install the page as an app. Read-only: it shows chats, it never
// writes to them.

#include <csignal>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "apps.hpp"
#include "http_server.hpp"
#include "paths.hpp"
#include "sessions.hpp"
#include "state.hpp"
#include "web_state.hpp"

static const int serve_port = 8651;

static volatile sig_atomic_t stop_requested = 0;

static void on_stop(int) {
    stop_requested = 1;
}

static bool read_file(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

static std::string type_for(const std::string& path) {
    auto ends = [&](const char* ext) {
        size_t n = strlen(ext);
        return path.size() >= n && path.compare(path.size() - n, n, ext) == 0;
    };
    if (ends(".html")) {
        return "text/html; charset=utf-8";
    }
    if (ends(".js")) {
        return "text/javascript";
    }
    if (ends(".json") || ends(".webmanifest")) {
        return "application/json";
    }
    if (ends(".svg")) {
        return "image/svg+xml";
    }
    if (ends(".png")) {
        return "image/png";
    }
    if (ends(".jpg") || ends(".jpeg")) {
        return "image/jpeg";
    }
    if (ends(".gif")) {
        return "image/gif";
    }
    if (ends(".webp")) {
        return "image/webp";
    }
    return "application/octet-stream";
}

// the page's own files, by fixed name only
static HttpReply web_file(const std::string& name) {
    HttpReply reply;
    if (!read_file(assets_dir() + "/web/" + name, reply.body)) {
        reply.status = 404;
        return reply;
    }
    reply.type = type_for(name);
    return reply;
}

static HttpReply handle(const State& state, const HttpRequest& req) {
    if (req.path == "/" || req.path == "/index.html") {
        return web_file("index.html");
    }
    if (req.path == "/manifest.webmanifest" || req.path == "/sw.js" || req.path == "/icon.svg" ||
        req.path == "/icon-192.png" || req.path == "/icon-512.png") {
        return web_file(req.path.substr(1));
    }
    if (req.path == "/state.json") {
        HttpReply reply;
        reply.type = "application/json";
        reply.body = state_json(state);
        reply.no_store = true;
        return reply;
    }
    if (req.path == "/image") {
        // only the image a tile points at right now
        std::string path = tile_image(state, atoi(req.param("chat").c_str()), req.param("agent"));
        HttpReply reply;
        if (path.empty() || !read_file(path, reply.body)) {
            reply.status = 404;
            return reply;
        }
        reply.type = type_for(path);
        return reply;
    }
    HttpReply reply;
    reply.status = 404;
    return reply;
}

int run_serve(int argc, char** argv) {
    (void)argc;
    (void)argv;
    State state;
    state.home = home_dir();
    std::string pid_file = runtime_dir() + "/serve.pid";
    signal(SIGTERM, on_stop);
    signal(SIGINT, on_stop);
    signal(SIGPIPE, SIG_IGN);

    HttpServer server;
    if (!http_listen(server, "127.0.0.1", serve_port)) {
        return 1;
    }
    std::ofstream(pid_file) << getpid() << "\n";
    // the phone shows finished agents of the last hour, like a viewer
    while (!stop_requested) {
        state.now = wall_now();
        if (state.now >= state.next_session_scan) {
            scan_sessions(state);
            // set after the scan too, so chats found this scan keep theirs from the start
            for (Chat& chat : state.chats) {
                chat.crew_open = true;
            }
            state.next_session_scan = state.now + 1.0;
        }
        if (state.now >= state.next_tail_poll) {
            poll_all_tails(state);
            state.next_tail_poll = state.now + 0.5;
        }
        http_poll(server, 200, [&](const HttpRequest& req) { return handle(state, req); });
    }
    http_close(server);
    unlink(pid_file.c_str());
    return 0;
}
