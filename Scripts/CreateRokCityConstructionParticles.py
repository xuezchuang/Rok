"""Import original Unity particle textures and publish the sampled world-space effects."""
import json
import re
import unreal

from BuildRokAcademyVisualSource import field, file_id, guid, vec, yaml_objects
from CreateRokCityResourcePresentationAssets import identity
from CreateRokCityStructures import import_asset
from rok_data_paths import DATA_ROOT, PROJECT_ROOT, resolve


PACKAGE = "/Game/RokUnity/City/ConstructionParticles"
OUTPUT = PROJECT_ROOT / "Saved/RokUnityMigration/CityConstructionParticles"
EDIT = unreal.MaterialEditingLibrary
ASSETS = PROJECT_ROOT / "ref/client-unity/Assets"


def create_material(row):
    source = ASSETS / row["texture"].removeprefix("Assets/Source/")
    texture = import_asset(source, f"{PACKAGE}/T_{source.stem}", True)
    name = "M_" + row["name"]
    asset_path = PACKAGE + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if unreal.EditorAssetLibrary.get_metadata_tag(asset, "RokConstructionParticleShader") == "1":
            return asset
        EDIT.delete_all_material_expressions(asset)
    else:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
    asset.set_editor_property("two_sided", True)
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE if row["additive"]
                              else unreal.BlendMode.BLEND_TRANSLUCENT)

    def node(cls, **properties):
        expression = EDIT.create_material_expression(asset, cls)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def connect(source, output, target, input_name):
        if not EDIT.connect_material_expressions(source, output, target, input_name):
            raise RuntimeError(f"{name}: connect {input_name}")

    color = node(unreal.MaterialExpressionTextureSample, texture=texture,
                 sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    vertex = node(unreal.MaterialExpressionVertexColor)
    uv = node(unreal.MaterialExpressionTextureCoordinate)
    custom = node(unreal.MaterialExpressionCustom, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                  code="return UV * Scale + Offset;")
    scale = node(unreal.MaterialExpressionConstant2Vector, r=row["scale"]["x"], g=row["scale"]["y"])
    offset = node(unreal.MaterialExpressionConstant2Vector, r=row["offset"]["x"],
                  g=1 - row["scale"]["y"] - row["offset"]["y"])
    inputs = []
    for label in ("UV", "Scale", "Offset"):
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", label)
        inputs.append(entry)
    custom.set_editor_property("inputs", inputs)
    connect(uv, "", custom, "UV")
    connect(scale, "", custom, "Scale")
    connect(offset, "", custom, "Offset")
    connect(custom, "", color, "UVs")
    tint = node(unreal.MaterialExpressionConstant3Vector,
                constant=unreal.LinearColor(*(row["color"][key] for key in "rgba")))
    vertex_rgb = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    product = node(unreal.MaterialExpressionMultiply)
    shaded = node(unreal.MaterialExpressionMultiply)
    opacity = node(unreal.MaterialExpressionMultiply)
    connect(color, "RGB", product, "A")
    connect(vertex, "", vertex_rgb, "")
    connect(vertex_rgb, "", product, "B")
    connect(product, "", shaded, "A")
    connect(tint, "", shaded, "B")
    connect(color, "A", opacity, "A")
    connect(vertex, "A", opacity, "B")
    if (not EDIT.connect_material_property(shaded, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR) or
            not EDIT.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)):
        raise RuntimeError(f"{name}: material outputs")
    EDIT.recompile_material(asset)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, "RokConstructionParticleShader", "1")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"{name}: save failed")
    return asset


