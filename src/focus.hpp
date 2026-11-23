#pragma once

#include <string>
#include <vector>

#include "state.hpp"

// bring the terminal running this chat to the front: through tmux when the
// chat lives in a tmux pane, else the kitty window that owns the process
void focus_chat(const Chat& chat);

// the X window id (decimal) of the terminal showing this chat, "" when none
std::string chat_window(const Chat& chat);

// run a program (no shell) and return what it printed
std::string run_program(const std::vector<std::string>& args);
// start a program that outlives us
void launch_program(const std::vector<std::string>& args);
