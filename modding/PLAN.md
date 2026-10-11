# Modding framework plan: portable data and Lua, with native extensions

## Objective

Build an optional modding framework separate from recovered UG2 source. Most
mods should ship the same configuration and Lua files on every supported host.
Advanced physics and rendering extensions use a versioned native C SDK and
platform-specific binaries inside the same mod package.

The current host is the 32-bit Linux demo. Windows support is a later host port;
portable mod files do not imply that a Windows host already exists. Do not author
replacement game mechanics or a replacement physics algorithm. Expose recovered
behavior through documented native adapters, preserving original call boundaries,
arithmetic ordering, collision behavior, and fixed-size object layouts.

This plan extends the native-first plan. Lua and configuration mods become planned
deliverables rather than unspecified future additions. Their implementation is
phased around the availability of verified recovered data and consumers.

## Existing foundation and scope changes

The working tree contains the native SDK, optional runtime, ordered loader,
lifecycle/render/HUD/projection hooks, vehicle provider bridge, original-physics
forwarding example, and widescreen HUD/FOV example. `README.md` documents the
implemented interface and bounded validation evidence. This plan describes the
next architecture and completion criteria; it does not claim that the portable
layers below are implemented.

Retain these native features and their existing tests. Add:

- A versioned package manifest with explicit data, Lua, and native components.
- Declarative configuration overrides for verified fields.
- A restricted Lua API for events and supported gameplay operations.
- One installation and load-order mechanism for all three components.
- An eventual Windows host using the same portable contracts.

Keep hot reload, dependency resolution, an in-game manager, arbitrary memory
editing, portable compiled-code runtimes, and a replacement solver out of this
release sequence.

## Authoring layers

| Layer | Intended use | Distribution | Access |
| --- | --- | --- | --- |
| Configuration | Vehicle tuning, part/car values, and race definitions where recovered consumers are verified | Same JSON files on supported hosts | Validated schema and named records |
| Lua | Event-driven behavior, telemetry, HUD layout, and supported gameplay commands | Same Lua source on supported hosts | Versioned host bindings; no internal pointers |
| Native C/C++ | Physics providers, OpenGL rendering, and expert extensions | Separate binaries for each OS/architecture | Stable C SDK; optional exact-build expert table |

Shared source is not sufficient to make native binaries universal. A package may
contain both Linux and Windows binaries; the host selects one. Lua/data portability
requires matching API versions, supported capabilities, and compatible game data.
Platform-specific libraries, file paths, and OpenGL assumptions invalidate a
claim of portable scripting.

## Repository and build organization

Keep the framework in this repository under top-level `modding/`:

```text
modding/
    PLAN.md
    README.md
    sdk/                 # stable native C headers
    runtime/             # loader, ownership, ordering, lifecycle
    config/              # schemas, validation, override staging
    lua/                 # interpreter integration and stable bindings
    examples/            # native, configuration, Lua, and mixed packages
    tests/               # protocol fixtures and integration coverage
    docs/                # schema/API references and author guides
```

Game-specific data, HUD, projection, and collision adapters stay in
`src/Platform/Native/`. Do not insert hooks into recovered routines. Configuration
and Lua use the same underlying adapters as the stable native API; they must not
duplicate recovered mechanics.

Retain `UG2_ENABLE_MODDING=OFF` by default and a separate runtime target excluded
from recovered-code verification targets. Add `UG2_ENABLE_LUA`, default ON when
modding is enabled, allowing a native/data-only host build. Select and pin the Lua
version and JSON parser during implementation, documenting licensing and build
reproducibility. Avoid a required network fetch during ordinary builds.

Build native plugins as 32-bit PIC shared libraries with their own compiler
settings and SDK headers only. Install headers, schema/API documentation, examples,
and optional author tools. Players install packages afterward without rebuilding
the game. A separate SDK distribution can be published later without moving the
runtime out of the repository now.

## Package format and discovery

```text
mods/
    load_order.txt
    example_mod/
        manifest.json
        config/
            vehicles.json
            parts.json
        scripts/
            main.lua
        native/
            linux-x86/plugin.so
            windows-x86/plugin.dll
```

