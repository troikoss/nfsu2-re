# UG2 PC vehicle update recovery

The driving prototype's physics are not established as retail-equivalent.
This research uses UG2 first; existing UG2 mechanics must not be substituted
with another game's solver or guessed formulas.

This document follows the historical recovery sequence. Later sections supersede
earlier leads; dated-stage references to missing callbacks are not a current
coverage inventory. See [wheel/body verification](pc-wheel-physics.md) and
[the native demo](NATIVE_DEMO.md) for subsequent recovery and integration.

## Locating consumers

`tools/find_pc_vehicle_consumers.py` fingerprints the local executable and all
18,444 seeded function ranges before searching decoded instructions for selected
vehicle-field displacements and exact NUL-delimited string references. Stack
displacements and absolute addresses are excluded from field matching. The first
pass found 98 candidates, including unrelated structures; these are leads only.
The seed boundaries themselves remain community-derived, and linear decoding
may stop at embedded data. This is not a complete control-flow analysis.

The `PhysicsMover` string at VA `0x7a2094` is referenced by `0x5a47b0`.
That routine passes named state through a serializer-like call chain and calls
subobjects at offsets `0x20`, `0x24`, and four-element pointer arrays at `0x28`
and `0x38`. Exact subobject meanings still need confirmation.

A static function-pointer occurrence of `0x5a47b0` exists at `0x7a25e8`.
The preceding entry at `0x7a25e4` points to `0x5b72d0`, a 658-byte routine that
accepts a float argument and iterates a sub-update. The surrounding pointer
table includes getter-like methods and an adjusted-this thunk; table ownership
and callers have not yet been fully recovered. Call it a PhysicsMover-associated
update candidate, not a fully named original class method.

## Original timestep partition verified

Routine `0x5b72d0..0x5b7562` SHA-256:
`a8e4f0888958717ca75d905c1f6fb2982b6e1c378e267666e1a4ed3a4366a565`.

The fragment `0x5b7344..0x5b736d` reads the float argument, divides it by
the float at `0x7fc070`, calls the original conversion helper at `0x75bed4`,
adds one to the integer result, and divides the original argument by this count.
The constant is binary32 `e02d903b`, approximately `0.004399999976158142`.
For the finite nonnegative inputs tested, conversion truncates the quotient.
The count is therefore truncation of the quotient plus one, **not** simple ceil;
an exactly representable integer quotient still receives an extra substep.

Interpreting the argument as seconds gives:

| Supplied duration | Original count | Stored substep duration |
| --- | ---: | ---: |
| 1/60 s | 4 | approximately 1/240 s |
| 1/144 s | 2 | approximately 1/288 s |
| 1/240 s | 1 | approximately 1/240 s |

The caller's clock source and unit must still be traced. These rows describe
supplied test values, not verified render-rate coupling. This function alone
does not prove the original simulation uses a fixed 240 Hz tick or that the
whole game is frame-rate independent.

`tools/probe_pc_vehicle_substeps.py` executes this original fragment and the
original 117-byte conversion helper without stubs. It passed 216 cases: durations
corresponding to 10–1000 Hz, zero, and one-ULP neighbors of 32 threshold multiples,
with x87 control words `0x027f` and `0x037f`. It checks original count, stored count
as float, stored substep duration, instruction termination and stack balance.
Some threshold expectations depend on x87 precision; the test accounts for this.
It does not execute the full vehicle update or test negative/NaN/infinite durations.

Subsequent static instructions iterate the computed count, write each substep
duration to global `0x7fc800`, and call `0x5aa480` with receiver `this + 8`.
The routine accumulates two three-float arrays from the subobject at `this + 0x20`
and divides the accumulated arrays by the count. Their force/telemetry meanings
have not been proven. Do not transplant this stepping fragment into the custom
solver and claim original handling.

## Reproduce and continue

From the matching-decomp checkout:

```sh
build/venv/bin/python -m pip install -r requirements-pc-analysis.txt
build/venv/bin/python tools/find_pc_vehicle_consumers.py
build/venv/bin/python tools/probe_pc_vehicle_substeps.py
```

Capstone is installed only in the local analysis environment. Original assets
and executable are read-only inputs; generated candidate/disassembly details
remain in ignored `build/analysis/racing/` artifacts.

Next: recover the input/caller chain for `0x5b72d0`, the sub-update at `0x5aa480`,
and the creation/ownership of the component at `this + 0x20`. Follow actual car
record pointers into drivetrain and tyre computations before specifying torque
sample RPM spacing or units. The original stepping rule is useful evidence,
but does not complete a source-built playable driving demo.

## Recovered derivative collector and integrator

The sub-update at `0x5aa480` calls `0x5a0d20` to collect derivatives from an
intrusive list. For each block it invokes virtual slot `0x18` with the output
cursor, then slot `0x04` for its scalar count. This collects **all** block
derivatives before updating any state. Links point to the link subobject four
bytes after the block's virtual pointer in the original 32-bit layout.

The integration routine then invokes each block's count at slot `0x04`, state
pointer at `0x0c`, and pre-integration callback at `0x20`. It updates scalar
states with `state += duration * derivative`, preserving list order, and finally
invokes the controller callback at virtual slot `0x04`.

Recovered C++ is in `src/Speed/Indep/Src/Physics/VehicleIntegrator.h`,
`VehicleDerivatives.cpp`, and `VehicleIntegrator.cpp`. Unknown virtual slots
remain unnamed. A 140-float local derivative buffer reproduces the observed
stack reservation; its exact original declaration is not known from symbols.
Actual registered counts must be traced before proving the maximum runtime size.

