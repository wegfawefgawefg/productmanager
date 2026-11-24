#include "web_state.hpp"

#include <algorithm>
#include <cmath>

#include "json.hpp"

using nlohmann::json;

// the page draws barcodes from the last stretch of history only
static const size_t ticks_sent = 300;
// an image is "fresh" (fills the tile) this long after it was read
static const double image_fresh_for = 40;

static std::string folder_of(const std::string& path) {
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string trouble(const Tail& tail, bool finished, double now) {
    if (finished) {
        return "";
    }
    if (tail.error_streak >= 3) {
        return std::to_string(tail.error_streak) + " failed calls in a row";
    }
    if (tail.act != Act::Waiting && tail.act != Act::NeedsYou && tail.act != Act::Idle &&
        now - tail.last_line_at > 5 * 60) {
        return "silent for " + std::to_string((int)((now - tail.last_line_at) / 60)) + "m";
    }
    return "";
}

static json tail_json(const Tail& tail, double now) {
    json ticks = json::array();
    size_t first = tail.ticks.size() > ticks_sent ? tail.ticks.size() - ticks_sent : 0;
    for (size_t i = first; i < tail.ticks.size(); i++) {
        const Tick& t = tail.ticks[i];
        ticks.push_back({std::round((t.at - now) * 10) / 10, (int)t.act, t.failed ? 1 : 0,
                         t.detail});
    }
    return {
        {"act", (int)tail.act},
        {"detail", tail.detail},
        {"since", now - tail.act_since},
        {"remark", tail.remark},
        {"started", tail.started_at > 0 ? tail.started_at - now : 0},
        {"last_line", tail.last_line_at - now},
        {"out", tail.output_tokens},
        {"ctx", tail.context_tokens},
        {"calls", tail.tools_used},
        {"image", !tail.image_path.empty()},
        {"image_fresh", !tail.image_path.empty() && now - tail.image_at < image_fresh_for},
        // changes whenever a new image is read, so the page reloads it
        {"image_key", std::to_string((long)(tail.image_at * 1000))},
        {"image_name", folder_of(tail.image_path)},
        {"ticks", ticks},
        {"shots", tail.shots.size()},
    };
}

static json ticks_json(const std::vector<Tick>& ticks, double now) {
    json out = json::array();
    for (const Tick& t : ticks) {
        out.push_back({std::round((t.at - now) * 10) / 10, (int)t.act, t.failed ? 1 : 0, t.detail});
    }
    return out;
}

static void add_shots(json& out, const Tail& tail, const std::string& agent_id,
                      const std::string& who, double now) {
    for (size_t i = 0; i < tail.shots.size(); i++) {
        const Shot& s = tail.shots[i];
        out.push_back({
            {"agent", agent_id},
            {"who", who},
            {"n", i},
            {"at", s.at - now},
            {"name", folder_of(s.path)},
            {"said_before", s.said_before},
            {"said_after", s.said_after},
            {"before", ticks_json(s.before, now)},
            {"after", ticks_json(s.after, now)},
        });
    }
}

std::string shots_json(const State& state, int chat_pid) {
    json items = json::array();
    for (const Chat& chat : state.chats) {
        if (chat.pid != chat_pid) {
            continue;
        }
        add_shots(items, chat.tail, "", "leader", state.now);
        for (const Agent& agent : chat.agents) {
            add_shots(items, agent.tail, agent.id,
                      agent.description.empty() ? agent.id : agent.description, state.now);
        }
    }
    std::stable_sort(items.begin(), items.end(),
                     [](const json& a, const json& b) { return a["at"] > b["at"]; });
    return json{{"shots", items}}.dump();
}

static json agent_json(const Chat& chat, const Agent& agent, double now) {
    json j = tail_json(agent.tail, now);
    std::string parent;
    for (const Agent& other : chat.agents) {
        if (other.id == agent.parent_id) {
            parent = other.description;
        }
    }
    j["id"] = agent.id;
    j["name"] = agent.description.empty() ? agent.id : agent.description;
    j["model"] = agent.model;
    j["type"] = agent.type;
    j["parent"] = parent;
    j["finished"] = agent.finished;
    j["finished_ago"] = agent.finished ? now - agent.finished_at : 0;
    j["trouble"] = trouble(agent.tail, agent.finished, now);
    return j;
}

std::string state_json(const State& state) {
    json chats = json::array();
    for (const Chat& chat : state.chats) {
        json c = tail_json(chat.tail, state.now);
        c["pid"] = chat.pid;
        c["name"] = chat.name.empty() ? std::to_string(chat.pid) : chat.name;
        c["kind"] = chat.kind == Kind::Codex ? "codex" : "claude";
        c["folder"] = folder_of(chat.cwd);
        c["status"] = chat.status;
        c["open_url"] =
            chat.remote_id.empty() ? "" : "https://claude.ai/code/" + chat.remote_id;
        json agents = json::array();
        for (const Agent& agent : chat.agents) {
            agents.push_back(agent_json(chat, agent, state.now));
        }
        c["agents"] = agents;
        chats.push_back(c);
    }
    return json{{"now", state.now}, {"chats", chats}}.dump();
}

static std::string image_of(const Tail& tail, int shot) {
    if (shot < 0) {
        return tail.image_path;
    }
    return (size_t)shot < tail.shots.size() ? tail.shots[shot].path : "";
}

std::string tile_image(const State& state, int chat_pid, const std::string& agent_id, int shot) {
    for (const Chat& chat : state.chats) {
        if (chat.pid != chat_pid) {
            continue;
        }
        if (agent_id.empty()) {
            return image_of(chat.tail, shot);
        }
        for (const Agent& agent : chat.agents) {
            if (agent.id == agent_id) {
                return image_of(agent.tail, shot);
            }
        }
    }
    return "";
}
