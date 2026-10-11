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
this is inner `selection2E0.values0[1]`, rather than the previous mislocated array's
index 5. This correction preserves the absolute access address.

## Constructor and upgrade selection

`PhysicsParameters.cpp` recovers the parameter constructor `0x5ac6e0` (227 bytes)
and tuning constructor `0x5a7970` (127 bytes). It preserves partial initialization:
unknown bytes and padding are not cleared by the recovered methods. The original
64-sample curve starts with 63 values of four and a final zero, with exact immediate
endpoint/inverse-spacing values. Tuning defaults retain the original 100-valued
bytes, zero float blocks, pointers and sentinel values.

`PhysicsUpgrades.cpp` recovers selection `0x5a6320`, index selection `0x599680`,
group selection `0x5a6400`, group clearing `0x5a6450`, and accessors `0x500070`,
`0x500080`, `0x500150`. Chunk 0x34a15 has 0x17c-byte groups containing four
0x5c-byte items. Selection preserves the already-selected early return,
prerequisite recursion, exclusion order, changed-count arithmetic and low-byte
boolean arguments. It retains direct indexing rather than inventing validation.
Unknown descriptor fields remain offset-based. The 0x94-byte selection object
belongs at parameter +0x2e0, and the 0xe8-byte tuning object at +0x374.

Native construction now calls the recovered constructor before loading its stock
record, then the recovered drive-curve builder. The physics speed method is not
yet called by steering. Record/upgrade composition `0x5b4dc0`, tuning application and mode variants remain
unresolved. The refresh wrapper `0x5b4fc0` (140 bytes) is recovered in
`PhysicsRefresh.cpp` and replaces its native trap. It suppresses composition
when the car index is -1, uses only the low byte of the select argument, chooses
modes 0/3/10/2 with the original flag precedence, and forwards the first two
arguments as complete dwords. The composer declaration retains those dwords
rather than truncating them prematurely to chars. Native still copies a
stock record and assigns index zero as a documented construction seam; its GLOBAL
loader does not yet install upgrade descriptors. The constructor recovery does
not establish full original record composition. Native traps retain the unresolved
method boundaries.

## Mode variants

`PhysicsVariants.cpp` recovers shared application `0x5a7530` (540 bytes) and
stdcall wrappers `0x5a7750` / `0x5a77c0` / `0x5a7830` (105/105/93 bytes).
They select class rows at exact 0.2/0.8 thresholds, preserving equality and the
0x144-byte table stride. Application retains the original interpolation order,
changes drag coefficients, inertia scale/mass, centre height, axle correction
coefficients, brake bias and drivetrain selector, then clamps height/bias and
rebuilds inertia through recovered `WheelIdentity`. The mode-3 and mode-10
wrappers zero nitrous capacity; mode 2 preserves it. Double intermediates retain
the original unspilled x87 arithmetic at 53-bit precision.

These routines are compiled into the native target but are not yet called by
its stock-record entry seam. The enclosing original composer and upgrade/tuning
application chain are still required; adding these helpers does not establish
mode integration.

## Verification

```sh
build/venv/bin/python tools/verify_pc_physics_parameters.py
build/venv/bin/python tools/verify_pc_physics_construction.py
build/venv/bin/python tools/verify_pc_physics_variants.py
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
and call traces agree. Refresh executes recovered source on the rebuilt side, including the speed
helper's invalid-index cases. A further **9,216 refresh comparisons** pass with
bit-identical state and composition-call traces: all 128 combinations of seven
mode flags, valid/-1 car indices, select arguments 0/1/-1/256, and complete dword
forwarding of 0/-1/0x12345678 in the first two arguments. Compose alone is a
controlled interface in explicit sentinel/refresh fixtures; it does not mutate
the object, and its caller registers are clobbered. Its mechanics are not verified
by this test. Generated results
stay under `build/analysis/physics-parameters/`.

The constructor/upgrade verifier pins nine original function bodies in
`physics-construction.json`. On 2026-10-11, **12,928 comparisons** were bit-identical:
128 cases each for parameter/tuning constructors and three accessors, and 3,072
cases each for four selection methods. Both sides execute their complete covered
callee chains. No controlled interfaces are used. Fixtures cover four synthetic
groups, null item lookups, filtered/all-group clearing, partial initial byte
patterns, stateful enabling/disabling, acyclic prerequisites, exclusions and
low-byte argument behavior. It compares full guarded state and descriptor/table
buffers, integer/pointer returns, stack cleanup and preserved registers. Reports
remain under `build/analysis/physics-construction/`. Cyclic prerequisites,
malformed indices/counts and chunk loading are excluded.

The variant manifest `physics-variants.json` pins four recovered routines and
matrix identity. On 2026-10-11, **6,624 comparisons** (1,656 per routine) passed
with bit-identical guarded state, stack cleanup and preserved registers across
46 retail records, four upgrade-level patterns and nine selectors including
immediately adjacent binary32 values around both thresholds. Arbitrary rows
exercise the shared routine, including clamp branches; wrappers use original
executable tables. Rebuilt calls execute recovered matrix identity. There are
no controlled interfaces. Generated reports remain in
`build/analysis/physics-variants/`. NaNs, arbitrary malformed records and alternate
floating precision are excluded. MSVC uses `/Op` for the variant unit and the
existing `/Ox` configuration for the separately compiled matrix dependency.

Native build and forward/reverse driving smoke checks pass with the recovered
constructor and curve builder integrated. Finite stock fixtures do not cover arbitrary upgrade combinations,
malformed curves, alternate floating precision or full steering composition.
**Byte matching is not established.**
