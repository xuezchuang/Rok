// Isolated Unity 2019.4 source-render probe. Run only in Saved/UnityAcademyVisualProbe.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using Client;
using UnityEditor;
using UnityEngine;

public sealed class RenderRokAcademyVisualNative : MonoBehaviour
{
    private const string Root = "D:/ueDemo/Rok";
    private const string Probe = Root + "/Saved/UnityAcademyVisualProbe";
    private const string OutputRoot = Probe + "/OutputRuns";
    private static string Output;
    private const string ModeKey = "RokAcademyNativeProbeMode";

    [Serializable] public sealed class V3 { public float x, y, z; }
    [Serializable] public sealed class Q4 { public float x, y, z, w; }
    [Serializable] public sealed class Rgba { public float r, g, b, a; }
    [Serializable] public sealed class TransformData { public long file_id, parent_transform_file_id; public V3 local_position, local_scale; public Q4 local_rotation; }
    [Serializable] public sealed class RendererData { public bool enabled, flip_x, flip_y; public int sorting_order; public Rgba color; }
    [Serializable] public sealed class NodeData { public long file_id; public string name; public bool active; public TransformData transform; public RendererData renderer; }
    [Serializable] public sealed class MaterialData { public Rgba color_1, color_2; public float city_transparency; }
    [Serializable] public sealed class CameraData { public string source_row; public float dxf, distance, vertical_fov; public float[] forward; public int width, height; }
    [Serializable] public sealed class LightingData { public float[] neutral_sprite_color, day_sprite_color; public float city_transparency; }
    [Serializable] public sealed class Contract { public string schema, model, source_catalog_sha256; public NodeData root, sprite_child; public MaterialData material; public CameraData camera; public LightingData lighting; }
    [Serializable] public sealed class PlatformImport
    {
        public string name;
        public bool overridden;
        public int maxTextureSize, resizeAlgorithm, format, textureCompression, compressionQuality;
    }
    [Serializable] public sealed class ImportedTexture
    {
        public int width, height, mipmapCount, format, filterMode, wrapMode;
        public PlatformImport defaultPlatform, standalonePlatform;
    }
    [Serializable] public sealed class MaskExport
    {
        public string png, method, limitation;
        public int width, height, readbackFormat, renderTargetFormat, reloadFormat, reloadFilterMode, reloadWrapMode;
        public bool renderTargetSrgb;
    }
    [Serializable] public sealed class Geometry
    {
        public Rect rect, textureRect;
        public Vector2 pivot, textureRectOffset;
        public float pixelsPerUnit;
        public Vector2[] vertices, uvs, screenVertices;
        public int[] triangles;
        public Vector3[] worldVertices;
        public Vector3 spriteBoundsCenter, spriteBoundsExtents, rendererBoundsCenter, rendererBoundsExtents;
        public Vector3 rootLocalPosition, rootLocalScale, childLocalPosition, childLocalScale;
        public Quaternion rootLocalRotation, childLocalRotation;
        public Vector3 childWorldPosition;
        public Quaternion childWorldRotation;
    }
    [Serializable] public sealed class Frame
    {
        public string name, png;
        public Color spriteColor, clearColor;
        public float globalCityTransparencyAtRender;
        public string maskSource;
        public int width, height;
    }
    [Serializable] public sealed class Report
    {
        public string schema = "rok.academy_visual_native_render.v2";
        public string scope = "Source SpriteRenderer, original shader/material and MaskSprite property block; source prefab rendering subtree recreated from pinned YAML. Fixed city_default camera and explicit global lighting. Source .mat serializes CityTransparency=1, while undeclared Material.GetFloat is deliberately not called; actual render-time Shader global is measured. Mask export is GPU-blit RGBA8 decode with quantization, not native compressed source bytes. No city terrain, UI, LOD animation, or full GameView.";
        public string runDirectory;
        public string unityVersion, colorSpace, graphicsDevice, shaderName, spriteAsset, maskAsset, materialAsset, maskBinding;
        public int spriteTextureWidth, spriteTextureHeight, maskTextureWidth, maskTextureHeight;
        public float serializedSourceMaterialCityTransparency;
        public bool materialHasCityTransparencyProperty;
        public bool materialGetFloatAttempted, materialGetFloatAvailable;
        public string materialGetFloatObservation;
        public float materialGetFloatCityTransparencyObserved, globalCityTransparencyBeforeOverride;
        public Color materialColor1, materialColor2, rendererColor;
        public bool rendererFlipX, rendererFlipY, rendererEnabled;
        public int rendererSortingOrder;
        public float spriteImporterPixelsPerUnit, maskImporterPixelsPerUnit;
        public Vector2 spriteImporterPivot, maskImporterPivot;
        public int spriteImporterMeshType, maskImporterMeshType;
        public bool spriteImporterSrgb, maskImporterSrgb, spriteImporterAlphaIsTransparency, maskImporterAlphaIsTransparency;
        public ImportedTexture spriteImport, maskImport;
        public MaskExport maskExport;
        public Geometry geometry;
        public CameraData camera;
        public Frame[] frames;
    }

