#include "views.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "json.hpp"
#include "paths.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

static std::string own_file() {
    return runtime_dir() + "/views/" + std::to_string(getpid()) + ".json";
}

void write_view(int pinned, const std::vector<int>& shows) {
    json info = {{"pid", (int)getpid()}, {"pinned", pinned}, {"shows", shows}};
    std::string path = own_file();
    std::ofstream out(path + ".tmp");
    out << info.dump();
    out.close();
    std::error_code ec;
    fs::rename(path + ".tmp", path, ec);
}

void remove_view() {
    std::error_code ec;
    fs::remove(own_file(), ec);
}

std::vector<ViewInfo> read_views() {
    std::vector<ViewInfo> views;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(runtime_dir() + "/views", ec)) {
        std::string path = entry.path().string();
        if (entry.path().extension() != ".json") {
            continue;
        }
        std::ifstream in(path);
        std::stringstream ss;
        ss << in.rdbuf();
        json info = json::parse(ss.str(), nullptr, false);
        if (!info.is_object()) {
            continue;
        }
        ViewInfo view;
        view.pid = info.value("pid", 0);
        view.pinned = info.value("pinned", 0);
        if (info.contains("shows") && info["shows"].is_array()) {
            view.shows = info["shows"].get<std::vector<int>>();
        }
        if (view.pid > 0 && is_ours(view.pid)) {
            views.push_back(view);
        } else {
            // a viewer that died without cleaning up
            fs::remove(path, ec);
        }
    }
    return views;
}
