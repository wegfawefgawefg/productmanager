#include "sessions.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <ctime>

#include "json.hpp"
#include "transcript_parts.hpp"
#include "codex_sessions.hpp"
#include "transcript.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

// an agent whose log has been quiet this long, and never said it finished,
// is treated as gone (its chat was closed, or it was killed)
static const double agent_stale_after = 30 * 60;
// finished agents stay this long: briefly in the strip, an hour in the crew window
static const double strip_done_linger = 90;
static const double crew_done_linger = 60 * 60;

static json read_json_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return json();
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return json::parse(ss.str(), nullptr, false);
}

static double file_mtime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return 0.0;
    }
    return (double)st.st_mtim.tv_sec + st.st_mtim.tv_nsec * 1e-9;
}

static bool pid_is_claude(int pid) {
    std::ifstream in("/proc/" + std::to_string(pid) + "/comm");
    std::string comm;
    std::getline(in, comm);
    return comm == "claude";
}

static std::string find_project_dir(const std::string& home, const std::string& session_id) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(home + "/.claude/projects", ec)) {
        std::string dir = entry.path().string();
        if (fs::exists(dir + "/" + session_id + ".jsonl", ec)) {
            return dir;
        }
    }
    return "";
}

static Chat* find_chat(State& state, int pid) {
    for (Chat& chat : state.chats) {
        if (chat.kind == Kind::Claude && chat.pid == pid) {
            return &chat;
        }
    }
    return nullptr;
}

static Agent* find_agent(Chat& chat, const std::string& id) {
    for (Agent& agent : chat.agents) {
        if (agent.id == id) {
            return &agent;
        }
    }
    return nullptr;
}

static void read_agent_meta(Agent& agent, const std::string& meta_path, double now) {
    json meta = read_json_file(meta_path);
    if (!meta.is_object()) {
        return;
    }
    agent.description = str_or(meta, "description", agent.description);
    agent.model = str_or(meta, "model", agent.model);
    agent.type = str_or(meta, "agentType", agent.type);
    // meta names the parent without the "agent-" its file name (our id) carries
    std::string parent = str_field(meta, "parentAgentId");
    if (!parent.empty()) {
        agent.parent_id = "agent-" + parent;
    }
    agent.depth = (int)num_field(meta, "spawnDepth", agent.depth);
    // once finished (or given up on as stale) an agent stays finished
    if (bool_field(meta, "finished", false) && !agent.finished) {
        agent.finished = true;
        agent.finished_at = now;
    }
}

// the agents folder can hold hundreds of old agents; only recent ones are read
static void scan_agents(Chat& chat, double now) {
    double linger = chat.crew_open ? crew_done_linger : strip_done_linger;
    std::string dir = chat.project_dir + "/" + chat.session_id + "/subagents";
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        std::string name = entry.path().filename().string();
        const std::string suffix = ".meta.json";
        if (name.size() <= suffix.size() ||
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
            continue;
        }
        std::string id = name.substr(0, name.size() - suffix.size());
        std::string log_path = dir + "/" + id + ".jsonl";
        double log_time = std::max(file_mtime(log_path), file_mtime(entry.path().string()));
        Agent* agent = find_agent(chat, id);
        if (!agent) {
            if (now - log_time > std::max(agent_stale_after, linger)) {
                continue;
            }
            Agent fresh;
            fresh.id = id;
            fresh.tail.path = log_path;
            // agent logs are small enough to read whole, for the full history
            fresh.tail.first_window = 16 * 1024 * 1024;
            read_agent_meta(fresh, entry.path().string(), now);
            if (fresh.finished) {
                // already done before we saw it: only show it if it just ended
                if (now - log_time > linger) {
                    continue;
                }
                fresh.finished_at = log_time;
            }
            chat.agents.push_back(fresh);
            agent = &chat.agents.back();
        } else {
            read_agent_meta(*agent, entry.path().string(), now);
        }
        agent->seen_at = now;
        if (!agent->finished && now - log_time > agent_stale_after && agent->finished_at < 0) {
            agent->finished = true;
            agent->finished_at = log_time;
        }
    }
    std::erase_if(chat.agents, [&](const Agent& a) {
        return a.seen_at != now || (a.finished && now - a.finished_at > linger);
    });
}

static void read_session_file(State& state, const std::string& path, double now) {
    json info = read_json_file(path);
    if (!info.is_object()) {
        return;
    }
    int pid = (int)num_field(info, "pid", 0);
    if (pid <= 0 || !pid_is_claude(pid)) {
        return;
    }
    Chat* chat = find_chat(state, pid);
    if (!chat) {
        state.chats.push_back(Chat{});
        chat = &state.chats.back();
        chat->pid = pid;
    }
    std::string session_id = str_field(info, "sessionId");
    if (session_id != chat->session_id) {
        // new chat in this process (/clear or /resume): start over
        chat->session_id = session_id;
        chat->project_dir = find_project_dir(state.home, session_id);
        chat->tail = Tail{};
        chat->tail.path = chat->project_dir + "/" + session_id + ".jsonl";
        chat->agents.clear();
    } else if (chat->project_dir.empty()) {
        chat->project_dir = find_project_dir(state.home, session_id);
        chat->tail.path = chat->project_dir + "/" + session_id + ".jsonl";
    }
    chat->name = str_field(info, "name");
    chat->status = str_field(info, "status");
    chat->cwd = str_field(info, "cwd");
    chat->tmux = str_field(info, "tmux");
    chat->remote_id = str_field(info, "bridgeSessionId");
    chat->seen_at = now;
    if (!chat->project_dir.empty()) {
        scan_agents(*chat, now);
    }
}

void scan_sessions(State& state) {
    double now = state.now;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(state.home + "/.claude/sessions", ec)) {
        if (entry.path().extension() == ".json") {
            read_session_file(state, entry.path().string(), now);
        }
    }
    scan_codex(state);
    std::erase_if(state.chats, [&](const Chat& c) { return c.seen_at != now; });
    std::sort(state.chats.begin(), state.chats.end(),
              [](const Chat& a, const Chat& b) { return a.name < b.name; });
}

// the leader's own log says what tool it runs; the session file says whether
// it is working at all, which is the more reliable of the two
static void settle_leader(Chat& chat, double now) {
    Tail& tail = chat.tail;
    if (chat.status == "busy") {
        if (tail.act == Act::NeedsYou || tail.act == Act::Idle) {
            tail.act = Act::Thinking;
            tail.detail = "";
            tail.act_since = now;
        }
        return;
    }
    if (tail.act == Act::NeedsYou && tail.detail == "asking you") {
        return;
    }
    // a turn that ended in the last ten minutes is waiting on you; older is idle
    Act want = now - tail.last_line_at < 10 * 60 ? Act::NeedsYou : Act::Idle;
    if (tail.act != want) {
        tail.act = want;
        tail.detail = chat.status == "shell" ? "shell" : "";
        tail.act_since = tail.last_line_at;
    }
}

void poll_all_tails(State& state) {
    for (Chat& chat : state.chats) {
        if (chat.project_dir.empty()) {
            continue;
        }
        poll_tail(chat.tail, state.now);
        for (Agent& agent : chat.agents) {
            poll_tail(agent.tail, state.now);
        }
        if (chat.kind == Kind::Codex) {
            settle_codex(chat, state.now);
        }
        settle_leader(chat, state.now);
        for (Agent& agent : chat.agents) {
            if (agent.finished) {
                agent.tail.act = Act::Done;
                agent.tail.detail = agent.model;
            }
        }
    }
}

double wall_now() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}
