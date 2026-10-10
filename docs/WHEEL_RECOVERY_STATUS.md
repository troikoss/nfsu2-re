# Latest wheel recovery — 2026-10-11

Wheel identity, transpose, transform, scale-add, normalization and cross product
pass 12,288 original/rebuilt whole-state comparisons, including aliases and
partial overlaps in two x87 precision modes. Scale-add (51 bytes) and cross
product (61 bytes) byte-match; the four other bodies remain nonmatching.
The conditional correction-scale routine at 0x59d490 passes 3,240 exact return
comparisons; its 128-byte rebuilt body remains nonmatching. These are finite
synthetic inputs, not a full wheel correction or driving trace.

The last pre-migration expanded drivetrain regression passed 23,040 comparisons.
The full wheel correction at 0x59d600 still requires its quantized angle helper,
source attribute records, contact state and body transforms to be reconstructed
and composed before native integration. No project gameplay was changed.


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
build/analysis/racing/wheel-angle-verification.json. Full wheel/body correction
and native runtime adaptation remain outstanding.
