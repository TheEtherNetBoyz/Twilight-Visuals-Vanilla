# Custom music for Twilight Visuals (vanilla Dusklight)

You can either bundle Nintendo AST stream files in this directory, or supply
them externally. External files are not packaged into the `.dusk` archive.

For external use, place the files in a `music` or `external_music` folder in
one of these locations:

- the build/run working directory, such as `build-latest/music`
- the extracted mod package directory or its native runtime directory
- Dusklight's current working directory
- the mod's Dusklight data directory

Use these exact names:

- `astral_plane.ast`
- `astral_plane_combat.ast`
- `dark_hour.ast`
- `dark_hour_combat.ast`

The vanilla Dusklight audio service does not accept MP3 files. The old MFB
mixing path is intentionally not used here. AST files are registered through
Dusklight's official `AudioResService` and mounted through `OverlayService`.

The current vanilla integration replaces the scene/ambient track. Native
battle and fanfare bookkeeping remains owned by Dusklight until a supported
battle-track provider exists.
