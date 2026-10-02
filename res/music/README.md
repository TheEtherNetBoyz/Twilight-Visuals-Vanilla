# Custom music for Twilight Visuals (vanilla Dusklight)

This directory only documents the external music files. They are not bundled
inside the `.dusk` archive and are not loaded from this `res/music` directory.

Place the MP3 files in one of these locations:

- Windows: beside `Dusklight.exe`
- macOS preferred: `~/Library/Application Support/TwilitRealm/Dusklight/Twilight Visuals/custom music/`
- macOS fallback: beside `Dusklight.app/Contents/MacOS/Dusklight`
- Android: Dusklight's per-mod data directory, in `custom music/` (the exact
  path is logged when the mod loads)

On macOS, the Application Support folder is created automatically the first
time the mod starts. That copy takes priority for each track when it exists;
missing tracks individually fall back to the executable directory.

Use these exact filenames:

- `Astral Plane.mp3` — Astral Plane ambient music
- `Astral Plane CM.mp3` — Astral Plane ordinary combat music
- `tartarus 0d06.mp3` — The Dark Hour ambient music
- `Mass Destruction.mp3` — The Dark Hour ordinary combat music
- `Master of Shadow.mp3` — optional boss music replacement

Restart Dusklight after adding or replacing files. In the Twilight Visuals
settings, select `Astral Plane` or `The Dark Hour` under `Visual Style & Music`.
Use `Custom Music Volume` for the replacement volume and enable `Override
Temple Music` for temple and dungeon scenes. Missing or unsupported files are
reported in the Dusklight log.
