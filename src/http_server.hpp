#pragma once

#include <functional>
#include <string>

// a tiny blocking-free HTTP/1.0 server for the phone page: one request per
// connection, GET only, answered in full before the next is read

struct HttpReply {
    int status = 200;
    std::string type = "text/plain";
    std::string body;
    bool no_store = false; // live data the browser must not cache
};

// path without the query, and the query split into key=value pairs
struct HttpRequest {
    std::string path;
    std::string query;
    std::string param(const std::string& key) const;
};

struct HttpServer {
    int fd = -1;
};

bool http_listen(HttpServer& server, const char* address, int port);
// handle whatever arrived within wait_ms; returns after at most that long
void http_poll(HttpServer& server, int wait_ms,
               const std::function<HttpReply(const HttpRequest&)>& handle);
void http_close(HttpServer& server);
