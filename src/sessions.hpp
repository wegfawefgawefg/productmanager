#pragma once

#include "state.hpp"

// find running Claude chats from ~/.claude/sessions and the agents each one
// has under its log folder; keeps what it already knows about each
void scan_sessions(State& state);

// read new log lines for every chat and agent
void poll_all_tails(State& state);

// wall-clock seconds, the same clock as file modification times
double wall_now();