def completion_offsets():
    # CityBuildingLevelUP uses the prefab effect child or GetMenuTargetGameObject.
    city_object = (ASSETS / "Scripts/Hotfix/Common/Utils/CityObjData.cs").read_text(encoding="utf-8-sig")
    sizes = re.search(r"SIZE_BUTTON = new\[\] \{ ([^}]+)\}", city_object)[1]
    heights = [float(height) for height in re.findall(r"new Vector3\(0, ([0-9.]+)f?, 0\)", sizes)]
    assert len(heights) == 9
    catalog_path = OUTPUT.parent / "CityBuildingVisualSources/catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    offsets = {}
    for row in catalog["models"]:
        nodes = row["prefab"]["nodes"]
        transforms = {node["transform"]["file_id"]: node["transform"] for node in nodes}
        effect = next((node for node in nodes if node["name"] == "effect"), None)
        if effect:
            transform = effect["transform"]
            assert transforms[transform["parent_transform_file_id"]]["parent_transform_file_id"] == 0
            offset = list(transform["local_position"].values())
        else:
            button = next(node for node in nodes if node["name"] == "button")
            parent = transforms[button["transform"]["parent_transform_file_id"]]
            assert transforms[parent["parent_transform_file_id"]]["parent_transform_file_id"] == 0
            assert list(parent["local_position"].values()) == [0, 0, 0]
            assert list(parent["local_scale"].values()) == [1, 1, 1]
            assert list(parent["local_rotation"].values()) == [0, 0, 0, 1]
            offset = [0, heights[row["footprint"]["width"] - 1], 0]
        if any(offset):
            offsets[row["model_id"]] = offset

    # The wall menu uses the first TownBuilding's first collider. Towers
    # default to Collider1 when no individual tower has been selected.
    town_script = guid(ASSETS / "Scripts/Client/CityManager/TownBuilding.cs")
    catalog = json.loads((DATA_ROOT / "CityConstructionSources/catalog.json").read_text(encoding="utf-8"))
    boundaries = {row["model_id"]: row for row in catalog["models"]
                  if row["type"] in (6, 8) and row["scene_prefab"]}
    for model, row in boundaries.items():
        objects = yaml_objects(resolve(row["scene_prefab"]))
        transforms = {file_id(body, "m_GameObject"): (fid, body) for kind, fid, body in objects if kind == 4}
        root, = (fid for fid, body in transforms.values() if not file_id(body, "m_Father"))
        names = {fid: field(body, "m_Name") for kind, fid, body in objects if kind == 1}
        effect = next((fid for fid, name in names.items() if name == "effect"), None)
        if effect is not None:
            anchor = effect
        elif row["type"] == 8:
            anchor, = (fid for fid, name in names.items() if name == "Collider1")
        else:
            def hierarchy_order(body):
                transform, pose = transforms[file_id(body, "m_GameObject")]
                if transform == root:
                    return -1
                assert file_id(pose, "m_Father") == root
                return int(field(pose, "m_RootOrder"))

            helper = min((body for kind, _, body in objects if kind == 114 and town_script in body),
                         key=hierarchy_order)
            collider = int(re.search(r"colliders:\s*\n  - \{fileID: (\d+)\}", helper)[1])
            anchor, = (file_id(body, "m_GameObject") for kind, fid, body in objects if kind == 65 and fid == collider)
        _, pose = transforms[anchor]
        assert file_id(pose, "m_Father") == root
        offsets[model] = list(vec(pose, "m_LocalPosition").values())
    return offsets


def apply():
    identity()
    source = json.loads((OUTPUT / "unity_particles.json").read_text(encoding="utf-8"))
    assert source["rate"] == 60 and source["duration"] == 2
    materials = {row["name"]: create_material(row).get_path_name() for row in source["materials"]}
    for effect in source["effects"]:
        effect["materials"] = [materials[name] for name in effect["materials"]]
    target = DATA_ROOT / "CityConstructionParticles/effects.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    offsets = completion_offsets()
    target.write_text(json.dumps({"schema": "rok.city_construction_particles.v1", **source,
                                  "completion_offsets": offsets},
                                 separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"Construction particle assets saved: {len(materials)} source materials, "
          f"{len(source['effects'])} effects, {len(offsets)} nonzero building offsets")


if __name__ == "__main__":
    apply()
