"""Import source-baked boundary meshes and animated Unity shelf layers in UE 5.8."""
import hashlib
import json
from pathlib import Path

import unreal

from CreateRokCityResourcePresentationAssets import CODE, identity
from rok_data_paths import DATA_ROOT, PROJECT_ROOT


INPUT = PROJECT_ROOT / "Saved/RokUnityMigration/CityStructures"
DESTINATION = "/Game/RokUnity/City/Structures"
EDIT = unreal.MaterialEditingLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def import_asset(filename, destination, is_texture, raw=False):
    asset = unreal.EditorAssetLibrary.load_asset(destination) if unreal.EditorAssetLibrary.does_asset_exist(destination) else None
    if asset:
        if is_texture and not asset.get_editor_property("never_stream"):
            asset.set_editor_property("never_stream", True)
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
                raise RuntimeError(f"Cannot save {destination}")
        return asset
    folder, name = destination.rsplit("/", 1)
    task = unreal.AssetImportTask()
    values = {"filename": str(filename), "destination_path": folder, "destination_name": name,
              "automated": True, "save": False}
    if is_texture:
        values["factory"] = unreal.TextureFactory()
    else:
        options = unreal.FbxImportUI()
        for key, value in {"import_mesh": True, "import_as_skeletal": False, "import_materials": False,
                           "import_textures": False, "automated_import_should_detect_type": False}.items():
            options.set_editor_property(key, value)
        data = options.get_editor_property("static_mesh_import_data")
        for key, value in {"combine_meshes": True, "generate_lightmap_u_vs": False,
                           "auto_generate_collision": False, "build_nanite": False}.items():
            data.set_editor_property(key, value)
        values.update(factory=unreal.FbxFactory(), options=options)
    for key, value in values.items():
        task.set_editor_property(key, value)
    ASSET_TOOLS.import_asset_tasks([task])
    asset = unreal.EditorAssetLibrary.load_asset(destination)
    if not isinstance(asset, unreal.Texture2D if is_texture else unreal.StaticMesh):
        raise RuntimeError(f"Import failed: {destination}")
    if is_texture:
        # The Unity source wall atlases and shelf textures are resident.
        asset.set_editor_property("never_stream", True)
    if is_texture and raw:
        asset.set_editor_property("srgb", False)
        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
        asset.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        asset.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        asset.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    elif not is_texture and "/Boundary/" in destination:
        asset.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Cannot save {destination}")
    return asset


def rgba(values):
    return unreal.LinearColor(*(values[key] for key in "rgba"))


def verify_texture_flags(texture, raw):
    if texture.get_editor_property("srgb") != (not raw):
        raise RuntimeError(f"Wrong color space for {texture.get_path_name()}")


