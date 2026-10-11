# Native vehicle demo

A 32-bit native Linux build now drives a UG2 car using the recovered vehicle
sources end to end. This playable demo is an intermediate milestone,
not completion of the decompilation. Recovered routines in this pass are
reconstructed from disassembly but are **not yet behaviour-verified** against the
original by differential execution, except for the isolated comparisons in
[vehicle recovery](pc-vehicle-physics.md), [wheel/body verification](pc-wheel-physics.md),
and the compression evidence below. Recovery-pass labels describe implementation
history, not independent verification levels.

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
records segments to draw them. Wall collision is recovered in the fifth pass below; the
demo still respawns a car that leaves the collision coverage (scaffolding).

## Wall collision (fifth pass)

Cars now collide with the track's walls through UG2's collision-body world, recovered in
`src/Speed/Indep/Src/Physics/Collision/` (class names from the original slot pools and
serialisation strings). Walls are not special-cased: every frame each moving body turns
the track faces near it into `TrackPolygonCollisionBody` objects, pairs overlapping bodies,
measures pairs with GJK and resolves touching pairs with an impulse.

- World step `0x5ffa50` (after `0x5f0f00` advances every dynamic body), stale pairs
  `0x592e60`, pair search `0x5b23b0` over the body list kept sorted by maximum X
  (`0x5a0680`, largest width `0x5a05a0`), contact list sort `0x43bee0`/`0x5a0eb0`/`0x5a0dc0`,
  contact removal `0x5af090`, orphan polygons `0x5a0cd0`, pools `0x5a0200`/`0x593db0`/`0x592b00`.
- Bodies: `CollisionBody` `0x5a04f0`/`0x5a9c00`, `DynamicCollisionBody` `0x5a9ce0` (pose
  snapshot `0x593210`, interpolated pose `0x593280`, rewind `0x593630`/`0x593590`, mover
  attach `0x5938c0`, shape extents `0x5ae520`, bounds `0x5a9de0`, polygon reach `0x5937b0`,
  polygon spawning `0x5ae930`), `CarCollisionBody` `0x5aec20` with slots `0x593b40`..`0x5a0be0`
  (impulse `0x5a0b00` → `MoverBody` `0x59bd80`, inverse mass/inertia `0x593a00`/`0x593a40`,
  restitution/friction `0x5a0c90`/`0x5a0be0`, shape choice `0x5a0a80`), `TrackPolygonCollisionBody`
  `0x5aa0b0`/`0x5aefa0` with its fixed-body slots and polygon shape `0x5a1a90`.
- Track faces near a body: collider area update `0x5e7100`, area fill `0x5e0d30`, planar
  face distance `0x5d3450`; car hulls from `CARS/<car>/GEOMETRY.BIN` chunk `0x80034020`
  (`0x5a1750`, lookup `0x5a18c0`, key = model name + `_CV`), falling back to the box `0x598760`.
- Shapes: vertices `0x5aad20`/`0x5a1970`/`0x5a2010`/`0x5987d0`, support points (hull hill
  climbing `0x5afba0`, polygon `0x5a2060`, box `0x5a2280`, point `0x5a2360`).
- Pair witness `0x5a0340`/`0x5b22b0`, shapes `0x598590`, poses `0x598620`, distance update
  `0x5b3d20`, GJK `0x5b3390` with the feature-pair sub-algorithm `0x595040` and simplex copies
  `0x598130`/`0x598240`, pair test `0x5b5050`, thick-polygon flags `0x592f00`/`0x593010`.
- Contacts: impact time bisection `0x5b4be0`, separation `0x5b42f0`, polygon normal side
  `0x593df0`, contact frame `0x594000`, resolve `0x5b2610` (rewind, impulse, re-advance),
  impulse with restitution and friction `0x5af0e0`/`0x5941c0`.
