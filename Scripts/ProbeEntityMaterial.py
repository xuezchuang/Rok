import unreal

OUT = open(
    r"D:\ueDemo\Rok\Saved\RokUnityMigration\material_probe.txt", "w"
)


def report(msg):
    OUT.write(str(msg) + "\n")
    OUT.flush()


MAT = unreal.load_asset(
    "/Game/RokUnityWorldMapPrototype/Materials/"
    "M_RokWorldEntitySprite.M_RokWorldEntitySprite"
)
report(f"material: {MAT}")
if MAT:
    params = unreal.MaterialEditingLibrary.get_scalar_parameter_names(MAT)
    report(f"scalar params: {params}")
    textures = unreal.MaterialEditingLibrary.get_texture_parameter_names(
        MAT
    )
    report(f"texture params: {textures}")
    report(f"blend mode: {MAT.get_editor_property('blend_mode')}")
    report(f"shading model: {MAT.get_editor_property('shading_model')}")
    report(f"two sided: {MAT.get_editor_property('two_sided')}")

    tex = unreal.load_asset(
        r"D:\ueDemo\Rok\Saved\RokUnityMigration\WorldEntityPresentation"
        r"\Arms2_1_idle_d1_f0.png"
    )
    report(f"unit texture: {tex}")
    mid = unreal.MaterialInstanceDynamic.create(MAT, None)
    report(f"mid: {mid}")
    if mid and tex:
        mid.set_texture_parameter_value("Sprite", tex)
        report("set texture parameter: ok")
        mid.get_texture_parameter_value("Sprite")

OUT.close()
