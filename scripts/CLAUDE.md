# scripts/ — Claude context

## Directory purpose

Utility scripts for the driftslide project. Currently contains one script: `prepare_images.py`.

## Language and runtime

All scripts are written in **Python 3** (minimum 3.10). No shell scripts. Dependencies are managed via system pip; the only current dependency beyond the stdlib is **Pillow ≥ 10.0**.

## prepare_images.py — key facts

### What it does (single-pass pipeline per file)

1. SHA-256 content hash → skip exact duplicates
2. `Image.convert("RGB")` → normalize color space
3. `Image.resize((1920, 1080), Image.LANCZOS)` → exact pixel dimensions, no aspect-ratio preservation
4. `ImageEnhance.Sharpness(img).enhance(1.1)` → 10% sharpen (factor 1.0 = original)
5. `img.save(dst, format="JPEG", quality=95, optimize=True)` → 95% quality JPEG

### I/O paths (both relative to repo root)

- Input: `files/input/`
- Output: `files/output/`
- Both paths are `pathlib.Path` constants at the top of the script: `INPUT_DIR`, `OUTPUT_DIR`

### Output filename pattern

`datetime.strftime("%Y%m%d-%H%M%S") + ".jpg"` → e.g. `20260620-073752.jpg`

This implements the spec pattern `%Y%M%D-%H%m%s` with corrected Python strftime codes (spec mixed up case conventions).

Timestamp source: `stat.st_birthtime` with fallback to `stat.st_mtime` (Linux has no reliable birth time in most configurations).

Timestamp collisions (two files with the same second): counter suffix appended → `20260620-073752-1.jpg`.

### Tunable constants (top of file)

```python
TARGET_SIZE = (1920, 1080)
SHARPEN_FACTOR = 1.1
JPEG_QUALITY = 95
SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".tif", ".webp"}
```

### Running

```bash
python3 scripts/prepare_images.py
```

No CLI flags. Does not clear output dir — delete existing output manually before re-running if a clean slate is needed.

## Conventions for new scripts

- One Python file per script, executable via `python3 scripts/<name>.py`
- Paths relative to repo root via `Path(__file__).parent.parent / ...`
- Content deduplication (if applicable): SHA-256 hash of raw bytes
- Image processing: Pillow only (ImageMagick not installed in this environment)
- No argparse unless the script genuinely needs CLI flags
- Document each script in a matching `scripts/<name>.md`
