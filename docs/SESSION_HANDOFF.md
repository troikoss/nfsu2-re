# Session handoff — 2026-10-11 (fourth session: wall collision)

## This session (uncommitted, on top of the uncommitted camera/HUD work)
- Recovered UG2's collision-body world so cars hit walls: `src/Speed/Indep/Src/Physics/Collision/`
  (bodies, track-polygon bodies, GJK witness, contacts, impulse solver), plus `0x5e0d30`/`0x5e7100`/
  `0x5d3450` in `WCollision.cpp` and the world clock `0x57a0e0` (`World/WorldClock.cpp`). Car `+0x3c`
  (formerly the `MoverMode` placeholder) is now `CarCollisionBody *collisionBody3C`.
  Details, routine list and seams: docs/NATIVE_DEMO.md, "Wall collision (fifth pass)".
- Platform: `NativeCollision.cpp` (seams/traps), hull loading and `DestroyNativeVehicle` in
  `NativeVehicle.cpp`, slot pools with capacities in `NativeWorld.cpp`; the step is now
  `AdvanceCollisionBodies` → `UpdateCollisionWorld` → `AdvanceWorldClock`.
- Verification: `build/venv/bin/python tools/verify_pc_collision_simplex.py` (Unicorn, MSVC 7.1).
- Open: car-vs-car and world objects (type 1/2 bodies, `0x5b31f0`, `0x5ae000`, slot 0x8c), cylinders
  (`0x5a23d0`/`0x5a2780`), collision effects/sounds in `0x5a9620` and `0x59fb70`, the real car update
  `0x6012b0`, the original call site of the world clock, Unicorn checks for `0x5b3390`/`0x5af0e0`,
  and the 42 degenerate-tie differences in `0x595040` (x87 spill rounding).
- Test: `UG2_STEER=-0.4 UG2_DEBUG_COLLISION=1 build/native/ug2sim "Need for Speed Underground 2" SKYLINE 900`;
  the demo's `--scripted` mode honours `UG2_STEER` too.

# Session handoff — 2026-10-12 (third session: camera and HUD)

## This session (uncommitted)
- Recovered UG2's drive camera (`src/Speed/Indep/Src/Camera/DriveCamera.*`) and the tach/speed/gear
  HUD gauge (`src/Speed/Indep/Src/Hud/HudGauge.*`); the demo uses both. Details and seams: docs/NATIVE_DEMO.md,
  "Drive camera and HUD". New platform files: NativeFng, NativeHud; vendored `third_party/imstb_truetype.h`.
- Open: camera world-collision pushes and shake, the acceleration producer (car +0x80), minimap and other
  HUD features (mask `0x600090`/`0x4ca090`, per-feature updates in `0x4ec5a0`), shift light, nitrous bar,
  `tach_fill` behaviour, FEng script playback beyond the INIT script.
- Test: `build/native/ug2demo --frames 120 --scripted --camera 3 --screenshot x.ppm`; `UG2_DEBUG_CAMERA=1`,
  `UG2_DEBUG_HUD=1`.

# Previous handoff — 2026-10-11 (second session)

## Goals
- Overall (AGENTS.md): completely decompile UG2 PC v1.2 into verified, reproducibly
  buildable source. The playable demo is only an intermediate milestone.
- Done: a playable native demo with original assets (`62e6cd2`) and correct L4RA scenery
  visibility (this session's commit). Both are local only and **not pushed**; pushing
  needs fresh user approval.
- Active goal: "keep working until the map looks right". From the spawn, orbit and
  scripted-drive screenshots, Bayview now reads like UG2 (road and lane markings,
  sidewalks, skyline, night sky dome, additive spotlight beams).

## What changed (see docs/NATIVE_DEMO.md, "Scenery visibility")
- Section visibility from `0x5e96b0`/`0x5e00d0` and the `0x80034150` data. This was the
  root cause of the smeared ground and the black rectangle: PAN_* panoramas were drawn
  everywhere.
- The view-flag test from `0x5d6570`, with the player view flags from `0x4888a0` (0x18).
- Instance flag 0x100 means identity matrix.
- Scaffold alpha/blend passes from TextureInfo `+0x49`/`+0x4a`.
- Extra texture packs: LOC4DYNTEX (SKYDOME), InGameCommon (light beams), GLOBALB and
  GlobalTexturesNA.
- Debug aids: `UG2_DEBUG_AT`, `UG2_DEBUG_PICK` (stencil pick), `UG2_DEBUG_SECTIONS`.
- The old handoff's issue 4 ("overlapping `_0`/`_R0` variants") was wrong: they are
  complementary terrain chunks.

## Open items
1. **LOD by pixel size:** recover `0x5d64d0`. The demo always uses info hash `+0x20`.
2. **Nearest-boundary fallback:** `0x5db770` is not recovered. With no drivable section,
   the demo uses the "number < 10" fallback, which may show wrong scenery off-road.
3. **View-flag bits 0x02/0x01:** recover eView `+4` and `0x8707dc`; both are left clear.
4. **Blended sorting:** blended parts are not depth-sorted.
5. **Time-of-day:** the clear/fog colour is not recovered.
6. **Remaining missing textures:** 18 TRN_SH parts (`0x9c3ce745`) and 4 XS signs. Run
   `$S/pf.py` over the packs to list them.
7. **Free camera:** a free/aerial camera flag would still help with wider map checks (the drive camera now exists).

## How to view the work
```sh
cmake --build build/native
S=<scratchpad>
build/native/ug2demo --frames 60 --orbit 200 --screenshot $S/o200.ppm
build/native/ug2demo --frames 900 --scripted --screenshot $S/d900.ppm
UG2_DEBUG_PICK="640 400" build/native/ug2demo --frames 60 --orbit 0 --screenshot $S/p.ppm
```
Convert PPM to PNG with PIL (system python).

## Rules reminder
- Keep unknown fields offset-based.
- Mark behavioural verification separately from byte matching.
- Run MSVC/wibo jobs sequentially.
- Retail data and disassembly stay in ignored directories.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