With MSVC 7.1 `/Ox /Z7 /EHsc`:

- The 91-byte derivative collector compiles byte-identically to original
  `0x5a0d20..0x5a0d7b`, SHA-256
  `b1fe80481921e3b9631b63170d2299c9c86b0a3eab6ea225d2f2a50ebf3bf0a4`.
- The integration candidate compiles to 274 bytes versus the original 275.
  It is **nonmatching** and must not be counted as byte-matched. Compiler loop
  scheduling differs; no inline assembly or forced registers are used to fake a match.

`tools/verify_pc_vehicle_integrator.py` compiles both, resolves the collector
call explicitly, and executes original and rebuilt code with the same controlled
virtual callbacks. It passed 480 differential cases, comparing every resulting
state byte and exact callback order. Tests include empty lists, zero-sized blocks,
scalar/unrolled-loop boundaries, multiple blocks, aggregate counts through 140,
zero/small/large finite durations, random signed states and two x87 precision modes.
State guards, instruction completion, balanced stacks and preserved callee-saved
registers are checked. Fixture callbacks clobber caller-saved registers.

The derivative fixture depends on other blocks' live state; the pre-integration
fixture mutates its own state. These expose accidental collect/update interleaving
and incorrect callback order. They are deliberate test fixtures, **not** original
vehicle dynamics. Original derivative and tyre/drivetrain callbacks have not been
recovered by this comparison. No engine gameplay source was changed.

Reproduce from this checkout:

```sh
build/venv/bin/python tools/verify_pc_vehicle_integrator.py
```

The evidence is stored locally in ignored
`build/analysis/racing/vehicle-integrator-verification.json`.

## Next component leads

A pointer-table shape scan found these leads near the PhysicsMover data. They
are not identified components until construction and list registration prove it:

| Table VA | Count-like result | State getter | Derivative-like callback |
| --- | ---: | --- | --- |
| `0x7a20c4` | 32 | `0x5a80c0` | `0x59c1e0` |
| `0x7a20e8` | 2 | `0x5a8230` | `0x59d350` |
| `0x7a210c` | 2 | `0x5a8740` | `0x59e130` |
| `0x7a2508` | 1 | `0x5aa700` | `0x5949b0` |
| `0x7a2560` | 4 | `0x5a80c0` | `0x598ac0` |

The literal `Drvtrn` immediately precedes table `0x7a2508`. Callback `0x5949b0`
writes one output float, uses fields `0x18`, `0x38`, `0x44`, `0x60`, `0x68`,
`0xe8`, and a coefficient table at `0x7fc298`. Its units and field meanings
remain unresolved; do not label it engine torque from this evidence alone.

The broad table scan also matched unrelated virtual tables. For example,
`0x7a24c4` has a superficially plausible count getter but its supposed derivative
slot returns a Boolean without writing a float. Table shape by itself is insufficient.
Local candidates are retained in `build/analysis/racing/integrator-block-candidates.json`.

Next action: trace construction/registration for `0x7a2508` and the actual
consumer/writers of its single scalar state, then recover callback `0x5949b0`
against original instruction execution. Keep the full force and road-contact
requirements intact; the generic integrator alone is not a playable solver.

## DriveTrain identified and recovered consumers

The following evidence supersedes the unresolved table ownership above:

- Constructor `0x5af9d0..0x5afb25` (341 bytes, SHA-256
  `240b4481fe4b3dbd9a69a7ad1248ccf3ae2241f5268c2a95aae69b5279bfd8d8`)
  installs table `0x7a2508`, stores its first argument at `+0x18`, copies four
  wheel pointers into `+0x24..0x30`, initializes scalar `+0x14` to zero, and
  copies the literal `Drvtrn` into its name field.
- Serializer `0x5944a0` uses the literal `DriveTrain`. PhysicsMover serializer
  `0x5a47b0` calls it on the pointer at mover offset `0x4c`.
- PhysicsMover construction `0x5b5cc0..0x5b62f1` calls the drivetrain constructor
  at `0x5b607f`. Its first argument is `mover[0x54] + 0x1b0`; the second points
  into a selected global record at `0x7fc2a8 + selector * 0x144`. The next
  arguments are the component at `mover+0x48`, the four-pointer array at
  `mover+0x38`, and `mover[0x58]`.
- Instructions `0x5b608f..0x5b60bc` store the result at `mover+0x4c`, append
  its link subobject `+4` to the controller list at `mover+8`, and add the
  virtual scalar count to the controller total. This is actual registration,
  rather than table-shape inference.

`Drivetrain.h` preserves a partial 32-bit layout. The `spec` pointer starts at
the observed car-record offset `0x1b0`. Its ratios begin at relative `0x20`,
therefore car-record `0x1d0`. Units and exact original field names remain
unconfirmed. Use offset-based names where semantics are unresolved.

Recovered `GetWheelDerivedValue`, original `0x5945f0..0x594665`, SHA-256
`9f91e5f805cd637690f9297d5e1ff054e501f7314433058ea7a7961b16b2a7cb`:

1. If spec `+0x10 >= 0.001f`, sum wheel pointers 3 and 2's `+0x28` values,
   multiply by half, then spec `+0x0c`.
2. If spec `+0x10 <= 0.999f`, add wheel pointers 1 and 0's `+0x28` values
   times half and spec `+8`. If the first branch also applies, halve the sum.
3. Multiply by spec's ratio selected by drivetrain integer `+0x3c`.

