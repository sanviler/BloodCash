# Changelog

## 1.0.0

- Options panel: the same fixed size in every port (256 x 200 pixels of its own, at an integer scale of its own),
  drawn over the picture and never through the CRT filter. Long values and footers (for example the name of a
  controller) are split on more lines instead of widening the panel; only if the text needs more rows the panel
  grows downwards.
- Faster picture filters: the worker threads are made once instead of every frame, the glow is computed in
  parallel, and the CRT filter of FILL computes the beams on the source columns (about half the time before,
  same look); the beam table of FILL is kept per picture.

## 0.6.0

- Picture modes: PIXEL PERFECT (largest integer scale, CRT-Pixie) and FILL (full height at any scale: sharp
  bilinear pixels or a CRT filter for non-integer scales, with the same light on every scanline). Both start with
  the CRT filter on (F5 switches it). Settings `picture` and `crt` in `bloodcash.ini`.
- Windows with display scaling (125%, 150%): the picture uses the real pixels of the screen (before, Windows
  stretched a smaller window: smaller and blurred picture).
- The picture is the display window of each screen: the game (title, shop, levels) is 320×200 and now fills the
  height and is centred (before, a 320×256 frame with 56 black lines below, so the game sat high on the screen).
- The intro can be skipped with Space, Return, Right Ctrl or A on a controller.

## 0.5.0

- Picture always at an integer scale (common video rule of the Framework core): the window opens at half the
  desktop size and can be resized, fullscreen uses the desktop resolution, black around the picture; the
  border option is gone.
- CRT filter (CRT-Pixie), from the options menu or with F5, computed at the output scale.
- Window title: name, version and frames per second.
- Two players: on the first start player 1 gets the first game controller, player 2 the second one (or the
  keyboard joystick when only one controller is connected).
- Core updated to the current Framework core; CI and CMake presets aligned with the reference configuration
  (Windows MSVC + Ninja + vcpkg with the `windows-ci` preset, Linux and macOS with system SDL2).

## 0.1.0

First playable version.

- Blood Money runs with its original 68000 code from `Disk.1` and `Disk.2` of the WHDLoad installation,
  recognised by SHA-256.
- Track loaders replaced by direct reads from the disk images; drive motor routines, disk change request and
  disk copy protection checks bypassed; PAL always detected.
- The intro waits as long as the disk loading used to take, so pictures and music stay in step; it can be
  skipped (options menu, `--custom1 1`).
- High scores saved in `BloodCash.high`.
- Fix: the music code stored a byte into a word and left its upper half undefined.
- Integer scaling, window or fullscreen, optional border, screenshots (F12).
- Options menu (Escape): screen, picture, control ports 1 and 2, intro, restart, quit; settings in
  `bloodcash.ini`.
- Game texts replaceable by a UTF-8 file (`--export-texts`, `bloodcash_texts.txt`), with checks on length,
  characters and score fields.
- Cheats for testing (`--custom1`): lives, energy, money, most money.
