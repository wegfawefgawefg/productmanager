#include "paths.hpp"

#include <filesystem>
#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

std::string runtime_dir() {
    const char* run = getenv("XDG_RUNTIME_DIR");
    std::string dir = std::string(run ? run : "/tmp") + "/" + app_name;
    std::error_code ec;
    fs::create_directories(dir + "/views", ec);
    return dir;
}

static std::string exe_of(const std::string& proc_link) {
    char buf[4096];
    ssize_t n = readlink(proc_link.c_str(), buf, sizeof buf - 1);
    if (n <= 0) {
        return "";
    }
    std::string path(buf, (size_t)n);
    // a binary replaced by a reinstall shows up as "<path> (deleted)"
    const std::string deleted = " (deleted)";
    if (path.size() > deleted.size() &&
        path.compare(path.size() - deleted.size(), deleted.size(), deleted) == 0) {
        path.resize(path.size() - deleted.size());
    }
    return path;
}

std::string own_exe() {
    std::string exe = exe_of("/proc/self/exe");
    return exe.empty() ? app_name : exe;
}

bool is_ours(int pid) {
    return pid > 0 && exe_of("/proc/" + std::to_string(pid) + "/exe") == own_exe();
}

std::string assets_dir() {
    std::error_code ec;
    fs::path installed = fs::path(own_exe()).parent_path().parent_path() / "share" / app_name;
    if (fs::exists(installed / "mono.ttf", ec)) {
        return installed.string();
    }
    return PM_SOURCE_ASSETS;
}

std::string home_dir() {
    const char* home = getenv("HOME");
    if (home && *home) {
        return home;
    }
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/";
}
