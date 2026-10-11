# Physics parameter layout and drive curve

Reference: UG2 PC v1.2 executable SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.

## Recovery and native layout correction

`PhysicsDriveCurve.cpp` recovers `0x5b5ab0` (513 bytes) and physics speed helper
`0x5b5930` (371 bytes). The drive curve derives wheel radius and coefficient from
the drivetrain selector, computes shift points, samples engine force against
speed, then resamples the result into the destination curve. It retains the
original single gear transition per sample, the input calculated before that
transition, and the zeroing of the last generated sample. No new force model
or shift rule is introduced.

The speed helper searches the curve against aerodynamic drag divided by mass,
retains refresh/composition calls for the invalid index sentinel, and applies
the record +0x290 speed limit. At `0x5b59d7`, the original stores a binary32 next
speed but compares the unrounded x87 value against the curve endpoint. The
source makes this order explicit with a double intermediate under the demo's
53-bit x87 precision. Rounding before this comparison changes iteration counts
for a retail vehicle record.

The original methods receive the physics object's **+0x10 subobject**.
`PhysicsParameters` therefore holds the record at +0x30, upgrades at +0x2e0,
64 curve samples at +0x45c and the curve at +0x55c, for a size of 0x570.
The enclosing `PhysicsInfo` holds that subobject at +0x10, placing the record at
outer +0x40. Native previously placed it at outer +0x30 and allocated only
0x400 bytes. Native now allocates 0x580 bytes and passes the correct record and
subobject addresses to mover, HUD and parameter methods. Unknown fields remain
offset-based.

`VehicleStateExport` retains the original read at outer +0x2f4 (`0x5a5968`):
this is inner `upgrades2E0[1]`, rather than the previous mislocated array's
index 5. This correction preserves the absolute access address.

The drive-curve and speed methods are compiled into the demo but are **not yet
called by its partial vehicle construction or steering**. Original construction
`0x5ac6e0`, record/upgrade composition `0x5b4dc0`, refresh `0x5b4fc0`, tuning and
mode variants remain unresolved. Native still loads a stock record and assigns
index zero as a documented construction seam. The allocation correction does
not make that composition complete. Native traps retain unresolved method
boundaries; no missing fields are synthesized to force the steering chain to run.

## Verification

```sh
build/venv/bin/python tools/verify_pc_physics_parameters.py
cmake --build build/native -j2
python tools/smoke_native_driving.py
```

Run MSVC/wibo jobs sequentially. `physics-parameters.json` pins the recovered
bodies, curve lookup and shift-point dependencies. The verifier compiles recovered
curve lookup and shift-point source into the rebuilt side, executes original
`__ftol2` as an analysis runtime dependency, and uses MSVC 7.1 `/Ox /Op /Ob0`.
`/Op` preserves floating arithmetic order; it does not establish byte matching.

On 2026-10-11, all **184 drive-curve and 736 physics-speed comparisons** were
bit-identical across 46 retail records, output counts 2/9/16/64, three speed-limit
fixtures, mode-selection flags and invalid-index cases. Full compared state,
guarded scratch samples, floating returns, stack cleanup, preserved registers
and call traces agree. Compose/Refresh are controlled interfaces only in explicit
invalid-index fixtures; they do not mutate the object, and their caller registers
are clobbered. Their mechanics are not verified by this test. Generated results
stay under `build/analysis/physics-parameters/`.

Native build and forward/reverse driving smoke checks pass with the corrected
layout. Finite stock fixtures do not cover arbitrary upgrade combinations,
malformed curves, alternate floating precision or full steering composition.
**Byte matching is not established.**
