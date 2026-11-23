#include "wm.hpp"

#include "focus.hpp"
#include "json.hpp"

using nlohmann::json;

static void walk(const json& node, const std::string& workspace,
                 std::map<long, std::string>& out) {
    std::string here = workspace;
    if (node.value("type", "") == "workspace") {
        here = node.value("name", "");
    }
    auto win = node.find("window");
    if (win != node.end() && win->is_number_integer() && !here.empty()) {
        out[win->get<long>()] = here;
    }
    for (const char* key : {"nodes", "floating_nodes"}) {
        auto kids = node.find(key);
        if (kids == node.end() || !kids->is_array()) {
            continue;
        }
        for (const json& kid : *kids) {
            walk(kid, here, out);
        }
    }
}

std::map<long, std::string> window_workspaces() {
    std::map<long, std::string> out;
    json tree = json::parse(run_program({"i3-msg", "-t", "get_tree"}), nullptr, false);
    if (tree.is_object()) {
        walk(tree, "", out);
    }
    return out;
}

void move_to_workspace(long window, const std::string& workspace) {
    std::string cmd = "[id=" + std::to_string(window) + "] move container to workspace \"" +
                      workspace + "\"";
    run_program({"i3-msg", "-q", cmd});
}
