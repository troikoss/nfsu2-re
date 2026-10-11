# Native vehicle demo — 2026-10-11

A 32-bit native Linux build now drives a UG2 car using the recovered vehicle
sources end to end. This is an intermediate milestone toward the playable demo,
not completion of the decompilation. Recovered routines in this pass are
reconstructed from disassembly but are **not yet behaviour-verified** against the
original by differential execution, unless listed in an earlier status document.

## What runs as recovered UG2 code

The full PhysicsMover update chain from `0x5b72d0`:

- Construction: mover `0x5b5cc0`, body `0x5a7f30`/`0x59b940`/`0x43c7c0`, wheel
  `0x5a8160`, suspension arm `0x5a86c0`, engine `0x5aa5a0`/`0x457b00`, drivetrain
  `0x5af9d0` with shift points `0x5ac7d0`, inline Ackermann steering and aero objects,
  initial placement `0x5a48a0`.
- Pre-steps: load refresh `0x5ab990` (centre `0x5a60f0`, mount `0x59e0c0`, centre move
  `0x59b8b0`), mode refresh `0x5a49a0` (response scale `0x599170`, slip `0x591d90`),
  control transfer `0x5abbf0` with gear selection `0x5aa960`/`0x5a1700`/`0x594f20`/
  `0x594670`, brake/grip distribution `0x5a4d80` with traction factor `0x5990c0`, launch
  assist `0x5b6300`.
- Substeps: integrator `0x5aa480` (earlier verified), orthonormalisation `0x5bb0d0`.
- Post-integration `0x5b04d0`/`0x5a0d80`: block post-updates (wheel `0x59d030`, arm
  `0x5a8790` with ground query `0x59d250`, surface noise `0x5d3ca0`), rest handling
  `0x5abb00`/`0x5a5e20`/`0x5992b0`, bump stop `0x59e1a0`, limited impulse `0x59bc60`.
- Controller forces `0x5b0520`: aero `0x59e620`, wheel contact `0x59d160`, the earlier
  verified coupling/drivetrain/engine chain, yaw assist `0x5ac260`, wheel lift
  `0x5a51b0`, remote correction `0x5a5c20`, guide force `0x5ecf70`.
- State export `0x5a5540` (origin `0x592400`, wheel render frames `0x5921b0` with
  rotation helpers `0x5bac90`/`0x5bada0`/`0x5bae60`), state reset `0x59f880`, nitrous
  timers `0x591cd0`/`0x592480`/`0x5925b0`/`0x592500`/`0x59fa30`.
- Car-object queries: `0x5ed290`, `0x5ed470`, `0x5ebfa0`, `0x5ed220`, `0x5ed1b0`,
  `0x5ebf60`, `0x5ee250`, `0x5ec790`, `0x5f67f0`, `0x592870`, draft/ambient
  `0x5ed5d0`/`0x61e380`/`0x6134e0`/`0x5f0b40`, helpers `0x401020`, `0x43c370`,
  `0x43c390`, `0x43cf90`, `0x440f70`, `0x5eb8f0`/`0x5eb900`/`0x5eb910`.
- Static initializers that build vehicle curves (`0x77de60`..`0x77ed80`), found with
  `tools/list_pc_static_initializers.py`.

All 50 physics sources compile with MSVC 7.1 (`/Ox /Ob0`), and every partial layout
carries compile-time offset checks (`Ug2Layout.h`). No routine in this list is counted
as byte-matching.

## Platform scaffolding (not original code)

`src/Platform/Native` contains everything that is not recovered UG2 code:

- `OriginalImage.cpp` maps the player's own `speed2.exe` `.rdata`/`.data` at their
  original addresses after a SHA-256 check; `OriginalSymbols.ld` binds recovered
  `extern` globals to those addresses. No original instruction executes.
- `NativeVehicle.cpp`: vtable shims forwarding each original slot to the recovered
  method, the `New...` allocation factories, the D3DX matrix multiply, a flat road plane
  standing in for world collision `0x5e72e0`, scaffold race-status/context/random
  objects, keyboard controls, and stock car records copied from GLOBALB.BUN CarTypeInfo
  `+0x110` (upgrade composition `0x5ac4f0`/`0x5997e0` not yet recovered).
- `DemoMain.cpp`: SDL2/OpenGL window, chase camera and placeholder box/wheel drawing.
- Unrecovered routines the demo state should never reach stop the program with a
  message instead of being stubbed (`NativeTrap`).

## Build and run

Requires i386 SDL2/GL runtime libraries and a multilib GCC.

```sh
cmake -S . -B build/native && cmake --build build/native
build/native/ug2sim "Need for Speed Underground 2" SKYLINE 960   # headless check
build/native/ug2demo --car SKYLINE                                 # W/S/A/D, Space, R, Esc
```

The headless Skyline run reaches about 127 km/h in eleven seconds with automatic
shifts near 7,400–7,800 rpm, steers, coasts and brakes to rest, with finite state
throughout. These are plausibility observations, not equivalence evidence.

## Known limitations

- Flat ground replaces UG2 world collision; no track or car model is loaded yet.
- Steering input scaling (±30° road-wheel angle) and the controls object are scaffold;
  the original input filtering is unrecovered.
- Native GCC x87 code is not bit-exact with MSVC; FPU control word is set to 0x027f.
- Free-drive race phase 0 is used; race-phase-dependent branches are untested.
