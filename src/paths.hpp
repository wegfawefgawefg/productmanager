#pragma once

#include <string>

// the app's one name: command, window class, runtime and install folders
inline constexpr const char* app_name = "productmanager";

// $XDG_RUNTIME_DIR/productmanager, with its views/ folder, created on demand
std::string runtime_dir();
// fonts: <prefix>/share/productmanager when installed, else the source tree's assets
std::string assets_dir();
// $HOME, or the password database's home when it is unset
std::string home_dir();
// this program's executable
std::string own_exe();
// is pid a running copy of this same program (even after a reinstall replaced it)
bool is_ours(int pid);
