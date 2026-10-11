# Documentation

## Scope and development

- [Decompilation scope](DECOMPILATION_SCOPE.md): complete-game objectives and required completion evidence.
- [Repository guidelines](../AGENTS.md): contributor workflow, build commands, and recovery rules.
- [Native modding](../modding/README.md): optional C SDK, native plugins, widescreen HUD/FOV, and physics-provider seams.
- [Native demo](NATIVE_DEMO.md): recovered routines, platform seams, build/run instructions, and outstanding integration checks.

## Recovery evidence

- [Strings](pc-strings.md): nine byte-matching routines, behavioral checks, and local reproduction.
- [HUD minimap](pc-hud-minimap.md): angle/TrackInfo/indicator helper verification, city-map integration and rendering seams.
- [Freeroam HUD](pc-hud-freeroam.md): turbo/NOS/heat, shift light, career text, GPS and notification scheduling, mask dispatch and bounded animation evidence.
- [Pause menu](pc-pause-menu.md): `UI_Pause.fng` object, option lists and item handlers, with SEH-aware original/rebuilt verification.
- [Racing](pc-racing.md): drift ranking, career reward/eligibility recovery, and bounded verification.
- [Vehicle physics](pc-vehicle-physics.md): timestep, integrator, engine, and drivetrain investigation.
- [Wheel and body physics](pc-wheel-physics.md): vector math, wheel preparation/correction, coupling, and body verification.

The physics documents preserve the recovery sequence: later findings supersede
earlier leads. Use the native demo guide for integration status. Source presence,
finite behavioral comparisons, and byte matching are distinct evidence levels;
none alone establishes complete game recovery.

Retail inputs and generated disassembly/reports stay in ignored local directories.
Document their fingerprints, commands, and limitations here without redistributing
them. Session-specific commit state and temporary goals belong in chat or Git history.
