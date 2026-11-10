# BloodCash

**BloodCash** is a free, open-source port of the Amiga shoot-'em-up **Blood Money** (DMA Design / Psygnosis, 1989)
to Windows and Linux. It runs the original game, unchanged, on a small Amiga 500 written in C++, with sharp
integer or full-screen scaling with CRT filters, configurable control ports, fullscreen and translatable texts.

> **No part of the original game is included in this project.** BloodCash contains no game code, graphics,
> sound, text, ROM or disk image. To play you need your own copy of Blood Money (see [Game files](#game-files)).
> Blood Money and all its contents are the property of their respective owners. The author of BloodCash claims
> no rights over the game. BloodCash is not affiliated with or endorsed by DMA Design, Psygnosis or any rights
> holder.

---

> **Made largely with AI.** Most of BloodCash (the analysis of the original game, the code and the
> documentation) was written by an AI assistant (Anthropic's Claude), directed, tested and reviewed by the
> author.

---

## Features

- **The original game, as it was.** The original 68000 code runs on an emulated Amiga 500 (CPU, copper, blitter,
  bitplanes, sprites, Paula audio, CIAs, keyboard and joystick ports). No Kickstart ROM is needed.
- **No disk drive.** The game's own track loaders are replaced: the tracks come straight from the disk images,
  without loading times; the "insert disk 2" request is skipped.
- The intro keeps its timing (it used to wait for the disk), and can be skipped from the options menu.
- **High scores are saved** in `BloodCash.high`, next to the executable (not when a cheat is on).
- Each screen shown as its display window (320×200 in the game, 320×256 in the first intro pictures), centred,
  in a window (half the desktop, resizable) or fullscreen; lossless screenshots; frames per second in the window
  title.
- **Picture modes** (options menu, PICTURE): PIXEL PERFECT = the largest integer scale that fits the window or
  the desktop, black borders, with the CRT-Pixie filter computed exactly at that scale; FILL = the full height of
  the window or screen at any scale, with sharp pixels (sharp bilinear: no uneven pixels) or a CRT filter made
  for non-integer scales (every scanline gets the same light). Both start with their CRT filter on; F5 switches
  it off and on. Windows display scaling (125%, 150%) no longer blurs or shrinks the picture.
- The intro can be skipped with Space, Return, Right Ctrl or fire (A) on a controller.
- Both Amiga control ports configurable: keyboard, mouse or any joystick or game controller.
- The game texts can be replaced by a text file (for example a translation, see [Texts](#texts)).
- One fix of the original: in the music code a byte was stored into a word and left its upper half undefined.

## Game files

BloodCash needs **`Disk.1` and `Disk.2` of the WHDLoad installation of Blood Money** (by Wepl and StingRay,
versions 1.4 and 1.5a; 979,600 bytes each). They are recognised by their SHA-256 digests.

By default BloodCash looks for them **in the folder of the executable** (also in a `BloodMoney` or `data`
subfolder): copy the two files next to `bloodcash.exe` and start it. To keep them somewhere else, set the folder
in `bloodcash.ini` (next to the executable), relative to the executable or absolute:

```
game_path=.                      # default: next to the executable
game_path=BloodMoney             # a folder next to the executable
game_path=D:/Games/Amiga/BloodMoney
```

If the files are not there, BloodCash opens the system's folder picker and saves the choice in `game_path`
(on Linux the picker needs `zenity` or `kdialog`). The command line option `--data <folder>` overrides the setting.

## Controls

| Key | Action |
|---|---|
| F1 / F2 | 1 / 2 players (title screen) |
| F3 / F4 | music or sound effects / "patient" or "impatient" (title screen, as in the original) |
| Joystick in port 2 | player 1 (as on the Amiga) |
| Joystick in port 1 | player 2 |
| Keyboard joystick | numeric keypad 8 4 6 2 (7 9 1 3 diagonals), Right Ctrl = fire |
| Escape | options menu (the game is paused) |
| F5 | CRT filter on / off |
| Space / Return / Right Ctrl / A | skip the intro |
| F11 | window / fullscreen |
| F12 | screenshot (PNG next to the executable) |
| Game controller | stick or d-pad = joystick, A = fire, Start = options menu (one controller per player) |

The options menu sets: window or fullscreen, the picture (pixel perfect or fill), the CRT filter, the device in each control
port (nothing, keyboard, mouse or any joystick), the intro (play or skip); it can restart the game or quit.
The settings are kept in `bloodcash.ini`. On the first start port 2 (player 1) gets the first controller (or the
keyboard), port 1 (player 2) the second controller, or the keyboard when player 1 has a controller.

## Texts

`bloodcash --export-texts texts.txt` writes the texts of the game (title, shop, game over, high scores) into a
UTF-8 file, one line per text:

```
7DA0 "BLOOD MONEY" = "BLOOD MONEY"
```

Change the text after `=` and save the file as `bloodcash_texts.txt` next to the executable (or start with
`--texts <file>`). The game font has upper case letters, digits, space and `. , $ @` only, and a text cannot be
longer than the original (shorter texts are centred); score fields such as `$0000` must stay in place. Texts that
break a rule are left in English and reported on the console. The file contains the original texts: keep it
private.

## Command line

```
bloodcash [--data <folder>] [--fullscreen] [--texts <file>] [--export-texts <file>] [--custom1 N]
```

`--custom1` takes the sum of: 1 skip the intro, 2 unlimited lives, 4 unlimited energy, 8 unlimited money,
16 start with the most money (testing; high scores are not saved with a cheat on).

## Building

C++20 and SDL2. The Amiga core (68000 CPU from [Moira](https://github.com/dirkwhoffmann/Moira) by Dirk W. Hoffmann,
MIT licence, and the chipset) is in `core/`.

- **Windows** (Visual Studio 2022 + vcpkg): `cmake --preset windows-vcpkg` then `cmake --build --preset release`;
  the result is a single `bloodcash.exe` with SDL2 linked in. `scripts/package.ps1` makes the zip.
- **Linux / macOS**: install SDL2 (`libsdl2-dev`, `brew install sdl2`), then `cmake --preset linux` and
  `cmake --build build -j`. `-DBC_LINUX_PORTABLE=ON` makes an executable for other distributions (glibc 2.34+).
- `-DBC_BUILD_TOOLS=ON` adds `bcrun`, the game without a window for tests (screenshots, keys, joystick scripts).

The GitHub workflow builds Windows, Linux and macOS at every push and attaches the packages to a release for every
`v*` tag.

## Roadmap

- Widescreen view: the level redrawn in C++ from the state of the game, wider than the original screen.
- More of the original routines translated to C++.
- macOS and Android packages: open to the community (the code builds on macOS; nobody tests it yet).
- Support for other versions of the game (original disks as ADF images).

## Licence

BloodCash is © Lorenzo Castelletta and released under the **GNU General Public License, version 3 or later**
(see `LICENSE`). Moira (in `core/ext/moira`) is under the MIT licence.
