# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

re3: a fully reversed-engineered source port of GTA III (`master` branch; Vice City lives on the `miami` branch). Code is meant to match the original binary's behavior, so it reads like Rockstar's code, not modern C++. There are no automated tests and no linter; verification means building and running the game, which needs a legal copy of GTA III assets.

## Build

Several build systems coexist. On Linux use CMake or premake:

- **CMake + Conan** (what CI uses; see `README.md` and `.github/workflows/build-cmake-conan.yml`):
  ```
  conan export vendor/librw librw/master@
  mkdir build && cd build
  conan install .. re3/master@ -if build -o re3:audio=openal -o librw:platform=gl3 -o librw:gl3_gfxlib=glfw --build missing -s re3:build_type=RelWithDebInfo -s librw:build_type=RelWithDebInfo
  conan build .. -if build -bf build -pf package
  ```
  Plain CMake also works: `RE3_AUDIO` (`OAL`, or `MSS` on Windows), `RE3_WITH_OPUS`, `RE3_WITH_LIBSNDFILE`, `RE3_VENDORED_LIBRW`, plus librw's own `LIBRW_PLATFORM` / `LIBRW_GL3_GFXLIB` options. Only `glfw` is supported as the GL3 gfx lib.
- **premake** (`premake5.lua`, `premake5Linux`, `premake-vs20xx.cmd`): used for Windows/VS and the Linux/macOS/FreeBSD wiki instructions. Options: `--with-librw`, `--glfwdir64`, `--with-opus`, `--with-lto`, `--with-asan`, `--no-git-hash`. Set `GTA_III_RE_DIR` to have the post-build step copy the executable into the game folder.
- `codewarrior/` and `autoconf/` are alternative/legacy builds; ignore unless asked.

`src/CMakeLists.txt` globs all `*.cpp`/`*.h` recursively and adds every directory under `src/` to the include path, so new files need no build-file edits and headers are included by bare name (`#include "Ped.h"`).

Submodules (`vendor/librw`, opus, ogg, opusfile) must be checked out (`git clone --recursive` or `git submodule update --init`). In the author's checkout `vendor/librw` is populated and the Ninja build in `build/` works. Do not edit `vendor/`.

## Architecture

- `src/core/`: game loop and top-level systems (`main.cpp`, `Game.cpp`, `Frontend*.cpp` menus, `Streaming.cpp`, `World.cpp`, `Pools.cpp`, `Camera.cpp`, `FileLoader.cpp`). `src/core/config.h` is the central feature/bug-fix switchboard (see Conventions).
- Game content, by directory: `entities`, `peds`, `vehicles`, `objects`, `buildings`, `weapons`, `control` (mission scripts `Script*.cpp`, `PathFind`, `CarCtrl`, garages, pickups), `collision`, `modelinfo` (model/TXD definitions), `animation`, `audio`, `save`, `text`, `renderer`, `math`.
- **Rendering layer**: the game was written against RenderWare (RW). re3 replaces the proprietary RW with **librw** (`vendor/librw`). `src/fakerw/` is a shim exposing the RW C API (`rwcore.h`, `rpworld.h`, ...) on top of librw, so game code still calls `Rw*`/`Rp*` functions. `src/rw/` holds the game's own RW helpers (clump/TXD loading, lights, visibility plugins, matfx).
- **Platform skeleton**: `src/skel/` is the cross-platform layer (window, input events, main entry). `skel/glfw` and `skel/win` are the backends; `crossplatform.cpp` supplies POSIX replacements for Win32 calls. Platform-specific code goes here, not in game code.
- **Audio**: `src/audio/` has the game-side audio logic (`AudioManager`, `AudioScriptObject`, sample/cutscene data) over a sample-manager backend selected at build time: `sampman_oal.cpp` + `audio/oal` (OpenAL, default), `sampman_miles.cpp` (Miles Sound System, `vendor/milessdk`, Windows), or `sampman_null.cpp`.
- `src/extras/`: re3-specific additions not in the original game (custom render pipelines per backend: `custompipes*.cpp`).

## Minecraft mode (branch `minecraft-mode`, fork project)

This branch adds a "Steve mode" to re3 (voxel blocks in the GTA III world, F8 toggles it). **Start from `docs/minecraft/HANDOFF.md`**: it has the build/test/run commands, the code map (`src/minecraft/core` for pure code with tests in `tests/minecraft`, `src/minecraft/game` for the re3 side), the known limitations and the prioritised list of remaining work. Specs and plans are in `docs/superpowers/specs` and `docs/superpowers/plans`. The survival core (inventory, mining, crafting) is a Rust staticlib `src/minecraft/rust/mc_bridge` over the external MinecraftOSS crates (CMake `RE3_MINECRAFT_SURVIVAL`, needs `cargo`; see HANDOFF section 9 and `scripts/minecraft/fetch-catalogs.sh`).

Rules that matter when working on it:
- Everything lives in `src/minecraft` behind `#ifdef MINECRAFT_MODE` (CMake option `RE3_MINECRAFT_MODE`, not `config.h`). Rockstar files only get one-line guarded hooks (`Game.cpp`, `main.cpp`).
- Core code uses only the standard library and is unit-tested outside the game: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j && build/mctest/mctests`.
- Game files for manual testing are outside the repo: run `cd "/mnt/1TB/Juegos/GTAs/0. GTA III/0. TEST" && /home/akilex/Descargas/gtas/re3/build/src/re3`. Do not overwrite the user's own `re3` binary there. Downloaded Mojang textures go to `mcassets/` next to the game and are never committed.
- librw's GL3 `setAddressU/V` has an inverted condition (vendor bug, do not edit): set texture filter/address with no raster bound. `rw::readPNG` asserts on a missing file: check existence first.

## Conventions (from `CODING_STYLE.md` and the PR template)

- Match the style of the file you are editing. Tabs for indentation; brace on the same line for control statements, on the next line for function definitions/structs; return type on its own line; no braces around single statements; `int *ptr`, not `int* ptr`.
- Use the project's typedefs (`int16`, `uint8`, `bool`, ...), never Win32 types, `unsigned`, or `__int16`; `char` only for characters.
- Use named enums instead of magic numbers; keep Hungarian-style names (`m_`, `ms_`, `f`, `i`/`n`, `b`, `a`). Make decompiled code readable, not a raw IDA paste.
- **Reversed code must stay faithful to the original binary.** Any behavior change or fix to Rockstar code goes behind a preprocessor flag: bug fixes behind `FIX_BUGS`, other additions behind the relevant `config.h` switch (e.g. `GTA_PS2_STUFF`, `GTA_PC`, `GTA_VERSION`). Only skeleton/cross-platform compatibility code may be unconditional custom code.
- The PR template accepts: features that exist in some GTA, game/UI bug fixes (under `FIX_BUGS`), not-yet-reversed platform/unused code, more accurate reversed code, cross-platform layers, translation fixes, maintainability improvements.