The threshold constants are the original binary32 values at `0x78435c` and
`0x79b158`. This is not continuous blending by the selector fraction. Wheel
mount identity and units are not inferred solely from array order.

Recovered `WriteDerivative`, original `0x5949b0..0x594a21`, SHA-256
`c0bda45979735e718fff1549be02300d2443f3fc20ccfaa4642d92b9c4b7fd7c`:

- Status `+0x60 == 3` writes zero.
- Otherwise start with spec `+0x1c` times drivetrain `+0xe8`.
- Positive adjustment `+0x68` multiplies by `1 + adjustment`.
- Negative adjustment with signed integer `+0x38 >= 3` multiplies by the
  original `0.35f` constant at `0x7a18d4`.
- Multiply by table `0x7fc298[mode44]`; initial indices 0..3 hold -3, 0, 1.5, 0.
  Wider valid table extent and runtime changes have not been established.
- Write one float and return integer zero.

MSVC 7.1 builds these to 119 and 132 bytes, versus original 117 and 113.
Both are **nonmatching**. `tools/verify_pc_drivetrain.py` passed 1,408 wheel-value
and 2,688 derivative cases with exact binary32 output comparison, x87 precision
control words `0x027f`/`0x037f`, balanced stacks, preserved callee-saved registers
and output guards. It executes no stubbed callees. Inputs are synthetic finite
values, eight ratio indices and modes 0..3; not retail handling traces.

Three tiny callbacks in `DrivetrainAccessors.cpp` compile byte-identically:
count 1 (`0x40cb20`, 6 bytes), state pointer `this+0x14` (`0x5aa700`, 4 bytes),
and pre-integration callback returning zero (`0x50d510`, 3 bytes). Their exact
fingerprints are recorded in the verifier. These shared tiny bodies do not
establish other classes' identities.

`tools/verify_pc_drivetrain_integration.py` passed another 1,280 comparisons,
executing original/rebuilt collectors and integrators with the real drivetrain
derivative, count, state pointer and pre-integration bodies. It checks the exact
state bytes, callback order, unaffected component bytes, stack and saved registers.
The single-block list is a synthetic fixture. The controller's final callback
is explicitly replaced by a no-op with caller-saved register clobber; the other
vehicle blocks and full post-step dynamics are excluded. This verifies composition
of recovered operations, not a playable car solver.

```sh
build/venv/bin/python tools/verify_pc_drivetrain.py
build/venv/bin/python tools/verify_pc_drivetrain_integration.py
```

Evidence is in ignored `build/analysis/racing/drivetrain-verification.json` and
`drivetrain-integration-verification.json`. Retail executable and assets are
unchanged. Source remains in the matching-decomp checkout, separate from OpenUG2.

Next: recover the block's slot `0x1c` at `0x5a1400`, which calls the wheel-derived
routine and stores its result at `+0x4c`, and trace writers of scalar `+0x14`.
The controller post-step chain also needs recovery. Car torque curves, tyre forces
and road contacts remain requirements before replacing the prototype handling.

Static follow-up identifies the controller table at `0x7a25d4` as entries
`0x5b04c0`, `0x5b04d0`, `0x5b0520`. Its final integration callback `0x5b04d0`
iterates all blocks' slot `0x1c`, then calls `0x5abb00` with the owning mover
at `controller - 8`, and tail-calls controller slot `0x08`. Therefore the no-op
controller fixture explicitly omits this real work.

DriveTrain slot `0x1c` at `0x5a1400..0x5a16e9` (745 bytes, SHA-256
`900882593a8fda0fcca4529c11e4f8041b67690c321574f79d0f1aaead33f98d`)
contains a four-way mode switch and writes scalar `+0x14` at bounds 0 and 1
in specific branches. It also changes the selected ratio and calls predicates
`0x594e90`/`0x594ed0` and routine `0x594710`. This establishes a fourth observed
mode and motivates extending the probes to 0..3; it does not yet establish
complete shift/clutch semantics. `0x594710` uses global state and randomization
through `0x5ea500`, so it must not be replaced with guessed shift timing.

## Recovered drivetrain post-integration chain

`DrivetrainPostUpdate.cpp` reconstructs `0x5a1400`, preserving all four mode
branches, status-3 behavior, ratio changes, scalar 0/1 bounds, component `+0x80`
writes and two timer paths. The timer paths read signed global `0x86518c` and
multiply by the original binary32 constant at `0x784268` (approximately
0.0002500000118743628). The clock's upstream source is still unresolved.
The timer limits are original globals: `0x7a18c8` approximately 0.45 and
`0x7a18cc` exactly 0.5; adjustment scale `0x7a18d0` is 2. These are not the
prototype's configurable 0.4/0.8-second missed-shift settings.

`DrivetrainPredicates.cpp` reconstructs both predicates called by the update:

- `0x594e90`: component `+0x78 < 0.3f` and component's pointed value at
  `+0x18` times original scale `0x7a1d38`, plus epsilon `0x7843ec`, greater
  than or equal to the wheel-derived scalar at drivetrain `+0x4c`.
- `0x594ed0`: absolute difference between component `+0x14` and drivetrain
  `+0x4c` less than or equal to the original tolerance at `0x7fc028` (50).

`DrivetrainRandom.cpp` recovers `0x594710`, its member wrapper `0x5ea500`,
and shared random-number routine `0x43c170`. The shared routine returns a
scaled remainder of the **old** unsigned seed modulo `0x7fffffff`, then updates
the seed with the original xor/shift sequence. Refresh clears three outputs,
checks the global context's suppression byte, and, when enabled, draws three
values using six bounds at drivetrain `+0xcc..0xe0`. Context and parameter
semantics remain partly unresolved; no random draws are replaced by averages.

