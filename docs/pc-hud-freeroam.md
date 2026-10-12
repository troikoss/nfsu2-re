# PC freeroam HUD recovery

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Ghidra decompilation was checked against x86 disassembly and original/rebuilt
execution. This pass extends the existing gauges, visibility and minimap; it
**does not establish complete freeroam HUD recovery or complete native parity**.

## Recovered source and behavior

`HudFreeroam.cpp` retains the original feature layouts and call boundaries:

- Turbo equipment show/hide, normal/drag dial sweeps and boost reader.
- Shift-light tail `0x4c6acf..0x4c6b4d`, upshift-point reader and shift-window
  reader. The original gauge-selected RPM and tuning index are caller inputs;
  native free drive uses its current RPM and drivetrain tuning index.
- Nitrous empty/normal/gamebreaker branches, single/multiple-player and drag
  layouts, mask rotation, colours and exported negative angle. Fraction zero
  hides the fill; fractions above one retain their extra-capacity behavior.
  The existing recovered `0x5925b0` fraction provider remains a separate routine.
- Heat clamp and bar update. GPS arrow mode/texture/position update, offset-based
  camera flag, repeated route queries, and arrival event `0x12`.
- Region hash change/localisation/ZoomIn; cash formatting, suppression and
  reward animation; delayed one-shot reward text, language separator and globals.
- Notification scheduler `0x4da9a0`: eight entry updates, selection, priority
  comparison, preemption, suspension of type `0x13`, timestamps and sound calls.
  Priority, selection and entry dispatch are recovered. The individual entry
  preparation/presentation routines remain boundaries.

`HudController.cpp` recovers full mask application `0x4ca090`, including the
stripped low word, changed-bit dispatch, player visibility callback, lap/leader/
style group script choices, and the visibility subobject at feature `+0x70 +8`.
Unidentified slots retain offset names.

`HudPlayerMasks.cpp` recovers mask *production*: `PlayerRecord::GetHudMask`
`0x600090` (64-bit result, low word in EAX, high in EDX) and its helpers — score
`0x5ea8c0`, map `0x5ea8e0`, camera `0x5ea930`, gauges `0x5f32a0`, race `0x5f33b0`,
ambient tail `0x5f34c0`, restart wait `0x5fff50`, race-status queries `0x5f85b0`/
`0x5fca90`, replay query `0x5f0880`, `0x571250`, the `+0x414` timer reader
`0x401860`, session queries `0x401910`/`0x401930`/`0x5ed990`, game-flow `0x5ea190`,
profile `0x457ba0`, flags `0x5012c0`/`0x5012d0`/`0x510050`, holder `0x501690`/
`0x501680`, career bank `0x500f90` (on `0x861e74`), language `0x4ff970` (reads
`0x7f70d0`) and career-state queries `0x502420`–`0x5024b0`. The cash feature calls
the recovered `0x501680`/`0x500f90` and its separator choice calls `0x4ff970`. The player record keeps
offset names (`+0x10` settings row, `+0x28` mode, `+0x50` restart frame, `+0x58`
state, `+0x234`, `+0x238` camera mover, `+0x249`, `+0x24a`). Camera mover kind
`+0xc` 1 is the chase mover (`0x451660`, vtable `0x787f28`); kinds 2/3 return mask 1
and 4/`0x12` return 0. The per-car race reset `0x5fc2a0` is not recovered.

`tools/verify_pc_hud_player_masks.py` (manifest `hud-player-masks.json`) runs the
original and the MSVC 7.1 rebuild on 20,000 randomised player/flow/race/camera/
settings/replay scenarios: 730,040 comparisons of EAX:EDX (or bool/int/80-bit x87
results), callee traces, the arena and every read global range, stack cleanup and
callee-saved registers, all equal (150 distinct `0x600090` masks). `0x5f00f0` and
`0x4018f0` run as original bytes on both sides; `0x5fc2a0` and the camera vtable
slot `+0x28` are traced interfaces. Every original basic block executes except
`0x60050e` (state `1`/`2` after the state `0` test) and `0x6004e2` (bit `0x4000000`,
which no path sets) — both statically unreachable. A comparison mutation is
detected. Byte matching is not established.

`HudAnimation.cpp` recovers vector and integer colour interpolation at
`0x505150`/`0x5051a0`, retaining arithmetic order and rounding.

## Reproduction and evidence

Run MSVC/wibo jobs sequentially:

