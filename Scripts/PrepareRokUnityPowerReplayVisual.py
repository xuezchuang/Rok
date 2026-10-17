"""Install the bounded replay probe into the existing isolated Unity project.
Does not launch Unity or modify the source project.
"""
from pathlib import Path
import shutil
import hashlib
import json
import stat

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityBuiltinShaderProbe"
if ROOT.resolve() != Path("D:/ueDemo/Rok").resolve() or not (PROBE / "ProjectSettings/ProjectVersion.txt").is_file():
    raise RuntimeError("Expected the existing isolated UnityBuiltinShaderProbe project")


def copy_isolated(original, destination):
    destination = Path(destination)
    # Resolve existing links before changing attributes. Never touch source attrs
    # or a path escaping the verified isolated project's Assets directory.
    resolved = destination.resolve()
    resolved.relative_to(PROBE.resolve() / "Assets")
    if resolved == Path(original).resolve():
        raise RuntimeError("Source and isolated target must differ")
    if destination.exists():
        destination.chmod(destination.stat().st_mode | stat.S_IWRITE)
        # Windows rejects CREATE_ALWAYS (Python 'wb') on an existing Hidden
        # file even with Modify permission. OPEN_EXISTING ('r+b') updates it
        # without clearing Hidden/System attributes or changing source attrs.
        with open(original, "rb") as reader, destination.open("r+b") as writer:
            shutil.copyfileobj(reader, writer, length=1024 * 1024)
            writer.truncate()
    else:
        with open(original, "rb") as reader, destination.open("xb") as writer:
            shutil.copyfileobj(reader, writer, length=1024 * 1024)
    shutil.copystat(original, destination)
    # copy2 imports source read-only metadata too; keep only the isolated copy writable.
    destination.chmod(destination.stat().st_mode | stat.S_IWRITE)


editor = PROBE / "Assets/Editor"
editor.mkdir(parents=True, exist_ok=True)
copy_isolated(ROOT / "Scripts/ProbeRokUnityPowerReplayVisual.cs", editor / "ProbeRokUnityPowerReplayVisual.cs")
support = PROBE / "Assets/PowerGainSupport"
support.mkdir(parents=True, exist_ok=True)
source = ROOT / "ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/AutoDestory.cs"
for suffix in ("", ".meta"):
    copy_isolated(Path(str(source) + suffix), support / (source.name + suffix))
export = json.loads((ROOT / "Saved/RokUnityMigration/PowerGain/source_geometry.json").read_text(encoding="utf-8-sig"))
inputs = []
for entry in export["sources"]:
    original = (ROOT / entry["path"]).resolve()
    relative = original.relative_to((ROOT / "ref/client-unity/Assets").resolve())
    target = PROBE / "Assets/PowerGain" / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    for suffix in ("", ".meta"):
        copy_isolated(Path(str(original) + suffix), Path(str(target) + suffix))
        inputs.append({"path": str(original) + suffix, "sha256": hashlib.sha256(Path(str(original) + suffix).read_bytes()).hexdigest()})
for original in (source, Path(str(source) + ".meta"), ROOT / "Scripts/ProbeRokUnityPowerReplayVisual.cs"):
    inputs.append({"path": str(original), "sha256": hashlib.sha256(original.read_bytes()).hexdigest()})
output = ROOT / "Saved/RokUnityMigration/PowerGain/ReplayVisual"
output.mkdir(parents=True, exist_ok=True)
(output / "inputs.json").write_text(json.dumps({"inputs": inputs}, indent=2), encoding="utf-8")
print("Installed original AutoDestory with its original GUID and the isolated visual replay probe:", PROBE)
