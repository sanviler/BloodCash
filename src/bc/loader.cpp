// Game loader: starts Blood Money on the machine without Kickstart and without disk drives.
//
// Blood Money takes over the whole Amiga: it does not use the operating system and reads its own disk format
// (Psygnosis tracks of 6,200 bytes). This file does in C++ what a WHDLoad install does, written from scratch
// (no slave code is used or needed):
//   - the boot code (2 tracks of disk 1) is put at $7A120 and started at $7A39A, as the boot block would do;
//   - the two track loaders (boot $7C41C, game $4066) are replaced by hooks copying from the disk images;
//     the drive motor and head routines do nothing; the disk change ($4322) swaps the image at once;
//   - the disk copy protection checks are bypassed (they read the original disks or checksum the code);
//   - the PAL/NTSC test always answers PAL (the original counts raster lines in a loop);
//   - the intro waits for the time that the disk loading used to take, so pictures and music stay in step;
//   - the high scores are kept in a file (BloodCash.high) instead of being written to the game disk;
//   - one fix in the music code: a byte stored into a word left its high byte undefined.
// Optional cheats and the intro skip come from custom1 (see bc/options.h).
#include "amiga/cpu.h"
#include "amiga/machine.h"
#include "bc/disks.h"
#include "bc/options.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace amiga {

namespace {
constexpr uint32_t kBootBase = 0x7A120, kBootEntry = 0x7A39A;
constexpr uint32_t kGameBase = 0x400, kGameEntry = 0x40C;
constexpr uint32_t kHighScores = 0x8194, kHighScoreSize = 0xFC, kHighTrackBuf = 0x7541E;
constexpr const char* kHighFile = "BloodCash.high";

int gDisk = 1;                 // disk in the drive
bool gBootActive = true;       // the boot hooks are valid only until the game starts
uint32_t gTodMark = 0;         // CIA-A TOD (frames) when an intro part started

// side: bit 2 of CIA-B port B (0 = upper head = side 1)
int side(Machine& m) { return (m.busRead8(0xBFD100) & 4) ? 0 : 1; }

bool readTracks(Machine& m, uint32_t dst, int track, int count) {
    const int s = side(m);
    if (dst + uint32_t(count) * bc::kTrackSize > Machine::kChipSize) return false;
    const bool ok = bc::Disks::instance().read(gDisk, s, track, count, m.chip() + dst, Machine::kChipSize - dst);
    if (std::getenv("BC_LOG_READS") && m.log) {
        char b[96]; std::snprintf(b, sizeof b, "read disk %d side %d track %d x%d -> $%X (frame %llu)", gDisk, s, track, count, dst,
                                  static_cast<unsigned long long>(m.frameCount()));
        m.log(b);
    }
    if (!ok && m.log) m.log("bad track read: disk " + std::to_string(gDisk) + " side " + std::to_string(s) + " track " + std::to_string(track));
    return ok;
}

// writes "value" at "addr" only if the original bytes are there
void patch16(Machine& m, uint32_t addr, uint16_t expect, uint16_t value) {
    if (m.peek16(addr) == expect) m.poke16(addr, value);
    else if (m.log) { char b[96]; std::snprintf(b, sizeof b, "patch skipped at $%X: $%04X", addr, m.peek16(addr)); m.log(b); }
}

void bra(Machine& m, uint32_t addr, uint32_t target) {      // bra.w addr -> target
    m.poke16(addr, 0x6000);
    m.poke16(addr + 2, uint16_t(target - addr - 2));
}

// the music code: "move.b 3(a6),8(a5)" stores a byte into the high half of a word that is read later as a
// whole; the word gets the byte in its low half and zero above (as the WHDLoad install does)
void installMusicFix(Machine& m, uint32_t addr, bool thenRts) {
    m.addHook(addr, [thenRts](Machine& mm) {
        Cpu& c = mm.cpu();
        const uint32_t pc = c.getPC();
        if (mm.peek16(pc) != 0x1B6E || mm.peek32(pc + 2) != 0x00030008) { c.execute(); return; }
        const uint32_t v = mm.peek8(uint32_t(c.getA(6)) + 3);
        mm.poke16(uint32_t(c.getA(5)) + 8, uint16_t(v));
        c.setD(0, v);
        if (thenRts) mm.returnFromSubroutine(); else mm.jumpTo(pc + 6);
    });
}

void afterLoad(Machine& m, uint32_t dst) {
    if (dst == 0x3D090 && m.peek32(0x6A084) == 0x610001A6) m.poke32(0x6A084, 0x600000CA);   // world protection
}

// hooks for the boot code: act only while the boot is running (the area is reused later)
void bootHook(Machine& m, uint32_t addr, Machine::Hook h) {
    m.addHook(addr, [h](Machine& mm) { if (gBootActive) h(mm); else mm.cpu().execute(); });
}
} // namespace

