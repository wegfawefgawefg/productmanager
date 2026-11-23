#pragma once

// helpers shared by the Claude and Codex log readers

#include <string>

#include "json.hpp"
#include "state.hpp"

std::string base_name(const std::string& path);
bool is_image_path(const std::string& path);
// field readers that never throw: a missing, null or wrongly typed field
// gives the fallback (logs and session files do hold nulls)
std::string str_field(const nlohmann::json& obj, const char* key);
std::string str_or(const nlohmann::json& obj, const char* key, const std::string& fallback);
int64_t num_field(const nlohmann::json& obj, const char* key, int64_t fallback);
bool bool_field(const nlohmann::json& obj, const char* key, bool fallback);
// a member, or a null when it is missing (reading a missing member of a const
// json through [] is not allowed)
const nlohmann::json& field(const nlohmann::json& obj, const char* key);
std::string first_line(const std::string& s, size_t max_len);
bool looks_like_build(const std::string& cmd);
void set_act(Tail& tail, Act act, const std::string& detail, double now);
void add_tick(Tail& tail, Act act, const std::string& detail, double now);
std::string one_line(const std::string& text);
double parse_timestamp(const std::string& text);

// one line of a Codex rollout log
void read_codex_line(Tail& tail, const std::string& line, double when);
