# Underground 2 decompilation: complete game recovery

The objective was revised on 2026-10-11: completely decompile Underground 2
PC v1.2 into verified, reproducibly buildable source. A basic playable demo
remains an intermediate milestone; it does not complete this objective.

Recover original code and mechanics from the locally supplied reference
executable. Preserve evidence, call boundaries and arithmetic order. Keep
unknown layouts explicit and distinguish platform scaffolding from recovered
game routines. Do not substitute invented gameplay algorithms for original
behavior.

Prioritize the driving and integration chain already under investigation, then
extend recovery across the whole game: asset loading, rendering, audio, input,
world simulation, collision, AI, racing, career, police, customization, menus,
and persistence. Maintain a coverage inventory with unresolved routines,
layouts, dependencies and verification limits; source presence alone is not
evidence of completion.

Complete decompilation requires:

- Accounted-for executable code and data, with remaining unknowns resolved or
  explicitly justified as external compiler/platform runtime dependencies.
- Reproducible source builds of the game implementation, independent of a
  retail executable as the runtime implementation.
- Original UG2 assets loaded from the player's local installation, without
  redistributing retail files.
- Verification of recovered routines and their actual caller composition,
  parameter loading, state transitions and subsystem integration.
- A playable rebuilt game covering the original gameplay systems, with
  repeatable end-to-end checks and documented limitations.
- Byte-matching status reported separately from bounded behavioral evidence;
  neither isolated emulation tests nor a playable driving demo establish
  executable-wide completion.

Record provenance and verification evidence as recovery proceeds. Retail
assets, executables, compilers and generated disassembly remain local. Current
verification still depends on an original conversion helper and mapped retail
data; these analysis dependencies are not a completed native runtime.

Work on the separate application project is deferred. This repository's
objective is original UG2 recovery.
