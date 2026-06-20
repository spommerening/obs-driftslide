# prepare_images.py

Batch image preparation pipeline: deduplicates source images by content hash, resizes them to 1920×1080, applies 10% sharpening, and exports them as high-quality JPEG files named after the source file's creation timestamp.

## Requirements

- Python 3.10+
- [Pillow](https://pillow.readthedocs.io/) ≥ 10.0 (`pip install Pillow`)

## Usage

```bash
python3 scripts/prepare_images.py
```

No arguments or flags are required. Input and output paths are fixed relative to the repository root.

| Path | Purpose |
|------|---------|
| `files/input/` | Source images (read-only) |
| `files/output/` | Processed JPEG output |

## Processing pipeline

Each source file goes through these steps in order:

1. **Content deduplication** — A SHA-256 hash of the raw file bytes is computed. If an identical hash was already seen earlier in the same run, the file is skipped entirely. This catches exact byte-level duplicates regardless of filename.

2. **Color-space normalization** — The image is converted to RGB, ensuring consistent output for source files that use palette mode (PNG with transparency, GIF, etc.).

3. **Resize to 1920×1080** — The image is resized to exactly 1920×1080 pixels using the Lanczos/bicubic resampling filter (`Image.LANCZOS`), which preserves edge detail better than bilinear resampling at the cost of slightly more CPU time.

4. **Sharpening (+10%)** — `PIL.ImageEnhance.Sharpness` is applied with a factor of `1.1` (1.0 = original, 2.0 = maximum sharpness). This counteracts the mild softening introduced by resampling.

5. **JPEG export at 95% quality** — The image is saved with `quality=95` and `optimize=True`. Quality 95 is considered visually lossless for photographic content while being significantly smaller than quality 100.

6. **Filename from creation time** — The output filename is derived from the source file's creation timestamp using the pattern `YYYYMMDD-HHmmss.jpg`.

## Output naming

### Timestamp pattern

The filename pattern specified is `%Y%M%D-%H%m%s.jpg`. The mapping to Python `datetime.strftime` format codes:

| Spec token | Intended field | Python strftime code |
|-----------|---------------|----------------------|
| `%Y`      | 4-digit year  | `%Y`                 |
| `%M`      | Month (01–12) | `%m`                 |
| `%D`      | Day (01–31)   | `%d`                 |
| `%H`      | Hour (00–23)  | `%H`                 |
| `%m`      | Minute (00–59)| `%M`                 |
| `%s`      | Second (00–59)| `%S`                 |

Resulting Python format string used in code: `"%Y%m%d-%H%M%S"` → example: `20260620-073752.jpg`

### Creation time on Linux

POSIX filesystems do not reliably expose file birth (creation) time. The script uses `stat.st_birthtime` when available (macOS, and Linux kernels ≥ 4.11 on ext4/btrfs with `statx`). On all other Linux configurations it falls back to `st_mtime` (last modification time), which is typically the download or copy time.

### Timestamp collisions

If two different source images share the same creation timestamp (down to the second), the script appends a numeric counter to avoid overwriting the first output:

```
20260620-073752.jpg      ← first image at that second
20260620-073752-1.jpg    ← second image at same second
20260620-073752-2.jpg    ← third image at same second
```

## Supported source formats

Any format Pillow can open: `.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.tiff`, `.tif`, `.webp`. Files with other extensions are reported as skipped but do not cause an error.

## Console output

```
OK: ChatGPT Image 20. Juni 2026, 00_04_02.png -> 20260620-000402.jpg
SKIP (duplicate content): ChatGPT Image 20. Juni 2026, 00_05_54.png
OK: ChatGPT Image 20. Juni 2026, 00_08_20.png -> 20260620-000820.jpg
...
Done: 32 processed, 1 duplicate(s) skipped, 0 unsupported file(s) skipped, 0 timestamp collision(s) resolved.
```

## Re-running the script

The script does **not** clear the output directory before running. Output files are written fresh each time, overwriting any existing file with the same name. To start clean:

```bash
rm files/output/*.jpg && python3 scripts/prepare_images.py
```

## Configuration

All tuneable constants are defined at the top of the script:

| Constant | Default | Description |
|----------|---------|-------------|
| `INPUT_DIR` | `files/input/` | Source directory (relative to repo root) |
| `OUTPUT_DIR` | `files/output/` | Destination directory |
| `TARGET_SIZE` | `(1920, 1080)` | Output pixel dimensions |
| `SHARPEN_FACTOR` | `1.1` | Pillow sharpness factor (1.0 = original) |
| `JPEG_QUALITY` | `95` | JPEG quality (1–95, 95 = visually lossless) |
| `SUPPORTED_EXTENSIONS` | see script | Set of lowercase file extensions to process |
