# Twilight Visuals — Vanilla Dusklight port

## Disclaimer

Yeah, this project was vibe coded, and yeah, AI sucks in a lot of ways.
However, it allows me to make these cool mods and it's mostly for fun—I'll
never ask for any money.

I tried making this as easy as possible for anyone to do. All you should need
is the extracted disc contents of both Twilight Princess and Skyward Sword. If
there are any issues, please reach out.

---

## About

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

The music files are not stored inside the `.dusk` archive. Twilight Visuals
creates one shared `custom assets` root with separate `music` and `animations`
folders. Music is loaded only from the following platform-specific folder:

- Windows: `<Dusklight folder>\\custom assets\\music\\`
- macOS: `~/Library/Application Support/TwilitRealm/Dusklight/Twilight Visuals/custom assets/music/`
- Android: Dusklight's per-mod data directory, in `custom assets/music/`

The folder is created automatically when the mod starts. Previous music
locations are not checked.

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
instead of these MP3 replacements. Missing, empty, unreadable, or unsupported
files are reported in the Dusklight log with the checked path and reason; the
corresponding replacement track will not play.

## User-provided wall-running animations

All normal builds load converted wall-running animations only from the
`animations` folder under the same shared `custom assets` root:

```text
Windows: <Dusklight folder>\\custom assets\\animations\\wall_run.bck
         <Dusklight folder>\\custom assets\\animations\\ledge_grab.bck

macOS:   ~/Library/Application Support/TwilitRealm/Dusklight/Twilight Visuals/custom assets/animations/wall_run.bck
         ~/Library/Application Support/TwilitRealm/Dusklight/Twilight Visuals/custom assets/animations/ledge_grab.bck
```

The folder is created automatically when the mod starts. Copy the converter's
two output files into the matching folder, then restart Dusklight. Previous
animation locations are never read.

The public mod package intentionally does not contain these animation files.
Users provide their own converted files so the package does not redistribute
game-derived assets.

If either file is missing, empty, unreadable, or not a valid BCK, Twilight
Visuals records the exact expected path and the reason in the Dusklight log.
Both files are checked so the log can identify every file that needs attention.

For local development only, `-DTWILIGHT_BUNDLED_ANIMATION_BACKUP=ON` restores
the old bundled loading code. The corresponding files must first be restored
from the ignored `local-animation-backup/` directory into `res/animations/`.
Do not enable that option for a public upload.

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

### Android ARM64

Build the matching Dusklight Android dependency tree first, then configure and
package this mod with the Android NDK toolchain. The official Dusklight Android
shell requires Android SDK Platform 37, the NDK version selected by the
Dusklight presets, and JDK 17 or newer:

```powershell
Push-Location dusklight-latest
cmake --preset android-arm64
cmake --build --preset android-arm64 --target dusklight
Pop-Location

cmake -S . -B build-android-arm64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_HOME/ndk/$env:ANDROID_NDK_VERSION/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-28 `
  -DDUSKLIGHT_DIR="$PWD/dusklight-latest" `
  -DDUSKLIGHT_BUILD_DIR="$PWD/dusklight-latest/build/android-arm64" `
  -DDUSKLIGHT_VERSION=b245c6bef8b4a370afb2453104a585dd41c97676
cmake --build build-android-arm64 --target twilight_visuals_package
```

The Android bundle is emitted at
`build-android-arm64/mods/twilight_visuals.dusk` and contains
`lib/android-arm64/mod.so`. For a normal Android installation, copy that
`.dusk` into Dusklight's Android mod/import flow or include it in the APK's
`assets/mods` directory when rebuilding the Dusklight shell. The mod itself
does not modify Dusklight or Aurora.

The included GitHub Actions workflow also stages the bundle into Dusklight's
Android assets, builds the debug APK, and uploads both the `.dusk` package and
APK as workflow artifacts. Push the repository to GitHub, run the workflow
from the Actions tab, then download
`dusklight-twilight-visuals-android-arm64-debug` from the completed run.

The hook design follows Dusklight's official
[Hooking Game Functions guide](https://github.com/TwilitRealm/dusklight/blob/main/docs/modding.md#hooking-game-functions).
