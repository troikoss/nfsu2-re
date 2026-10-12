# Human steering dependencies

Reference: UG2 PC v1.2 executable SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
This checkpoint recovers construction, destruction and the steering rate helper;
it does **not** replace the demo's steering smoothing or ±30° scale yet.

## Recovered source and layouts

`Libs/bWare/TimedAverage.cpp` recovers the sample-average constructor `0x579f20`,
non-deleting destructor `0x5768e0`, timed constructor `0x579f80`, timed updater
`0x576aa0`, and non-deleting timed destructor `0x576a60`. The base object is
0x28 bytes, with inline storage for five floats, an allocated sample pointer,
and byte-sized capacity/count/cursor. The timed extension is 0x34 bytes, with
window +0x28, oldest index +0x2c and allocated timestamps +0x30. The original
integer capacity controls allocation and initial clearing while its low byte
is stored in the object. No replacement clamping or zero-capacity policy was added.

Expiry uses `time - timestamp > window`; a slot is unused only when both its
sample and timestamp are zero. Destruction compares the sample pointer with
inline storage, frees the timed allocation first, and retains the original
vtable identity writes. `malloc`/`free` remain platform/compiler boundaries.

`Physics/HumanSteering.cpp` recovers the normal constructor `0x405c60`, normal
destructor `0x405d00`, and rate helper `0x4060e0`. The 0x88-byte object belongs at
human input +0x220. Construction creates the original 0.55-second and 0.15-second
averages at 120 Hz (66 and 18 slots), retains car +0, and initializes the observed
flags/scalars/angle. Native partial input construction now calls this recovered
constructor; native vehicle teardown calls its recovered destructor. Ownership
of the enclosing native objects is still partial.

The rate helper uses the absolute change from +0x84 divided by duration, curve
`0x7e9f28`, timestamp `ticks86518C * scale784268`, timed average +0x48, mode scale
`0x783ae4`/`0x783ae8`, speed curve `0x7e9f14` and history curve `0x7e9f3c`. It keeps
the unused third argument because the original method cleans up twelve bytes.
It is recovered and verified but is not yet used by native steering.

Original constructor/destructor SEH exception unwinding and allocation-failure
handling are unresolved. The explicit recovered methods cover their normal
execution paths. Vtables here are identity/layout data; native code does not
execute mapped retail vtable targets.

## Verification

```sh
build/venv/bin/python tools/verify_pc_timed_average.py
build/venv/bin/python tools/verify_pc_timed_average.py --steering
build/venv/bin/python tools/verify_pc_demo_driving.py
cmake --build build/native -j2
python tools/smoke_native_driving.py
```

Run MSVC/wibo jobs sequentially. The manifests `timed-average.json` and
`human-steering.json` in `config/SPEED2_EXE_1_2/` pin original bodies and dependencies.
The verifier compares state, allocation/free arguments/order, stack cleanup,
preserved registers, constructor return pointers and the rate helper's floating
return. Stateful sequences include wrapping, expiry, zero samples, multiple
capacities, speeds and mode flags. The original constructor/destructor SEH chain
is mapped for normal execution; exceptions are not raised.

On 2026-10-11, 6,030 average comparisons and 5,600 steering comparisons passed,
with bit-identical compared state and rate results. Native build and driving
checks passed after constructor/destructor integration. Generated reports stay
in `build/analysis/timed-average/`. **Byte matching is not established.** These
finite fixtures exclude invalid capacities, exceptional allocation, pathological
expiry and complete steering caller composition. Analysis executes original
`__ftol2` as a compiler runtime dependency.

## Keyboard, analog and limit routines

`Physics/HumanSteeringUpdate.cpp` recovers keyboard steering `0x416bb0`,
analog steering `0x4061c0`, the keyboard limit `0x405dc0`, auxiliary slip
gate `0x59fa10` and analog-device test `0x5c1000`.

- `0x405dc0` returns the steering limit in degrees and stores it at +0x80. A
  speed curve (`0x7e9ed8`) is scaled by a pedal term (`0x7e9f14` response
  times `1 - ((throttle + 1) - (handbrake + brake) / 2) / 2`) and by input
  history (`0x7e9f00`). Within one second of a collision stamp (car body
  +0x140), outside groups 89E7D9/890104, it is eased by recovery curve
  `0x81e5b0` and the speed ratio to `0x5b5930`. Counter-steering slip (slip
  angle opposite to the input, with hysteresis flag +0x10 between 15/5
  degrees and 13.41/0 speed) replaces it with the slip magnitude times a
  counter curve (`0x7e9eb0`, `0x7e9ec4` in group 3), capped at that curve's
  multiple of 45; otherwise, while steering is held, it may not fall below
  the previous limit minus `0x783af0` per second. The result is capped at 45.
- `0x416bb0` maps the input through curve `0x7e9e60` (`0x7e9e9c` in groups 3
  and E2), scales the limit by record +0x270 (times yaw curve `0x7e9f50` when
  turning into the yaw in group 3), clamps the target to +-45, approaches it
  from the current angle by the rate from `0x4060e0`, and stores the angle at
  +0x7c in 65536/360 units through `__ftol2` and integer division by 360.
- `0x4061c0` clamps the raw input to +-value8, maps it to -1..1 and through
  curve `0x7e9e88`, computes the same limit with curve `0x7e9eec`, a spilled
  pedal term and a different multiplication order (skipped when flag +4 is
  set), and replaces counter-steering with the slip magnitude capped at 45.
  It sets the angle directly, without a rate, and leaves +0x80 unchanged.

`tools/verify_pc_human_steering_update.py` fingerprints the five routines and
their callee closure. On 2026-10-12 **6,460 comparisons** were bit-identical:
1,600 each for the limit, keyboard, analog and slip routines over 46 retail
records with synthetic car/input states, all mode groups, collision stamps
(including unset and 0x7fffffff), counter-steering, NaN inputs and synthetic
recovery curves with 4/0/1 pairs (`0x81e5b0` is filled at runtime), plus 60
device tests. Slip angle `0x591d90` and physics speed `0x5b5930` execute as
original code on both sides (controlled interfaces, verified separately).
Dependencies compile without `/Op` as in their own verifiers; the new routines
compile with `/Op`, and an analysis-only `fsqrt` helper stands in for the
`__CIsqrt` call `/Op` emits (the original uses inline `fsqrt`). The empty-yaw
case reads a stale stack slot in the original and is not exercised. Mutations
of the counter limit, recovery ease and recovery floor were detected. Byte
matching is not established. Dispatcher `0x41dfc0`, alternate steering
`0x406a40` and force feedback `0x406460` remain, so native steering still uses
its substitutes.

## Remaining steering chain

The human update `0x41dfc0` selects keyboard `0x416bb0`, analog `0x4061c0` or
alternate `0x406a40`, applies `0x406460`, updates history and exports the angle.
Maximum-angle helper `0x405dc0` additionally uses physics speed helper `0x5b5930`,
car-body signed angle `0x59fa10`, clock-sensitive feedback and several curves.
The [physics helper and drive curve](pc-physics-parameters.md) are now recovered
and verified. Native uses the corrected +0x10 parameter subobject and 0x580-byte
allocation. The parameter constructor and refresh wrapper `0x5b4fc0` are recovered and
integrated. Composition `0x5b4dc0` and tuning application remain unresolved. Recover and populate that original composition before integrating
the full steering chain.
