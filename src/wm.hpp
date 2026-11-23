#pragma once

#include <map>
#include <string>

// i3: every X window id in the tree mapped to the name of its workspace
std::map<long, std::string> window_workspaces();
// i3: move one X window to a workspace by name, without following it
void move_to_workspace(long window, const std::string& workspace);
