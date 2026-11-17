"""Compare two UI captures pixel by pixel (WBP conversion gate G2).

Usage: py -3.12 Scripts/RokUi/PixelDiff.py <before.png> <after.png> [--out diff.png] [--tolerance 2]
       [--ignore x0,y0,x1,y1 ...]

Prints the number of pixels whose largest channel difference exceeds the
tolerance, the largest difference, and the bounding box of the differences.
With --out, writes a heatmap (differences amplified) for inspection by eye.
--ignore excludes a rectangle (inclusive pixel bounds), e.g. the HUD clock line,
which changes when a minute turns between two captures; the number of differing
pixels inside ignored rectangles is still printed.
Exit code 0 = identical within tolerance, 1 = different, 2 = not comparable.
"""

import argparse
import sys

import numpy as np
from PIL import Image


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--out")
    parser.add_argument("--tolerance", type=int, default=2)
    parser.add_argument("--ignore", action="append", default=[], metavar="x0,y0,x1,y1")
    args = parser.parse_args()

    before = np.asarray(Image.open(args.before).convert("RGB"), dtype=np.int16)
    after = np.asarray(Image.open(args.after).convert("RGB"), dtype=np.int16)
    if before.shape != after.shape:
        print(f"size mismatch: {before.shape[1]}x{before.shape[0]} vs {after.shape[1]}x{after.shape[0]}")
        return 2

    delta = np.abs(before - after).max(axis=2)
    mask = delta > args.tolerance
    ignored = 0
    for rect in args.ignore:
        x0, y0, x1, y1 = (int(v) for v in rect.split(","))
        region = mask[y0:y1 + 1, x0:x1 + 1]
        ignored += int(region.sum())
        region[...] = False
    count = int(mask.sum())
    total = mask.size
    print(f"pixels over tolerance {args.tolerance}: {count} of {total} ({100.0 * count / total:.4f}%)")
    if args.ignore:
        print(f"ignored (inside {len(args.ignore)} ignored rectangle(s)): {ignored}")
    print(f"largest channel difference: {int(delta.max())}")
    if count:
        ys, xs = np.nonzero(mask)
        print(f"difference bbox: x {xs.min()}..{xs.max()}, y {ys.min()}..{ys.max()}")

    if args.out:
        heat = np.clip(delta * 8, 0, 255).astype(np.uint8)
        base = (after.mean(axis=2) * 0.35).astype(np.uint8)
        image = np.stack([np.maximum(base, heat), base, base], axis=2)
        Image.fromarray(image).save(args.out)
        print(f"heatmap: {args.out}")
    return 0 if count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
