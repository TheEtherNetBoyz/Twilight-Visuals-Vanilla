# Vanilla Dusklight port status

This project is intentionally separate from the MFB-specific Twilight Visuals
projects. It builds with `FEATURES game` against an unmodified upstream
Dusklight checkout and contains no `src/dusk/TwilightHostApi.h` dependency.

## Ported to the vanilla hook boundary

- settings and quick-menu registration
- environment, post-processing, weather, and particle hooks
- sky palette decoding and authored sky variants
- monochrome and Dark Hour visual paths
- Skyward Sword running and movement hooks
- native particle/trail paths with interpolation disabled safely

## Deliberately native-owned or deferred

- MFB's private MP3 mixer and master-volume integration
- sequence/audio-manager callback registries
- MFB frame interpolation and matrix replacement
- host-level geometry/celestial callback registries

Custom music now has a vanilla path: Nintendo AST files placed in
`res/music/` are registered through Dusklight's official `AudioResService` and
mounted with `OverlayService`. The current safe integration replaces scene
music while leaving native battle/fanfare bookkeeping intact. MP3 files still
need to be converted to AST externally.

## Verification

The source was migrated to official typed hooks and the `game` SDK feature.
The next verification step is loading the resulting bundle in a vanilla
Dusklight build and exercising settings reload, area transitions, visual style
changes, running, and shutdown.
