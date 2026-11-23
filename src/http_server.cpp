#include "http_server.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

static const size_t max_request = 8192;

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && hex_value(s[i + 1]) >= 0 &&
            hex_value(s[i + 2]) >= 0) {
            out += (char)(hex_value(s[i + 1]) * 16 + hex_value(s[i + 2]));
            i += 2;
        } else {
            out += s[i] == '+' ? ' ' : s[i];
        }
    }
    return out;
}

std::string HttpRequest::param(const std::string& key) const {
    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        std::string pair = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        size_t eq = pair.find('=');
        if (pair.substr(0, eq) == key) {
            return eq == std::string::npos ? "" : url_decode(pair.substr(eq + 1));
        }
        if (amp == std::string::npos) {
            break;
        }
        pos = amp + 1;
    }
    return "";
}

bool http_listen(HttpServer& server, const char* address, int port) {
    server.fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (server.fd < 0) {
        return false;
    }
    int yes = 1;
    setsockopt(server.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, address, &addr.sin_addr);
    if (bind(server.fd, (sockaddr*)&addr, sizeof addr) != 0 || listen(server.fd, 16) != 0) {
        perror("productmanager serve");
        close(server.fd);
        server.fd = -1;
        return false;
    }
    return true;
}

static void send_all(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return;
        }
        sent += (size_t)n;
    }
}

static const char* status_text(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    default:
        return "Error";
    }
}

// read one request (short, from the same machine's Tailscale proxy) and answer it
static void serve_one(int fd, const std::function<HttpReply(const HttpRequest&)>& handle) {
    timeval timeout = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
    std::string request;
    char buf[2048];
    while (request.size() < max_request && request.find("\r\n\r\n") == std::string::npos) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n <= 0) {
            return;
        }
        request.append(buf, (size_t)n);
    }
    char method[16] = {0};
    char target[2048] = {0};
    sscanf(request.c_str(), "%15s %2047s", method, target);
    HttpReply reply;
    if (strcmp(method, "GET") != 0) {
        reply.status = 405;
    } else {
        HttpRequest req;
        std::string t = target;
        size_t q = t.find('?');
        req.path = url_decode(t.substr(0, q));
        req.query = q == std::string::npos ? "" : t.substr(q + 1);
        reply = handle(req);
    }
    char head[512];
    snprintf(head, sizeof head,
             "HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n%s"
             "Connection: close\r\n\r\n",
             reply.status, status_text(reply.status), reply.type.c_str(), reply.body.size(),
             reply.no_store ? "Cache-Control: no-store\r\n" : "");
    send_all(fd, head);
    send_all(fd, reply.body);
}

void http_poll(HttpServer& server, int wait_ms,
               const std::function<HttpReply(const HttpRequest&)>& handle) {
    if (server.fd < 0) {
        return;
    }
    pollfd p = {server.fd, POLLIN, 0};
    if (poll(&p, 1, wait_ms) <= 0) {
        return;
    }
    while (true) {
        int client = accept4(server.fd, nullptr, nullptr, SOCK_CLOEXEC);
        if (client < 0) {
            return;
        }
        serve_one(client, handle);
        close(client);
    }
}

void http_close(HttpServer& server) {
    if (server.fd >= 0) {
        close(server.fd);
    }
    server.fd = -1;
}
