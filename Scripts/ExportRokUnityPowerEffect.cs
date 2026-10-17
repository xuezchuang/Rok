using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEngine;

// Run only in Saved/UnityBuiltinShaderProbe with the matching Unity 2019 editor.
// Copies source assets with their import metadata; never saves the live project.
public static class ExportRokUnityPowerEffect
{
    const string Root = "D:/ueDemo/Rok";
    const string Source = "ref/client-unity/Assets/";
    const string Output = Root + "/Saved/RokUnityMigration/PowerGain";
    [Serializable] public class SourceFile { public string path, sha256, metadataSha256; }
    [Serializable] public class Draw
    {
        public Vector3[] positions;
        public Vector2[] uv;
        public Color[] colors;
        public int[] indices;
    }
    [Serializable] public class Sample
    {
        public float time, uTiling, particleRemainingLifetime;
        public int particleCount;
        public Draw particle;
    }
    [Serializable] public class Export
    {
        public string unityVersion, colorSpace, particleScalingMode;
        public string projection = "Unity UI XY; root origin is power button pivot; no bounds auto-fit";
        public float lifetime = 1, sampleRate = 120;
        public uint seed = 10010;
        public bool runtimeAccepted = false;
        public SourceFile[] sources;
        public Draw mesh;
        public Sample[] samples;
        public int roundTripMaxChannelError;
        public float roundTripMeanChannelError;
    }
    static SourceFile Copy(string relative)
    {
        string source = Root + "/" + Source + relative;
        string target = "Assets/PowerGain/" + relative;
        Directory.CreateDirectory(Path.GetDirectoryName(target));
        File.Copy(source, target, true);
        File.Copy(source + ".meta", target + ".meta", true);
        using (var hash = SHA256.Create())
            return new SourceFile { path = Source + relative,
                sha256 = BitConverter.ToString(hash.ComputeHash(File.ReadAllBytes(source))).Replace("-", "").ToLowerInvariant(),
                metadataSha256 = BitConverter.ToString(hash.ComputeHash(File.ReadAllBytes(source + ".meta"))).Replace("-", "").ToLowerInvariant() };
    }
    static Draw ReadMesh(Mesh mesh, Matrix4x4 transform)
    {
        var positions = mesh.vertices.Select(p => transform.MultiplyPoint3x4(p)).ToArray();
        var colors = mesh.colors;
        if (colors.Length == 0) colors = Enumerable.Repeat(Color.white, positions.Length).ToArray();
        if (mesh.uv.Length != positions.Length || colors.Length != positions.Length)
            throw new InvalidOperationException("Incomplete source vertex streams");
        return new Draw { positions = positions, uv = mesh.uv, colors = colors, indices = mesh.triangles };
    }
    public static void Run()
    {
        if (Path.GetFullPath(Application.dataPath).Replace('\\', '/') != Root + "/Saved/UnityBuiltinShaderProbe/Assets")
            throw new InvalidOperationException("Wrong Unity export project");
        if (Application.unityVersion != "2019.4.28f1c1")
            throw new InvalidOperationException("Use the source Unity version");
        Directory.CreateDirectory(Output);
        var paths = new[] {
            "BundleAssets/UI/UIEffect/UI_10010.prefab",
            "BundleAssets/UI/UIAnimation/UI_10010.anim",
            "BundleAssets/UI/UIAnimation/UI_10010.controller",
            "BundleAssets/Effect/Meshes/m_liupian001.FBX",
            "BundleAssets/Effect/Materials/shared_effectmaterials/tile003.mat",
            "BundleAssets/Effect/Materials/shared_effectmaterials/glow002.mat",
            "BundleAssets/Effect/Textures/shared_effecttextures/tile003.png",
            "BundleAssets/Effect/Textures/shared_effecttextures/glow002.png"
        };
        var result = new Export { unityVersion = Application.unityVersion,
            colorSpace = QualitySettings.activeColorSpace.ToString(), sources = paths.Select(Copy).ToArray() };
        AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/PowerGain/" + paths[0]);
        var clip = AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/PowerGain/" + paths[1]);
        if (!prefab || !clip) throw new InvalidOperationException("Missing source prefab/clip");
        var instance = UnityEngine.Object.Instantiate(prefab);
        // AutoDestory is a game script, absent in this isolated project. Its one-second
        // lifetime is enforced by the export range and subsequently by the UE player.
        foreach (var transform in instance.GetComponentsInChildren<Transform>())
            GameObjectUtility.RemoveMonoBehavioursWithMissingScript(transform.gameObject);
        instance.GetComponent<Animator>().enabled = false;
        var uBinding = AnimationUtility.GetCurveBindings(clip).Single(b => b.propertyName == "material._MainTex_ST.x");
        var uCurve = AnimationUtility.GetEditorCurve(clip, uBinding);
        var filter = instance.GetComponentInChildren<MeshFilter>();
        // SampleAnimation does not update material properties in this batch editor.
        // Evaluate the imported AnimationCurve itself, including authored tangents.
        var meshMaterial = filter.GetComponent<Renderer>().material;
        var particles = instance.GetComponentInChildren<ParticleSystem>();
        var renderer = particles.GetComponent<ParticleSystemRenderer>();
        foreach (var r in instance.GetComponentsInChildren<Renderer>())
            if (r.sharedMaterial == null || r.sharedMaterial.shader.name != "Mobile/Particles/Additive")
                throw new InvalidOperationException("Source additive shader did not resolve");
        result.mesh = ReadMesh(filter.sharedMesh, filter.transform.localToWorldMatrix);
        result.particleScalingMode = particles.main.scalingMode.ToString();
        particles.useAutoRandomSeed = false;
        particles.randomSeed = result.seed;
        var cameraObject = new GameObject("Source UI XY camera");
        var camera = cameraObject.AddComponent<Camera>();
        camera.transform.position = new Vector3(0, 0, -1000);
        camera.orthographic = true;
        camera.orthographicSize = 180;
        camera.nearClipPlane = 1;
        camera.farClipPlane = 2000;
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = new Color(0.035f, 0.035f, 0.035f, 1);
        camera.allowHDR = false;
        camera.allowMSAA = false;
        var target = new RenderTexture(640, 360, 24, RenderTextureFormat.ARGB32);
        camera.targetTexture = target;
        var baked = new Mesh();
        result.samples = new Sample[121];
        particles.Simulate(0, false, true, false);
        for (int i = 0; i < result.samples.Length; i++)
        {
            float t = i / result.sampleRate;
            float uTiling = uCurve.Evaluate(t);
            meshMaterial.mainTextureScale = new Vector2(uTiling, 1);
            if (i > 0) particles.Simulate(1 / result.sampleRate, false, false, false);
            renderer.BakeMesh(baked, camera, true);
            var liveParticles = new ParticleSystem.Particle[particles.particleCount];
            particles.GetParticles(liveParticles);
            result.samples[i] = new Sample { time = t,
                uTiling = uTiling,
                particleCount = particles.particleCount,
                particleRemainingLifetime = liveParticles.Length > 0 ? liveParticles[0].remainingLifetime : 0,
                // BakeMesh(useTransform:true) already includes rotation and scale.
                particle = ReadMesh(baked, Matrix4x4.Translate(particles.transform.position)) };
            if (new[] { 0, 9, 20, 51, 102, 119 }.Contains(i))
            {
                camera.Render();
                RenderTexture.active = target;
                var image = new Texture2D(640, 360, TextureFormat.RGB24, false);
                image.ReadPixels(new Rect(0, 0, 640, 360), 0, 0);
                image.Apply();
                File.WriteAllBytes(Output + "/unity_" + i.ToString("D3") + ".png", image.EncodeToPNG());
                if (i == 9)
                {
                    // Round-trip the exported particle geometry through an ordinary mesh
                    // renderer. This catches transform/vertex-color errors before UE uses it.
                    var roundTrip = new GameObject("Exported particle round trip");
                    roundTrip.transform.position = particles.transform.position;
                    roundTrip.AddComponent<MeshFilter>().sharedMesh = baked;
                    roundTrip.AddComponent<MeshRenderer>().sharedMaterial = renderer.sharedMaterial;
                    renderer.enabled = false;
                    camera.Render();
                    var replay = new Texture2D(640, 360, TextureFormat.RGB24, false);
                    replay.ReadPixels(new Rect(0, 0, 640, 360), 0, 0);
                    replay.Apply();
                    File.WriteAllBytes(Output + "/unity_geometry_roundtrip_009.png", replay.EncodeToPNG());
                    var originalPixels = image.GetPixels32();
                    var replayPixels = replay.GetPixels32();
                    long error = 0;
                    for (int p = 0; p < originalPixels.Length; p++)
                    {
                        var a = originalPixels[p]; var b = replayPixels[p];
                        int r = Math.Abs(a.r - b.r), g = Math.Abs(a.g - b.g), blue = Math.Abs(a.b - b.b);
                        result.roundTripMaxChannelError = Math.Max(result.roundTripMaxChannelError, Math.Max(r, Math.Max(g, blue)));
                        error += r + g + blue;
                    }
                    result.roundTripMeanChannelError = (float)error / (originalPixels.Length * 3);
                    UnityEngine.Object.DestroyImmediate(replay);
                    UnityEngine.Object.DestroyImmediate(roundTrip);
                    renderer.enabled = true;
                }
                UnityEngine.Object.DestroyImmediate(image);
                RenderTexture.active = null;
            }
        }
        File.WriteAllText(Output + "/source_geometry.json", JsonUtility.ToJson(result, true));
        if (result.roundTripMeanChannelError > 0.1f)
            throw new InvalidOperationException("Exported particle geometry does not match source rendering: " + result.roundTripMeanChannelError);
        camera.targetTexture = null;
        target.Release();
        UnityEngine.Object.DestroyImmediate(target);
        UnityEngine.Object.DestroyImmediate(baked);
        UnityEngine.Object.DestroyImmediate(meshMaterial);
        UnityEngine.Object.DestroyImmediate(cameraObject);
        UnityEngine.Object.DestroyImmediate(instance);
        Debug.Log("ROK_POWER_EFFECT_EXPORTED vertices=" + result.mesh.positions.Length + " samples=" + result.samples.Length);
    }
}