Manifest version 1 declares:

- Stable ID, display name, mod version, and manifest version.
- Portable API compatibility and required/optional capabilities.
- Ordered configuration file list and optional Lua entry point.
- Native entry points keyed by supported platform/architecture.
- Whether each component is required or optional.
- Any required game-data/reference identity for data-dependent operations.

Use explicit components; do not infer executable entry points by scanning folders.
Manifest ID must match the folder and order-file ID. Missing required components,
unsupported capabilities, or incompatible API versions skip the entire package.
Missing optional components produce a diagnostic and allow the declared remainder
to load. Never silently omit a required gameplay component on another platform.

Retain executable-relative `mods/`, `--mods-dir`, `--no-mods`, and the required
`load_order.txt`. All components obey that one order file. Only listed IDs load;
reject duplicate IDs and path-like IDs. Missing order files run without mods.
Invalid packages are skipped individually. Resolve component paths within their
package, rejecting traversal and symlink escapes; apply documented file-size and
entry-count limits before parsing or executing.

Preserve existing native packages without manifests as a documented legacy form:
`mods/<id>/plugin.so` on Linux. New manifests take precedence; do not load both
layouts. Migrate examples to manifests when the package loader is implemented.
The future GUI reads and writes the same order file.

## Startup, ordering, ownership, and failure policy

Load only at startup and unload at shutdown. Establish native globals and load
the relevant recovered records before applying configuration, then construct
vehicles/world objects after configuration commits.

For each package, in order:

1. Validate manifest, compatibility, paths, and all required components.
2. Parse configuration into a private staged transaction.
3. Initialize native and Lua components with package-owned registrations.
4. If initialization succeeds, commit its configuration and registrations.
5. If initialization fails, discard staged changes, remove registrations, and
   clean up any partially initialized component.

Within a mixed package, native initialization precedes Lua initialization, and
native callbacks precede Lua callbacks for the same event. This is a fixed rule,
not an implicit dependency resolver. Dispatch packages in file order. Registration
changes become visible outside active dispatch; commands take effect at documented
safe boundaries.

At shutdown, stop dispatch, destroy vehicle/provider state while handles are
valid, send shutdown notifications, remove registrations, shut down Lua then
native components, and unload successful packages in reverse order. Finish render
callbacks and unload plugins before graphics-context destruction.

Lua errors identify package, event, and traceback and deactivate that component's
ordinary callbacks. Committed configuration remains until host shutdown; do not
silently roll it back mid-drive. Revoke pending commands from the failed component.
Native callback errors retain the current deactivation policy. Failure in an
active physics provider terminates the run through normal cleanup; never switch
solvers mid-drive. Native crashes, corruption, and hangs remain in-process hazards.

## Configuration API

Use JSON for declarative values and manifests. Files declare a schema version and
explicit changes keyed by stable, verified record identifiers. Publish examples
only for fields with established meanings and consumers; do not turn unknown
offsets into guessed tuning names.

Create a supported-field registry recording, for each field:

- Record type and identifier, recovered field/consumer evidence, and units.
- Accepted type, valid range, and dependencies on other fields.
- When the override applies and whether it affects already-created objects.
- Supported host/data versions and unresolved recovery limitations.

Initial operations are explicit `set` overrides. Later successful packages win
for the same record/field; report the winning package and previous owner. Retain
original loaded values separately so validation and diagnostics can show the
effective configuration. Validate and commit each package atomically; reject
unknown fields, nonexistent records, nonfinite/out-of-range values, and invalid
cross-field combinations. Do not silently ignore unsupported entries.

Prefer overrides in native record-loading/construction adapters before recovered
consumers run. Do not patch instructions or enlarge original pool objects. If a
supported consumer cannot be reached through a clean adapter, defer that field
until its recovery/integration work is established.

Deliver tuning domains incrementally:

1. A small verified vehicle-tuning subset with observable demo effects.
2. Part/car values when the relevant recovered records and consumers are verified.
3. Race definitions and economy values when recovered race/economy execution is
   available and can be exercised end to end.

