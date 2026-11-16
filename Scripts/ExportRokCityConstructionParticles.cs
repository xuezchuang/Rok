using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

// Run inside the isolated, source-backed Unity construction effects probe.
public static class ExportRokCityConstructionParticles
{
    [Serializable] class MaterialRow
    {
        public string name, source, texture;
        public bool additive;
        public Color color;
        public Vector2 scale, offset;
    }
    [Serializable] class Geometry { public float[] vertices; public int[] indices; }
    [Serializable] class Frame { public Geometry[] emitters; }
    [Serializable] class Effect { public string name; public string[] materials; public Frame[] frames; }
    [Serializable] class Report
    {
        public int rate = 60;
        public float duration = 2;
        public MaterialRow[] materials;
        public Effect[] effects;
    }

    const string AssetRoot = "Assets/Source/BundleAssets/";
    const string Output = "D:/ueDemo/Rok/Saved/RokUnityMigration/CityConstructionParticles";
    static readonly Dictionary<string, MaterialRow> Materials = new Dictionary<string, MaterialRow>();

    static MaterialRow ReadMaterial(Material material)
    {
        string name = material.name;
        if (Materials.ContainsKey(name)) return Materials[name];
        int src = material.HasProperty("_SrcBlend") ? material.GetInt("_SrcBlend") : 5;
        int dst = material.HasProperty("_DstBlend") ? material.GetInt("_DstBlend") : 1;
        if (src != 5 || (dst != 1 && dst != 10) || !material.mainTexture)
            throw new InvalidOperationException("Unsupported source particle material " + name);
        var row = new MaterialRow { name = name, source = AssetDatabase.GetAssetPath(material),
            texture = AssetDatabase.GetAssetPath(material.mainTexture), additive = dst == 1,
            color = material.HasProperty("_Color") ? material.color : Color.white,
            scale = material.mainTextureScale, offset = material.mainTextureOffset };
        Materials.Add(name, row);
        return row;
    }

    static Geometry Sample(ParticleSystemRenderer renderer, Camera camera, Mesh mesh)
    {
        mesh.Clear();
        renderer.BakeMesh(mesh, camera, true);
        if (mesh.vertexCount == 0) return new Geometry { vertices = new float[0], indices = new int[0] };
        var points = mesh.vertices;
        var colors = mesh.colors32;
        var uv = mesh.uv;
        if (mesh.subMeshCount != 1 || points.Length != uv.Length || points.Length != colors.Length)
            throw new InvalidOperationException("Unexpected particle stream on " + renderer.name);
        var packed = new float[points.Length * 9];
        for (int i = 0; i < points.Length; ++i)
        {
            // BakeMesh with useTransform already includes the emitter transform.
            int j = i * 9;
            packed[j] = -points[i].x * 500;
            packed[j + 1] = points[i].z * 500;
            packed[j + 2] = points[i].y * 500;
            packed[j + 3] = uv[i].x;
            packed[j + 4] = 1 - uv[i].y;
            packed[j + 5] = colors[i].r;
            packed[j + 6] = colors[i].g;
            packed[j + 7] = colors[i].b;
            packed[j + 8] = colors[i].a;
        }
        return new Geometry { vertices = packed, indices = mesh.GetTriangles(0) };
    }

    static void Capture(Camera camera, RenderTexture target, string file)
    {
        camera.Render();
        var prior = RenderTexture.active;
        RenderTexture.active = target;
        var image = new Texture2D(target.width, target.height, TextureFormat.RGB24, false);
        image.ReadPixels(new Rect(0, 0, image.width, image.height), 0, 0);
        image.Apply();
        File.WriteAllBytes(file, image.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(image);
        RenderTexture.active = prior;
    }

    static Effect Export(string name, Quaternion pose, Camera camera, RenderTexture target, Report report)
    {
        var source = AssetDatabase.LoadAssetAtPath<GameObject>(AssetRoot + "Effect/Prefabs/" + name + ".prefab");
        if (!source) throw new InvalidOperationException("Missing Unity prefab " + name);
        var instance = (GameObject)PrefabUtility.InstantiatePrefab(source);
        instance.transform.SetPositionAndRotation(Vector3.zero, pose);
        var systems = instance.GetComponentsInChildren<ParticleSystem>();
        var emitters = systems.Select(s => s.GetComponent<ParticleSystemRenderer>())
            .Where(r => r && r.enabled && r.sharedMaterial && r.sharedMaterial.mainTexture).ToArray();
        string variant = pose == Quaternion.identity ? name : name + "_shelf";
        var effect = new Effect { name = variant, materials = emitters.Select(r => ReadMaterial(r.sharedMaterial).name).ToArray(),
            frames = new Frame[(int)(report.duration * report.rate)] };
        var mesh = new Mesh();
        try
        {
            for (int frame = 0; frame < effect.frames.Length; ++frame)
            {
                foreach (var system in systems) system.Stop(false, ParticleSystemStopBehavior.StopEmittingAndClear);
                for (int i = 0; i < systems.Length; ++i)
                {
                    systems[i].useAutoRandomSeed = false;
                    systems[i].randomSeed = (uint)(91231 + i * 2897);
                }
                foreach (var system in systems)
                    if (system.transform.parent == instance.transform || system.transform == instance.transform)
                        system.Simulate((float)frame / report.rate, false, true, true);
                effect.frames[frame] = new Frame { emitters = emitters.Select(r => Sample(r, camera, mesh)).ToArray() };
                if (frame == 15 || frame == 30 || frame == 60)
                    Capture(camera, target, Path.Combine(Output, variant + "_" + frame + ".png"));
            }
            if (!effect.frames.Any(frame => frame.emitters.Any(e => e.indices.Length > 0)))
                throw new InvalidOperationException("Unity emitted no visible particles for " + variant);
            return effect;
        }
        finally
        {
            UnityEngine.Object.DestroyImmediate(mesh);
            UnityEngine.Object.DestroyImmediate(instance);
        }
    }

    public static void Run()
    {
        try
        {
            Directory.CreateDirectory(Output);
            var camera = new GameObject("City reference camera").AddComponent<Camera>();
            camera.transform.rotation = Quaternion.Euler(45, 0, 0);
            camera.transform.position = -camera.transform.forward * 20;
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(0.40f, 0.48f, 0.27f);
            camera.fieldOfView = 7.5f;
            camera.aspect = 1.6f;
            camera.allowHDR = false;
            var target = new RenderTexture(1280, 800, 24);
            camera.targetTexture = target;
            var shelf = Quaternion.Euler(0, -45, 0) * new Quaternion(0.35355338f, 0.35355338f, -0.1464466f, 0.8535535f);
            var report = new Report();
            report.effects = new[] { Export("build_3003", Quaternion.identity, camera, target, report),
                Export("build_3003", shelf, camera, target, report), Export("build_3004", Quaternion.identity, camera, target, report) };
            report.materials = Materials.Values.ToArray();
            File.WriteAllText(Path.Combine(Output, "unity_particles.json"), JsonUtility.ToJson(report));
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            EditorApplication.Exit(1);
        }
    }
}
