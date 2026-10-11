# PC Controls UI and native device input

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.

`Frontend/PCControls.cpp` recovers the 36 keyboard binding items' `React` and
`SetLabels` methods: 72 routines at `0x4acfe0` through `0x4adae0`.
`config/SPEED2_EXE_1_2/pc-controls.json` records individual addresses, lengths,
fingerprints, item vtables, action IDs, binding columns and action-name hashes.
The row order and two-column association follow original keyboard list builder
`0x4dba50`. `React` invokes device `0x874c40`'s capture method `0x5c2100` with the
original action and column, then refreshes labels through virtual slot `+0x0c`.
Label routines call `0x4acf40` with their label pointer and original arguments.
No replacement driving algorithm is introduced.

Ghidra identified the screen factory `0x4f92e0`, constructor `0x4ecd10`, message
handler `0x4f9340`, device discovery `0x4ace00`, device cycling `0x4acaa0`, and
row association/lookup `0x4cb4b0` / `0x4acca0`. These larger routines are analysis
boundaries, **not fully recovered source**. Factory registration is at
`0x7f7e54`; screen vtable is `0x797de4`. Original name/data masters and numbered
objects determine the native list geometry. Full screen/vtable equivalence,
DirectInput, profile settings, controller/wheel binding lists, duplicate-binding
rules, default restoration and persistence remain unresolved.

## Verification

```sh
build/venv/bin/python tools/verify_pc_controls.py
cmake --build build/native
python tools/smoke_native_controls.py
python tools/smoke_native_pause_options.py
```

On 2026-10-11, the controls verifier passed **720 comparisons over 72 routines**.
It compiles the recovered source with MSVC 7.1 and compares original/rebuilt
Unicorn execution, including interface arguments/order, fixture memory,
callee-saved registers and stack cleanup. It checks four label pointers and
four React messages. Capture, label formatting and virtual label refresh are
controlled interfaces, including caller-saved register clobbering. Results are
in ignored `build/analysis/controls/verification.json`.

**Byte matching is not established.** These finite comparisons do not verify
the complete screen or backend.

## Native platform seams

`NativeControls.cpp` displays the player's `UI_Options_PC_Controller.fng` from
`FRONTEND/FrontB.lzc`. It owns list selection, scrolling, binding capture and
session SDL scancode storage, and supplies the capture/label call boundaries
for recovered binding items. It reuses the retail 18 action names and primary /
secondary columns. Storage permits duplicate keys; it does not implement the
original conflict policy. Initial keys preserve the demo's previous controls,
not the player's saved retail profile. Unsupported actions remain unbound and
editing them does not add the missing gameplay systems.

Steering, throttle, reverse/brake, handbrake, look-back, camera, vehicle reset
and pause consume the session mappings in `DemoMain.cpp`. Transmission/NOS,
world-map, race/shop entry, SMS, career statistics and hydraulics still lack
native gameplay consumers. The demo's existing steering smoothing remains
platform scaffolding; complete original car input processing is not recovered.

Open Controls from the added **Player → Controls** row, or press **F2** while
paused. This extra row/shortcut is a native entry seam because the original
frontend Options menu is unavailable in the driving demo. Arrows select rows
and columns; Enter / A begins capture. Tab, either shoulder button, or clicking
the device arrows switches Keyboard / Gamepad. Escape cancels capture or returns
to the parent page. Gamepad capture accepts buttons and signed axes; discrete
camera/reset/pause bindings accept buttons. Both bindings work until the demo exits.

`NativeUiInput.h` provides SDL platform device handling:

- First supported gamepad, startup discovery, hotplug and disconnect cleanup.
- Start opens pause; D-pad / left stick navigate; A accepts, B returns,
  Y opens Controls. Directions repeat after 350 ms, then every 110 ms.
- Mouse click selects/activates icons, hover selects option rows, and clicks
  edit buttons/toggles. Slider track clicks and left-button dragging map the
  pointer position to recovered left/right messages; arrow clicks still step.
  Click either binding column in Controls to capture the selected device.
  Visible settings page arrows also scroll.
- Wheel moves rows; right-click returns/cancels. Focus loss suppresses device
  input. Pointer coordinates use window height for the centred 640×480 FEng
  space, including wide windows and drawable/window size differences.

Gamepads also drive the existing demo vehicle. `NativeGamepad.h` supplies a
separate session SDL profile: left stick steers, right/left triggers throttle
and brake/reverse, A is handbrake, right-stick press looks back, Y changes camera,
Back resets the car, and Start pauses. Signed stick axes have a 15% deadzone,
triggers 5%, with the remaining range normalized to [0,1]. Keyboard and pad
inputs combine using the greater pedal value and clamped steering. Focus loss
and disconnect suppress pad driving. The original DirectInput processing,
profiles, controller defaults and device list construction remain unrecovered;
this does not claim retail-equivalent input. NOS, manual shifts, map/SMS/shop,
career and hydraulics still lack demo consumers even if bindings are assigned.
Image hit regions use the renderer's composed transforms; text/list regions
use native row geometry. General FEng mouse dispatch, menu audio, full scroll
animation/scrollbar, and original pointer behavior remain seams.

An isolated SDL virtual-controller check exercises hotplug, Start/A/B/Y,
D-pad repeat, stick, analog driving/deadzones, secondary bindings, focus loss
and mouse conversion/click/drag/wheel:

```sh
g++ -m32 -std=c++11 -I src/Platform/Native/include tools/smoke_native_ui_input.cpp \
    -L/usr/lib/i386-linux-gnu -l:libSDL2-2.0.so.0 -o build/analysis/smoke-native-ui-input
SDL_VIDEODRIVER=dummy build/analysis/smoke-native-ui-input
```

This passed on 2026-10-11. It selects the virtual fixture explicitly so existing
user gamepads cannot intercept its events. It verifies native device routing,
not physical-controller mappings or original input behavior. Ten native Controls scenarios passed (keyboard primary/secondary capture,
pad axis/button capture, both device cancellations, mouse capture, end-of-list
scrolling, parent return and Player entry). The
existing settings suite also passed all 19 scenarios (including slider click/drag) and rendered slider-fill
checks after adapting its bottom-row navigation to the added Controls entry.
Rendered screens/logs stay in ignored `build/analysis/native-controls/`.
