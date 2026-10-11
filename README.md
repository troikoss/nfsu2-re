# NFSU2 RE

An unofficial source reconstruction and verification project for **Need for Speed
Underground 2, PC v1.2**. The goal is complete recovery into verified,
reproducibly buildable source. The current Linux driving demo is an intermediate
milestone; this is an incomplete decompilation.

## You must provide the game files

**Game assets and the retail executable are not supplied by this project. You
must provide them from your own lawfully obtained PC installation of Need for
Speed Underground 2.** Cloning this repository does not provide a copy of the game.

The demo reads car models, track data, textures, HUD packages, and other required
files from your installation. It also requires the supported PC v1.2
`speed2.exe`: the current runtime checks its SHA-256 and maps reference data
from it. No original instructions execute in the native demo. Keep the game's
directory structure intact and pass its installation root with `--game`.
Both build variants below require these files.

Do not commit or distribute retail assets, executables, compiler binaries, or
generated disassembly with this project. Local `build/`, `orig/`, and `local/`
directories are ignored. Game files remain the property of their respective
rights holders; this repository grants no license to them.

## Build without the mod loader

The native targets require a multilib C++ compiler and i386 SDL2/OpenGL
libraries and development headers. Native scaffolding uses C++11. The current
build includes host-specific Linux include/library paths; see
[the native demo notes](docs/NATIVE_DEMO.md) for setup and limitations.
These commands build the current demo and analysis executables, not a completed
reconstruction of the entire game.

```sh
cmake -S . -B build/native -DUG2_ENABLE_MODDING=OFF
cmake --build build/native
build/native/ug2demo --game "/path/to/Need for Speed Underground 2" --car SKYLINE
```

`UG2_ENABLE_MODDING=OFF` is the default. This build does not include the native
mod loader or its example plugins.

## Build with the mod loader

Use a separate build directory to keep both variants available:

```sh
cmake -S . -B build/modding -DUG2_ENABLE_MODDING=ON
cmake --build build/modding
build/modding/ug2demo --game "/path/to/Need for Speed Underground 2" --car SKYLINE --width 1920 --height 1080
```

This builds the native loader and example plugins. Plugins and their
`load_order.txt` are placed in `build/modding/mods`. The supplied order enables
`original_physics` (which forwards to the recovered solver) and `widescreen_hud`.
By default, the loader looks for `mods` beside the executable. Edit
`load_order.txt` to select active plugins, or choose another directory:

```sh
build/modding/ug2demo --game "/path/to/Need for Speed Underground 2" --mods-dir "/path/to/mods" --car SKYLINE
```

To run the loader-enabled build with all mods disabled:

```sh
build/modding/ug2demo --game "/path/to/Need for Speed Underground 2" --no-mods --car SKYLINE
```

Optional installation of the hosts, SDK, and examples:

```sh
cmake --install build/modding --prefix "/path/to/install"
```

Installation does not copy game files. See [the modding guide](modding/README.md)
for plugin layout, SDK rules, load order, and verification instructions.

## How to mod the demo

The current plugin API targets the rebuilt 32-bit Linux demo. It does not load
these plugins into the retail Windows game. Native plugins run inside the host
process without a sandbox; use plugins you trust.

### Install and enable plugins

Build with `UG2_ENABLE_MODDING=ON`, then place each plugin in its own folder
under the executable's `mods` directory:

```text
build/modding/mods/
    load_order.txt
    original_physics/plugin.so
    widescreen_hud/plugin.so
    my_mod/plugin.so
```

Add one plugin ID per line in `load_order.txt`, in the order you want them loaded:

```text
original_physics
widescreen_hud
my_mod
```

The folder name must match the plugin's descriptor ID. IDs accept ASCII letters,
digits, `_`, and `-`, up to 80 characters. Blank lines and `#` comments are
accepted. Remove or comment out a line to disable that plugin; unlisted folders
are inactive. Restart the demo after changing plugins or load order. A missing
order file runs without mods. Libraries must be 32-bit ELF shared objects.

The bundled `widescreen_hud` plugin adjusts driving HUD anchors and horizontal
field of view for wide screens. The bundled `original_physics` plugin delegates
to the recovered solver. Callbacks run in load order; the last successfully
initialized physics provider wins. Use `--no-mods` to disable all plugins.

### Create a plugin

Use [the SDK header](modding/sdk/ug2_mod_api.h) and start from
[the HUD example](modding/examples/widescreen_hud/plugin.cpp) for presentation
changes or [the forwarding physics example](modding/examples/original_physics/plugin.cpp)
for physics-provider integration.