def material(section, masked_sprite):
    keys = ["texture", "mask", "shader", "color", "team1", "team2", "uvScale", "uvOffset"]
    digest = hashlib.sha256(json.dumps([2 if masked_sprite else 3, {key: section[key] for key in keys}],
                                       sort_keys=True).encode()).hexdigest()[:16]
    folder = DESTINATION + "/Materials"
    name = "M_" + digest
    path = folder + "/" + name
    textures = {}
    for key in ("texture", "mask") if masked_sprite else ("texture",):
        file = Path(section[key])
        textures[key] = import_asset(file, DESTINATION + "/Textures/T_" + file.stem, True, masked_sprite)
        verify_texture_flags(textures[key], masked_sprite)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    asset = ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    asset.set_editor_property("two_sided", masked_sprite)
    if masked_sprite:
        asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    def node(cls, **properties):
        expression = EDIT.create_material_expression(asset, cls)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def sample(key, raw):
        return node(unreal.MaterialExpressionTextureSample, texture=textures[key],
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
                    if raw else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

    def vector(name, value):
        return node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=value)

    color = sample("texture", masked_sprite)
    lighting = vector("UnitySpriteColor", unreal.LinearColor(1.07203974, 1.07203974, 1.07203974, 1))
    if masked_sprite:
        mask = sample("mask", True)
        inputs = [
            ("MainSample", color, "RGBA"), ("MaskSample", mask, "RGBA"),
            ("Team1", vector("Color1", rgba(section["team1"])), "RGBA"),
            ("Team2", vector("Color2", rgba(section["team2"])), "RGBA"),
            ("Renderer", vector("RendererColor", rgba(section["color"])), "RGBA"),
            ("Lighting", lighting, "RGBA"),
            ("Transparency", node(unreal.MaterialExpressionConstant, r=1.0), ""),
        ]
        custom = node(unreal.MaterialExpressionCustom, code=CODE,
                      output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        custom_inputs = []
        for label, _, _ in inputs:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", label)
            custom_inputs.append(entry)
        custom.set_editor_property("inputs", custom_inputs)
        for label, upstream, output in inputs:
            if not EDIT.connect_material_expressions(upstream, output, custom, label):
                raise RuntimeError(f"Cannot connect {label}: {name}")
        rgb = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
        alpha = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
        EDIT.connect_material_expressions(custom, "", rgb, "")
        EDIT.connect_material_expressions(custom, "", alpha, "")
        EDIT.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        EDIT.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    else:
        if section["uvScale"] != {"x": 1, "y": 1} or section["uvOffset"] != {"x": 0, "y": 0}:
            raise RuntimeError(f"Unexpected boundary texture transform: {section['node']}")
        multiply = node(unreal.MaterialExpressionMultiply)
        EDIT.connect_material_expressions(color, "RGB", multiply, "A")
        EDIT.connect_material_expressions(lighting, "", multiply, "B")
        EDIT.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    EDIT.recompile_material(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Cannot save {path}")
    return asset


def apply():
    identity()
    source = json.loads((INPUT / "unity_structures.json").read_text(encoding="utf-8"))
    result = {"schema": "rok.city_structures.v1", "models": {}}
    counts = {}
    for model in source["models"]:
        model_id = model["id"]
        shelf = model_id.startswith("Build_shelf_")
        parts = []
        for section in model["sections"]:
            # The model owns the baked transform. Content hashes prevent stale
            # imports when the source prefab changes or gains a render section.
            geometry = hashlib.sha256(Path(section["obj"]).read_bytes()).hexdigest()[:12]
            path = f"{DESTINATION}/{'Scaffold' if shelf else 'Boundary'}/{model_id}/SM_{geometry}"
            mesh = import_asset(section["obj"], path, False)
            bounds = mesh.get_bounding_box()
            center = (bounds.min + bounds.max) * 0.5
            size = bounds.max - bounds.min
            if any(abs(getattr(center, axis) - section["center"][axis]) > 1.1
                   or abs(getattr(size, axis) - section["size"][axis]) > 2.1 for axis in "xyz"):
                # The OBJ importer may retain unused vertices from other submeshes.
                # Report the precise offending bounds before accepting a transformed mesh.
                raise RuntimeError(f"{path}: center={center} size={size}; source={section['center']}/{section['size']}")
            surface = material(section, bool(section["mask"]))
            parts.append({"mesh": mesh.get_path_name(), "material": surface.get_path_name(),
                          "order": section["order"], "poses": section.get("poses", []),
                          "sprite": bool(section["sprite"])})
        if not parts:
            raise RuntimeError(f"No imported parts for {model_id}")
        result["models"][model_id] = parts
        counts[model_id] = len(parts)
    target = DATA_ROOT / "CityStructures/manifest.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(result, separators=(",", ":")) + "\n", encoding="utf-8")
    (INPUT / "ue_import.json").write_text(json.dumps(counts, indent=2) + "\n", encoding="utf-8")
    print(f"Imported and bounds-verified {sum(counts.values())} parts across {len(counts)} original prefabs")


if __name__ == "__main__":
    apply()
