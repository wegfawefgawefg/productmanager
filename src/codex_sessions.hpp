#pragma once

#include "state.hpp"

// add or refresh the live Codex chats and their subagents in state.chats
// (each marks itself seen this scan, like the Claude chats)
void scan_codex(State& state);

// after reading new log lines: busy or idle, and which agents finished
void settle_codex(Chat& chat, double now);
