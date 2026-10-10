# Latest wheel recovery — 2026-10-11

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
wheel preparation/update and the vehicle mover still require recovery before
native driving integration. No application gameplay was changed.


Quantized angle recovery: WheelAngle at 0x43c570 (218 original / 224 rebuilt
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


Full wheel force correction: `DrivetrainWheel::CorrectForces` at 0x59d600
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
The next dependency on the original mover path is wheel preparation at 0x5a8280;
full mover 0x5abdf0 and collision/contact producers are still unrecovered.