bool Machine::boot(const std::string& saveDir, std::string& err) {
    saveDir_ = saveDir;
    auto& disks = bc::Disks::instance();
    if (!disks.ready()) { err = "Blood Money disks not loaded"; return false; }
    gDisk = 1;
    gBootActive = true;
    std::fill(chip_.begin(), chip_.end(), 0);
    if (!disks.read(1, 0, 28, 2, chip() + kBootBase, kChipSize - kBootBase)) { err = "Disk.1: boot tracks missing"; return false; }

    const uint32_t c1 = custom1_;
    // --- boot code ---
    // PAL test: d1 = 300 lines (PAL), straight to the comparison
    bootHook(*this, 0x7A402, [](Machine& m) { m.cpu().setD(1, 300); m.cpu().setD(0, 0); m.jumpTo(0x7A412); });
    // track loader: a0 destination, d0 first track, d1 number of tracks, side from CIA-B
    bootHook(*this, 0x7C41C, [](Machine& m) {
        Cpu& c = m.cpu();
        readTracks(m, uint32_t(c.getA(0)), int(c.getD(0) & 0xFFFF), int(c.getD(1) & 0xFFFF));
        m.returnFromSubroutine();
    });
    for (uint32_t a : {0x7C340u, 0x7C540u, 0x7C586u, 0x7C5A2u}) poke16(a, 0x4E75);    // drive motor and heads
    // the intro waited for the loading: mark the time at each part, then wait for what the disk took
    for (uint32_t a : {0x7A4C0u, 0x7A5F0u})
        bootHook(*this, a, [](Machine& m) { gTodMark = m.ciaA_.tod; m.jumpTo(uint32_t(m.cpu().getPC()) + 6); });
    bootHook(*this, 0x7A528, [](Machine& m) {
        if (((m.ciaA_.tod - gTodMark) & 0xFFFFFF) < 150) return;              // wait (time goes on)
        m.cpu().setA(0, 0x7C7D6);
        m.jumpTo(0x7A52E);
    });
    bootHook(*this, 0x7A61A, [](Machine& m) {
        if (((m.ciaA_.tod - gTodMark) & 0xFFFFFF) < 100) return;
        m.cpu().setA(0, 0x7C6E2);
        const uint32_t sp = uint32_t(m.cpu().getA(7)) - 4;                    // bsr $7C640, back at $7A622
        m.cpu().setA(7, sp);
        m.poke32(sp, 0x7A622);
        m.jumpTo(0x7C640);
    });
    if (c1 & bc::kSkipIntro) { bra(*this, 0x7A446, 0x7A4C6); bra(*this, 0x7A504, 0x7A5F0); bra(*this, 0x7A616, 0x7A6FA); }
    // end of the boot: the original jumps into its protection; the game is at $400 now
    bootHook(*this, 0x7A72A, [](Machine& m) {
        gBootActive = false;
        m.installGameHooks(kGameBase);
        if (m.onGamePatched) m.onGamePatched(m);
        m.jumpTo(kGameEntry);
    });

    // port directions as left by the Kickstart (drive and LED lines are outputs)
    ciaA_.ddra = 0x03;
    ciaB_.ddra = 0xC0;
    ciaB_.ddrb = 0xFF;
    customWrite(0x100, 0);
    Cpu& c = *cpu_;
    c.reset();
    c.setSR(0x2700);
    c.setA(7, kChipSize);
    jumpTo(kBootEntry);
    lineStart_ = c.getClock();
    installMusicFix(*this, 0x2EB58, false);
    installMusicFix(*this, 0x2E9C6, true);
    installMusicFix(*this, 0xD8F2, false);
    installMusicFix(*this, 0xD760, true);
    gameBase_ = kGameBase;
    return true;
}

