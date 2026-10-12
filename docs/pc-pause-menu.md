# PC pause menu recovery

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Ghidra decompilation was checked against x86 disassembly and original/rebuilt
execution. This pass covers `UI_Pause.fng` and its option items. The IconScroller base, most of FEng, the in-game dialog and the game-flow states the menu queues
remain unrecovered. Package mask, selection and focus recovery is recorded below.

## Recovered source

`src/Speed/Indep/Src/Frontend/PauseMenu.cpp` (community name `PauseFNGObject`,
vtable `0x797cb0`, 0x158 bytes, base `IconScrollerFNGObject` vtable `0x7974d4`):

| Routine | Address | Notes |
|---|---|---|
| `CreatePauseFNGObject` | `0x4f4790` | `FNGINIT_UI_Pause.fng` factory; `new` + constructor, null on failed allocation |
| Constructor | `0x4eb410` | Zeroes then copies timer `0x8651ac` to `+0x150`, sets control mask, clears `0x83aa04`, runs `Setup`, adds and syncs the help bar if absent |
| Destructor / scalar | `0x4eb510` / `0x4eb4f0` | Removes the help bar if loaded; scalar form is compiler generated |
| `UnknownSlot0C` | `0x49dfc0` | Shared folded override (slot 3): `0x50d1d0(arg1, arg5)` |
| `Setup` | `0x4d9300` | Header/back/select labels, freeroam or race option list by `0x89e7b0`, scroller slots `+0x20`/`+0x04`, virtual `SelectedIconChanged` tail call |
| `AcceptsInput` | `0x4a7bc0` | Timer changed since opening and the player's controller is assigned (`0x571a60`) |
| `OptionsAvailable` | `0x4a7bf0` | Scans the two-entry table `0x8900ac` for the current player |
| `QuitToCrib` / `QuitToCribKeepHelpBar` | `0x4a7c70` / `0x4a7cc0` | Career `0x513b30`, return package `UI_CareerCrib.fng`, flow message 6 |
| `QuitToProfile` | `0x4a7cf0` | Return package from profile `0x502ff0(0)`, flow message 6, career `0x5139c0` in mode 1 |
| `MessageHandler` | `0x4c2170` | Base handler gate, 13 message cases and the `+0x13c` dialog-result dispatch |
| `SetupFreeroam` | `0x4c2900` | Option list when `0x89e7b0 == 0` |
| `SetupRace` | `0x4c2bd0` | Option list otherwise; only the quit item when `0x8500d0 >= 3` |
| Item constructors | `0x4c2640`, `0x4c27f0` | Out-of-line constructors; the other items' constructors are inline in the original |
| Item `React` | `0x4c24c0`–`0x4c2820` | Nine handlers, all acting on message `0x0c407210` |

Option items derive from `IconScrollerIcon` (constructor `0x51f670`, 0x48 bytes)
and are named after their retail vtables. Each vtable is `[0x4aeb70, React]`;
`0x4aeb70` is the folded scalar deleting destructor calling `0x527770`.

| Vtable | React | Effect on select |
|---|---|---|
| `0x7954d0` | `0x4c24c0` | flow message 1, package value reset, help bar removed |
| `0x7954d8` | `0x4c2510` | flow message 2, package value reset, help bar removed |
| `0x7954e0` | `0x4c2560` | `UI_InGameDialog.fng`, result `0x33c42e03` |
| `0x7954e8` | `0x4c25d0` | dialog, result `0xe1a57d51` |
| `0x7954f0` | `0x4c26b0` | `ui_ReplayControl.fng` when replay data is available; stores the package at `0x8363e4` |
| `0x795510` | `0x4c2700` | `UI_PauseOptionsMain.fng` |
| `0x795518` | `0x4c2730` | dialog, result `0x43da9fd0` |
| `0x795520` | `0x4c2780` | dialog, result `0x451e768e` |
| `0x795528` | `0x4c2820` | dialog, result `0x30ed2368`; text hash depends on mode `0x850078`, network `0x5f0120`, `0x850098` and flow state |

