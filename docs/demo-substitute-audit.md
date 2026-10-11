# Demo substitute audit and driving recovery

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
This is an integration inventory, not an executable-wide coverage claim.
Linux device, window, filesystem, OpenGL and allocation backends remain separate
platform seams. Original gameplay decisions must be recovered from the binary.

Active recovery workflow: continue replacing recoverable substitutes, validate
each checkpoint, and commit and push the verified changes before pauses.

## Inventory and implementation order

| Milestone | Substitute or incomplete composition | Evidence and remaining recovery |
| --- | --- | --- |
| 1. Driving controls | `DemoMain.cpp` combines keyboard/pad actions and smooths steering at 2.5 units/s; `NativeVehicle.cpp` scales steering to ±30°. Native constructs a partial input object. | Pedals `0x407a30`, forward selection `0x407310` and gear accessors are now recovered and integrated. Human update `0x41e2c0` captures devices via `0x571a60`, updates pedals, then steering at input +0x220 via `0x41dfc0`. Human constructor `0x425fc0`, base constructor `0x423330`, keyboard filtering `0x416bb0`, analog filtering `0x4061c0` and alternate path `0x406a40` remain unresolved. `0x421030` belongs to the AI input vtable and must not substitute for human steering. |
| 2. Vehicle/session construction | `CreateNativeVehicle` manually allocates objects and selects stock source data and installs native vtables. `LoadCarType` and hull parsing use native loaders. | Physics constructor `0x5ac6e0`, tuning constructor `0x5a7970`, upgrade selection/grade queries and drive curve `0x5b5ab0` are now recovered; native integrates construction and curve building with the corrected layout. Speed helper `0x5b5930` and refresh wrapper `0x5b4fc0` are verified; refresh replaces its native trap. Mode variants `0x5a7530`/`0x5a7750`/`0x5a77c0`/`0x5a7830` are recovered and verified but await integration. Nine per-upgrade record modifiers are verified with recovered grade-query callees. Record builder `0x5ac4f0` and its automatic upgrade search are now recovered and verified; native uses the builder instead of its raw stock-record copy. Extra application/removal `0x5997e0`/`0x599d90` and tuning eligibility/count `0x59ae40`/`0x59b0e0` are now recovered and verified; native applies extras after record building. Tuning offset query `0x59aee0`, field-address switch `0x59b200`, engine normalization `0x59b3c0` and value reader `0x5a7da0` are recovered and verified but await integration, as are leaves `0x59af80`, `0x59b080`, `0x59b160`, `0x59b4a0`, `0x59b5c0`, `0x59b6c0`, `0x401050`, `0x5a79f0`, `0x5a7b80`, `0x5a7cb0`, `0x591320`, `0x401790`, range builder `0x5aca50`, display range `0x5b09a0`, value mapping `0x5b2800`, engine range limit `0x5ace80`, entry limits `0x5b2980`, setters `0x5b3ee0`/`0x5b2ed0`/`0x5b3060`/`0x5b3100`/`0x5b2c00`, setting getter `0x5b3e40` and curve peak `0x5ac980`. Parameter composer `0x5b4dc0`, full tuning application `0x5b4780`, descriptor loading and class-index calculation remain unresolved. Recover input, car `0x604260`, player/session and upgrade composition rather than extending records with guessed values. |
| 3. Update composition and timing | `StepNativeVehicle` composes body/collision routines and advances the clock after physics. Enclosing car update `0x6012b0` is reduced. Native previously produced linear acceleration using its own previous-velocity state. | Kinematics prefixes of `0x5fbbe0` and `0x604740` now replace that producer, including angular acceleration. World caller `0x609ce0` orders snapshot, body advance `0x5f0f00`, collisions `0x5ffa50`, and post-update. Complete those callers and the game-loop clock call at `0x5814ea` inside `0x5811c0`; isolated prefix equivalence does not verify the whole update. |
| 4. Placement, reset and camera | `NativeWorld.cpp` chooses a flat road polygon near map centre; `CreateNativeVehicle` builds a yaw matrix. `DemoMain.cpp` recreates a vehicle after 150 airborne frames, using periodically saved grounded positions. Drive camera source still omits several original callees. | Recover original spawn/reset and player/camera composition. Camera collision pushes `0x444530`/`0x44b040`, shake `0x452ed0`/`0x572a50`, finalisation `0x447280` and cockpit obstruction `0x445fc0`/`0x5d39f0` remain seams. The acceleration input is now supplied by the recovered prefix. |
| 5. HUD and frontend | `NativeHud.cpp` supplies free-drive feature masks, selected values, fixed node storage and partial FEng adapters. Native pause/settings/Controls adapters compose recovered items inside native screen/list handling. | Player masks `0x600090`, feature allocation `0x5f1e10`, full screen constructors/dispatch, profile persistence, animation, GPS, notifications and sound remain unresolved. Existing HUD, pause and Controls verifiers cover bounded routines, not all frontend behavior. |
| 6. Assets, world and presentation | Native bundle parsers load all collision packs at startup; OpenGL draws retail geometry/textures and FEng. Visibility, LOD, shader/material, font and animation adaptations are partial. Audio and many world/gameplay systems are absent. | Recover streaming and original resource composition, scenery visibility/LOD, material/time-of-day decisions, full FEng composition and audio. Linux rendering and device APIs may remain backends; they do not establish original rendering or gameplay equivalence. |

