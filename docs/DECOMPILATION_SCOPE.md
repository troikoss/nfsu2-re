# Complete Underground 2 decompilation

The objective, set on 2026-10-11, is to completely decompile Underground 2.
The current reference is the locally supplied PC v1.2 executable. Recovery of
isolated functions is progress toward this objective, not its completion.

Cover the whole game: startup and platform integration, memory and resource
management, asset loading, rendering, audio, input, vehicle physics, world and
collision, AI and traffic, races and scoring, career and economy, customization,
menus and UI, saves, and any remaining executable subsystems. Use a function and
data inventory to identify coverage gaps rather than assuming this list captures
every dependency.

Preserve original behavior, call boundaries and arithmetic. Record provenance,
unknown layouts, external runtime dependencies and verification limitations.
Label byte matches separately from bounded behavioral comparisons. Do not replace
unrecovered mechanics with invented algorithms or conceal unknowns behind stubs.

Completion requires:

- An executable-wide function/data inventory with unresolved code and dependencies
  accounted for, including code outside the current physics research.
- Recovered source for the original game and reproducible build instructions.
- Verification evidence for recovered subsystems against the reference game,
  distinguishing exact matches, behavioral evidence and remaining differences.
- A game built from recovered source that loads locally supplied original assets
  and exercises the original game flows, including driving, races, career,
  customization, menus, audio and persistence.
- No unexplained missing code, placeholder gameplay implementations or runtime
  reliance on the retail executable to perform unrecovered game functions.

Retail assets, executables, compilers and generated disassembly remain local.
Current verification still depends on an original conversion helper and mapped
retail data; the existing recovered routines do not establish a complete build.
