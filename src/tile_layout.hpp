#pragma once

#include <vector>

struct Slot {
    float x, y, w, h;
};

// cut the area into n tiles that fill it completely, separated by gap.
// With a master (n up to 5) the first tile takes half the area, on top when
// the area is tall and on the left when wide, and the rest share the other
// half. Otherwise a grid as close to 1.4:1 tiles as fits, its last row
// stretched across the full width.
std::vector<Slot> layout_tiles(int n, float x, float y, float w, float h, float gap, bool master);
