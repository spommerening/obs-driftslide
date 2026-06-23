# DriftSlide

An OBS Studio source plugin that fades images from a directory into your stream — one at a time,
on a loop, with smooth transitions. Set it, forget it, and let your visuals breathe.

---

## What it does

DriftSlide registers as a standard OBS input source. Add it to any scene and point it at a folder
of images. It will cycle through them automatically: each image fades in, holds for a configurable
duration, then fades out — leaving a fully transparent gap before the next one appears.

Eleven transition styles are available across three families:

- **Fade** — pure alpha dissolve.
- **Slide** Left / Right / Up / Down — image enters from one edge and bounces back out the same side.
- **Scroll** Left / Right / Up / Down — continuous flow: the image enters from one edge and exits the opposite.
- **Zoom** — image starts cropped at 1.67× and settles to full size.
- **Zoom In** — image scales up from a point at the centre.

An optional **Ken Burns** mode adds a gentle slow pan and zoom throughout each image's entire
visible lifetime (fade-in through fade-out), with a randomised direction per image.

All transitions use smoothstep easing for a polished, broadcast-quality feel.
Images play in alphabetic order or in a randomised shuffle.

**Typical use cases:** artwork rotators, sponsor loops, ambient backgrounds, intermission slides,
countdown interstitials.

---

## State machine

DriftSlide runs a four-state cycle driven by `video_tick` elapsed time:

```
TRANSPARENT ──(transparent duration)──► FADE IN
FADE IN     ──(transition duration)───► DISPLAYING
DISPLAYING  ──(display duration)──────► FADE OUT
FADE OUT    ──(transition duration)───► TRANSPARENT  →  advance to next image
```

During TRANSPARENT the source outputs zero pixels — it is fully invisible and costs nothing to
composite. Images are loaded from disk at the start of FADE IN and freed from GPU memory
immediately after FADE OUT, so no VRAM is held during the transparent gap.

---

## Settings

| Setting | Default | Range | Description |
|---|---|---|---|
| Image Directory | _(empty)_ | — | Folder containing the images to display |
| Image Order | Alphabetic | Alphabetic / Random | Playback order |
| Transparent Duration | 30 s | 1–3600 s | Time between images (fully invisible) |
| Display Duration | 15 s | 1–300 s | How long each image is fully visible |
| Transition Duration | 2 s | 0.1–10 s | Fade-in and fade-out duration (each) |
| Transition Type | Fade | Fade / Slide Left / Slide Right / Slide Up / Slide Down / Zoom / Scroll Left / Scroll Right / Scroll Up / Scroll Down / Zoom In | Transition style |
| Ken Burns | Off | On / Off | Slow pan + zoom throughout each image's visible lifetime |

**Slide** transitions enter from the opposite edge and exit back the same way (Slide Left enters
from the right). **Scroll** transitions are the continuous-flow variant: the image enters from one
side and exits the other (Scroll Left enters from the right and exits to the left). **Zoom** starts
the image at 1.67× crop and settles to full size. **Zoom In** grows the image from a single point
at the centre outward.

---

## Installing on Windows

