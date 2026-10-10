# PC v1.2 racing recovery

This slice is recovered from the locally supplied UG2 PC executable, not
replacement gameplay designed for the combined engine. It remains separate
from OpenUG2. It is not a playable executable or a full-game source rebuild.

## Compiled recovery

| Routine | Original VA | Bytes | Verification |
| --- | --- | ---: | --- |
| Intrusive list sentinel accessor | `0x502fd0` | 3 | Compiled function body identical |
| Drift list position lookup | `0x56dd90` | 54 | Compiled body identical after three checked relocations |
| Career bank reward selector | `0x532e50` | 79 | Compiled body identical after two checked relocations |
| Career reward eligibility | `0x5287a0` | 480 original / 419 candidate | **Nonmatching**, bounded behavior comparisons pass |

Function names are descriptive recovery names, not recovered debug symbols.
Inter-function padding, original mangled names and a complete linked executable
are outside the byte comparisons. This verification does not run objdiff; it
checks every relocated instruction byte directly and rejects unknown relocations.

Sources: `src/Speed/Indep/Src/Gameplay/{DriftRankList,DriftRanking,CareerReward,
CareerEligibility}.cpp`, with partial recovered layouts in the two accompanying
headers. Layout sizes are checked at compile time for the original 32-bit target.
Unidentified fields remain unnamed. The eligibility candidate is not counted as
a matched function and must not be advertised as a complete recovered mechanic.

The list lookup returns the first matching identifier's one-based position or
zero when absent. It does not compute points, decide list sort order, or prove
identifier zero is the human player. The eligibility consumer requests identifier
zero and accepts only position one for its drift branch. Score accumulation and
the list's insertion/sorting functions still need recovery.

## Reproduce

From this checkout, with the existing local compiler/tool environment:

```sh
build/venv/bin/python tools/verify_pc_drift_ranking.py
```

The verifier compiles all four translation units with MSVC 7.1 `/Ox /Z7 /EHsc`,
resolves only the enumerated COFF relocations, and checks the original fingerprints.
No inline assembly, register forcing or embedded original instruction bytes are
used in the recovered C++.

It exercises original and rebuilt list lookup in 5,808 deterministic cases:
empty lists, lengths through 32, duplicate identifiers, first/last matches,
missing identifiers and signed-bit edge values. Both machines must return the
expected position, preserve callee-saved registers and balance the stack.

The recovered reward and eligibility candidates are also exercised together:
960 non-drift eligibility executions, 960 non-drift payout executions, 16 drift
eligibility executions and 16 drift payout executions. The corresponding original
instruction probe runs the same states. No helpers are stubbed for those cases.
The drift cases include absent, leading, trailing and duplicated identifier zero,
and deliberately absent non-drift detail data to prove the drift-specific branch.

Results are local ignored artifacts under `build/analysis/racing/`:
`drift-ranking-verification.json`, `eligibility-probe.json`, and
`rebuilt-eligibility-probe.json`. All comparisons use synthetic states, not recorded
retail race outcomes. Multi-entry eligibility still calls the original overall
result helper and is outside this probe; the source slice is not standalone yet.

## Playable demo boundary

The existing OpenUG2 driving demo remains a provisional native reimplementation.
These routines have not been inserted into it: its derived checkpoints and result
data are not proven to correspond to UG2's race-result objects. Connecting them
now would invent the conversion that the fidelity instruction asks us to recover.

Remaining requirements for a faithful source-built race demo include original
vehicle handling/update rules, event route/checkpoint semantics, start/countdown/
finish lifecycle, construction of result objects, opponent driving and source
data consumers. A matched reward selector alone does not satisfy that milestone.
