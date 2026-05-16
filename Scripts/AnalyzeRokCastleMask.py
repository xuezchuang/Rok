import argparse
import csv
import json
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SPRITE = PROJECT_ROOT / "ref" / "resources" / "Sprite" / "Castle_6_5.png"
DEFAULT_MASK = PROJECT_ROOT / "ref" / "resources" / "Sprite" / "Castle_6_5_mask.png"
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityPlacement" / "mask_analysis" / "Castle_6_5"


def relative_path(path):
    try:
        return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def channel_bbox(values, width, height, threshold):
    min_x = width
    min_y = height
    max_x = -1
    max_y = -1
    count = 0
    for y in range(height):
        row = y * width
        for x in range(width):
            if values[row + x] >= threshold:
                count += 1
                if x < min_x:
                    min_x = x
                if y < min_y:
                    min_y = y
                if x > max_x:
                    max_x = x
                if y > max_y:
                    max_y = y
    if count == 0:
        return count, None
    return count, [min_x, min_y, max_x, max_y]


def save_channel_images(mask_image, color_image, channel_name, threshold, output_dir):
    channel = mask_image.getchannel(channel_name)
    grayscale_path = output_dir / "Castle_6_5_mask_{0}_grayscale.png".format(channel_name)
    threshold_path = output_dir / "Castle_6_5_mask_{0}_threshold_{1}.png".format(channel_name, threshold)
    overlay_path = output_dir / "Castle_6_5_mask_{0}_alpha_overlay.png".format(channel_name)

    channel.save(grayscale_path)
    threshold_image = channel.point(lambda value: 255 if value >= threshold else 0)
    threshold_image.save(threshold_path)

    overlay = color_image.copy()
    overlay.putalpha(channel)
    overlay.save(overlay_path)

    return {
        "grayscale": relative_path(grayscale_path),
        "threshold": relative_path(threshold_path),
        "alpha_overlay": relative_path(overlay_path),
    }


def analyze_channel(mask_image, color_image, channel_name, threshold, output_dir):
    channel = mask_image.getchannel(channel_name)
    values = channel.tobytes()
    width, height = mask_image.size
    active_count, bbox = channel_bbox(values, width, height, threshold)
    total = width * height
    image_paths = save_channel_images(mask_image, color_image, channel_name, threshold, output_dir)
    return {
        "channel": channel_name,
        "min": min(values),
        "max": max(values),
        "mean": round(sum(values) / float(max(1, total)), 4),
        "threshold": threshold,
        "active_pixels": active_count,
        "coverage": round(active_count / float(max(1, total)), 6),
        "bbox": bbox,
        "outputs": image_paths,
    }


def write_reports(summary, output_dir):
    json_path = output_dir / "Castle_6_5_mask_channel_analysis.json"
    csv_path = output_dir / "Castle_6_5_mask_channel_analysis.csv"
    json_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    with csv_path.open("w", encoding="utf-8", newline="") as csv_file:
        writer = csv.DictWriter(
            csv_file,
            fieldnames=["channel", "min", "max", "mean", "threshold", "active_pixels", "coverage", "bbox"],
        )
        writer.writeheader()
        for item in summary["channels"]:
            writer.writerow(
                {
                    "channel": item["channel"],
                    "min": item["min"],
                    "max": item["max"],
                    "mean": item["mean"],
                    "threshold": item["threshold"],
                    "active_pixels": item["active_pixels"],
                    "coverage": item["coverage"],
                    "bbox": item["bbox"],
                }
            )

    summary["reports"] = {
        "json": relative_path(json_path),
        "csv": relative_path(csv_path),
    }


def parse_args():
    parser = argparse.ArgumentParser(description="Analyze Castle_6_5_mask.png channels without assuming any channel is alpha.")
    parser.add_argument("--sprite", default=str(DEFAULT_SPRITE), help="Raw Castle_6_5 color sprite.")
    parser.add_argument("--mask", default=str(DEFAULT_MASK), help="Castle_6_5 mask texture.")
    parser.add_argument("--out", default=str(DEFAULT_OUTPUT_DIR), help="Output directory for channel diagnostics.")
    parser.add_argument("--threshold", type=int, default=128, help="Threshold used for bbox and coverage.")
    return parser.parse_args()


def main():
    args = parse_args()
    sprite_path = Path(args.sprite)
    mask_path = Path(args.mask)
    output_dir = Path(args.out)
    threshold = max(0, min(255, int(args.threshold)))

    if not sprite_path.exists():
        raise FileNotFoundError("Missing raw sprite: {0}".format(sprite_path))
    if not mask_path.exists():
        raise FileNotFoundError("Missing mask texture: {0}".format(mask_path))

    output_dir.mkdir(parents=True, exist_ok=True)
    color_image = Image.open(sprite_path).convert("RGBA")
    mask_image = Image.open(mask_path).convert("RGBA")
    if color_image.size != mask_image.size:
        raise RuntimeError("Sprite and mask sizes differ: sprite={0}, mask={1}".format(color_image.size, mask_image.size))

    summary = {
        "sprite": relative_path(sprite_path),
        "mask": relative_path(mask_path),
        "size": [mask_image.size[0], mask_image.size[1]],
        "threshold": threshold,
        "rule": "Diagnostic only. Do not assume R, G, B, or A is the opacity mask until the Unity shader/material semantics are confirmed.",
        "channels": [],
    }
    for channel_name in ("R", "G", "B", "A"):
        summary["channels"].append(analyze_channel(mask_image, color_image, channel_name, threshold, output_dir))

    write_reports(summary, output_dir)

    print("Mask analysis written to {0}".format(relative_path(output_dir)))
    print("channel,min,max,mean,threshold,active_pixels,coverage,bbox")
    for item in summary["channels"]:
        print(
            "{channel},{min},{max},{mean},{threshold},{active_pixels},{coverage},{bbox}".format(
                **item
            )
        )
    print("json={0}".format(summary["reports"]["json"]))
    print("csv={0}".format(summary["reports"]["csv"]))


if __name__ == "__main__":
    main()
