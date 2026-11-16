"""Verify the staged world-army sprite strips and the config-to-model mappings."""

import json

from PIL import Image

from BuildRokWorldArmySprites import OUTPUT, POSES, resolve


def main():
    data = json.loads((OUTPUT / "index.json").read_text(encoding="utf-8"))
    mapped = set(data["heroes"].values()) | {model for model, _ in data["arms"].values()}
    assert mapped == set(data["units"]) | set(data["missing"])
    assert not (set(data["units"]) & set(data["missing"]))
    assert set(data["formation"]) == {
        "SquareMaxNumber", "SquareRowWidth", "SquareSpacing", "SquareNumberBySum", "SquareOffset"
    }
    assert all(data["formation"].values()) and len(data["scale"]) > 1

    frame_total = 0
    for model, clips in data["units"].items():
        assert set(clips) == set(POSES), model
        for pose, clip in clips.items():
            label = f"{model} {pose}"
            source = resolve(clip["source"])
            assert source.is_file() and source.with_suffix(".tpsheet").is_file(), label
            counts = clip["counts"]
            width, height = clip["width"], clip["height"]
            assert len(counts) == 5 and min(counts) > 0, label
            assert clip["ppu"] > 0 and clip["rate"] > 0, label
            assert all(0 <= clip[f"pivot_{axis}"] <= 1 for axis in "xy"), label
            with Image.open(OUTPUT / clip["sheet"]) as packed:
                assert packed.mode == "RGBA" and packed.size == (max(counts) * width, 5 * height), label
                alpha = packed.getchannel("A")
                for direction, count in enumerate(counts):
                    for frame in range(count):
                        x, y = frame * width, direction * height
                        assert alpha.crop((x, y, x + width, y + height)).getbbox(), (label, direction, frame)
                frame_total += sum(counts)

    print(f"Verified {len(data['units'])} world army models and {frame_total} frames; missing source: {', '.join(data['missing'])}")


if __name__ == "__main__":
    main()
