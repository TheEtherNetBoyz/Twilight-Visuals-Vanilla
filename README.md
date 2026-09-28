# Twilight Visuals — Vanilla Dusklight port

This is a separate native Dusklight mod project. It targets the pinned upstream
Dusklight SDK and uses the official `game` feature with typed game-function
hooks. It does not include or link `TwilightHostApi`, does not require a rebuilt
host, and does not modify `aurora`.

The port keeps the visual and gameplay work that can be implemented directly in
vanilla Dusklight: settings/quick menu, environment and post-processing hooks,
weather and particle effects, sky palette decoding, monochrome rendering,
Dark Hour styling, and Skyward Sword running hooks. Custom music is streamed
from external MP3 files by the mod's native audio mixer.

## Custom music

The music files are not stored inside the `.dusk` archive. Place them in the
same directory as the Dusklight executable:

- Windows: beside `Dusklight.exe`
- macOS: beside the app executable at `Dusklight.app/Contents/MacOS/`

Use these exact filenames:

| File | Used for |
| --- | --- |
| `Astral Plane.mp3` | Astral Plane ambient music |
| `Astral Plane CM.mp3` | Astral Plane ordinary combat music |
| `tartarus 0d06.mp3` | The Dark Hour ambient music |
| `Mass Destruction.mp3` | The Dark Hour ordinary combat music |
| `Master of Shadow.mp3` | Optional boss music replacement |

After adding or replacing files, restart Dusklight so the mod can reload them.
Then open the Twilight Visuals settings and select `Astral Plane` or `The Dark
Hour` under `Visual Style & Music`. Use `Custom Music Volume` to adjust the
replacement volume. Enable `Override Temple Music` if the selected custom
style should also replace music in temples and dungeons; the Palace exclusion
setting still takes precedence.

`Normal Twilight` and `Black and White` use the Palace of Twilight sequence
instead of these MP3 replacements. Missing or unsupported files are reported
in the Dusklight log and the corresponding replacement track will not play.

## Build

From this directory, point CMake at an unmodified Dusklight checkout:

```powershell
cmake -S . -B build -G Ninja `
  -DDUSKLIGHT_DIR="..\Twilight Visuals Standalone\dusklight"
cmake --build build --target twilight_visuals_package
```

The bundle is written to `build/mods/twilight_visuals.dusk`. The same project
can fetch the pinned revision automatically when `DUSKLIGHT_DIR` is omitted.

On macOS, use an arm64 build and point `DUSK_GAME_EXE` at the local Dusklight
application executable so the mod links against the exact game build you will
test:

```sh
cmake -S . -B build-macos -G Ninja \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDUSK_GAME_EXE="/path/to/Dusklight.app/Contents/MacOS/Dusklight" \
  -DDUSKLIGHT_DIR="/path/to/dusklight" \
  -DDUSKLIGHT_BUILD_DIR="/path/to/dusklight/build/macos-default-relwithdebinfo"
cmake --build build-macos --target twilight_visuals_package
```

The macOS package contains `lib/macos-arm64/mod.so` and can be copied into the
test app's `mods` directory. The mod's platform layer keeps Dusklight and
Aurora read-only.

The hook design follows Dusklight's official
[Hooking Game Functions guide](https://github.com/TwilitRealm/dusklight/blob/main/docs/modding.md#hooking-game-functions).
