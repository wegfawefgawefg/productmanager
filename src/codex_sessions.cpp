// Codex chats and their subagents, found from ~/.codex/sessions.
//
// Codex keeps no "this chat is running" file like Claude Code does. A main
// thread counts as live while a codex process runs in its folder, or while its
// log was written in the last half hour. Subagents are their own logs whose
// header names the parent thread.

#include "codex_sessions.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#include "json.hpp"
#include "transcript_parts.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

// a thread with no live process and a log quiet this long is gone
static const double thread_quiet_gone = 30 * 60;

// the header of one rollout log, read once
struct ThreadInfo {
    std::string id;
    std::string cwd;
    std::string parent_id; // empty for a main thread
    std::string nickname; // subagents: "Cicero"
    std::string task; // subagents: "luna_doodle_1"
    int depth = 0;
    double started_at = 0;
};

struct CodexProcess {
    int pid;
    std::string cwd;
    double started_at;
};

static std::map<std::string, ThreadInfo> header_cache; // by log path
static std::map<std::string, std::string> thread_names; // thread id -> title
static double names_read_mtime = 0;

static double mtime_of(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return 0;
    }
    return (double)st.st_mtim.tv_sec + st.st_mtim.tv_nsec * 1e-9;
}

static bool read_header(const std::string& path, ThreadInfo& out) {
    std::ifstream in(path);
    std::string line;
    if (!std::getline(in, line)) {
        return false;
    }
    json row = json::parse(line, nullptr, false);
    if (!row.is_object() || str_field(row, "type") != "session_meta") {
        return false;
    }
    const json& meta = row["payload"];
    out.id = str_field(meta, "id");
    out.cwd = str_field(meta, "cwd");
    out.started_at = parse_timestamp(str_field(row, "timestamp"));
    const json& source = meta.contains("source") ? meta["source"] : json();
    if (source.is_object() && source.contains("subagent")) {
        const json& spawn = source["subagent"].value("thread_spawn", json::object());
        out.parent_id = str_field(spawn, "parent_thread_id");
        out.nickname = str_field(spawn, "agent_nickname");
        out.task = base_name(str_field(spawn, "agent_path"));
        out.depth = spawn.value("depth", 1);
    }
    return !out.id.empty();
}

// thread titles ("Make 4 moons doodling") live in session_index.jsonl
static void read_thread_names(const std::string& home) {
    std::string path = home + "/.codex/session_index.jsonl";
    double mtime = mtime_of(path);
    if (mtime == names_read_mtime) {
        return;
    }
    names_read_mtime = mtime;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        json row = json::parse(line, nullptr, false);
        if (row.is_object()) {
            thread_names[str_field(row, "id")] = str_field(row, "thread_name");
        }
    }
}

static double boot_time() {
    std::ifstream in("/proc/stat");
    std::string key;
    double value = 0;
    while (in >> key) {
        if (key == "btime") {
            in >> value;
            return value;
        }
        in.ignore(1 << 16, '\n');
    }
    return 0;
}

// running codex terminal sessions (not its background app-server)
static std::vector<CodexProcess> codex_processes() {
    std::vector<CodexProcess> out;
    double boot = boot_time();
    double tick = (double)sysconf(_SC_CLK_TCK);
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        std::string name = entry.path().filename().string();
        if (name.empty() || !isdigit((unsigned char)name[0])) {
            continue;
        }
        std::string dir = entry.path().string();
        std::ifstream comm_in(dir + "/comm");
        std::string comm;
        std::getline(comm_in, comm);
        if (comm != "codex") {
            continue;
        }
        std::ifstream cmd_in(dir + "/cmdline");
        std::string cmdline((std::istreambuf_iterator<char>(cmd_in)), {});
        if (cmdline.find("app-server") != std::string::npos) {
            continue;
        }
        std::ifstream stat_in(dir + "/stat");
        std::string stat_line;
        std::getline(stat_in, stat_line);
        std::istringstream rest(stat_line.substr(stat_line.rfind(')') + 2));
        std::string field;
        double start_ticks = 0;
        for (int i = 3; i <= 22 && rest >> field; i++) {
            if (i == 22) {
                start_ticks = atof(field.c_str());
            }
        }
        out.push_back({atoi(name.c_str()), fs::read_symlink(dir + "/cwd", ec).string(),
                       boot + start_ticks / tick});
    }
    return out;
}

// logs from today and yesterday, the only ones that can still be live
static std::vector<std::string> recent_logs(const std::string& home, double now) {
    std::vector<std::string> out;
    std::error_code ec;
    for (double t : {now, now - 86400}) {
        time_t secs = (time_t)t;
        for (bool utc : {false, true}) {
            struct tm tm;
            utc ? gmtime_r(&secs, &tm) : localtime_r(&secs, &tm);
            char day[32];
            strftime(day, sizeof day, "/%Y/%m/%d", &tm);
            std::string dir = home + "/.codex/sessions" + day;
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                std::string path = entry.path().string();
                if (entry.path().extension() == ".jsonl" &&
                    std::find(out.begin(), out.end(), path) == out.end()) {
                    out.push_back(path);
                }
            }
        }
    }
    return out;
}

static int made_up_pid(const std::string& thread_id) {
    return -(int)(std::hash<std::string>{}(thread_id) & 0x3fffffff) - 1;
}

