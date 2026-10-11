# PC wheel and body verification evidence

This record preserves isolated original/rebuilt comparisons from the recovery
sequence. Later sections supersede earlier recovery leads. Statements about
unrecovered construction or the absence of a native demo describe that stage,
not current repository status. See [the native demo guide](NATIVE_DEMO.md) for
integration status; these comparisons do not prove native equivalence.

## Vector math and correction scaling

Wheel identity, transpose, transform, scale-add, normalization and cross product
pass 12,288 original/rebuilt whole-state comparisons, including aliases and
partial overlaps in two x87 precision modes. Scale-add (51 bytes) and cross
product (61 bytes) byte-match; the four other bodies remain nonmatching.
The conditional correction-scale routine at 0x59d490 passes 3,240 exact return
comparisons; its 128-byte rebuilt body remains nonmatching. These are finite
synthetic inputs, not a full wheel correction or driving trace.

The last pre-migration expanded drivetrain regression passed 23,040 comparisons.
The wheel force correction at 0x59d600 is now reconstructed with its real
helper chain (see below). Source attribute loading, contact generation, full
wheel update and the vehicle mover still require recovery before
native driving integration. No application gameplay was changed.


## Quantized angle

WheelAngle at 0x43c570 (218 original / 224 rebuilt
bytes) passes 16,432 exact low-16-bit comparisons across all quadrants, signed
zero/axes, lookup-bin neighbors and varied finite magnitudes, in two x87 precision
modes. The compiled body remains nonmatching. It preserves UG2's byte-table
quantization, negative ratio scale and quadrant arithmetic, rather than replacing
it with atan2. The original conversion helper still executes on both sides;
lookup data remains a fingerprinted mapped retail input, not a committed asset.

The caller reads AX only; original high EAX bits vary and are not the return
contract. Original code may overwrite its first cdecl argument while taking its
absolute value, so caller argument slots are checked as scratch, not read-only.
The verifier uses correctly relocated relative calls at its actual execution VA.
Commands: python tools/verify_pc_wheel_angle.py. Report is in ignored
build/analysis/racing/wheel-angle-verification.json. Full wheel preparation/update, vehicle body correction
and native runtime adaptation remain outstanding.


## Wheel force correction

`DrivetrainWheel::CorrectForces` at 0x59d600
(1,258 original / 1,195 rebuilt bytes) passes 7,504 exact full-state comparisons
in 24-bit and 53-bit x87 precision. The machine code remains nonmatching.
The verifier links rebuilt correction scaling, matrix/vector helpers, quantized
angle and sine routines; no gameplay callees are stubbed. Both paths still
execute the original 117-byte conversion runtime, and read mapped retail angle
and surface data. This is isolated emulation evidence, not native execution.

Observed behavior preserves conditional drive-force scaling and its undo path,
force-length limiting using the source's coefficient product, saturation ratio,
contact/flag overrides, motion-angle correction and all four selectable force
bases plus the identity default. Original angle calculation uses transformed
X/Y, and both its sign adjustment and the absolute-speed threshold read X.
The shifted sine call is evaluated and discarded as in the original.
The method consumes one unused stack argument and preserves callee-saved
registers. Only wheel offsets 0x124, 0x128, 0x130/134/138 and 0x170 may change.

Tests include deterministic and randomized finite states, zero forces and
limits, negative motion, translated/non-orthogonal matrices, flags, threshold
neighbors around 0.001 and 1.0, and exactly degenerate projection in modes 2/3.
Every identified original branch is visited. Partial 32-bit layouts now expose
these fields without changing previously recovered offsets. Attribute/surface
field names remain based on observed offsets; units and higher-level semantics
are not established merely by these tests.

Command: `python tools/verify_pc_wheel_force_correction.py`.
Report: ignored `build/analysis/racing/wheel-force-correction-verification.json`.
Wheel preparation at 0x5a8280 and its two missing helpers are now recovered
(see below). Full mover 0x5abdf0 and collision/contact producers remain
unrecovered.


## Wheel preparation

`PrepareForces` at 0x5a8280 (1,078 original / 1,085 rebuilt
bytes) passes 4,576 exact whole-state comparisons. Preparation followed by force
correction passes another 4,576 comparisons. Helpers `UpdateLockState` at
0x59d400 (133 original / 142 rebuilt bytes) and `EvaluateLateralForce` at
0x59d510 (225 original / 233 rebuilt bytes) each pass 4,608 comparisons.
All three reconstructed bodies remain nonmatching. Both requested x87 control
words are exercised; these are emulation comparisons, not native hardware tests.

