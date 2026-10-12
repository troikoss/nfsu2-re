# Session handoff — 2026-10-12

## Goal and standing instructions

Audit the native UG2 demo and replace every replaceable gameplay substitute
with original UG2 PC v1.2 code recovered from the supplied executable. Continue
through steering, vehicle/session construction, update timing, reset/camera,
HUD/frontend, and asset/world/presentation logic. Preserve necessary Linux
backends, verify recovered routines and integration, and document unresolved
dependencies and unavoidable platform seams.

The larger repository objective is complete, verified, reproducibly buildable
UG2 source; a playable demo is intermediate. Completion evidence is defined in
`docs/DECOMPILATION_SCOPE.md`. Do not redefine completion around isolated tests.

The user requires verified changes to be committed and pushed between pauses.
Push authorization persists for `main` at
`https://github.com/troikoss/nfsu2-re.git`; do not ask again. Do not commit retail
assets, executables, compiler binaries, or generated disassembly. Keep unverified
work clearly identified rather than describing it as a verified checkpoint.

Follow `AGENTS.md`: always recover original code, never invent replacement game
algorithms. Preserve arithmetic order, call boundaries, addresses and x86
layouts; keep unknown fields offset-based. Report behavioral equivalence
separately from byte matching. Native code uses C++11; recovered source must
also compile with MSVC 7.1. Run MSVC/wibo jobs sequentially. Do not spawn agents
unless the user or applicable repository/skill instructions request delegation.

## Authoritative checkpoint

Workspace: `/home/dq/Documents/Developement/nfsu2decomp/nfsu2-re`, branch `main`.
Latest verified checkpoint: **Recover pause icon placement and cubic tweening**
(see Git history for its commit). Navigation checkpoint: `c0b18f1`; package mask/focus
checkpoint: `3878b16`. Direct user authorization to commit and push to `main` was renewed on 2026-10-12. The preceding code checkpoint is `3e58488` — **Give native
FEng objects the original object and script prefix**.

Recovered and verified since the previous handoff (each with a Unicorn differential verifier and
mutation tests; byte matching not established):

- HUD: mask producer `0x600090` + helpers (`verify_pc_hud_player_masks.py`), gauge update
  `0x4c68d0`, map feature `0x4da640`, feature dispatch `0x4ec5a0`, career/language readers
  (`verify_pc_hud_freeroam.py`). Evidence: `docs/pc-hud-freeroam.md`.
- Start grid `0x5eb5d0` and track position markers (`verify_pc_track_markers.py`); the native
  spawn is now the original grid (smoke 46.6/15.8 km/h from the parking-bay start).
  Evidence: `docs/pc-track-markers.md`.
- Collision sound wrappers, game message post, profile readers, camera option step
  `0x5f2fa0`, game-event record/apply `0x578ef0`/`0x4a7b30`, transmission setter `0x407280`,
  replay camera score `0x578e90`, control mask, return package (`verify_pc_small_queries.py`,
  `verify_pc_replay_events.py`). Evidence: `docs/pc-collision-notifications.md`.
- Native player record and camera-mover seam live in `src/Platform/Native/NativePlayer.h`.

## Current focus: replace native pause-menu code with original code

The user asked to focus on the pause menu. `NativePause.cpp`/`NativePauseOptions.cpp` are mostly
a stand-in for the FEng UI engine plus native versions of original frontend classes. The original
classes write FEng internals directly, so the work is staged:

1. **Done (`3e58488`)**: native FEng objects keep the original FEObject prefix
   (`HudVisibilityObject` in `Hud/HudVisibility.h`: guid `+0xc`, hash `+0x10`, type `+0x18`,
   flags `+0x1c` (0x400000 dirty, 0x4000000 focus), texture `+0x24`, state block `+0x2c`
   (size at `+0x38/+0x3c`), script list `+0x4c`, current script `+0x54`) and the FEScript
   prefix (length `+0xc`, time `+0x10`, hash `+0x30`). The native script player runs on
   `currentScript54->time10`. HUD/pause frames byte-identical.
