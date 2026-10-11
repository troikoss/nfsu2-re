# PC pause menu recovery

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Ghidra decompilation was checked against x86 disassembly and original/rebuilt
execution. This pass covers `UI_Pause.fng` and its option items. It **does not
recover the IconScroller base, FEng, the in-game dialog, the options screens
(`UI_PauseOptionsMain.fng`), or the game-flow states the menu queues**.

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

The source is not part of the native demo build (no CMake glob), so no
`OriginalSymbols.ld` entries were added.

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

FEng (`0x505450`, `0x537a10`, `0x50c900`, `0x50b8a0`/`0x50b8c0`, `0x50cea0`,
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

- Pause options screens: `0x4a7d60`, `0x4a7f70`, `0x4a8010`, constructors
  `0x4eb580`/`0x4d93c0`, factories `0x4f47f0`/`0x4eb650`.
- IconScroller base, in-game dialog `0x558790`, and the game-flow handling of the
  queued messages.
- Native integration requires FEng package/script scaffolding that the demo
  does not have yet.
