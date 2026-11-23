#include "transcript.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <sys/stat.h>

#include "json.hpp"
#include "transcript_parts.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

static const size_t max_ticks = 4000;
static const size_t max_remark = 400;

const char* act_name(Act act) {
    switch (act) {
    case Act::Thinking:
        return "thinking";
    case Act::Reading:
        return "reading";
    case Act::Searching:
        return "searching";
    case Act::Editing:
        return "editing";
    case Act::Running:
        return "running";
    case Act::Building:
        return "building";
    case Act::Looking:
        return "looking";
    case Act::Web:
        return "web";
    case Act::Spawning:
        return "spawning";
    case Act::Talking:
        return "messaging";
    case Act::Publishing:
        return "publishing";
    case Act::Waiting:
        return "waiting";
    case Act::NeedsYou:
        return "your turn";
    case Act::Idle:
        return "idle";
    case Act::Done:
        return "done";
    }
    return "?";
}

std::string base_name(const std::string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool is_image_path(const std::string& path) {
    std::string ext = lower(fs::path(path).extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp" ||
           ext == ".bmp";
}

std::string str_field(const json& obj, const char* key) {
    if (!obj.is_object()) {
        return "";
    }
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_string()) {
        return "";
    }
    return it->get<std::string>();
}

std::string first_line(const std::string& s, size_t max_len) {
    std::string out = s.substr(0, s.find('\n'));
    if (out.size() > max_len) {
        out = out.substr(0, max_len);
    }
    return out;
}

bool looks_like_build(const std::string& cmd) {
    const char* words[] = {"cmake", "ninja", "make ", "make\n", "cargo build", "cargo run",
                           "g++",   "clang", "meson", "cargo test", "cc65", "tisple "};
    for (const char* w : words) {
        if (cmd.find(w) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// shell commands: show the description Claude gave, else the command itself
static Act classify_bash(const json& input, std::string& detail) {
    std::string cmd = str_field(input, "command");
    std::string desc = str_field(input, "description");
    detail = desc.empty() ? first_line(cmd, 60) : desc;
    if (looks_like_build(cmd)) {
        return Act::Building;
    }
    return Act::Running;
}

static Act classify_tool(const std::string& name, const json& input, std::string& detail,
                         std::string& image) {
    if (name == "Bash") {
        return classify_bash(input, detail);
    }
    if (name == "Read") {
        std::string path = str_field(input, "file_path");
        detail = base_name(path);
        if (is_image_path(path)) {
            image = path;
            return Act::Looking;
        }
        return Act::Reading;
    }
    if (name == "Grep" || name == "Glob") {
        detail = str_field(input, "pattern");
        return Act::Searching;
    }
    if (name == "Edit" || name == "Write" || name == "MultiEdit" || name == "NotebookEdit") {
        detail = base_name(str_field(input, "file_path"));
        if (detail.empty()) {
            detail = base_name(str_field(input, "notebook_path"));
        }
        return Act::Editing;
    }
    if (name == "WebSearch") {
        detail = str_field(input, "query");
        return Act::Web;
    }
    if (name == "WebFetch") {
        detail = str_field(input, "url");
        return Act::Web;
    }
    if (name == "Agent" || name == "Task" || name == "Workflow") {
        detail = str_field(input, "description");
        return Act::Spawning;
    }
    if (name == "SendMessage") {
        detail = "to " + str_field(input, "to");
        return Act::Talking;
    }
    if (name == "Artifact" || name == "SendUserFile") {
        detail = base_name(str_field(input, "file_path"));
        return Act::Publishing;
    }
    if (name == "AskUserQuestion" || name == "ExitPlanMode") {
        detail = "asking you";
        return Act::NeedsYou;
    }
    if (name == "Monitor" || name == "TaskOutput" || name == "ScheduleWakeup") {
        detail = str_field(input, "description");
        return Act::Waiting;
    }
    if (name.rfind("mcp__", 0) == 0) {
        detail = name.substr(5);
        return Act::Web;
    }
    detail = name;
    return Act::Thinking;
}

void set_act(Tail& tail, Act act, const std::string& detail, double now) {
    if (tail.act != act || tail.detail != detail) {
        tail.act_since = now;
    }
    tail.act = act;
    tail.detail = detail;
}

void add_tick(Tail& tail, Act act, const std::string& detail, double now) {
    if (tail.ticks.size() >= max_ticks) {
        tail.ticks.erase(tail.ticks.begin(), tail.ticks.begin() + max_ticks / 4);
    }
    tail.ticks.push_back({now, act, false, detail});
}

// squash whitespace so a remark reads as one line of prose
std::string one_line(const std::string& text) {
    std::string out;
    bool space = false;
    for (char c : text) {
        bool ws = c == '\n' || c == '\t' || c == ' ' || c == '\r';
        if (ws) {
            space = !out.empty();
            continue;
        }
        if (space) {
            out += ' ';
            space = false;
        }
        out += c;
        if (out.size() >= max_remark) {
            break;
        }
    }
    return out;
}

static void read_usage(Tail& tail, const json& msg) {
    std::string id = str_field(msg, "id");
    const json& usage = msg.contains("usage") ? msg["usage"] : json();
    if (!usage.is_object() || id == tail.last_message_id) {
        return;
    }
    // one reply is split over several lines that repeat its usage
    tail.last_message_id = id;
    tail.output_tokens += usage.value("output_tokens", (int64_t)0);
    tail.context_tokens = usage.value("input_tokens", (int64_t)0) +
                          usage.value("cache_read_input_tokens", (int64_t)0) +
                          usage.value("cache_creation_input_tokens", (int64_t)0);
}

static void read_assistant(Tail& tail, const json& msg, double now) {
    const json& content = msg["content"];
    if (!content.is_array()) {
        return;
    }
    for (const json& block : content) {
        std::string type = str_field(block, "type");
        if (type == "tool_use") {
            std::string detail;
            std::string image;
            Act act = classify_tool(str_field(block, "name"), block["input"], detail, image);
            set_act(tail, act, detail, now);
            tail.edit_path = act == Act::Editing ? str_field(block["input"], "file_path") : "";
            add_tick(tail, act, detail, now);
            tail.tools_used++;
            if (!image.empty()) {
                tail.image_path = image;
                tail.image_at = now;
            }
        } else if (type == "text") {
            set_act(tail, Act::Thinking, "writing", now);
            std::string said = one_line(str_field(block, "text"));
            if (!said.empty()) {
                tail.remark = said;
                tail.remark_at = now;
            }
        } else if (type == "thinking") {
            set_act(tail, Act::Thinking, "", now);
        }
    }
    tail.turn_over = str_field(msg, "stop_reason") == "end_turn";
    read_usage(tail, msg);
}

static void read_user(Tail& tail, const json& msg, double now) {
    const json& content = msg["content"];
    tail.turn_over = false;
    if (content.is_string()) {
        set_act(tail, Act::Thinking, "new prompt", now);
        return;
    }
    if (!content.is_array()) {
        return;
    }
    for (const json& block : content) {
        if (str_field(block, "type") == "tool_result") {
            bool failed = block.value("is_error", false);
            tail.error_streak = failed ? tail.error_streak + 1 : 0;
            if (failed && !tail.ticks.empty()) {
                tail.ticks.back().failed = true;
            }
            set_act(tail, Act::Thinking, "", now);
            return;
        }
    }
    set_act(tail, Act::Thinking, "new prompt", now);
}

// "2026-11-23T01:02:03.456Z" to unix seconds; 0 when it doesn't parse
double parse_timestamp(const std::string& text) {
    struct tm tm = {};
    int millis = 0;
    if (sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d.%dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &millis) < 6) {
        return 0;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return (double)timegm(&tm) + millis * 0.001;
}

static void read_line(Tail& tail, const std::string& line, double when) {
    if (tail.kind == Kind::Codex) {
        read_codex_line(tail, line, when);
        return;
    }
    // skip the bookkeeping lines cheaply; they are most of the file
    if (line.find("\"type\":\"assistant\"") == std::string::npos &&
        line.find("\"type\":\"user\"") == std::string::npos) {
        return;
    }
    json row = json::parse(line, nullptr, false);
    if (row.is_discarded() || !row.is_object()) {
        return;
    }
    double stamp = parse_timestamp(str_field(row, "timestamp"));
    if (stamp > 0) {
        when = stamp;
    }
    if (tail.started_at == 0) {
        tail.started_at = when;
    }
    std::string type = str_field(row, "type");
    if (type == "assistant") {
        read_assistant(tail, row["message"], when);
    } else if (type == "user" && !row.value("isMeta", false)) {
        read_user(tail, row["message"], when);
    } else {
        return;
    }
    tail.last_line_at = when;
}

void poll_tail(Tail& tail, double now) {
    struct stat st;
    if (stat(tail.path.c_str(), &st) != 0) {
        return;
    }
    uint64_t size = (uint64_t)st.st_size;
    double mtime = (double)st.st_mtim.tv_sec + st.st_mtim.tv_nsec * 1e-9;
    bool first = !tail.opened;
    if (first) {
        tail.opened = true;
        tail.offset = size > tail.first_window ? size - tail.first_window : 0;
        tail.act_since = mtime;
        tail.last_line_at = mtime;
    }
    if (size < tail.offset) {
        // rewritten from scratch
        tail.offset = 0;
        tail.partial.clear();
    }
    if (size == tail.offset) {
        return;
    }
    FILE* f = fopen(tail.path.c_str(), "rb");
    if (!f) {
        return;
    }
    fseek(f, (long)tail.offset, SEEK_SET);
    std::string chunk(size - tail.offset, '\0');
    size_t got = fread(chunk.data(), 1, chunk.size(), f);
    fclose(f);
    chunk.resize(got);
    tail.offset += got;

    std::string text = tail.partial + chunk;
    size_t start = 0;
    if (first && tail.offset - got > 0) {
        // landed mid-line; drop the piece before the first newline
        size_t nl = text.find('\n');
        start = nl == std::string::npos ? text.size() : nl + 1;
    }
    // on first open the old lines belong to when the file was last written
    double when = first ? mtime : now;
    while (true) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            break;
        }
        read_line(tail, text.substr(start, nl - start), when);
        start = nl + 1;
    }
    tail.partial = text.substr(start);
}