1. Copy an example into your own `plugin.cpp` and change its descriptor ID to
   `my_mod`. Keep its exported `ug2_mod_query` entry point, C linkage, and
   `UG2_CALL`/`UG2_EXPORT` declarations.
2. Register callbacks or a provider through the host API in initialization.
   Use the documented API rather than modifying recovered layouts directly.
3. Compile from the repository root with the SDK include path:

   ```sh
   mkdir -p build/modding/mods/my_mod
   c++ -std=c++11 -m32 -fPIC -shared -fvisibility=hidden \
       -I modding/sdk /path/to/my_mod/plugin.cpp \
       -o build/modding/mods/my_mod/plugin.so
   ```

4. Add `my_mod` to `build/modding/mods/load_order.txt`, then launch the
   loader-enabled demo with your game installation using the command above.

Keep descriptor and callback storage alive until unload, match the SDK API
version and structure sizes, and do not let C++ exceptions cross API calls.
Do not link recovered game libraries into plugins. Render callbacks must
restore graphics state. The stable API supports HUD translation, projection
adjustments, lifecycle callbacks, and physics-provider integration; an in-game
mod manager, Lua support, hot reload, and dependency resolution are not yet
implemented. See [the full SDK guide](modding/README.md) for callback timing,
ownership rules, physics contracts, and validation commands.

## Demo controls and status

The demo drives a stock car on the L4RA streets with recovered vehicle,
collision, camera, and HUD routines. SDL2 window/input handling, OpenGL drawing,
and documented integration seams are platform scaffolding.

Controls: W/S throttle and brake, hold S at rest to reverse, A/D steering,
Space handbrake, R reset, C cycle camera, hold V to look back, and Esc quit.
See [native demo notes](docs/NATIVE_DEMO.md) for options and known limitations.

The default game directory is `Need for Speed Underground 2` relative to the
working directory. `UG2_EXE`, where supported by analysis tools, selects the
reference executable for verification; the demo uses `--game` instead.

Some functions byte-match the reference executable; others have bounded
original/rebuilt execution comparisons and remain nonmatching. Many routines
and integration paths still require verification. A playable demo or the
presence of a source file does not establish complete recovery or equivalence.
Behavioral verification and byte matching are reported separately.
Start with [the documentation index](docs/README.md) and
[the decompilation scope](docs/DECOMPILATION_SCOPE.md).

## Local verification

```sh
python -m venv build/venv
build/venv/bin/pip install -r requirements-pc-analysis.txt
UG2_EXE="/path/to/Need for Speed Underground 2/speed2.exe" build/venv/bin/python tools/verify_pc_wheel_math.py
```

Verification also requires locally supplied MSVC 7.1 and wibo; string matching
uses delink and objdiff. These tools and compiler binaries are not bundled.
Run MSVC/wibo jobs sequentially because they share temporary filenames.
See the subsystem documentation for verifier-specific setup and limitations.

## Repository layout

- `src/Speed/Indep/`: recovered game routines and explicit original layouts.
- `src/Platform/Native/`: Linux demo and compatibility scaffolding.
- `tools/`: analysis, investigation, and verification tools.
- `config/SPEED2_EXE_1_2/`: reference manifests and fingerprints.
- `docs/`: subsystem evidence, limitations, and demo instructions.
- `modding/`: optional native plugin runtime, SDK, and examples.

## Licensing and attribution

Original project contributions are offered under the MIT terms in
[LICENSE](LICENSE), subject to its scope and exclusions. **That grant does not
cover recovered UG2 code, reconstructed game data, retail assets, or material
owned by third parties.** This repository is not licensed as a whole under MIT.
No permission from the game's rights holders to redistribute recovered code
is represented or implied. These notices do not establish that publication or
use of recovered code is legally permitted.

Third-party components retain their own terms. See [NOTICE](NOTICE) and the
notices embedded in individual files. Preserve those notices when distributing
applicable material.

This independent repository was created on 2026-10-11 with new Git history.
Research previously used the `cobanov/nfsu2-decomp` checkout; migration input
fingerprints and the baseline are recorded in [PROVENANCE.json](PROVENANCE.json).
That provenance does not establish ownership or grant licensing rights.

This project is not affiliated with, sponsored by, or endorsed by Electronic
Arts. Need for Speed and other game names and marks belong to their respective
owners and are used here to identify the subject of the research.
