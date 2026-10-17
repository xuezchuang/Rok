using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;

// Isolated source presentation probe. No GameApp, server event or hotfix mediator is started.
public static class RenderRokConstructionScaffoldNative
{
    const string Repo = "D:/ueDemo/Rok";
    const string Probe = Repo + "/Saved/UnityConstructionScaffoldProbe";
    const string Prefix = "Assets/Source/";
    const string InventoryHash = "01dd55e415bfc8f9284642f22e73f47e111a1b34ccaf126788e7357189844ffe";
    const string PrefabPath = Prefix + "BundleAssets/Scene/City_building/city_building_1/Build_shelf_5.prefab";
    [Serializable] public class SourceFile { public string source_path, source_sha256, meta_sha256, probe_path; public bool copied; }
    [Serializable] public class MaskBinding
    {
        public string scaffold, sprite_node, sprite_guid, sprite_path, sprite_sha256;
        public string mask_guid, mask_path, mask_sha256, source_prefab_path, source_prefab_sha256;
        public long sprite_file_id, mask_file_id;
    }
    [Serializable] public class CameraContract
    {
        public string source_row; public float dxf, distance, vertical_fov, city_transparency;
        public float[] forward, day_sprite_color; public int width, height;
    }
    [Serializable] public class Contract
    {
        public string schema, repo_root, probe_root, unity_version, source_color_space;
        public string source_project_settings_sha256, source_inventory_sha256, exporter_sha256, lifecycle_limit;
        public string camera_evidence_path, camera_evidence_sha256;
        public SourceFile[] source_files; public string[] omitted_script_guid_refs, allowed_builtin_guid_refs;
        public MaskBinding[] mask_bindings; public CameraContract camera;
    }
    [Serializable] public class AssetRef
    {
        public string path, source_path, guid, sha256, meta_sha256;
        public long file_id; public bool builtin_or_generated;
    }
    [Serializable] public class Node
    {
        public string path, parent_path, name; public bool active_self, active_in_hierarchy;
        public Vector3 local_position, local_scale; public Quaternion local_rotation;
        public Matrix4x4 local_to_world; public int missing_mono_behaviours;
    }
    [Serializable] public class SpriteLayer
    {
        public string node; public AssetRef sprite, mask_sprite, material, shader;
        public Rect sprite_rect, texture_rect; public Vector2 pivot; public float pixels_per_unit;
        public Vector2[] vertices, uv; public ushort[] triangles;
        public Color color; public int sorting_layer_id, sorting_order;
        public bool renderer_enabled, mask_property_block_verified;
        public string mask_texture_path, mask_texture_sha256, shader_name, material_serialized_unity_json;
        public Bounds renderer_bounds, sprite_bounds;
    }
    [Serializable] public class Key { public float time, value, in_tangent, out_tangent, in_weight, out_weight; public string weighted_mode; }
    [Serializable] public class Curve
    {
        public string path, type, property, pre_wrap, post_wrap; public Key[] keys;
    }
    [Serializable] public class Clip
    {
        public AssetRef asset; public string name, wrap_mode; public bool legacy, loop_time;
        public float length, frame_rate; public Curve[] curves;
    }
    [Serializable] public class AnimationComponent
    {
        public string node, clip; public bool source_enabled, source_play_automatically;
    }
    [Serializable] public class LayerFrame
    {
        public string node; public bool active, renderer_enabled, mask_property_block_verified;
        public string mask_texture_guid, mask_texture_sha256;
        public Vector3 local_position, local_scale; public Quaternion local_rotation;
        public Bounds world_bounds;
        public Vector3[] world_vertices; public Vector2[] screen_vertices;
    }
    [Serializable] public class Frame
    {
        public string phase, png, png_sha256, observation_kind;
        public float elapsed_seconds; public int non_background_pixels, bbox_x0, bbox_y0, bbox_x1, bbox_y1;
        public bool[] animation_enabled; public bool build_3003_active;
        public int particle_count; public LayerFrame[] layers;
    }
    [Serializable] public class Report
    {
        public string schema = "rok.construction_scaffold_native_render.v1";
        public bool passed, game_app_loaded, server_event_replayed, actual_mask_script_loaded;
        public string project_path, run_directory, unity_version, color_space, graphics_device;
        public string source_inventory_sha256, source_contract_sha256, renderer_sha256, camera_evidence_sha256;
        public string source_scope, lifecycle_boundary, particle_limit, source_camera_limit;
        public CameraContract camera; public Vector4 applied_sprite_color;
        public float applied_city_transparency;
        public Quaternion mediator_assigned_local_rotation, observed_root_local_rotation;
        public string background_control_png, background_control_sha256;
        public SourceFile[] source_files; public MaskBinding[] source_mask_bindings;
        public Node[] source_nodes; public SpriteLayer[] layers;
        public AnimationComponent[] source_animation_components; public Clip[] clips;
        public Frame[] frames; public string[] unresolved_references, failures;
    }

