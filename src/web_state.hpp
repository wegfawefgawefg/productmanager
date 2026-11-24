#pragma once

#include <string>

#include "state.hpp"

// everything the phone page shows, as JSON
std::string state_json(const State& state);

// every image a chat's tiles looked at, newest first, with context
std::string shots_json(const State& state, int chat_pid);

// the image a tile shows now (shot < 0) or one from its history, or "" (only
// paths a tile looked at are ever served, never one named by the request)
std::string tile_image(const State& state, int chat_pid, const std::string& agent_id, int shot);
