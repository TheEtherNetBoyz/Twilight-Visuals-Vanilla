# Twilight Visuals — Vanilla Dusklight port

## Disclaimer

Yeah, this project was vibe coded, and yeah, AI sucks in a lot of ways.
However, it allows me to make these cool mods and it's mostly for fun—I'll
never ask for any money.

I tried making this as easy as possible for anyone to do. All you should need
is the extracted disc contents of both Twilight Princess and Skyward Sword. If
there are any issues, please reach out.

---

## What the mod does

Twilight Visuals lets you change the overall mood of *Twilight Princess* with
several full visual presets. These are more than simple color filters: each
one can change the lighting, sky, fog, bloom, weather, particles, screen
effects, and music together.

The important part is that these changes are mostly visual. For example,
**Normal Twilight** can make a regular area look like it is covered in
Twilight without actually loading that area's Twilight gameplay layer. That
means it should not bring back vessels, spirits, or other Twilight-only story
objects just because you changed the visuals.

There are two main switches:

- **Enable Twilight Visuals** is the master switch for the entire mod.
- **Enable Visual Style & Music** turns the selected visual preset and its
  music on or off. The other features, such as running, wall running, hotkeys,
  and menu scaling, can still be used while this is off.

## Installation and first use

1. Copy `twilight_visuals.dusk` into a Dusklight `mods` folder.
2. Start Dusklight and confirm that **Twilight Visuals (Vanilla Dusklight)** is
   shown as loaded.
3. Open Dusklight's quick menu and select the **Twilight Visuals** tab.
4. If a visual option does not update immediately, leave and re-enter the room.

The normal Windows user-mod directory is:

```text
%APPDATA%\TwilitRealm\Dusklight\mods\
```

A development build can also load mods from a `mods` folder beside the game
executable. Try not to keep two copies installed at once. The copy in your
user-mod folder may override the newer one beside the executable, which can
make it look like a new build did not install correctly.

## Visual styles

### Normal Twilight

This is the regular Twilight style from the final version of *Twilight
Princess*. It is meant to look as close to the real in-game effect as possible,
including its golden sky, lighting, fog, bloom, colors, particles, and Palace
of Twilight music behavior. Areas that normally have a Twilight version use
the matching environment properties. Other areas get the same general look
without switching their story state or gameplay layer.

### Black and White

This preset is based on the early beta version of *Twilight Princess*, where
areas covered in Twilight had a much more colorless, black-and-white look than
the golden style used in the finished game. The preset recreates that unused
direction while keeping the rest of the Twilight effects. It can also play the
Palace of Twilight music anywhere without changing the game's progression.

### Astral Plane

This preset is inspired by the Astral Plane from PlatinumGames' *Astral Chain*.
In that game, the Astral Plane is an alternate dimension connected to the
Chimeras and Red Matter invading the human world. The preset brings that
strange, otherworldly atmosphere into *Twilight Princess* with its own colors,
ambient music, combat music, and chromatic aberration effect. Use **Astral
Chromatic Aberration** to make the color split more or less noticeable without
changing the brightness.

### The Dark Hour

This preset is inspired by the Dark Hour from Atlus' *Persona 3*. In that game,
the Dark Hour is a hidden period between one day and the next when the world
changes, ordinary people are unaware of what is happening, and Shadows roam
freely. The preset brings that eerie feeling into *Twilight Princess* with a
dark blue-and-green nighttime look, moonlit skies, green highlights, extra
atmosphere, weather effects, and custom music from *Persona 3*.

The game clock still moves normally, but the lighting stays visually locked to
midnight so it does not keep getting brighter or darker throughout the day.

The preset can also use Blood Rain and blood effects on the ground. These are
only visual and do not change anything related to the story.

## Lighting, weather, and environment controls

- **Brightness** changes the overall brightness of the active style.
- **Per-Area Brightness** lets every stage and room have its own brightness.
- **Current Area Brightness** changes the room you are standing in. When you
  enter another room, the slider updates to that room's saved value.
- **Twilight Camera Light** controls the light that follows the camera. Turning
  it off can help with the noticeable lighting shift when moving the camera.
