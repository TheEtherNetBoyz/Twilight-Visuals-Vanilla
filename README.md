# Twilight Visuals — Vanilla Dusklight port

This is a separate native Dusklight mod project. It targets the pinned upstream
Dusklight SDK and uses the official `game` feature with typed game-function
hooks. It does not include or link `TwilightHostApi`, does not require a rebuilt
host, and does not modify `aurora`.

The port keeps the visual and gameplay work that can be implemented directly in
vanilla Dusklight: settings/quick menu, environment and post-processing hooks,
weather and particle effects, sky palette decoding, monochrome rendering,
Dark Hour styling, and Skyward Sword running hooks. Custom music uses the
official Dusklight `AudioResService`/`OverlayService` path and accepts bundled
Nintendo AST streams in `res/music/`; the MFB-only MP3 mixer and host callback
registries are not used.

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