Preparation retains the original two coefficient contributions to drive state,
local motion transformation and quantized angle, slip calculations, conditional
negative-drive adjustment, lock-state update, slip/fallback force branches and
low-speed rescaling. The negative-drive adjustment requires the second
contribution to equal zero. Original 0x5a82d3 stores binary32 and 0x5a82f7 reloads
it between additions; a local volatile reference preserves this rounding boundary
rather than retaining the compiler's higher-precision intermediate.

Lock-state update compares its coefficient sum against a velocity/input term;
it may zero the wheel value and set or clear the lock flag. Lateral evaluation
retains both the fallback curve and the indexed pair interpolation / last-curve
paths. Source offsets and units remain provisional. The caller supplies
nonnegative lateral angles; negative indices are not silently clamped.

The preparation verifier visits every identified caller branch and links the
real reconstructed helpers, curve lookup and existing angle/matrix routines.
No gameplay callees are stubbed. The original conversion runtime and mapped
lookup data are still analysis dependencies. Composing preparation with correction
omits intervening body/contact updates and does not prove the full vehicle mover.
Only the observed wheel fields may change; full state, stack, preserved registers
and FPU control word are checked.

Commands: `python tools/verify_pc_wheel_preparation_helpers.py` and
`python tools/verify_pc_wheel_preparation.py`. Reports are ignored
`build/analysis/racing/wheel-preparation-helpers-verification.json` and
`build/analysis/racing/wheel-preparation-verification.json`.

Static inspection of the 1,136-byte mover at 0x5abdf0 (SHA-256
93627fe9e8572e1972ebad92d88d7b027eb5f414d101e72603f635a388162687) finds direct
calls to recovered initial drive application, preparation, matrix helpers,
iterative drive correction, wheel force correction and drivetrain finishing.
The mover's own body arithmetic, state layout and actual parameter/contact loading
remain to be recovered and verified. Direct-call coverage is not proof that its
whole transitive dependency graph or the playable demo is complete.


## Mover coupling

`VehicleMover::CorrectWheelCoupling` reconstructs 0x5abdf0
(1,136 original / 625 rebuilt bytes) and passes 3,072 exact full-state
comparisons with the recovered real drivetrain, wheel and math callees.
It remains nonmatching. Inputs cover both selected body matrices, signed
mode values, each wheel's adjustment/skip behavior, rotated frames, varied
wheel radii/inertias and non-axis-aligned attachment arms.

The actual original update order is now composed: initial drivetrain drive,
four preparations, five passes of body/wheel response and drivetrain correction,
four wheel force corrections, and drivetrain finishing. Branch counters verify
12,288 preparations, 15,360 correction passes and drivetrain corrections,
12,288 force corrections and 3,072 finishing calls. Whole state, the observed
write footprint, stack, preserved registers and both FPU control words are checked.
Gameplay callees are not stubbed; the original conversion runtime and mapped
retail data remain analysis dependencies.

First-wheel force components and accumulated X/Y sums preserve the original
binary32 storage boundaries. Other force/torque terms retain wider intermediates;
angular and linear response values are rounded at their observed stores. Fields
and layouts remain partial and offset-based, rather than inferred SI definitions.
The unused force-correction argument receives the original raw loop-index bits.

Command: `python tools/verify_pc_vehicle_mover.py`.
Report: ignored `build/analysis/racing/vehicle-mover-verification.json`.
This method corrects wheel coupling; it does not integrate the body's pose or
produce road contacts. The enclosing callback at 0x5b0520 still has unrecovered
pre/post-coupling work, including body-force application and contact updates.
For example, its 188-byte post-coupling method at 0x5a5b60 reads body vector
0xc0, conditionally evaluates a curve and applies a scaled opposing body force.
That method and its real callees are now recovered (see below).
Actual car-parameter loading and a source-built runnable demo remain incomplete.


## Post-coupling body force

`ApplyOpposingBodyForce` at 0x5a5b60 (188 original /
191 rebuilt bytes) passes 3,072 exact full-state comparisons. It retains the
original state-1/state-2 exclusions, signed mode gate, vector length addition
order, below-threshold test, curve lookup and coefficient-scaled force addition.
Threshold neighbors, exact threshold, zero and multi-component motion are tested.
Body fields 0xc0 and 0x120 are exposed without moving existing offsets; their
higher-level units and complete state lifecycle remain unresolved.

Its pair lookup at 0x576790 (190 original / 192 rebuilt bytes) passes 5,266 exact
float-return comparisons. Endpoints, single/nonpositive counts with a valid data
pointer, narrow intervals and the epsilon midpoint path are preserved. This
returning lookup differs from the older output-parameter `Evaluate`: count <= 0
still reads the first value rather than leaving an output untouched. It does not
silently sanitize the original table contract.