- Contact events (the mover's "contact bodies"): `0x5a9b10`/`0x5a9430`/`0x5a9570`/`0x5ae470`,
  lookup `0x5a0820`, wall/floor tests `0x5a0160`/`0x5a01b0`, accumulation part of `0x5a9620`.
- World clock `0x57a0e0` (60 Hz frames, 4 kHz ticks at `0x86518c`), which ages the events.

Verification: `tools/verify_pc_collision_simplex.py` runs the original and the MSVC 7.1
rebuild of `0x595040`/`0x598130`/`0x598240` in Unicorn on 6,400 synthetic simplices. All
non-degenerate cases agree within float rounding (about two thirds bit-identical); 42
cases with duplicated or collinear vertices pick a different vertex at an exact tie
because of x87 spill rounding. Nothing here is byte-matching; the rest is behaviour
observed in the demo only. In the headless run with `UG2_STEER=-0.4` the Skyline hits
walls at 71 and 90 km/h, loses roughly half its speed, and its centre stays 1.2 to 2.3 m
in front of the wall plane.

Scaffolding and seams (`src/Platform/Native/NativeCollision.cpp`): the car update `0x6012b0`
is reduced to the mover step; collision effects after the impulse sums (sparks, sounds,
replay, force feedback), audio registration `0x483e20`/`0x483e40` and the game message
`0x5e0360` are no-ops; the hull key `0x61c320` is computed by the loader for stock parts;
slot pools are heap blocks with the original capacities; the world clock runs after the
step (its original call site is not recovered). World objects (type 1/2), cylinders,
replay serialisation and mover point velocity (`0x59bec0`) are trapped. `UG2_DEBUG_COLLISION=1`
prints every impulse with the wall normal and the car's distance to the wall plane.

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
  0x2000 = 1). All 48,270 L4RA instances resolve to loaded solids. The section number
  is at `0x34101` `+0xc` (letter `n / 100 + 0x40`, number `n % 100`).

### Scenery visibility (fourth pass)

Read from the original and applied by the demo (behaviour, not byte matching):

- Visible sections, `0x5e96b0`: the drivable section whose boundary holds the eye
  (`0x5e00d0`: bounds test `0x43d410`, even-odd crossing test `0x5d6dd0`) supplies a list
  of section numbers; the Z sections 2600-2699 (SKYDOME) are always added. With no
  drivable section the loaded sections numbered below 10 are used. The data is
  `0x80034150` in `L4RA.BUN`, loaded by `0x5e9c10`: `0x34151` header, `0x34152` boundaries
  (section `+0x8`, point count `+0xa`, bounds `+0xc`/`+0x14`, x/y points from `+0x24`),
  `0x34153` 0xa4-byte drivable records (section `+0x8`, count `+0x10`, sections from
  `+0x14`). The nearest-boundary fallback `0x5db770` is not recovered.
  Drawing every section put the low-detail panoramas (`PAN_*`, sections X9x) over the
  detailed scenery: that was the smeared ground and the black rectangle.
- View flags, view setup `0x4888a0`: `view +0x44` is 0x10 always, 0x08/0x04 by
  `0x89e7a4` (0 for the forward direction), 0x02 for eView `+4` types 1/2, 0x01 when
  `0x8707dc` is 3 or 4, 0x40 for 0x1800 (mirror) views, 0x20 for type 3. The per-instance
  test in `0x5d6570` skips an instance when `(instance +0x1a ^ 0x60) & (byte)view +0x44`;
  the demo uses 0x18 for the player view. Instances flagged 0x10 (race barriers, shop
  beams, start/finish graphics) are therefore hidden in every view.
- Instance flag 0x100 queues the model with no matrix (its vertices are in world space).
- Scenery is reached through the `0x34104` 0x24-byte tree nodes (bounds, child count
  `+0x18`, up to five children; negative entries are nodes), traversed by `0x5d69b0`.
  Every L4RA instance is reachable, so the demo walks the instance array.

Scaffold render states (the D3D renderer is not recovered): TextureInfo `+0x49` reads as
alpha usage (0 none, 1 punch-through, 2 modulated) and `+0x4a` as blend type (0 none,
1 blended, 2 additive), judged from how they are spread across L4RA's textures. Blend
type 0 draws opaque and alpha-tests only punch-through textures, so the alpha of road and
terrain textures (modulated, used as a mask) is ignored. Types 1 and 2 are drawn after
the opaque pass without depth writes. Extra texture packs: `TRACKS/LOC4DYNTEX.BIN`
(SKYDOME), `GLOBAL/InGameCommon.bun` (SFX_LIGHT_BEAMA), `GLOBAL/GLOBALB.BUN` and
`GLOBAL/GlobalTexturesNA.bin`. The sky dome is drawn scaled about the eye because it is
wider than the demo's far plane, and the clear colour is a fixed dusk tone.

Debug aids: `UG2_DEBUG_AT="x y"` lists the instances whose bounds hold a point,
`UG2_DEBUG_PICK="x y"` names the scenery instance drawn at a screenshot pixel (top-left
origin, final frame; it redraws into the stencil buffer), and `UG2_DEBUG_SECTIONS=1`
prints the drivable section and its visible list whenever it changes.

