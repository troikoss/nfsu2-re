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

## Track collision (second pass)

The demo now drives on UG2's L4RA streets using recovered world-collision code in
`src/Speed/Indep/Src/World/WCollision.cpp`:

- Collision manager `0x88f768` (constructor `0x5e81d0`), chunk handler `0x5e78a0` for
  per-section packs `0x80034130` and barrier groups `0x3412f`, pack fixup `0x5d8140`,
  registration `0x5e6980`, section index `0x5d3c10`, lookup `0x591380`.
- Face cache `0x5d8350`, expansion `0x5d7f10`/`0x5d2cc0`, fetch `0x5e0c90`, eviction
  `0x5dcad0`/`0x5dca70`/`0x5d82c0`, edge test `0x5d3330`.
- Per-car collider (car `+0x5c4`): constructor `0x5e0b40`, release/area `0x5dc9c0`/
  `0x5d80a0`/`0x5e5890`, pack switching `0x5e6890`/`0x5e68d0`, and the ground query
  `0x5e72e0` (cached faces, 32 m cells, 8 m sub-cell masks, fixed-point bounds).
- Section topology `0x883e70` (chunk `0x80034150`, loader `0x5e9c10`), tracker
  `0x5dfeb0`, section search `0x5db850`/`0x5db770`, polygon tests `0x5d6dd0`,
  `0x5d7090`, `0x5d7270`, `0x5d70d0`.
- Skid-mark emitter `0x59dbb0` with rotation helper `0x59db50`.

The skid-mark trail methods (`0x5e6e70`, `0x5d3230`) are a renderer seam: the demo
records segments to draw them. The collider area fill `0x5e0d30`, used by barrier
collision, is trapped. Barrier (car-versus-wall) collision `0x5ae930` is not recovered,
so the demo respawns the car at its last grounded position after it leaves the
collision coverage; this respawn is scaffolding.

## Original assets (third pass)

The demo now draws UG2's own car and track data from the player's installation instead of
placeholder boxes and collision polygons.

Recovered and behaviour-verified (`tools/verify_pc_compression.py`): the bWare
decompressors in `src/Speed/Indep/Libs/bWare/Compression.cpp`, namely dispatcher
`0x574670`, JDLZ `0x573700`, HUFF wrapper `0x5744c0` with the EA Huffman decoder
`0x573840`, and RAWW `0x5744f0`. All 2,404 compressed slots of the Skyline, shared car and
Skyline vinyl texture packs decode byte-for-byte like the original running in Unicorn,
including 64 guard bytes after each output. The sources compile with MSVC 7.1; they are not
counted as byte-matching. The COMP decoder `0x574530` is not recovered.

Asset reading and drawing (`NativeAssets.cpp`, `NativeScene.cpp`) is platform scaffolding.
The original loaders (`0x493bb0`, `0x494ea0`, `0x5cc280`) are not recovered. The data
layouts used, checked on the retail files:

- Solid (`0x80134010`): header `0x134011` with the full-name hash at `+0x10` (stored names
  are cut to 27 characters, so parts are matched by hash) and bounds at `+0x20`/`+0x30`;
  texture slots `0x134012`; platform info `0x134900` with the vertex count at `+0x34`;
  vertices `0x134b01` (128-aligned, 36-byte position/normal/colour/UV on cars, 24-byte
  position/colour/UV on 98% of track solids); 60-byte draw records `0x134b02` (index count
  `+0xc`, texture slot `+0x1c`, start `+0x34`, flags `+0x38`); 16-bit indices `0x134b03`.
- Texture pack (`0xb3300000`): `0x33310004` holds 0x7c-byte TextureInfo records after a
  fixed 12-byte pad (name, hash `+0x18`, data offset `+0x24`, palette offset `+0x28`, size
  `+0x2c`, width/height `+0x38`, compression `+0x3e`: 0x08 palette, 0x20 BGRA, 0x22/0x24/
  0x26 DXT1/3/5). Pixel data starts at the `0x33320002` payload aligned to 0x80. Car packs
  instead list 24-byte `0x33310003` slots (hash, file offset, packed size, size, TextureInfo
  distance from the end) of JDLZ/HUFF payloads holding pixels followed by the TextureInfo.
- Scenery section (`0x80034100`): `0x34102` 0x44-byte infos (name, LOD solid hashes at
  `+0x20`/`+0x24`); `0x34103` 0x40-byte instances (bounds, info index in the low half and
  flags in the high half of `+0x18`, position `+0x20`, int16 rotation rows at `+0x2c` with
  0x2000 = 1). All 48,270 L4RA instances resolve to loaded solids.

The stock car is `<CAR>_BASE_A` plus the KIT00 parts of LOD A. The generic texture keys
those parts use are the replacement keys built by CarRenderInfo `0x638190` and the
used-texture setup `0x631e20`, resolved with lights off and no damage: `WINDOW_*`,
`DRIVER` → `DRIVER_MALE` (the in-game branch of `0x638190`), `BRAKELIGHT_LEFT/RIGHT` →
`<CAR>_KIT00_BRAKELIGHT_OFF`, `BRAKELIGHT_CENTRE` → `<CAR>_CENTRE_BRAKELIGHT_OFF`. Scaffold
choices: windows bind GLOBALB's `WINDOW`; the paint/vinyl layers (`<CAR>_SKIN1` and the
unnamed body-panel and skirt keys `0x3c84d757`/`0xa7366ae6`) are drawn in a fixed colour;
lighting is fixed-function OpenGL. The wheel model is drawn at the recovered wheel render
frames (`0x5921b0`), mirrored for right-side wheels.

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
- `DemoMain.cpp`: SDL2/OpenGL window, chase camera, HUD bars; `NativeAssets.cpp`/
  `NativeScene.cpp` read and draw the original car and scenery assets (see above).
  `--placeholder` restores the box car and collision-polygon world.
- Unrecovered routines the demo state should never reach stop the program with a
  message instead of being stubbed (`NativeTrap`).

## Build and run

Requires i386 SDL2/GL runtime libraries and a multilib GCC.

```sh
cmake -S . -B build/native && cmake --build build/native
build/native/ug2sim "Need for Speed Underground 2" SKYLINE 960   # headless check on L4RA
build/native/ug2probe -579.8 533.2                                 # collision coverage probe
build/native/ug2demo --car SKYLINE                                 # W/S/A/D, Space, R, Esc
build/native/ug2demo --frames 60 --orbit 30 --screenshot car.ppm   # scripted capture
```

The headless Skyline run accelerates through automatic shifts near 7,800 rpm and reaches
about 95 km/h when its scripted steering takes it off the road at eight seconds; without
barrier collision it then leaves the collision coverage and falls. These are plausibility
observations, not equivalence evidence.

## Known limitations

- All scenery instances are drawn regardless of their unidentified flags, so some LOD or
  variant pieces overlap; streaming, visibility and the original shaders are not recovered.
- Paint, vinyls, damage textures and headlight/brakelight switching are not applied.
- No barrier collision yet (see above); every collision pack is loaded at start-up
  instead of the original streaming, and the spawn point is chosen heuristically.
- Steering input scaling (±30° road-wheel angle), reverse engagement (hold brake at rest)
  and the controls object are scaffold; the original input filtering is unrecovered.
- Native GCC x87 code is not bit-exact with MSVC; FPU control word is set to 0x027f.
- Free-drive race phase 0 is used; race-phase-dependent branches are untested.
