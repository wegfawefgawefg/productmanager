#pragma once

#include <string>

#include "state.hpp"

// everything the phone page shows, as JSON
std::string state_json(const State& state);

// the image a tile currently shows, or "" (only paths a tile points at are
// ever served, never one named by the request)
std::string tile_image(const State& state, int chat_pid, const std::string& agent_id);