The current driving demo cannot establish race/economy behavior by merely parsing
files. Unsupported domains must be reported as unavailable. Do not create new
race or economy mechanics to make a configuration example appear functional.

Configuration writes are startup-only in the first portable release. Runtime
tuning writes require a later, separately verified safe-boundary contract.
Custom physics providers receive effective initial state but are not assumed to
consume every recovered tuning field. Document field applicability and report
configuration/provider combinations whose intended effect is unsupported.

## Lua API

Give each mod its own Lua state, memory budget, and package-owned callbacks. Pin
one supported interpreter version for reproducible behavior. Do not expose `ffi`,
native module loading, raw pointers, unrestricted `io`/`os`, or the debug library.
Provide a package-local module loader and bounded read access to explicitly
allowed package data. No arbitrary filesystem or shell access.

Expose a small versioned `ug2` namespace for:

- Logging, host/API identification, and capability queries.
- Event subscriptions and callback-scoped vehicle telemetry snapshots.
- Generation-checked vehicle/HUD handles represented without integer truncation.
- HUD enumeration, hash lookup, parent relationships, and draw-only translation.
- Supported configuration inspection and named gameplay commands as recovery
  enables them.

Use documented values/tables rather than internal C++ objects. Provide opaque
handles and copied snapshot data; reject stale handles after respawn. Lua API
versions are distinct from the native binary ABI and manifest schema versions.
Bind verified units and conventions consistently across all authoring layers.

Dispatch Lua on the game thread, preserving host floating-point control settings.
Use protected calls, allocation limits, and instruction budgets. Do not permit
yielding from host callbacks. Bound host operations too: instruction hooks alone
cannot interrupt an arbitrarily long C binding. Budget failures follow Lua's
ordinary component-error policy; this is containment, not a claim that all hostile
scripts are safely sandboxed.

HUD translations are allowed only during the layout event and reset every pass.
Other side-effecting commands queue for documented boundaries, validate handles
again at execution, and retain originating ownership. Keep raw OpenGL and physics
provider registration in the native layer for this release. Native plugins must
not publish ad hoc Lua bindings in the first portable API.

## Retained native contracts

Keep exported `ug2_mod_query`, C linkage, explicit calling conventions,
fixed-width fields, versioned structures with `struct_size`, and host function
tables. Do not pass STL objects, exceptions, or allocation ownership across the
ABI. Document borrowed lifetimes and preserve host floating-point settings.

The existing native ABI requires exact structure sizes. Adding configuration
functions must therefore use an explicit compatible extension/query design or
an ABI version bump; do not append fields and claim existing plugins remain
compatible without loader negotiation and tests.

Expert access remains optional and exact-build gated. Generate the identity from
relevant sources, headers, compiler/build settings, and reference-image identity.
Reject mismatches before initialization. Portable mods do not receive expert
access; identical API versions do not make internal pointers portable.

Native physics providers retain create/destroy/step/impulse/post-collision-sync
callbacks and explicit original/custom/failure results. Last successful provider
in load order wins; otherwise recovered UG2 physics runs. Print the selected
provider. Forwarding requests original advance exactly once.

Retain recovered collision queries, snapshots, bounds, ordering, shapes, and
materials. Custom state supplies mass and body-local inertia consistently to
collision response, including world-axis inverse inertia. Preserve host position
separation and impulse changes in the provider snapshot, then publish to camera,
rendering, and HUD. Keep provider storage outside recovered fixed-size layouts.
Validate finite state, proper orientation, positive mass, and valid inertia;
invalid state or provider failure terminates with cleanup.

## Widescreen HUD and FOV

Keep the supplied widescreen fix as a native example built solely against the SDK.
Its policy stays in the plugin; the host exposes generic HUD/projection operations.
A later Lua example may demonstrate equivalent HUD anchoring using the same API,
but do not apply both anchoring examples simultaneously.

Use verified package hashes and parent relationships, including minimap masks,
contents, and indicators. Apply group translations once through the separate
render layer after animation and before composition. Reset offsets each pass;
leave recovered animated state unchanged. Preserve uniform height-based scale,
original edge margins, and central alignment. Displacement is half the width
beyond the centered 4:3 area, and zero at 4:3 or narrower.

