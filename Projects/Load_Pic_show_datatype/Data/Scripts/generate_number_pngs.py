#!/usr/bin/env python3

from __future__ import annotations

import argparse
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent.parent
OUTPUT_DIR = PROJECT_DIR / "Data" / "Pics"

IMAGE_SIZES = [16, 32, 64, 128, 256]
BIT_DEPTHS = [32, 24, 16]


def random_distinct_colors(count: int) -> list[tuple[int, int, int, int]]:
    colors: list[tuple[int, int, int, int]] = []
    used: set[tuple[int, int, int]] = set()

    while len(colors) < count:
        rgb = (random.randint(0, 255), random.randint(0, 255), random.randint(0, 255))
        if rgb in used:
            continue
        used.add(rgb)
        colors.append((rgb[0], rgb[1], rgb[2], 255))

    return colors


def contrast_text_color(bg: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    r, g, b, _ = bg
    luminance = 0.299 * r + 0.587 * g + 0.114 * b
    return (0, 0, 0, 255) if luminance > 160 else (255, 255, 255, 255)


def best_font(size: int = 28) -> ImageFont.ImageFont:
    candidates = [
        "DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "Arial.ttf",
    ]

    for candidate in candidates:
        try:
            return ImageFont.truetype(candidate, size=size)
        except OSError:
            continue

    return ImageFont.load_default()


def draw_centered_digit(
    number: int,
    image_size: tuple[int, int],
    bg_color: tuple[int, int, int, int],
    font: ImageFont.ImageFont,
) -> Image.Image:
    image = Image.new("RGBA", image_size, bg_color)
    draw = ImageDraw.Draw(image)

    text = str(number)
    text_color = contrast_text_color(bg_color)

    left, top, right, bottom = draw.textbbox((0, 0), text, font=font)
    text_w = right - left
    text_h = bottom - top

    x = (image_size[0] - text_w) // 2 - left
    y = (image_size[1] - text_h) // 2 - top

    draw.text((x, y), text, fill=text_color, font=font)
    return image


def quantize_rgb565(image: Image.Image) -> Image.Image:
    rgb = image.convert("RGB")
    pixels = rgb.load()

    for y in range(rgb.height):
        for x in range(rgb.width):
            r, g, b = pixels[x, y]
            r5 = (r >> 3) & 0x1F
            g6 = (g >> 2) & 0x3F
            b5 = (b >> 3) & 0x1F

            r8 = (r5 << 3) | (r5 >> 2)
            g8 = (g6 << 2) | (g6 >> 4)
            b8 = (b5 << 3) | (b5 >> 2)
            pixels[x, y] = (r8, g8, b8)

    return rgb


def convert_for_bit_depth(image: Image.Image, bit_depth: int) -> Image.Image:
    if bit_depth == 32:
        return image.convert("RGBA")
    if bit_depth == 24:
        return image.convert("RGB")
    if bit_depth == 16:
        return quantize_rgb565(image)
    raise ValueError(f"Unsupported bit depth: {bit_depth}")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate numbered PNG tiles for sizes and bit depths.")
    parser.add_argument(
        "--size",
        type=int,
        choices=IMAGE_SIZES,
        action="append",
        help="Generate only the given tile size. Can be passed multiple times.",
    )
    parser.add_argument(
        "--bit-depth",
        type=int,
        choices=BIT_DEPTHS,
        action="append",
        help="Generate only the given bit depth. Can be passed multiple times.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> None:
    args = parse_args(argv)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    random.seed()
    background_colors = random_distinct_colors(10)

    created_files = 0
    selected_sizes = args.size or IMAGE_SIZES
    selected_bit_depths = args.bit_depth or BIT_DEPTHS

    for image_edge in selected_sizes:
        image_size = (image_edge, image_edge)
        font = best_font(size=max(10, int(image_edge * 0.72)))

        for bit_depth in selected_bit_depths:
            variant_dir = OUTPUT_DIR / f"{image_edge}x{image_edge}" / f"{bit_depth}bit"
            variant_dir.mkdir(parents=True, exist_ok=True)

            for number in range(10):
                image = draw_centered_digit(number, image_size, background_colors[number], font)
                converted = convert_for_bit_depth(image, bit_depth)
                output_file = variant_dir / f"num_{number}.png"
                converted.save(output_file, format="PNG")
                created_files += 1

    print(
        f"Created {created_files} images in: {OUTPUT_DIR} "
        f"(sizes={selected_sizes}, bit_depths={selected_bit_depths})"
    )


if __name__ == "__main__":
    main()
