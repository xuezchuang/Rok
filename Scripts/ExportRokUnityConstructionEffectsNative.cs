using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;

// Isolated Unity 2019 editor probe. This does not start GameApp or its hotfix mediators.
public static class ExportRokUnityConstructionEffectsNative
{
    const string Repo = "D:/ueDemo/Rok";
    static readonly string ProbeName = Environment.GetEnvironmentVariable("ROK_CONSTRUCTION_EFFECT_PROBE_NAME") ?? "";
    static readonly string Probe = Repo + "/Saved/" + ProbeName;
    const string Prefix = "Assets/Source/";
    const string ExpectedInventoryHash = "01dd55e415bfc8f9284642f22e73f47e111a1b34ccaf126788e7357189844ffe";

    [Serializable] public class SourceFile
    {
        public string source_path, source_sha256, meta_sha256, probe_path;
        public bool copied;
    }
    [Serializable] public class Contract
    {
        public string schema, repo_root, probe_root, unity_version, source_color_space;
        public string source_project_settings_sha256, source_inventory_sha256, exporter_sha256, lifecycle_limit;
        public SourceFile[] source_files;
        public string[] omitted_script_guid_refs;
        public string[] allowed_builtin_guid_refs;
    }
    [Serializable] public class AssetRef
    {
        public string probe_path, source_path, guid, sha256, meta_sha256, type;
        public long local_file_id;
        public bool builtin_or_generated;
    }
    [Serializable] public class ComponentRecord
    {
        public string type, serialized_unity_json;
        public bool enabled;
    }
    [Serializable] public class NodeRecord
    {
        public string path, parent_path, name;
        public int sibling_index, layer, missing_mono_behaviours;
        public bool active_self, active_in_hierarchy;
        public Vector3 local_position, local_scale;
        public Quaternion local_rotation;
        public Matrix4x4 local_to_world;
        public AssetRef prefab_source;
        public ComponentRecord[] components;
    }
    [Serializable] public class CurveKey
    {
        public float time, value, in_tangent, out_tangent, in_weight, out_weight;
        public string weighted_mode;
    }
    [Serializable] public class CurveRecord
    {
        public string path, type, property, pre_wrap, post_wrap;
        public CurveKey[] keys;
    }
    [Serializable] public class ObjectCurveKey
    {
        public float time;
        public AssetRef value;
    }
    [Serializable] public class ObjectCurveRecord
    {
        public string path, type, property;
        public ObjectCurveKey[] keys;
    }
    [Serializable] public class AnimationEventRecord
    {
        public float time, float_parameter;
        public int int_parameter;
        public string function_name, string_parameter, message_options;
        public AssetRef object_parameter;
    }
    [Serializable] public class ClipRecord
    {
        public AssetRef asset;
        public string name, wrap_mode;
        public bool legacy, loop_time;
        public float frame_rate, length, start_time, stop_time;
        public CurveRecord[] curves;
        public ObjectCurveRecord[] object_curves;
        public AnimationEventRecord[] events;
    }
    [Serializable] public class AnimationRecord
    {
        public string node, default_clip, wrap_mode;
        public bool enabled, play_automatically;
        public string[] clip_names;
        public string serialized_unity_json;
    }
    [Serializable] public class ParticleRecord
    {
        public string node, serialized_unity_json, renderer_serialized_unity_json;
        public bool active_in_hierarchy, playing_at_prefab_load, looping, prewarm, auto_random_seed;
        public uint authored_random_seed;
        public float duration, start_delay_max, start_lifetime_max, derived_one_cycle_end;
        public string simulation_space, scaling_mode, stop_action, culling_mode, render_mode;
        public AssetRef[] materials;
    }
    [Serializable] public class MaterialPropertyRecord
    {
        public string name, kind;
        public float number;
        public Color color;
        public Vector4 vector;
        public AssetRef texture;
        public Vector2 texture_scale, texture_offset;
    }
    [Serializable] public class MaterialRecord
    {
        public AssetRef asset, shader;
        public string shader_name, keywords, serialized_unity_json, authored_shader_guid;
        public int render_queue;
        public bool enable_instancing, shader_runtime_resolved, shader_source_available;
        public MaterialPropertyRecord[] properties;
    }
    [Serializable] public class TextureRecord
    {
        public AssetRef asset;
        public int width, height, importer_max_size, standalone_max_size;
        public bool srgb, alpha_is_transparency, mipmaps;
        public string filter, wrap, compression, standalone_format, serialized_importer_json;
    }
    [Serializable] public class SpriteRecord
    {
        public string node;
        public AssetRef sprite, texture;
        public Rect rect, texture_rect;
        public Vector2 pivot;
        public float pixels_per_unit;
        public Bounds bounds;
        public bool flip_x, flip_y;
        public Color color;
        public int sorting_layer_id, sorting_order;
        public Vector2[] vertices, uv;
        public ushort[] triangles;
    }
    [Serializable] public class MeshRecord
    {
        public AssetRef asset;
        public string node, name;
        public Bounds bounds;
        public Vector3[] vertices, normals;
        public Vector4[] tangents;
        public Color32[] colors;
        public UvChannel[] uv_channels;
        public SubmeshRecord[] submeshes;
    }
    [Serializable] public class UvChannel { public int channel; public Vector4[] values; }
    [Serializable] public class SubmeshRecord { public int index; public int[] indices; public string topology; }
    [Serializable] public class LiveParticle
    {
        public Vector3 position, velocity;
        public float remaining_lifetime, start_lifetime, start_size;
        public Color32 start_color;
        public uint random_seed;
    }
    [Serializable] public class ParticleSample
    {
        public string node;
        public uint simulation_seed;
        public bool substituted_auto_seed, stopped_before_seed, seed_verified_before_simulate;
        public bool active_at_sample, bake_succeeded;
        public int count;
        public LiveParticle[] particles;
        public MeshRecord baked_mesh;
        public string bake_error;
    }
    [Serializable] public class TimeSample
    {
        public float time;
        public string diagnostic_png, diagnostic_png_sha256;
        public ParticleSample[] particles;
    }
    [Serializable] public class PrefabRecord
    {
        public AssetRef asset;
        public string name, sample_policy, lifecycle_limit;
        public float source_derived_horizon, observed_step;
        public bool finite_particle_end_proven;
        public NodeRecord[] nodes;
        public AnimationRecord[] animations;
        public ParticleRecord[] particle_systems;
        public SpriteRecord[] sprites;
        public MeshRecord[] meshes;
        public TimeSample[] samples;
    }
    [Serializable] public class Report
    {
        public string schema = "rok.construction_effect_native_inventory.v1";
        public string project_path, unity_version, color_space, graphics_device, run_directory;
        public string source_inventory_sha256, source_contract_sha256, exporter_sha256;
        public string scope, diagnostic_camera, runtime_lifecycle_limit;
        public bool passed, game_app_loaded, server_events_replayed, sample_is_authored_curve;
        public SourceFile[] source_files;
        public string[] allowed_builtin_guid_refs;
        public PrefabRecord[] prefabs;
        public ClipRecord[] animation_clips;
        public MaterialRecord[] materials;
        public TextureRecord[] textures;
        public string[] unresolved_references, failures;
    }