Vector scaling at 0x4011f0 (39 bytes) and body force accumulation at 0x59bc20
(53 bytes) both byte-match and each pass 4,096 exact whole-state comparisons,
including input/output aliases and partial overlap. Both capture Y/Z before X
writes, preserving original behavior for overlapping memory. All four routines
pass 16,530 comparisons in total. Larger bodies remain nonmatching. Stack,
preserved registers, control words, write footprint and output guards are checked.

Command: `python tools/verify_pc_body_force.py`. Report: ignored
`build/analysis/racing/body-force-verification.json`. There are no stubbed callees
or borrowed executable-code dependencies on this rebuilt path; original retail
instructions run only on the reference side of the isolated comparison. Synthetic
pair tables provide the initialized curve at 0x86b4a0. Its retail initialization
and actual car parameters still need recovery. This body-force method does not
advance position/orientation or create road contacts; the playable source-built
demo remains incomplete.


## Body derivative

`MoverBody::WriteDerivative` at 0x59c1e0 (1,033
original / 1,021 rebuilt bytes) passes 2,048 exact whole-state/output comparisons.
The 186-byte matrix derivative at 0x59c120 passes 4,096 comparisons including
output aliases with its matrix/vector inputs and partial overlap. Neither body
byte-matches, despite equal matrix-writer sizes. No gameplay callees are stubbed;
rebuilt matrix/vector math and EngineCurve lookup run on the candidate side.
The original conversion runtime is still an analysis dependency of curve lookup.

The callback retains its conditional tilt/threshold adjustment to body vector
0x110, scalar rise/fall logic, curve and state damping, and final Z-limit checks.
The limit branch compares the original accumulator at 0x128, not the already
modified output. Original damping Y/Z products cross explicit binary32 stores;
the X products remain wider until addition to the output. Every identified
conditional branch is visited under two FPU control words.

Output slots 3, 23, 27 and 29..31 are untouched, as in the executable. They are
not silently filled with zero. The matrix writer also retains its observed
bottom-right value of 1.0. Its final component is evaluated before later zero
writes, which matters when output overlaps input. This preserves observed
behavior rather than substituting a textbook rotation derivative.

Ownership is now corroborated beyond a table-shape guess: constructor 0x5a7f30
(373 bytes, SHA-256 fc322b2face590dcad63214d5c2eb56610040cd42d72dd214c3de59256021006)
installs vtable 0x7a20c4. Parent constructor 0x5b5cc0 calls it at 0x5b5d6d with
car record +0x110, stores its result at mover +0x20, appends its +4 link to the
integration list and adds its scalar count. Slot 0x04 returns 32 at 0x5a80b0;
slot 0x0c returns the state region +0x20 at 0x5a80c0; slot 0x18 is the recovered
derivative. Slot 0x20 at 0x5a80d0 returns a static pointer; the earlier generic
name `BeforeIntegrate` does not imply that this body's callback mutates state.

Command: `python tools/verify_pc_body_derivative.py`.
Report: ignored `build/analysis/racing/body-derivative-verification.json`.
The body post-update at 0x59c5f0 is now recovered (see below). Constructor
parameter refresh at 0x59b940 reads record +0 and matrices starting at +0x10;
its matrix inversion callees and actual retail parameter imports remain pending.
Original body/integrator composition and a playable native runtime are not yet
verified. These finite comparisons do not establish completion of the demo.


## Body post-update

`MoverBody::PostUpdate` at 0x59c5f0 (903 original / 795
rebuilt bytes) passes 2,048 exact whole-state comparisons. Its point-force
helper 0x59bb00 (274/308) passes 4,096, including force/point/accumulator
aliases. Matrix copy 0x4012b0, product 0x4013f0 and the multiply wrapper
0x43c990 each pass 3,072 comparisons with partial output overlaps. None is
counted as byte-matching. No callees are stubbed and no original runtime helper
executes on the rebuilt side.

The update derives vector 0xa0 from 0x70 times scalar 0x168, limits its length
only when divisor 0x164 exceeds the original binary64 threshold at 0x7a1e88,
derives 0xb0 from 0x110, transforms 0x80 through the selected inertia matrix
(0x170 or 0x1b0 by flag 0x158) conjugated by orientation 0x30, limits the result
at 0xc0, clamps scalar 0x90 at zero, writes the pose matrix 0xd0 from 0x30 and
position 0x20, saves and clears accumulators 0x110/0x120, then applies a
downward point force of 0x164 * 0x160 at the body origin when 0x160 is positive.
The stored length crosses a binary32 boundary before the speed-limit division.
Point-force caps use original limits at 0x7a1ad8 and 0x7a1adc.

Command: `python tools/verify_pc_body_post_update.py`. Report: ignored
`build/analysis/racing/body-post-update-verification.json`. Body, force and
mover regressions also pass after the layout extension. Body construction and
parameter refresh 0x59b940, actual retail record loading and the native runtime
remain unrecovered.
