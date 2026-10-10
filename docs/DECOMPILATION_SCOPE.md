# Underground 2 decompilation: basic playable demo

The objective was revised on 2026-10-11: decompile UG2 until a really basic demo
is playable. This supersedes the previous immediate requirement to decompile the
whole game. The reference remains the locally supplied PC v1.2 executable.

Prioritize recovery of the original systems needed to load an original car and
road, initialize vehicle state, accept controls, simulate driving and contact,
and render a usable demonstration. Recover original mechanics rather than
inventing replacements. Platform and presentation scaffolding must stay clearly
distinguished from original recovered routines.

The demo milestone requires:

- Reproducible source builds and a locally runnable demonstration.
- Locally supplied UG2 vehicle/world assets; no redistributed retail files.
- A car that accelerates, steers and brakes under player input, with basic road
  contact/collision and a way to recover after leaving the usable area.
- Verification against original routines for the recovered driving chain,
  including actual caller composition and necessary parameter/state loading.
- A usable rendered view and a repeatable manual driving check.
- No retail executable acting as the runtime game implementation. Isolated
  emulation comparisons and an existing unrelated prototype cannot establish
  completion of the source-built demo.

Record provenance, unknown layouts, external runtime dependencies and limitations.
Distinguish byte matching from bounded behavioral evidence. Retail assets,
executables, compilers and generated disassembly remain local. Current verification
still depends on an original conversion helper and mapped retail data; these
analysis dependencies are not a completed native runtime.

Full career, menus, AI, customization and executable-wide decompilation coverage
are beyond this immediate milestone. Do not claim complete game decompilation
when the basic demo becomes playable.
