using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;

// Unity 2019.4 editor probe. Run only in Saved/UnityCityPlacementBottomProbe.
public static class ExportRokCityPlacementBottomNative
{
    private const string ProjectSuffix = "/Saved/UnityCityPlacementBottomProbe/Assets";
    private const string MeshPath = "Assets/Source/building_move_bottom.asset";
    private const string PrefabPath = "Assets/Source/building_move_bottom.prefab";
    private const string TexturePath = "Assets/Source/buiding_bottom2.png";
    private const string MaterialPath = "Assets/Source/city_building_move_bottom.mat";

    [Serializable]
    private sealed class PlatformRecord
    {
        public bool overridden;
        public int maxTextureSize, format, textureCompression, compressionQuality;
    }

    [Serializable]
    private sealed class Report
    {
        public string schema = "rok.city_placement_bottom_native.v1";
        public string qualification = "Actual Unity2019 Mesh.uv/triangles and GPU render from copied source mesh/material/texture; diagnostic top-down camera, not final city parity.";
        public string unityVersion, colorSpace, graphicsDevice, runDirectory;
        public string sourceContractSha256, preparedProbeContractSha256, meshSha256, meshMetaSha256;
        public string textureSha256, textureMetaSha256, materialSha256, prefabSha256, shaderName;
        public Vector3[] vertices, normals;
        public Vector2[] uv0;
        public int[] triangles;
        public Bounds bounds;
        public int subMeshCount, vertexCount, textureWidth, textureHeight, textureMips;
        public bool textureSrgb, textureMipmapEnabled, textureAlphaIsTransparency;
        public string textureFormat, filterMode, wrapU, wrapV;
        public PlatformRecord defaultPlatform, standalonePlatform;
        public Vector3 prefabRootLocalPosition, prefabRootLocalScale;
        public Quaternion prefabRootLocalRotation;
        public Color serializedMaterialColor;
        public Vector2 serializedMaterialTiling;
        public string[] renderPngs;
        public int width = 512, height = 512, footprintWidth = 5, footprintLength = 5;
        public string camera = "orthographic top down; +Y to origin; source prefab root pose and width/length=5; no source city scene implied";
    }

    private static string Sha(string path)
    {
        using (var stream = File.OpenRead(path))
        using (var hash = SHA256.Create())
            return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }

    private static string Absolute(string assetPath)
    {
        return Path.Combine(Directory.GetParent(Application.dataPath).FullName,
            assetPath.Replace('/', Path.DirectorySeparatorChar));
    }

    private static PlatformRecord Platform(TextureImporter importer, string name)
    {
        var value = importer.GetPlatformTextureSettings(name);
        return new PlatformRecord { overridden = value.overridden, maxTextureSize = value.maxTextureSize,
            format = (int)value.format, textureCompression = (int)value.textureCompression,
            compressionQuality = value.compressionQuality };
    }