Other explicit native boundaries include slot-pool/list allocation in
`NativeWorld.cpp`, trace/trap methods in `NativeVehicle.cpp` and `NativeHud.cpp`,
and mod-provider callbacks. The input event dispatcher `0x578e90` currently drops
the selected event in the native demo. Its original receiver and arguments are
retained in the recovered caller and checked at the analysis boundary. It has
not been proven irrelevant to gameplay.

## Implemented driving recovery

The next [human steering checkpoint](pc-human-steering.md) recovers and integrates
the steering subobject constructor/destructor and verifies its timed averages
and rate helper. Steering smoothing and angle scaling still remain native while
the maximum-angle/physics composition chain is recovered.

The [physics parameter checkpoint](pc-physics-parameters.md) corrects the native
record/subobject offsets and allocation, integrates the recovered constructor
and drive-curve builder, and verifies speed search and upgrade selection. Full original record composition remains required before
steering integration.

`Physics/HumanDriverInput.cpp` recovers gear accessors `0x405b20`, `0x405b30`,
`0x405b40`, `0x405b50`, human forward selection `0x407310`, timer advancement
`0x577310`, and pedal processing `0x407a30`. The latter retains hold timing,
reverse gating, pedal swapping/clamping, mode-specific curve/ramp processing,
RPM gating, replay suppression, event selection and car +0x438 export. The raw
handbrake action is input +0x2ec; reverse gating uses brake >0.6 and throttle <0.5.
SDL capture feeds +0x2d4/+0x2dc/+0x2ec instead of directly inventing reverse logic.
The original curve helper `0x5764d0` is the previously recovered `EngineCurve`.
Native free-drive startup explicitly calls recovered forward selection after
partial input construction; otherwise the original constructor's neutral
request would persist. The original session entry that selects drive remains
unrecovered, and this call is a documented native entry seam.

`Physics/ReplayState.cpp` recovers predicate `0x5f00f0`; recording/playback itself
is still unresolved. `MoverInput.cpp` holds the recovered pedal setters formerly
in `PhysicsMoverUpdate.cpp` so analysis can execute them with the recovered input.

`Physics/CarFrameKinematics.cpp` recovers only the guarded prefixes
`0x5fbbe0..0x5fbc56` and `0x604740..0x60482a`. The first snapshots position,
linear velocity and angular velocity into car +0x510/+0x520/+0x530. The second
computes linear acceleration at +0x80 and angular acceleration at +0x120, using
one reciprocal and the observed binary32 difference spills. Native invokes
them around body advancement and collision resolution and removes its former
previous-velocity acceleration calculation. Original world/player logic after
each prefix is not recovered by this change.

## Verification and reproduction

```sh
build/venv/bin/python tools/verify_pc_demo_driving.py
build/venv/bin/python tools/verify_pc_engine.py
cmake --build build/native -j2
python tools/smoke_native_driving.py
build/native/ug2demo --car SKYLINE --frames 60 --screenshot build/analysis/demo-recovery/demo.ppm
```

Run MSVC/wibo verification jobs sequentially. The manifest
`config/SPEED2_EXE_1_2/demo-driving.json` pins the executable and individual
function/prefix fingerprints, including the timer, curve and setter dependencies.
The verifier builds with MSVC 7.1, executes original and rebuilt code in Unicorn,
and checks state, setter/curve/timer/replay/forward call traces, the controlled
event receiver/arguments, stack cleanup and preserved registers. Prefix checks
stop the original before its first unrecovered instruction.

On 2026-10-11, 4,216 comparisons passed: 3,126 pedal cases, 1,024 kinematics
prefix cases, 24 gear accessor cases, 15 forward-selection cases, and 27 replay
predicate cases. Compared state was bit-identical in all cases. Fixtures include
reverse thresholds, the 0.6 trigger, mode flags, hold timers, timer sentinels,
replay flow, RPM gates and nonpositive prefix durations. Float checks allow a
2e-6 tolerance, but no tested state required it. Generated results are local at
`build/analysis/demo-recovery/verification.json`.

**Byte matching is not established.** Finite synthetic cases do not establish
complete behavior or whole-caller equivalence. Analysis executes original
`__ftol2` as a compiler runtime dependency and maps retail data; native builds
use their own compiler conversion. Native forward/braking (960 frames) and
reverse (360 frames) smoke checks passed with finite state. Those are demo
integration checks, not original game trajectory comparisons. The remaining
inventory rows are unfinished recovery work, not implemented milestones.
The native smoke script also requires forward/reverse displacement and forward
gear changes, so an idle neutral run cannot pass. A 60-frame graphical demo run
rendered successfully without an unrecovered-code trap. The existing engine
verifier passed its 780 curve, 8,064 drive/derivative, 144 control and 2,700
adjustment cases after integration of its curve helper into pedal processing.
