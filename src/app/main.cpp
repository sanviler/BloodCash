// BloodCash: Blood Money (Psygnosis, 1989) on Windows and Linux.
// The original 68000 code runs on a simplified Amiga 500 (Framework core); the game reads the user's own disk
// images (Disk.1 and Disk.2 of the WHDLoad install), recognised by their SHA-256 digests.
//
// Keys:
//   F1 / F2                   1 / 2 players (title screen), F3 music, F4 "patient" (as in the original)
//   control ports             set in the options menu: nothing, keyboard, mouse or any joystick for each port
//                             (player 1 plays on port 2, as on the Amiga)
//   keyboard joystick         numeric keypad 8/4/6/2 (7/9/1/3 diagonals), Right Ctrl = fire
//   Escape                    options menu (the game is paused): screen, picture, CRT filter, ports, intro, quit
//   Space / Return / fire     during the intro: skip it (straight to the title screen)
//   F5                        CRT filter on / off
//   F11                       window / fullscreen
//   F12                       screenshot (PNG next to the executable)
//   game controller           stick or d-pad = joystick, A = fire, Start = options menu (one per player)
// Picture: the display window of each screen (320x200 in the game, 320x256 in the intro), centred.
//   PIXEL PERFECT = largest integer scale (CRT-Pixie filter), FILL = full height at any scale (sharp pixels or
//   the CRT filter for non-integer scales). Both start with the CRT filter on.
// Texts: bloodcash_texts.txt next to the executable (or --texts <file>) replaces the game texts;
//        --export-texts <file> writes the original texts in that format and quits (see bc/texts.h).
// Options: --data <folder>  --fullscreen  --custom1 N (bits: 1 skip intro, 2 lives, 4 energy, 8 money, 16 max money)
//          --test-frames N (run N frames and quit)
#include "amiga/machine.h"
#include "ui/options_panel.h"
#include "bc/disks.h"
#include "bc/options.h"
#include "bc/texts.h"
#include "bc/view.h"
#include "input/input.h"
#include "input/keymap.h"
#include "platform/folder_dialog.h"
#include "platform/png_write.h"
#include "platform/settings.h"
#include "platform/video.h"
#include "version.h"
#include <SDL.h>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
using bc::kViewW; using bc::kViewH;                  // the largest picture (bc/view.h)

std::string toUtf8(const fs::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}
fs::path toPath(const std::string& utf8) { return fs::path(std::u8string(utf8.begin(), utf8.end())); }

void fail(const std::string& msg) {
    std::fprintf(stderr, "%s\n", msg.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "BloodCash", msg.c_str(), nullptr);
}

std::string portText(const lm::PortLabel& l) {
    switch (l.kind) {
        case lm::DeviceKind::Keyboard: return "KEYBOARD";
        case lm::DeviceKind::Mouse: return "MOUSE";
        case lm::DeviceKind::Joystick: return l.number ? "JOYSTICK " + std::to_string(l.number) : "JOYSTICK (OFF)";
        default: return "NOTHING";
    }
}
} // namespace