    static Contract contract; static Report report; static string output;
    static readonly Dictionary<string, SourceFile> SourceFiles = new Dictionary<string, SourceFile>(StringComparer.OrdinalIgnoreCase);
    static readonly List<string> Unresolved = new List<string>();
    static string Normal(string path) { return Path.GetFullPath(path).Replace('\\', '/').TrimEnd('/'); }
    static string Hash(string path)
    {
        using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant();
    }
    static void Need(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
    static AssetRef Ref(UnityEngine.Object asset)
    {
        if (!asset) return null;
        string path = AssetDatabase.GetAssetPath(asset); string guid; long id;
        var result = new AssetRef { path = path, builtin_or_generated = string.IsNullOrEmpty(path) || !path.StartsWith(Prefix, StringComparison.Ordinal) };
        if (AssetDatabase.TryGetGUIDAndLocalFileIdentifier(asset, out guid, out id))
        { result.guid = guid; result.file_id = id; }
        SourceFile source;
        if (SourceFiles.TryGetValue(path, out source))
        { result.source_path = source.source_path; result.sha256 = source.source_sha256; result.meta_sha256 = source.meta_sha256; }
        else if (!result.builtin_or_generated) Unresolved.Add("Unpinned imported asset: " + path);
        return result;
    }
    static IEnumerable<Transform> Tree(Transform root)
    {
        yield return root;
        foreach (Transform child in root) foreach (var node in Tree(child)) yield return node;
    }
    static string PathOf(Transform node, Transform root)
    {
        var names = new List<string>();
        for (var current = node; current; current = current.parent)
        {
            names.Add(current.name + "[" + current.GetSiblingIndex() + "]");
            if (current == root) break;
        }
        names.Reverse(); return string.Join("/", names.ToArray());
    }
    static uint Seed(string path)
    {
        uint hash = 2166136261;
        foreach (char c in path) { hash ^= c; hash *= 16777619; }
        return hash == 0 ? 1u : hash;
    }
    static Key[] Keys(AnimationCurve curve)
    {
        return curve.keys.Select(k => new Key { time = k.time, value = k.value,
            in_tangent = k.inTangent, out_tangent = k.outTangent,
            in_weight = k.inWeight, out_weight = k.outWeight,
            weighted_mode = k.weightedMode.ToString() }).ToArray();
    }
    static Clip ReadClip(AnimationClip clip)
    {
        return new Clip { asset = Ref(clip), name = clip.name, length = clip.length,
            frame_rate = clip.frameRate, legacy = clip.legacy, wrap_mode = clip.wrapMode.ToString(),
            loop_time = AnimationUtility.GetAnimationClipSettings(clip).loopTime,
            curves = AnimationUtility.GetCurveBindings(clip).Select(binding => {
                var curve = AnimationUtility.GetEditorCurve(clip, binding);
                return new Curve { path = binding.path, type = binding.type.FullName,
                    property = binding.propertyName, pre_wrap = curve.preWrapMode.ToString(),
                    post_wrap = curve.postWrapMode.ToString(), keys = Keys(curve) };
            }).ToArray() };
    }
    static void Save()
    {
        report.unresolved_references = Unresolved.Distinct().OrderBy(x => x).ToArray();
        File.WriteAllText(Path.Combine(output, "native_scaffold_render.json"),
            JsonUtility.ToJson(report, true), new UTF8Encoding(false));
    }
    static string Png(Camera camera, RenderTexture target, string name, out Color32[] pixels)
    {
        camera.Render();
        var previous = RenderTexture.active;
        RenderTexture.active = target;
        var image = new Texture2D(target.width, target.height, TextureFormat.RGB24, false);
        try
        {
            image.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0);
            image.Apply(false); pixels = image.GetPixels32();
            File.WriteAllBytes(Path.Combine(output, name), image.EncodeToPNG());
            return Hash(Path.Combine(output, name));
        }
        finally { RenderTexture.active = previous; UnityEngine.Object.DestroyImmediate(image); }
    }
    static void SeedAndSimulate(ParticleSystem[] systems, Transform root, float time)
    {
        foreach (var system in systems) system.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
        Need(systems.All(s => s.isStopped), "A particle system was playing before seed write");
        foreach (var system in systems)
        {
            var seed = Seed(PathOf(system.transform, root));
            system.useAutoRandomSeed = false; system.randomSeed = seed;
            Need(!system.useAutoRandomSeed && system.randomSeed == seed, "Particle seed write failed");
        }
        foreach (var system in systems)
            if (!system.GetComponentsInParent<ParticleSystem>(true).Any(parent => parent != system))
                system.Simulate(time, true, true, false);
    }
    static Frame Capture(string phase, float elapsed, string kind, GameObject instance,
        Animation[] animations, ParticleSystem[] systems, SpriteRenderer[] sprites,
        Camera camera, RenderTexture target, Color32[] background)
    {
        foreach (var animation in animations)
            if (animation.clip) animation.clip.SampleAnimation(animation.gameObject,
                Mathf.Min(elapsed, animation.clip.length));
        SeedAndSimulate(systems, instance.transform, elapsed);
        var layers = sprites.Select(sprite => {
            var vertices = sprite.sprite.vertices;
            var world = vertices.Select(v => sprite.transform.TransformPoint(v)).ToArray();
            var screen = world.Select(v => {
                var p = camera.WorldToScreenPoint(v); return new Vector2(p.x, p.y);
            }).ToArray();
            var block = new MaterialPropertyBlock(); sprite.GetPropertyBlock(block);
            var maskTexture = Ref(block.GetTexture("_Mask"));
            var binding = contract.mask_bindings.Single(b =>
                sprite.transform.parent && b.scaffold == sprite.transform.parent.name &&
                b.sprite_node == sprite.name);
            bool maskValid = maskTexture != null && maskTexture.guid == binding.mask_guid &&
                maskTexture.sha256 == binding.mask_sha256;
            Need(maskValid, "Per-frame source _Mask binding differs: " + sprite.name);
            return new LayerFrame { node = PathOf(sprite.transform, instance.transform),
                active = sprite.gameObject.activeInHierarchy, renderer_enabled = sprite.enabled,
                mask_property_block_verified = maskValid,
                mask_texture_guid = maskTexture.guid, mask_texture_sha256 = maskTexture.sha256,
                local_position = sprite.transform.localPosition, local_rotation = sprite.transform.localRotation,
                local_scale = sprite.transform.localScale, world_bounds = sprite.bounds,
                world_vertices = world, screen_vertices = screen };
        }).ToArray();
        Color32[] pixels;
        string png = phase + ".png";
        string pngSha = Png(camera, target, png, out pixels);
        int count = 0, x0 = target.width, y0 = target.height, x1 = 0, y1 = 0;
        for (int y = 0; y < target.height; y++) for (int x = 0; x < target.width; x++)
        {
            int i = y * target.width + x;
            if (pixels[i].r == background[i].r && pixels[i].g == background[i].g && pixels[i].b == background[i].b) continue;
            count++; x0 = Math.Min(x0, x); y0 = Math.Min(y0, y);
            x1 = Math.Max(x1, x + 1); y1 = Math.Max(y1, y + 1);
        }
        return new Frame { phase = phase, elapsed_seconds = elapsed, observation_kind = kind,
            png = png, png_sha256 = pngSha, non_background_pixels = count,
            bbox_x0 = count > 0 ? x0 : 0, bbox_y0 = count > 0 ? y0 : 0,
            bbox_x1 = count > 0 ? x1 : 0, bbox_y1 = count > 0 ? y1 : 0,
            animation_enabled = animations.Select(a => a.enabled).ToArray(),
            build_3003_active = instance.transform.Find("build_3003").gameObject.activeSelf,
            particle_count = systems.Sum(s => s.particleCount), layers = layers };
    }
    static Report Render(Camera camera, RenderTexture target)
    {
        var sourcePrefab = AssetDatabase.LoadAssetAtPath<GameObject>(PrefabPath);
        Need(sourcePrefab, "Source Build_shelf_5 prefab did not import");
        var instance = UnityEngine.Object.Instantiate(sourcePrefab);
        instance.name = sourcePrefab.name;
        try
        {
            var root = instance.transform;
            var nodes = Tree(root).ToArray();
            var sprites = nodes.SelectMany(n => n.GetComponents<SpriteRenderer>()).ToArray();
            var animations = nodes.SelectMany(n => n.GetComponents<Animation>()).ToArray();
            var systems = nodes.SelectMany(n => n.GetComponents<ParticleSystem>()).ToArray();
            Need(sprites.Length == 8 && animations.Length == 4 && contract.mask_bindings.Length == 8,
                "Source scaffold sprite/animation count differs");
            var nodeRecords = nodes.Select(n => new Node { path = PathOf(n, root),
                parent_path = n == root ? "" : PathOf(n.parent, root), name = n.name,
                active_self = n.gameObject.activeSelf, active_in_hierarchy = n.gameObject.activeInHierarchy,
                local_position = n.localPosition, local_rotation = n.localRotation, local_scale = n.localScale,
                local_to_world = n.localToWorldMatrix,
                missing_mono_behaviours = GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(n.gameObject)
            }).ToArray();
            var animationRecords = animations.Select(a => new AnimationComponent {
                node = PathOf(a.transform, root), source_enabled = a.enabled,
                source_play_automatically = a.playAutomatically, clip = a.clip ? a.clip.name : ""
            }).ToArray();
            Need(animationRecords.All(a => !a.source_enabled && a.source_play_automatically && a.clip.Length > 0),
                "Source scaffold Animation initial state differs");
            var clips = animations.Select(a => ReadClip(a.clip)).ToArray();
            Need(clips.Sum(c => c.curves.Length) == 15 && clips.All(c => c.length > 0 && c.length < 2),
                "Authored scaffold clip bindings/lengths differ");
            var layers = new List<SpriteLayer>();
            foreach (var sprite in sprites)
            {
                var parent = sprite.transform.parent;
                var binding = contract.mask_bindings.SingleOrDefault(b =>
                    parent && b.scaffold == parent.name && b.sprite_node == sprite.name);
                Need(binding != null, "Source MaskSprite node binding missing: " + PathOf(sprite.transform, root));
                var spriteRef = Ref(sprite.sprite);
                Need(spriteRef != null && spriteRef.guid == binding.sprite_guid &&
                    spriteRef.file_id == binding.sprite_file_id && spriteRef.sha256 == binding.sprite_sha256,
                    "Source SpriteRenderer GUID/fileID differs: " + sprite.name);
                var mask = AssetDatabase.LoadAssetAtPath<Sprite>(binding.mask_path);
                var maskRef = Ref(mask);
                Need(mask && maskRef != null && maskRef.guid == binding.mask_guid &&
                    maskRef.file_id == binding.mask_file_id && maskRef.sha256 == binding.mask_sha256,
                    "Source MaskSprite GUID/fileID differs: " + sprite.name);
                var material = sprite.sharedMaterial;
                Need(material && material.shader &&
                    material.shader.name == "custom/map/map_building_with_mask-5",
                    "Source scaffold material/shader differs: " + sprite.name);
                // Exact MaskSprite.UpdateMaskTex source behavior, scoped to this pinned source mapping.
                var block = new MaterialPropertyBlock(); sprite.GetPropertyBlock(block);
                block.SetTexture("_Mask", mask.texture); sprite.SetPropertyBlock(block);
                var observed = new MaterialPropertyBlock(); sprite.GetPropertyBlock(observed);
                Need(observed.GetTexture("_Mask") == mask.texture,
                    "Per-renderer _Mask property block did not bind: " + sprite.name);
                var sourceSprite = sprite.sprite;
                layers.Add(new SpriteLayer { node = PathOf(sprite.transform, root),
                    sprite = spriteRef, mask_sprite = maskRef, material = Ref(material), shader = Ref(material.shader),
                    sprite_rect = sourceSprite.rect, texture_rect = sourceSprite.textureRect,
                    pivot = sourceSprite.pivot, pixels_per_unit = sourceSprite.pixelsPerUnit,
                    vertices = sourceSprite.vertices, uv = sourceSprite.uv, triangles = sourceSprite.triangles,
                    color = sprite.color, sorting_layer_id = sprite.sortingLayerID,
                    sorting_order = sprite.sortingOrder, renderer_enabled = sprite.enabled,
                    mask_property_block_verified = true, mask_texture_path = binding.mask_path,
                    mask_texture_sha256 = binding.mask_sha256, shader_name = material.shader.name,
                    material_serialized_unity_json = EditorJsonUtility.ToJson(material, true),
                    renderer_bounds = sprite.bounds, sprite_bounds = sourceSprite.bounds });
            }
            var assignedRotation = new Quaternion(0, 0, 0, 0);
            instance.transform.localRotation = assignedRotation; // Exact source mediator assignment.
            var observedRotation = instance.transform.localRotation;
            foreach (var animation in animations) animation.enabled = true;
            var originalState = instance.activeSelf;
            instance.SetActive(false);
            Color32[] background;
            var backgroundName = "source_city_default_background_control.png";
            var backgroundSha = Png(camera, target, backgroundName, out background);
            instance.SetActive(originalState);
            float maxClip = clips.Max(c => c.length);
            var frames = new List<Frame>();
            foreach (var phase in new[] {
                new { name = "00_start", time = 0f },
                new { name = "01_clip_mid", time = maxClip * 0.5f },
                new { name = "02_one_second", time = 1f },
                new { name = "03_clip_end", time = maxClip },
                new { name = "04_before_two_second_timer", time = 1.999f },
                new { name = "05_two_second_timer_before", time = 2f }
            })
                frames.Add(Capture(phase.name, phase.time, "source_curve_sample_and_pre_timer_fixture",
                    instance, animations, systems, sprites, camera, target, background));
            foreach (var animation in animations) animation.enabled = false;
            var effect = instance.transform.Find("build_3003");
            Need(effect, "Source build_3003 child missing at 2-second mediator step");
            effect.gameObject.SetActive(false);
            frames.Add(Capture("06_two_second_timer_after", 2f,
                "source_mediator_2s_callback_equivalent_not_GameApp_execution",
                instance, animations, systems, sprites, camera, target, background));
            Need(frames.Any(f => f.non_background_pixels > 0) &&
                frames.Last().animation_enabled.All(v => !v) && !frames.Last().build_3003_active,
                "Scaffold mask/animation/timer render did not produce evidence");
            return new Report { passed = true, project_path = Probe, run_directory = output,
                unity_version = Application.unityVersion, color_space = QualitySettings.activeColorSpace.ToString(),
                graphics_device = SystemInfo.graphicsDeviceType.ToString(),
                source_inventory_sha256 = InventoryHash,
                source_contract_sha256 = Hash(Path.Combine(Probe, "source_contract.json")),
                renderer_sha256 = contract.exporter_sha256,
                camera_evidence_sha256 = contract.camera_evidence_sha256,
                source_scope = "Original imported Build_shelf_5 prefab, source SpriteRenderers, per-sprite source mask GUID/fileID mapping, authored AnimationClip curves, city_default camera. Isolated mediator-equivalent transition only.",
                lifecycle_boundary = contract.lifecycle_limit,
                particle_limit = "Nested build_3003 seeded native particle observation is present, but built-in particle shader implementation and full GameApp particle lifetime are not validated.",
                source_camera_limit = "Source city_default camera and unscaled original prefab at origin; actual Farm/Wood transform_shelf parent and surrounding GameApp scene are not loaded.",
                camera = contract.camera, applied_sprite_color = new Vector4(contract.camera.day_sprite_color[0],
                    contract.camera.day_sprite_color[1], contract.camera.day_sprite_color[2], contract.camera.day_sprite_color[3]),
                applied_city_transparency = Shader.GetGlobalFloat("_CityTransparency"),
                mediator_assigned_local_rotation = assignedRotation, observed_root_local_rotation = observedRotation,
                background_control_png = backgroundName, background_control_sha256 = backgroundSha,
                source_files = contract.source_files, source_mask_bindings = contract.mask_bindings,
                source_nodes = nodeRecords, layers = layers.ToArray(),
                source_animation_components = animationRecords, clips = clips, frames = frames.ToArray(),
                game_app_loaded = false, server_event_replayed = false, actual_mask_script_loaded = false };
        }
        finally { UnityEngine.Object.DestroyImmediate(instance); }
    }
    public static void Run()
    {
        output = Path.Combine(Probe, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") +
            "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        if (Directory.Exists(output)) throw new InvalidOperationException("Scaffold output already exists: " + output);
        Directory.CreateDirectory(output);
        Camera camera = null; RenderTexture target = null;
        try
        {
            Need(Normal(Application.dataPath) == Probe + "/Assets" &&
                Application.unityVersion == "2019.4.28f1c1" &&
                QualitySettings.activeColorSpace == ColorSpace.Gamma,
                "Wrong isolated Unity 2019 Gamma project");
            var contractPath = Path.Combine(Probe, "source_contract.json");
            contract = JsonUtility.FromJson<Contract>(File.ReadAllText(contractPath, Encoding.UTF8));
            Need(contract != null && contract.schema == "rok.construction_scaffold_native_contract.v1" &&
                Normal(contract.repo_root) == Repo && Normal(contract.probe_root) == Probe &&
                contract.source_inventory_sha256 == InventoryHash &&
                contract.unity_version == Application.unityVersion &&
                contract.source_color_space == "Gamma" &&
                contract.mask_bindings != null && contract.mask_bindings.Length == 8,
                "Source scaffold contract identity differs");
            Need(Hash(Path.Combine(Repo, "Scripts/RenderRokConstructionScaffoldNative.cs")) == contract.exporter_sha256 &&
                Hash(Path.Combine(Probe, "Assets/Editor/RenderRokConstructionScaffoldNative.cs")) == contract.exporter_sha256 &&
                Hash(Path.Combine(Repo,
                    "Saved/RokUnityMigration/BatchReview20261009Construction/construction_effect_source_inventory.json")) == InventoryHash &&
                Hash(Path.Combine(Repo, contract.camera_evidence_path)) == contract.camera_evidence_sha256,
                "Pinned renderer/inventory/camera evidence differs");
            Need(contract.source_files != null && contract.source_files.Length == 54 &&
                contract.omitted_script_guid_refs != null && contract.omitted_script_guid_refs.Length == 4 &&
                contract.camera != null && contract.camera.source_row == "city_default" &&
                Mathf.Abs(contract.camera.vertical_fov - 7.5f) < 0.0001f &&
                contract.camera.width == 1280 && contract.camera.height == 720 &&
                contract.camera.day_sprite_color.Length == 4,
                "Source file/lighting/camera shape differs");
            foreach (var file in contract.source_files)
            {
                Need(Hash(Path.Combine(Repo, file.source_path)) == file.source_sha256 &&
                    Hash(Path.Combine(Repo, file.source_path + ".meta")) == file.meta_sha256,
                    "Source reference changed: " + file.source_path);
                if (!file.copied) continue;
                Need(file.probe_path.StartsWith(Prefix, StringComparison.Ordinal) &&
                    Hash(Path.Combine(Probe, file.probe_path)) == file.source_sha256 &&
                    Hash(Path.Combine(Probe, file.probe_path + ".meta")) == file.meta_sha256,
                    "Isolated source copy changed: " + file.probe_path);
                SourceFiles.Add(file.probe_path, file);
            }
            foreach (var script in contract.omitted_script_guid_refs)
                Unresolved.Add("Intentionally omitted source MonoBehaviour: " + script);
            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
            var cameraObject = new GameObject("SourceCityDefaultScaffoldCamera");
            camera = cameraObject.AddComponent<Camera>();
            camera.orthographic = false; camera.fieldOfView = contract.camera.vertical_fov;
            camera.aspect = (float)contract.camera.width / contract.camera.height;
            camera.nearClipPlane = 0.01f; camera.farClipPlane = 1000f;
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(0.5255f, 0.6314f, 0.3216f, 1f) *
                new Color(contract.camera.day_sprite_color[0], contract.camera.day_sprite_color[1],
                    contract.camera.day_sprite_color[2], 1f);
            camera.allowHDR = false; camera.allowMSAA = false;
            var forward = new Vector3(contract.camera.forward[0], contract.camera.forward[1],
                contract.camera.forward[2]).normalized;
            camera.transform.rotation = Quaternion.LookRotation(forward, Vector3.up);
            camera.transform.position = -forward * contract.camera.distance;
            target = new RenderTexture(contract.camera.width, contract.camera.height, 24,
                RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
            target.Create(); camera.targetTexture = target;
            Shader.SetGlobalColor("_SpriteColor", new Color(contract.camera.day_sprite_color[0],
                contract.camera.day_sprite_color[1], contract.camera.day_sprite_color[2],
                contract.camera.day_sprite_color[3]));
            Shader.SetGlobalFloat("_CityTransparency", contract.camera.city_transparency);
            report = Render(camera, target);
            Save();
            Debug.Log("ROK_CONSTRUCTION_SCAFFOLD_NATIVE_RENDERED " + output);
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            if (report != null) { report.passed = false; report.failures = new[] { error.ToString() }; Save(); }
            File.WriteAllText(Path.Combine(output, "failure.txt"), error.ToString(), new UTF8Encoding(false));
            Debug.LogError(error); EditorApplication.Exit(1);
        }
        finally
        {
            if (camera) { camera.targetTexture = null; UnityEngine.Object.DestroyImmediate(camera.gameObject); }
            if (target) { target.Release(); UnityEngine.Object.DestroyImmediate(target); }
        }
    }
}
