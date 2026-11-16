"""Pack the city worker clips referenced by CityGlobalMediator.CreateCityWorker."""

import json

from BuildRokWorldArmySprites import pack_clip
from rok_data_paths import DATA_ROOT, resolve


def main():
    source = resolve("ref/client-unity/Assets/BundleAssets/Monster")
    output = DATA_ROOT / "CityLife"
    output.mkdir(parents=True, exist_ok=True)
    units = {}
    for model in ("NPC_03", "NPC_04"):
        stem = f"{model}_walk"
        units[model] = {"walk": pack_clip(source / model / stem / f"{stem}.png", 0.12, output, None)}
    (output / "index.json").write_text(json.dumps({"units": units}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
