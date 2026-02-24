# Modernization Status & Strategy

> Last updated: 2026-02-24

This document tracks the state of the UFO:AI modernization effort and outlines the recommended strategy for continuing.

## Current state

### Completed

| Item | Commit | Notes |
|------|--------|-------|
| C++17 upgrade | `d3c9013` | Removed `cxx.h` compat shim, replaced custom smart pointers with `std::shared_ptr`/`std::unique_ptr`, replaced `ScopedMutex` with `std::lock_guard`, updated CMake standards |
| BUILDING.md | (master) | Build instructions with dependency lists for Linux/Windows |
| GitHub Actions CI/CD | `f737058` | Linux + Windows matrix builds (Debug/Release), test job, smoke test script |
| Fixed timestep foundation | `f85bd91` | `CL_Frame()` decoupled into `CL_RunSimulation()` + `CL_RunRendering()`, accumulator-based fixed timestep, `cl_simRate`/`r_fps` cvars added |

### In progress / WIP

| Item | Status | What remains |
|------|--------|-------------|
| Fixed timestep (Milestone 1) | Foundation only | Frame limiter not yet implemented (r_fps cvar registered but unused). Variable FPS integration incomplete. No time-step audit of subsystems yet. |

### Not started

| Milestone | Items | Priority |
|-----------|-------|----------|
| M1 Phase 2 | Frame limiter, frame pacing, VSync/VRR | High — completes 120fps readiness |
| M1 Phase 3 | Time-step audit (movement, animation, particles) | High — ensures correctness |
| M2 | Gamepad navigation, UI scale presets, controller remap, Steam Input docs | Medium |
| M3 | HUD safe zones, ultrawide test matrix | Medium |
| M4 | Post-processing cleanup, tone mapping, shadow tiers | Low |
| M5 | Renderer abstraction, Vulkan backend | Low |

## Strategy

### Principles

1. **Fix hygiene first** — don't build features on a dirty foundation. Build artifacts, gitignore, documentation come before engine changes.
2. **Small, focused commits** — each commit should be independently reviewable and revertable. Avoid mixing cleanup with feature work.
3. **Verify before extending** — ensure CI passes and builds are clean before adding more engine changes.
4. **Milestone order matters** — M0 (guardrails) must be solid, M1 (120fps) must be complete before M2+.
5. **Don't break what works** — the game must remain functional at every step. No speculative refactors.

### Recommended order of work

#### Phase 1: Housekeeping (immediate)
- [x] Remove accidentally tracked CMake build artifacts
- [x] Update `.gitignore` for CMake outputs
- [x] Create `CLAUDE.md` (project context)
- [x] Create this status document

#### Phase 2: Complete Milestone 1 (120fps readiness)
- [ ] Implement `r_fps` frame limiter (cap rendering rate without busy-wait)
- [ ] Implement frame pacing (smooth frame delivery, reduce jitter)
- [ ] Audit `src/client/battlescape/` for frame-dependent timing
  - Movement speed tied to framerate?
  - Animation playback tied to delta?
  - Particle systems using fixed per-frame steps?
- [ ] Audit `src/client/renderer/` for frame-coupled assumptions
- [ ] Test at 30/60/120/240 FPS and verify gameplay speed is constant

#### Phase 3: Milestone 2 (Steam Deck / controller)
- [ ] Gamepad focus navigation system for UI menus
- [ ] UI scale presets + DPI-aware scaling
- [ ] Controller remap + deadzone settings
- [ ] Steam Input compatibility documentation

#### Phase 4: Milestones 3-5 (ultrawide, lighting, Vulkan)
- These are larger architectural changes and should be tackled once M1-M2 are stable.
- M5 (Vulkan) in particular requires a renderer abstraction layer first.

### Risks and considerations

| Risk | Mitigation |
|------|-----------|
| Fixed timestep breaks existing game logic | Thorough time-step audit in Phase 2; test at multiple FPS targets |
| CI doesn't catch runtime regressions | Smoke test exists but is limited; consider expanding to basic map load test |
| `build/` directory confusion | Documented in CLAUDE.md; CMake users should use `cmake-build/` instead |
| Dual build system drift | Changes to one build system may need mirroring in the other; CI uses CMake |

## Reference

- Full roadmap: [`docs/codex-ready-issue-list.md`](codex-ready-issue-list.md)
- Build instructions: [`BUILDING.md`](../BUILDING.md)
- Project context: [`CLAUDE.md`](../CLAUDE.md)