2. **Done**: native screens retain FEPackage masks `+0x1c/+0x20/+0x24/+0x28`, focus
   `+0x8c`, focusable-object array `+0x90`/count `+0x94` (flag 0x10000000, reader `0x555140`),
   archive node's package `+0x18` and manager's queue pointer `+8`. `FEngPackage.cpp` recovers
   mask setter `0x537950`, selection get/set `0x50b8a0`/`0x50b8c0`, focus setter `0x52b3a0`
   and hash focus `0x52ce40`. `verify_pc_feng_package.py`: 10,000 comparisons; four rejected
   source mutations. Native icon selection uses hash focus; option widget focus uses SetFocus.
   Focus notifications are queued before assignment. **Correction:** selection `+0x10` is
   in the static FNG record (table `0x7f7dc8`, lookup `0x50b840`), not the live package.
   Native package/archive/record lookups and queue dispatch remain seams, including unmodelled
   target -5 delivery. The global manager at `*0x8384c4` and original lookup lists are still
   unrecovered (`0x52cef0`, `0x51bb70`, `0x517f80`, `0x52c570`). Native build, pause verifier
   (11,723), options verifier (5,730) and all 19 native settings scenarios pass.
   Evidence: `docs/pc-pause-menu.md#feng-package-masks-selection-and-focus`.
