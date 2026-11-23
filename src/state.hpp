#pragma once

#include <cstdint>
#include <string>
#include <vector>

// what a chat or agent is doing right now, picked from its last log lines
enum class Act {
    Thinking, // model is working between tools
    Reading,
    Searching,
    Editing,
    Running, // a shell command
    Building, // a shell command that compiles
    Looking, // reading an image
    Web,
    Spawning, // launching agents
    Talking, // messaging another session
    Publishing,
    Waiting, // monitor / sleep / background wait
    NeedsYou, // the leader finished its turn
    Idle,
    Done, // an agent that has finished
};

// which tool wrote the log
enum class Kind { Claude, Codex };

// one tool call or reply in an agent's history
struct Tick {
    double at = 0;
    Act act = Act::Thinking;
    bool failed = false;
    std::string detail;
};

// one log file being followed, leader or agent
struct Tail {
    Kind kind = Kind::Claude;
    std::string path;
    uint64_t first_window = 512 * 1024; // how much of an old log is read on open
    uint64_t offset = 0;
    bool opened = false;
    std::string partial; // bytes after the last newline

    Act act = Act::Idle;
    std::string detail; // file name, command, pattern...
    std::string image_path; // last image read, for the thumbnail
    std::string edit_path; // full path of the file being edited, to spot clashes
    double act_since = 0.0; // seconds, monotonic clock of this program
    double last_line_at = 0.0;
    double image_at = -1000.0;
    int tools_used = 0;
    bool turn_over = false; // last assistant line ended the turn

    std::vector<Tick> ticks;
    std::string remark; // the last thing it said in plain text
    double remark_at = 0;
    double started_at = 0; // first line seen
    int64_t output_tokens = 0;
    int64_t context_tokens = 0; // size of the latest request
    std::string last_message_id;
    int error_streak = 0; // tool calls failing in a row
    std::string model; // when the log names it (Codex does)
    std::string thread_id; // Codex: its thread, for finding generated images
};

struct Agent {
    std::string id; // agent-xxxx
    std::string description;
    std::string model;
    std::string type; // general-purpose, Explore, fork...
    std::string parent_id; // empty when the leader launched it
    int depth = 1;
    bool finished = false;
    double finished_at = -1.0;
    double seen_at = 0.0;
    Tail tail;
};

struct Chat {
    Kind kind = Kind::Claude;
    int pid = 0; // Codex chats with no process found get a made-up negative id
    std::string session_id;
    std::string name;
    std::string status; // busy, idle, shell... from ~/.claude/sessions
    std::string cwd;
    std::string tmux; // "session:@win.%pane" when Claude knows it
    std::string project_dir; // ~/.claude/projects/<folder>
    double seen_at = 0.0;
    Tail tail;
    std::vector<Agent> agents;
    bool crew_open = false; // the big window shows this chat; keep finished agents longer
};

// a big viewer window, as the strip sees it
struct ViewInfo {
    int pid = 0;
    int pinned = 0; // the chat it was opened for; 0 = it shows its workspace's chats
    std::vector<int> shows;
};

// a clickable rectangle from the last drawn frame
struct HitBox {
    float x = 0, y = 0, w = 0, h = 0;
    int chat_pid = 0;
    std::string agent_id; // empty for the leader tile
    bool chip = false;
    int button = 0; // 1 opens a big viewer for the chat, 2 closes its viewers
};

struct State {
    std::string home;
    std::vector<Chat> chats;
    std::vector<HitBox> hits;
    std::vector<ViewInfo> views; // running big viewers, plus ones still starting
    double now = 0.0;
    double next_session_scan = 0.0;
    double next_tail_poll = 0.0;

    // window
    int win_w = 300;
    int win_h = 100;
    float mouse_x = -1, mouse_y = -1;
    int corner = 1; // 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left
};