- **Skybox** can use Twilight Day, Twilight Night, Sunrise, Sunset, Overcast,
  Faron Twilight, Eldin Twilight, Lanayru Twilight, Palace of Twilight, Sacred
  Grove, Snowpeak, Gerudo Desert, Lake Hylia, Fishing Hole, Ordon, Hyrule
  Field, or Castle Town behavior.
- **Weather** can retain the current weather or force Clear, Rain, Snow,
  Lightning, Wind Storm, Snow Storm, Heavy Fog, or Blood Rain.
- **Bloom Override Mode** lets you use Native Dusklight, Off, Classic (MFB), or
  Dusklight bloom.
- **Bloom Brightness** controls how strong the selected bloom looks.
- **Foreground Visibility** changes how thick the foreground fog is and how
  far away it starts. Setting it to `0%` keeps the room's original fog.
- **Exclude Palace of Twilight** leaves the Palace's original visuals and
  music alone instead of applying the current preset there.

Outdoor areas, indoor rooms, dungeons, and dungeon rooms that are actually
outside are handled separately. The mod updates the sun, moon, sky, fog,
colors, particles, background, and post-processing as one complete look. The
sun is hidden during the Dark Hour, while its moon and night sky stay active.
Blood Rain and supported Dark Hour rooms can also place blood effects on the
floor.

## Music and audio behavior

- Every style can use its own ambient music without changing the actual stage.
- Normal Twilight and Black and White can play the Palace of Twilight music
  outside of the Palace.
- **Override Temple Music** keeps the selected custom music playing in normal
  temple and dungeon rooms. Boss and miniboss music is left alone.
- **Custom Music Volume** changes only the music added by this mod.
- Custom music pauses when an item fanfare, cutscene, boss theme, or another
  protected vanilla track needs to play.
- Dark Hour music still plays when the in-game clock reaches nighttime.
- If a music file is missing, the rest of the mod will still work. Only that
  track will be unavailable.

## Movement and gameplay features

### Skyward Sword-style running

Human Link can hold **A** while moving to sprint. You can attack and roll out
of a run, and the mod includes running trails and special Magic Armor water
behavior. It also adjusts the speed properly for snow, sand, Iron Boots,
indoor areas, and dungeon rooms.

### Put sword away while sprinting

Link automatically puts his sword away when he starts sprinting. You can turn
this off without turning sprinting off.

### Skyward Sword-style wall running

While sprinting, Link can run up walls, step onto short walls, and grab ledges.
The optional custom animations go in the external animation folder listed
later in this README.

### Wolf Senses as Human

After wolf senses have been unlocked normally, human Link can use them with
**D-pad Down**. This does not give you the ability early.

## Interface and quality-of-life features

- **Load Mode** has Normal and Fast options. Fast mode shortens normal room and
  area transitions, while story transitions and other sensitive loads keep
  their original behavior.
- **Hide Mouse Cursor During Gameplay** hides the cursor while playing and
  brings it back when a menu needs it.
- **Menu Scaling Override** can use Native Dusklight, GameCube, Wii, or
  Dusklight layouts for file select, collection, save, name-entry, and TV-check
  menus.
- **Facial Expression Tuner** lets you choose from the game's human and wolf
  facial animations. Automatic gives control back to the game, and a chosen
  expression only applies to the correct form.

## Bindable hotkeys

The mod provides bindable actions for:

- Gyro on/off
- Bloom mode
- Texture replacement on/off

Each action has one binding that can be a keyboard key or controller button.
Using a hotkey shows a small message on screen so you know what changed. Press
**Escape** while choosing a binding to clear it.

Some graphics are already loaded into memory by the time you use a hotkey. If
a texture or render option does not update right away, reload the room.

## Configuration files

The main settings are saved through Dusklight's mod configuration and do not
touch the game's original config. Per-area brightness uses its own file named
`twilight_visuals_area_brightness.cfg` in Dusklight's writable data folder.
Each stage and room gets its own entry, so going through a door can load a
different saved brightness.

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