The stock car is `<CAR>_BASE_A` plus the KIT00 parts of LOD A. The generic texture keys
those parts use are the replacement keys built by CarRenderInfo `0x638190` and the
used-texture setup `0x631e20`, resolved with lights off and no damage: `WINDOW_*`,
`DRIVER` → `DRIVER_MALE` (the in-game branch of `0x638190`), `BRAKELIGHT_LEFT/RIGHT` →
`<CAR>_KIT00_BRAKELIGHT_OFF`, `BRAKELIGHT_CENTRE` → `<CAR>_CENTRE_BRAKELIGHT_OFF`. Scaffold
choices: windows bind GLOBALB's `WINDOW`; the paint/vinyl layers (`<CAR>_SKIN1` and the
unnamed body-panel and skirt keys `0x3c84d757`/`0xa7366ae6`) are drawn in a fixed colour;
lighting is fixed-function OpenGL. The wheel model is drawn at the recovered wheel render
frames (`0x5921b0`), mirrored for right-side wheels.

## Drive camera and HUD (fifth pass)

Recovered and applied by the demo (behaviour, not byte matching; `DriveCamera.cpp` and `HudGauge.cpp`
compile with MSVC 7.1 `/Ox /Ob0`):

- **Drive camera**, the mover Player::SetCameraState (`0x5f1fd0`) builds for state 1: constructor
  `0x451660`, update `0x453570`, pose `0x44de90`, setting read `0x445eb0`, car frame `0x445e00`.
  Eye, look-at and up are cubic Hermite followers (`0x5749e0`/`0x574df0`, restarted on every target
  change, duration from the dynamics key) in car-local axes. Targets come from the per-car camera
  table at CarTypeInfo `+0x720` (GLOBALB.BUN: six 16-byte `{type, angle, x, height, lateral}`
  entries, types 0 far, 1 close, 2 bumper, 3 driver, 4 hood, 5 super far; option to type map
  `0x502500`) plus the 4-frame average of the car acceleration (`car +0x80`) scaled and clamped by the
  static dynamics curve table `0x7ec610` sampled at the constant `0x787154`. Field of view per type
  is the table `0x7eb5b4` (71.5 degrees chase, 90 cockpit); near plane 0.5 (0.4 driver); projection
  from `0x5c7ec0` (half-angle tangents from the field of view and the 1.1 / 0.8696 aspect constants).
  Bumper and driver types hide the car (Camera `+0x280`). Keys: `C` cycles far/close/hood/bumper,
  `V` looks back (hold); `--camera N` picks the profile option 0..4 (default 3, outside far).
- **HUD**: `HUD_SingleRace.fng` from `GLOBAL/InGameFreeRoam.bun`. FNG layout (package `FEn\xe7`:
  `PkHd`, `TypS`, `Res\xcc` with `RsNm`/`RsRq` (50 six-dword requests, name hash, name offset,
  kind), `Obj\xcc` with `FOb\xea` blocks of `ObjD` 16-bit tag/size records and `Scrp` scripts) is read
  from the loaders `0x5554a0`/`0x555140`/`0x5515e0`/`0x518380`. Object state block: colour (B, G, R, A
  dwords), pivot, position, rotation quaternion, size and for images UVs. Position `OP +0xc` low half is
  the resource index, `PA` the parent GUID, flag bit 0 hides (`0x50ca00`/`0x50ca50`). Coordinates are
  640x480 with the origin at the screen centre; lower z draws in front. Object hashes are the case
  folded `h = h * 33 + c` (`0x505450`) of names found in the executable (`3rdPersonNeedle`,
  `3rdPersonSpeed`, `3rdPersonGear`, ...). The gauge feature is the recovered `0x4f5100`/`0x4c68d0`:
  dial type `0x60fe10` (7500..10500 scale), needle sweep `(maximum rpm / scale) * 235` degrees, needle
  angle `0x4aa0e0`, speed `|v| * 2.237` mph or `* 3.6` km/h, gear characters from `0x7f66a4`, gear
  colour while shifting, skin textures `TACH_FILL_CUSTOM_nn`, `3RDPERSON_*LINES_CUSTOM_nn` and
  `3RDPERSON_RPMDIAL_CUSTOM_nn` from `HUD_CustomTextures_ALL.bin` (`--hud-skin N`, 0 default), unit
  text `MPH`/`KM/H` by hash from `LANGUAGES/English.bin` (`--metric`).

