using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using Client;
using UnityEditor;
using UnityEngine;

// Isolated, source-material GPU composition probe. Launch Unity only through Run().
public sealed class RenderRokCityGammaCompositeNative : MonoBehaviour
{
    private const string Root = "D:/ueDemo/Rok";
    private const string Probe = Root + "/Saved/UnityCityGammaCompositeProbe";
    private const string OutputRoot = Probe + "/OutputRuns";
    private const string ModeKey = "RokCityGammaCompositeNativeMode";
    private static string Output;
    private static System.Threading.Timer HardWatchdog;
    private static System.Diagnostics.Stopwatch Clock;
    private static volatile string Stage = "not-started";

    [Serializable] public sealed class V3 { public float x, y, z; }
    [Serializable] public sealed class Q4 { public float x, y, z, w; }
    [Serializable] public sealed class Rgba { public float r, g, b, a; }
    [Serializable] public sealed class TransformData { public long file_id, parent_transform_file_id; public V3 local_position, local_scale; public Q4 local_rotation; }
    [Serializable] public sealed class RendererData { public bool enabled, flip_x, flip_y; public int sorting_order; public Rgba color; }
    [Serializable] public sealed class NodeData { public long file_id; public string name; public bool active; public int layer; public TransformData transform; public RendererData renderer; }
    [Serializable] public sealed class AssetData { public string path; }
    [Serializable] public sealed class ModelData
    {
        public string label, model, source_catalog_sha256;
        public NodeData root, sprite_child;
        public AssetData sprite, mask;
    }
    [Serializable] public sealed class MaterialData { public string path; public Rgba color_1, color_2; public float city_transparency; }
    [Serializable] public sealed class CameraData { public string source_row; public float dxf, distance, vertical_fov; public float[] forward; public int width, height; }
    [Serializable] public sealed class LightingData { public float[] neutral_sprite_color, day_sprite_color; public float city_transparency; }
    [Serializable] public sealed class OffsetData { public float[] academy, farm, wood; }
    [Serializable] public sealed class LayerData { public string label; public int order; }
    [Serializable] public sealed class FixtureData { public string name, light, background; public LayerData[] layers; }
    [Serializable] public sealed class Contract
    {
        public string schema, renderer_script_sha256;
        public ModelData[] models;
        public MaterialData material;
        public CameraData camera;
        public LightingData lighting;
        public OffsetData diagnostic_offsets;
        public FixtureData[] fixtures;
    }
    [Serializable] public sealed class ImportedTexture
    {
        public int width, height, mipmapCount, format, filterMode, wrapMode;
        public float pixelsPerUnit;
        public Vector2 importerPivot;
        public int importerMeshType;
        public bool importerSrgb, importerAlphaIsTransparency;
    }
    [Serializable] public sealed class Geometry
    {
        public Rect rect, textureRect;
        public Vector2 pivot, textureRectOffset;
        public float pixelsPerUnit;
        public Vector2[] vertices, uvs, screenVertices;
        public int[] triangles;
        public Vector3[] worldVertices;
        public Vector3 spriteBoundsCenter, spriteBoundsExtents;
        public Vector3 rendererBoundsCenter, rendererBoundsExtents;
        public Vector3 sourceRootLocalPosition, sourceRootLocalScale, childLocalPosition, childLocalScale;
        public Quaternion sourceRootLocalRotation, childLocalRotation;
        public Vector3 diagnosticOffset, actualRootPosition, childWorldPosition;
        public Quaternion childWorldRotation;
    }
    [Serializable] public sealed class ModelReport
    {
        public string label, model, sourceCatalogSha256, spriteAsset, maskAsset, materialAsset;
        public string maskBinding;
        public int sourceSortingOrder, nativeLayer;
        public string nativeSortingLayer;
        public Color rendererColor;
        public bool flipX, flipY;
        public ImportedTexture spriteImport, maskImport;
        public Geometry geometry;
        public string coveragePng;
    }
    [Serializable] public sealed class FrameReport
    {
        public string name, png, light, background, backgroundMethod;
        public LayerData[] layers;
        public Color spriteColor, flatClearColor;
        public float globalCityTransparencyAtRender;
        public int width, height;
        public bool cameraAllowHDR, cameraAllowMSAA, targetSrgb;
        public int targetFormat;
        public string targetFormatName;
    }
    [Serializable] public sealed class Report
    {
        public string schema = "rok.city_gamma_composite_native_render.v1";
        public string qualification = "Actual Unity 2019 Gamma GPU source SpriteRenderer blending. Offsets/sorting are diagnostic controls, not actual city layout. Separate transparent-black coverage RT stores postblend alpha approximately source-alpha squared due Blend SrcAlpha OneMinusSrcAlpha on alpha; its quantized sqrt is not direct opacity evidence.";
        public string runDirectory, unityVersion, colorSpace, graphicsDevice, shaderName, materialAsset;
        public Color materialColor1, materialColor2;
        public bool materialHasCityTransparencyProperty;
        public float globalCityTransparencyBeforeOverride;
        public CameraData camera;
        public string patternSourcePng;
        public ModelReport[] models;
        public FrameReport[] frames;
    }
    private sealed class Layer
    {
        public ModelData source;
        public SpriteRenderer renderer;
        public Sprite sprite, mask;
        public Transform root, child;
        public TextureImporter spriteImporter, maskImporter;
        public Vector3 offset;
    }

