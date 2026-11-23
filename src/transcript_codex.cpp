// Codex rollout logs (~/.codex/sessions/YYYY/MM/DD/rollout-*.jsonl).
//
// Finished steps arrive as typed items (event_msg item_completed: commands,
// file changes, image views, remarks), which give the history. What is
// running right now only shows up as the tool call that starts it, either a
// direct function_call or, in code mode, an "exec" script calling tools.*.

#include <regex>

#include "paths.hpp"
#include "transcript_parts.hpp"

using nlohmann::json;

// "file:///home/x/a.png" to "/home/x/a.png"
static std::string strip_file_url(const std::string& path) {
    const std::string scheme = "file://";
    return path.rfind(scheme, 0) == 0 ? path.substr(scheme.size()) : path;
}

static Act command_act(const std::string& kind, const std::string& cmd) {
    if (kind == "read") {
        return Act::Reading;
    }
    if (kind == "search" || kind == "list_files") {
        return Act::Searching;
    }
    return looks_like_build(cmd) ? Act::Building : Act::Running;
}

// the script a command runs: Codex stores ["/bin/bash", "-lc", "<script>"]
static std::string command_text(const json& command) {
    if (command.is_string()) {
        return command.get<std::string>();
    }
    if (command.is_array() && !command.empty() && command.back().is_string()) {
        return command.back().get<std::string>();
    }
    return "";
}

static void finish_tick(Tail& tail, Act act, const std::string& detail, bool failed, double when) {
    add_tick(tail, act, detail, when);
    tail.ticks.back().failed = failed;
    tail.error_streak = failed ? tail.error_streak + 1 : 0;
    tail.tools_used++;
}

static void read_command(Tail& tail, const json& item, double when) {
    std::string cmd = command_text(item.value("command", json()));
    std::string kind;
    std::string detail = first_line(cmd, 70);
    const json& parsed = item.contains("parsed_cmd") ? item["parsed_cmd"] : json();
    if (parsed.is_array() && !parsed.empty() && parsed[0].is_object()) {
        kind = str_field(parsed[0], "type");
        std::string name = str_field(parsed[0], "name");
        if (!name.empty()) {
            detail = name;
        }
    }
    bool failed = item.value("exit_code", 0) != 0;
    finish_tick(tail, command_act(kind, cmd), detail, failed, when);
}

static void read_file_change(Tail& tail, const json& item, double when) {
    const json& changes = item.contains("changes") ? item["changes"] : json();
    std::string path;
    if (changes.is_object() && !changes.empty()) {
        path = changes.begin().key();
    }
    tail.edit_path = path;
    finish_tick(tail, Act::Editing, base_name(path), false, when);
}

static void show_image(Tail& tail, const std::string& path, double when) {
    tail.image_path = path;
    tail.image_at = when;
}

static void read_item(Tail& tail, const json& item, double when) {
    std::string type = str_field(item, "type");
    if (type == "CommandExecution") {
        read_command(tail, item, when);
    } else if (type == "FileChange") {
        read_file_change(tail, item, when);
    } else if (type == "ImageView") {
        std::string path = strip_file_url(str_field(item, "path"));
        finish_tick(tail, Act::Looking, base_name(path), false, when);
        show_image(tail, path, when);
    } else if (type == "Extension" && str_field(item, "kind") == "image_gen.generation") {
        // generated images are saved as generated_images/<thread>/<item id>.png
        std::string path = home_dir() + "/.codex/generated_images/" + tail.thread_id + "/" +
                           str_field(item, "id") + ".png";
        finish_tick(tail, Act::Publishing, "generated an image", false, when);
        show_image(tail, path, when);
        set_act(tail, Act::Looking, "generated image", when);
    } else if (type == "AgentMessage") {
        const json& content = item.contains("content") ? item["content"] : json();
        if (content.is_array() && !content.empty()) {
            std::string said = one_line(str_field(content[0], "text"));
            if (!said.empty()) {
                tail.remark = said;
                tail.remark_at = when;
            }
        }
        set_act(tail, Act::Thinking, "writing", when);
    } else if (type == "UserMessage") {
        tail.turn_over = false;
        set_act(tail, Act::Thinking, "new prompt", when);
    }
}