Scaffolding in this pass: the FEng object model and renderer (`NativeFng.cpp`), UI text (the original
uses GDI `CreateFontA` fonts; a system italic sans face is rasterised with the vendored
`imstb_truetype.h`, public domain/MIT, override with `--hud-font`), the acceleration vector at car
`+0x80` (its original producer is not recovered; it is the change of the exported velocity per second,
inferred from its camera use and from `0x5ec3a0` dotting it with the forward axis), the free-drive
feature selection (native chase/map/equipment context supplies masks to recovered
`0x4ca090`; producer `0x600090`, laps and `tach_fill` colouring remain seams).
The shift-light tail, turbo and NOS updates are now recovered. Region/cash/reward
widgets accept external career state; the demo has no recovered career producer
and keeps them inactive. GPS, heat and notification scheduling have bounded source
verification but are not live native features. Message-script playback supports
retained noncycling vector/colour tracks with recovered arithmetic. Complete NOS
stencil rendering and full FEng scripts remain unresolved; see
[freeroam HUD evidence](pc-hud-freeroam.md).
The minimap uses recovered `0x4ca840`, TrackInfo record 4000, `TRACKMAP4000.BIN`,
and the retail map/mask/north/player objects. Its border group follows recovered
mask application `0x4c6690` and recursive FEng show/hide `0x50ca50`/`0x50ca00`;
native free drive supplies mask bit 0. Its native pool setup and OpenGL
RGB/stencil mask rendering remain platform seams; see [minimap evidence](pc-hud-minimap.md).

Not recovered, left as seams: world collision pushes for the chase types (`0x444530`, `0x44b040`),
camera shake (`0x452ed0`, `0x572a50`), Camera object finalisation (`0x447280`), the cockpit blocked
tests (`0x445fc0`, `0x5d39f0`), the chase heading follower (`0x445c20`, only read by a subclass), the
replay RPM source and HUDINDEX option.

## Platform scaffolding (not original code)

`src/Platform/Native` contains everything that is not recovered UG2 code:

- `OriginalImage.cpp` maps the player's own `speed2.exe` `.rdata`/`.data` at their
  original addresses after a SHA-256 check; `OriginalSymbols.ld` binds recovered
  `extern` globals to those addresses. No original instruction executes.
- `NativeVehicle.cpp`: vtable shims forwarding each original slot to the recovered
  method, the `New...` allocation factories, the D3DX matrix multiply,
  scaffold race-status/context/random
  objects, keyboard controls, and stock car records copied from GLOBALB.BUN CarTypeInfo
  `+0x110` (upgrade composition `0x5ac4f0`/`0x5997e0` not yet recovered).
- `DemoMain.cpp`: SDL2/OpenGL window and input; `NativeFng.cpp`/`NativeHud.cpp`: FNG HUD; `NativeAssets.cpp`/
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
about 95 km/h when its scripted steering takes it into the roadside wall at eight seconds,
where it now slides along the wall instead of leaving the map. These are plausibility
observations, not equivalence evidence.

## Known limitations

- Visible-section selection and the player-view flag test are applied (see
  [scenery visibility](#scenery-visibility-fourth-pass)). Pixel-size LOD `0x5d64d0`,
  the nearest-boundary fallback `0x5db770`, and view flags 0x02/0x01 remain unresolved.
  Blended draws are not depth-sorted; streaming, original shaders, and time-of-day
  colour selection remain unrecovered. Missing textures include 18 TRN_SH parts
  (`0x9c3ce745`) and four XS signs, as recorded in the scenery investigation.
- Paint, vinyls, damage textures and headlight/brakelight switching are not applied.
- Every collision pack is loaded at start-up
  instead of the original streaming, and the spawn point is chosen heuristically.
- Steering input scaling (±30° road-wheel angle), reverse engagement (hold brake at rest)
  and the controls object are scaffold; the original input filtering is unrecovered.
- Native GCC x87 code is not bit-exact with MSVC; FPU control word is set to 0x027f.
- Free-drive race phase 0 is used; race-phase-dependent branches are untested.

## Outstanding recovery checks

- Extend collision verification to GJK `0x5b3390` and impulse resolution `0x5af0e0`;
  resolve the 42 degenerate simplex tie differences in `0x595040`.
- Recover car-vs-car/world-object paths (`0x5b31f0`, `0x5ae000`, slot 0x8c),
  cylinders (`0x5a23d0`, `0x5a2780`), and collision effects/sounds (`0x5a9620`, `0x59fb70`).
- Recover the enclosing car update `0x6012b0` and the original world-clock call site.
- Recover camera collision pushes/shake, the acceleration producer at car +0x80,
  remaining HUD inputs/features, and full FEng script playback/rendering. Additional HUD
  source recovery requires separate verification and integration evidence.
- A free/aerial camera remains useful for wider scenery checks.

For reproducible wall checks, run:

```sh
UG2_STEER=-0.4 UG2_DEBUG_COLLISION=1 build/native/ug2sim "Need for Speed Underground 2" SKYLINE 900
```