    private static System.Threading.Timer HardWatchdog;
    private static System.Diagnostics.Stopwatch Clock;
    private static volatile string Stage = "not-started";
    private const int SoftTimeoutSeconds = 45;
    private const int HardTimeoutSeconds = 120;

    private static void StageLog(string stage)
    {
        Stage = stage;
        UnityEngine.Debug.Log("ROK_ACADEMY_NATIVE_STAGE " + stage);
    }

    private static void OnEditorUpdate()
    {
        if (Clock == null || Clock.Elapsed.TotalSeconds < SoftTimeoutSeconds) return;
        string stage = Stage;
        bool paused = EditorApplication.isPaused;
        StopSoftWatchdog();
        UnityEngine.Debug.LogError("ROK_ACADEMY_NATIVE_WATCHDOG_SOFT_TIMEOUT stage=" + stage + " paused=" + paused);
        EditorApplication.Exit(1);
    }

    private static void ArmWatchdog()
    {
        DisarmWatchdog();
        Clock = System.Diagnostics.Stopwatch.StartNew();
        EditorApplication.update += OnEditorUpdate;
        HardWatchdog = new System.Threading.Timer(_ =>
        {
            try
            {
                Directory.CreateDirectory(Output);
                File.WriteAllText(Output + "/watchdog_timeout_" + System.Diagnostics.Process.GetCurrentProcess().Id + ".txt",
                    "ROK_ACADEMY_NATIVE_WATCHDOG_HARD_TIMEOUT stage=" + Stage + "\n");
            }
            finally { Environment.Exit(2); }
        }, null, HardTimeoutSeconds * 1000, Timeout.Infinite);
    }

    private static void DisarmWatchdog()
    {
        StopSoftWatchdog();
        if (HardWatchdog != null) { HardWatchdog.Dispose(); HardWatchdog = null; }
    }

    private static void StopSoftWatchdog()
    {
        EditorApplication.update -= OnEditorUpdate;
        Clock = null;
    }

    [InitializeOnLoadMethod]
    private static void Install() { EditorApplication.playModeStateChanged += OnPlayMode; }

    public static void Run()
    {
        if (Application.dataPath.Replace('\\', '/') != Probe + "/Assets" || Application.unityVersion != "2019.4.28f1c1")
            throw new Exception("Wrong Unity Academy native probe target/version");
        SessionState.SetBool(ModeKey, true);
        EditorApplication.isPlaying = true;
    }

