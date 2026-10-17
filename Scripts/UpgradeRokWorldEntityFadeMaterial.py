"""Upgrade only the world entity material; never regenerate or save a map."""
from pathlib import Path
import sys
import unreal

EXPECTED_PROJECT = Path(r"D:\ueDemo\Rok").resolve()
actual_project = Path(unreal.Paths.project_dir()).resolve()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if actual_project != EXPECTED_PROJECT or not world or world.get_name() != "L_RokUnityWorldMapPrototype":
    raise RuntimeError(f"Wrong target: project={actual_project}, world={world.get_name() if world else None}")

sys.path.insert(0, str(EXPECTED_PROJECT / "Scripts"))
import CreateRokUnityWorldMapPrototype as factory

factory.REUSE_EXISTING_ASSETS = False
material, path = factory.create_entity_sprite_material()
unreal.log(f"Upgraded Rok world entity spawn fade material: {path}; defaults retain normal sprite opacity")
