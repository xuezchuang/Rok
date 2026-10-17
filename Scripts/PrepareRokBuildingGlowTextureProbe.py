"""Prepare the isolated Unity 2019 star003 importer/GPU probe; never run Unity."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / "ref/client-unity"
PROJECT = ROOT / "Saved/UnityBuildingGlowTextureProbe"
OUT = ROOT / "Saved/RokUnityMigration/BuildingGlowTexture"
TEXTURE = "Assets/BundleAssets/Effect/UIsprite/shared_uisprite/star003.png"
MATERIAL = "Assets/BundleAssets/Effect/Materials/shared_effectmaterials/0UIEffect.mat"
SHADER = "Assets/BundleAssets/Map/Shader/UI-Add.shader"
EXPECTED = "9f159150cc1614e2eb38f88217433d2c68b1d28d0b73910ecd249f8f39c40388"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert PROJECT.resolve().parent == (ROOT / "Saved").resolve()
    assert OUT.resolve().parent == (ROOT / "Saved/RokUnityMigration").resolve()
    assert sha(CLIENT / TEXTURE) == EXPECTED
    source_report = ROOT / "Saved/RokUnityMigration/BuildingPreviewGlowSource/source_glow.json"
    design = ROOT / "Saved/RokUnityMigration/BuildingPreviewGlow/ue_render_design.json"
    assert source_report.is_file() and design.is_file()
    copied = []

    def copy(source, relative):
        target = PROJECT / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            import stat
            target.chmod(target.stat().st_mode | stat.S_IWRITE)
        shutil.copyfile(source, target)
        assert sha(source) == sha(target)
        copied.append({"source": source.relative_to(ROOT).as_posix(), "target": relative,
                       "sha256": sha(source)})

    for relative in (TEXTURE, TEXTURE + ".meta", MATERIAL, MATERIAL + ".meta", SHADER, SHADER + ".meta"):
        copy(CLIENT / relative, relative)
    for relative in ("ProjectSettings/ProjectSettings.asset", "ProjectSettings/ProjectVersion.txt"):
        copy(CLIENT / relative, relative)
    copy(ROOT / "Scripts/RenderRokBuildingGlowTextureProbe.cs", "Assets/Editor/RenderRokBuildingGlowTextureProbe.cs")
    shader = PROJECT / "Assets/Editor/RokGlowTextureCopy.shader"
    shader.write_text('''Shader "Hidden/RokGlowTextureCopy" {
Properties { _MainTex ("Texture", 2D) = "white" {} }
SubShader { Tags { "RenderType"="Opaque" } Cull Off ZWrite Off ZTest Always
Pass { CGPROGRAM
#pragma vertex vert
#pragma fragment frag
#include "UnityCG.cginc"
sampler2D _MainTex;
struct appdata { float4 vertex : POSITION; float2 uv : TEXCOORD0; };
struct v2f { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
v2f vert(appdata v) { v2f o; o.pos = UnityObjectToClipPos(v.vertex); o.uv = v.uv; return o; }
fixed4 frag(v2f i) : SV_Target { return tex2D(_MainTex, i.uv); }
ENDCG } }
Fallback Off }
''', encoding="utf-8", newline="\n")
    (PROJECT / "Packages").mkdir(exist_ok=True)
    (PROJECT / "Packages/manifest.json").write_text(json.dumps({"dependencies": {
        "com.unity.modules.imageconversion": "1.0.0", "com.unity.modules.ui": "1.0.0"}}, indent=2) + "\n", encoding="utf-8")
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"schema": "rok.building.glow_texture.prepare.v1", "project": str(PROJECT),
              "expected_unity": "2019.4.28f1c1", "expected_color_space": "Gamma",
              "source_png_sha256": EXPECTED, "source_report_sha256": sha(source_report),
              "ue_render_design_sha256": sha(design), "copies": copied,
              "copy_shader_sha256": sha(shader), "unity_launched": False,
              "unity_command": '"C:\\Program Files\\Unity\\Hub\\Editor\\2019.4.28f1c1\\Editor\\Unity.exe" -batchmode -quit -projectPath "' + str(PROJECT) + '" -executeMethod RenderRokBuildingGlowTextureProbe.Run -logFile "' + str(OUT / "unity.log") + '"',
              "validation_command": 'D:\\ue\\UE_5.8\\Engine\\Binaries\\ThirdParty\\Python3\\Win64\\python.exe Scripts\\VerifyRokBuildingGlowTextureProbe.py'}
    (OUT / "prepare.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unity = Path("C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data")
    csc = unity / "Tools/Roslyn/csc.exe"
    refs = list((unity / "Managed/UnityEngine").glob("*.dll"))
    refs += list((unity / "MonoBleedingEdge/lib/mono/4.7.1-api").glob("*.dll"))
    refs += list((unity / "MonoBleedingEdge/lib/mono/4.7.1-api/Facades").glob("*.dll"))
    refs.append(unity / "Managed/UnityEditor.dll")
    assert csc.is_file() and refs, "Unity 2019 compiler/assemblies missing"
    validation = PROJECT / "Validation"
    validation.mkdir(exist_ok=True)
    args = ["/nologo", "/target:library", "/define:UNITY_EDITOR",
            "/out:" + str(validation / "GlowTextureSyntax.dll")]
    args += ["/reference:" + str(p) for p in refs]
    args.append(str(PROJECT / "Assets/Editor/RenderRokBuildingGlowTextureProbe.cs"))
    response = validation / "compiler.rsp"
    response.write_text("\n".join('"' + arg + '"' for arg in args), encoding="utf-8")
    compiled = subprocess.run([str(csc), "@" + str(response)], capture_output=True)
    (validation / "compiler.log").write_bytes(compiled.stdout + compiled.stderr)
    (validation / "compiler_result.json").write_text(json.dumps({"exit_code": compiled.returncode,
        "unity_started": False, "scope": "New native renderer syntax only; no Unity editor launch."}, indent=2), encoding="utf-8")
    if compiled.returncode:
        raise RuntimeError("Unity 2019 C# syntax check failed; see " + str(validation / "compiler.log"))
    print(json.dumps({"prepared": True, "unity_launched": False, "project": str(PROJECT), "copies": len(copied)}))


if __name__ == "__main__":
    main()