    static Contract contract;
    static Report report;
    static string output;
    static readonly Dictionary<string, SourceFile> copied = new Dictionary<string, SourceFile>(StringComparer.OrdinalIgnoreCase);
    static readonly Dictionary<string, ClipRecord> clips = new Dictionary<string, ClipRecord>(StringComparer.Ordinal);
    static readonly Dictionary<string, MaterialRecord> materials = new Dictionary<string, MaterialRecord>(StringComparer.Ordinal);
    static readonly Dictionary<string, TextureRecord> textures = new Dictionary<string, TextureRecord>(StringComparer.Ordinal);
    static readonly HashSet<string> unresolved = new HashSet<string>(StringComparer.Ordinal);

    static string Normal(string value) { return Path.GetFullPath(value).Replace('\\', '/').TrimEnd('/'); }
    static string Hash(string path)
    {
        using (var h = SHA256.Create())
            return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant();
    }
    static void Require(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
    static string SourcePath(string probePath)
    {
        SourceFile item;
        return copied.TryGetValue(probePath, out item) ? item.source_path : "";
    }
    static AssetRef Reference(UnityEngine.Object obj)
    {
        if (!obj) return null;
        var path = AssetDatabase.GetAssetPath(obj);
        var result = new AssetRef { probe_path = path, source_path = SourcePath(path), type = obj.GetType().FullName };
        string guid;
        long localId;
        if (AssetDatabase.TryGetGUIDAndLocalFileIdentifier(obj, out guid, out localId))
        { result.guid = guid; result.local_file_id = localId; }
        result.builtin_or_generated = string.IsNullOrEmpty(path) || !path.StartsWith(Prefix, StringComparison.Ordinal);
        if (!string.IsNullOrEmpty(result.source_path))
        {
            var item = copied[path];
            result.sha256 = item.source_sha256;
            result.meta_sha256 = item.meta_sha256;
        }
        else if (!result.builtin_or_generated)
            unresolved.Add("Asset outside pinned source list: " + path);
        return result;
    }
    static string NodePath(Transform node, Transform root)
    {
        var names = new List<string>();
        for (var current = node; current; current = current.parent)
        {
            names.Add(current.name + "[" + current.GetSiblingIndex() + "]");
            if (current == root) break;
        }
        names.Reverse();
        return string.Join("/", names.ToArray());
    }
    static IEnumerable<Transform> Tree(Transform root)
    {
        yield return root;
        foreach (Transform child in root)
            foreach (var node in Tree(child)) yield return node;
    }
    static NodeRecord ReadNode(Transform node, Transform root)
    {
        var source = PrefabUtility.GetCorrespondingObjectFromSource(node.gameObject);
        var components = new List<ComponentRecord>();
        foreach (var component in node.GetComponents<Component>())
        {
            if (!component) continue;
            var behaviour = component as Behaviour;
            components.Add(new ComponentRecord {
                type = component.GetType().FullName,
                enabled = behaviour == null || behaviour.enabled,
                serialized_unity_json = EditorJsonUtility.ToJson(component, true)
            });
        }
        int missing = GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(node.gameObject);
        if (missing > 0) unresolved.Add("Missing MonoBehaviour on " + NodePath(node, root) + ": " + missing);
        return new NodeRecord {
            path = NodePath(node, root),
            parent_path = node == root ? "" : NodePath(node.parent, root),
            name = node.name,
            sibling_index = node.GetSiblingIndex(), layer = node.gameObject.layer,
            active_self = node.gameObject.activeSelf,
            active_in_hierarchy = node.gameObject.activeInHierarchy,
            missing_mono_behaviours = missing,
            local_position = node.localPosition, local_rotation = node.localRotation,
            local_scale = node.localScale, local_to_world = node.localToWorldMatrix,
            prefab_source = Reference(source), components = components.ToArray()
        };
    }
    static CurveKey[] Keys(AnimationCurve curve)
    {
        return curve.keys.Select(k => new CurveKey { time = k.time, value = k.value,
            in_tangent = k.inTangent, out_tangent = k.outTangent,
            in_weight = k.inWeight, out_weight = k.outWeight,
            weighted_mode = k.weightedMode.ToString() }).ToArray();
    }
    static void AddClip(AnimationClip clip)
    {
        if (!clip) return;
        var asset = Reference(clip);
        var key = asset != null ? asset.guid + ":" + asset.local_file_id : clip.name;
        if (clips.ContainsKey(key)) return;
        var curves = new List<CurveRecord>();
        foreach (var binding in AnimationUtility.GetCurveBindings(clip))
        {
            var curve = AnimationUtility.GetEditorCurve(clip, binding);
            curves.Add(new CurveRecord { path = binding.path, type = binding.type.FullName,
                property = binding.propertyName, pre_wrap = curve.preWrapMode.ToString(),
                post_wrap = curve.postWrapMode.ToString(), keys = Keys(curve) });
        }
        var objectCurves = new List<ObjectCurveRecord>();
        foreach (var binding in AnimationUtility.GetObjectReferenceCurveBindings(clip))
            objectCurves.Add(new ObjectCurveRecord { path = binding.path,
                type = binding.type.FullName, property = binding.propertyName,
                keys = AnimationUtility.GetObjectReferenceCurve(clip, binding)
                    .Select(k => new ObjectCurveKey { time = k.time, value = Reference(k.value) }).ToArray() });
        var events = AnimationUtility.GetAnimationEvents(clip).Select(e => new AnimationEventRecord {
            time = e.time, function_name = e.functionName, float_parameter = e.floatParameter,
            int_parameter = e.intParameter, string_parameter = e.stringParameter,
            object_parameter = Reference(e.objectReferenceParameter),
            message_options = e.messageOptions.ToString()
        }).ToArray();
        clips.Add(key, new ClipRecord { asset = asset, name = clip.name, legacy = clip.legacy,
            frame_rate = clip.frameRate, length = clip.length, wrap_mode = clip.wrapMode.ToString(),
            start_time = 0, stop_time = clip.length,
            loop_time = AnimationUtility.GetAnimationClipSettings(clip).loopTime,
            curves = curves.ToArray(), object_curves = objectCurves.ToArray(), events = events });
    }
    static float Maximum(ParticleSystem.MinMaxCurve curve)
    {
        var values = new List<float> { curve.constantMin, curve.constantMax };
        foreach (var animation in new[] { curve.curveMin, curve.curveMax })
            if (animation != null) foreach (var key in animation.keys)
                values.Add(key.value * curve.curveMultiplier);
        return values.Where(v => !float.IsNaN(v) && !float.IsInfinity(v)).DefaultIfEmpty(0).Max();
    }
    static ParticleRecord ReadParticle(ParticleSystem system, Transform root)
    {
        var main = system.main;
        var renderer = system.GetComponent<ParticleSystemRenderer>();
        var delay = Mathf.Max(0, Maximum(main.startDelay));
        var lifetime = Mathf.Max(0, Maximum(main.startLifetime));
        return new ParticleRecord {
            node = NodePath(system.transform, root), active_in_hierarchy = system.gameObject.activeInHierarchy,
            playing_at_prefab_load = system.isPlaying, looping = main.loop, prewarm = main.prewarm,
            auto_random_seed = system.useAutoRandomSeed, authored_random_seed = system.randomSeed,
            duration = main.duration, start_delay_max = delay, start_lifetime_max = lifetime,
            derived_one_cycle_end = delay + Mathf.Max(0, main.duration) + lifetime,
            simulation_space = main.simulationSpace.ToString(), scaling_mode = main.scalingMode.ToString(),
            stop_action = main.stopAction.ToString(), culling_mode = main.cullingMode.ToString(),
            render_mode = renderer ? renderer.renderMode.ToString() : "missing",
            serialized_unity_json = EditorJsonUtility.ToJson(system, true),
            renderer_serialized_unity_json = renderer ? EditorJsonUtility.ToJson(renderer, true) : "",
            materials = renderer ? renderer.sharedMaterials.Select(Reference).ToArray() : new AssetRef[0]
        };
    }
    static void AddTexture(Texture texture)
    {
        if (!texture) return;
        var asset = Reference(texture);
        if (asset == null || string.IsNullOrEmpty(asset.probe_path) || textures.ContainsKey(asset.probe_path)) return;
        var importer = AssetImporter.GetAtPath(asset.probe_path) as TextureImporter;
        TextureImporterPlatformSettings platform = importer != null
            ? importer.GetPlatformTextureSettings("Standalone") : null;
        textures.Add(asset.probe_path, new TextureRecord { asset = asset,
            width = texture.width, height = texture.height,
            importer_max_size = importer != null ? importer.maxTextureSize : 0,
            standalone_max_size = platform != null ? platform.maxTextureSize : 0,
            srgb = importer != null && importer.sRGBTexture,
            alpha_is_transparency = importer != null && importer.alphaIsTransparency,
            mipmaps = importer != null && importer.mipmapEnabled,
            filter = texture.filterMode.ToString(), wrap = texture.wrapMode.ToString(),
            compression = importer != null ? importer.textureCompression.ToString() : "",
            standalone_format = platform != null ? platform.format.ToString() : "",
            serialized_importer_json = importer != null ? EditorJsonUtility.ToJson(importer, true) : ""
        });
    }
    static void AddMaterial(Material material)
    {
        if (!material) return;
        var asset = Reference(material);
        var key = asset != null ? asset.probe_path : material.name;
        if (materials.ContainsKey(key)) return;
        var shader = material.shader;
        var properties = new List<MaterialPropertyRecord>();
        if (shader)
        {
            for (int i = 0; i < ShaderUtil.GetPropertyCount(shader); i++)
            {
                var name = ShaderUtil.GetPropertyName(shader, i);
                var kind = ShaderUtil.GetPropertyType(shader, i);
                var property = new MaterialPropertyRecord { name = name, kind = kind.ToString() };
                if (kind == ShaderUtil.ShaderPropertyType.Color) property.color = material.GetColor(name);
                else if (kind == ShaderUtil.ShaderPropertyType.Vector) property.vector = material.GetVector(name);
                else if (kind == ShaderUtil.ShaderPropertyType.Float ||
                         kind == ShaderUtil.ShaderPropertyType.Range) property.number = material.GetFloat(name);
                else if (kind == ShaderUtil.ShaderPropertyType.TexEnv)
                {
                    var texture = material.GetTexture(name);
                    property.texture = Reference(texture);
                    property.texture_scale = material.GetTextureScale(name);
                    property.texture_offset = material.GetTextureOffset(name);
                    AddTexture(texture);
                }
                properties.Add(property);
            }
        }
        if (!shader) unresolved.Add("Material shader missing: " + key);
        var shaderRef = Reference(shader);
        var authoredShaderGuid = "";
        if (asset != null && !string.IsNullOrEmpty(asset.source_path))
        {
            var yaml = File.ReadAllText(Path.Combine(Repo, asset.source_path), Encoding.UTF8);
            var shaderMatch = Regex.Match(yaml, @"m_Shader:\s*\{[^\r\n]*guid:\s*([0-9a-f]{32})");
            if (shaderMatch.Success) authoredShaderGuid = shaderMatch.Groups[1].Value;
        }
        materials.Add(key, new MaterialRecord { asset = asset, shader = shaderRef,
            shader_name = shader ? shader.name : "missing", render_queue = material.renderQueue,
            keywords = string.Join(";", material.shaderKeywords),
            enable_instancing = material.enableInstancing,
            shader_runtime_resolved = shader,
            shader_source_available = shaderRef != null && !string.IsNullOrEmpty(shaderRef.source_path),
            authored_shader_guid = authoredShaderGuid,
            serialized_unity_json = EditorJsonUtility.ToJson(material, true),
            properties = properties.ToArray() });
    }
    static MeshRecord ReadMesh(Mesh mesh, string node)
    {
        if (!mesh) return null;
        var uv = new UvChannel[8];
        for (int channel = 0; channel < 8; channel++)
        {
            var list = new List<Vector4>();
            mesh.GetUVs(channel, list);
            uv[channel] = new UvChannel { channel = channel, values = list.ToArray() };
        }
        var submeshes = new SubmeshRecord[mesh.subMeshCount];
        for (int i = 0; i < mesh.subMeshCount; i++)
            submeshes[i] = new SubmeshRecord { index = i, indices = mesh.GetIndices(i),
                topology = mesh.GetTopology(i).ToString() };
        return new MeshRecord { asset = Reference(mesh), node = node, name = mesh.name,
            bounds = mesh.bounds, vertices = mesh.vertices, normals = mesh.normals,
            tangents = mesh.tangents, colors = mesh.colors32, uv_channels = uv,
            submeshes = submeshes };
    }
    static uint Seed(string value)
    {
        uint hash = 2166136261;
        foreach (var ch in value) { hash ^= ch; hash *= 16777619; }
        return hash == 0 ? 1u : hash;
    }
    static ParticleSample SampleParticle(ParticleSystem system, Transform root, Camera camera)
    {
        var path = NodePath(system.transform, root);
        var particles = new ParticleSystem.Particle[system.particleCount];
        int count = system.GetParticles(particles);
        var renderer = system.GetComponent<ParticleSystemRenderer>();
        var sample = new ParticleSample { node = path, count = count,
            simulation_seed = system.randomSeed,
            substituted_auto_seed = false,
            active_at_sample = system.gameObject.activeInHierarchy,
            particles = particles.Take(count).Select(p => new LiveParticle {
                position = p.position, velocity = p.velocity,
                remaining_lifetime = p.remainingLifetime, start_lifetime = p.startLifetime,
                start_size = p.startSize, start_color = p.startColor, random_seed = p.randomSeed
            }).ToArray() };
        if (renderer && system.gameObject.activeInHierarchy)
        {
            var baked = new Mesh();
            try
            {
                renderer.BakeMesh(baked, camera, true);
                sample.baked_mesh = ReadMesh(baked, path);
                sample.bake_succeeded = true;
            }
            catch (Exception error) { sample.bake_error = error.GetType().Name + ": " + error.Message; }
            finally { UnityEngine.Object.DestroyImmediate(baked); }
        }
        return sample;
    }
    static string DiagnosticPng(Camera camera, RenderTexture target, string name)
    {
        camera.Render();
        var previous = RenderTexture.active;
        RenderTexture.active = target;
        var image = new Texture2D(target.width, target.height, TextureFormat.RGB24, false);
        try
        {
            image.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0);
            image.Apply();
            var path = Path.Combine(output, name);
            File.WriteAllBytes(path, image.EncodeToPNG());
            return name;
        }
        finally { RenderTexture.active = previous; UnityEngine.Object.DestroyImmediate(image); }
    }
    static PrefabRecord CapturePrefab(string path, Camera camera, RenderTexture target)
    {
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(path);
        Require(prefab, "Required prefab failed to import: " + path);
        var instance = UnityEngine.Object.Instantiate(prefab);
        instance.name = prefab.name;
        try
        {
            var root = instance.transform;
            var nodes = Tree(root).ToArray();
            var animations = nodes.SelectMany(n => n.GetComponents<Animation>()).ToArray();
            var systems = nodes.SelectMany(n => n.GetComponents<ParticleSystem>()).ToArray();
            var renderers = nodes.SelectMany(n => n.GetComponents<Renderer>()).ToArray();
            // Record the imported prefab state before any deterministic probe sampling.
            var initialNodes = nodes.Select(n => ReadNode(n, root)).ToArray();
            foreach (var renderer in renderers)
                foreach (var material in renderer.sharedMaterials) AddMaterial(material);
            var animationRecords = new List<AnimationRecord>();
            foreach (var animation in animations)
            {
                var stateClips = new List<AnimationClip>();
                foreach (AnimationState state in animation) if (state.clip) stateClips.Add(state.clip);
                if (animation.clip && !stateClips.Contains(animation.clip)) stateClips.Add(animation.clip);
                foreach (var clip in stateClips) AddClip(clip);
                animationRecords.Add(new AnimationRecord { node = NodePath(animation.transform, root),
                    enabled = animation.enabled, play_automatically = animation.playAutomatically,
                    default_clip = animation.clip ? animation.clip.name : "",
                    wrap_mode = animation.wrapMode.ToString(),
                    clip_names = stateClips.Select(c => c.name).ToArray(),
                    serialized_unity_json = EditorJsonUtility.ToJson(animation, true) });
                animation.enabled = false; // Source state is already captured; SampleAnimation is explicit.
            }
            var sprites = new List<SpriteRecord>();
            foreach (var spriteRenderer in nodes.SelectMany(n => n.GetComponents<SpriteRenderer>()))
            {
                var sprite = spriteRenderer.sprite;
                if (!sprite) continue;
                AddTexture(sprite.texture);
                sprites.Add(new SpriteRecord { node = NodePath(spriteRenderer.transform, root),
                    sprite = Reference(sprite), texture = Reference(sprite.texture),
                    rect = sprite.rect, texture_rect = sprite.textureRect, pivot = sprite.pivot,
                    pixels_per_unit = sprite.pixelsPerUnit, bounds = sprite.bounds,
                    flip_x = spriteRenderer.flipX, flip_y = spriteRenderer.flipY,
                    color = spriteRenderer.color, sorting_layer_id = spriteRenderer.sortingLayerID,
                    sorting_order = spriteRenderer.sortingOrder,
                    vertices = sprite.vertices, uv = sprite.uv, triangles = sprite.triangles });
            }
            var meshRecords = nodes.SelectMany(n => n.GetComponents<MeshFilter>())
                .Where(f => f.sharedMesh).Select(f => ReadMesh(f.sharedMesh, NodePath(f.transform, root))).ToArray();
            var particleRecords = systems.Select(s => ReadParticle(s, root)).ToArray();
            Require(prefab.name != "Build_shelf_5" || animations.Length > 0,
                "Scaffold source Animation components did not import");
            Require(prefab.name == "Build_shelf_5" || systems.Length > 0,
                "Effect source ParticleSystem components did not import: " + prefab.name);
            float horizon = Mathf.Max(0, particleRecords.Select(p => p.derived_one_cycle_end).DefaultIfEmpty(0).Max());
            horizon = Mathf.Max(horizon, animationRecords.Select(a => a.clip_names)
                .SelectMany(names => names).Select(name => clips.Values.Where(c => c.name == name)
                    .Select(c => c.length).DefaultIfEmpty(0).Max()).DefaultIfEmpty(0).Max());
            // The 2-second hide/destroy behavior belongs to a missing GameApp mediator.
            // It is not replayed as if it were a native ParticleSystem stop time.
            Require(horizon > 0 && horizon < 120, "Source-derived sampling horizon invalid: " + path + " / " + horizon);
            float clipRate = clips.Values.Select(c => c.frame_rate).Where(v => v > 0).DefaultIfEmpty(0).Max();
            int intervals = Mathf.Clamp(clipRate > 0 ? Mathf.CeilToInt(horizon * clipRate) : 16, 8, 120);
            float step = horizon / intervals;
            var sampled = new List<TimeSample>();
            bool capturedActiveDiagnostic = false;
            var systemSources = systems.ToDictionary(s => s, s => new {
                auto = s.useAutoRandomSeed, seed = s.randomSeed
            });
            for (int i = 0; i <= intervals; i++)
            {
                float time = i == intervals ? horizon : i * step;
                foreach (var animation in animations)
                    if (animation.clip) animation.clip.SampleAnimation(animation.gameObject,
                        Mathf.Min(time, animation.clip.length));
                // Instantiated source prefabs can already be playing via playOnAwake. Unity
                // rejects randomSeed writes until StopEmittingAndClear has completed.
                foreach (var system in systems)
                    system.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
                var stoppedBeforeSeed = systems.ToDictionary(s => s, s => s.isStopped);
                Require(stoppedBeforeSeed.Values.All(v => v),
                    "ParticleSystem did not stop before deterministic seeding: " + path);
                var seedVerified = new Dictionary<ParticleSystem, bool>();
                foreach (var system in systems)
                {
                    var source = systemSources[system];
                    uint expectedSeed = source.auto ? Seed(path + ":" + NodePath(system.transform, root)) : source.seed;
                    system.useAutoRandomSeed = false;
                    system.randomSeed = expectedSeed;
                    seedVerified[system] = !system.useAutoRandomSeed && system.randomSeed == expectedSeed;
                }
                Require(seedVerified.Values.All(v => v),
                    "ParticleSystem seed write was not applied before Simulate: " + path);
                foreach (var system in systems)
                    if (!system.GetComponentsInParent<ParticleSystem>(true).Any(parent => parent != system))
                        system.Simulate(time, true, true, false);
                var samples = systems.Select(s => SampleParticle(s, root, camera)).ToArray();
                for (int s = 0; s < systems.Length; s++)
                {
                    samples[s].substituted_auto_seed = systemSources[systems[s]].auto;
                    samples[s].stopped_before_seed = stoppedBeforeSeed[systems[s]];
                    samples[s].seed_verified_before_simulate = seedVerified[systems[s]];
                }
                string png = "";
                bool hasLiveParticles = samples.Any(s => s.count > 0);
                if (i == 0 || i == intervals / 2 || i == intervals ||
                    (hasLiveParticles && !capturedActiveDiagnostic))
                    png = DiagnosticPng(camera, target, prefab.name + "_diagnostic_" + i.ToString("D3") + ".png");
                if (hasLiveParticles && png.Length > 0) capturedActiveDiagnostic = true;
                sampled.Add(new TimeSample { time = time, diagnostic_png = png,
                    diagnostic_png_sha256 = png.Length > 0 ? Hash(Path.Combine(output, png)) : "",
                    particles = samples });
            }
            return new PrefabRecord { asset = Reference(prefab), name = prefab.name,
                sample_policy = "Authored AnimationClip frameRate when present; otherwise 16 equal intervals over source-derived one-cycle particle horizon. Sample cap 120 intervals is a probe resource limit, not a source lifetime.",
                lifecycle_limit = "No server Build_BuildingInfo, CityBuildingStart/LevelUP, BuildShelf 2-second MonoBehaviour disable, or AutoDespawn callback is replayed.",
                source_derived_horizon = horizon, observed_step = step,
                finite_particle_end_proven = systems.All(s => !s.main.loop && !s.subEmitters.enabled),
                nodes = initialNodes,
                animations = animationRecords.ToArray(), particle_systems = particleRecords,
                sprites = sprites.ToArray(), meshes = meshRecords, samples = sampled.ToArray() };
        }
        finally { UnityEngine.Object.DestroyImmediate(instance); }
    }
    static void SaveReport()
    {
        report.animation_clips = clips.Values.OrderBy(c => c.name).ToArray();
        report.materials = materials.Values.OrderBy(m => m.asset != null ? m.asset.probe_path : "").ToArray();
        report.textures = textures.Values.OrderBy(t => t.asset.probe_path).ToArray();
        report.unresolved_references = unresolved.OrderBy(s => s).ToArray();
        File.WriteAllText(Path.Combine(output, "native_effect_inventory.json"),
            JsonUtility.ToJson(report, true), new UTF8Encoding(false));
    }
    public static void Run()
    {
        if (!Regex.IsMatch(ProbeName, @"^UnityCityConstructionEffectsProbeV[2-9][0-9]*$"))
            throw new InvalidOperationException("Invalid isolated construction-effects probe name");
        output = Path.Combine(Probe, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff")
            + "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        if (Directory.Exists(output)) throw new InvalidOperationException("Native output run already exists: " + output);
        Directory.CreateDirectory(output);
        Camera camera = null;
        RenderTexture target = null;
        try
        {
            Require(Normal(Application.dataPath) == Probe + "/Assets", "Wrong isolated Unity project");
            Require(Application.unityVersion == "2019.4.28f1c1", "Wrong Unity editor version");
            Require(QualitySettings.activeColorSpace == ColorSpace.Gamma, "Source project requires Gamma");
            var contractPath = Path.Combine(Probe, "source_contract.json");
            contract = JsonUtility.FromJson<Contract>(File.ReadAllText(contractPath, Encoding.UTF8));
            Require(contract != null && contract.schema == "rok.construction_effect_native_probe_contract.v1",
                "Probe contract schema differs");
            Require(contract.source_inventory_sha256 == ExpectedInventoryHash &&
                contract.unity_version == Application.unityVersion && contract.source_color_space == "Gamma" &&
                Normal(contract.repo_root) == Repo && Normal(contract.probe_root) == Probe,
                "Probe identity/source contract differs");
            Require(Hash(Path.Combine(Repo, "Scripts/ExportRokUnityConstructionEffectsNative.cs"))
                == contract.exporter_sha256 &&
                Hash(Path.Combine(Probe, "Assets/Editor/ExportRokUnityConstructionEffectsNative.cs"))
                == contract.exporter_sha256, "Native exporter hash differs");
            Require(Hash(Path.Combine(Repo, "Saved/RokUnityMigration/BatchReview20261009Construction/construction_effect_source_inventory.json"))
                == ExpectedInventoryHash, "Sealed source inventory changed");
            Require(contract.source_files != null && contract.source_files.Length >= 40,
                "Source asset list incomplete");
            Require(contract.omitted_script_guid_refs != null &&
                contract.omitted_script_guid_refs.Length > 0,
                "Missing-script GUID inventory was not captured");
            Require(contract.allowed_builtin_guid_refs != null &&
                contract.allowed_builtin_guid_refs.Length == 1 &&
                contract.allowed_builtin_guid_refs[0] == "0000000000000000f000000000000000",
                "Unity built-in GUID allowlist differs");
            foreach (var missingScript in contract.omitted_script_guid_refs)
                unresolved.Add("Intentionally omitted hotfix/client MonoBehaviour: " + missingScript);
            foreach (var file in contract.source_files)
            {
                Require(Hash(Path.Combine(Repo, file.source_path)) == file.source_sha256 &&
                    Hash(Path.Combine(Repo, file.source_path + ".meta")) == file.meta_sha256,
                    "Reference source changed: " + file.source_path);
                if (!file.copied) continue;
                Require(file.probe_path.StartsWith(Prefix, StringComparison.Ordinal) &&
                    Hash(Path.Combine(Probe, file.probe_path)) == file.source_sha256 &&
                    Hash(Path.Combine(Probe, file.probe_path + ".meta")) == file.meta_sha256,
                    "Isolated copied asset changed: " + file.probe_path);
                copied.Add(file.probe_path, file);
            }
            report = new Report { project_path = Probe, unity_version = Application.unityVersion,
                color_space = QualitySettings.activeColorSpace.ToString(),
                graphics_device = SystemInfo.graphicsDeviceType.ToString(), run_directory = output,
                source_inventory_sha256 = ExpectedInventoryHash,
                source_contract_sha256 = Hash(contractPath), exporter_sha256 = contract.exporter_sha256,
                source_files = contract.source_files,
                allowed_builtin_guid_refs = contract.allowed_builtin_guid_refs,
                scope = "Native isolated source hierarchy/curves/particle-asset and deterministic simulation observation; no UE visual acceptance.",
                diagnostic_camera = "Orthographic camera at (0,0,-10), size 5, 640x360. PNGs and billboard BakeMesh use this probe camera; it is not the source GameApp camera.",
                runtime_lifecycle_limit = contract.lifecycle_limit,
                game_app_loaded = false, server_events_replayed = false, sample_is_authored_curve = false };
            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
            foreach (var file in contract.source_files.Where(f => f.copied && f.probe_path.EndsWith(".anim", StringComparison.OrdinalIgnoreCase)))
                AddClip(AssetDatabase.LoadAssetAtPath<AnimationClip>(file.probe_path));
            var cameraObject = new GameObject("Construction effect diagnostic camera");
            camera = cameraObject.AddComponent<Camera>();
            camera.transform.position = new Vector3(0, 0, -10);
            camera.orthographic = true; camera.orthographicSize = 5;
            camera.nearClipPlane = 0.01f; camera.farClipPlane = 100;
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(0.04f, 0.04f, 0.04f, 1);
            camera.allowHDR = false; camera.allowMSAA = false;
            target = new RenderTexture(640, 360, 24, RenderTextureFormat.ARGB32);
            camera.targetTexture = target;
            var paths = new[] {
                Prefix + "BundleAssets/Scene/City_building/city_building_1/Build_shelf_5.prefab",
                Prefix + "BundleAssets/Effect/Prefabs/build_3003.prefab",
                Prefix + "BundleAssets/Effect/Prefabs/build_3004.prefab"
            };
            report.prefabs = paths.Select(p => CapturePrefab(p, camera, target)).ToArray();
            report.passed = report.prefabs.Length == 3
                && report.prefabs.All(p => p.nodes.Length > 0 && p.samples.Length > 1);
            SaveReport();
            Debug.Log("ROK_CONSTRUCTION_EFFECT_NATIVE_EXPORTED " + output);
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            if (report != null)
            {
                report.passed = false;
                report.failures = new[] { error.ToString() };
                SaveReport();
            }
            File.WriteAllText(Path.Combine(output, "failure.txt"), error.ToString(), new UTF8Encoding(false));
            Debug.LogError(error);
            EditorApplication.Exit(1);
        }
        finally
        {
            if (camera) { camera.targetTexture = null; UnityEngine.Object.DestroyImmediate(camera.gameObject); }
            if (target) { target.Release(); UnityEngine.Object.DestroyImmediate(target); }
        }
    }
}