`tools/verify_pc_drivetrain_post_update.py` compiles these and the recovered
wheel-derived routine with MSVC 7.1 `/Ox /Ob0 /Z7 /EHsc`. Inlining must stay
disabled for this comparison: `/Ox` alone inlined a random wrapper and retained
extra precision for a range argument, producing a detectable rounding difference
in one variation output. `/Ob0` preserves the original call boundaries and
binary32 argument rounding. This is a numeric requirement for these recovered
operations, not a custom gameplay change.

The verifier passed 456 original/rebuilt random cases and 1,920 post-update
cases with exact output byte comparison and **no stubbed callees**. It covers
unsigned seed boundaries, signed/zero ranges, x87 precision modes `0x027f` and
`0x037f`, all four modes plus out-of-range modes, scalar bounds, pending flags,
ratio changes, status 3, context suppression, timer/adjustment branches, and
affected component bytes. It checks unaffected fields, seed progression,
balanced stacks, saved registers and instruction-range guards. These are finite
synthetic mapped states, not retail traces or proof of complete shift/clutch
behavior in a driving car. Post-step routines of other vehicle components and
road contact still remain outside this comparison.

After original-address relocation, variation refresh is byte-identical (150
bytes) and its random member wrapper is byte-identical (23 bytes). Both
predicates and the shared RNG are **nonmatching**. The post-update compiles to
764 versus original 745 bytes; its linked jump table is executed, but it is
not counted as a byte match. `tools/pc_coff.py` loads and relocates compiled
sections including their local jump tables for analysis-only execution.

```sh
build/venv/bin/python tools/verify_pc_drivetrain_post_update.py
```

The report is in ignored `build/analysis/racing/drivetrain-post-update-verification.json`.
Run verification tools sequentially: the legacy compiler's shared temporary
files can collide when multiple compiler jobs run at once.

Project-specific input, FFB, optional handling and map changes remain outside
these original-source files. See the root
[repository guidelines](../AGENTS.md).

## Engine block now identified

The constructor at `0x5aa5a0..0x5aa6f8` (344 bytes, SHA-256
`88a8d4f8db7d189a64523d8990b1a8b0c44f74143e5089ae3e4fe60381448462`)
installs table `0x7a235c` and copies the `Engine` literal at `0x7a1d20`.
The mover constructor passes car-record `+0x1f0` into this constructor, stores
the resulting Engine at mover `+0x48` and registers its single scalar in the
integration list. Engine `+0x20` receives the DriveTrain pointer after the
drivetrain is constructed. The existing count/state/pre-step callbacks are
shared by Engine and DriveTrain; their identities require constructor evidence.

Engine's derivative callback is **`0x5a1290`**, not the unrelated four-scalar
table candidate `0x598ac0`. It calls `0x594300`, adds Engine `+0x7c`, divides
by spec `+0x0c` with an extra coefficient for drivetrain modes 0/2, and writes
one derivative. The 406-byte `0x594300` uses curve lookup `0x5764d0`, source
parameters, component controls, limiter behavior and optional additional drive.
Engine post-step `0x5aa760` clamps its scalar using spec endpoints, changes a
limiter byte and calls `0x5a0f30` and `0x5a11e0` using substep global `0x7fc800`.
These Engine routines have been traced statically, **not yet reconstructed or
behavior-verified**. They are the next recovery target toward original torque
and drivetrain coupling. A full native source-built playable demo remains incomplete.

## Engine drive-value and adjustment source recovered

The engine routines described above now have a first recovered-source slice in
`Engine.h`, `EngineCurve.cpp`, `EngineDriveValue.cpp`, `EngineDerivative.cpp`
and `EngineAdjustment.cpp`. This supersedes the earlier statement that all
Engine routines are only static leads; the complete post-update is still a lead.
These original-source files contain no project-specific handling or control policy.

| Original routine | Original bytes | Rebuilt bytes | Byte-match status |
| --- | ---: | ---: | --- |
| Curve lookup `0x5764d0` | 138 | 134 | Nonmatching |
| Engine drive value `0x594300` | 406 | 397 | Nonmatching |
| Engine derivative `0x5a1290` | 56 | 56 | Nonmatching |
| Effective control `0x5942e0` | 27 | 27 | Nonmatching |
| Adjustment advance `0x5a11e0` | 172 | 168 | Nonmatching |

Equal length alone is not a byte match. The verifier compares bodies after
explicit original-address relocation and records every fingerprint and relocation.
No inline assembly or fixed registers are used to force matching.

### Curve and record provenance

Curve lookup reads count, start, inverse spacing and sample pointer from a
20-byte original 32-bit curve record. It computes the sample coordinate, uses
the original truncating conversion helper, clamps out-of-range lookup to the
first/last sample, and preserves the original interpolation arithmetic and
integer-boundary adjustment. The record's end field is not read by lookup.

The Engine constructor takes its spec from car-record `+0x1f0`. It establishes
four curve records starting at Engine `+0x24`, with stride `0x14`:

| Engine curve offset | Count | Samples | Start/end source | Inverse spacing |
| --- | ---: | --- | --- | --- |
| `+0x24` | 9 | spec `+0x10` (car `+0x200`) | spec `+0`/`+8` | 8 / (end - start) |
| `+0x38` | 3 | spec `+0x58` (car `+0x248`) | spec `+0`/`+8` | 2 / (end - start) |
| `+0x4c` | 9 | spec `+0x34` (car `+0x224`) | spec `+0`/`+8` | 8 / (end - start) |
| `+0x60` | 9 | global `0x7fbf38` | spec `+0`/`+4` | 8 / (end - start) |

