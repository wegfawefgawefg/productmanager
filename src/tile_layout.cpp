#include "tile_layout.hpp"

#include <cmath>

// k tiles side by side (row) or stacked (column), or 2x2 for four
static void split_rest(std::vector<Slot>& out, int k, float x, float y, float w, float h,
                       float gap, bool tall) {
    if (k == 4) {
        float cw = (w - gap) / 2;
        float ch = (h - gap) / 2;
        for (int i = 0; i < 4; i++) {
            out.push_back({x + (i % 2) * (cw + gap), y + (i / 2) * (ch + gap), cw, ch});
        }
        return;
    }
    for (int i = 0; i < k; i++) {
        if (tall) {
            float cw = (w - gap * (k - 1)) / k;
            out.push_back({x + i * (cw + gap), y, cw, h});
        } else {
            float ch = (h - gap * (k - 1)) / k;
            out.push_back({x, y + i * (ch + gap), w, ch});
        }
    }
}

static std::vector<Slot> grid(int n, float x, float y, float w, float h, float gap) {
    int best_cols = 1;
    float best_score = 1e9f;
    for (int cols = 1; cols <= n; cols++) {
        int rows = (n + cols - 1) / cols;
        float cw = (w - gap * (cols - 1)) / cols;
        float ch = (h - gap * (rows - 1)) / rows;
        // close to 1.4:1, and a penalty for empty spots in the last row
        float score = std::fabs(std::log((cw / ch) / 1.4f)) + 0.15f * (rows * cols - n);
        if (score < best_score) {
            best_score = score;
            best_cols = cols;
        }
    }
    int cols = best_cols;
    int rows = (n + cols - 1) / cols;
    float ch = (h - gap * (rows - 1)) / rows;
    std::vector<Slot> out;
    for (int row = 0; row < rows; row++) {
        int in_row = row == rows - 1 ? n - cols * (rows - 1) : cols;
        float cw = (w - gap * (in_row - 1)) / in_row;
        for (int c = 0; c < in_row; c++) {
            out.push_back({x + c * (cw + gap), y + row * (ch + gap), cw, ch});
        }
    }
    return out;
}

std::vector<Slot> layout_tiles(int n, float x, float y, float w, float h, float gap, bool master) {
    std::vector<Slot> out;
    if (n <= 0) {
        return out;
    }
    if (n == 1) {
        out.push_back({x, y, w, h});
        return out;
    }
    if (!master || n > 5) {
        return grid(n, x, y, w, h, gap);
    }
    bool tall = h >= w;
    if (tall) {
        float mh = (h - gap) / 2;
        out.push_back({x, y, w, mh});
        split_rest(out, n - 1, x, y + mh + gap, w, h - mh - gap, gap, true);
    } else {
        float mw = (w - gap) / 2;
        out.push_back({x, y, mw, h});
        split_rest(out, n - 1, x + mw + gap, y, w - mw - gap, h, gap, false);
    }
    return out;
}