```sh
build/venv/bin/python tools/verify_pc_hud_freeroam.py
build/venv/bin/python tools/verify_pc_hud_minimap.py
build/venv/bin/python tools/verify_pc_hud_visibility.py
cmake --build build/native
SDL_VIDEODRIVER=offscreen build/native/ug2demo --frames 120 --scripted \
  --screenshot build/analysis/freeroam-hud/native-final.ppm
```

The new verifier compiles the recovered source with MSVC 7.1 `/Ox /Ob0 /Z7 /EHsc`,
links its COFF sections and runs both versions in Unicorn. It compares complete
fixture memory, interface call order/arguments, returned float bits and written
reward/NOS globals, stack cleanup, saved registers and x87 control words
`0x027f`/`0x037f`. Execution is guarded to the recovered routines, declared
interfaces and retained original helpers `__ftol2` and `0x5947b0`.

FEng operations, localisation, bank/suppression/language queries, nitrous
fraction, GPS route queries/event receiver, notification preparation/presentation/sound and controller child-mask application are **controlled interfaces**.
Their internal behavior is not established by this verifier. Existing visibility
verification separately covers the real show/hide/mask routines. The shift-light
comparison enters the original tail and stops before its surrounding epilogue;
its frame setup and tuning query are controlled. It does not verify the earlier
replay RPM branch or the complete gauge function.

Report: `build/analysis/freeroam-hud/verification.json`. **10,612 bounded comparisons
pass; byte matching is not established.** Finite synthetic cases do not prove
complete coverage. Fingerprints below cover the recovered original spans:

| Routine | Address | Bytes | Cases | SHA-256 |
|---|---:|---:|---:|---|
| HudTurboAngle | 0x4aa160 | 71 | 1080 | `ed7b3c8a6950960ce85d62f8953a53e7df2c4aa278d4ff575371aeb4a9365aa2` |
| HudReadBoost | 0x5ecec0 | 94 | 192 | `e1be28cde2058fb191e96dcc84f15360e0b89edd17e34238ee7d500f54ef06ee` |
| HudReadHeat | 0x5eccd0 | 87 | 64 | `8032868f61ca46148f6999fc1eda7e54e3e2b0a840e73c2590dc9702bd06f45c` |
| HeatUpdate | 0x4c7aa0 | 125 | 60 | `0784c26b6f4402f84edc7f5c4a780afd4e3592831191cb5674b9cd588b75b1cd` |
| GpsUpdate | 0x4c8410 | 434 | 320 | `210f86426eb1007022e0aca1e16a1b1400f85fe3f95ddfacef37d1c5f053d408` |
| NotificationUpdate | 0x4da9a0 | 468 | 1600 | `689080d2115a89c9a28368908607eedcc687828fa198bbbd806d61304a22bac3` |
| HudNotificationUpdate | 0x4c9030 | 115 | 112 | `a31addef4f7d4137dc0f9a4402cc61db740429f9cbbc6d228ae1a42292b554fe` |
| HudNotificationNext | 0x4c90b0 | 112 | 1024 | `9b1810d9b8bc8f62016b2c5432e5a38fbf2a2d42af4563d9dbcf8830c72c0d66` |
| HudNotificationPriority | 0x4abf20 | 39 | 56 | `73cf228f66f794dd52799769a3466bc5403a42c10f475359c7e28ab681bf6c7c` |
| HudReadShiftWindow | 0x594f00 | 31 | 288 | `ffbdb5f263d2c41a1b52b62156757515094bc8e94c8b0f7223bc85edc14912b6` |
| HudReadShiftPoint | 0x5ecd30 | 53 | 72 | `af3755ab3f82f4355d116561846b9b61404a907dab1f47b9003e1f04bf37758c` |
| TurboUpdate | 0x4c6b80 | 302 | 96 | `d731cc3259ff1e76acbb9f26d6febfd872aeb4b1d8b87060d609fd414bac8590` |
| NitrousUpdate | 0x4c7b20 | 757 | 288 | `9c1bb332bb68ffb7acfbd642047817e606b3f6e840ba178c4976f1e313a0035c` |
| RegionUpdate | 0x4a9e70 | 115 | 48 | `635a72d079745ebd0652a7a87f039cf1214678eb1b448e6a31363fe9df6b5da0` |
| CashUpdate | 0x4a9ef0 | 250 | 96 | `f06a1c0d3f1a8fdbc2681ed6d4b65d8f80991decab0303aa6e626e7a1f664c7a` |
| RewardUpdate | 0x4a9ff0 | 235 | 384 | `f86932bef538e192a9675110df5824d7e81ad0439a1be45071366d9e2e65ec31` |
| HudLerpVector3 | 0x505150 | 71 | 640 | `5fca7aecedc1b83239d1edd101b407492a9d8dc8981b96003f3311f205c35501` |
| HudLerpColour | 0x5051a0 | 190 | 640 | `6f2abd49af5cea01139f4a294aa045848a8de80038d7be8e152d5be8d2447640` |
| ControllerMasks | 0x4ca090 | 1500 | 2016 | `fc1086f9073018e55ac248fccbc695b4d0c7d11413bde82ad55cbd3311e57d0e` |
| ShiftLightTail | 0x4c6acf | 126 | 1536 | `1d00c08a2ddd9997b296abbace296a4ac93cd5640601d587b58b9058b1233cca` |