    private static void OnPlayMode(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode && SessionState.GetBool(ModeKey, false))
        {
            SessionState.SetBool(ModeKey, false);
            Output = OutputRoot + "/" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") + "_" + System.Diagnostics.Process.GetCurrentProcess().Id;
            if (Directory.Exists(Output)) throw new Exception("Academy native run directory already exists");
            Directory.CreateDirectory(Output);
            UnityEngine.Debug.Log("ROK_ACADEMY_NATIVE_OUTPUT " + Output);
            StageLog("entered_play_mode");
            ArmWatchdog();
            new GameObject("AcademyNativeRender").AddComponent<RenderRokAcademyVisualNative>();
        }
    }

    private static Vector3 Vector(V3 v) { return new Vector3(v.x, v.y, v.z); }
    private static Quaternion Rotation(Q4 q) { return new Quaternion(q.x, q.y, q.z, q.w); }
    private static Color ColorFrom(Rgba c) { return new Color(c.r, c.g, c.b, c.a); }
    private static Color ColorFrom(float[] c)
    {
        if (c == null || c.Length != 4) throw new Exception("Invalid source global RGBA");
        return new Color(c[0], c[1], c[2], c[3]);
    }
    private static void ApplyTransform(Transform target, TransformData data)
    {
        target.localPosition = Vector(data.local_position);
        target.localRotation = Rotation(data.local_rotation);
        target.localScale = Vector(data.local_scale);
    }
    private static bool Near(float a, float b) { return Mathf.Abs(a - b) < 0.00001f; }
    private static void CheckColor(Color actual, Rgba expected, string name)
    {
        if (!Near(actual.r, expected.r) || !Near(actual.g, expected.g) || !Near(actual.b, expected.b) || !Near(actual.a, expected.a))
            throw new Exception(name + " differs from pinned material");
    }

    private static PlatformImport Platform(TextureImporter importer, string name)
    {
        var settings = importer.GetPlatformTextureSettings(name);
        return new PlatformImport { name = name, overridden = settings.overridden,
            maxTextureSize = settings.maxTextureSize, resizeAlgorithm = (int)settings.resizeAlgorithm,
            format = (int)settings.format, textureCompression = (int)settings.textureCompression,
            compressionQuality = settings.compressionQuality };
    }

    private static ImportedTexture ImportInfo(TextureImporter importer, Texture2D texture)
    {
        return new ImportedTexture { width = texture.width, height = texture.height,
            mipmapCount = texture.mipmapCount, format = (int)texture.format,
            filterMode = (int)texture.filterMode, wrapMode = (int)texture.wrapMode,
            defaultPlatform = Platform(importer, "DefaultTexturePlatform"),
            standalonePlatform = Platform(importer, "Standalone") };
    }

    private IEnumerator Start()
    {
        var capture = Capture();
        while (true)
        {
            bool more = false;
            bool failed = false;
            object current = null;
            try
            {
                more = capture.MoveNext();
                if (more) current = capture.Current;
            }
            catch (Exception error)
            {
                Debug.LogException(error);
                failed = true;
            }
            if (failed) { RenderTexture.active = null; StageLog("capture_failed_exit_requested"); StopSoftWatchdog(); EditorApplication.Exit(1); yield break; }
            if (!more) break;
            yield return current;
        }
        StageLog("capture_complete_exit_requested");
        StopSoftWatchdog();
        UnityEngine.Debug.Log("ROK_ACADEMY_NATIVE_RENDER_COMPLETE");
        EditorApplication.Exit(0);
    }

    private IEnumerator Capture()
    {
        if (Application.unityVersion != "2019.4.28f1c1" || QualitySettings.activeColorSpace != ColorSpace.Gamma)
            throw new Exception("Academy native probe needs source Unity 2019.4 Gamma color space");
        var contract = JsonUtility.FromJson<Contract>(File.ReadAllText(Probe + "/probe_contract.json"));
        if (contract == null || contract.schema != "rok.academy_visual_native_probe.v1" || contract.model != "build_13_101_2")
            throw new Exception("Wrong Academy native contract");
        if (!contract.root.active || !contract.sprite_child.active || !contract.sprite_child.renderer.enabled ||
            contract.sprite_child.transform.parent_transform_file_id != contract.root.transform.file_id)
            throw new Exception("Academy source rendering hierarchy changed");
        if (contract.camera.source_row != "city_default" || !Near(contract.camera.dxf, 150f) ||
            !Near(contract.camera.distance, 20f) || !Near(contract.camera.vertical_fov, 7.5f))
            throw new Exception("Academy camera is not source city_default row");

        const string spritePath = "Assets/Source/build_13_101_2.png";
        const string maskPath = "Assets/Source/build_13_101_2_mask.png";
        const string materialPath = "Assets/Source/map_building.mat";
        var sprite = AssetDatabase.LoadAssetAtPath<Sprite>(spritePath);
        var mask = AssetDatabase.LoadAssetAtPath<Sprite>(maskPath);
        var material = AssetDatabase.LoadAssetAtPath<Material>(materialPath);
        var spriteImporter = AssetImporter.GetAtPath(spritePath) as TextureImporter;
        var maskImporter = AssetImporter.GetAtPath(maskPath) as TextureImporter;
        if (sprite == null || mask == null || material == null || material.shader == null ||
            material.shader.name != "custom/map/map_building_with_mask-5" || spriteImporter == null || maskImporter == null)
            throw new Exception("Missing original Academy sprite/mask/material/shader");
        var spriteImporterSettings = new TextureImporterSettings();
        var maskImporterSettings = new TextureImporterSettings();
        spriteImporter.ReadTextureSettings(spriteImporterSettings);
        maskImporter.ReadTextureSettings(maskImporterSettings);
        CheckColor(material.GetColor("_Color_1"), contract.material.color_1, "_Color_1");
        CheckColor(material.GetColor("_Color_2"), contract.material.color_2, "_Color_2");
        // The source shader does not declare _CityTransparency in Properties.
        // The source .mat nevertheless serializes 1; the shader reads a global
        // uniform. Keep these observations separate instead of setting a new
        // material property or treating Material.GetFloat as the render value.
        bool materialHasTransparency = material.HasProperty("_CityTransparency");
        bool materialGetFloatAttempted = materialHasTransparency;
        float materialGetFloatTransparency = materialGetFloatAttempted ? material.GetFloat("_CityTransparency") : 0f;
        float globalTransparencyBeforeOverride = Shader.GetGlobalFloat("_CityTransparency");

        var root = new GameObject(contract.root.name);
        ApplyTransform(root.transform, contract.root.transform);
        var child = new GameObject(contract.sprite_child.name);
        child.transform.SetParent(root.transform, false);
        ApplyTransform(child.transform, contract.sprite_child.transform);
        var renderer = child.AddComponent<SpriteRenderer>();
        renderer.sprite = sprite;
        renderer.sharedMaterial = material;
        renderer.color = ColorFrom(contract.sprite_child.renderer.color);
        renderer.flipX = contract.sprite_child.renderer.flip_x;
        renderer.flipY = contract.sprite_child.renderer.flip_y;
        renderer.sortingOrder = contract.sprite_child.renderer.sorting_order;
        renderer.enabled = true;
        var sourceMaskScript = child.AddComponent<MaskSprite>();
        sourceMaskScript.m_mask_sprite = mask;
        StageLog("before_yield_1");
        yield return null;
        StageLog("after_yield_1_before_yield_2");
        yield return null;
        StageLog("after_yield_2");
        var block = new MaterialPropertyBlock();
        renderer.GetPropertyBlock(block);
        if (block.GetTexture("_Mask") != mask.texture)
            throw new Exception("Original MaskSprite did not bind mask texture through property block");

        var cameraObject = new GameObject("AcademySourceCityDefaultCamera");
        var camera = cameraObject.AddComponent<Camera>();
        camera.orthographic = false;
        camera.fieldOfView = contract.camera.vertical_fov;
        camera.aspect = (float)contract.camera.width / contract.camera.height;
        camera.nearClipPlane = 0.01f;
        camera.farClipPlane = 1000f;
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.allowHDR = false;
        camera.allowMSAA = false;
        var forward = new Vector3(contract.camera.forward[0], contract.camera.forward[1], contract.camera.forward[2]).normalized;
        camera.transform.rotation = Quaternion.LookRotation(forward, Vector3.up);
        camera.transform.position = -forward * contract.camera.distance;
        var target = new RenderTexture(contract.camera.width, contract.camera.height, 24,
            RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
        target.Create();
        camera.targetTexture = target;
        var vertices = sprite.vertices;
        var worldVertices = new Vector3[vertices.Length];
        var screenVertices = new Vector2[vertices.Length];
        for (int i = 0; i < vertices.Length; ++i)
        {
            worldVertices[i] = child.transform.TransformPoint(vertices[i]);
            var screen = camera.WorldToScreenPoint(worldVertices[i]);
            screenVertices[i] = new Vector2(screen.x, screen.y);
        }
        var geometry = new Geometry {
            rect = sprite.rect, textureRect = sprite.textureRect, pivot = sprite.pivot,
            textureRectOffset = sprite.textureRectOffset, pixelsPerUnit = sprite.pixelsPerUnit,
            vertices = vertices, uvs = sprite.uv, triangles = Array.ConvertAll(sprite.triangles, x => (int)x),
            worldVertices = worldVertices, screenVertices = screenVertices,
            spriteBoundsCenter = sprite.bounds.center, spriteBoundsExtents = sprite.bounds.extents,
            rendererBoundsCenter = renderer.bounds.center, rendererBoundsExtents = renderer.bounds.extents,
            rootLocalPosition = root.transform.localPosition, rootLocalRotation = root.transform.localRotation,
            rootLocalScale = root.transform.localScale, childLocalPosition = child.transform.localPosition,
            childLocalRotation = child.transform.localRotation, childLocalScale = child.transform.localScale,
            childWorldPosition = child.transform.position, childWorldRotation = child.transform.rotation,
        };
        Directory.CreateDirectory(Output);
        var frames = new List<Frame>();
        var variants = new KeyValuePair<string, float[]>[] {
            new KeyValuePair<string, float[]>("neutral", contract.lighting.neutral_sprite_color),
            new KeyValuePair<string, float[]>("day", contract.lighting.day_sprite_color),
        };
        foreach (var variant in variants)
        {
            StageLog("before_render_" + variant.Key);
            Color spriteColor = ColorFrom(variant.Value);
            Color clearColor = new Color(0.5255f, 0.6314f, 0.3216f, 1f) * spriteColor;
            Shader.SetGlobalColor("_SpriteColor", spriteColor);
            Shader.SetGlobalFloat("_CityTransparency", contract.lighting.city_transparency);
            float globalTransparencyAtRender = Shader.GetGlobalFloat("_CityTransparency");
            if (!Near(globalTransparencyAtRender, contract.lighting.city_transparency))
                throw new Exception("Source Academy global _CityTransparency control was not applied");
            camera.backgroundColor = clearColor;
            camera.Render();
            StageLog("after_render_" + variant.Key);
            RenderTexture.active = target;
            var image = new Texture2D(contract.camera.width, contract.camera.height, TextureFormat.RGB24, false);
            image.ReadPixels(new Rect(0, 0, contract.camera.width, contract.camera.height), 0, 0);
            image.Apply(false);
            string name = "academy_" + variant.Key + ".png";
            File.WriteAllBytes(Output + "/" + name, image.EncodeToPNG());
            Destroy(image);
            frames.Add(new Frame { name = variant.Key, png = name, spriteColor = spriteColor,
                clearColor = clearColor, globalCityTransparencyAtRender = globalTransparencyAtRender,
                maskSource = "original_unity_imported", width = contract.camera.width, height = contract.camera.height });
        }
        StageLog("before_gpu_mask_export");
        var exportTarget = new RenderTexture(mask.texture.width, mask.texture.height, 0,
            RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
        exportTarget.Create();
        Graphics.Blit(mask.texture, exportTarget);
        RenderTexture.active = exportTarget;
        var exportedImage = new Texture2D(mask.texture.width, mask.texture.height, TextureFormat.RGBA32, false, true);
        exportedImage.ReadPixels(new Rect(0, 0, mask.texture.width, mask.texture.height), 0, 0);
        exportedImage.Apply(false);
        const string exportedName = "academy_mask_imported_gpu_rgba.png";
        var exportedPng = exportedImage.EncodeToPNG();
        File.WriteAllBytes(Output + "/" + exportedName, exportedPng);
        var maskExport = new MaskExport {
            png = exportedName,
            method = "Graphics.Blit imported Texture2D to same-size ARGB32 Linear RT; RGBA32 ReadPixels; EncodeToPNG",
            limitation = "GPU-decoded/sampled RGBA8, including an additional 8-bit render-target quantization; not original compressed texture bytes",
            width = exportedImage.width, height = exportedImage.height,
            readbackFormat = (int)exportedImage.format,
            renderTargetFormat = (int)exportTarget.format,
            renderTargetSrgb = exportTarget.sRGB,
        };
        Destroy(exportedImage);
        exportTarget.Release();
        Destroy(exportTarget);
        StageLog("after_gpu_mask_export_before_control");
        var exportedMask = new Texture2D(mask.texture.width, mask.texture.height, TextureFormat.RGBA32, false, true);
        if (!exportedMask.LoadImage(exportedPng, false) || exportedMask.width != mask.texture.width ||
            exportedMask.height != mask.texture.height)
            throw new Exception("GPU-exported Academy mask could not be reloaded for control");
        exportedMask.filterMode = mask.texture.filterMode;
        exportedMask.wrapMode = mask.texture.wrapMode;
        maskExport.reloadFormat = (int)exportedMask.format;
        maskExport.reloadFilterMode = (int)exportedMask.filterMode;
        maskExport.reloadWrapMode = (int)exportedMask.wrapMode;
        renderer.GetPropertyBlock(block);
        block.SetTexture("_Mask", exportedMask);
        renderer.SetPropertyBlock(block);
        renderer.GetPropertyBlock(block);
        if (block.GetTexture("_Mask") != exportedMask)
            throw new Exception("GPU-exported Academy mask control binding failed");
        Color dayColor = ColorFrom(contract.lighting.day_sprite_color);
        Color dayClear = new Color(0.5255f, 0.6314f, 0.3216f, 1f) * dayColor;
        Shader.SetGlobalColor("_SpriteColor", dayColor);
        Shader.SetGlobalFloat("_CityTransparency", contract.lighting.city_transparency);
        float controlTransparency = Shader.GetGlobalFloat("_CityTransparency");
        if (!Near(controlTransparency, contract.lighting.city_transparency))
            throw new Exception("Academy export-control global transparency changed");
        camera.backgroundColor = dayClear;
        StageLog("before_render_day_exported_mask_control");
        camera.Render();
        StageLog("after_render_day_exported_mask_control");
        RenderTexture.active = target;
        var controlImage = new Texture2D(contract.camera.width, contract.camera.height, TextureFormat.RGB24, false);
        controlImage.ReadPixels(new Rect(0, 0, contract.camera.width, contract.camera.height), 0, 0);
        controlImage.Apply(false);
        const string controlName = "academy_day_exported_mask_control.png";
        File.WriteAllBytes(Output + "/" + controlName, controlImage.EncodeToPNG());
        Destroy(controlImage);
        frames.Add(new Frame { name = "day_exported_mask_control", png = controlName,
            spriteColor = dayColor, clearColor = dayClear, globalCityTransparencyAtRender = controlTransparency,
            maskSource = "gpu_exported_rgba8_reload", width = contract.camera.width, height = contract.camera.height });
        var report = new Report {
            runDirectory = Output,
            unityVersion = Application.unityVersion, colorSpace = QualitySettings.activeColorSpace.ToString(),
            graphicsDevice = SystemInfo.graphicsDeviceName, shaderName = material.shader.name,
            spriteAsset = spritePath, maskAsset = maskPath, materialAsset = materialPath,
            maskBinding = "original Client.MaskSprite.Start MaterialPropertyBlock _Mask = mask_sprite.texture",
            spriteTextureWidth = sprite.texture.width, spriteTextureHeight = sprite.texture.height,
            maskTextureWidth = mask.texture.width, maskTextureHeight = mask.texture.height,
            materialColor1 = material.GetColor("_Color_1"), materialColor2 = material.GetColor("_Color_2"),
            serializedSourceMaterialCityTransparency = contract.material.city_transparency,
            materialHasCityTransparencyProperty = materialHasTransparency,
            materialGetFloatAttempted = materialGetFloatAttempted,
            materialGetFloatAvailable = materialHasTransparency,
            materialGetFloatObservation = materialGetFloatAttempted ? "measured" : "unavailable: shader does not declare material property",
            materialGetFloatCityTransparencyObserved = materialGetFloatTransparency,
            globalCityTransparencyBeforeOverride = globalTransparencyBeforeOverride,
            rendererColor = renderer.color, rendererFlipX = renderer.flipX, rendererFlipY = renderer.flipY,
            rendererEnabled = renderer.enabled, rendererSortingOrder = renderer.sortingOrder,
            spriteImporterPixelsPerUnit = spriteImporter.spritePixelsPerUnit,
            maskImporterPixelsPerUnit = maskImporter.spritePixelsPerUnit,
            spriteImporterPivot = spriteImporter.spritePivot, maskImporterPivot = maskImporter.spritePivot,
            spriteImporterMeshType = (int)spriteImporterSettings.spriteMeshType,
            maskImporterMeshType = (int)maskImporterSettings.spriteMeshType,
            spriteImporterSrgb = spriteImporter.sRGBTexture, maskImporterSrgb = maskImporter.sRGBTexture,
            spriteImporterAlphaIsTransparency = spriteImporter.alphaIsTransparency,
            maskImporterAlphaIsTransparency = maskImporter.alphaIsTransparency,
            spriteImport = ImportInfo(spriteImporter, sprite.texture),
            maskImport = ImportInfo(maskImporter, mask.texture),
            maskExport = maskExport,
            geometry = geometry, camera = contract.camera, frames = frames.ToArray(),
        };
        File.WriteAllText(Output + "/native_render.json", JsonUtility.ToJson(report, true));
        Destroy(exportedMask);
        RenderTexture.active = null;
        target.Release();
        Destroy(target);
    }
}
