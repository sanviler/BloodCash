#pragma once
// The picture shown in the window: the machine's frame (hires, 2 pixels per lores pixel) cut to the display
// window the game sets in DIWSTRT/DIWSTOP. The title, the shop and the levels use $2C81-$F4C1 (320x200), the
// first intro pictures 320x256: each screen fills the height of the window or screen and stays centred (the
// black below a 200-line screen is not part of the picture).
#include "amiga/machine.h"
#include "formats/palette.h"
#include <vector>

namespace bc {

constexpr int kViewW = 320, kViewH = 256;   // the largest picture (PAL), used before the game sets its window

struct ViewRect { int x, y, w, h; };

inline ViewRect viewRect(const amiga::Machine& m) {
    const uint16_t s = m.customReg(0x08E), e = m.customReg(0x090);
    const int hs = s & 0xFF, vs = s >> 8, he = (e & 0xFF) | 0x100, ve = (e >> 8) | ((e & 0x8000) ? 0 : 0x100);
    if (he - hs >= 256 && he - hs <= 352 && ve - vs >= 160 && ve - vs <= 288 && hs >= 0x50 && vs >= 0x1A)
        return {hs, vs, he - hs, ve - vs};
    return {0x81, 0x2C, kViewW, kViewH};
}

// a new window size is taken only when the game keeps it for `frames` frames (no jumps during fades)
struct ViewTracker {
    ViewRect cur{0x81, 0x2C, kViewW, kViewH}, cand = cur;
    int same = 0;
    const ViewRect& update(const ViewRect& r, int frames = 25) {
        if (r.x == cand.x && r.y == cand.y && r.w == cand.w && r.h == cand.h) ++same; else { cand = r; same = 1; }
        if (same >= frames) cur = cand;
        return cur;
    }
};

inline void copyView(const amiga::Machine& m, std::vector<dm::Rgb>& out, const ViewRect& r) {
    const auto& fb = m.framebuffer();
    out.resize(size_t(r.w) * r.h);
    const int x0 = r.x - amiga::Machine::kFbFirstLores, y0 = r.y - amiga::Machine::kFbFirstLine;
    for (int y = 0; y < r.h; ++y)
        for (int x = 0; x < r.w; ++x) {
            const int sx = (x0 + x) * 2, sy = y0 + y;
            uint32_t c = 0;
            if (sx >= 0 && sy >= 0 && sx < amiga::Machine::kFbWidth && sy < amiga::Machine::kFbHeight)
                c = fb[size_t(sy) * amiga::Machine::kFbWidth + size_t(sx)];
            out[size_t(y) * r.w + x] = dm::Rgb{uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)};
        }
}

} // namespace bc
