# NFSU2 RE

Independent source reconstruction and verification project for Need for Speed Underground 2.
Created 2026-10-11 with new Git history.

Contains our recovered UG2 PC v1.2 C++ and its analysis/verification tools.
The objective is complete game decompilation into verified, reproducibly
buildable source. A playable demo is an intermediate milestone. See
[the decompilation scope](docs/DECOMPILATION_SCOPE.md) for completion evidence.
Original code stays here; combined-game and platform changes belong in the
separate application repositories. Existing research was copied without editing
recovered algorithms. PROVENANCE.json records migration input fingerprints;
the historical upstream baseline did not contain these local recovered files.

Recovery is incomplete. Some functions byte-match; others have bounded original /
rebuilt execution comparisons and remain nonmatching. Partial 32-bit layouts and
an unrebuilt conversion helper in emulation prevent claiming a full native game.
See docs/pc-vehicle-physics.md and docs/pc-racing.md for evidence and limitations.
Do not infer verification status merely because a source file is present.

## Native driving demo

A 32-bit Linux build drives a stock UG2 car on the L4RA streets using the recovered
vehicle and track-collision sources; window, input, placeholder drawing and a few
documented seams are platform scaffolding. Behavioural verification of the routines
recovered for the demo is still pending. See [the demo notes](docs/NATIVE_DEMO.md).

```sh
cmake -S . -B build/native && cmake --build build/native
build/native/ug2demo --car SKYLINE   # W/S throttle/brake (hold S at rest to reverse), A/D, Space, R, Esc
```

## Local verification

Install requirements-pc-analysis.txt into a Python virtual environment. Tools
expect an ignored build directory containing tools/wibo, the Win32/7.1 compiler
and, for string verification, delink and objdiff-cli. The migrated workspace
reuses existing tools through ignored local links; none is redistributed here.
Set UG2_EXE to your own PC v1.2 speed2.exe. The installation in the parent workspace games/ folder is
the local default. Compiler jobs must run sequentially because the old compiler
and wibo share temporary filenames.

Example: python tools/verify_pc_wheel_math.py. This executes isolated original /
rebuilt instructions, not the retail game. Native integration is still pending.

No game assets, retail executables, compiler binaries, generated disassembly are included. Existing project scaffolding informed the research;
retain attribution for imported tooling. Original source provenance is not a
claim to relicense EA assets or recovered game code.
