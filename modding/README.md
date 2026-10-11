# Native modding, API version 1

The expanded roadmap for configuration, Lua, and native mods is in [PLAN.md](PLAN.md).
This README describes the implemented native interface.

This is optional platform scaffolding for the rebuilt 32-bit Linux demo. The SDK,
loader and example plugins are separate from recovered UG2 source. They do not
establish full game recovery, and no alternative vehicle solver ships here.

## Build, launch, install

```sh
cmake -S . -B build/modding -DUG2_ENABLE_MODDING=ON
cmake --build build/modding -j4
build/modding/ug2demo --car SKYLINE --width 1920 --height 1080
build/modding/ug2demo --no-mods
build/modding/ug2sim "Need for Speed Underground 2" SKYLINE 1200
cmake --install build/modding --prefix /your/install/directory
```

The default remains `UG2_ENABLE_MODDING=OFF`. Enabling it builds the loader into
native hosts and puts examples in `build/modding/mods`. The SDK installs under
`include/ug2`, the hosts under `bin`, and examples under `bin/mods`. The hosts still
need the player's installation and the existing reference-image/runtime data
seams. No assets, executables or compiler binaries are redistributed.

The demo accepts `--game PATH`, `--mods-dir PATH`, and `--no-mods`. The headless
host retains its positional game/car/frame arguments; mod options follow them.
Without `--mods-dir`, look for `mods` beside `/proc/self/exe`, independent of the
working directory. Without mods, existing recovered dynamics and HUD behavior
remain the baseline.

```
mods/
    load_order.txt
    original_physics/plugin.so
    widescreen_hud/plugin.so
```

`load_order.txt` lists one enabled mod ID per line. Blank lines and `#` comments
are accepted. IDs contain only ASCII letters, digits, `_`, and `-`, up to 80
characters. Duplicate and malformed entries are skipped with diagnostics.
Unlisted folders remain inactive. Missing order files run without mods. Libraries
must be 32-bit ELF shared objects. Missing libraries, unresolved symbols,
incompatible descriptors and failed initialization are reported and skipped.

The supplied order is `original_physics`, then `widescreen_hud`. Callbacks run in
file order; the last successfully initialized physics provider wins. The future
mod manager can edit this file. There is no dependency resolver or reload system.

## Write a plugin

Include only `ug2_mod_api.h` for the stable interface. Export `ug2_mod_query` with
C linkage and `UG2_CALL`, returning a static `Ug2ModDescriptor`. Its ID must match
the folder and order-file entry. Return `UG2_OK` from initialization after
registering callbacks/providers using the supplied host function table.

```sh
c++ -std=c++11 -m32 -fPIC -shared -fvisibility=hidden \
    -I /your/install/directory/include/ug2 \
    modding/examples/original_physics/plugin.cpp -o plugin.so
```

The header is also usable from C. Do not link recovered static libraries into
plugins. Do not use the game's executable-only `-fno-pie` flags for shared
libraries. Use the platform's default i386 structure alignment and calling
conventions. Version 1 requires exact descriptor/provider/state sizes and an
exact API version; incompatible future revisions require a version change.

No STL objects, exceptions or ownership cross this ABI. Keep descriptors and
function pointers alive until unload. Host function tables, identity strings,
event pointers, and input snapshots are borrowed. Plugins allocate and free their
own state; copy snapshots before retaining them. Never throw through host calls.

Subscriptions belong to the registering plugin. Register during initialization
or callbacks; new subscriptions become visible after the current event finishes.
Register at most one physics provider per plugin, during initialization. Failed
initialization discards registrations and calls shutdown to roll back partial
plugin allocations. Shutdown must tolerate partial initialization.

Lifecycle and presentation callbacks are:

- `UG2_HOST_READY`: native globals and validated reference image are available;
  a vehicle, world track and OpenGL context are not necessarily available yet.
- `UG2_VEHICLE_CREATED` / `UG2_VEHICLE_DESTROYING`: handles are valid throughout
  the event. The provider create callback precedes CREATED. DESTROYING precedes
  provider destruction and handle invalidation.
- `UG2_PRE_SIMULATION` / `UG2_POST_SIMULATION`: bracket the outer native world
  advance and collision response, not every original integrator substep.
- `UG2_PRE_WORLD_PROJECTION`: current framebuffer dimensions and projection
  tangent read/write API are available before the world is drawn.