void Machine::installGameHooks(uint32_t base) {
    (void)base;
    // disk copy protection: checksums and disk checks
    patch16(*this, 0x3E00, 0x267C, 0x602C);
    patch16(*this, 0x2BEC, 0x43F9, 0x602A);
    patch16(*this, 0x44C2, 0x3039, 0x4E75);
    patch16(*this, 0x4994, 0x91C8, 0x4E75);
    patch16(*this, 0x314E, 0x207C, 0x6024);
    patch16(*this, 0x3E6A, 0x6100, 0x4E75);
    // drive motor routines
    poke16(0x42DE, 0x4E75);
    poke16(0x4292, 0x4E75);
    // "insert disk 2" message: skipped
    bra(*this, 0x4B6, 0x4C4);
    // PAL test
    addHook(0x4B1C, [](Machine& m) { m.cpu().setD(1, 300); m.cpu().setD(0, 0); m.jumpTo(0x4B2C); });
    // disk change: swap the image
    addHook(0x4322, [](Machine& m) { gDisk ^= 3; m.returnFromSubroutine(); });
    // track loader
    addHook(0x4066, [](Machine& m) {
        Cpu& c = m.cpu();
        const uint32_t dst = uint32_t(c.getA(0));
        readTracks(m, dst, int(c.getD(0) & 0xFFFF), int(c.getD(1) & 0xFFFF));
        afterLoad(m, dst);
        m.returnFromSubroutine();
    });
    // high scores: file in the save folder; without it the table on the game disk
    addHook(0x3F70, [](Machine& m) {
        std::ifstream f(fs::path(m.saveDir_) / kHighFile, std::ios::binary);
        std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
        if (d.size() == kHighScoreSize) {
            for (uint32_t i = 0; i < kHighScoreSize; ++i) m.poke8(kHighScores + i, d[i]);
        } else {
            m.busWrite8(0xBFD100, m.busRead8(0xBFD100) | 4);              // side 0
            if (readTracks(m, kHighTrackBuf, 13, 1))
                for (uint32_t i = 0; i < kHighScoreSize; ++i) m.poke8(kHighScores + i, m.peek8(kHighTrackBuf + i));
        }
        m.returnFromSubroutine();
    });
    const bool cheats = (custom1_ & bc::kCheatMask) != 0;
    addHook(0x3F3A, [cheats](Machine& m) {
        if (!cheats && !m.saveDir_.empty()) {
            std::error_code ec;
            fs::create_directories(m.saveDir_, ec);
            std::ofstream f(fs::path(m.saveDir_) / kHighFile, std::ios::binary);
            for (uint32_t i = 0; i < kHighScoreSize; ++i) f.put(char(m.peek8(kHighScores + i)));
        }
        m.returnFromSubroutine();
    });
    // cheats (testing)
    if (custom1_ & bc::kLives) poke8(0x67E, 0x4A);
    if (custom1_ & bc::kEnergy) { poke8(0x14CC, 0x4A); poke8(0x152C, 0x4A); poke8(0x2FB8, 0x4A); }
    if (custom1_ & bc::kMoney) { poke8(0x17EC, 0x4A); poke16(0x353A, 0); poke8(0x445E, 0x4A); poke8(0x446E, 0x4A); }
    if (custom1_ & bc::kMaxMoney) { poke16(0x8C7A, 0x03E7); poke16(0x2BBE, 0x03E7); }
}

} // namespace amiga

namespace bc {
bool introRunning() { return amiga::gBootActive; }
} // namespace bc
