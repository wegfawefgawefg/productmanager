#include "web_thumbs.hpp"

#include <algorithm>
#include <map>
#include <sys/stat.h>
#include <vector>

#include "stb_image.h"
#include "stb_image_write.h"

static const int thumb_w = 360;
static const int thumb_h = 240;
static const size_t cache_max = 400;

struct Cached {
    long mtime = 0;
    long size = 0;
    std::string jpeg;
};

static std::map<std::string, Cached> cache;

static void append(void* context, void* data, int size) {
    static_cast<std::string*>(context)->append(static_cast<char*>(data), size);
}

// box filter down to fit thumb_w x thumb_h, over black for transparent parts
static std::string make_thumb(const std::string& path) {
    int w = 0, h = 0, n = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!pixels) {
        return "";
    }
    double scale = std::min(1.0, std::min((double)thumb_w / w, (double)thumb_h / h));
    int tw = std::max(1, (int)(w * scale));
    int th = std::max(1, (int)(h * scale));
    std::vector<unsigned char> out(tw * th * 3);
    for (int y = 0; y < th; y++) {
        int y0 = y * h / th, y1 = std::max(y0 + 1, (y + 1) * h / th);
        for (int x = 0; x < tw; x++) {
            int x0 = x * w / tw, x1 = std::max(x0 + 1, (x + 1) * w / tw);
            long sum[3] = {0, 0, 0};
            long count = 0;
            for (int sy = y0; sy < y1; sy++) {
                for (int sx = x0; sx < x1; sx++) {
                    const unsigned char* p = pixels + (sy * w + sx) * 4;
                    for (int c = 0; c < 3; c++) {
                        sum[c] += p[c] * p[3] / 255;
                    }
                    count++;
                }
            }
            for (int c = 0; c < 3; c++) {
                out[(y * tw + x) * 3 + c] = (unsigned char)(sum[c] / count);
            }
        }
    }
    stbi_image_free(pixels);
    std::string jpeg;
    stbi_write_jpg_to_func(append, &jpeg, tw, th, 3, out.data(), 80);
    return jpeg;
}

std::string web_thumb(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return "";
    }
    auto it = cache.find(path);
    if (it != cache.end() && it->second.mtime == (long)st.st_mtime &&
        it->second.size == (long)st.st_size) {
        return it->second.jpeg;
    }
    if (cache.size() >= cache_max) {
        cache.clear();
    }
    Cached& entry = cache[path];
    entry.mtime = (long)st.st_mtime;
    entry.size = (long)st.st_size;
    entry.jpeg = make_thumb(path);
    return entry.jpeg;
}