- `UG2_POST_WORLD_DRAW`: after scenery and vehicle rendering, before the HUD.
- `UG2_HUD_READY`: after the driving HUD has been configured; can repeat on respawn.
- `UG2_PRE_HUD_LAYOUT`: after HUD updates/animation, before draw composition.
- `UG2_POST_HUD_DRAW`: after the HUD, including when no HUD package is active.
- `UG2_SHUTDOWN`: after vehicles/provider state have been destroyed. Plugins unload
  in reverse order before the demo destroys its OpenGL context.

All callbacks run synchronously on the game thread. Return promptly. Ordinary
callback errors disable that plugin's ordinary callbacks. Errors in the active
provider, including its ordinary callbacks, terminate the run with cleanup. A
failed provider never switches to another solver mid-drive.

The host preserves floating-point environment/control settings around plugin
query, library initialization, initialization, callbacks, shutdown and unload.
Render callbacks have a current OpenGL context. Restore all modified graphics
state, matrices and bindings before returning. The host does not sandbox native
code or recover crashes, hanging callbacks, or memory corruption.

## Stable HUD and projection access

During callbacks, enumerate or look up HUD objects by original hash, inspect type
and parent handles, and request a draw translation during PRE_HUD_LAYOUT. Handles
contain a package generation and index; unloading invalidates them. Vehicle
handles are separately checked monotonic lifetime IDs; respawn creates a new ID.

Translations use original centred 640x480 virtual-screen units, with Y down.
Each frame begins with zero translations. `hud_translate` sets an absolute
per-object draw offset; later plugins can override it. Group offsets propagate
once through descendants and do not change their original state or animations.
Masks use the same translated draw items as their contents.

During PRE_WORLD_PROJECTION, `get_projection` reports current half-angle tangents
and the recovered 4:3 reference tangents for the current camera FOV. `set_projection`
changes only the current tangents; they must be finite and positive. Reference
values and clip planes remain host-owned. The result is reused for debug picking.

The `widescreen_hud` plugin both anchors the driving HUD and implements Hor+ FOV:
retain the original 4:3 vertical tangent and multiply its horizontal tangent by
`(width/height)/(4/3)`. Camera selection, camera motion and FOV interpolation remain
recovered code. At 4:3 and narrower it leaves projection and HUD placement alone.
For wider screens HUD displacement is `(width*480/height - 640)/2` virtual units.

Observed HUD_SingleRace.fng anchor evidence:

| Hash | Role | Direction |
| --- | --- | --- |
| `5164d4ea` | Group GUID `8b81`, parent of tachometer, turbo, NOS line/fill/gamebreaker | Right |
| `1149c7da` | NOS_Mask, separate root | Right |
| `3f8644ca` | Group GUID `e49b6`, parent of Cash_data and Cash_header | Right |
| `30d59b88` | Group GUID `e0722`, parent of Show_case_backing | Right |
| `90fe80f3` | Track2000_map | Left |
| `40cdc515` | Minimap_Mask | Left |
| `f2a54430` | Minimap_North_indicator | Left |
| `382d2fc9` | TrackMapTargetRing group | Left |
| `c1347e2f` | PlayerCarIndicator_1 | Left |
| `1b259972` | StartLineIndicator | Left |

Cash_Bank, Cash_Reward and Region_Message remain centred messages. This list comes
from the locally inspected package hierarchy and current HUD consumers, not
coordinate guessing. Missing core anchors produce a warning; absent optional
objects are skipped. Menus and other HUD packages are outside version 1.

## Vehicle physics providers

Implement `create`, `destroy`, `step`, `impulse`, and `synchronize`. The default
example returns `UG2_PHYSICS_ORIGINAL` from step; the host invokes the recovered
CarCollisionBody::Advance exactly once and retains original collision impulses.
It demonstrates dispatch, not a second implementation of UG2 physics.

A custom provider returns `UG2_PHYSICS_CUSTOM` and a complete output snapshot.
Choose delegated or custom mode on the first step and keep it for that vehicle's
lifetime. A new vehicle on respawn may choose again. Custom providers own dynamics
and tire/engine/suspension calculations. The host retains collision geometry,
pair finding, contact solving, materials, bounds/snapshots and collision ordering.

State conventions:

- Position, linear velocity, angular velocity and impulses use world axes. +Z is
  up; the vehicle's local +X is forward. Time is seconds. Matrices have the original
  WheelMatrix/OpenGL element layout: axis vectors at 0..2, 4..6, 8..10, last row
  0,0,0,1 for orientation. Position is separate.
- Mass and body-local inertia retain original record units. No unverified kg or
  SI conversions are implied. The adapter rotates inverse inertia into world axes
  for collision response using recovered matrix helpers.
