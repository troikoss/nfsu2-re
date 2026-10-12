# Collision notifications, profile readers and player camera option

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.

Recovered, replacing silent native stubs:

- `Physics/Collision/CollisionNotify.cpp`: the audio system's collision wrappers `0x483e20`
  (register) and `0x483e40` (unregister), called on `*0x82b884` by collision events
  `0x5a9430`/`0x5a9570`. Both forward to the collision sound set at `+0x3c` (`0x481cd0`/
  `0x481e50`, not recovered) only while the audio flag `0x7faaa8` is set. `0x580130` clears
  that flag when no sound device exists; the native runtime has no sound device and stores 0,
  so the sound set is never reached (it is a native trap). The game message post `0x5e0360`
  allocates a cleared 0x24-byte message from the pool `*0x883e38` (`bOMalloc` `0x440e50`),
  stores the type at `+0xc`, appends it to the queue `*0x800ecc` and counts it at `0x876dc8`.
  It is reached only for contacts between two cars, which the one-car demo does not have.
- `Frontend/ProfileQueries.cpp`: force-feedback presence `0x5b8240` (`0x87078c`), the profile
  settings byte of the current mode `0x5026f0` (mode 12 reads `+0x100`, other modes the first
  byte of a 0x14-byte row from `+0x10`) and the memory-card byte `0x502fe0` (`+0x20`, returned
  unconverted). The pause options now call the original career query `0x500b60` on
  `0x85ad40` instead of a native `false` stub.

- `Frontend/PlayerCamera.cpp`: the player's camera option step `0x5f2fa0` (called by the pause
  options on the pause menu's view of the player record) and the option cycler `0x502540`
  (four choices, five in drift sessions and on tracks `0x104e`..`0x1053`). The new camera type
  (`0x502500`) is passed to the camera mover's slot `+0x20` when the car's CarTypeInfo (car
  `+0xc`) has a setting for it (`0x441140` at `+0x720`, view mode from `0x5b98a0`). This replaces
  the native camera-cycling stand-in.

`tools/verify_pc_small_queries.py` compares original and MSVC 7.1 rebuilds (**18,000
comparisons pass; byte matching is not established**): enable-flag values, sound-set calls
traced by receiver and argument, camera options across steps, drift flags, track numbers and
view modes (table lookup and camera slot traced), message pools with zero to two free slots and clear-on-
allocate flags, queue links and count, and the reader values. Mutations of the flag test,
queue link and count are detected. `verify_pc_pause_options.py` passes with the method call.

| Routine | Address | Bytes | Cases | SHA-256 |
|---|---:|---:|---:|---|
| `RegisterCollision@Audio82B884` | 0x483e20 | 20 | 2000 | `2d493f4b135dd90641c4d8e3c021d67e1455c3c0384ce53d849e730a7f71ef7b` |
| `UnregisterCollision@Audio82B884` | 0x483e40 | 20 | 2000 | `70e5cc66648e22f1e0e5d825364bb48f93a6bbbdcb3f66707ca72497654cd901` |
| `PostGameMessage` | 0x5e0360 | 71 | 2000 | `bce5932313927b70276b663ee0d400fbef8f694e635639a939554c020c228163` |
| `Value5B8240` | 0x5b8240 | 6 | 2000 | `598838c13ad237aa4cc456ec4a63ef8e36d78cd79653bd5cacac994f45736eab` |
| `Value5026F0@Settings83AAB4` | 0x5026f0 | 24 | 2000 | `286b19c9f1d8740214aa8e5586c028565060755086bfb4b19f4ec3a1cd555920` |
| `Value502FE0@MemoryCard83A9E0` | 0x502fe0 | 4 | 2000 | `e2716ac6dcee970cd16c7522928d79654211f8657ccbbba4bc89f0a7ceb4c3c2` |
| `CycleCamera@PlayerSettings83AA64` | 0x502540 | 97 | 3000 | `d4eb79259ff258db087fc4e6c011850dbb8b357418352e4ebad32e66632fbeee` |
| `Value5F2FA0@Player8900AC` | 0x5f2fa0 | 98 | 3000 | `03e7a94c557f3c75da0809229a4fa925785bd90484cc9ca8ad9ea61aaeb953fe` |
