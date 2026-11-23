#include "focus.hpp"

#include <SDL3/SDL.h>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

// run a program with arguments (no shell) and return what it printed
std::string run_program(const std::vector<std::string>& args) {
    int fds[2];
    if (pipe(fds) != 0) {
        return "";
    }
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);
        std::vector<char*> argv;
        for (const std::string& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(fds[1]);
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0) {
        out.append(buf, (size_t)n);
    }
    close(fds[0]);
    waitpid(pid, nullptr, 0);
    return out;
}

// start a program that outlives us (a new terminal window)
void launch_program(const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        if (fork() == 0) {
            // detached from the terminal that started us
            int null_fd = open("/dev/null", O_RDWR);
            if (null_fd >= 0) {
                dup2(null_fd, 0);
                dup2(null_fd, 1);
                dup2(null_fd, 2);
            }
            std::vector<char*> argv;
            for (const std::string& a : args) {
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            _exit(127);
        }
        _exit(0);
    }
    waitpid(pid, nullptr, 0);
}

static int parent_of(int pid) {
    std::ifstream in("/proc/" + std::to_string(pid) + "/stat");
    std::string stat;
    std::getline(in, stat);
    // the command name is in parens and may hold spaces; ppid follows ") S "
    size_t close = stat.rfind(')');
    if (close == std::string::npos) {
        return 0;
    }
    std::istringstream rest(stat.substr(close + 2));
    std::string state;
    int ppid = 0;
    rest >> state >> ppid;
    return ppid;
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

// the first X window owned by the process or one of its parents (windows on
// other i3 workspaces are unmapped, so visibility is not asked for)
static std::string window_of(int pid) {
    for (int p = pid; p > 1; p = parent_of(p)) {
        std::vector<std::string> wins =
            split_lines(run_program({"xdotool", "search", "--pid", std::to_string(p)}));
        if (!wins.empty()) {
            return wins.front();
        }
    }
    return "";
}

static void focus_window(const std::string& win) {
    if (win.empty()) {
        return;
    }
    run_program({"i3-msg", "-q", "[id=" + win + "] focus"});
}

struct Pane {
    std::string session;
    std::string window_id;
    std::string pane_id;
};

// the tmux pane whose process is the chat or one of its parents
static bool find_pane(int chat_pid, Pane& out) {
    std::string listing = run_program({"tmux", "list-panes", "-a", "-F",
                               "#{pane_pid} #{session_name} #{window_id} #{pane_id}"});
    std::vector<std::string> lines = split_lines(listing);
    for (int p = chat_pid; p > 1; p = parent_of(p)) {
        for (const std::string& line : lines) {
            std::istringstream in(line);
            int pane_pid = 0;
            Pane pane;
            in >> pane_pid >> pane.session >> pane.window_id >> pane.pane_id;
            if (pane_pid == p) {
                out = pane;
                return true;
            }
        }
    }
    return false;
}

static bool focus_in_tmux(const Chat& chat) {
    Pane pane;
    if (!find_pane(chat.pid, pane)) {
        return false;
    }
    run_program({"tmux", "select-window", "-t", pane.window_id});
    run_program({"tmux", "select-pane", "-t", pane.pane_id});
    std::vector<std::string> clients =
        split_lines(run_program({"tmux", "list-clients", "-F", "#{client_pid} #{client_session}"}));
    for (const std::string& line : clients) {
        std::istringstream in(line);
        int client_pid = 0;
        std::string session;
        in >> client_pid >> session;
        if (session == pane.session) {
            focus_window(window_of(client_pid));
            return true;
        }
    }
    // nobody is attached: open a terminal on it
    launch_program({"kitty", "tmux", "attach-session", "-t", "=" + pane.session});
    return true;
}

void focus_chat(const Chat& chat) {
    if (focus_in_tmux(chat)) {
        return;
    }
    std::string win = window_of(chat.pid);
    if (win.empty()) {
        SDL_Log("no window found for %s (pid %d)", chat.name.c_str(), chat.pid);
        return;
    }
    focus_window(win);
}

std::string chat_window(const Chat& chat) {
    Pane pane;
    if (!find_pane(chat.pid, pane)) {
        return window_of(chat.pid);
    }
    std::vector<std::string> clients = split_lines(
        run_program({"tmux", "list-clients", "-F", "#{client_pid} #{client_session}"}));
    for (const std::string& line : clients) {
        std::istringstream in(line);
        int client_pid = 0;
        std::string session;
        in >> client_pid >> session;
        if (session == pane.session) {
            return window_of(client_pid);
        }
    }
    return "";
}
