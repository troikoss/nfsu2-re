# Session handoff — 2026-10-11

## Goal and standing instructions

Audit the native UG2 demo and replace every replaceable gameplay substitute
with original UG2 PC v1.2 code recovered from the supplied executable. Continue
through steering, vehicle/session construction, update timing, reset/camera,
HUD/frontend, and asset/world/presentation logic. Preserve necessary Linux
backends, verify recovered routines and integration, and document unresolved
dependencies and unavoidable platform seams.

The larger repository objective is complete, verified, reproducibly buildable
UG2 source; a playable demo is intermediate. Completion evidence is defined in
`docs/DECOMPILATION_SCOPE.md`. Do not redefine completion around isolated tests.

The user requires verified changes to be committed and pushed between pauses.
Push authorization persists for `main` at
`https://github.com/troikoss/nfsu2-re.git`; do not ask again. Do not commit retail
assets, executables, compiler binaries, or generated disassembly. Keep unverified
work clearly identified rather than describing it as a verified checkpoint.

At handoff creation, the goal tool reports **paused**. This handoff request does
not resume it. Resume recovery when the user resumes the goal.

Follow `AGENTS.md`: always recover original code, never invent replacement game
algorithms. Preserve arithmetic order, call boundaries, addresses and x86
layouts; keep unknown fields offset-based. Report behavioral equivalence
separately from byte matching. Native code uses C++11; recovered source must
also compile with MSVC 7.1. Run MSVC/wibo jobs sequentially. Do not spawn agents
unless the user or applicable repository/skill instructions request delegation.

## Authoritative checkpoint

Workspace: `/home/dq/Documents/Developement/nfsu2decomp/nfsu2-re`.
Branch: `main`. Last verified recovery commit, already pushed:
`b864ead61feaef5cebd85c7830dbdbc9a48eceed` —
**Recover original extra adjustments and tuning eligibility**.
Local `origin/main` agrees with this recovery checkpoint at handoff creation.

This checkpoint recovers:

- Extra adjustment/application `0x5997e0` and removal `0x599d90`.
- Tuning entry lookup `0x59ae40` and active count `0x59b0e0`.
- Integer flag layout at selection +0x28, signed tuning kind/priority bytes,
  first-entry priority ties, and original gear eligibility limits.
- Native stock construction now runs original record builder `0x5ac4f0`
  followed by extra application, instead of the former raw stock-record copy.

Validation: 33,856 bit-identical comparisons for these four routines; 3,680
record upgrade/builder comparisons; 46 native stock records/selections match
original construction plus extra application; native build and forward/reverse
smoke checks pass (114.6/76.8 km/h). Parameter and constructor/selection checks
also pass. Byte matching is not established. These are bounded routine and
integration checks, not proof of complete parameter or game equivalence.

Earlier verified recovery includes parameter/tuning constructors, upgrade
selection/fractions/search, nine record modifiers, drive curve and speed helper,
refresh wrapper, mode variants, pedals, gear accessors, forward selection, and
kinematics prefixes. Some recovered routines still await complete caller
integration. See the subsystem evidence rather than assuming full coverage.

## Tuning value helpers (verified after the interruption)

The interrupted draft is now verified and committed: offset query `0x59aee0`,
field-address switch `0x59b200` (thiscall member ignoring `ecx`), engine
normalization `0x59b3c0` and value reader `0x5a7da0` in
`PhysicsTuningValues.cpp`. `tools/verify_pc_physics_tuning_values.py` reports
94,722 bit-identical comparisons; see `docs/pc-physics-parameters.md`. Native
CarTypeInfo table installation is still absent, so they are not yet called
natively. Verifier note: Unicorn keeps translated blocks, so never rewrite a
probe wrapper at the same address for a different target; also reset FPTAG.

## Immediate plan on resume

1. Reinspect Git status and the goal state.
2. Full tuning application `0x5b4780` and its whole callee tree are recovered
   and verified (`PhysicsTuningValues.cpp`, `tools/verify_pc_physics_tuning_values.py`,
   ~7.5 min). Native CarTypeInfo/tuning-table installation is still absent,
   so none of it runs natively yet.
3. Recover enclosing parameter composer `0x5b4dc0`, class-index calculation,
   performance helper `0x5b06a0`, real descriptor installation and source table
   lifecycle. Integrate its original call chain and remove native manual
   composition as evidence permits; do not force empty tables to evade callees.
4. Continue human steering (`0x405dc0`, `0x41dfc0`, keyboard `0x416bb0`, analog
   `0x4061c0`, alternate `0x406a40`, device capture `0x571a60`, constructors).
   The native smoothing at 2.5 units/s and ±30° angle scaling remain substitutes.
5. Follow the audit inventory through vehicle/session construction, complete
   update/timing callers, spawn/reset/camera, HUD/frontend, and asset/world/
   presentation. Keep committing/pushing verified checkpoints before pauses.

## Evidence, tools and useful commands

Start with `docs/README.md`, `docs/demo-substitute-audit.md`,
`docs/pc-physics-parameters.md`, `docs/pc-human-steering.md`, and
`docs/NATIVE_DEMO.md`. Manifests live in `config/SPEED2_EXE_1_2/`.

Reference: local `Need for Speed Underground 2/speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Retail source fixtures come from local `GLOBAL/GLOBALB.BUN`, not committed data.

Ghidra skill: `/home/dq/.codex/skills/ghidra-rpc/SKILL.md` (already used).
Project: `build/ghidra/ug2.gpr`, program `speed2.exe`. Check current daemon
status before relying on prior state. Local daemon access may need escalation.
Ignored `build/analysis/demo-recovery/` contains decompilation JSON and disassembly
for the draft tuning helpers; `build/analysis/function-index.json` has boundaries
and fingerprints. Ghidra's x87 types/signatures can be misleading: disassembly
and verified execution determine arithmetic and calling convention.

```sh
cmake --build build/native -j4
build/venv/bin/python tools/verify_pc_physics_extra_tuning.py
build/venv/bin/python tools/verify_pc_physics_construction.py
build/venv/bin/python tools/verify_pc_physics_parameters.py
build/venv/bin/python tools/verify_pc_physics_record_upgrades.py --native-stock build/native/ug2recordprobe
python tools/smoke_native_driving.py
```

Run the verifier commands sequentially because MSVC/wibo shares temporary
filenames. Local tools are `build/tools/wibo` and
`build/compilers/Win32/7.1/cl.exe`. Native32 execution requires escalation in this
sandbox; a sandbox exit 159 does not establish a native program failure.
Git mutations/push also require escalation because `.git` is sandbox read-only.
No analysis jobs were left running by the interrupted draft-edit step.
