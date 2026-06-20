# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**driftslide** is an OBS Studio plugin that periodically fades images from a directory into a
live stream with configurable transitions. It registers as an `OBS_SOURCE_TYPE_INPUT` source
("DriftSlide") that streamers add as a layer in any OBS scene.

Built from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate). Plugin metadata
(name, version, author, website) lives in `buildspec.json` — CMake reads it at configure time via
`cmake/common/bootstrap.cmake`. The plugin is currently built against OBS `31.1.1` (the latest
version supported by the official template); tested and working on OBS `32.1.2`.

## Source Layout

```
src/
  plugin-main.cpp        # OBS entry points: obs_module_load / obs_module_unload,
                         # registers driftslide_source via obs_register_source()
  driftslide-source.hpp  # DSState / TransitionType enums, DriftSlideSource struct,
                         # callback declarations
  driftslide-source.cpp  # All OBS callbacks: create/destroy/update/tick/render/
                         # get_properties/get_defaults/get_width/get_height
  image-list.hpp         # ImageList class declaration
  image-list.cpp         # Directory scan, alphabetic sort, Fisher-Yates shuffle;
                         # stores paths as UTF-8 (u8string) for OBS compatibility
  plugin-support.h       # Declares obs_log(), PLUGIN_NAME, PLUGIN_VERSION
  plugin-support.c.in    # CMake-configured; fills in PLUGIN_NAME/VERSION — never edit directly

data/
  locale/en-US.ini       # All UI strings (fallback locale; OBS falls back here for any language)
  effects/driftslide.effect  # OBS effect (HLSL on D3D11 / GLSL on OpenGL): Fade + SlideLeft/Right/Up/Down transitions

buildspec.json           # Single source of truth for plugin name, version, author, OBS SDK version
```

## Architecture: State Machine

The plugin runs a 4-state machine driven by `video_tick()` elapsed time:

```
TRANSPARENT ──(transparent_duration elapsed)──► FADE_IN
FADE_IN     ──(transition_duration elapsed)───► DISPLAYING
DISPLAYING  ──(display_duration elapsed)──────► FADE_OUT
FADE_OUT    ──(transition_duration elapsed)───► TRANSPARENT  (advance to next image)
```

- **TRANSPARENT**: returns immediately from `video_render` — zero pixel output, fully transparent.
- **FADE_IN / FADE_OUT**: smoothstep-eased alpha (and UV offset for slide transitions).
- **DISPLAYING**: renders the image at full opacity.
- Images are loaded with `gs_image_file4_init(..., GS_IMAGE_ALPHA_PREMULTIPLY_SRGB)` and freed
  between display cycles to avoid holding GPU memory during the transparent interval.

## Thread Safety

`video_tick` and `video_render` run on the OBS render thread; `update` runs on the UI thread.
Settings are transferred via a pending-values pattern:

- `update()` locks `settings_mutex`, writes to `pending_*` fields, sets `settings_dirty = true`.
- `video_tick()` locks `settings_mutex` at the top of each tick, copies pending to active fields
  if dirty, then releases the lock before any GPU work.
- All GPU operations (`gs_image_file4_*`, `gs_effect_*`) happen with the lock released.

## Render Pipeline

- Images are loaded as premultiplied SRGB (`GS_IMAGE_ALPHA_PREMULTIPLY_SRGB`), which converts
  pixels to linear light and premultiplies alpha in linear space. The resulting GPU texture holds
  linear values.
- The custom effect (`data/effects/driftslide.effect`) multiplies all channels by `t` (`col *= t`)
  — correct for premultiplied textures.
- The effect declares `uniform float4x4 ViewProj` explicitly. The OpenGL backend injects this
  automatically; the D3D11/HLSL compiler requires an explicit declaration.
- Textures are bound with `gs_effect_set_texture` (not `_srgb`). Because
  `GS_IMAGE_ALPHA_PREMULTIPLY_SRGB` already produced a linear texture, using the `_srgb` variant
  would apply a second sRGB→linear decode and make images appear too dark.
- `OBS_SOURCE_CUSTOM_DRAW` is set in `output_flags` so OBS does not activate its own default
  effect before calling `video_render`. Without it, calling `gs_effect_loop` on the plugin's custom
  effect while OBS's effect is already active produces a `gs_effect_loop: An effect is already
  active` error and no rendering.
- `OBS_SOURCE_SRGB` is set in `output_flags` so OBS applies correct linear→sRGB conversion when
  compositing the source.
- Blend mode: `GS_BLEND_ONE, GS_BLEND_INVSRCALPHA` (premultiplied alpha compositing).
- For Fade (transition_type 0): no UV offset, pure alpha fade.
- For Slide variants: UV is offset by `(1.0 - t)` in the relevant axis; pixels outside [0,1] UV
  return `float4(0,0,0,0)` (transparent).

## Settings

| Key | Type | Default | UI range |
|-----|------|---------|----------|
| `image_directory` | path | `""` | directory picker |
| `image_order` | int (0=Alphabetic, 1=Random) | 0 | list |
| `transparent_duration` | double (s) | 30.0 | 1–3600 |
| `display_duration` | double (s) | 15.0 | 1–300 |
| `transition_duration` | double (s) | 2.0 | 0.1–10 |
| `transition_type` | int (0–4) | 0 | Fade/SlideLeft/Right/Up/Down |

## Build

### Linux (development machine)

Prerequisites on Ubuntu: `libobs-dev`, `ninja-build`, `pkg-config`, `build-essential`.

```bash
# Configure
cmake --preset ubuntu-x86_64

