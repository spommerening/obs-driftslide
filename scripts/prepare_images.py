#!/usr/bin/env python3
"""Prepare images: deduplicate by content, resize to 1920x1080, sharpen, convert to JPEG 95%."""

import hashlib
import os
import sys
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageEnhance

INPUT_DIR = Path(__file__).parent.parent / "files" / "input"
OUTPUT_DIR = Path(__file__).parent.parent / "files" / "output"

TARGET_SIZE = (1920, 1080)
SHARPEN_FACTOR = 1.1  # 1.0 = original, 1.1 = 10% sharper
JPEG_QUALITY = 95

SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".tif", ".webp"}


def file_hash(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def creation_time(path: Path) -> datetime:
    """Return file creation time, falling back to modification time on Linux."""
    stat = path.stat()
    try:
        # st_birthtime is available on macOS and some Linux filesystems
        ts = stat.st_birthtime
    except AttributeError:
        ts = stat.st_mtime
    return datetime.fromtimestamp(ts)


def output_name(dt: datetime) -> str:
    # Pattern from spec: %Y%M%D-%H%m%s mapped to Python strftime:
    #   %Y=year, %m=month, %d=day, %H=hour, %M=minute, %S=second
    return dt.strftime("%Y%m%d-%H%M%S") + ".jpg"


def unique_dst(base: Path) -> Path:
    """Return base if it does not exist, otherwise append a counter suffix."""
    if not base.exists():
        return base
    stem = base.stem
    counter = 1
    while True:
        candidate = base.parent / f"{stem}-{counter}.jpg"
        if not candidate.exists():
            return candidate
        counter += 1


def process_image(src: Path, dst: Path) -> None:
    with Image.open(src) as img:
        img = img.convert("RGB")
        img = img.resize(TARGET_SIZE, Image.LANCZOS)
        img = ImageEnhance.Sharpness(img).enhance(SHARPEN_FACTOR)
        img.save(dst, format="JPEG", quality=JPEG_QUALITY, optimize=True)


def main() -> None:
    if not INPUT_DIR.is_dir():
        print(f"ERROR: Input directory not found: {INPUT_DIR}", file=sys.stderr)
        sys.exit(1)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    seen_hashes: set[str] = set()
    processed = 0
    skipped_duplicates = 0
    skipped_unsupported = 0
    conflicts_resolved = 0

    candidates = sorted(
        f for f in INPUT_DIR.iterdir()
        if f.is_file() and f.suffix.lower() in SUPPORTED_EXTENSIONS
    )

    unsupported = [f for f in INPUT_DIR.iterdir() if f.is_file() and f.suffix.lower() not in SUPPORTED_EXTENSIONS]
    for f in unsupported:
        print(f"SKIP (unsupported format): {f.name}")
        skipped_unsupported += 1

    for src in candidates:
        digest = file_hash(src)
        if digest in seen_hashes:
            print(f"SKIP (duplicate content): {src.name}")
            skipped_duplicates += 1
            continue
        seen_hashes.add(digest)

        dt = creation_time(src)
        base_dst = OUTPUT_DIR / output_name(dt)
        dst = unique_dst(base_dst)

        if dst != base_dst:
            conflicts_resolved += 1

        try:
            process_image(src, dst)
            print(f"OK: {src.name} -> {dst.name}")
            processed += 1
        except Exception as exc:
            print(f"ERROR: {src.name}: {exc}", file=sys.stderr)

    print(
        f"\nDone: {processed} processed, "
        f"{skipped_duplicates} duplicate(s) skipped, "
        f"{skipped_unsupported} unsupported file(s) skipped, "
        f"{conflicts_resolved} timestamp collision(s) resolved."
    )


if __name__ == "__main__":
    main()