The recovered drive-value routine samples `+0x24`, `+0x38` and, when its
additional-drive index is positive, `+0x60`. It does **not** sample curve
`+0x4c` in this routine. The spec source's loading/upgrading chain and the
additional-drive table's complete valid index range remain to be traced.
Do not substitute the prototype's sinusoidal torque formula or estimated RPM
spacing for these original curves.

### Observed drive computation

- Multiply the original engine scalar by the float at `0x7a1d28`, approximately
  9.549304, before each curve lookup. This constant differs from the rounded
  conversion used by the custom drivetrain prototype.
- Multiply the first curve by Engine `+0x90`, then by the original 1.05 scale
  at `0x7fbff4` when integer `+0x74` is nonzero.
- The effective control is zero when byte `+0x88` is nonzero; otherwise it is
  Engine `+0x80 * +0x78`. This control multiplies the drive contribution.
- A positive index `+0x84` enables the fourth curve, a coefficient from
  `0x7a1798[index]` and the pointed attribute record's field `+0x2ac`.
- Adjustment `+0x8c` above the original threshold at `0x7fbff0` (approximately
  0.6) progressively reduces the accumulated contribution. The adjustment is
  bounded to 1 for this calculation, with scale 0.75 at `0x7a1914`.
- Subtract the second curve times `1 - effective control`. Byte `+0x88`
  multiplies this drag term by 2 at `0x7a18d8`.
- A negative final value, with that byte clear, uses scale 0.65 for selected
  ratio index 1 or 0.85 for other indices (`0x7a1918`/`0x7a191c`).
- The derivative adds Engine `+0x7c` to this drive value and divides by spec
  `+0x0c`. DriveTrain modes 0/2 additionally scale the divisor by `0x7a1920`,
  initially 1. Field names and units beyond observed use remain unconfirmed.

Adjustment advance `0x5a11e0` operates only on `+0x8c` in the original [0, 2)
range, and skips when selected ratio index is 2 and drivetrain `+0x68` is
positive. Comparing the converted scalar against spec `+4` chooses rate
approximately 0.45 or -0.1. It adds rate times supplied duration, bounds below
zero, and writes the original sentinel 2 when reaching approximately 1.2.
These constants come from `0x7a1904..0x7a1910`; this is not a guessed modern
temperature/boost model. Its player-facing meaning and activation writer remain
unresolved.

### Original/rebuilt verification

`tools/verify_pc_engine.py` uses MSVC 7.1 `/Ox /Ob0 /Z7 /EHsc` and passed:

- 780 curve cases: sample counts 1/2/3/9, multiple starts/spacings, endpoint
  clamping and one-ULP neighbors of knots.
- 8,064 drive-value **and** derivative cases, comparing both exact binary32
  outputs, including signed/zero input, flags, additional-drive indices
  -1/0/1/3, adjustment-threshold neighbors, ratio indices 0/1/2, and two x87
  precision modes. Different divisor scales exercise the mode branch despite
  the original default being 1.
- 144 effective-control cases.
- 2,700 adjustment-update cases, including threshold neighbors, suppression,
  positive/zero/negative finite durations and scalar values on either side of
  the spec comparison. Only `+0x8c` may change.

The original conversion helper at `0x75bed4` executes on **both** sides and is
fingerprinted; no callees are stubbed. This runtime helper has not been rebuilt
by these source files. Stack balance, preserved registers, output guards,
unchanged input records and instruction-range guards are checked. The 11,688
cases use synthetic parameters, not imported car-record data or retail driving
traces. These comparisons do not prove a complete playable engine or full
drivetrain/tyre coupling.

```sh
build/venv/bin/python tools/verify_pc_engine.py
```

Report: ignored `build/analysis/racing/engine-verification.json`. Original assets
and executable remain unchanged, and these routines are not linked into OpenUG2.

Next: recover Engine post-step `0x5aa760` and its remaining helper `0x5a0f30`,
then follow torque feedback into tyre/body dynamics and actual car-record loading.
The controller's final callback at `0x5b0520` also performs wheel/contact work;
its chain cannot be omitted when claiming original vehicle handling.

## Engine post-step and feedback chain recovered

`EnginePostUpdate.cpp` reconstructs original `0x5aa760`. It bounds Engine
scalar `+0x14` using spec endpoints times the original scale at `0x7a1d38`,
resets feedback `+0x7c`, and preserves limiter byte `+0x88` hysteresis:
enable above spec `+8` plus offset -25 (`0x7a18dc`), and clear below spec
`+8` minus 0.0125 times that endpoint (`0x7a18e0`). It then calls both
`AdvanceFeedback` and the previously recovered `AdvanceAdjustment`, in original
order, using the same supplied substep global `0x7fc800`.

`EngineFeedback.cpp` reconstructs the complete 675-byte helper `0x5a0f30`.
It preserves its three mode branches and fields `+0x98..0xac`, including a
positive-capacity/zero-duration gate, control threshold, bounded rise, oscillation,
crossing checks, and return to mode zero. Player-facing meaning and activation
writers remain unresolved. These fields must not be assumed to implement a
new turbo, nitrous or clutch design.

Important original details established by differential execution:

- Mode 1 caps its rise at the **positive** baseline from `+0x98 / 0.19`,
  not the negative baseline used in other branches. The original stack argument
  at the minimum call determines this; a misread candidate failed comparison.
