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