**Confirmed working on OBS 32.1.2.** Get the latest `.dll` from
[GitHub Actions → Artifacts](#building-on-windows-via-ci) and place the three files exactly here:

```
C:\ProgramData\obs-studio\plugins\driftslide\bin\64bit\driftslide.dll
C:\ProgramData\obs-studio\plugins\driftslide\data\locale\en-US.ini
C:\ProgramData\obs-studio\plugins\driftslide\data\effects\driftslide.effect
```

Restart OBS. The source appears under **Sources → Add → DriftSlide**.

> **Note:** Use `C:\ProgramData\` — not `%APPDATA%`. OBS on Windows does not scan the AppData
> path for user plugins.

---

## Building

### Linux (Ubuntu 24.04)

Install prerequisites:

```bash
sudo apt install libobs-dev ninja-build pkg-config build-essential
```

Configure, build, and install into OBS's local plugin directory:

```bash
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
cmake --install build_x86_64 --prefix ~/.config/obs-studio/plugins/driftslide
```

Output: `build_x86_64/driftslide.so`

### Windows (local)

Requires Visual Studio 2022 with the C++ workload. CMake downloads the OBS SDK and prebuilt
dependencies automatically via `buildspec.json`.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

### Windows (via CI)

The Linux `.so` cannot load on Windows. The easiest path is to let GitHub Actions build it for
you:

1. Push your branch to GitHub — `push.yaml` triggers `build-project.yaml` automatically.
2. Go to **GitHub → Actions → latest run → Artifacts** and download the Windows package.
3. Unzip; the archive contains `driftslide.dll` and the `data/` tree ready to install.

### macOS

```bash
cmake --preset macos-arm64   # or macos-x86_64
cmake --build --preset macos-arm64
```

---

## Code formatting

Two formatters are enforced and checked in CI. Run them before every commit or the
`check-format` workflow will fail.

```bash
# C/C++ sources — requires clang-format 19.1.1
clang-format-19 -i src/plugin-main.cpp src/driftslide-source.hpp \
    src/driftslide-source.cpp src/image-list.hpp src/image-list.cpp

# CMake files — requires gersemi >= 0.12.0
gersemi -i CMakeLists.txt
```

Project wrappers (require `zsh`):

```bash
./build-aux/run-clang-format
./build-aux/run-gersemi
```

Install tools:

```bash
sudo apt install clang-format-19
pip install --break-system-packages gersemi
```

---

## Image preparation script

`scripts/prepare_images.py` is a batch pipeline that converts raw source images into stream-ready
JPEGs:

1. SHA-256 deduplication — identical files are skipped
2. Color space normalisation (`RGB`)
3. Resize to exactly 1920 × 1080 (LANCZOS, no aspect-ratio padding)
4. 10% sharpening pass
5. Save as JPEG at 95% quality

Input: `files/input/` — Output: `files/output/`

```bash
pip install Pillow
python3 scripts/prepare_images.py
```

Output filenames are derived from the source file's creation timestamp:
`20260620-073752.jpg`. Collisions get a counter suffix: `20260620-073752-1.jpg`.

The output directory is not cleared between runs — delete it manually if you need a clean slate.

---

## CI / releases

| Workflow | Trigger | What it does |
|---|---|---|
| `push.yaml` | Push to `main`/`master` | Runs build + format check |
| `pr-pull.yaml` | Pull request opened/synced | Runs build + format check |
| `dispatch.yaml` | Manual trigger | Same as push |
| `build-project.yaml` | Called by others | Compiles for Windows, macOS, Linux |
| `check-format.yaml` | Called by others | Verifies clang-format and gersemi |

To cut a release, push a semver tag to `main` (e.g. `1.2.3`). A draft GitHub Release is created
automatically with packaged installers for all platforms attached as artifacts.

Version is read from `buildspec.json` — bump it there before tagging.

---

## Troubleshooting

**Plugin does not appear in OBS**

Open the OBS log (**Help → Log Files → Show Log Files**, newest `.txt`) and search for
`driftslide`. No entry means OBS never found the file; check the install path. An error entry
gives the exact reason.

**Missing files**

All three files must be present: the DLL/SO, `en-US.ini`, and `driftslide.effect`. OBS loads
the binary first — without it the data files are ignored entirely.

**Wrong platform binary**

The Linux build produces `.so`, which cannot load on Windows. Use the Windows artifact from CI
(or build locally with Visual Studio).

**OBS version mismatch**

The plugin is built against OBS `31.1.1` and tested on `32.1.2`. If a future OBS version breaks
the ABI, update the `obs-studio` version and hashes in `buildspec.json` and push to trigger a
new CI build.

**Images not showing up**

- Open the OBS log (**Help → Log Files → Show Log Files**) and search for `driftslide: scanned`.
  The plugin logs the directory it scanned and how many images it found — this is the fastest way
  to confirm whether the path and files are being picked up correctly.
- Ensure the Image Directory path in the source settings points to a folder containing supported
  image files (PNG, JPEG, BMP, TGA, WebP, GIF).
- Check Transparent Duration — with the default of 30 s you may simply be waiting for the first
  cycle to complete.
- On Windows, directory paths that contain non-ASCII characters (accented letters, etc.) require
  build 0.1.0 or later.

---

## License

[GPLv2](LICENSE) — the same license as OBS Studio itself.

Built from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).