The `neg/sbb/and/add` selections are written as ternaries with the summed
retail constants. Function names other than community IDC names
(`PauseFNGObject::MessageHandler`, `IconScrollerFNGObject::*`, `FNGINIT_UI_Pause.fng`)
are descriptive; unidentified fields and globals keep offset names. The
`QuitTo*` names follow the return packages they set; game-flow message meanings
(1, 2, 3, 4, 6, 0xb) are not established.

The native demo compiles this source unchanged (CMake glob `Src/Frontend`); its globals are
bound in `OriginalSymbols.ld` and the unrecovered interfaces are native seams, described in
[native demo: pause menu](NATIVE_DEMO.md#pause-menu-sixth-pass).

## Retail data

`UI_Pause.fng` (`0x32124083`) and `UI_InGameDialog.fng` (`0xaa469286`) are JDLZ-compressed
`0x30210` chunks in `GLOBAL/InGameCommon.bun`. The pause package has eight `option_N` icon
objects, a hidden `OPTION_MASTER` anchor, `ICON_TITLE`, `Header_text`, and the event handler
`0x47ff4e7c`, whose `Forward` script sends `EXIT_COMPLETE` at 320 ms. Labels resolve in
`English.bin`: the header is "Pause", items read Return to Free Run/Explore Mode/World Map, Resume
Race, Restart, Jump to Garage?, Options, Replay and the Quit variants.

## Reproduction and evidence

```sh
cd tools && ../build/venv/bin/python verify_pc_pause_menu.py
```

The verifier compiles the source with MSVC 7.1 `/Ox /Ob0 /Z7 /EHsc`, links the
COFF object with every external bound to its retail address, and runs original
and rebuilt code in Unicorn with a flat GDT and an FS-based TIB so the SEH
frames execute. Interfaces are hooked at the same retail addresses in both
machines, so cdecl/thiscall argument order and callee cleanup are compared
directly. Each comparison checks the interface trace (including the 0x48-byte
item passed to `AddIcon`), the return value, fixture memory, the written globals
`0x83aa04`/`0x8363e4`, stack cleanup, callee-saved registers and the restored
SEH head. Rebuilt vtables are checked slot by slot against the retail tables
(25 slots) and normalised to retail addresses in memory. Execution is guarded to
the listed routines and declared interfaces.

Current result: **11,723 passing comparisons**, recorded with fingerprints in
`build/analysis/pause-menu/verification.json`. Cases cover game modes
0/1/2/4/8/0x200, race types 0/1/7, `0x8500d0` 0/2/3/4, the `0x8635a8`/`0x85ba67`
flags, six player-table layouts, failed allocation, help-bar/script/controller
states, every handled message plus unhandled ones, every `+0x13c` dialog result,
flow states and replay availability. Mutating constants, conditions, item flags,
flow messages or handler targets in the source makes the verifier fail.

The verifier found that `0x571a60` returns an `int` tested as a full register
(a `bool` declaration fails the `0x100` case).

**Byte matching: not established.** The original inlines most item constructors
and the help-bar removal; the rebuilt `/Ob0` objects call them out of line.

### Controlled interfaces (not verified)

FEng (`0x505450`, `0x537a10`, `0x50c900`, `0x50cea0`,
`0x52cf60`, `0x5379a0`, `0x555f10`, `0x537980`, `0x537950`, `0x50d2d0`,
`0x537cc0`, `0x537c00`, `0x55dda0`, `0x558790`, `0x50c8d0`, `0x50d1d0`),
controller `0x571a60`, game flow `0x5f41e0`/`0x5f4240`, help bar
`0x54e6a0`/`0x54e6e0`, career `0x513b30`/`0x5139c0`, profile `0x502ff0`,
network `0x5f0120`, allocation `0x575620`/`0x575630`, IconScroller base
(`0x552330`, `0x4defb0`, `0x543d40`, `0x52fbf0`, `0x50da20`, `0x50da50`,
`0x50dab0`, `0x40d080`), `IconScrollerIcon` `0x51f670`/`0x527770`, and the
scroller object's slots `+0x04`/`+0x20` (fake vtable). The base constructor
stand-in writes the package pointer, `+0x20` and the scroller vtable that the
pause routines read.

## Remaining work

- Pause options bases and backends: `0x4a7d60`, `0x4a7f70`, `0x4a8010`, constructors
  `0x4eb580`/`0x4d93c0`, factories `0x4f47f0`/`0x4eb650`. These screens are now
  recovered and integrated separately; see [settings evidence](pc-pause-options.md).
- IconScroller base, in-game dialog `0x558790`, and the game-flow handling of the
  queued messages.
- Recover the IconScroller base and FEng message/script dispatch that the native demo
  currently supplies as seams (`NativePause.cpp`).

## FEng package masks, selection and focus

`Frontend/FEngPackage.cpp` recovers five routines from Ghidra and checked x86 disassembly:

| Routine | Address | Behavior |
|---|---|---|
| `FngSetControlMask` | `0x537950` | Writes all four masks `+0x1c/+0x20/+0x24/+0x28`; missing package is ignored |
| `FngGetPackageValue10` | `0x50b8a0` | Hashes package name and reads the static FNG record's `+0x10`; zero if absent |
| `FngSetPackageValue10` | `0x50b8c0` | Writes that same record; missing record is ignored |
| `FEPackage::SetFocus` | `0x52b3a0` | Queues old-object lost-focus `0x55d1e635` and new-object gained-focus `0xabc08912`, each first to the object then to target -5, before assigning `+0x8c` |
| `FEngManager::SetFocusByHash` | `0x52ce40` | Looks up archive node, reads package `+0x18`, scans the full focus array, and selects the last matching hash |

The selection field is **not** a live FEPackage field: `0x50b840` finds it in the
static 0x1c-byte FNG records beginning at `0x7f7dc8`. The earlier native seam and
handoff described its ownership incorrectly, although they preserved it across reloads.
`FEngPackage.h` now defines the observed FEPackage prefix through `+0x94`, the
archive node's package pointer and manager's queue pointer at `+8` with offset checks.
Unknown storage stays unnamed beyond its offset.

Native screens own an FEPackage prefix, an archive-node prefix and a separate persisted
record. The native loader builds the focus array from objects with flag `0x10000000`
(as read in original object reader `0x555140`). Icon selection calls the recovered
hash lookup; option widget selection calls recovered `SetFocus` directly. Input routing
reads `focus8C`. Native archive lookup, object loading, record lookup and queue storage
remain explicit seams; target -5 delivery remains unmodelled. This does not establish
recovery of the archive lists, manager lifecycle or original FEng dispatch.

`build/venv/bin/python tools/verify_pc_feng_package.py` compiles with MSVC 7.1 and
executes original/rebuilt code in Unicorn. **10,000 passing comparisons** cover null
package/record/node, null node package, arbitrary input masks and saved values, null/old/new/same
focus, notifications disabled or enabled (including noncanonical byte values), empty arrays,
missing hashes and duplicate hashes. It checks complete fixture memory, queue message/order/
arguments, focus at each queue call, return values where defined, stack cleanup and preserved
registers. Lookup/hash/queue boundaries are controlled interfaces at their original addresses;
their implementations are outside this proof. Fingerprints and results are saved locally in
`build/analysis/feng-package/verification.json`. **Byte matching is not established.**

Mutation checks reject zeroing one mask, changing the gained-focus message, selecting the
first duplicate hash and assigning focus before notifications. Existing pause verification
still passes 11,723 comparisons and 25 vtable slots; options passes 5,730 comparisons over
89 routines and 510 vtable slots. The native build and all 19 settings smoke scenarios,
including rendered slider checks, pass. Native smoke evidence establishes routing and
session integration only, separately from original/rebuilt execution.

## IconScroller navigation and dispatch

`Frontend/IconScrollerNavigation.cpp` recovers eight routines:

| Routine | Address | Behavior |
|---|---|---|
| `AtFirst` / `AtLast` | `0x5276d0` / `0x527700` | Current icon equals first/last; only AL is defined in the original |
| `Next` / `Previous` | `0x527730` / `0x527750` | Choose wrapping or non-wrapping stepping from byte `+0x2c` |
| `StepNoWrap` | `0x51f9f0` | Step toward the boundary, skipping disabled icons unless byte `+0x30` permits them |
| `StepWrap` | `0x51fac0` | Wrap at either boundary, with the same disabled policy |
| `IndexOf` | `0x51f820` | 1-based list position; -1 for null or absent icons |
| `Dispatch` | `0x50d980` | Call current icon's React slot only when sender equals its FEObject `+0x10` |

The observed scroller prefix through `+0x34` is now explicit in `IconScroller4C`:
node links at `+4/+8`, current icon `+0xc`, package `+0x1c`, layout flag `+0x2e`,
disabled-selection byte `+0x30` and last direction `+0x34`. Links point to icon
**+4**, including the sentinel at scroller +4. Both step routines call virtual
slot `+0x14` first, then set the layout flag and direction. The native scroller
now supplies this list and calls recovered stepping, indexing and dispatch;
its selection callback still sets current icon, synchronizes native layout and focus.
Mouse selection synchronizes the same current-icon field. List construction, icon
construction, slot `+0x14` itself, layout, scroll/fade animation and the owning
FNG object's message handler remain native seams.

The original wrapping routine does not terminate for direction ±1 when all icons
are disabled and `+0x30` is zero. No replacement guard is added. Normal native
pause lists include enabled items. Empty/null-current lists are not valid step
inputs; callers retain their existing empty-list checks.

`build/venv/bin/python tools/verify_pc_icon_scroller_navigation.py` compiles with
MSVC 7.1 and runs original/rebuilt routines in Unicorn. **34,670 comparisons**
exhaust all disabled masks and current positions for lists of 1–6 items, allowed
selection bytes 0/1/0x80, directions ±1/0/±10, and both wrapper modes; the known
nonterminating wrap inputs are excluded. Tests also cover empty/null/absent index
queries, null current icon and matching/mismatching dispatch sender, arbitrary
message arguments, and callbacks that either update or preserve current selection.
They compare fixture memory, virtual callback traces and pre-callback layout state,
stack cleanup, callee-saved registers and defined return bits. The selection and React
callbacks are controlled interfaces, not verified implementations. Fingerprints and
counts are stored in ignored `build/analysis/icon-scroller-navigation/verification.json`.
**Byte matching: not established.**

Four mutation checks reject inverted disabled policy, a zeroed last direction, a zero-based
index and inverted dispatch-sender gating. Existing pause (11,723) and options (5,730)
comparisons remain passing. Native compilation and all 19 settings smoke scenarios with
rendered slider checks pass separately as integration evidence.

An additional native run (`--frames 240 --pause-at 20 --pause-keys RRA`) skips the
disabled Replay icon and reaches the Quit dialog with result `0x30ed2368`. Its
screenshot/log stay in ignored `build/analysis/icon-scroller-navigation/`.

## Icon selection and the full scroller layout

`IconScrollerSelection.cpp` now recovers `SelectIcon` (`0x52f680`), indexed selection
(`0x52f7c0`) and intrusive list `NodeAt` (`0x43be60`). `FEngPackage.cpp` also recovers
focus-flag helper `0x51cc60`: only argument byte 1 clears bit `0x4000000`; every
other byte sets it. Disassembly and stack verification establish that this helper
is **stdcall**, including its 12-byte callee cleanup.

Both selection paths mark every icon's matching FEObject before deciding whether
selection can change. Direct selection then checks the disabled byte, assigns current
icon, clears the selected object's flag, queues package focus and stores its **1-based**
list index. Indexed selection is different: it passes a **zero-based** input to `NodeAt`,
rejects the current icon or a disallowed disabled icon, and stores the input unchanged.
No new range checks are added: `NodeAt` returns the sentinel when an index is outside
the list, and original indexed selection can interpret that sentinel as an icon.
These APIs retain their original caller preconditions.

`IconScroller4C` retains the complete observed `0xe8`-byte scroller extent, including
ScrollBar storage `+0x38` (0x58 bytes), tween storage `+0x90` (0x30), alignment `+0xc0`,
selected index `+0xc8`, window width `+0xcc`, anchor position `+0xd4/+0xd8`, running offset
`+0xe0` and fade byte `+0xe4`. This is layout evidence, not recovery of the constructors
or all fields. The native vector, ownership and other backend state moved to separate
storage keyed by the embedded scroller; it no longer occupies these original offsets.
The manager pointer at `0x8384c4` now binds to the native manager when pause assets load.
Native selection calls recovered `SelectIcon`; native layout still follows that call
and remains a seam. Original layout/animation/construction remains outstanding.

`build/venv/bin/python tools/verify_pc_icon_scroller_selection.py` compiles selection,
previously recovered navigation and package focus code together with MSVC 7.1.
**12,063 comparisons** execute original/rebuilt selection plus the actual focus-flag,
hash-focus, focus-queue and list-index routines. Only archive lookup `0x51bb70` and queue
storage `0x51aa20` are controlled interfaces. Cases cover 1–8 icons, null/current/other
selection, disabled policies, absent manager/node/package, empty or duplicate-hash focus
arrays, pre-existing focus, same-icon refusal, zero/negative/out-of-range indices and
empty list indexing. The sentinel fixture includes a valid object so out-of-range
selection is executable without adding a substitute guard. Checks compare complete
fixture memory, lookup/queue traces, defined return bits, stack cleanup and preserved
registers. Fingerprints stay in ignored `build/analysis/icon-scroller-selection/verification.json`.
The package verifier now includes focus flags for **12,000** comparisons. Navigation
still passes **34,670**. **Byte matching remains unestablished.**

Four mutation checks reject changing the stored indexed value, leaving the selected object's
focus flag set, reversing the disabled-selection gate and removing same-icon refusal.
The native build, existing pause/options verifiers and all 19 settings smoke scenarios pass.

## FEng coordinates, text metrics and script state

`Frontend/FEngCoordinates.cpp` recovers these original routines:

| Routine | Address | Behavior |
|---|---|---|
| Glyph binary search | `0x6f45b0` | Sorted ushort keys with variable stride |
| Text width / height | `0x50bec0` / `0x50bf40` | Maximum suffix/line measurements; low-byte glyph lookup and trailing-break policy |
| Horizontal / vertical justification | `0x50c020` / `0x50c050` | Bit priority, -0.5 scale or negation; stdcall |
| Vector equality tolerance | `0x504500` | Ordered inclusive bounds using `0x7992ec`; AL result |
| State offset lookup | `0x5046f0` | Keys 1–10; offset zero otherwise; stdcall |
| Vector state writer | `0x5178d0` | First matching track per script, then live state; set/add and clear the track's high flag bit |
| Top-left getter | `0x51d070` | Image half-size subtraction, string metrics/justification, group no-op |
| Top-left setter | `0x51d380` | Image half-size addition, string alignment subtraction; preserve z and guard type >= 0x100 |
| Centre setter | `0x51d4d0` | Image centre directly, string metric midpoint/alignment subtraction; same dirty/state-write path |
| Size setter | `0x50cb00` | Writes size and dirty bit, including non-image types |

The state writer **always updates live state**, including when scripts exist; it also
updates only the first matching 0x38-byte track of each script. The original state-key
table is bound at `0x7992f4`. Dirty-bit decisions use the original epsilon comparisons,
including ordered NaN behavior, rather than rounded coordinate differences.

Native strings now retain UTF-16 `+0x64`, justification `+0x6c`, leading `+0x70` and
wrap width `+0x74`; font resource hash is also at the original `+0x24`. Script headers
point to track prefixes with baseline `+0x18` and state selector byte `+7`. Loader evidence
from `0x5515e0` establishes that FI selectors advance by value size/4 and To overrides
that selector (it is not FI payload byte 7); Tb/first Kd values supply the inline base.
Ta/Tt update flags/interpolation. The loader remains a platform seam, not a recovered
or original/rebuilt-verified package loader. Native playback synchronizes its baseline
from those prefixes. Icon positioning/size calls the recovered centre and size setters;
rendered text height uses the recovered height routine. Font lookup `0x51bd70` and low-level
line measurement `0x50bce0` remain native boundaries. Their native implementations, full
track dispatch, scroller placement and tweening are outside this proof.

`build/venv/bin/python tools/verify_pc_feng_coordinates.py`: **24,024 comparisons** over
12 routines, with 53-bit and 64-bit x87 precision. The tests execute the actual recovered
state/metric/helper dependency chain, controlling only font lookup and line measurement.
Cases include null object/font/text, image/string/group/default/high object types,
justification combinations, multiline/trailing-break strings, glyph hits/misses and strides,
empty/multiple script lists and repeated matching tracks, set/add bytes, offset keys,
pre-existing flags/z and NaNs/infinities/epsilon edges for vector comparison. Checks cover
fixture memory, defined return bits, x87 float returns, font-call arguments, stack cleanup,
callee-saved registers and unchanged precision. Fingerprints/results remain local in
`build/analysis/feng-coordinates/verification.json`. Five mutations are rejected: reversed
half-size subtraction, updating multiple matching tracks, wrong position offset, changed
justification priority and an extra trailing line. **Byte matching is not established.**

The native build, pause verifier (11,723), options verifier (5,730), freeroam HUD verifier
(15,180), all 19 settings scenarios with slider pixel checks and all 10 Controls scenarios
pass. These smoke checks establish bounded integration separately from differential execution.

## Scroller placement and cubic tween

`Frontend/IconScrollerLayout.cpp` recovers nine routines:

| Routine | Address | Behavior |
|---|---|---|
| End-slope / acceleration limits | `0x5748c0` / `0x574910` | Retain coefficient arithmetic and observed float stores |
| Tween update | `0x574e30` | States 0/1/2, restart mode, cubic coefficients/evaluation and end-state handling |
| Edge fade factor | `0x50d8d0` | Window/edge comparisons and fractional fades |
| Window visibility | `0x50d9c0` | Includes both endpoints; delegates to recovered show/hide |
| Icon alpha | `0x51fd60` | Current RGB and icon alpha; disabled/not-allowed base alpha 150; float-to-int truncation |
| Icon placement | `0x52f8c0` | Full size and centre, then unselected size 0.8 and top-left alignment; optional alpha |
| Layout reset | `0x538a30` | Snap tween, place using old selection, then select and update scrollbar |
| Frame layout | `0x538940` | Relayout clock/target/arrow updates, lock scripts, place all icons, then advance tween |

`ScrollerTween` describes its original 0x30 bytes. It occupies scroller `+0x90`,
with states/mode `+0xbc/+0xbe`. The acceleration limiter stores twice-coefficient and
start acceleration as single precision at original `0x574925`/`0x574942`; preserving
those rounding points resolves a one-ULP mismatch. Subsequent scaled start acceleration
remains extended until the coefficient stores. End-slope calculation retains inverse-duration
multiplication rather than substituting one. No derivative coefficients or algorithms are invented.

Original placement alignment 0/1/2 is top/centre/bottom for smaller image icons. The old
native seam incorrectly described alignment 1 as bottom-aligned. Frame placement uses the
current tween value and updates the tween **after** positioning, so the rendered transition
retains the original frame ordering. Reset performs its selection callback **after** placement.

The native menu now calls recovered reset/frame/placement code; its old direct layout loop
is removed. The platform frame delta is supplied at `0x865198`, and the original scroller
clock is bound at `0x8641a0`. Native construction and icon insertion still supply initial
state, geometry and bindings and remain seams. Colour read/write and lock policy are now
recovered as described below. Scrollbar update `0x538080` remains a controlled interface and native no-op.
Arrow policy is now recovered as described below. This is not complete IconScroller or FNG-base recovery.

`build/venv/bin/python tools/verify_pc_icon_scroller_layout.py`: **14,762 original/rebuilt
comparisons**. Tween cases run with 53-/64-bit x87 precision and cover states/modes, limits,
completion thresholds, zero/negative/infinite/NaN duration and arithmetic order. Placement
cases cover empty/null frame paths, every alignment, first/current/other selection, reset
indices, window edges, disabled alpha policy and fade flags. Actual recovered coordinate,
state-write, visibility, colour and lock-policy routines execute on both sides; selection,
formatted hash, script activation and scrollbar callbacks remain controlled and
traced, including tween state at callbacks. The original list
helper and `0x75bed4` conversion runtime are retained dependencies. Placement fixtures are
image objects, not proof of arbitrary font/group layout. Memory, global clock, callback
order/arguments, cleanup, preserved registers and precision are checked. Five mutations are
rejected: exclusive visibility edge, wrong disabled alpha, wrong alignment, changed cubic
coefficient and moving tween advancement before placement. Fingerprints/results remain
in ignored `build/analysis/icon-scroller-layout/verification.json`. **Byte matching is
not established.**

Native smoke: `python tools/smoke_native_pause_scroller.py` samples the Options graphic
at frames 53/60/75 after Right at frame 50, observing motion toward the fixed selection
frame and settled centring. The green selection frame itself stays fixed. Captures/logs
stay in `build/analysis/native-pause-scroller/`; a settled screenshot is at
`build/analysis/icon-scroller-layout/pause-options-icon.png`. The existing pause/options,
navigation and selection verifiers pass, as do all 19 native settings scenarios.

Finite scripted UI captures now ignore live keyboard/mouse/controller UI events and controller
repeat, while still accepting their explicit scripted device and pointer actions. This platform
fixture change prevents desktop pointer motion from selecting a different Controls row during
keyboard/gamepad smoke sequences; normal interactive input is unchanged.

After isolating finite scripted UI input, all 10 native Controls scenarios and all 19
settings scenarios pass, including explicit mouse/gamepad fixture actions. The rendered
Options graphic centres are 869.0, 758.5 and 640.0 pixels at the three sampled frames.


## FEng colours and icon lock scripts

`Frontend/FEngColour.cpp` and `Frontend/IconScrollerScripts.cpp` recover eight routines:

| Routine | Address | Behaviour |
|---|---|---|
| Pack colour | `0x5054d0` | Clamp signed R/G/B/A channels to 0–255 and pack ARGB |
| Copy object colour | `0x50ccc0` | Read state channels, or zero channels for a null object |
| Get packed colour | `0x495570` | Copy then pack |
| Colour proximity | `0x504640` | Signed comparisons against wrapping ±1 bounds |
| Write colour state | `0x517ad0` | Update first matching track in each script, then live state |
| Set colour | `0x51d7f0` | Gate on type < 0x100; mark dirty outside tolerance; always write |
| Script active | `0x51d040` | Hash even for null object; current hash and time ≤ length |
| Icon lock policy | `0x51df00` | Select lock SHOW/HIDE and button HIDE/FadeIn/Animate policy |

The state writer preserves modulo-32-bit additions and observed channel load/store order,
including aliased input. Matching tracks clear flag 0x80 and stop the search within that script.
The colour setter's tolerance only gates the dirty flag; it does not suppress the state write.
Icon byte +0x22 selects SHOW lock/HIDE button. Otherwise +0x23 enables button FadeIn,
unless the current FadeIn or Animate is still active. Script time equal to length remains active.
Unknown flag bytes retain offset-based storage.

`build/venv/bin/python tools/verify_pc_feng_colour_scripts.py`: **29,200 original/rebuilt
comparisons**, checking fixture memory, call traces, arguments, cleanup, preserved registers
and return values. Cases include signed extrema, clamp/tolerance edges, null objects,
type gates, multiple scripts and repeated matching tracks, additive/aliased writes,
noncanonical flag bytes and script expiry boundaries. Five source mutations are rejected:
wrong clamp, widened tolerance, processing later matching tracks, exclusive expiry and wrong
lock flag byte. Formatted hash `0x50b9c0` and script activation `0x51cf70` are controlled
interfaces; previously verified state-offset `0x5046f0` executes as a retained dependency.
Fingerprints/results remain in ignored `build/analysis/feng-colour-scripts/verification.json`.
**Finite behavioural verification passes; byte matching is not established.**

Native FEng colour writes now use the recovered setter, including script baseline updates.
Native construction binds the retail LOCK and BUTTON_MASTER objects so recovered lock policy
can run. Construction, formatted hashing, script activation/playback and scrollbar updates
remain seams. The layout verifier passes its 14,762 comparisons with actual recovered colour
and lock-policy routines linked in. Native build, pause/options regression verifiers,
19 settings scenarios, 10 Controls scenarios and rendered scroller motion pass.


## Scroller arrow scripts and endpoint visibility

`IconScrollerScripts.cpp` now also recovers `IconScroller4C::UpdateArrows` (`0x52fa70`),
`ScrollerScrollBar::SetArrow2Static` (`0x527660`) and `SetArrow1Blink` (`0x495810`).
The scrollbar exposes its two original arrow pointers at +0x48/+0x4c (scroller +0x80/+0x84);
its remaining unknown fields stay offset-based.

Directions 1/-10 request Static on arrow 1, then Blink on arrow 2. Directions -1/10
call the original arrow-2 Static helper, then arrow-1 Blink helper. Other directions do
not request scripts. Null arrows skip hashing. Matching current scripts are active through
time equal to length, and only absent, mismatched or expired scripts restart. The Static
helper directly hashes/tests state; the Blink helper calls recovered `0x51d040`, preserving
that original call boundary.

After script policy, the routine compares current icon with first, then last. At first it
hides arrow 1 and returns; at last it hides arrow 2 and returns. Those early returns leave
the other arrow's visibility unchanged. At neither endpoint it shows both. Null links map
to null icons; an empty intrusive list uses its sentinel instead. Native construction binds
`ScrollBar_Arrow_1`/`ScrollBar_Arrow_2` by their original constructor names, and the arrow
no-op is removed. Full scrollbar/scroller construction and geometry remain unrecovered.

The expanded `verify_pc_feng_colour_scripts.py` passes **39,700 comparisons**, including
**10,500 new arrow/helper comparisons**. Cases cover all four directional requests and
other directions, null arrows/current scripts, hash mismatches, inclusive expiry, signed
times, first/last/neither/null/sentinel selection and pre-existing visibility flags. Actual
recovered visibility routines execute on both sides; object memory, visibility global,
script/hash traces, arguments, cleanup and preserved registers are compared. Five arrow
mutations are rejected: wrong direction, wrong helper arrow, exclusive expiry, wrong
endpoint visibility, and removing the first endpoint's early return. Formatted hashing
and script activation remain controlled boundaries. **Byte matching is not established.**

The layout verifier's 14,762 comparisons now execute actual recovered arrow code along
with colour/lock/coordinate/visibility code. Native build, all 19 settings scenarios,
10 Controls scenarios and rendered icon motion pass. These finite smoke checks establish
integration for their exercised paths, not complete FEng animation or menu recovery.
