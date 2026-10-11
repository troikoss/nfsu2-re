# PC HUD minimap recovery

The native city demo now loads TrackInfo chunk `0x34201` from `GLOBAL/GLOBALB.BUN`,
retains its backing storage, and displays `TRACKS/TRACKMAP4000.BIN` through the
retail `HUD_SingleRace.fng` objects. The recovered update `0x4ca840` centres its
UV window on player 0, rotates the map/mask with the exported car heading, rotates
the north marker, and positions the player indicator. Native setup selects city
record 4000 and rotating free-roam mode 2; original profile/feature setup is not
recovered by this integration.

## Original evidence and helper verification

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Ghidra project analysis was cross-checked against the original x86 instructions.

| Routine | Address | Fingerprinted span (bytes) | Exact comparisons | SHA-256 |
| --- | --- | --- | --- | --- |
| `HudArcSineUnits` | `0x43c490` | 216 | 2468 | `e8bad2acff9eb89450d2632da31e859c2ce65e0cc50ca9b20159d74de38298a8` |
| `HudAngleUnits` | `0x43e6d0` | 326 | 4168 | `ae1a41c8e94daa3da152dd4309072465dd0befee482363f79703f959c5ae5ca2` |
| `LoadTrackInfoChunk` | `0x5d3eb0` | 55 | 20 | `bc890669cced61a27a6bfd4e7244f3e72d8f7b1af707a8ea9b691888ff57d282` |
| `FindTrackInfo` | `0x5d3e40` | 58 | 20 | `564ed7cf7202ddc05caf40b1be711110f129ad24dd7068cce1303ce28a40f4f9` |
| `GetPose` | `0x4ac2b0` | 427 | 1280 | `1289e294c6b7410573d5cbced9eb76456fdcdf1dd007f8ca066bb14b8f8d64db` |
| `FindIndicator` | `0x4ca800` | 64 | 16 | `960242773bbe788a50b55edf16996bb921f7926a428f3d59dedfe4edd9b4e8c9` |

The pose span includes its dispatch table; the list search span includes trailing
padding. These are contiguous fingerprints, not Ghidra's discontiguous body-size
counts. The cosine wrapper at `0x401100` passes a float to the sine routine
`0x43c350`. Keeping that call boundary resolves a cosine output difference under
64-bit x87 precision; the type-1/2 and player paths now call `HudCosineRadians`.
The other fixed-icon path adds the quarter turn directly, as the original does.

Reproduce sequentially with the existing local MSVC 7.1/wibo dependencies:

```sh
build/venv/bin/python tools/verify_pc_hud_minimap.py
build/venv/bin/python tools/verify_pc_hud_visibility.py
cmake --build build/native -j2
SDL_VIDEODRIVER=offscreen build/native/ug2demo --frames 120 --scripted \
  --screenshot build/analysis/minimap/minimap.ppm
```

The verifier passed **7,972 exact comparisons**: lookup-bin neighbors, axes,
quadrants, seeded direction inputs, signed/duplicate/missing TrackInfo numbers,
chunk acceptance/counts, pose types 1/2/4/5/18, and list searches including a null
slot. It checks low-16-bit angle returns, exact pose output bytes, TrackInfo global
writes, stack cleanup, preserved registers and both x87 control words `0x027f`
and `0x037f`. Generated evidence is in `build/analysis/minimap/verification.json`.
It retains the original conversion routine, arcsine lookup data and sine helpers;
it does not establish independence from the mapped retail image.

Native build and 120-frame scripted driving smoke passed. The inspected capture
shows the city roads, centred green player arrow, north marker, retail border and gauge cluster.
The screenshot stays in ignored `build/analysis/minimap/north-border.png` (converted
from the captured PPM for inspection). This is integration/visual evidence,
separate from the helper comparisons. **Byte matching is not established.**

## North marker and border integration

The FNG north object `0xf2a54430` stores position `(-223, 70)`, pivot `(0, 72)`,
and size `(32, 16)`. Its pivot therefore lies at the map centre `(-223, 142)`.
The native renderer now rotates about that stored pivot, applying
`position + pivot + R * (vertex - pivot)`. Previously it ignored the pivot and
spun the marker in place above the map. The recovered north rotation formula in
`0x4ca840` is unchanged. `Describe` now includes pivot and initial angle values.

