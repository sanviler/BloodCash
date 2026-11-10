// bcrun: BloodCash without a window, for tests (developer tool, not shipped).
// Usage: bcrun <folder with Disk.1/Disk.2> <frames> [--shot N file.png]... [--key N hexcode] [--joy N bits length]
//              [--custom1 N] [--wav file] [--dump N file] [--state N file]
// joystick bits (port 2): 1 up, 2 down, 4 left, 8 right, 16 fire; --joy1 for port 1 (set as joystick).
#include "amiga/cpu.h"
#include "amiga/machine.h"
#include "bc/disks.h"
#include "bc/texts.h"
#include "bc/view.h"
#include "platform/png_write.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

static void savePng(const amiga::Machine& m, const std::string& path) {
    const auto& fb = m.framebuffer();
    std::vector<uint8_t> rgb;
    rgb.reserve(fb.size() * 3);
    for (uint32_t c : fb) { rgb.push_back(uint8_t(c >> 16)); rgb.push_back(uint8_t(c >> 8)); rgb.push_back(uint8_t(c)); }
    dm::writePng(path, amiga::Machine::kFbWidth, amiga::Machine::kFbHeight, rgb);
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: bcrun <folder> <frames> [options]\n"); return 1; }
    std::string msg;
    if (!bc::Disks::instance().locate({argv[1]}, msg)) { std::fprintf(stderr, "%s\n", msg.c_str()); return 1; }
    std::fprintf(stderr, "%s\n", msg.c_str());
    amiga::Machine m;
    static uint32_t ring[256]; static unsigned ri = 0; static bool dumped = false;
    static amiga::Machine* mp = &m;
    if (std::getenv("BC_TRACE")) m.onStep = [](amiga::Machine& mm) { const uint32_t pc = uint32_t(mm.cpu().getPC()); if (ring[(ri - 1) & 255] != pc) ring[ri++ & 255] = pc; };
    m.log = [](const std::string& s) {
        std::fprintf(stderr, "[amiga] %s\n", s.c_str());
        if (std::getenv("BC_TRACE") && !dumped && s.find("exception") != std::string::npos) {
            dumped = true;
            for (unsigned k = 0; k < 256; ++k) std::fprintf(stderr, "%06X%c", ring[(ri + k) & 255], (k % 16 == 15) ? '\n' : ' ');
            std::fprintf(stderr, "A7=%08X\n", unsigned(mp->cpu().getA(7)));
        }
    };
    const int frames = std::atoi(argv[2]);
    std::multimap<int, std::string> shots, dumps, views;
    std::multimap<int, std::pair<int, bool>> keys;
    struct Joy { int start, len, bits, port; };
    std::vector<Joy> joys;
    std::string wav, texts, exportPath;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--shot" && i + 2 < argc) { shots.insert({std::atoi(argv[i + 1]), argv[i + 2]}); i += 2; }
        else if (a == "--view" && i + 2 < argc) { views.insert({std::atoi(argv[i + 1]), argv[i + 2]}); i += 2; }
        else if (a == "--dump" && i + 2 < argc) { dumps.insert({std::atoi(argv[i + 1]), argv[i + 2]}); i += 2; }
        else if (a == "--key" && i + 2 < argc) { const int f = std::atoi(argv[i + 1]); const int k = int(std::strtol(argv[i + 2], nullptr, 16));
            keys.insert({f, {k, true}}); keys.insert({f + 4, {k, false}}); i += 2; }
        else if ((a == "--joy" || a == "--joy1") && i + 3 < argc) {
            joys.push_back({std::atoi(argv[i + 1]), std::atoi(argv[i + 3]), std::atoi(argv[i + 2]), a == "--joy1" ? 0 : 1}); i += 3; }
        else if (a == "--custom1" && i + 1 < argc) m.setCustom1(uint32_t(std::strtoul(argv[++i], nullptr, 0)));
        else if (a == "--texts" && i + 1 < argc) texts = argv[++i];
        else if (a == "--export-texts" && i + 1 < argc) exportPath = argv[++i];
        else if (a == "--wav" && i + 1 < argc) wav = argv[++i];
    }
    m.setPortDevice(0, amiga::Machine::PortDevice::Joystick);
    m.onGamePatched = [&](amiga::Machine& mm) {
        if (!exportPath.empty()) { std::string e; if (!bc::exportTexts(bc::scanTexts(mm), exportPath, e)) std::fprintf(stderr, "%s\n", e.c_str()); }
        if (!texts.empty()) {
            const auto r = bc::applyTexts(mm, texts);
            std::fprintf(stderr, "texts: %d changed\n", r.changed);
            for (const auto& w : r.warnings) std::fprintf(stderr, "texts: %s\n", w.c_str());
        }
    };
    std::string err;
    if (!m.boot("/tmp/bcrun_saves", err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    std::vector<int16_t> pcm;
    for (int f = 0; f < frames && !m.aborted(); ++f) {
        for (auto it = keys.lower_bound(f); it != keys.upper_bound(f); ++it) m.keyEvent(uint8_t(it->second.first), it->second.second);
        int bits[2] = {0, 0};
        for (auto& j : joys) if (f >= j.start && f < j.start + j.len) bits[j.port] |= j.bits;
        for (int p = 0; p < 2; ++p)
            m.setPortJoystick(p, bits[p] & 1, bits[p] & 2, bits[p] & 4, bits[p] & 8, bits[p] & 16);
        m.runFrame();
        if (std::getenv("BC_LOG_DIW") && f % 100 == 0) std::fprintf(stderr, "diw %d %04X %04X ddf %04X %04X\n", f, m.customReg(0x8E), m.customReg(0x90), m.customReg(0x92), m.customReg(0x94));
        if (!wav.empty()) pcm.insert(pcm.end(), m.audioOut().begin(), m.audioOut().end());
        for (auto it = shots.lower_bound(f); it != shots.upper_bound(f); ++it) savePng(m, it->second);
        for (auto it = views.lower_bound(f); it != views.upper_bound(f); ++it) {
            std::vector<dm::Rgb> v;
            const bc::ViewRect r = bc::viewRect(m);
            bc::copyView(m, v, r);
            std::vector<uint8_t> rgb;
            for (auto c : v) { rgb.push_back(c.r); rgb.push_back(c.g); rgb.push_back(c.b); }
            dm::writePng(it->second, r.w, r.h, rgb);
        }
        for (auto it = dumps.lower_bound(f); it != dumps.upper_bound(f); ++it) {
            std::ofstream o(it->second, std::ios::binary);
            o.write(reinterpret_cast<const char*>(m.chip()), amiga::Machine::kChipSize);
        }
        if (f % 250 == 0) std::fprintf(stderr, "frame %d PC=%06X\n", f, unsigned(m.cpu().getPC()));
    }
    if (!wav.empty()) {
        std::ofstream o(wav, std::ios::binary);
        auto u32 = [&](uint32_t v) { o.write(reinterpret_cast<const char*>(&v), 4); };
        auto u16 = [&](uint16_t v) { o.write(reinterpret_cast<const char*>(&v), 2); };
        const uint32_t bytes = uint32_t(pcm.size() * 2);
        o.write("RIFF", 4); u32(36 + bytes); o.write("WAVEfmt ", 8); u32(16); u16(1); u16(2); u32(44100); u32(44100 * 4); u16(4); u16(16);
        o.write("data", 4); u32(bytes); o.write(reinterpret_cast<const char*>(pcm.data()), bytes);
    }
    std::fprintf(stderr, "exceptions:");
    for (int i = 2; i < 64; ++i) if (m.exceptionCount[i]) std::fprintf(stderr, " %d=%u", i, m.exceptionCount[i]);
    std::fprintf(stderr, "\n");
    return 0;
}