Use Hor+ world projection: preserve the recovered 4:3 vertical view and increase
horizontal tangent in proportion to aspect ratio above 4:3. Use the same effective
projection for drawing and picking. Preserve baseline projection at 4:3 or
narrower. Report framebuffer dimensions and retain demo resolution options.

## Delivery phases and completion gates

### Phase 1: native foundation and widescreen fixes

Retain and review the implemented native foundation. Loader, lifecycle, physics
delegation/custom exchange, installed-header builds, HUD/FOV captures, and affected
recovered verifiers must pass. Record bounded evidence separately from byte
matching; do not equate a passing fixture with full game recovery.

### Phase 2: portable packages and configuration vertical slice

Implement manifest loading, legacy compatibility, component selection, package
ownership, staged initialization, and transactional configuration. Ship one
vehicle-tuning example that changes a verified value and demonstrates the effect
through its recovered consumer. A parser-only fixture is insufficient.

### Phase 3: Lua vertical slice

Implement the restricted runtime and versioned bindings. Ship examples for
telemetry logging and event-driven HUD placement. Verify independent states,
budgets, errors, cleanup, handle invalidation, and stable command boundaries.
Ship a mixed package demonstrating shared ownership and rollback.

### Phase 4: recovered tuning domains

Extend the registry to part/car values, races, and economy only as recovered
consumers become testable. Each supported domain needs original/rebuilt evidence,
an installed example, and end-to-end behavior checks. Maintain an explicit
supported/unavailable matrix rather than declaring all requested domains complete.

### Phase 5: Windows portability

Port native host loading, paths, calling conventions, FP preservation, and build
rules. Build Windows examples separately. Run the same data/Lua packages and
protocol tests on Linux and Windows and compare observable API behavior. Until
this passes, describe portable mods as designed for portability and tested on
Linux, not verified on both platforms.

## Validation matrix

- Loader: ordered loading, unlisted packages, missing files/order, duplicate IDs,
  traversal, symlink escape, malformed manifests, ABI/API/schema mismatch, expert
  mismatch, missing required/optional platform components, and init rollback.
- Ownership/lifecycle: event order, mixed component order, deferred registrations,
  pending command cleanup, stale handles, repeated HUD configuration/respawn, and
  reverse shutdown before context destruction.
- Configuration: unknown fields/records, bounds and units, cross-field validation,
  conflict ownership, atomic rejection, baseline preservation, and effects through
  each supported recovered consumer.
- Lua: isolated globals, restricted module/filesystem access, allocation/instruction
  budgets, protected callback errors, FP restoration, handle representation, and
  queued-command validation at safe boundaries.
- Baseline: identical input runs with modding disabled, all mods disabled, empty
  order, and the forwarding provider; equivalent simulation and exactly one
  original advance per corresponding step.
- Physics: provider precedence, custom publication, host position separation,
  collision impulses, mass/inertia conversion, post-collision synchronization,
  and clean termination on failures/invalid state. Synthetic fixtures remain
  protocol tests rather than replacement gameplay.
- Visual: 4:3, 16:9, 21:9, and 32:9; unchanged 4:3 baseline, proportions, margins,
  central alignment, clipping, masks/indicators, accepted skins, animation,
  respawn, and constant vertical FOV with increasing horizontal coverage.
- Build/distribution: modding OFF/ON, Lua OFF/ON, external C/C++ plugins using
  installed headers only, install-and-run packages, and later both host platforms.

Run affected original/rebuilt verifiers sequentially as repository instructions
require. Keep retail assets and generated results out of version control. Preserve
existing working-tree changes, especially ongoing HUD work.

## Release definition

The first portable-layer release requires the native examples, a verified
configuration example, Lua examples, and a mixed package to run through published
interfaces, with the relevant validation gates passing. Publish installation,
schemas, conflicts, lifetimes, event order, Lua restrictions, graphics obligations,
physics contracts, expert compatibility, and remaining recovery limitations.

Do not call race/economy support complete before its Phase 4 gates pass, or
cross-platform execution verified before Phase 5 passes. The package architecture
supports those additions without making current Linux users rebuild the game for
each installed mod.