static Chat* codex_chat(State& state, const std::string& thread_id) {
    for (Chat& chat : state.chats) {
        if (chat.kind == Kind::Codex && chat.session_id == thread_id) {
            return &chat;
        }
    }
    return nullptr;
}

// which process each main thread runs in: same folder, and the newest log
// started (or resumed) after the process did
static std::map<std::string, int> match_processes(
    const std::vector<std::pair<std::string, ThreadInfo>>& mains) {
    std::map<std::string, int> out;
    for (const CodexProcess& proc : codex_processes()) {
        std::string best;
        double best_mtime = 0;
        for (const auto& [path, info] : mains) {
            double mtime = mtime_of(path);
            if (info.cwd == proc.cwd && mtime >= proc.started_at - 5 && mtime > best_mtime &&
                !out.count(info.id)) {
                best = info.id;
                best_mtime = mtime;
            }
        }
        if (!best.empty()) {
            out[best] = proc.pid;
        }
    }
    return out;
}

static void update_main(State& state, const std::string& path, const ThreadInfo& info, int pid) {
    Chat* chat = codex_chat(state, info.id);
    if (!chat) {
        state.chats.push_back(Chat{});
        chat = &state.chats.back();
        chat->kind = Kind::Codex;
        chat->session_id = info.id;
        chat->tail.kind = Kind::Codex;
        chat->tail.path = path;
    }
    chat->pid = pid != 0 ? pid : made_up_pid(info.id);
    chat->cwd = info.cwd;
    chat->project_dir = fs::path(path).parent_path().string();
    auto name = thread_names.find(info.id);
    std::string folder = base_name(info.cwd);
    chat->name = name != thread_names.end() && !name->second.empty()
                     ? name->second
                     : "codex " + (folder.empty() ? std::string("~") : folder);
    chat->seen_at = state.now;
}

static std::string root_of(const std::map<std::string, ThreadInfo>& by_id, std::string id) {
    for (int guard = 0; guard < 16; guard++) {
        auto it = by_id.find(id);
        if (it == by_id.end() || it->second.parent_id.empty()) {
            return id;
        }
        id = it->second.parent_id;
    }
    return id;
}

static void update_agent(State& state, const std::string& path, const ThreadInfo& info,
                         const std::map<std::string, ThreadInfo>& by_id) {
    Chat* chat = codex_chat(state, root_of(by_id, info.parent_id));
    if (!chat) {
        return;
    }
    Agent* agent = nullptr;
    for (Agent& a : chat->agents) {
        if (a.id == info.id) {
            agent = &a;
        }
    }
    if (!agent) {
        Agent fresh;
        fresh.id = info.id;
        fresh.tail.kind = Kind::Codex;
        fresh.tail.path = path;
        fresh.tail.first_window = 16 * 1024 * 1024;
        chat->agents.push_back(fresh);
        agent = &chat->agents.back();
    }
    agent->description = info.nickname.empty() ? info.task : info.nickname + " (" + info.task + ")";
    agent->type = "codex";
    agent->depth = info.depth;
    // the parent is another subagent when it is not the main thread
    agent->parent_id = info.parent_id == chat->session_id ? "" : info.parent_id;
    agent->seen_at = state.now;
}

void scan_codex(State& state) {
    read_thread_names(state.home);
    std::vector<std::pair<std::string, ThreadInfo>> mains;
    std::vector<std::pair<std::string, ThreadInfo>> subs;
    std::map<std::string, ThreadInfo> by_id;
    for (const std::string& path : recent_logs(state.home, state.now)) {
        auto cached = header_cache.find(path);
        if (cached == header_cache.end()) {
            ThreadInfo info;
            if (!read_header(path, info)) {
                continue;
            }
            cached = header_cache.emplace(path, info).first;
        }
        const ThreadInfo& info = cached->second;
        by_id[info.id] = info;
        (info.parent_id.empty() ? mains : subs).push_back({path, info});
    }
    std::map<std::string, int> pids = match_processes(mains);
    for (const auto& [path, info] : mains) {
        auto pid = pids.find(info.id);
        bool live = pid != pids.end() || state.now - mtime_of(path) < thread_quiet_gone;
        if (live) {
            update_main(state, path, info, pid != pids.end() ? pid->second : 0);
        }
    }
    for (const auto& [path, info] : subs) {
        if (state.now - mtime_of(path) < thread_quiet_gone) {
            update_agent(state, path, info, by_id);
        }
    }
}

// a Codex chat is working while its turn is open and its log is moving;
// agents finish when their own task completes
void settle_codex(Chat& chat, double now) {
    bool open_turn = !chat.tail.turn_over && now - chat.tail.last_line_at < 10 * 60;
    chat.status = open_turn ? "busy" : "idle";
    for (Agent& agent : chat.agents) {
        agent.model = agent.tail.model;
        if (agent.tail.turn_over && !agent.finished && agent.tail.opened) {
            agent.finished = true;
            agent.finished_at = agent.tail.last_line_at;
        } else if (!agent.tail.turn_over && agent.finished) {
            agent.finished = false; // sent more work
        }
    }
    std::erase_if(chat.agents, [&](const Agent& a) {
        double linger = chat.crew_open ? 60 * 60 : 90;
        return a.seen_at + 2 < now || (a.finished && now - a.finished_at > linger);
    });
}
