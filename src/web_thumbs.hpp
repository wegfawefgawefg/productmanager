#pragma once

#include <string>

// a small JPEG of an image file for the phone's picture grid, or "" when the
// file can't be read; kept in memory, remade when the file changes
std::string web_thumb(const std::string& path);