    private static void RenderFrame(string output, Mesh mesh, Material source, Texture texture,
        Vector3 rootPosition, Quaternion rootRotation, Color tint)
    {
        GameObject go = null, cameraObject = null;
        Material material = null;
        RenderTexture target = null;
        Texture2D image = null;
        try
        {
            go = new GameObject("SourcePlacementBottomActualMesh");
            var filter = go.AddComponent<MeshFilter>();
            filter.sharedMesh = mesh;
            var renderer = go.AddComponent<MeshRenderer>();
            material = new Material(source);
            material.SetTexture("_MainTex", texture);
            material.SetTextureScale("_MainTex", new Vector2(5, 5));
            material.SetColor("_Color", tint);
            renderer.sharedMaterial = material;
            go.transform.position = rootPosition;
            go.transform.rotation = rootRotation;
            go.transform.localScale = new Vector3(5, .01f, 5);

            cameraObject = new GameObject("SourcePlacementBottomTopDownCamera");
            var camera = cameraObject.AddComponent<Camera>();
            camera.orthographic = true;
            camera.orthographicSize = .4f;
            camera.transform.position = new Vector3(0, 2, 0);
            camera.transform.rotation = Quaternion.Euler(90, 0, 0);
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(.13f, .17f, .19f, 1);
            camera.nearClipPlane = .01f;
            camera.farClipPlane = 10;
            camera.allowHDR = false;
            camera.allowMSAA = false;
            target = new RenderTexture(512, 512, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
            target.antiAliasing = 1;
            target.Create();
            camera.targetTexture = target;
            camera.Render();
            RenderTexture.active = target;
            image = new Texture2D(512, 512, TextureFormat.RGBA32, false, false);
            image.ReadPixels(new Rect(0, 0, 512, 512), 0, 0);
            image.Apply(false, false);
            File.WriteAllBytes(output, image.EncodeToPNG());
        }
        finally
        {
            RenderTexture.active = null;
            if (image) UnityEngine.Object.DestroyImmediate(image);
            if (target) { target.Release(); UnityEngine.Object.DestroyImmediate(target); }
            if (material) UnityEngine.Object.DestroyImmediate(material);
            if (cameraObject) UnityEngine.Object.DestroyImmediate(cameraObject);
            if (go) UnityEngine.Object.DestroyImmediate(go);
        }
    }

    public static void Run()
    {
        string project = Application.dataPath.Replace('\\', '/');
        if (!project.EndsWith(ProjectSuffix, StringComparison.Ordinal) || Application.unityVersion != "2019.4.28f1c1" ||
            QualitySettings.activeColorSpace != ColorSpace.Gamma)
            throw new InvalidOperationException("Wrong isolated Unity 2019 placement-bottom probe identity");
        string root = project.Substring(0, project.Length - ProjectSuffix.Length);
        string probe = Directory.GetParent(project).FullName;
        string sourceContract = Path.Combine(root, "Saved/RokUnityMigration/BatchReview20261009Gamma/placement_bottom_source_contract.json");
        string prepared = Path.Combine(probe, "source_contract.json");
        if (!File.Exists(sourceContract) || !File.Exists(prepared)) throw new InvalidOperationException("Probe contract missing");
        string output = Path.Combine(probe, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") + "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        if (Directory.Exists(output)) throw new InvalidOperationException("Native run already exists");
        Directory.CreateDirectory(output);
        try
        {
            var mesh = AssetDatabase.LoadAssetAtPath<Mesh>(MeshPath);
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(PrefabPath);
            var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(TexturePath);
            var source = AssetDatabase.LoadAssetAtPath<Material>(MaterialPath);
            var importer = AssetImporter.GetAtPath(TexturePath) as TextureImporter;
            if (!mesh || !prefab || !texture || !source || importer == null ||
                prefab.GetComponent<MeshFilter>() == null || prefab.GetComponent<MeshFilter>().sharedMesh != mesh ||
                prefab.GetComponent<MeshRenderer>() == null || prefab.GetComponent<MeshRenderer>().sharedMaterial != source ||
                source.GetTexture("_MainTex") != texture || mesh.subMeshCount != 1 || mesh.vertexCount != 4 ||
                mesh.uv.Length != 4 || mesh.triangles.Length != 6)
                throw new InvalidOperationException("Source bottom prefab/mesh/material/texture binding or topology differs");
            var report = new Report {
                unityVersion = Application.unityVersion, colorSpace = QualitySettings.activeColorSpace.ToString(),
                graphicsDevice = SystemInfo.graphicsDeviceName, runDirectory = output.Replace('\\', '/'),
                sourceContractSha256 = Sha(sourceContract), preparedProbeContractSha256 = Sha(prepared),
                meshSha256 = Sha(Absolute(MeshPath)), meshMetaSha256 = Sha(Absolute(MeshPath + ".meta")),
                textureSha256 = Sha(Absolute(TexturePath)), textureMetaSha256 = Sha(Absolute(TexturePath + ".meta")),
                materialSha256 = Sha(Absolute(MaterialPath)), prefabSha256 = Sha(Absolute(PrefabPath)),
                shaderName = source.shader.name, vertices = mesh.vertices, normals = mesh.normals,
                uv0 = mesh.uv, triangles = mesh.triangles, bounds = mesh.bounds,
                subMeshCount = mesh.subMeshCount, vertexCount = mesh.vertexCount,
                textureWidth = texture.width, textureHeight = texture.height, textureMips = texture.mipmapCount,
                textureSrgb = importer.sRGBTexture, textureMipmapEnabled = importer.mipmapEnabled,
                textureAlphaIsTransparency = importer.alphaIsTransparency,
                textureFormat = texture.format.ToString(), filterMode = texture.filterMode.ToString(),
                wrapU = texture.wrapModeU.ToString(), wrapV = texture.wrapModeV.ToString(),
                defaultPlatform = Platform(importer, "DefaultTexturePlatform"),
                standalonePlatform = Platform(importer, "Standalone"),
                prefabRootLocalPosition = prefab.transform.localPosition,
                prefabRootLocalRotation = prefab.transform.localRotation,
                prefabRootLocalScale = prefab.transform.localScale,
                serializedMaterialColor = source.GetColor("_Color"),
                serializedMaterialTiling = source.GetTextureScale("_MainTex"),
                renderPngs = new[] { "bottom_green_5x5.png", "bottom_red_5x5.png" }
            };
            // The source prefab pose is preserved. These two colors reproduce
            // CityGlobalMediator's runtime MaterialPropertyBlock override.
            RenderFrame(Path.Combine(output, report.renderPngs[0]), mesh, source, texture,
                report.prefabRootLocalPosition, report.prefabRootLocalRotation, Color.green);
            RenderFrame(Path.Combine(output, report.renderPngs[1]), mesh, source, texture,
                report.prefabRootLocalPosition, report.prefabRootLocalRotation, Color.red);
            File.WriteAllText(Path.Combine(output, "native_bottom.json"), JsonUtility.ToJson(report, true), new UTF8Encoding(false));
            Debug.Log("ROK_CITY_PLACEMENT_BOTTOM_NATIVE_OUTPUT " + output);
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            File.WriteAllText(Path.Combine(output, "failure.txt"), error.ToString());
            Debug.LogException(error);
            EditorApplication.Exit(1);
        }
    }
}