The constructor `0x4f73e0` registers `TrackMapTargetRing` (`0x382d2fc9`) in the
minimap feature's object list and sets required masks `(1, 0)`. Native setup now
copies the seven core-map registrations in their original order, skipping absent
objects as the constructor does. It calls the recovered mask application
`0x4c6690`, first with `(0, 0)` as at constructor completion, then with `(1, 0)`
for native free drive. This replaces the previous texture-based border selection.
The routine saves both current masks and shows the list when either corresponding
required-mask intersection is nonzero; otherwise it hides the list.

Recovered FEng operations `0x50ca00` and `0x50ca50` preserve the original hidden
and dirty-bit conditions and recursively process type-5 groups. The native FNG
loader reconstructs the original child-chain prefix at offsets `+0x60/+0x64`,
so those routines show or hide all border children directly.

The retail border group contains four image children with texture
`MINIMAP_BORDER` (`0x92a75587`), offsets `(16, -16)`, `(-16, -16)`, `(16, 16)`,
`(-16, 16)`, and respective angles 90, 0, 180, -90 degrees.
No border geometry or textures are generated.

| Routine | Address | Fingerprinted span (bytes) | Exact comparisons | SHA-256 |
| --- | --- | --- | --- | --- |
| `HudHideObject` | `0x50ca00` | 69 | 1923 | `c515ccd4725590f00564e30a435694ac8cc63c13a7ec59aa811f3ed15d028e09` |
| `HudShowObject` | `0x50ca50` | 82 | 1923 | `064ab5d1f7e0cee0ff31fc7c60e66c4c0f20f49a61a4d912bb3bf6bf3cf52e90` |
| `ApplyMasks` | `0x4c6690` | 85 | 144 | `4f7974cceb4b5b6065684f787da5eb3a3295c3542359ac3689eddb28bc0e3ab6` |

The visibility verifier passed **3,990 exact comparisons**, with no stubbed
callees. It compares the whole object/feature arena, stack cleanup and preserved
registers across both mask words, high bits, dirty-global states, recursive
groups (including four border children), repeated transitions, empty lists,
null objects and nonpositive child counts. Report:
`build/analysis/minimap/visibility-verification.json`. These finite synthetic
cases establish bounded behavioral agreement; **byte matching is not established**.

The native build, `git diff --check`, and a 120-frame scripted offscreen capture
passed after these changes. The inspected capture shows the north arrow on the
left rim for the current heading, aligned with north on the rotated map, and
all four border pieces surrounding the map. The latest capture is
`build/analysis/minimap/original-visibility.png`. These are native rendering
checks, separate from the original/rebuilt comparisons.

## Platform seams and remaining recovery

- FEng position, size and UV setters remain native operations. Show/hide now
  execute recovered routines against the retained original object prefix.
- The full feature constructor `0x4f73e0`, its GPS/career/AI registration tail,
  list allocator `0x440ec0`, and player mask producer `0x600090` remain seams.
  Native free drive supplies mask bit 0; the recovered mask application and
  recursive FEng operations determine border visibility from that input.
- `0x4db900` registers existing `*INDICATOR_%d` objects, then `0x4ec730`
  takes an object of the requested type from the pool. The object suffix is not
  the player ID: this FNG contains `PLAYERCARINDICATOR_1` for the single native
  player. Native setup scans slots 0..7 and owns one intrusive-list node;
  original pools/allocation and the full constructor `0x4ec950` remain seams.
- TRACKMAP4000 is DXT3 with an all-zero decoded alpha plane and TextureInfo
  blend byte `+0x4a = 5`. The native renderer samples its RGB and uses the
  retail `Minimap_Mask` alpha silhouette as an OpenGL stencil. This is an
  explicit rendering substitute; original D3D blend-mode-5 handling and edge
  blending have not been recovered or compared. Ordinary FNG rendering state
  is restored after the map draw. The mask's soft alpha edge becomes a stencil
  threshold, so edge blending is approximate.
- AI pose/visibility/refresh (type 3), career icon visibility/refresh, GPS
  minimap integration, race/start/finish icon population and complete HUD feature
  mask production remain unresolved. Unsupported paths trap through `HudMinimapSeam`;
  native setup does not populate those types.
  The separate GPS arrow update `0x4c8410` and controller mask dispatcher
  `0x4ca090` now have bounded recovery evidence in [freeroam HUD](pc-hud-freeroam.md);
  their route/context producers remain unresolved.
- `0x4ca840` and minimap indicator visibility/refresh helpers are source recovery, not yet covered
  by original/rebuilt comparison. Next verification should compare the update's
  FEng call trace, UV/global writes, rotating/static modes, rim clamping and
  hide flags using controlled FEng interfaces. Finite helper cases do not prove
  complete minimap behavior, and the driving smoke does not prove D3D parity.