int main(int argc, char** argv) {
    std::string dataDir, textsPath, exportPath;
    bool fullscreen = false;
    uint32_t custom1 = 0;
    bool custom1Given = false;
    long testFrames = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--data") dataDir = next();
        else if (a == "--fullscreen") fullscreen = true;
        else if (a == "--custom1") { custom1 = uint32_t(std::strtoul(next().c_str(), nullptr, 0)); custom1Given = true; }
        else if (a == "--texts") textsPath = next();
        else if (a == "--export-texts") exportPath = next();
        else if (a == "--test-frames") testFrames = std::atol(next().c_str());
    }
    if (!exportPath.empty()) { custom1 |= bc::kSkipIntro; custom1Given = true; if (testFrames < 0) testFrames = 2000; }
    const bool testing = testFrames >= 0;

    dm::Video::hints();                              // before SDL_Init: real pixels with Windows display scaling
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) std::fprintf(stderr, "no audio: %s\n", SDL_GetError());
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) std::fprintf(stderr, "no game controllers: %s\n", SDL_GetError());

    fs::path exeDir = fs::current_path();
    if (char* b = SDL_GetBasePath()) { exeDir = toPath(b); SDL_free(b); }
    const std::string settingsPath = toUtf8(exeDir / "bloodcash.ini");
    dm::Settings settings;
    const bool haveSettings = !testing && settings.load(settingsPath);

    // the user's disks: --data, game_path in bloodcash.ini (default: next to the executable), the current folder
    auto resolve = [&](const std::string& p) {
        const fs::path q = toPath(p.empty() ? std::string(".") : p);
        return toUtf8((q.is_absolute() ? q : exeDir / q).lexically_normal());
    };
    auto& disks = bc::Disks::instance();
    const std::string gamePath = !dataDir.empty() ? dataDir : settings.get("game_path", ".");
    std::string msg;
    bool found = disks.locate({resolve(gamePath), toUtf8(fs::current_path())}, msg);
    while (!found) {
        if (testing || !dm::folderDialogAvailable()) { fail(msg); SDL_Quit(); return 1; }
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose folder..."},
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"}};
        const std::string text = msg + "\n\nChoose the folder that contains Disk.1 and Disk.2?";
        const SDL_MessageBoxData box{SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT, nullptr, "BloodCash",
                                     text.c_str(), 2, buttons, nullptr};
        int choice = 0;
        if (SDL_ShowMessageBox(&box, &choice) != 0 || choice != 1) { SDL_Quit(); return 1; }
        std::string chosen;
        if (!dm::chooseFolder("Choose the folder with Disk.1 and Disk.2 of Blood Money", toUtf8(exeDir), chosen)) continue;
        msg.clear();
        found = disks.locate({chosen}, msg);
        if (found) {
            std::error_code ec;
            const fs::path rel = fs::relative(toPath(chosen), exeDir, ec);
            const bool inside = !ec && !rel.empty() && rel.begin()->string() != "..";
            settings.set("game_path", inside ? toUtf8(rel) : chosen);
            settings.save(settingsPath);
        }
    }
    std::fprintf(stderr, "%s: %s\n", msg.c_str(), disks.folder().c_str());

    // settings
    bool skipIntro = settings.get("skip_intro", "off") == "on";
    bool crtOn = settings.get("crt", "on") == "on";
    bool fillMode = settings.get("picture", "pixel") == "fill";
    if (!fullscreen) fullscreen = settings.get("fullscreen", "off") == "on";
    if (!custom1Given) custom1 = skipIntro ? bc::kSkipIntro : 0;

    lm::InputPorts ports;
    if (haveSettings && !settings.get("port2").empty()) ports.load(settings.get("port1"), settings.get("port2"));
    else {
        // player 1 (port 2): first controller, else the keyboard; player 2 (port 1): second controller, else
        // the keyboard when player 1 has a controller (no mouse in Blood Money)
        ports.setDefaultsTwoPlayers(1);
    }

    dm::Video video;
    const std::string title = std::string("BloodCash ver. ") + BC_VERSION_STRING + " (Blood Money)";
    if (!video.init(title.c_str(), kViewW, kViewH, 3)) { fail(std::string("video: ") + SDL_GetError()); SDL_Quit(); return 1; }
    bool isFull = false;
    if (fullscreen) { video.toggleFullscreen(); isFull = true; }
    video.setCrt(crtOn);
    video.setMode(fillMode ? dm::Video::Mode::Fill : dm::Video::Mode::PixelPerfect);
    auto saveSettings = [&] {
        if (testing) return;
        settings.set("skip_intro", skipIntro ? "on" : "off");
        settings.set("crt", crtOn ? "on" : "off");
        settings.set("picture", fillMode ? "fill" : "pixel");
        settings.set("fullscreen", isFull ? "on" : "off");
        settings.set("port1", ports.save(0));
        settings.set("port2", ports.save(1));
        if (settings.get("game_path").empty()) settings.set("game_path", ".");
        settings.save(settingsPath);
    };

    bool running = true, shotRequested = false, resetRequested = false;
    std::unique_ptr<amiga::Machine> mp;
    std::string err;
    auto start = [&](uint32_t c1) {
        mp = std::make_unique<amiga::Machine>();
        mp->log = [](const std::string& s) { std::fprintf(stderr, "[amiga] %s\n", s.c_str()); };
        mp->setCustom1(c1);
        mp->onGamePatched = [&](amiga::Machine& mm) {
            if (!exportPath.empty()) {
                std::string e;
                if (bc::exportTexts(bc::scanTexts(mm), exportPath, e)) std::fprintf(stderr, "texts written to %s\n", exportPath.c_str());
                else std::fprintf(stderr, "%s\n", e.c_str());
                running = false;
                return;
            }
            const std::string tp = !textsPath.empty() ? textsPath : toUtf8(exeDir / "bloodcash_texts.txt");
            if (textsPath.empty() && !fs::exists(toPath(tp))) return;
            const auto r = bc::applyTexts(mm, tp);
            std::fprintf(stderr, "texts: %d changed\n", r.changed);
            for (const auto& w : r.warnings) std::fprintf(stderr, "texts: %s\n", w.c_str());
        };
        return mp->boot(toUtf8(exeDir), err);
    };
    if (!start(custom1)) { fail(err); SDL_Quit(); return 1; }

    SDL_AudioSpec want{}, have{};
    want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    const SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio) { mp->setAudioRate(have.freq); SDL_PauseAudioDevice(audio, 0); }
    const uint32_t bytesPerFrame = uint32_t(have.freq ? have.freq : 44100) / 50 * 4;

    dm::OptionsPanel menu;
    enum { ItFull, ItPicture, ItCrt, ItPort1, ItPort2, ItIntro, ItReset, ItQuit, ItBack, ItCount };
    bool amigaKeyDown[0x80] = {};
    auto releaseKeys = [&] {
        for (int k = 0; k < 0x80; ++k) if (amigaKeyDown[k]) { amigaKeyDown[k] = false; mp->keyEvent(uint8_t(k), false); }
    };
    auto activate = [&](int item, int dir) {        // dir 0 = Enter, -1 / +1 = left / right
        switch (item) {
            case ItFull: video.toggleFullscreen(); isFull = !isFull; break;
            case ItPicture: fillMode = !fillMode; crtOn = true; video.setCrt(true);      // each mode starts with its CRT filter
                video.setMode(fillMode ? dm::Video::Mode::Fill : dm::Video::Mode::PixelPerfect); break;
            case ItCrt: crtOn = !crtOn; video.setCrt(crtOn); break;
            case ItPort1: ports.cycle(0, dir ? dir : 1); break;
            case ItPort2: ports.cycle(1, dir ? dir : 1); break;
            case ItIntro: skipIntro = !skipIntro; break;
            case ItReset: if (!dir) { resetRequested = true; menu.open = false; } break;
            case ItQuit: if (!dir) running = false; break;
            case ItBack: if (!dir) menu.open = false; break;
        }
        saveSettings();
    };

    uint64_t nextTick = SDL_GetPerformanceCounter();
    const uint64_t perFrame = SDL_GetPerformanceFrequency() / 50;
    int shotN = 0;
    bc::ViewTracker viewTrack;
    bool skipNow = false;                        // Space / Return / fire during the intro
    uint64_t fpsStart = SDL_GetTicks64();
    int fpsFrames = 0;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ports.handleEvent(ev);
            switch (ev.type) {
                case SDL_QUIT: running = false; break;
                case SDL_CONTROLLERBUTTONDOWN: {
                    const int b = ev.cbutton.button;
                    if (b == SDL_CONTROLLER_BUTTON_START) { menu.open = !menu.open; menu.focus = 0; releaseKeys(); }
                    else if (!menu.open && bc::introRunning() && b == SDL_CONTROLLER_BUTTON_A) skipNow = true;
                    else if (menu.open) {
                        if (b == SDL_CONTROLLER_BUTTON_DPAD_UP) menu.move(-1, ItCount);
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) menu.move(1, ItCount);
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT) activate(menu.focus, -1);
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) activate(menu.focus, 1);
                        else if (b == SDL_CONTROLLER_BUTTON_A) activate(menu.focus, 0);
                        else if (b == SDL_CONTROLLER_BUTTON_B) menu.open = false;
                    }
                    break;
                }
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    const SDL_Scancode sc = ev.key.keysym.scancode;
                    const bool dn = ev.type == SDL_KEYDOWN;
                    if (dn && sc == SDL_SCANCODE_F11) { activate(ItFull, 1); break; }
                    if (dn && sc == SDL_SCANCODE_F12) { shotRequested = true; break; }
                    if (dn && sc == SDL_SCANCODE_F5) { activate(ItCrt, 1); break; }
                    if (menu.open) {
                        if (!dn) break;
                        if (sc == SDL_SCANCODE_ESCAPE) menu.open = false;
                        else if (sc == SDL_SCANCODE_UP) menu.move(-1, ItCount);
                        else if (sc == SDL_SCANCODE_DOWN) menu.move(1, ItCount);
                        else if (sc == SDL_SCANCODE_LEFT) activate(menu.focus, -1);
                        else if (sc == SDL_SCANCODE_RIGHT) activate(menu.focus, 1);
                        else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE) activate(menu.focus, 0);
                        break;
                    }
                    if (dn && sc == SDL_SCANCODE_ESCAPE) { menu.open = true; menu.focus = 0; releaseKeys(); break; }
                    if (dn && bc::introRunning() && (sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_RETURN ||
                        sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_RCTRL)) { skipNow = true; break; }
                    if (ports.isJoystickKey(sc) || ev.key.repeat) break;
                    const auto& km = lm::amigaKeymap();
                    if (auto it = km.find(sc); it != km.end() && amigaKeyDown[it->second] != dn) {
                        amigaKeyDown[it->second] = dn;
                        mp->keyEvent(it->second, dn);
                    }
                    break;
                }
                default: break;
            }
        }
        if (skipNow) {                               // the intro is skipped once (the INTRO setting stays as it is)
            skipNow = false;
            releaseKeys();
            if (!start(custom1 | bc::kSkipIntro)) { fail(err); break; }
            if (audio) { mp->setAudioRate(have.freq); SDL_ClearQueuedAudio(audio); }
        }
        if (resetRequested) {
            resetRequested = false;
            releaseKeys();
            if (!start((custom1 & ~bc::kSkipIntro) | (skipIntro ? bc::kSkipIntro : 0))) { fail(err); break; }
            if (audio) { mp->setAudioRate(have.freq); SDL_ClearQueuedAudio(audio); }
        }
        amiga::Machine& m = *mp;
        ports.apply(m, 0, menu.open);

        // 50 Hz: with audio the queue of samples sets the pace, otherwise the timer
        const bool runNow = testing || (audio ? SDL_GetQueuedAudioSize(audio) < bytesPerFrame * 3 : SDL_GetPerformanceCounter() >= nextTick);
        if (!runNow) { SDL_Delay(1); continue; }
        nextTick += perFrame;
        if (menu.open) {
            if (audio && SDL_GetQueuedAudioSize(audio) < bytesPerFrame * 3) {
                std::vector<int16_t> silence(bytesPerFrame / 2, 0);
                SDL_QueueAudio(audio, silence.data(), bytesPerFrame);
            }
        } else {
            m.runFrame();
            if (audio) {
                auto& a = m.audioOut();
                if (!a.empty()) SDL_QueueAudio(audio, a.data(), uint32_t(a.size() * sizeof(int16_t)));
            }
            const bc::ViewRect& vr = viewTrack.update(bc::viewRect(m));
            if (vr.w != video.width() || vr.h != video.height()) video.resizeLogical(vr.w, vr.h);
            bc::copyView(m, video.frame(), vr);
        }
        if (shotRequested) {                         // the picture without the menu, at 3x
            shotRequested = false;
            const int w = video.width(), h = video.height(), s = 3;
            std::vector<uint8_t> rgb(size_t(w) * h * s * s * 3);
            for (int y = 0; y < h * s; ++y)
                for (int x = 0; x < w * s; ++x) {
                    const dm::Rgb c = video.frame()[size_t(y / s) * w + x / s];
                    uint8_t* p = &rgb[(size_t(y) * w * s + x) * 3];
                    p[0] = c.r; p[1] = c.g; p[2] = c.b;
                }
            fs::path p;
            do { p = exeDir / ("bloodcash_" + std::to_string(++shotN) + ".png"); } while (fs::exists(p));
            dm::writePng(toUtf8(p), w * s, h * s, rgb);
        }
        if (menu.open) {
            const std::vector<dm::MenuItem> items = {
                {"SCREEN", isFull ? "FULL" : "WINDOW"},
                {"PICTURE", fillMode ? "FILL" : "PIXEL PERFECT"},
                {"CRT FILTER (F5)", crtOn ? "ON" : "OFF"},
                {"PORT 1 (PLAYER 2)", portText(ports.label(0))},
                {"PORT 2 (PLAYER 1)", portText(ports.label(1))},
                {"INTRO", skipIntro ? "SKIP" : "PLAY"},
                {"RESTART GAME", ""}, {"QUIT", ""}, {"BACK", ""}};
            menu.footer.clear();
            if (menu.focus == ItPort1 || menu.focus == ItPort2) menu.footer = ports.label(menu.focus - ItPort1).name;
            for (auto& ch : menu.footer) ch = char(std::toupper(static_cast<unsigned char>(ch)));
            // the panel is drawn over the filtered picture (never through the CRT filter), at its own scale
            const auto keep = video.frame();
            dm::OptionsPanel::darken(video.frame());
            menu.render(items);
            int ow = 0, oh = 0;
            video.outputSize(ow, oh);
            video.setOverlay(menu.pixels(), menu.width(), menu.height(), dm::OptionsPanel::scaleFor(ow, oh, menu.width(), menu.height()));
            video.present();
            video.clearOverlay();
            video.frame() = keep;
        } else video.present();
        ++fpsFrames;                                 // window title: name, version and the frames shown per second
        if (const uint64_t now = SDL_GetTicks64(); now - fpsStart >= 1000) {
            const int fps = int((fpsFrames * 1000 + (now - fpsStart) / 2) / (now - fpsStart));
            video.setTitle(title + " - " + std::to_string(fps) + " fps");
            fpsStart = now;
            fpsFrames = 0;
        }
        if (testing && long(m.frameCount()) >= testFrames) running = false;
    }
    saveSettings();
    if (audio) SDL_CloseAudioDevice(audio);
    video.shutdown();
    SDL_Quit();
    return 0;
}