3. **Navigation/dispatch done**: `IconScrollerNavigation.cpp` recovers `0x51f9f0`/`0x51fac0`,
   `0x5276d0`/`0x527700`, `0x527730`/`0x527750`, `0x51f820` and `0x50d980`. Native
   scroller retains the original prefix through `+0x34`, supplies icon +4 intrusive links,
   and uses recovered navigation/indexing/dispatch. Post-selection layout, construction,
   insertion and the remainder of the scroller still use native seams. The original wrap routine
   loops when every icon is disabled and disabled selection is disallowed; do not add a guard.
   `verify_pc_icon_scroller_navigation.py`: 34,670 comparisons, four rejected mutations;
   byte matching not established. Pause/options verifiers and all 19 native settings scenarios pass.
   **Selection/layout storage done**: `IconScrollerSelection.cpp` recovers `0x52f680`,
   `0x52f7c0`, and list `0x43be60`; FEng focus flag helper `0x51cc60` is stdcall (12-byte
   cleanup). Selection marks all icon objects before the disabled/same-item gates. Direct
   selection stores a 1-based index, indexed selection stores its zero-based input unchanged.
   Native selection now calls recovered SelectIcon. The complete scroller extent is 0xe8:
   ScrollBar +0x38, tween +0x90, alignment +0xc0, index +0xc8, window +0xcc, master +0xd4/+0xd8,
   running offset +0xe0 and fade +0xe4. Native ownership/vector state lives separately.
   The mapped manager pointer at 0x8384c4 is bound during native pause loading.
   `verify_pc_icon_scroller_selection.py`: 12,063 comparisons; four rejected mutations,
   package verifier now 12,000; native build and all 19 settings smoke scenarios pass.
   **Coordinate/state helpers done**: `FEngCoordinates.cpp` recovers `0x51d070`, `0x51d380`,
   `0x51d4d0`, `0x50cb00`, `0x5178d0`, `0x504500`, `0x5046f0`, `0x50bec0`, `0x50bf40`,
   `0x50c020`/`0x50c050` and glyph search `0x6f45b0`. Native image positioning/size and rendered
   text height call recovered code. Native strings retain text +0x64/justify +0x6c/leading +0x70;
   native scripts expose 0x38-byte track baselines. The state writer updates both first matching
   track per script and live state; it is not a deferred-only write. Selector byte +7 comes from
   cumulative FI value sizes or To; FI payload byte 7 is not the selector (loader `0x5515e0`).
   The full native package/script loader is still a seam. Font lookup `0x51bd70` and low-level
   line width `0x50bce0` remain controlled/native boundaries. `verify_pc_feng_coordinates.py`:
   24,024 comparisons under 53-/64-bit x87, five rejected mutations; byte matching unestablished.
   Native build, pause/options/HUD verifiers, 19 settings and 10 Controls smoke scenarios pass.
   Layout recovery must use top-left `0x51d070`/`0x51d380` versus centre `0x51d4d0` correctly.
   Alignment 1 in `0x52f8c0` appears centre-aligned; verify before replacing the old native
   bottom-alignment comment. `0x537a10` is a type-2 string lookup, not the generic native seam.
   **Placement/tween done**: `IconScrollerLayout.cpp` recovers `0x5748c0`/`0x574910`/`0x574e30`,
   `0x50d8d0`, `0x50d9c0`, `0x51fd60`, `0x52f8c0`, `0x538a30` and `0x538940`. Native direct
   layout is removed; reset and frame tick use original placement and cubic motion. Alignment 1
   is centre alignment (old native bottom-alignment comment was wrong). Positioning precedes
   tween advancement; reset selects only after placement. `ScrollerTween` is explicit at +0x90.
   Native frame delta binds `0x865198`; scroller relayout clock is `0x8641a0`. End-slope preserves
   inv-duration multiplication, and acceleration preserves original float spills at 0x574925/942.
   `verify_pc_icon_scroller_layout.py`: 14,762 comparisons, five rejected mutations; native
   19 settings, 10 Controls and regression verifiers pass. Motion is checked by
   `smoke_native_pause_scroller.py` (869 -> 758.5 -> 640 px). Finite scripted UI captures
   ignore external UI events/repeat to prevent desktop pointer interference; explicit script
   pointer/gamepad actions and normal interactive input remain supported.
   Colour `0x495570`/`0x51d7f0`, lock script `0x51df00`, scrollbar `0x538080` and arrow script
   `0x52fa70` are still controlled/native dependencies; constructors leave those optional UI
   bindings unbound. Do not claim full scroller/base recovery until these and construction,
   insertion and the remaining virtuals are recovered.
   **Next**: recover the rest of IconScroller (vtable `0x79c0f4`, base `0x795cbc`, 0xe8 bytes at
   IconScrollerFNGObject `+0x4c`) replacing `NativeIconScroller`:
   ctors `0x543840`/`0x543a90`, ScrollBar `0x542ac0` (at scroller `+0x38`), slots `0x543bf0`,
   `0x543c10`, `0x538b80`, `0x51f870`; helpers `0x51fb70`, `0x538080`, `0x52fa70`,
   `0x538130`, `0x52e4e0`, `0x52e670`, `0x52e200`, `0x52e320`, `0x51fd10`, `0x51fd60`,
   colour `0x495570`/`0x51d7f0`, lock helper `0x51df00`, list helper `0x43be80`.
   Layout notes: `+4/+8` icon list sentinel (icon node at icon `+4`), `+0xc` current icon,
   `+0x10/+0x14/+0x18` OPTION/LOCK/BUTTON_MASTER objects, `+0x1c` package, `+0x20` "option_"
   prefix, `+0x24` spacing (40), `+0x28` next option number, `+0x2c` vertical, `+0x2e` relayout,
   `+0x30` allow disabled, `+0x34` last direction, `+0x38` ScrollBar (0x58), `+0x90` tween
   (0x30), `+0xc0` alignment, `+0xc8` selected index, `+0xcc` window width (480),
   `+0xd4/+0xd8` master position, `+0xe0` running offset, `+0xe4` fade flag.
   Icon (0x48, ctor `0x51f670`): `+0xc` texture, `+0x10` object, `+0x14` offset, `+0x1c`
   colour, `+0x20` disabled, `+0x24/+0x28` size, `+0x2c/+0x30` title/description,
   `+0x34..+0x44` fade state, `+0x45` reacts.
4. Recover IconScrollerFNGObject: ctors `0x551de0`/`0x552330`, dtor `0x4defb0`, message handler
   `0x543d40` (sounds via table `0x837fe0` -> `0x458520`), `0x50da50`, `0x50dab0`,
   `0x52fbf0`/`0x52fba0` (stop/start input on the package masks), `0x50da20`, `0x538c00`/
   `0x538ca0` (dim scripts on object `0x47ff4e7c`).
5. Then the options scroller and option items in `NativePauseOptions.cpp`, the in-game dialog
   `0x558790`, and game-flow queue `0x5f41e0`/`0x5f4240`.