# Build
cmake --build --preset ubuntu-x86_64

# Install into local OBS plugin directory (Linux only)
cmake --install build_x86_64 --prefix ~/.config/obs-studio/plugins/driftslide
```

Build output: `build_x86_64/driftslide.so`.

### Windows (via GitHub Actions CI)

The Linux `.so` cannot run on Windows. To get a Windows `.dll`:

1. Push to GitHub — the `push.yaml` workflow triggers `build-project.yaml` automatically.
2. Go to **GitHub → Actions → latest run → Artifacts** and download the Windows package.
3. Unzip; you will find `driftslide.dll` and the `data/` tree inside.

To build locally on Windows (requires Visual Studio 2022 with C++ workload):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

CMake downloads the OBS SDK and prebuilt deps automatically via `buildspec.json`.

## Installing on Windows

**Confirmed working path** (OBS 32.1.2, Windows):

```
C:\ProgramData\obs-studio\plugins\driftslide\bin\64bit\driftslide.dll
C:\ProgramData\obs-studio\plugins\driftslide\data\locale\en-US.ini
C:\ProgramData\obs-studio\plugins\driftslide\data\effects\driftslide.effect
```

Note: `C:\ProgramData\` (not `%APPDATA%`). Restart OBS after copying files.
The source appears under **Sources → Add → DriftSlide**.

Paths that do NOT work:
- `%APPDATA%\obs-studio\plugins\` — OBS on Windows does not scan this for user plugins.
- `%APPDATA%\obs-studio\obs-plugins\64bit\` — not a valid OBS scan path.
- `C:\Program Files\obs-studio\obs-plugins\64bit\driftslide\` — DLL must be directly in
  `64bit\`, not inside a subdirectory.

## Debugging a Plugin That Does Not Appear

1. **Check the OBS log**: In OBS go to **Help → Log Files → Show Log Files**. Open the newest
   `.txt` and search for `driftslide`. A missing entry means OBS never found the file (wrong path);
   an error entry gives the exact reason.

   Search for `driftslide: scanned` to see which directory was scanned and how many images were
   found. A count of 0 means the path is wrong, the folder is empty, or the files have
   unsupported extensions.

2. **Verify all three files are present**: DLL, `en-US.ini`, and `driftslide.effect`. OBS loads
   the DLL first — without it the locale file is ignored entirely.

3. **Check the DLL is for the right platform**: The Linux build produces `.so` which cannot load
   on Windows. You need a `.dll` from the CI Windows artifact or a native Windows build.

4. **Language**: OBS falling back to English labels is normal and harmless. The plugin uses
   `OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")` which silently falls back to `en-US`
   when no locale file exists for the current UI language.

5. **OBS version mismatch**: The plugin is built against OBS `31.1.1` but works on `32.1.2`.
   If a future OBS version breaks the ABI, update the `obs-studio` version and hashes in
   `buildspec.json` to match, then push to trigger a new CI build.

## Formatting

Two formatters are enforced; both are checked in CI (`check-format` workflow).
Run them before every commit — CI will fail otherwise.

| Formatter | Targets | Version required |
|-----------|---------|-----------------|
| `clang-format` | `src/**/*.(c|cpp|h|hpp|m|mm)` | exactly 19.1.1 |
| `gersemi` | `CMakeLists.txt`, `cmake/**/*.cmake` | ≥ 0.12.0 |

```bash
# Format C/C++ sources (requires clang-format-19 on PATH)
clang-format-19 -i src/plugin-main.cpp src/driftslide-source.hpp \
    src/driftslide-source.cpp src/image-list.hpp src/image-list.cpp

# Or via the project wrapper (requires zsh)
./build-aux/run-clang-format

# Format CMake files
gersemi -i CMakeLists.txt

# Or via the project wrapper
./build-aux/run-gersemi
```

The `build-aux/run-*` wrappers require `zsh`; if not available, call the formatters directly as
shown above. `clang-format-19` can be installed on Ubuntu via `apt install clang-format-19`.
`gersemi` can be installed via `pip install --break-system-packages gersemi`.

Key style rules (`.clang-format`): indent width 8, tab for indentation, column limit 120,
brace-after-function on its own line, `SortIncludes: false`.

Key gersemi rules (`.gersemirc`): line length 120, indent 2, favour inlining short lists.

## CI / Release

- `push.yaml` / `pr-pull.yaml` / `dispatch.yaml` trigger `build-project.yaml` (compiles all
  platforms) and `check-format.yaml`.
- A semver tag pushed to `main`/`master` (e.g. `1.2.3`) triggers a draft GitHub Release with
  packaged installers for all platforms.
- Version is read from `buildspec.json` — bump it there before tagging.

## Scripts

See `scripts/CLAUDE.md` for details on the `scripts/` directory. The main script is
`scripts/prepare_images.py` (Python 3 + Pillow): batch image pipeline for
`files/input/` → `files/output/`.

```bash
python3 scripts/prepare_images.py
```
