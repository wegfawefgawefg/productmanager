#pragma once

#include <string>
#include <vector>

#include "state.hpp"

// big viewers say which chats they show through small files in
// $XDG_RUNTIME_DIR/productmanager/views, so the strip can offer open and close

// this viewer process: the chat it was opened for (0 = by workspace) and
// the chats it shows right now
void write_view(int pinned, const std::vector<int>& shows);
void remove_view();
// every running viewer
std::vector<ViewInfo> read_views();