- `impulse` receives a world-axis impulse and a world-axis lever arm relative to
  the chassis centre, not an absolute contact position. Update the mutable output
  state for each impulse; the host immediately republishes it for further contacts.
  Do not implement contact detection/response a second time.
- Wheel local/world matrices are presentation transforms from the current native
  wheel drawing path. Spin and suspension retain their original observed values;
  wheel angle is a float. Contacts are 0/1. RPM is the existing scaled engine value;
  gear is the original ratio index (0 reverse, 1 neutral, 2..7 forward).
  Turbo is raw state `value40C`; nitrous is raw nonnegative `timer414` ticks.
- Recovered collision rewind can interpolate pose/velocity before synchronization.
  Treat the post-collision snapshot as authoritative input, even if its intermediate
  orientation is interpolated. The next custom output must supply a valid rotation.

Custom state must contain finite fields, a positive finite mass/reciprocal,
right-handed approximately orthonormal orientation, positive-definite symmetric
inertia, and valid contacts/gear. Reject inverse-inertia overflow before contacts
run. Nonfinite state, invalid output, create/step/impulse/synchronize failure, or a
solver-mode change terminates the run. Destroy must tolerate partially initialized
provider state. Solver output is validated before publishing; plugin memory writes
outside the API are not protected.

Presentation is copied into native proxy objects and the recovered state exporter
is used to feed the existing camera/HUD. This conversion is a documented platform
seam; it does not claim recovery of a new original producer. Provider state stays
outside fixed-size recovered collision bodies; the native subclass remains exactly
0x1a0 bytes. Current integration covers the demo's single local vehicle and static
track. Broader recovered systems and additional original data consumers need their
own integration verification.

## Expert access and verification

Declare `UG2_CAP_EXPERT` and an exact `required_build_id` to obtain the expert table.
Mismatches are rejected before initialization. The ID is generated from relevant
source/header/linker-file hashes, native CMake settings/cache, compiler binary,
and the reference-image identity embedded in OriginalImage.cpp. It includes dirty
source changes; it is not just a Git commit. It is a conservative source/build
fingerprint, not a claim of an executable-wide byte match. Read the running host's
`build_id` to build a targeted expert plugin; the generated header is in the build's
`modding/` directory. `vehicle_pointer` borrows NativeVehicle only during callbacks.
Expert plugins must supply their own matching internal headers and rebuild after
fingerprint changes; stable-only plugins need no game-source headers.

```sh
ctest --test-dir build/modding --output-on-failure
python tools/verify_native_modding.py --build build/modding \
    --game "Need for Speed Underground 2" --baseline-build build/modding-disabled
python tools/verify_native_mod_hud.py --build build/modding
```

CTest exercises loader/protocol fixtures without assets. The optional game test
checks the real collision adapter, impulse/mass/inertia exchange, stale handles,
respawn, invalid-state cleanup, and 1201-frame baseline/empty/forwarding equivalence.
HUD checks capture 4:3, 16:9, 21:9 and 32:9 frames, verify 4:3 identity and edge
translations, compare the central 3D view for Hor+ consistency, render skins 0..10,
and capture a 120-frame animated run. They require Pillow and the player's assets.
Screenshots and reports stay under the ignored build directory. Translucent HUD
coverage has a bounded image-threshold tolerance; inspect screenshots as well.

Validation recorded on 2026-10-11: both CMake option states built; installed
headers compiled as C and an external C++ forwarding plugin; all 15 loader/native
scenarios passed. The native adapter fixture checks inverse mass, unequal
body-local inertia rotated into world axes, impulse publication, collision
position separation, and respawn. The 1201-frame simulation comparison matched
disabled, empty-order, and forwarding-provider runs. HUD/FOV checks passed at
640x480, 1280x720, 1680x720, and 2560x720; all 11 skins and the animated run were
captured, with the default skin's wide screenshots visually inspected. These
captures do not establish exhaustive animation, skin, or respawn visual coverage.

Affected recovered verifiers ran sequentially: `verify_pc_hud_freeroam.py`
(10,612 comparisons), `verify_pc_hud_minimap.py` (7,972 cases), and
`verify_pc_body_post_update.py` passed their bounded checks. This evidence is
separate from mod integration; it does not establish additional byte matching.

These are bounded native integration tests, not full physics coverage or byte
matching. The current reference-image, renderer, fonts and asset-loader seams still
apply. Lua/configuration loading, stable tuning-write APIs, race/economy editing,
Windows, reloads, dependency resolution and the in-game mod manager are deferred.