// what a code-mode "exec" script starts: its first tools.* call
static void read_exec_script(Tail& tail, const std::string& script, double when) {
    static const std::regex call(R"(tools\.(\w+)\()");
    static const std::regex cmd_arg(R"re("cmd"\s*:\s*"((?:[^"\\]|\\.)*)")re");
    static const std::regex patch_file(R"(\*\*\* (?:Update|Add|Delete) File: ([^\\\n"]+))");
    static const std::regex path_arg(R"re("path"\s*:\s*"([^"]*)")re");
    std::smatch m;
    std::string tool = std::regex_search(script, m, call) ? m[1].str() : "";
    if (tool == "exec_command" || tool == "shell") {
        std::string cmd = std::regex_search(script, m, cmd_arg) ? m[1].str() : "";
        set_act(tail, looks_like_build(cmd) ? Act::Building : Act::Running, first_line(cmd, 70),
                when);
    } else if (tool == "apply_patch") {
        std::string file = std::regex_search(script, m, patch_file) ? m[1].str() : "";
        set_act(tail, Act::Editing, base_name(file), when);
    } else if (tool == "view_image") {
        std::string path = std::regex_search(script, m, path_arg) ? m[1].str() : "";
        set_act(tail, Act::Looking, base_name(path), when);
    } else if (!tool.empty()) {
        set_act(tail, Act::Thinking, tool, when);
    }
}

static void read_function_call(Tail& tail, const std::string& name, const json& args,
                               double when) {
    if (name == "spawn_agent") {
        set_act(tail, Act::Spawning, str_field(args, "task_name"), when);
        add_tick(tail, Act::Spawning, str_field(args, "task_name"), when);
        tail.tools_used++;
    } else if (name == "wait_agent" || name == "wait") {
        set_act(tail, Act::Waiting, "its agents", when);
        add_tick(tail, Act::Waiting, "its agents", when);
        tail.tools_used++;
    } else if (name == "exec_command" || name == "shell") {
        std::string cmd = command_text(args.contains("cmd") ? args["cmd"] : args.value("command", json()));
        set_act(tail, looks_like_build(cmd) ? Act::Building : Act::Running, first_line(cmd, 70),
                when);
    } else if (name == "apply_patch") {
        set_act(tail, Act::Editing, "patch", when);
    } else if (name == "view_image") {
        set_act(tail, Act::Looking, base_name(str_field(args, "path")), when);
    } else if (name.rfind("mcp__", 0) == 0 || name == "web_search") {
        set_act(tail, Act::Web, name, when);
    } else {
        set_act(tail, Act::Thinking, name, when);
    }
}

static void read_response_item(Tail& tail, const json& item, double when) {
    std::string type = str_field(item, "type");
    std::string name = str_field(item, "name");
    if (type == "custom_tool_call") {
        if (name == "exec") {
            read_exec_script(tail, str_field(item, "input"), when);
        } else {
            set_act(tail, Act::Thinking, name, when);
        }
    } else if (type == "function_call") {
        json args = json::parse(str_field(item, "arguments"), nullptr, false);
        read_function_call(tail, name, args.is_object() ? args : json::object(), when);
    } else if (type == "reasoning") {
        set_act(tail, Act::Thinking, "", when);
    }
}

static void read_tokens(Tail& tail, const json& info) {
    if (!info.is_object()) {
        return;
    }
    if (info.contains("total_token_usage")) {
        tail.output_tokens = info["total_token_usage"].value("output_tokens", (int64_t)0);
    }
    if (info.contains("last_token_usage")) {
        tail.context_tokens = info["last_token_usage"].value("input_tokens", (int64_t)0);
    }
}

void read_codex_line(Tail& tail, const std::string& line, double when) {
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
    const json& payload = row.contains("payload") ? row["payload"] : json();
    std::string ptype = str_field(payload, "type");
    if (type == "session_meta") {
        // a subagent's log repeats its parent's header later on; keep the first
        if (tail.thread_id.empty()) {
            tail.thread_id = str_field(payload, "id");
        }
        return;
    }
    if (type == "turn_context") {
        tail.model = str_field(payload, "model");
        return;
    }
    if (type == "response_item") {
        read_response_item(tail, payload, when);
    } else if (type == "event_msg" && ptype == "item_completed") {
        read_item(tail, payload.contains("item") ? payload["item"] : json(), when);
    } else if (type == "event_msg" && ptype == "task_started") {
        tail.turn_over = false;
    } else if (type == "event_msg" && (ptype == "task_complete" || ptype == "turn_aborted")) {
        tail.turn_over = true;
    } else if (type == "event_msg" && ptype == "token_count") {
        read_tokens(tail, payload.contains("info") ? payload["info"] : json());
        return;
    } else {
        return;
    }
    tail.last_line_at = when;
}