- Mode 2 resets on elapsed time **greater than** 0.5, not equal to it. The
  original compares the stored binary32 elapsed time; a candidate using `>=`
  failed on the exact 0.5 boundary.
- Crossing logic uses strict comparisons between the next output, previous
  output and stored anchor. It can reset early when the previous/anchor gap
  is below the original approximately 0.15 threshold.
- The oscillation uses UG2's own angle-quantized polynomial sine and original
  minimum/clamp helpers, not CRT `sin()` or a replacement oscillator.

`EngineMath.cpp` recovers shared original helpers used by this chain:

| Original helper | Original bytes | Rebuilt bytes | Byte comparison |
| --- | ---: | ---: | --- |
| Minimum `0x401070` | 25 | 25 | Identical |
| Clamp `0x401090` | 45 | 45 | Identical |
| Quantized sine `0x43c2b0` | 150 | 164 | Nonmatching |
| Float-angle sine wrapper `0x43c350` | 24 | 24 | Nonmatching |

The fixed-angle helper consumes the low 16 bits of its argument, scales to an
angle, reduces the appropriate quadrants and evaluates its original odd-power
polynomial through power 9, retaining the observed operation order. Original
float coefficients reside at `0x786d88..0x786d94`; angle/quarter-turn constants
and their exact binary32 forms are used through original-address relocations.
The float-angle wrapper multiplies by the original scale at `0x78423c`, invokes
the original conversion helper and forwards the resulting angular bits.

The feedback candidate compiles to 692 bytes versus original 675; post-step
compiles to 176 versus original 196. Both are **nonmatching**. Shared helper
names are recovery labels, not original debug symbols. Original game source and
project changes continue to follow the separate-source policy.

### Verification

`tools/verify_pc_engine_post_update.py` uses MSVC 7.1 `/Ox /Ob0 /Z7 /EHsc`
and runs the real original/rebuilt helper chains without stubs:

- 131,072 fixed-angle sine comparisons: every 16-bit angular input in both
  x87 precision modes `0x027f` and `0x037f`, with exact binary32 outputs.
- 18 signed/zero float-angle comparisons.
- 90 minimum/clamp parameter combinations, including signed zero and inverted
  bounds. Both helper outputs are compared per combination.
- 2,160 complete feedback-state comparisons and 2,160 complete post-update
  comparisons, across modes including out-of-range values, capacity gates,
  controls, limiter state, elapsed times and durations.
- 288 limiter-boundary comparisons, using one-ULP neighbors of lower/upper
  scalar bounds and activation/release thresholds.

The verifier checks all affected component bytes, unchanged fields, stack
balance, preserved registers, untouched output guards for void methods and
unexpected instruction paths. The original CRT conversion helper is still an
explicit unrebuilt runtime dependency. No retail executable or asset is changed.
Inputs are finite synthetic records; construction, actual parameter loading,
the complete vehicle, contact queries and retail driving traces are excluded.
These results are not a playable-demo claim.

```sh
build/venv/bin/python tools/verify_pc_engine_post_update.py
```

Report: ignored `build/analysis/racing/engine-post-update-verification.json`.

## Wheel coupling traced for the next recovery step

For the latest preparation/correction and full coupling recovery, see
[wheel and body verification evidence](pc-wheel-physics.md). The investigation below
records the earlier state of the recovery.

The controller's final callback `0x5b0520` calls mover routine `0x5abdf0`.
The latter is 1,136 bytes, fingerprint
`93627fe9e8572e1972ebad92d88d7b027eb5f414d101e72603f635a388162687`.
It calls drivetrain `0x594cb0`, prepares four wheel components, performs
five correction passes, calls drivetrain `0x594da0` in each pass, invokes
four wheel methods at `0x59d600`, then finishes with drivetrain `0x5aa830`.

The final drivetrain method `0x5aa830` explicitly reads the Engine scalar,
computes a wheel-derived difference and adds a bounded value to Engine `+0x7c`.
This finishing method is another engine-feedback writer to recover; the engine's
`AdvanceFeedback` helper independently uses field `+0x9c` and must not be
mistaken for the complete tyre-to-engine coupling.

The body and wheel methods have only been disassembled/fingerprinted,
not reconstructed or behavior-verified. Drivetrain correction recovery follows below. They depend on contact state, body
matrices, source parameters and more original math helpers. Those requirements
remain intact before a native driving solver can be claimed faithful.


## Drivetrain wheel feedback and distribution recovered

Three additional original PC v1.2 routines are reconstructed in the separate
original-source tree. Offset-based names remain where units are unresolved.

| Routine | Original VA | Original / rebuilt bytes | SHA-256 of original body |
| --- | --- | --- | --- |
| `GetWheelDerivedRate` | `0x594840` | 197 / 197 | `638a2b166683ab9ffb69868ca6221fdf9a448ca0abdaed9ce724dd25cbf7b402` |
| `GetAverageWheelScalar` | `0x5947c0` | 115 / 113 | `eff0ab720277335bce2ddd15bc0aeecc5c82f3cca77fa45ac4508f825ddbe466` |
| `DistributeDriveValue` | `0x594a30` | 636 / 638 | `1f0f6fcab7e4926b21de327954988b64030838e1278e75eec56c7bc518ed8c2d` |

None is byte-identical after relocation. MSVC 7.1 `/Ox /Ob0 /Z7 /EHsc`
reconstructions pass exact original/rebuilt execution comparisons without
stubbed callees. This is bounded behavioral evidence, not full-game equivalence.

