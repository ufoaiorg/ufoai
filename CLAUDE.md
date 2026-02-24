# CLAUDE.md — UFO:AI Project Context

## What is this?

UFO:AI (UFO: Alien Invasion) is an open-source turn-based strategy game inspired by X-COM, built on a heavily modified Quake 2 engine. Written in C/C++ (now C++17), it targets Linux and Windows.

## Build systems

The project has **two** build systems:

### Configure/Make (primary, traditional)
```bash
./configure
make -j$(nproc)
```
Produces binaries at the repo root: `ufo`, `ufoded`, `ufo2map`, `ufomodel`, `testall`.

### CMake (used by CI and IDE users)
```bash
mkdir -p cmake-build && cd cmake-build
cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja
ninja
```
**Important:** Do NOT use `build/` as the CMake output directory — `build/` contains tracked game data files (`build/base/`, `build/radiant/`). Use `cmake-build/` or similar.

## Key directories

```
src/client/          # Game client (rendering, input, UI, sound, battlescape)
src/client/renderer/ # OpenGL renderer (r_*.cpp)
src/server/          # Dedicated server
src/game/            # Game logic library (tactical battle)
src/common/          # Shared engine code (networking, filesystem, commands)
src/shared/          # Shared types, math, utilities
src/tests/           # Google Test suite
src/tools/           # ufo2map, ufomodel, radiant (map editor)
src/ports/           # Platform-specific main() and system code
base/                # Game data (maps, models, textures, scripts, configs)
build/base/          # Copied game data for out-of-source builds (tracked)
.github/workflows/   # CI/CD pipeline
tools/               # Helper scripts (smoke test, etc.)
docs/                # Planning and issue docs
```

## Architecture notes

- Engine is Quake 2 derived: uses `cvar_t` system, command system (`Cmd_AddCommand`), `Sys_*` platform layer
- Client frame loop is in `src/client/cl_main.cpp` → `CL_Frame()`
- Rendering is OpenGL 2.x/3.x via `r_*.cpp` files
- UI system is custom (`src/client/ui/`)
- Game logic is a separate shared library (`src/game/`)
- Build configuration detected by `./configure` script which generates `config.h` and `config.mk`

## Current modernization effort

See `docs/codex-ready-issue-list.md` for the full roadmap (5 milestones, 19 issues).
See `docs/MODERNIZATION-STATUS.md` for current status and strategy.

### What's been done:
- C++17 upgrade (removed legacy compat code, uses std smart pointers/mutexes)
- GitHub Actions CI/CD (Linux + Windows builds + tests)
- Fixed timestep simulation decoupling (WIP foundation in `CL_Frame()`)

### Coding conventions
- C++17 standard, C11 for C files
- Doxygen-style comments (`@brief`, `@param`, `@sa`)
- Hungarian-ish naming: `CL_` prefix for client, `SV_` for server, `R_` for renderer, `UI_` for UI
- `cvar_t*` for configuration variables, registered via `Cvar_Get()`
- Use `Com_Printf` / `Com_DPrintf` for logging, not `printf`

### Testing
```bash
make -j$(nproc) testall && ./testall
```
Or via CMake: build the `testall` target and run it.

### Common pitfalls
- `build/` is NOT a build output directory — it contains tracked game data
- The configure/make and CMake systems coexist; changes to one may need mirroring
- Many subsystems assume frame-coupled timing (ongoing audit, Milestone 1)
