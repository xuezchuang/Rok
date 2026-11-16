"""Pack the Unity world-map troop/hero flipbooks into pivot-aligned runtime strips."""

import csv
import json
import re

from PIL import Image

import BuildRokUnityWorldEntityPresentation as entity
from rok_data_paths import DATA_ROOT, resolve

TROOP_ROOT = resolve("ref/client-unity/Assets/BundleAssets/Troop")
CSV_ROOT = resolve("ref/config-tables/编译客户端配置/CSV")
OUTPUT = DATA_ROOT / "WorldArmySprites"
POSES = ("idle", "walk", "attack")


def table(path):
    with path.open(encoding="utf-8-sig", newline="") as handle:
        rows = csv.DictReader(handle)
        next(rows)  # Unity CSV type row.
        return list(rows)


def pack_clip(sheet, rate, output_dir=OUTPUT, team_tint=(0.137255, 0.529411, 0.831372)):
    image = Image.open(sheet).convert("RGBA")
    geometry = entity.parse_packed_sprite_geometry(sheet)
    directions = [[] for _ in range(5)]
    for line in sheet.with_suffix(".tpsheet").read_text(encoding="utf-8-sig").splitlines():
        fields = line.split(";")
        match = re.fullmatch(re.escape(sheet.stem) + r"_([1-5])_(\d+)", fields[0])
        if not match:
            continue
        x, y, width, height = geometry[fields[0]]["rect"]
        left = round(width * float(fields[5]))
        bottom = round(height * float(fields[6]))
        crop = image.crop((x, image.height - y - height, x + width, image.height - y))
        # Same masked and premultiplied sprite representation as the existing world entities.
        # Apply RS.blue_troop to the shader's team-color channel before premultiplying.
        frame = entity.decode_unit_sprite(crop, geometry[fields[0]])
        if team_tint is not None:
            red, green, blue, opacity = frame.split()
            team_mask = crop.getchannel("A").point(lambda value: max(0, 2 * value - 255))
            channels = []
            for channel, tint in zip((red, green, blue), team_tint):
                tinted = channel.point(lambda value: min(255, round(value * 2 * tint)))
                channels.append(Image.composite(tinted, channel, team_mask))
            frame = Image.merge("RGBA", (*channels, opacity))
        directions[int(match[1]) - 1].append((int(match[2]), frame, left, bottom))
    counts = [len(frames) for frames in directions]
    if min(counts) == 0:
        raise ValueError(f"missing directions: {sheet}")

    replaced_frames = []
    for direction, frames in enumerate(directions):
        visible = [item for item in frames if item[1].getchannel("A").getbbox()]
        if not visible:
            raise ValueError(f"fully transparent direction {direction + 1}: {sheet}")
        for slot, (index, frame, _, _) in enumerate(frames):
            if not frame.getchannel("A").getbbox():
                replacement, replacement_image, x, y = min(visible, key=lambda item: abs(item[0] - index))
                frames[slot] = (index, replacement_image, x, y)
                replaced_frames.append([direction + 1, index, replacement])

    all_frames = [frame for frames in directions for frame in frames]
    left = max(frame[2] for frame in all_frames)
    bottom = max(frame[3] for frame in all_frames)
    width = left + max(frame.width - pivot for _, frame, pivot, _ in all_frames)
    height = bottom + max(frame.height - pivot for _, frame, _, pivot in all_frames)
    packed = Image.new("RGBA", (width * max(counts), height * len(directions)))
    for direction, frames in enumerate(directions):
        for index, (_, frame, x, y) in enumerate(sorted(frames, key=lambda item: item[0])):
            packed.paste(frame, (index * width + left - x, direction * height + height - bottom - frame.height + y))
    ppu_text = sheet.with_suffix(".png.meta").read_text(encoding="utf-8-sig")
    ppu = float(re.search(r"spritePixelsToUnits:\s*([\d.]+)", ppu_text)[1])
    output = output_dir / sheet.name
    packed.save(output)
    return {"sheet": output.name, "source": entity.rel(sheet), "width": width, "height": height,
            "pivot_x": left / width, "pivot_y": bottom / height, "ppu": ppu, "rate": rate, "counts": counts,
            "replaced_blank_frames": replaced_frames}


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    arms = {row["ID"]: [row["armsModel"], int(row["armsType"])] for row in table(CSV_ROOT / "Arms.csv") if row["armsModel"]}
    heroes = {row["ID"]: row["heroAction"] for row in table(CSV_ROOT / "Hero.csv") if row["heroAction"]}
    prefabs = {path.stem: path for path in TROOP_ROOT.glob("troop_unit_*/*.prefab")}
    sheets = {path.stem: path for path in TROOP_ROOT.glob("sprite_*/*/*/*.png")}
    units, missing = {}, []
    for model in sorted({model for model, _ in arms.values()} | set(heroes.values())):
        prefab = prefabs.get(model)
        if not prefab or any(f"{model}_{pose}" not in sheets for pose in POSES):
            missing.append(model)
            continue
        text = prefab.read_text(encoding="utf-8-sig")
        state = text.split("m_state:", 1)[1].split("m_shadow_state:", 1)[0]
        rates = [float(rate) for rate in re.findall(r"m_update_rate:\s*([\d.]+)", state)]
        if len(rates) < 3:
            missing.append(model)
            continue
        try:
            units[model] = {pose: pack_clip(sheets[f"{model}_{pose}"], rates[index]) for index, pose in enumerate(POSES)}
        except (OSError, ValueError, TypeError, KeyError) as exc:
            print(f"{model}: {exc}")
            missing.append(model)

    rule_sources = ("SquareMaxNumber", "SquareRowWidth", "SquareSpacing", "SquareNumberBySum", "SquareOffset")
    rules = {name: [{key: float(value) for key, value in row.items()} for row in table(CSV_ROOT / (name + ".csv"))
                    if row["group"] == "0"] for name in rule_sources}
    scale = json.loads((DATA_ROOT / "server_barbarian_presentation.json").read_text(encoding="utf-8"))["source_scale_curve"]
    result = {"units": units, "arms": arms, "heroes": heroes, "formation": rules, "scale": scale["keys"], "missing": missing}
    (OUTPUT / "index.json").write_text(json.dumps(result, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"World army sprites: {len(units)} models, {len(missing)} missing: {', '.join(missing)}")


if __name__ == "__main__":
    main()
