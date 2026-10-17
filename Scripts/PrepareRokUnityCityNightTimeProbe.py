"""Prepare or strictly verify an isolated Unity2019 same-command-buffer Night _Time.y GPU probe.

This script never launches Unity. The root runner starts the editor against the prepared project.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
import struct
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "Saved/UnityCityNightTimeProbeV1"
V2 = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2"
V2_CONTRACT = V2 / "state_contract.json"
V2_CONTRACT_SHA = "9555b7e77ff67dad29789923109cde5574358dbc4cc7072e174b0f3987f27a49"
V2_NATIVE = V2 / "OutputRuns/20261008_234412_007_44320/native_state.json"
V2_NATIVE_SHA = "671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71"
V2_VERIFY = V2_NATIVE.parent / "native_verify.json"
V2_VERIFY_SHA = "ab5e63f472ee14980e93299492114e5d11cc0405dca3288d8339ea16e6c3686e"
SCHEMA = "rok.unity_city_night_time_contract.v1"
NATIVE_SCHEMA = "rok.unity_city_night_time_native.v1"
STUB = (V2 / "Assets/ProbeDependencyStubs.cs").read_bytes()

TIME_SHADER = r'''Shader "Hidden/RokCityNightVertexTimeReadback" {
    SubShader { Tags { "RenderType"="Opaque" }
        Pass { Cull Off ZWrite Off ZTest Always Blend Off
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 3.0
            #include "UnityCG.cginc"
            struct appdata { float4 vertex : POSITION; };
            struct v2f { float4 position : SV_POSITION; float shaderTimeY : TEXCOORD0; };
            v2f vert(appdata input) {
                v2f output;
                output.position = UnityObjectToClipPos(input.vertex);
                output.shaderTimeY = _Time.y;
                return output;
            }
            float4 frag(v2f input) : SV_Target { return float4(input.shaderTimeY, 0, 0, 1); }
            ENDCG
        }
    }
    Fallback Off
}
'''

EXPORTER = r'''using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using Client;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

// Only the root process runs this isolated Unity2019 Gamma GPU measurement.
public static class ExportRokUnityCityNightTimeProbe
{
    private const string ProbeSuffix = "/Saved/UnityCityNightTimeProbeV1/Assets";
    private const string PrefabPath = "Assets/Source/BundleAssets/Scene/City_building/city_building_1/build_1_101_1.prefab";
    private const string MaterialPath = "Assets/Source/BundleAssets/Scene/City/city_function/city_building_night.mat";
    [Serializable] private sealed class SourceFile {
        public string source_path, source_sha256, meta_sha256, source_guid, probe_path;
    }
    [Serializable] private sealed class SettingFile { public string name, sha256; }
    [Serializable] private sealed class Contract {
        public string schema, repo_root, probe_root, unity_version, source_color_space;
        public string v2_contract_sha256, v2_native_sha256, v2_verify_sha256;
        public string exporter_sha256, time_shader_sha256, stub_sha256;
        public SourceFile[] source_files;
        public SettingFile[] project_settings;
    }
    [Serializable] private sealed class Output {
        public string schema = "rok.unity_city_night_time_native.v1";
        public string run_directory, unity_version, color_space, graphics_device, source_contract_sha256;
        public string exporter_sha256, time_shader_sha256, source_prefab_guid, source_material_guid;
        public string source_renderer_file_id, source_mask_sprite_path, source_mask_sprite_file_id;
        public string source_shader_name, source_mask_binding_scope, draw_order, time_source_scope;
        public string time_before_bin, time_before_sha256, time_after_bin, time_after_sha256;
        public string b1_png, b1_sha256, b0_png, b0_sha256, background_png, background_sha256;
        public string b1_b_png, b1_b_sha256, b0_b_png, b0_b_sha256, background_b_png, background_b_sha256;
        public string background_a_control, background_b_control;
        public string target_format, time_target_format, time_readback_format, failure;
        public float shader_time_before, shader_time_after, cpu_realtime_before, cpu_realtime_after;
        public bool complete, game_app_loaded, source_mask_property_block_matches;
        public int source_queue, source_sprite_vertices, source_sprite_triangles;
    }
    private static void Need(bool ok, string why) { if (!ok) throw new InvalidOperationException(why); }
    private static string Normal(string p) { return Path.GetFullPath(p).Replace('\\', '/').TrimEnd('/'); }
    private static string Sha(string path) {
        using (var input = File.OpenRead(path)) using (var hasher = SHA256.Create())
            return BitConverter.ToString(hasher.ComputeHash(input)).Replace("-", "").ToLowerInvariant();
    }
    private static string Guid(string assetPath) { return AssetDatabase.AssetPathToGUID(assetPath).ToLowerInvariant(); }
    private static string FileId(UnityEngine.Object value) {
        string guid; long id = 0;
        Need(value && AssetDatabase.TryGetGUIDAndLocalFileIdentifier(value, out guid, out id), "Unity source fileID missing");
        return id.ToString(CultureInfo.InvariantCulture);
    }
    private static Mesh Quad() {
        var mesh = new Mesh(); mesh.name = "night_time_diagnostic_quad_not_source_sprite_mesh";
        mesh.vertices = new [] { new Vector3(-1,-1,0), new Vector3(1,-1,0),
            new Vector3(1,1,0), new Vector3(-1,1,0) };
        mesh.uv = new [] { new Vector2(0,0), new Vector2(1,0), new Vector2(1,1), new Vector2(0,1) };
        mesh.colors32 = new [] { new Color32(255,255,255,255), new Color32(255,255,255,255),
            new Color32(255,255,255,255), new Color32(255,255,255,255) };
        mesh.triangles = new [] { 0,2,1,0,3,2 };
        mesh.RecalculateNormals();
        return mesh;
    }
    private static Texture2D Constant(Color32 color) {
        var texture = new Texture2D(2, 2, TextureFormat.RGBA32, false, false);
        texture.filterMode = FilterMode.Point; texture.wrapMode = TextureWrapMode.Clamp;
        texture.SetPixels32(new [] { color,color,color,color }); texture.Apply(false,false);
        return texture;
    }
    private static string SavePng(RenderTexture target, string output, string name) {
        var previous = RenderTexture.active; Texture2D pixels = null;
        try {
            RenderTexture.active = target;
            pixels = new Texture2D(128,128,TextureFormat.RGBA32,false,false);
            pixels.ReadPixels(new Rect(0,0,128,128),0,0,false); pixels.Apply(false,false);
            string file = Path.Combine(output,name); File.WriteAllBytes(file,pixels.EncodeToPNG()); return Sha(file);
        } finally { RenderTexture.active = previous; if (pixels) UnityEngine.Object.DestroyImmediate(pixels); }
    }
    private static float SaveTime(RenderTexture target, string output, string name, out string digest) {
        var previous = RenderTexture.active; Texture2D pixels = null;
        try {
            RenderTexture.active = target;
            pixels = new Texture2D(1,1,TextureFormat.RGBAFloat,false,true);
            pixels.ReadPixels(new Rect(0,0,1,1),0,0,false); pixels.Apply(false,false);
            byte[] raw = pixels.GetRawTextureData();
            Need(raw.Length == 16 && BitConverter.IsLittleEndian, "RGBAFloat time readback layout differs");
            string file = Path.Combine(output,name); File.WriteAllBytes(file,raw); digest = Sha(file);
            float value = BitConverter.ToSingle(raw,0);
            Need(!float.IsNaN(value) && !float.IsInfinity(value) && value >= 0, "GPU _Time.y is not finite/nonnegative");
            return value;
        } finally { RenderTexture.active = previous; if (pixels) UnityEngine.Object.DestroyImmediate(pixels); }
    }
    public static void Run() {
        string assets = Normal(Application.dataPath);
        Need(assets.EndsWith(ProbeSuffix, StringComparison.OrdinalIgnoreCase) &&
            Application.unityVersion == "2019.4.28f1c1" && QualitySettings.activeColorSpace == ColorSpace.Gamma &&
            SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D11 &&
            SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.ARGBFloat), "wrong isolated Unity2019 Gamma/D3D11/float target");
        string probe = assets.Substring(0,assets.Length-"/Assets".Length);
        string repo = assets.Substring(0,assets.Length-ProbeSuffix.Length);
        string contractPath = Path.Combine(probe,"night_time_contract.json");
        var contract = JsonUtility.FromJson<Contract>(File.ReadAllText(contractPath,Encoding.UTF8));
        Need(contract != null && contract.schema == "rok.unity_city_night_time_contract.v1" &&
            Normal(contract.repo_root) == repo && Normal(contract.probe_root) == probe &&
            Sha(Path.Combine(probe,"Assets/Editor/ExportRokUnityCityNightTimeProbe.cs")) == contract.exporter_sha256,
            "night time contract/project identity differs");
        string outputDir = Path.Combine(probe,"OutputRuns",DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") +
            "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        Need(!Directory.Exists(outputDir),"run already exists"); Directory.CreateDirectory(outputDir);
        var output = new Output { run_directory = Normal(outputDir), unity_version = Application.unityVersion,
            color_space = QualitySettings.activeColorSpace.ToString(), graphics_device = SystemInfo.graphicsDeviceType.ToString(),
            source_contract_sha256 = Sha(contractPath), exporter_sha256 = contract.exporter_sha256,
            time_shader_sha256 = contract.time_shader_sha256, game_app_loaded = false,
            target_format = "ARGB32 RenderTextureReadWrite.Default Gamma", time_target_format = "ARGBFloat RenderTextureReadWrite.Linear",
            time_readback_format = "Texture2D RGBAFloat 1x1 little-endian raw float32" };
        int exitCode = 1;
        try {
            Need(Sha(Path.Combine(probe,"Assets/Editor/ExportRokUnityCityNightTimeProbe.cs")) == contract.exporter_sha256 &&
                Sha(Path.Combine(probe,"Assets/ProbeNightTime.shader")) == contract.time_shader_sha256 &&
                Sha(Path.Combine(probe,"Assets/ProbeDependencyStubs.cs")) == contract.stub_sha256,
                "copied exporter/shader/stub differs");
            foreach (var file in contract.source_files) {
                Need(Sha(Path.Combine(repo,file.source_path)) == file.source_sha256 &&
                    Sha(Path.Combine(probe,file.probe_path)) == file.source_sha256 &&
                    Sha(Path.Combine(repo,file.source_path + ".meta")) == file.meta_sha256 &&
                    Sha(Path.Combine(probe,file.probe_path + ".meta")) == file.meta_sha256 &&
                    Guid(file.probe_path) == file.source_guid, "source asset/GUID changed: " + file.source_path);
            }
            foreach (var setting in contract.project_settings)
                Need(Sha(Path.Combine(probe,"ProjectSettings",setting.name)) == setting.sha256,
                    "isolated project setting differs from sealed V2: " + setting.name);
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(PrefabPath);
            var source = AssetDatabase.LoadAssetAtPath<Material>(MaterialPath);
            var timeShader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/ProbeNightTime.shader");
            Need(prefab && source && source.shader && source.shader.name == "custom/map/cm_city_mask_night" &&
                source.renderQueue == 2996 && timeShader && timeShader.isSupported, "night source material/shader unavailable");
            var renderers = prefab.GetComponentsInChildren<SpriteRenderer>(true).Where(r => r.name == "sprite").ToArray();
            Need(renderers.Length == 1 && renderers[0].sprite, "source night sprite renderer ambiguous");
            output.source_prefab_guid = Guid(PrefabPath); output.source_material_guid = Guid(MaterialPath);
            output.source_renderer_file_id = FileId(renderers[0]); output.source_shader_name = source.shader.name;
            output.source_queue = source.renderQueue; output.source_sprite_vertices = renderers[0].sprite.vertices.Length;
            output.source_sprite_triangles = renderers[0].sprite.triangles.Length / 3;
            var instance = UnityEngine.Object.Instantiate(prefab);
            try {
                var sr = instance.GetComponentsInChildren<SpriteRenderer>(true).Single(r => r.name == "sprite");
                var mask = sr.GetComponent<MaskSprite>();
                Need(mask && mask.m_mask_sprite, "source MaskSprite missing");
                var start = typeof(MaskSprite).GetMethod("Start",BindingFlags.Instance|BindingFlags.NonPublic);
                Need(start != null,"source MaskSprite.Start missing"); start.Invoke(mask,null);
                var block = new MaterialPropertyBlock(); sr.GetPropertyBlock(block);
                output.source_mask_property_block_matches = block.GetTexture("_Mask") == mask.m_mask_sprite.texture;
                output.source_mask_sprite_path = AssetDatabase.GetAssetPath(mask.m_mask_sprite.texture);
                output.source_mask_sprite_file_id = FileId(mask.m_mask_sprite);
                output.source_mask_binding_scope = "Exact copied source MaskSprite.Start invocation; isolated, not GameApp lifecycle";
                Need(output.source_mask_property_block_matches,"source MaskSprite did not bind _Mask");
            } finally { UnityEngine.Object.DestroyImmediate(instance); }
            Mesh quad = null; Texture2D main = null, blue = null, noBlue = null;
            Material b1Mat = null, b0Mat = null, timeMat = null;
            RenderTexture dummy = null, b1 = null, b0 = null, background = null;
            RenderTexture b1b = null, b0b = null, backgroundb = null, t0 = null, t1 = null;
            Camera camera = null; GameObject cameraObject = null; CommandBuffer commands = null;
            try {
                quad = Quad(); main = Constant(new Color32(205,83,41,128));
                blue = Constant(new Color32(255,0,255,255));
                noBlue = Constant(new Color32(255,0,0,255));
                b1Mat = new Material(source); b1Mat.SetTexture("_MainTex",main); b1Mat.SetTexture("_Mask",blue);
                b0Mat = new Material(source); b0Mat.SetTexture("_MainTex",main); b0Mat.SetTexture("_Mask",noBlue);
                timeMat = new Material(timeShader);
                Shader.SetGlobalColor("_SpriteColor",Color.white); Shader.SetGlobalFloat("_CityTransparency",1);
                dummy = new RenderTexture(128,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
                b1 = new RenderTexture(128,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
                b0 = new RenderTexture(128,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
                background = new RenderTexture(128,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
                b1b = new RenderTexture(128,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
                b0b = new RenderTexture(128,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
                backgroundb = new RenderTexture(128,128,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
                t0 = new RenderTexture(1,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);
                t1 = new RenderTexture(1,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);
                foreach (var rt in new [] { dummy,b1,b0,background,b1b,b0b,backgroundb,t0,t1 })
                    { rt.antiAliasing = 1; Need(rt.Create(),"RT create failed"); }
                cameraObject = new GameObject("isolated_night_time_camera");
                camera = cameraObject.AddComponent<Camera>(); camera.enabled = false; camera.orthographic = true;
                camera.orthographicSize = 1.25f; camera.aspect = 1f;
                camera.nearClipPlane = 0.1f; camera.farClipPlane = 10f;
                camera.transform.position = new Vector3(0,0,-5); camera.transform.rotation = Quaternion.identity;
                camera.clearFlags = CameraClearFlags.SolidColor;
                camera.backgroundColor = new Color(0.07f,0.15f,0.71f,1);
                camera.allowHDR = false; camera.allowMSAA = false; camera.targetTexture = dummy;
                commands = new CommandBuffer { name = "night_source_B1_B0_and_TimeY_same_camera_command_buffer" };
                Color bg = camera.backgroundColor;
                Color bgB = new Color(0.74f,0.09f,0.05f,1);
                output.background_a_control = "RGBA float (0.07,0.15,0.71,1)";
                output.background_b_control = "RGBA float (0.74,0.09,0.05,1)";
                commands.SetRenderTarget(t0); commands.ClearRenderTarget(true,true,Color.clear);
                commands.DrawMesh(quad,Matrix4x4.identity,timeMat,0,0);
                commands.SetRenderTarget(background); commands.ClearRenderTarget(true,true,bg);
                commands.SetRenderTarget(b1); commands.ClearRenderTarget(true,true,bg);
                commands.DrawMesh(quad,Matrix4x4.identity,b1Mat,0,0);
                commands.SetRenderTarget(b0); commands.ClearRenderTarget(true,true,bg);
                commands.DrawMesh(quad,Matrix4x4.identity,b0Mat,0,0);
                commands.SetRenderTarget(backgroundb); commands.ClearRenderTarget(true,true,bgB);
                commands.SetRenderTarget(b1b); commands.ClearRenderTarget(true,true,bgB);
                commands.DrawMesh(quad,Matrix4x4.identity,b1Mat,0,0);
                commands.SetRenderTarget(b0b); commands.ClearRenderTarget(true,true,bgB);
                commands.DrawMesh(quad,Matrix4x4.identity,b0Mat,0,0);
                commands.SetRenderTarget(t1); commands.ClearRenderTarget(true,true,Color.clear);
                commands.DrawMesh(quad,Matrix4x4.identity,timeMat,0,0);
                output.draw_order = "same CameraEvent.AfterEverything CommandBuffer: vertex _Time.y before; background A, Night B1a, Night B0a; background B, Night B1b, Night B0b; vertex _Time.y after";
                output.time_source_scope = "GPU vertex-stage Unity built-in _Time.y before and after four Night source shader draws in one command buffer; no SetGlobalVector(_Time) or CPU-derived time accepted";
                camera.AddCommandBuffer(CameraEvent.AfterEverything,commands);
                output.cpu_realtime_before = Time.realtimeSinceStartup;
                camera.Render();
                output.cpu_realtime_after = Time.realtimeSinceStartup;
                output.time_before_bin = "shader_time_before.rgba32f.bin";
                output.time_after_bin = "shader_time_after.rgba32f.bin";
                output.shader_time_before = SaveTime(t0,outputDir,output.time_before_bin,out output.time_before_sha256);
                output.shader_time_after = SaveTime(t1,outputDir,output.time_after_bin,out output.time_after_sha256);
                output.b1_png = "night_mask_b1.png"; output.b1_sha256 = SavePng(b1,outputDir,output.b1_png);
                output.b0_png = "night_mask_b0.png"; output.b0_sha256 = SavePng(b0,outputDir,output.b0_png);
                output.background_png = "night_background.png";
                output.background_sha256 = SavePng(background,outputDir,output.background_png);
                output.b1_b_png = "night_mask_b1_background_b.png";
                output.b1_b_sha256 = SavePng(b1b,outputDir,output.b1_b_png);
                output.b0_b_png = "night_mask_b0_background_b.png";
                output.b0_b_sha256 = SavePng(b0b,outputDir,output.b0_b_png);
                output.background_b_png = "night_background_b.png";
                output.background_b_sha256 = SavePng(backgroundb,outputDir,output.background_b_png);
                Need(output.shader_time_before == output.shader_time_after,
                    "GPU _Time.y changed within the single command buffer");
                output.complete = true; exitCode = 0;
            } finally {
                if (camera && commands != null) camera.RemoveCommandBuffer(CameraEvent.AfterEverything,commands);
                if (commands != null) commands.Release();
                if (cameraObject) UnityEngine.Object.DestroyImmediate(cameraObject);
                foreach (var rt in new [] { dummy,b1,b0,background,b1b,b0b,backgroundb,t0,t1 })
                    if (rt) { rt.Release(); UnityEngine.Object.DestroyImmediate(rt); }
                if (quad) UnityEngine.Object.DestroyImmediate(quad);
                foreach (var texture in new [] { main,blue,noBlue }) if (texture) UnityEngine.Object.DestroyImmediate(texture);
                foreach (var mat in new [] { b1Mat,b0Mat,timeMat }) if (mat) UnityEngine.Object.DestroyImmediate(mat);
            }
        } catch (Exception error) {
            output.failure = error.ToString();
            File.WriteAllText(Path.Combine(outputDir,"failure.txt"),error.ToString(),new UTF8Encoding(false));
            Debug.LogException(error);
        }
        File.WriteAllText(Path.Combine(outputDir,"native_night_time.json"),JsonUtility.ToJson(output,true),new UTF8Encoding(false));
        Debug.Log("ROK_NIGHT_TIME_NATIVE_OUTPUT " + outputDir + " complete=" + output.complete);
        EditorApplication.Exit(exitCode);
    }
}
'''


def need(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path: Path) -> dict:
    need(path.is_file(), f"missing {path}")
    return json.loads(path.read_text(encoding="utf-8-sig"))


def contract() -> dict:
    need(sha(V2_CONTRACT) == V2_CONTRACT_SHA and sha(V2_NATIVE) == V2_NATIVE_SHA and
         sha(V2_VERIFY) == V2_VERIFY_SHA and read_json(V2_VERIFY)["passed"],
         "sealed V2 family-state source/GPU evidence differs")
    source = read_json(V2_CONTRACT)
    need(source["source_color_space"] == "Gamma" and source["unity_version"] == "2019.4.28f1c1" and
         source["rows"][0]["source_key"] == "initial:5", "V2 night source identity differs")
    files = source["source_files"]
    need(len(files) == 26, "V2 source dependency closure differs")
    settings = [{"name": path.name, "sha256": sha(path)}
                for path in sorted((V2 / "ProjectSettings").iterdir()) if path.is_file()]
    need(len(settings) >= 16 and any(x["name"] == "ProjectSettings.asset" for x in settings) and
         any(x["name"] == "GraphicsSettings.asset" for x in settings) and
         any(x["name"] == "QualitySettings.asset" for x in settings),
         "sealed V2 project render settings missing")
    return {"schema": SCHEMA, "repo_root": ROOT.as_posix(), "probe_root": PROBE.as_posix(),
            "unity_version": "2019.4.28f1c1", "source_color_space": "Gamma",
            "v2_contract_sha256": V2_CONTRACT_SHA, "v2_native_sha256": V2_NATIVE_SHA,
            "v2_verify_sha256": V2_VERIFY_SHA,
            "exporter_sha256": hashlib.sha256(EXPORTER.encode("utf-8")).hexdigest(),
            "time_shader_sha256": hashlib.sha256(TIME_SHADER.encode("utf-8")).hexdigest(),
            "stub_sha256": hashlib.sha256(STUB).hexdigest(), "source_files": files,
            "project_settings": settings,
            "source_key": "initial:5", "scope": "same_command_buffer_gpu_vertex_Time_y_bracket_and_source_night_B1_B0; no_GameApp_claim"}


def prepare() -> None:
    c = contract()
    existing_runs = (PROBE / "OutputRuns").is_dir() and any((PROBE / "OutputRuns").iterdir())
    dst_contract = PROBE / "night_time_contract.json"
    if dst_contract.exists():
        need(read_json(dst_contract) == c or not existing_runs, "isolated night contract changed after a native run")
    dst_contract.parent.mkdir(parents=True, exist_ok=True)
    if not dst_contract.exists() or read_json(dst_contract) != c:
        dst_contract.write_text(json.dumps(c, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for item in c["source_files"]:
        original = resolve(item["source_path"])
        copy = PROBE / item["probe_path"]
        copy.parent.mkdir(parents=True, exist_ok=True)
        for src, dst, expected in ((original, copy, item["source_sha256"]),
                                   (Path(str(original) + ".meta"), Path(str(copy) + ".meta"), item["meta_sha256"])):
            need(src.is_file() and sha(src) == expected, f"source changed: {src}")
            if dst.exists():
                need(sha(dst) == expected, f"existing isolated source changed: {dst}")
            else:
                shutil.copyfile(src, dst)
                need(sha(dst) == expected, f"isolated copy failed: {dst}")
    for item in c["project_settings"]:
        src = V2 / "ProjectSettings" / item["name"]
        dst = PROBE / "ProjectSettings" / item["name"]
        need(sha(src) == item["sha256"], f"sealed V2 setting changed: {src}")
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.exists():
            need(sha(dst) == item["sha256"] or not existing_runs,
                 f"isolated project setting changed after a run: {dst}")
        if not dst.exists() or sha(dst) != item["sha256"]:
            shutil.copyfile(src, dst)
    generated = ((PROBE / "Assets/Editor/ExportRokUnityCityNightTimeProbe.cs", EXPORTER.encode("utf-8")),
                 (PROBE / "Assets/ProbeNightTime.shader", TIME_SHADER.encode("utf-8")),
                 (PROBE / "Assets/ProbeDependencyStubs.cs", STUB),
                 (PROBE / "Packages/manifest.json", b'{"dependencies":{"com.unity.modules.imageconversion":"1.0.0","com.unity.modules.jsonserialize":"1.0.0","com.unity.modules.ui":"1.0.0"}}\n'))
    for dst, content in generated:
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.exists():
            need(dst.read_bytes() == content or not existing_runs, f"generated file changed after native run: {dst}")
        if not dst.exists() or dst.read_bytes() != content:
            dst.write_bytes(content)
    print(f"PREPARED {PROBE}: 26 pinned source assets/scripts plus same-command-buffer Night GPU time probe; Unity not started")


def verify(run: Path) -> dict:
    run = run.resolve()
    need(run.is_dir() and run.parent == (PROBE / "OutputRuns").resolve(), "run outside isolated Night project")
    c = contract()
    cpath = PROBE / "night_time_contract.json"
    need(read_json(cpath) == c and sha(PROBE / "Assets/Editor/ExportRokUnityCityNightTimeProbe.cs") == c["exporter_sha256"] and
         sha(PROBE / "Assets/ProbeNightTime.shader") == c["time_shader_sha256"], "Night source/exporter contract differs")
    need(all(sha(PROBE / "ProjectSettings" / item["name"]) == item["sha256"] for item in c["project_settings"]),
         "isolated project settings differ from sealed V2")
    native_path = run / "native_night_time.json"
    data = read_json(native_path)
    need(data.get("schema") == NATIVE_SCHEMA and data.get("complete") is True and not data.get("failure") and
         data.get("source_contract_sha256") == sha(cpath) and
         data.get("exporter_sha256") == c["exporter_sha256"] and
         data.get("time_shader_sha256") == c["time_shader_sha256"] and
         data.get("unity_version") == "2019.4.28f1c1" and data.get("color_space") == "Gamma" and
         data.get("graphics_device") == "Direct3D11" and data.get("game_app_loaded") is False,
         "Night run incomplete or identity differs")
    old = read_json(V2_NATIVE)["rows"][0]
    need(data.get("source_prefab_guid") == next(x["source_guid"] for x in c["source_files"] if x["probe_path"].endswith("build_1_101_1.prefab")) and
         data.get("source_material_guid") == old["source_material_guid"] and
         data.get("source_renderer_file_id") == old["renderer_file_id_decimal"] and
         data.get("source_shader_name") == old["shader_name"] and data.get("source_queue") == 2996 and
         data.get("source_sprite_vertices") == 240 and data.get("source_sprite_triangles") == 238 and
         data.get("source_mask_property_block_matches") is True and
         data.get("source_mask_sprite_path") == old["source_mask_sprite_asset_path"] and
         data.get("source_mask_sprite_file_id") == old["source_mask_sprite_file_id_decimal"],
         "source night renderer/material/mask binding differs")
    need(data.get("draw_order") == "same CameraEvent.AfterEverything CommandBuffer: vertex _Time.y before; background A, Night B1a, Night B0a; background B, Night B1b, Night B0b; vertex _Time.y after" and
         "GPU vertex-stage" in data.get("time_source_scope", "") and
         data.get("background_a_control") == "RGBA float (0.07,0.15,0.71,1)" and
         data.get("background_b_control") == "RGBA float (0.74,0.09,0.05,1)" and
         data.get("target_format") == "ARGB32 RenderTextureReadWrite.Default Gamma" and
         data.get("time_target_format") == "ARGBFloat RenderTextureReadWrite.Linear" and
         data.get("time_readback_format") == "Texture2D RGBAFloat 1x1 little-endian raw float32",
         "same-command-buffer float readback provenance differs")
    measured = []
    for label in ("before", "after"):
        path = run / data[f"time_{label}_bin"]
        need(path.is_file() and path.name == data[f"time_{label}_bin"] and path.stat().st_size == 16 and
             sha(path) == data[f"time_{label}_sha256"], f"GPU time {label} raw bytes differ")
        r, g, b, a = struct.unpack("<4f", path.read_bytes())
        need(math.isfinite(r) and r >= 0 and abs(g) <= 1e-6 and abs(b) <= 1e-6 and abs(a - 1) <= 1e-6 and
             abs(r - data[f"shader_time_{label}"]) <= 1e-4, f"GPU time {label} float payload differs")
        measured.append(r)
    need((run / data["time_before_bin"]).read_bytes() == (run / data["time_after_bin"]).read_bytes(),
         "GPU built-in _Time changed between two source Night draws")
    pixels = {}
    for label in ("b1", "b0", "background", "b1_b", "b0_b", "background_b"):
        path = run / data[f"{label}_png"]
        need(path.is_file() and path.name == data[f"{label}_png"] and sha(path) == data[f"{label}_sha256"],
             f"Night {label} PNG hash differs")
        with Image.open(path) as img:
            need(img.mode == "RGBA" and img.size == (128, 128), f"Night {label} image format differs")
            pixels[label] = img.tobytes()
    def v2_frame(name: str) -> bytes:
        frame = next(f for f in old["frames"] if f["name"] == name)
        path = V2_NATIVE.parent / frame["png"]
        need(sha(path) == frame["sha256"], f"sealed V2 {name} PNG changed")
        with Image.open(path) as img:
            need(img.mode == "RGBA" and img.size == (128, 128), f"sealed V2 {name} layout differs")
            return img.tobytes()

    reference = v2_frame("night_mask_b_zero")
    b0_a_max_rgb = max(abs(pixels["b0"][i] - reference[i]) for i in range(len(reference)) if i % 4 != 3)
    need(b0_a_max_rgb <= 1,
         f"background A same-shader B0 differs from sealed V2 above locked 1 RGB LSB: {b0_a_max_rgb}")
    background_max_rgb = {}
    response = {}
    for suffix, output_background, source_background, b1_key, b0_key in (
            ("a", "background", "background_a", "b1", "b0"),
            ("b", "background_b", "background_b", "b1_b", "b0_b")):
        old_background = v2_frame(source_background)
        background_max_rgb[suffix] = max(abs(pixels[output_background][i] - old_background[i])
                                         for i in range(len(reference)) if i % 4 != 3)
        need(background_max_rgb[suffix] <= 1,
             f"pure background {suffix} differs from sealed V2 above 1 RGB LSB")
        max_rgb = max(abs(pixels[b1_key][i] - pixels[b0_key][i])
                      for i in range(len(reference)) if i % 4 != 3)
        changed = sum(any(pixels[b1_key][4 * p + c] != pixels[b0_key][4 * p + c] for c in range(3))
                      for p in range(128 * 128))
        max_alpha = max(abs(pixels[b1_key][i] - pixels[b0_key][i])
                        for i in range(3, len(reference), 4))
        need(changed >= 100 and max_rgb > 0 and max_alpha == 0,
             f"Night mask B GPU color response missing or altered alpha on background {suffix}")
        response[suffix] = {"max_rgb8": max_rgb, "changed_rgb_pixels": changed,
                            "max_alpha8": max_alpha}
    multiplier = min(1.0, abs(math.sin(measured[0] * 0.5)) + 0.5)
    need(0.5 <= multiplier <= 1.0, "GPU-measured night multiplier outside source shader bound")
    report = {"schema": "rok.unity_city_night_time_verify.v1", "passed": True, "run": str(run),
              "contract_sha256": sha(cpath), "native_sha256": sha(native_path),
              "gpu_shader_time_y_f32": measured[0], "before_after_float4_raw_equal": True,
              "source_night_light_multiplier_from_measured_gpu_time": multiplier,
              "sealed_v2_background_a_b0_max_rgb8": b0_a_max_rgb,
              "sealed_v2_pure_background_max_rgb8": background_max_rgb,
              "b1_b0_response_by_background": response,
              "background_b_b0_direct_sealed_reference_available": False,
              "scope": "one camera render, same CommandBuffer, GPU vertex _Time.y bracketing source Night B1/B0 on two controlled backgrounds; background B B0 has no sealed direct reference; not GameApp time or full city acceptance"}
    destination = run / "native_verify.json"
    need(not destination.exists(), f"verification already exists: {destination}")
    with destination.open("x", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"PASS same-buffer GPU _Time.y={measured[0]:.9g}, A B0 RGB max={b0_a_max_rgb}, "
          f"B response pixels A/B={response['a']['changed_rgb_pixels']}/{response['b']['changed_rgb_pixels']}: {run}")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--prepare", action="store_true")
    group.add_argument("--verify", type=Path)
    args = parser.parse_args()
    prepare() if args.prepare else verify(args.verify)