## Native integration and seams

The native demo registers the retail constructor object names/masks using fixed
node storage, supplies its one-player chase/map context, and uses recovered
`0x4ca090` transitions. It runs gauges, shift light, turbo, nitrous, region, cash
and reward updates, followed by bounded script playback and the minimap update.
Parent containers are revealed without recursively showing unrelated siblings.
Retail authoring sample strings for cash/reward/region are cleared at native
initialisation so they cannot appear as live events.

`SetCareerHudState` accepts actual balance and region hash from a future recovered
career/world producer. The demo does not call it and does not invent a wallet,
region transition or reward event. An ignored analysis fixture supplied explicit
synthetic values to inspect these layouts; they are not gameplay evidence.

The native FNG adapter retains scripts and supports constant tracks plus
interpolation-1 type-4 vector/type-6 colour message tracks with cycle 0 or 1. The `-1`
key is a baseline and later keys are offsets, as in `0x52bdc0`; arithmetic calls
use recovered primitives. ZoomIn and Cash_Animate use this bounded path.
Unsupported dynamic interpolation/cycles trap. The full FEng track dispatcher,
quaternion/relative/other-cycle playback and script lifecycle remain unrecovered.
Cycle-1 support was added for the settings highlight scripts; see
[settings integration](pc-pause-options.md).

Original rendering trace `0x536a50 -> 0x51a5d0 -> 0x5c7ea0 -> 0x5c5020` leads to
texture state `0x5bcc30`: blend mode 2 selects SRCALPHA/ONE, mode 5
DESTALPHA/INVDESTALPHA, mode 6 DESTALPHA/ONE. The native OpenGL adapter applies
those factors rather than treating NOS's black mask as an opaque image. This
state translation is scaffolding, not a recovered D3D renderer or verified
rendering parity. `0x5c5020` contains special NOS/minimap stencil increment and
comparison branches. **Full NOS destination-alpha/stencil composition is still
missing**, and equipped NOS visual fraction fidelity is not established. The
synthetic 0.5-capacity fixture still displays the full arc, demonstrating that
remaining composition gap; its bank text and absence of sample event strings
were inspected and it exited successfully.
The stock Skyline smoke has no installed NOS and cannot verify this path. The
minimap retains its documented RGB/stencil substitute.

Heat, GPS and notification scheduler source are verified but not registered as
live native features. Their absent inputs are not replaced with fabricated state;
unsupported GPS/notification callbacks trap if invoked. Remaining work includes:

- Mask caller `0x600590` (with controller update `0x4ec5a0`), rear-view mirror view `0x5f1e10`
  and per-car race reset `0x5fc2a0`.
- Region world trigger producer `0x5eb040`, including lookup `0x5d9bf0`,
  descriptor lookup `0x5d3de0` and `NEIGHBOURHOOD_%s` localisation hash production.
- Career bank/save/suppression and reward producers; notification preparation `0x4c8ac0`,
  presentations `0x4c8b80`/`0x4c8dd0`/`0x4abaf0`/`0x4aba30`, sound and event producer
  `0x4db8c0`. Selector `0x4c90b0`, priority `0x4abf20` and entry dispatch
  `0x4c9030` now execute recovered source in scheduler comparisons.
- GPS route manager `0x838590` (`0x501100`/`0x5010c0`), arrival receiver and minimap
  GPS/career/AI icon population and refresh.
- Style/trick/gamebreaker presentation and its event/score producers, other
  race-only HUD features, replay RPM and HUDINDEX.
- Complete FEng construction/update order, script engine, rendering and NOS
  stencil composition, with separate original/rebuilt and visual verification.

The minimap and visibility regressions passed (7,972 and 3,990 comparisons).
Native build and a 120-frame scripted offscreen stock Skyline run passed. The
inspected `build/analysis/freeroam-hud/native-final.png` shows the gauge cluster,
boost dial and city minimap without sample region/reward/cash messages. This is
native smoke evidence, separate from retail routine comparisons.
