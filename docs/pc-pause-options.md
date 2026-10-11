# PC in-game settings recovery

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Recovered source is `src/Speed/Indep/Src/Frontend/PauseOptions.cpp` and `.h`.
The native demo now compiles it and opens the player's own
`UI_PauseOptionsMain.fng` and `UI_PauseOptions.fng` from `GLOBAL/InGameCommon.bun`.
The initial source and verifier predate this integration pass; this pass leaves
the recovered routines unchanged and supplies their missing native interfaces.

## Recovered boundaries

| Screen / routine | Address |
|---|---|
| Main factory / constructor / setup | `0x4f47f0` / `0x4eb580` / `0x4c3000` |
| Main message handler | `0x4a7d60` |
| Category icon handlers | `0x49a620`, `0x49a640`, `0x49a660`, `0x49a680` |
| List factory / constructor / setup | `0x4eb650` / `0x4d93c0` / `0x4c4bd0` |
| Save-mode confirmation / message handler | `0x4a7f70` / `0x4a8010` |
| Audio / Display / Gameplay / Player setup | `0x4c3370` / `0x4c36c0` / `0x4c38f0` / `0x4c4740` |
| Player slot selection | `0x4a7ae0` |

The four category setups create 28 option classes. Their `React`, `SetLabels`
and slider initialization routines retain their retail vtable names and
addresses in the source. Audio edits the seven floats at `0x83aa28`; Display
edits `0x870d1c` and `0x87099c`; Gameplay edits the camera, mirror, units, map and
save flags; Player edits two `0x28`-byte records at `0x83aa64`. Unknown fields
remain offset-based. Context-dependent omissions, disabled feedback, player
selection, save confirmation, replay calls and external setting calls stay in
the recovered code.

## Behavioral verification

```sh
build/venv/bin/python tools/verify_pc_pause_options.py
build/venv/bin/python tools/verify_pc_pause_menu.py
```

On 2026-10-11 these passed **5,730 comparisons over 89 settings routines and
510 vtable slots**, and **11,723 pause-menu comparisons and 25 vtable slots**.
Fingerprints, per-routine counts and interface addresses are recorded in
`build/analysis/pause-options/verification.json` and
`build/analysis/pause-menu/verification.json`.

The settings verifier builds with MSVC 7.1, binds externals to retail addresses,
and compares original/rebuilt execution in Unicorn with SEH/TIB support. It
checks interface traces, fixture/global memory, returns, stack cleanup,
callee-saved registers and SEH restoration. Cases include all categories,
allocation failure, controller/player layouts, game modes, dialog outcomes,
slider boundaries, resolution availability, and invalid/noncanonical setting
values. Calls are guarded to the declared functions and interfaces.

**Byte matching is not established.** These are bounded behavioral comparisons
with controlled interfaces, not verification of the complete screen, FEng,
settings backends, or their composition in the original game.

## Native integration and provenance

`NativePause.cpp` owns the screen packages, deferred screen changes, focus,
message responses, scripts and dialogs. `NativeFrontend.h` shares those seams
with `NativePauseOptions.cpp`; the latter owns the unrecovered list/widget
bases and session initialization. All are platform scaffolding.

Ghidra reads of the original routines guided this adapter:

- List construction and master geometry: `0x552780`, `0x51ff80`.
- Numbered objects and creation call order: `0x54ee40`, `0x54ef30`, `0x54f410`.
  Names are `OPTION_NAME_N`, `OPTION_DATA_N`, `OPTION_BACKING_N`, arrows and
  the six `SLIDER_*_N` objects. Positions come from `TITLE_MASTER` and
  `DATA_MASTER`, with image top-left/centre conversion (`0x51d070`).
- One-based selection, disabled-item skipping and visible rows: `0x539650`,
  `0x5390e0`, `0x52fea0`, `0x52fdb0`; keyboard handler `0x556020`.
- Widget construction and scripts: `0x51dfc0`, `0x51e1b0`, `0x51e230`,
  `0x51e840`, `0x52db40`, `0x52dff0`, `0x51e550`, `0x51e8d0`.
- Slider clamping/previous value: `0x4ff480`, `0x4ff550`; visual binding,
  padding and fill/knob placement: `0x5275b0`, `0x540d20`.
- The `0x540d20` handle top includes half the knob height; omitting that
  subtraction detached the green fill below the track. Knob top is handle top
  minus the retail `2.0f` constant at `0x7844c0`. Width follows the same fraction
  and padded travel as the knob.
- Native presentation matches the supplied frontend reference by reading the
  `SLIDER_BASE_N` Init tint from retail `UI_Options.fng` in `FRONTEND/FrontB.lzc`.
  The in-game pause package instead initializes its track white. This tint
  transfer and selected-slider-only arrows are explicit native presentation
  seams, not recovered in-game rendering behavior. Missing frontend assets
  retain the pause package's own tint.
- Package pad routing and response commands: `0x54d4b0`, `0x52c7c0`,
  conditional-block skipping `0x504490`.
- Highlight loop: `0x52bdc0` joins the last key to the first for cycle 1.
  `NativeFng.cpp` now handles these type-4/type-6 interpolation-1 tracks with
  recovered arithmetic primitives; other dynamic forms still trap.
- Camera seam follows the read of `0x502540`: ordinary choices 0..3, 0..4
  for drift/special-track contexts. `0x5f2fa0` itself remains unrecovered;
  its profile/camera-object work is replaced by the demo's existing camera
  binding. The return uses recovered `DriveCameraTypeForOption` (`0x502500`).

Native list positions are reapplied after script advancement because the
adapter lacks FEng's separate layout baseline. List scroll animation, the full
scrollbar, mouse input, controller reassignment, menu audio, help-bar rendering,
FEng object ownership and complete script lifecycle remain seams. Responses
for unrecovered screen packages log their absence and resume the demo.

Speedometer units and camera choices feed the native driving demo; explore-map
state shares the existing minimap global. Other settings retain their edited
session values but do not imply recovered gameplay effects: transmission,
stability, mirror, jump camera and HUD mask production still need integration.
Audio has no backend. Resolution/quality changes update the retail labels and
session values without changing the native renderer. Display reset logs its
unrecovered backend and leaves the native renderer unchanged. Customize
Display Settings leads to an unavailable package. Registry/profile persistence,
replay recording and force feedback are absent; feedback is disabled and
Autosave is unavailable. Values reset on a fresh demo launch.

## Native smoke checks

```sh
python tools/smoke_native_pause_options.py
```

Requires the local game installation and an SDL/OpenGL display. The runner
captures retail screens and checks routing/session edits, category return,
dialog acceptance/cancellation, resume, scrolling, slider clamping and camera
interaction with `--no-hud`. It also compares green fill pixels at 0%, 50% and
100% to detect the detached-fill and movement regressions. On 2026-10-11,
17 scenarios and the rendered-fill checks passed.
It writes screenshots and logs under ignored `build/analysis/native-options/`.
These checks are integration evidence for the native adapter, separately from
the original/rebuilt comparisons above. They do not establish D3D rendering
parity or full FEng recovery.