`GetWheelDerivedRate` reads each selected wheel's `(value108 - spec14.value10
* value124) / divisor2C`, preserving original arithmetic order. Axle selection,
coefficient application and the dual-axle halving use the same original selector
thresholds as the previously recovered wheel-value consumer. The scalar average
instead uses wheel `+0x16c`, divided by the selected wheel count; its physical
meaning has not been established.

The distribution writer returns without any state changes for ratio index 1.
Otherwise it accumulates the input at drivetrain `+0x70` and subtracts
`max(0.25, auxiliary.value3E4 * 0.25) * input` from Engine `+0x7c`.
Original execution caught an inverted comparison during reconstruction: **0.25
is a floor, not a cap**. It then distributes the ratio-scaled input using
coefficient `+0xec` and the axle coefficients. Front weighting uses wheel 1
in its speed ratio; rear weighting uses wheel 2. Preserve this asymmetry and
the original threshold branches rather than substituting a new differential.
The two wheel `+0x11c` bytes select global pair threshold `0x7fbffc` (initially
about 0.35) or 0.5. Nonzero `+0x10c` suppresses that wheel's `+0x108` update.
The small-sum threshold is the original binary32 at `0x7845a0`, about 0.00001.

The wheel feedback verifier passes **4,224 cases per routine**, including
one-ULP selector boundaries, eight ratio indices, signed and zero values and
two x87 precision modes. It checks the exact binary32 return, the entire input
memory remaining unchanged, guarded output, stack and preserved registers.
The distribution verifier passes **49,152 cases** across all combinations of
four auxiliary selector values, eight ratio indices, sixteen contact-byte
patterns and sixteen wheel-block patterns at axle fractions 0, 0.5 and 1,
with two x87 precision modes. It compares the entire mapped state, constrains
writes to the six observed float fields and checks the neutral no-op and ABI.
Finite synthetic inputs exclude invalid pointers and zero divisors.
After expanding the partial layouts, the earlier drivetrain consumers, composed
integrator and post-update verifiers also pass all 7,752 regression cases.

```sh
build/venv/bin/python tools/verify_pc_drivetrain_wheel_feedback.py
build/venv/bin/python tools/verify_pc_drivetrain_distribution.py
```

Reports are ignored `build/analysis/racing/drivetrain-wheel-feedback-verification.json`
and `drivetrain-distribution-verification.json`. Original executable and assets
remain unchanged. Project modifications and the driving prototype are untouched.

Next recover `0x594cb0` initial drive application and `0x594da0` correction,
composing these verified wheel routines with the recovered Engine chain.
Wheel/body correction, source parameter loading and complete contact support
remain necessary before integrating an original source-built driving solver.


## Initial application and iterative drivetrain correction recovered

`DrivetrainCoupling.cpp` reconstructs two more PC v1.2 bodies:

| Routine | Original VA | Original / rebuilt bytes | Original SHA-256 |
| --- | --- | --- | --- |
| `ApplyInitialDriveValue` | `0x594cb0` | 233 / 237 | `5176544eb8374c2a0726baf15380c039481123737b3a21cc184339a4a24fc8ed` |
| `ApplyDriveCorrection` | `0x594da0` | 225 / 227 | `d663fcb5e9f686e221bc1bcae5d81bb9930e4f7d874bc9780dd21818d0c33a89` |

Neither body byte-matches. The initial method always clears drivetrain `+0x70`.
Mode 3 distributes the original engine drive value. Modes 0 and 2 clamp the
Engine scalar minus stored wheel value to +/- global `0x7fbf34` (initially 300).
The coefficient at spec `+0x14` is a float, correcting the previously incomplete
layout. An opposite-sign crossing, with both converted deltas above global
`0x78a514` (2000), scales it by `0x7a1924` (0.5). It stores the new converted
delta at `+0x58`, then distributes coefficient * drivetrain scalar * bounded
difference, preserving original multiplication order. Other modes only clear
the accumulator. These observations do not yet establish field units or names.

Correction is a no-op outside mode 3. It subtracts the recovered wheel-derived
rate from the Engine rate, with the Engine result stored as binary32 first.
The denominator starts at reciprocal Engine spec `+0x0c` and adds the selected
axles' ratio-squared contributions weighted by `+0xec`, wheel divisor `+0x2c`
and one half. It distributes the resulting correction using the recovered
writer. The original directly repeats the engine-rate expression here; source
preserves this rather than inserting a new controller or optional handling rule.

`tools/verify_pc_drivetrain_coupling.py` compiles and links these methods with
recovered Engine drive/curve, wheel-rate and distribution bodies. No gameplay
callee is stubbed. It fingerprints all original bodies and the original float
conversion helper `0x75bed4`, which remains an explicit unrebuilt runtime
dependency. There are 4,608 exact whole-state comparisons each for initial
application, one correction, and initial application followed by five corrections:
**13,824 comparisons total**. Modes 0..3, all eight ratio indices, both x87
precision modes, one-ULP axle-selector boundaries, opposite-sign delta crossings,
limiter states, engine curves and individual wheel flags are exercised using
finite synthetic records. The checks constrain writes to seven observed floats,
verify inactive correction is a no-op, and verify stack/preserved registers.

The five-pass comparison is a drivetrain-only composition: it deliberately
omits the intervening original wheel/body correction updates. It is **not** a
verified full mover or retail driving trace. Actual car-parameter loading,
wheel synchronization `0x5a12d0`, finishing `0x5aa830`, road contacts and body
forces still need recovery before integration into a source-built native demo.

```sh
build/venv/bin/python tools/verify_pc_drivetrain_coupling.py
```

Report: ignored `build/analysis/racing/drivetrain-coupling-verification.json`.
Original source remains independently verifiable and separate from all project
modifications. Retail inputs and the current driving prototype are unchanged.
The Engine consumer and drivetrain post-update regression verifiers pass all
14,064 comparisons after the layout changes.


Finishing dependency lead: `0x40c840..0x40c903` (195 bytes, SHA-256
`83eec8b8e85ad52c533955f72f37c36f636236bf99e60dbb306626faa1f2cc8b`)
reads a pointer/count record and 8-byte coordinate/value pairs, clamps endpoints,
searches an interval, stores its interpolation fraction as binary32, then calls
`0x576740`. Empty records leave the output untouched. This is still static
analysis, not reconstructed or behavior-verified. It is required by the
ratio-index-2 branch of drivetrain finishing; do not stub that branch to claim
full recovery. Wheel synchronization compares auxiliary `+0x3f0` times the
float at `0x78426c` (about 2.237) with `0x7864d8` (40), then averages each
pair when the comparison or two zero contact bytes require it. Units remain
unconfirmed.


## Wheel synchronization and drivetrain finishing recovered

The remaining traced drivetrain coupling methods now have original-source
reconstructions, separate from custom gameplay and native engine modifications.

| Routine | Original VA | Original / rebuilt bytes | Original SHA-256 |
| --- | --- | --- | --- |
| `SynchronizeWheelValues` | `0x5a12d0` | 303 / 309 | `3583f930b5d94bd94bdce67908b177f4e33f556f88b7d4ea42eeff2bc82a5f16` |
| `FinishDriveCoupling` | `0x5aa830` | 294 / 272 | `b95888e596b20554280a11f28796b4d7510836e4c1b33ab6b5787c5c45821340` |
| `DriveFeedbackCurve::Evaluate` | `0x40c840` | 195 / 185 | `83eec8b8e85ad52c533955f72f37c36f636236bf99e60dbb306626faa1f2cc8b` |
| `InterpolateDriveFeedback` | `0x576740` | 37 / 37 | `ba1420cd04c9483da32a1602ba546cb0cd2041304a0cb08befd5e652f2e4b545` |

The interpolation helper **byte-matches after relocation**; the other three
bodies remain nonmatching. Its weighted sum is `(1 - fraction) * previous +
fraction * next`, not a substituted subtraction-based lerp. The table reader
preserves empty-table output, endpoint clamping, interval selection and the
original binary32 fraction passed across a real call boundary.

Synchronization computes the input minus the original wheel-derived value once.
It updates the selected axle(s) by this difference divided by their ratio and
coefficient. It averages a wheel pair first if auxiliary `+0x3f0` times the
original scale is strictly below the threshold, or both pair contact bytes are
zero. The front axle resets this condition independently; the rear contact
condition must not leak into front averaging. Physical units are unresolved.

The finishing method preserves all traced branches: active ratio/mode/contact
conditions, original input and wheel-scalar thresholds, synchronization, bounded
Engine feedback, and the ratio-index-2 pair-curve adjustment. Engine drive/curve,
wheel value/scalar, synchronization, clamp and pair interpolation all execute
recovered compiled source in the rebuilt comparison. There are no gameplay
callee stubs. The original float conversion helper remains the explicit
unrebuilt runtime dependency inherited from Engine curve lookup.

`verify_pc_drivetrain_synchronization.py` passes **12,096** exact whole-state
comparisons across one-ULP selector/synchronization boundaries, seven nonzero
ratio indices, sixteen contact patterns and two x87 precision modes. Neutral
zero-ratio division is explicitly excluded. Writes are restricted to the four
wheel scalars, and stack/preserved registers are checked.

`verify_pc_drive_feedback_curve.py` passes **2,784 table cases** and **2,048
direct interpolation cases** with output guards, read-only input, empty-table
preservation and ABI checks. Coverage is finite inputs, negative/zero/single and
multiple counts, sorted distinct coordinates, endpoints and one-ULP knot
neighbors. The generic table's semantics for unordered/duplicate/NaN inputs
have not been established by these tests.

The expanded coupling verifier passes **23,040** whole-state comparisons: 4,608
each for initial application, correction, finishing, initial plus five
corrections, and initial plus five corrections plus finishing. It restricts
writes to twelve observed floats and verifies stack/register preservation.
The original global finishing table and coefficient array remain mapped as
retail data in this analysis; native parameter import is still outstanding.
The composed five-pass tests exclude intervening wheel/body updates, so these
are drivetrain-only comparisons, not a complete vehicle or playable-demo proof.

```sh
build/venv/bin/python tools/verify_pc_drivetrain_synchronization.py
build/venv/bin/python tools/verify_pc_drive_feedback_curve.py
build/venv/bin/python tools/verify_pc_drivetrain_coupling.py
```

Reports: ignored `build/analysis/racing/drivetrain-synchronization-verification.json`,
`drive-feedback-curve-verification.json` and `drivetrain-coupling-verification.json`.
The prototype, retail executable and retail assets are unchanged. Next recover
the wheel correction `0x59d600` and its helpers, then compose the mover's actual
five passes including body matrices and wheel updates. Source parameter loading
and road/contact construction also remain before faithful native integration.

The final composed run additionally records original finishing branch calls:
812 synchronization, 396 bounded feedback and
256 ratio-two curve calls. All three branches are exercised;
none is replaced by a fixture.