    private static void StageLog(string stage)
    {
        Stage = stage;
        Debug.Log("ROK_CITY_GAMMA_COMPOSITE_STAGE " + stage);
    }

    private static void OnEditorUpdate()
    {
        if (Clock == null || Clock.Elapsed.TotalSeconds < 60) return;
        var stage = Stage;
        bool paused = EditorApplication.isPaused;
        StopSoftWatchdog();
        Debug.LogError("ROK_CITY_GAMMA_COMPOSITE_SOFT_TIMEOUT stage=" + stage + " paused=" + paused);
        EditorApplication.Exit(1);
    }

    private static void StopSoftWatchdog()
    {
        EditorApplication.update -= OnEditorUpdate;
        Clock = null;
    }

    private static void DisarmWatchdog()
    {
        StopSoftWatchdog();
        if (HardWatchdog != null) { HardWatchdog.Dispose(); HardWatchdog = null; }
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
                File.WriteAllText(Output + "/watchdog_timeout.txt", "stage=" + Stage + "\n");
            }
            finally { Environment.Exit(2); }
        }, null, 150000, Timeout.Infinite);
    }

    [InitializeOnLoadMethod]
    private static void Install() { EditorApplication.playModeStateChanged += OnPlayMode; }

    public static void Run()
    {
        if (Application.dataPath.Replace('\\', '/') != Probe + "/Assets" || Application.unityVersion != "2019.4.28f1c1")
            throw new Exception("Wrong Unity city Gamma composition probe project/version");
        SessionState.SetBool(ModeKey, true);
        EditorApplication.isPlaying = true;
    }

    private static void OnPlayMode(PlayModeStateChange state)
    {
        if (state != PlayModeStateChange.EnteredPlayMode || !SessionState.GetBool(ModeKey, false)) return;
        SessionState.SetBool(ModeKey, false);
        Output = OutputRoot + "/" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") + "_" + System.Diagnostics.Process.GetCurrentProcess().Id;
        if (Directory.Exists(Output)) throw new Exception("Composition output already exists");
        Directory.CreateDirectory(Output);
        Debug.Log("ROK_CITY_GAMMA_COMPOSITE_OUTPUT " + Output);
        StageLog("entered_play_mode");
        ArmWatchdog();
        new GameObject("CityGammaCompositeNativeRender").AddComponent<RenderRokCityGammaCompositeNative>();
    }

    private static bool Near(float a, float b) { return Mathf.Abs(a - b) < 0.00001f; }
    private static Vector3 Vec(V3 v) { return new Vector3(v.x, v.y, v.z); }
    private static Quaternion Rot(Q4 q) { return new Quaternion(q.x, q.y, q.z, q.w); }
    private static Color Col(Rgba c) { return new Color(c.r, c.g, c.b, c.a); }
    private static Color Col(float[] values)
    {
        if (values == null || values.Length != 4) throw new Exception("Invalid source RGBA");
        return new Color(values[0], values[1], values[2], values[3]);
    }
    private static Vector3 Offset(float[] values)
    {
        if (values == null || values.Length != 3) throw new Exception("Invalid diagnostic offset");
        return new Vector3(values[0], values[1], values[2]);
    }
    private static void Apply(Transform transform, TransformData source)
    {
        transform.localPosition = Vec(source.local_position);
        transform.localRotation = Rot(source.local_rotation);
        transform.localScale = Vec(source.local_scale);
    }

    private static ImportedTexture Import(TextureImporter importer, Texture2D texture)
    {
        var settings = new TextureImporterSettings();
        importer.ReadTextureSettings(settings);
        return new ImportedTexture {
            width = texture.width, height = texture.height, mipmapCount = texture.mipmapCount,
            format = (int)texture.format, filterMode = (int)texture.filterMode, wrapMode = (int)texture.wrapMode,
            pixelsPerUnit = importer.spritePixelsPerUnit, importerPivot = importer.spritePivot,
            importerMeshType = (int)settings.spriteMeshType, importerSrgb = importer.sRGBTexture,
            importerAlphaIsTransparency = importer.alphaIsTransparency,
        };
    }

    private static Geometry GeometryOf(Layer layer, Camera camera)
    {
        var sprite = layer.sprite;
        var vertices = sprite.vertices;
        var world = new Vector3[vertices.Length];
        var screen = new Vector2[vertices.Length];
        for (int i = 0; i < vertices.Length; ++i)
        {
            world[i] = layer.child.TransformPoint(vertices[i]);
            Vector3 point = camera.WorldToScreenPoint(world[i]);
            screen[i] = new Vector2(point.x, point.y);
        }
        return new Geometry {
            rect = sprite.rect, textureRect = sprite.textureRect, pivot = sprite.pivot,
            textureRectOffset = sprite.textureRectOffset, pixelsPerUnit = sprite.pixelsPerUnit,
            vertices = vertices, uvs = sprite.uv, triangles = Array.ConvertAll(sprite.triangles, x => (int)x),
            worldVertices = world, screenVertices = screen,
            spriteBoundsCenter = sprite.bounds.center, spriteBoundsExtents = sprite.bounds.extents,
            rendererBoundsCenter = layer.renderer.bounds.center, rendererBoundsExtents = layer.renderer.bounds.extents,
            sourceRootLocalPosition = Vec(layer.source.root.transform.local_position),
            sourceRootLocalRotation = Rot(layer.source.root.transform.local_rotation),
            sourceRootLocalScale = Vec(layer.source.root.transform.local_scale),
            childLocalPosition = layer.child.localPosition, childLocalRotation = layer.child.localRotation,
            childLocalScale = layer.child.localScale,
            diagnosticOffset = layer.offset, actualRootPosition = layer.root.position,
            childWorldPosition = layer.child.position, childWorldRotation = layer.child.rotation,
        };
    }

    // Stable nonuniform opaque background. A source PNG of these exact bytes is
    // saved separately, so the verifier can check the GPU blit independently.
    private static Texture2D CreatePattern(int width, int height)
    {
        var pixels = new Color32[width * height];
        for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            int checker = ((x / 64) + (y / 64)) & 1;
            pixels[y * width + x] = new Color32(
                (byte)(47 + (x * 89 / (width - 1)) + 21 * checker),
                (byte)(63 + (y * 92 / (height - 1)) + 13 * checker),
                (byte)(41 + ((x + 2 * y) % 97) + 17 * checker), 255);
        }
        var result = new Texture2D(width, height, TextureFormat.RGBA32, false, false);
        result.SetPixels32(pixels);
        result.filterMode = FilterMode.Point;
        result.wrapMode = TextureWrapMode.Clamp;
        result.Apply(false);
        File.WriteAllBytes(Output + "/pattern_source.png", result.EncodeToPNG());
        return result;
    }

    private static void WriteFrame(Camera camera, RenderTexture target, string filename, bool alpha)
    {
        RenderTexture.active = target;
        var image = new Texture2D(target.width, target.height,
            alpha ? TextureFormat.RGBA32 : TextureFormat.RGB24, false);
        image.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0);
        image.Apply(false);
        File.WriteAllBytes(Output + "/" + filename, image.EncodeToPNG());
        Destroy(image);
        RenderTexture.active = null;
    }

    private IEnumerator Start()
    {
        bool failed = false;
        StageLog("before_capture");
        var enumerator = Capture();
        while (true)
        {
            object yielded = null;
            bool hasMore = false;
            try { hasMore = enumerator.MoveNext(); if (hasMore) yielded = enumerator.Current; }
            catch (Exception exception) { Debug.LogException(exception); failed = true; break; }
            if (!hasMore) break;
            yield return yielded;
        }
        RenderTexture.active = null;
        if (failed)
        {
            StageLog("capture_failed_exit_requested");
            StopSoftWatchdog();
            EditorApplication.Exit(1);
            yield break;
        }
        StageLog("capture_complete_exit_requested");
        DisarmWatchdog();
        EditorApplication.Exit(0);
    }

    private IEnumerator Capture()
    {
        if (Application.unityVersion != "2019.4.28f1c1" || QualitySettings.activeColorSpace != ColorSpace.Gamma)
            throw new Exception("Source Unity 2019.4 Gamma required");
        var contract = JsonUtility.FromJson<Contract>(File.ReadAllText(Probe + "/probe_contract.json"));
        if (contract == null || contract.schema != "rok.city_gamma_composite_native_contract.v1" ||
            contract.models == null || contract.models.Length != 3 || contract.fixtures == null || contract.fixtures.Length != 8)
            throw new Exception("Wrong Gamma composite contract");
        if (contract.camera.source_row != "city_default" || !Near(contract.camera.distance, 20f) ||
            !Near(contract.camera.vertical_fov, 7.5f) || contract.camera.width != 1280 || contract.camera.height != 720)
            throw new Exception("Source camera changed");
        const string materialPath = "Assets/Source/map_building.mat";
        var material = AssetDatabase.LoadAssetAtPath<Material>(materialPath);
        if (material == null || material.shader == null || material.shader.name != "custom/map/map_building_with_mask-5")
            throw new Exception("Source material/shader missing");
        if (material.HasProperty("_CityTransparency"))
            throw new Exception("Source global CityTransparency became a material property");
        Color materialColor1 = material.GetColor("_Color_1"), materialColor2 = material.GetColor("_Color_2");
        if (!Near(materialColor1.r, contract.material.color_1.r) ||
            !Near(materialColor1.g, contract.material.color_1.g) ||
            !Near(materialColor1.b, contract.material.color_1.b) ||
            !Near(materialColor2.r, contract.material.color_2.r) ||
            !Near(materialColor2.g, contract.material.color_2.g) ||
            !Near(materialColor2.b, contract.material.color_2.b))
            throw new Exception("Source material colors changed");
        float previousGlobalTransparency = Shader.GetGlobalFloat("_CityTransparency");

        var layers = new Dictionary<string, Layer>();
        foreach (var model in contract.models)
        {
            if (!model.root.active || !model.sprite_child.active || !model.sprite_child.renderer.enabled ||
                model.sprite_child.transform.parent_transform_file_id != model.root.transform.file_id ||
                layers.ContainsKey(model.label))
                throw new Exception("Source prefab renderer hierarchy changed: " + model.label);
            string spritePath = "Assets/Source/" + Path.GetFileName(model.sprite.path);
            string maskPath = "Assets/Source/" + Path.GetFileName(model.mask.path);
            var sprite = AssetDatabase.LoadAssetAtPath<Sprite>(spritePath);
            var mask = AssetDatabase.LoadAssetAtPath<Sprite>(maskPath);
            var spriteImporter = AssetImporter.GetAtPath(spritePath) as TextureImporter;
            var maskImporter = AssetImporter.GetAtPath(maskPath) as TextureImporter;
            if (sprite == null || mask == null || spriteImporter == null || maskImporter == null)
                throw new Exception("Source sprite/mask missing: " + model.label);
            float[] rawOffset = model.label == "academy" ? contract.diagnostic_offsets.academy :
                model.label == "farm" ? contract.diagnostic_offsets.farm : contract.diagnostic_offsets.wood;
            Vector3 offset = Offset(rawOffset);
            var rootObject = new GameObject(model.root.name);
            rootObject.layer = model.root.layer;
            Apply(rootObject.transform, model.root.transform);
            rootObject.transform.localPosition += offset;
            var childObject = new GameObject(model.sprite_child.name);
            childObject.layer = model.sprite_child.layer;
            childObject.transform.SetParent(rootObject.transform, false);
            Apply(childObject.transform, model.sprite_child.transform);
            var renderer = childObject.AddComponent<SpriteRenderer>();
            renderer.sprite = sprite;
            renderer.sharedMaterial = material;
            renderer.color = Col(model.sprite_child.renderer.color);
            renderer.flipX = model.sprite_child.renderer.flip_x;
            renderer.flipY = model.sprite_child.renderer.flip_y;
            renderer.sortingOrder = model.sprite_child.renderer.sorting_order;
            var sourceMaskScript = childObject.AddComponent<MaskSprite>();
            sourceMaskScript.m_mask_sprite = mask;
            layers.Add(model.label, new Layer { source = model, renderer = renderer, sprite = sprite,
                mask = mask, root = rootObject.transform, child = childObject.transform,
                spriteImporter = spriteImporter, maskImporter = maskImporter, offset = offset });
        }
        StageLog("before_mask_start_yield_1");
        yield return null;
        StageLog("after_mask_start_yield_1_before_2");
        yield return null;
        StageLog("after_mask_start_yield_2");
        foreach (var layer in layers.Values)
        {
            var block = new MaterialPropertyBlock();
            layer.renderer.GetPropertyBlock(block);
            if (block.GetTexture("_Mask") != layer.mask.texture)
                throw new Exception("Original MaskSprite did not bind source _Mask: " + layer.source.label);
        }

        var cameraObject = new GameObject("SourceCityDefaultCamera");
        var camera = cameraObject.AddComponent<Camera>();
        camera.orthographic = false;
        camera.fieldOfView = contract.camera.vertical_fov;
        camera.aspect = (float)contract.camera.width / contract.camera.height;
        camera.nearClipPlane = 0.01f;
        camera.farClipPlane = 1000f;
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.allowHDR = false;
        camera.allowMSAA = false;
        Vector3 forward = new Vector3(contract.camera.forward[0], contract.camera.forward[1],
            contract.camera.forward[2]).normalized;
        camera.transform.rotation = Quaternion.LookRotation(forward, Vector3.up);
        camera.transform.position = -forward * contract.camera.distance;
        var target = new RenderTexture(contract.camera.width, contract.camera.height, 24,
            RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
        target.Create();
        camera.targetTexture = target;
        var reports = new List<ModelReport>();
        foreach (var model in contract.models)
        {
            var layer = layers[model.label];
            reports.Add(new ModelReport {
                label = model.label, model = model.model, sourceCatalogSha256 = model.source_catalog_sha256,
                spriteAsset = "Assets/Source/" + Path.GetFileName(model.sprite.path),
                maskAsset = "Assets/Source/" + Path.GetFileName(model.mask.path),
                materialAsset = materialPath,
                maskBinding = "original Client.MaskSprite.Start MaterialPropertyBlock _Mask = mask_sprite.texture",
                sourceSortingOrder = model.sprite_child.renderer.sorting_order,
                nativeLayer = layer.renderer.gameObject.layer, nativeSortingLayer = layer.renderer.sortingLayerName,
                rendererColor = layer.renderer.color, flipX = layer.renderer.flipX, flipY = layer.renderer.flipY,
                spriteImport = Import(layer.spriteImporter, layer.sprite.texture),
                maskImport = Import(layer.maskImporter, layer.mask.texture),
                geometry = GeometryOf(layer, camera), coveragePng = model.label + "_coverage_rgba.png",
            });
        }

        // Capture separate GPU coverage for each complete source mesh. Its alpha
        // reflects blend to transparent black and is used only as a frozen mask.
        Shader.SetGlobalColor("_SpriteColor", Col(contract.lighting.day_sprite_color));
        Shader.SetGlobalFloat("_CityTransparency", contract.lighting.city_transparency);
        foreach (var model in contract.models)
        {
            foreach (var layer in layers.Values) layer.renderer.enabled = layer.source.label == model.label;
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = Color.clear;
            StageLog("before_coverage_" + model.label);
            camera.Render();
            StageLog("after_coverage_" + model.label);
            WriteFrame(camera, target, model.label + "_coverage_rgba.png", true);
        }

        var pattern = CreatePattern(contract.camera.width, contract.camera.height);
        var frames = new List<FrameReport>();
        foreach (var fixture in contract.fixtures)
        {
            foreach (var layer in layers.Values) layer.renderer.enabled = false;
            var used = new HashSet<string>();
            foreach (var item in fixture.layers)
            {
                if (!used.Add(item.label) || !layers.ContainsKey(item.label))
                    throw new Exception("Duplicate/unknown fixture model: " + fixture.name);
                layers[item.label].renderer.sortingOrder = item.order;
                layers[item.label].renderer.enabled = true;
            }
            Color light = fixture.light == "neutral" ? Col(contract.lighting.neutral_sprite_color) :
                fixture.light == "day" ? Col(contract.lighting.day_sprite_color) :
                throw new Exception("Unknown light control: " + fixture.name);
            Color clear = new Color(0.5255f, 0.6314f, 0.3216f, 1f) * light;
            Shader.SetGlobalColor("_SpriteColor", light);
            Shader.SetGlobalFloat("_CityTransparency", contract.lighting.city_transparency);
            float measuredGlobal = Shader.GetGlobalFloat("_CityTransparency");
            if (!Near(measuredGlobal, contract.lighting.city_transparency))
                throw new Exception("Global CityTransparency did not apply");
            string method;
            if (fixture.background == "flat")
            {
                camera.clearFlags = CameraClearFlags.SolidColor;
                camera.backgroundColor = clear;
                method = "camera solid-color source Academy control";
            }
            else if (fixture.background == "pattern")
            {
                // Blit opaque nonuniform pixels, then clear only depth before
                // the source SpriteRenderers draw into the same ARGB32 target.
                Graphics.Blit(pattern, target);
                camera.clearFlags = CameraClearFlags.Depth;
                method = "GPU point-filter pattern blit then depth-only camera clear";
            }
            else throw new Exception("Unknown background: " + fixture.name);
            StageLog("before_render_" + fixture.name);
            camera.Render();
            StageLog("after_render_" + fixture.name);
            string png = fixture.name + ".png";
            WriteFrame(camera, target, png, false);
            var measuredLayers = new LayerData[fixture.layers.Length];
            for (int i = 0; i < fixture.layers.Length; ++i)
            {
                var requested = fixture.layers[i];
                var actual = layers[requested.label].renderer;
                if (!actual.enabled || actual.sortingOrder != requested.order)
                    throw new Exception("Actual native renderer order differs: " + fixture.name);
                measuredLayers[i] = new LayerData { label = requested.label, order = actual.sortingOrder };
            }
            frames.Add(new FrameReport {
                name = fixture.name, png = png, light = fixture.light, background = fixture.background,
                backgroundMethod = method, layers = measuredLayers, spriteColor = light,
                flatClearColor = clear, globalCityTransparencyAtRender = measuredGlobal,
                width = contract.camera.width, height = contract.camera.height,
                cameraAllowHDR = camera.allowHDR, cameraAllowMSAA = camera.allowMSAA,
                targetSrgb = target.sRGB, targetFormat = (int)target.format,
                targetFormatName = target.format.ToString(),
            });
        }
        var report = new Report {
            runDirectory = Output, unityVersion = Application.unityVersion,
            colorSpace = QualitySettings.activeColorSpace.ToString(),
            graphicsDevice = SystemInfo.graphicsDeviceName, shaderName = material.shader.name,
            materialAsset = materialPath, materialColor1 = materialColor1,
            materialColor2 = materialColor2,
            materialHasCityTransparencyProperty = material.HasProperty("_CityTransparency"),
            globalCityTransparencyBeforeOverride = previousGlobalTransparency,
            camera = contract.camera, patternSourcePng = "pattern_source.png",
            models = reports.ToArray(), frames = frames.ToArray(),
        };
        File.WriteAllText(Output + "/native_render.json", JsonUtility.ToJson(report, true));
        RenderTexture.active = null;
        target.Release();
        Destroy(target);
        Destroy(pattern);
    }
}