Decompilations of the scroller set were saved under the session scratchpad (`scroller/`,
`feng/`); regenerate with ghidra-rpc if missing. Recreate missing Ghidra functions with
`create-function` first (several of these addresses were undefined).

## Other open work

- Native respawn (DemoMain airborne reset) is the remaining invented gameplay logic. The
  original reset `0x6009a0` (via fall check `0x5ecba0`) needs road navigation: `WRoadNav`
  (ctor `0x5da2f0` recovered; advance `0x5da370` -> `0x5d5c00`/`0x5d9e30`/`0x5da240`, spline
  helpers `0x570640`/`0x577710`) over the road network in `TRACKS/L4RA.BUN`: chunk `0x34107`
  (loader `0x5df8a0`, also `0x80034100` sub-chunks, table at `0x883df0`/`0x883df4`), `0x34108`
  (`0x5db2e0`, list `0x88f760`), `0x34110..0x34113` (`0x5def30` -> grid `0x88eb00` `0x5da7a0`).
  Loaders are registered by the static initializers at `0x77e7d0..0x77e8d0`.
- HUD caller `0x600590` (blocked on rear-view mirror view `0x5f1e10`), participant setup
  `0x6065b0`, car constructor `0x604260`, world update `0x609ce0`, asset chunk loaders.
- Earlier physics items (tuning-table loader, CarTypeInfo binding) remain as listed in
  `docs/demo-substitute-audit.md`.

## Evidence, tools and useful commands

Start with `docs/README.md`, `docs/demo-substitute-audit.md`,
`docs/pc-physics-parameters.md`, `docs/pc-human-steering.md`, and
`docs/NATIVE_DEMO.md`. Manifests live in `config/SPEED2_EXE_1_2/`.

Reference: local `Need for Speed Underground 2/speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Retail source fixtures come from local `GLOBAL/GLOBALB.BUN`, not committed data.

Ghidra skill: `/home/dq/.codex/skills/ghidra-rpc` (`uv run ghidra-rpc ...` from that directory).
Project: `build/ghidra/ug2.gpr`, program `speed2.exe`. Check current daemon
status before relying on prior state. Local daemon access may need escalation.
Ignored `build/analysis/demo-recovery/` contains decompilation JSON and disassembly
for the draft tuning helpers; `build/analysis/function-index.json` has boundaries
and fingerprints. Ghidra's x87 types/signatures can be misleading: disassembly
and verified execution determine arithmetic and calling convention.

```sh
cmake --build build/native -j4
build/venv/bin/python tools/verify_pc_physics_extra_tuning.py
build/venv/bin/python tools/verify_pc_physics_construction.py
build/venv/bin/python tools/verify_pc_physics_parameters.py
build/venv/bin/python tools/verify_pc_hud_player_masks.py
build/venv/bin/python tools/verify_pc_hud_freeroam.py
build/venv/bin/python tools/verify_pc_icon_scroller_layout.py
build/venv/bin/python tools/verify_pc_feng_coordinates.py
build/venv/bin/python tools/verify_pc_icon_scroller_selection.py
build/venv/bin/python tools/verify_pc_icon_scroller_navigation.py
build/venv/bin/python tools/verify_pc_feng_package.py
build/venv/bin/python tools/verify_pc_pause_menu.py
build/venv/bin/python tools/verify_pc_pause_options.py
build/venv/bin/python tools/verify_pc_track_markers.py
build/venv/bin/python tools/verify_pc_small_queries.py
build/venv/bin/python tools/verify_pc_replay_events.py
python tools/smoke_native_pause_options.py
python tools/smoke_native_controls.py
python tools/smoke_native_pause_scroller.py
build/venv/bin/python tools/verify_pc_physics_record_upgrades.py --native-stock build/native/ug2recordprobe
python tools/smoke_native_driving.py
```

Run the verifier commands sequentially because MSVC/wibo shares temporary
filenames. Local tools are `build/tools/wibo` and
`build/compilers/Win32/7.1/cl.exe`. Native32 execution requires escalation in this
sandbox; a sandbox exit 159 does not establish a native program failure.
Git mutations/push also require escalation because `.git` is sandbox read-only.
No analysis jobs are left running at this checkpoint.
