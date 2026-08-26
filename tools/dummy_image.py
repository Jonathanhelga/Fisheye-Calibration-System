#!/usr/bin/env python3
import argparse
import math
import os

from PIL import Image, ImageDraw

WIDTH = 1600
HEIGHT = 1200
CENTER_X = 812
CENTER_Y = 588
RADIUS = 540
RINGS = 9
SPOKES = 24
CHECKER_HALF = 64
CHECKER_CELL = 2


def build():
    image = Image.new("RGB", (WIDTH, HEIGHT), (14, 16, 18))
    draw = ImageDraw.Draw(image)

    draw.ellipse(
        [CENTER_X - RADIUS, CENTER_Y - RADIUS, CENTER_X + RADIUS, CENTER_Y + RADIUS],
        fill=(46, 52, 58),
        outline=(120, 132, 142),
    )

    for index in range(1, RINGS + 1):
        r = RADIUS * index / RINGS
        draw.ellipse(
            [CENTER_X - r, CENTER_Y - r, CENTER_X + r, CENTER_Y + r],
            outline=(150, 160, 170),
        )

    for index in range(SPOKES):
        angle = 2 * math.pi * index / SPOKES
        draw.line(
            [
                CENTER_X,
                CENTER_Y,
                CENTER_X + RADIUS * math.cos(angle),
                CENTER_Y + RADIUS * math.sin(angle),
            ],
            fill=(96, 106, 116),
        )

    pixels = image.load()
    for dy in range(-CHECKER_HALF, CHECKER_HALF):
        for dx in range(-CHECKER_HALF, CHECKER_HALF):
            on = ((dx // CHECKER_CELL) + (dy // CHECKER_CELL)) % 2 == 0
            pixels[CENTER_X + dx, CENTER_Y + dy] = (
                (232, 236, 240) if on else (24, 28, 32)
            )

    for offset in range(-10, 11):
        pixels[CENTER_X + offset, CENTER_Y] = (220, 40, 40)
        pixels[CENTER_X, CENTER_Y + offset] = (220, 40, 40)

    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "output",
        nargs="?",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "sample_shot.png"),
    )
    args = parser.parse_args()

    build().save(args.output)
    print(args.output)
    print(f"{WIDTH}x{HEIGHT}, true center at {CENTER_X},{CENTER_Y}")


if __name__ == "__main__":
    main()
