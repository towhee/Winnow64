#!/usr/bin/env python3
"""Generate the tiny image fixtures used by the smoke and TSan test layers.

Run with any Python that has Pillow:
    python3 tests/fixtures/generate_fixtures.py

Images are intentionally tiny (kept small in the repo). The JPEG carries a few
EXIF tags so the smoke launch exercises Winnow's metadata reader, not just pixel
decoding.
"""
from pathlib import Path
from PIL import Image
from PIL.ExifTags import Base

OUT = Path(__file__).resolve().parent / "images"
OUT.mkdir(parents=True, exist_ok=True)

W, H = 64, 48


def gradient(seed: int) -> Image.Image:
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = ((x * 4 + seed) % 256, (y * 5 + seed) % 256, (x + y + seed) % 256)
    return img


def main() -> None:
    # JPEG with EXIF (Make / Model / DateTime / Orientation).
    jpg = gradient(10)
    exif = jpg.getexif()
    exif[Base.Make.value] = "WinnowTest"
    exif[Base.Model.value] = "Fixture Cam"
    exif[Base.DateTime.value] = "2026:05:23 12:00:00"
    exif[Base.Orientation.value] = 1
    jpg.save(OUT / "sample01.jpg", "JPEG", quality=90, exif=exif.tobytes())

    # TIFF (uncompressed) and PNG to cover other decode paths.
    gradient(80).save(OUT / "sample02.tif", "TIFF")
    gradient(150).save(OUT / "sample03.png", "PNG")

    for f in sorted(OUT.iterdir()):
        print(f"{f.name}: {f.stat().st_size} bytes")


if __name__ == "__main__":
    main()
