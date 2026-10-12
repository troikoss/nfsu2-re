# Track position markers and the start grid

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.

## Recovered routines

`src/Speed/Indep/Src/World/TrackPositionMarkers.cpp` recovers:

- Marker chunk `0x34146` loader `0x5d8470` and unloader `0x5d84d0` (registered by
  `0x77e720`). The track loader `0x57f2d0` reads `Tracks\Routes<region>\TrackPosMarkersFreeRoam.bin`
  when the track number is a multiple of 1000, otherwise `TrackPosMarkersAll.bin`. Markers are
  0x30 bytes, 16-byte aligned after the chunk header, and are linked into the list at `0x88f740`
  (static initializer `0x77e700`): `+8` name hash, `+0xc` value (grid slot for starting
  positions), `+0x10` position, `+0x20` 16-bit heading, `+0x24` track number.
- Queries: count `0x5d8520` and indexed lookup `0x5d85d0` by (track, hash), where track 0
  matches any track, and the current-track wrappers `0x5d8560`/`0x5d8620` plus the name forms
  `0x5d8590`/`0x5d8660` (track from `*0x883dac` `+0x8a`).
- Start grid `0x5eb5d0`: with at least `count` `ForwardStartingPosition` markers
  (`BackwardStartingPosition` when `0x89e7a4` is set) each slot takes the marker whose value is
  the slot. If any slot is missing, every slot is laid out instead along the road from a
  `WRoadNav` (constructor `0x5da2f0` recovered; `0x5da370` needs the road network and is a
  native trap): 100 units ahead in steps of 10, rows 17 apart, alternating 3 units left/right.
  Each position then takes the ground height from a temporary collider (constructor
  `0x5e0b40`, `0x5e68d0`, `0x5e7e70`, destructor `0x5e0bf0`, now recovered).

The world start `0x60a680` calls `0x6065b0`, which places each participant with grid index
1..6 at the matching entry of a six-entry grid. Only career free roam (`0x500b60`) relocates
the player afterwards (`0x5412d0`).

## Verification

`tools/verify_pc_track_markers.py` compares the original and the MSVC 7.1 rebuild
(**61,860 comparisons pass; byte matching is not established**): loader/unloader on
misaligned chunks with existing list contents, every query form over random lists, the
navigator constructor, the grid builder with complete and incomplete marker sets
(collider and navigator calls traced with their point values, scripted pack presence and
ground heights), and the collider constructor/destructor on linked lists. The grid's
`fs:[0]` frame runs with a flat GDT. Mutations of the slot match, track wildcard,
fallback side, pack test, loader count and destructor unlink are detected.

| Routine | Address | Bytes | Cases | SHA-256 |
|---|---:|---:|---:|---|
| `LoadTrackPositionMarkers` | 0x5d8470 | 87 | 400 | `71195cbe43c050b4351cac772e52907547ab953b600e50c10834bb1927c02d1e` |
| `UnloadTrackPositionMarkers` | 0x5d84d0 | 77 | 400 | `1d5fee6b495b531cf3ad0ec5fa5bcce29ae65522bf81d138008fa9f0803fca19` |
| `CountTrackPositionMarkers@@YAHHI@Z` | 0x5d8520 | 58 | 4500 | `96f51f51c5dc050197966802e539840cc23837f9b02cd23ffaf37db88cb75b3c` |
| `GetTrackPositionMarker@@YAPAUTrackPositionMarker@@HIH@Z` | 0x5d85d0 | 74 | 22500 | `e6e2f0dd2210d4c1981fc0eb03ceac8b9d998c4649bc2d2b7ce3baa30b58e5e8` |
| `CountTrackPositionMarkers@@YAHI@Z` | 0x5d8560 | 48 | 4500 | `295f09816d95e4a41f55b60bde758e2090220ea0047c46d375db95ce6c6fec12` |
| `CountTrackPositionMarkers@@YAHPBD@Z` | 0x5d8590 | 54 | 1500 | `1d6e4189d864b7d35ca95646251ff4bd77f398f7d1e6ee03bc4e9b13ceeb2a84` |
| `GetTrackPositionMarker@@YAPAUTrackPositionMarker@@IH@Z` | 0x5d8620 | 58 | 22500 | `584fc8221de6eaee3e88771fe0e69871f001c1c47b1f6f054ea628102952f380` |
| `GetTrackPositionMarker@@YAPAUTrackPositionMarker@@PBDH@Z` | 0x5d8660 | 64 | 1500 | `32a928c8ad4698f7d76dcebc39a56ddde8ace4b0892b604bccb7eaa15356fcb5` |
| `Construct@WRoadNav` | 0x5da2f0 | 53 | 50 | `e118b1f46ef43005a51acdfcf895beb140ada033ea15e620c3aac39ccd5923ab` |
| `GetStartingPositions` | 0x5eb5d0 | 602 | 3000 | `24e9b42af386603090e65518514e591e5174f12c72adc143b7df0bcfd7e8f8cd` |
| `Construct@WCollider` | 0x5e0b40 | 167 | 600 | `d3d98db0ce3c48a0a19fc54b9da7001ea04c5dceff8aa8a522a8221f4e3ebdc4` |
| `Destruct@WCollider` | 0x5e0bf0 | 72 | 410 | `0a52b7547b744c420a8d13612a47fd5fcf5cfd2a9d7f0d01c285052723cce575` |
| `Release@WCollider` | 0x5dc9c0 | 105 | 0 | `ffa829af2fa766379d8ab8b7c99fdcfd3768fe2a8c5379a218c8665585b6d5d5` |
| `ReleaseArea@WCollider` | 0x5d80a0 | 72 | 0 | `9579519f19d0836bba1da19d54015073245f156d7ed90e6a67063054f44d9f37` |

## Native integration

`NativeWorld.cpp` loads `TRACKS/ROUTESL4RA/TrackPosMarkersFreeRoam.bin` through the recovered
loader, publishes TrackInfo 4000 at `0x883dac` when it is loaded, and builds the six-entry grid
with `0x5eb5d0`. Native session seam: the demo's single human participant has grid index 1 and
takes the first entry (L4RA: 1695.2, -883.6, ground 23.8, heading `0xf8be`). This replaces the
former invented "flat road polygon near the map centre" spawn. The car constructor `0x604260`
and participant setup remain native.
