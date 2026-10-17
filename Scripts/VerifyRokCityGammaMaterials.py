"""Read-only UE5.8 verification of the three registered-RT Gamma materials."""

import json

import unreal

import CreateRokCityGammaMaterials as gamma


def main():
    sources = gamma.preflight()
    if gamma.sha_bytes(gamma.GAMMA_CODE.encode("utf-8")) == gamma.ORIGINAL_CODE_SHA or \
            gamma.DECODE_BLOCK in gamma.GAMMA_CODE or \
            "return float4(gammaColor, opacity);" not in gamma.GAMMA_CODE or \
            "float opacity = col.a * saturate(red2) * Transparency;" not in gamma.GAMMA_CODE or \
            any(term in gamma.GAMMA_CODE for term in ("SceneColor", "Background", "SceneTexture")):
        raise RuntimeError("Registered-RT Gamma shader code contract differs")
    rows = []
    for model in gamma.MODELS:
        target = unreal.load_asset(gamma.target_path(model))
        if target is None:
            raise RuntimeError(f"Gamma variant missing: {model}")
        row = gamma.verify_target(model, target, sources[model])
        row["asset_sha256"] = gamma.sha(gamma.source_path(row["asset"]))
        rows.append(row)
    data = {"schema": "rok.city_gamma_materials.verify.v1", "passed": True,
            "project": str(gamma.ROOT), "world": gamma.WORLD,
            "checkpoint_sha256": gamma.CHECKPOINT_SHA,
            "create_script_sha256": gamma.sha(gamma.ROOT / "Scripts/CreateRokCityGammaMaterials.py"),
            "scope": "Asset/source/graph verification only; no actor/map write, normal-view binding or rendered visual acceptance.",
            "old_source_materials_unchanged": True,
            "intended_for_registered_rt_only": True,
            "runtime_composite_visual_accepted": False,
            "models": rows}
    path = gamma.report(data, "verify")
    unreal.log("ROK_CITY_GAMMA_MATERIALS_VERIFY_PASS " + str(path))
    unreal.log(json.dumps(data, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
