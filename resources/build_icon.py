"""Build resources/app.ico from a square PNG with transparency.

Centers visible artwork in a square canvas with uniform padding so small
sizes (16x16 title bar) are not clipped on one side.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

DEFAULT_SOURCE = Path(__file__).with_name("app-source-no-background.png")
DEFAULT_OUTPUT = Path(__file__).with_name("app.ico")
ICO_SIZES = (256, 128, 64, 48, 32, 24, 16)
ALPHA_THRESHOLD = 16
SAFE_MARGIN_RATIO = 0.08


def content_bbox(img: Image.Image, threshold: int = ALPHA_THRESHOLD) -> tuple[int, int, int, int]:
    rgba = img.convert("RGBA")
    w, h = rgba.size
    pixels = rgba.load()
    min_x, min_y, max_x, max_y = w, h, -1, -1
    for y in range(h):
        for x in range(w):
            if pixels[x, y][3] > threshold:
                min_x = min(min_x, x)
                max_x = max(max_x, x)
                min_y = min(min_y, y)
                max_y = max(max_y, y)
    if max_x < 0:
        return (0, 0, w, h)
    return (min_x, min_y, max_x + 1, max_y + 1)


def fit_centered_square(img: Image.Image, margin_ratio: float = SAFE_MARGIN_RATIO) -> Image.Image:
    left, top, right, bottom = content_bbox(img)
    cropped = img.crop((left, top, right, bottom))
    cw, ch = cropped.size
    side = max(cw, ch)
    margin = max(1, int(round(side * margin_ratio)))
    canvas_side = side + 2 * margin
    canvas = Image.new("RGBA", (canvas_side, canvas_side), (0, 0, 0, 0))
    offset_x = margin + (side - cw) // 2
    offset_y = margin + (side - ch) // 2
    canvas.paste(cropped, (offset_x, offset_y), cropped)
    return canvas


def build_ico(source: Path, output: Path, margin_ratio: float) -> None:
    img = Image.open(source).convert("RGBA")
    square = fit_centered_square(img, margin_ratio)
    icons = [square.resize((s, s), Image.Resampling.LANCZOS) for s in ICO_SIZES]
    icons[0].save(
        output,
        format="ICO",
        sizes=[(icon.width, icon.height) for icon in icons],
        append_images=icons[1:],
    )
    print(f"Wrote {output} ({len(icons)} sizes) from {source}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate app.ico from a source PNG.")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="Input PNG")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Output .ico path")
    parser.add_argument(
        "--margin",
        type=float,
        default=SAFE_MARGIN_RATIO,
        help="Uniform padding around artwork as a fraction of content size (default 0.08)",
    )
    args = parser.parse_args()
    if not args.source.is_file():
        raise SystemExit(f"Source not found: {args.source}")
    build_ico(args.source, args.output, args.margin)


if __name__ == "__main__":
    main()
