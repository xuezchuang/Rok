// Isolated Unity 2019.4.28f1c1 Editor witness. This file is copied into Saved/UnityWorldTileParticleProbeV2.
using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEngine;

public static class UnityRokWorldTileParticleOracleV2
{
    [Serializable] private class Contract
    {
        public string schema;
        public string prefab_sha256;
        public string material_sha256;
        public string texture_sha256;
        public string source_manifest_sha256;
        public string prefab_guid;
        public string material_guid;
        public string texture_guid;
        public string exporter_sha256;
        public string prepare_sha256;
        public string verifier_sha256;
    }

    [Serializable] private class Curve
    {
        public string mode;
        public float constant_min;
        public float constant_max;
        public float multiplier;
        public Key[] max_keys;
        public Key[] min_keys;
    }
    [Serializable] private class Key { public float time; public float value; public float in_tangent; public float out_tangent; }
    [Serializable] private class ColorKey { public float time; public Color color; }
    [Serializable] private class AlphaKey { public float time; public float alpha; }
    [Serializable] private class GradientRecord
    {
        public string mode;
        public Color color_min;
        public Color color_max;
        public ColorKey[] color_keys;
        public AlphaKey[] alpha_keys;
    }
    [Serializable] private class ParticleRecord
    {
        public Vector3 position;
        public Vector3 velocity;
        public float remaining_lifetime;
        public float start_lifetime;
        public float start_size;
        public float current_size;
        public float rotation;
        public uint random_seed;
        public Color32 start_color;
    }
    [Serializable] private class ParticleRun
    {
        public int particle_count;
        public ParticleRecord[] particles;
        public Vector3 position_min;
        public Vector3 position_max;
        public float size_min;
        public float size_max;
        public float remaining_lifetime_min;
        public float remaining_lifetime_max;
        public string simulation_space;
        public bool is_playing;
    }
    [Serializable] private class Sample
    {
        public float time_seconds;
        public ParticleRun[] independent_resets;
    }
    [Serializable] private class SystemRecord
    {
        public string name;
        public string hierarchy_path;
        public bool active_in_hierarchy;
        public Vector3 local_position;
        public Vector3 local_scale;
        public Quaternion local_rotation;
        public Vector3 world_position;
        public Quaternion world_rotation;
        public Vector3 world_lossy_scale;
        public TransformRecord[] ancestor_chain_child_to_root;
        public float duration;
        public bool loop;
        public bool play_on_awake;
        public bool prewarm;
        public string simulation_space;
        public string scaling_mode;
        public Curve start_lifetime;
        public Curve start_size;
        public Curve start_speed;
        public Curve emission_rate_over_time;
        public Curve size_over_lifetime;
        public GradientRecord color_over_lifetime;
        public string shape_type;
        public float shape_radius;
        public float shape_arc;
        public string render_mode;
        public string render_alignment;
        public string sort_mode;
        public int sorting_layer_id;
        public int sorting_order;
        public string mesh_name;
        public string mesh_asset_path;
        public string mesh_guid;
        public long mesh_local_file_id;
        public int mesh_vertex_count;
        public int mesh_triangle_index_count;
        public Bounds mesh_bounds;
        public string[] material_asset_paths;
        public Sample[] samples;
    }
    [Serializable] private class TransformRecord
    {
        public string name;
        public Vector3 local_position;
        public Quaternion local_rotation;
        public Vector3 local_scale;
        public Vector3 world_position;
        public Quaternion world_rotation;
        public Vector3 world_lossy_scale;
    }
    [Serializable] private class MaterialRecord
    {
        public string asset_path;
        public string name;
        public string shader_name;
        public int render_queue;
        public string render_type_tag;
        public string[] keywords;
        public bool has_main_texture;
        public string main_texture_asset_path;
        public int texture_width;
        public int texture_height;
        public string texture_format;
        public bool has_tint;
        public Color tint;
        public bool has_inv_fade;
        public float inv_fade;
    }
    [Serializable] private class Report
    {
        public string schema;
        public string unity_version;
        public string project_path;
        public string contract_sha256;
        public string exporter_sha256;
        public string source_manifest_sha256;
        public string prefab_sha256;
        public string material_sha256;
        public string texture_sha256;
        public string probe_method;
        public uint fixed_random_seed;
        public string reset_policy;
        public MaterialRecord material;
        public SystemRecord[] systems;
        public string[] limits;
    }

    private const string PrefabPath = "Assets/ProbeInputs/Tile-JuDian-A2_lod2.prefab";
    private const string MaterialPath = "Assets/ProbeInputs/Ring_06_light.mat";
    private const string TexturePath = "Assets/ProbeInputs/Ring_07.png";
    private const string SourceManifestPath = "D:/ueDemo/Rok/Saved/RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects/TileParticleSource/source_v1.json";
    private const string PreparePath = "D:/ueDemo/Rok/Scripts/PrepareRokWorldTileParticleProbeV2.py";
    private const string VerifierPath = "D:/ueDemo/Rok/Scripts/VerifyRokWorldTileParticleNativeV2.py";
    private const string SourceManifestSha = "3ac643b894e56d1a6b07935beb1ddee94967d453798bb8dcb23f63b0fb4f2161";
    private const string PrefabSha = "25933047850118e1e105121c062f461bd3c25e22921666e69013326c3a907a91";
    private const string MaterialSha = "b68fa5fa7ae10cef02e325c87869b6ea6d3e1e78fa16f3a3af4ebed188ac2208";
    private const string TextureSha = "1112dbcab4e82c35afb4686153aff77b0ae1397695bc466f99db24ccea7d1237";
    private const uint Seed = 424242;

    [MenuItem("Rok/World Tile Particle Oracle V2/Export")]
    public static void Run()
    {
        int exitCode = 0;
        try { Export(); }
        catch (Exception error) { Debug.LogException(error); exitCode = 1; }
        finally { EditorApplication.Exit(exitCode); }
    }

    private static string Digest(string path)
    {
        using (var stream = File.OpenRead(path))
        using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }
    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
    private static Key[] Keys(AnimationCurve curve)
    {
        return curve == null ? new Key[0] : curve.keys.Select(k => new Key
        { time = k.time, value = k.value, in_tangent = k.inTangent, out_tangent = k.outTangent }).ToArray();
    }
    private static Curve ReadCurve(ParticleSystem.MinMaxCurve value)
    {
        return new Curve { mode = value.mode.ToString(), constant_min = value.constantMin,
            constant_max = value.constantMax, multiplier = value.curveMultiplier,
            max_keys = Keys(value.curveMax), min_keys = Keys(value.curveMin) };
    }
    private static GradientRecord ReadGradient(ParticleSystem.MinMaxGradient value)
    {
        Gradient gradient = value.gradientMax;
        return new GradientRecord { mode = value.mode.ToString(), color_min = value.colorMin,
            color_max = value.colorMax,
            color_keys = gradient == null ? new ColorKey[0] : gradient.colorKeys.Select(k => new ColorKey
            { time = k.time, color = k.color }).ToArray(),
            alpha_keys = gradient == null ? new AlphaKey[0] : gradient.alphaKeys.Select(k => new AlphaKey
            { time = k.time, alpha = k.alpha }).ToArray() };
    }
    private static ParticleRun ReadIndependentReset(ParticleSystem system, float time)
    {
        // The source prefab may be playing because playOnAwake is true. Unity rejects
        // randomSeed writes on a playing system, so stop and clear before each run.
        system.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
        Require(!system.isPlaying && system.particleCount == 0, "Particle reset did not clear: " + system.name);
        system.useAutoRandomSeed = false;
        system.randomSeed = Seed;
        // restart=true always simulates from t=0. Each caller invokes this whole
        // stop/seed/restart sequence independently, never copying prior particles.
        system.Simulate(time, true, true, true);
        var particles = new ParticleSystem.Particle[Math.Max(1024, system.main.maxParticles)];
        int count = system.GetParticles(particles);
        Require(count == system.particleCount, "Particle GetParticles count mismatch: " + system.name);
        var full = new ParticleRecord[count];
        Vector3 min = new Vector3(float.PositiveInfinity, float.PositiveInfinity, float.PositiveInfinity);
        Vector3 max = new Vector3(float.NegativeInfinity, float.NegativeInfinity, float.NegativeInfinity);
        float sizeMin = float.PositiveInfinity, sizeMax = float.NegativeInfinity;
        float lifeMin = float.PositiveInfinity, lifeMax = float.NegativeInfinity;
        for (int i = 0; i < count; i++)
        {
            full[i] = new ParticleRecord { position = particles[i].position, velocity = particles[i].velocity,
                remaining_lifetime = particles[i].remainingLifetime, start_lifetime = particles[i].startLifetime,
                start_size = particles[i].startSize, current_size = particles[i].GetCurrentSize(system),
                rotation = particles[i].rotation, random_seed = particles[i].randomSeed,
                start_color = particles[i].startColor };
            min = Vector3.Min(min, particles[i].position);
            max = Vector3.Max(max, particles[i].position);
            sizeMin = Mathf.Min(sizeMin, particles[i].GetCurrentSize(system));
            sizeMax = Mathf.Max(sizeMax, particles[i].GetCurrentSize(system));
            lifeMin = Mathf.Min(lifeMin, particles[i].remainingLifetime);
            lifeMax = Mathf.Max(lifeMax, particles[i].remainingLifetime);
        }
        return new ParticleRun { particle_count = count, particles = full,
            position_min = count > 0 ? min : Vector3.zero, position_max = count > 0 ? max : Vector3.zero,
            size_min = count > 0 ? sizeMin : 0, size_max = count > 0 ? sizeMax : 0,
            remaining_lifetime_min = count > 0 ? lifeMin : 0,
            remaining_lifetime_max = count > 0 ? lifeMax : 0,
            simulation_space = system.main.simulationSpace.ToString(), is_playing = system.isPlaying };
    }
    private static Sample ReadSample(ParticleSystem system, float time)
    {
        return new Sample { time_seconds = time,
            independent_resets = new[] { ReadIndependentReset(system, time), ReadIndependentReset(system, time) } };
    }
    private static string Hierarchy(Transform node)
    {
        string path = node.name;
        while (node.parent != null) { node = node.parent; path = node.name + "/" + path; }
        return path;
    }
    private static TransformRecord[] ReadAncestorChain(Transform node)
    {
        var chain = new System.Collections.Generic.List<TransformRecord>();
        while (node != null)
        {
            chain.Add(new TransformRecord { name = node.name,
                local_position = node.localPosition, local_rotation = node.localRotation,
                local_scale = node.localScale, world_position = node.position,
                world_rotation = node.rotation, world_lossy_scale = node.lossyScale });
            node = node.parent;
        }
        return chain.ToArray();
    }
    private static SystemRecord ReadSystem(ParticleSystem system)
    {
        var main = system.main;
        var shape = system.shape;
        var emission = system.emission;
        var renderer = system.GetComponent<ParticleSystemRenderer>();
        Require(renderer != null, "Missing renderer: " + system.name);
        var materialPaths = renderer.sharedMaterials.Select(AssetDatabase.GetAssetPath).ToArray();
        Require(materialPaths.Length == 2 && materialPaths.All(path => path == MaterialPath),
            "Particle renderer material slots changed: " + system.name);
        Require(renderer.mesh != null, "Source renderer mesh missing: " + system.name);
        string meshGuid;
        long meshFileId;
        Require(AssetDatabase.TryGetGUIDAndLocalFileIdentifier(renderer.mesh, out meshGuid, out meshFileId)
            && meshGuid == "0000000000000000e000000000000000" && meshFileId == 10210,
            "Built-in source mesh identity changed: " + system.name);
        return new SystemRecord {
            name = system.name, hierarchy_path = Hierarchy(system.transform),
            active_in_hierarchy = system.gameObject.activeInHierarchy,
            local_position = system.transform.localPosition, local_scale = system.transform.localScale,
            local_rotation = system.transform.localRotation,
            world_position = system.transform.position, world_rotation = system.transform.rotation,
            world_lossy_scale = system.transform.lossyScale,
            ancestor_chain_child_to_root = ReadAncestorChain(system.transform),
            duration = main.duration, loop = main.loop, play_on_awake = main.playOnAwake,
            prewarm = main.prewarm, simulation_space = main.simulationSpace.ToString(),
            scaling_mode = main.scalingMode.ToString(), start_lifetime = ReadCurve(main.startLifetime),
            start_size = ReadCurve(main.startSize), start_speed = ReadCurve(main.startSpeed),
            emission_rate_over_time = ReadCurve(emission.rateOverTime),
            size_over_lifetime = ReadCurve(system.sizeOverLifetime.size),
            color_over_lifetime = ReadGradient(system.colorOverLifetime.color),
            shape_type = shape.shapeType.ToString(), shape_radius = shape.radius, shape_arc = shape.arc,
            render_mode = renderer.renderMode.ToString(), render_alignment = renderer.alignment.ToString(),
            sort_mode = renderer.sortMode.ToString(), sorting_layer_id = renderer.sortingLayerID,
            sorting_order = renderer.sortingOrder, mesh_name = renderer.mesh == null ? "" : renderer.mesh.name,
            mesh_bounds = renderer.mesh == null ? new Bounds() : renderer.mesh.bounds,
            mesh_asset_path = AssetDatabase.GetAssetPath(renderer.mesh), mesh_guid = meshGuid,
            mesh_local_file_id = meshFileId, mesh_vertex_count = renderer.mesh.vertexCount,
            mesh_triangle_index_count = renderer.mesh.triangles.Length,
            material_asset_paths = materialPaths,
            samples = new[] { 0f, .5f, 1f, 2.5f, 4.9f, 5.1f, 7.5f }
                .Select(time => ReadSample(system, time)).ToArray()
        };
    }
    private static MaterialRecord ReadMaterial(Material material)
    {
        Require(material != null && material.shader != null, "Missing particle source material/shader");
        Texture texture = material.HasProperty("_MainTex") ? material.GetTexture("_MainTex") : null;
        Require(texture != null && AssetDatabase.GetAssetPath(texture) == TexturePath,
            "Material MainTex did not resolve to Ring_07");
        return new MaterialRecord { asset_path = AssetDatabase.GetAssetPath(material), name = material.name,
            shader_name = material.shader.name, render_queue = material.renderQueue,
            render_type_tag = material.GetTag("RenderType", false, ""), keywords = material.shaderKeywords,
            has_main_texture = texture != null, main_texture_asset_path = AssetDatabase.GetAssetPath(texture),
            texture_width = texture.width, texture_height = texture.height,
            texture_format = texture is Texture2D ? ((Texture2D)texture).format.ToString() : texture.GetType().Name,
            has_tint = material.HasProperty("_TintColor"),
            tint = material.HasProperty("_TintColor") ? material.GetColor("_TintColor") : Color.clear,
            has_inv_fade = material.HasProperty("_InvFade"),
            inv_fade = material.HasProperty("_InvFade") ? material.GetFloat("_InvFade") : 0f };
    }
    private static void Export()
    {
        string project = Path.GetFullPath(Directory.GetParent(Application.dataPath).FullName).Replace('\\', '/');
        Require(project == "D:/ueDemo/Rok/Saved/UnityWorldTileParticleProbeV2", "Wrong isolated Unity project: " + project);
        Require(Application.unityVersion == "2019.4.28f1c1", "Wrong Unity version: " + Application.unityVersion);
        string contractPath = Path.Combine(project, "probe_contract.json");
        var contract = JsonUtility.FromJson<Contract>(File.ReadAllText(contractPath));
        Require(contract != null && contract.schema == "rok.world_tile_particle_probe_contract.v2", "Missing V2 contract");
        Require(contract.source_manifest_sha256 == SourceManifestSha && Digest(SourceManifestPath) == SourceManifestSha,
            "Source manifest differs from hard-pinned V2 source oracle");
        Require(contract.prefab_sha256 == PrefabSha && contract.material_sha256 == MaterialSha
            && contract.texture_sha256 == TextureSha, "Contract source hashes differ from hard-pinned V2 inputs");
        Require(contract.prefab_guid == "06e49016cd7a70648baf210c76991d88"
            && contract.material_guid == "707a424589ea046458deff5a47ad40ff"
            && contract.texture_guid == "f05537e81d3b5464a985ff05e06cb464",
            "Contract source GUIDs differ from hard-pinned V2 inputs");
        Require(Digest(Path.Combine(project, "Assets/Editor/UnityRokWorldTileParticleOracleV2.cs"))
            == contract.exporter_sha256, "Exporter differs from V2 contract");
        Require(Digest(PreparePath) == contract.prepare_sha256
            && Digest(VerifierPath) == contract.verifier_sha256,
            "V2 producer/verifier differs from contract");
        string assets = Path.Combine(project, "Assets", "ProbeInputs");
        string prefabHash = Digest(Path.Combine(assets, "Tile-JuDian-A2_lod2.prefab"));
        string materialHash = Digest(Path.Combine(assets, "Ring_06_light.mat"));
        string textureHash = Digest(Path.Combine(assets, "Ring_07.png"));
        Require(prefabHash == contract.prefab_sha256 && materialHash == contract.material_sha256
            && textureHash == contract.texture_sha256, "Probe asset differs from pinned source");
        Require(AssetDatabase.AssetPathToGUID(PrefabPath) == contract.prefab_guid
            && AssetDatabase.AssetPathToGUID(MaterialPath) == contract.material_guid
            && AssetDatabase.AssetPathToGUID(TexturePath) == contract.texture_guid,
            "Unity GUID import chain differs from source");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(PrefabPath);
        var material = AssetDatabase.LoadAssetAtPath<Material>(MaterialPath);
        Require(prefab != null && material != null, "Native Unity prefab or material did not import");
        var instance = UnityEngine.Object.Instantiate(prefab);
        try
        {
            var systems = instance.GetComponentsInChildren<ParticleSystem>(true)
                .Where(item => item.name == "bowen" || item.name == "bowen (1)")
                .OrderBy(item => item.name, StringComparer.Ordinal).ToArray();
            Require(systems.Length == 2 && systems[0].name == "bowen" && systems[1].name == "bowen (1)",
                "Native Unity bowen identity/count changed");
            var report = new Report { schema = "rok.world_tile_particle_native_witness.v2",
                unity_version = Application.unityVersion, project_path = project,
                contract_sha256 = Digest(contractPath), source_manifest_sha256 = contract.source_manifest_sha256,
                exporter_sha256 = contract.exporter_sha256,
                prefab_sha256 = prefabHash, material_sha256 = materialHash, texture_sha256 = textureHash,
                probe_method = "Imported source prefab/material/texture in isolated Unity 2019 project; stop/clear before seed; twice independently seed and restart Simulate/GetParticles per system/time; no game-map activation or camera visual claim",
                fixed_random_seed = Seed, material = ReadMaterial(material),
                reset_policy = "For every independent run: Stop(true, StopEmittingAndClear), assert empty/stopped, useAutoRandomSeed=false, randomSeed=424242, Simulate(time,true,true,true), GetParticles",
                systems = systems.Select(ReadSystem).ToArray(),
                limits = new[] { "No game-map LOD selection witnessed", "No GPU pixels or visual blend equivalence witnessed",
                    "Fixed seed is a measurement override on an instantiated copy; source autoRandomSeed remains unchanged",
                    "Serialized TintColor is not a shader property for Mobile/Particles/Alpha Blended; do not treat it as effective tint" } };
            string runDir = Path.Combine(project, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff", CultureInfo.InvariantCulture)
                + "_" + System.Diagnostics.Process.GetCurrentProcess().Id.ToString(CultureInfo.InvariantCulture));
            Directory.CreateDirectory(runDir);
            File.WriteAllText(Path.Combine(runDir, "particle_native.json"), JsonUtility.ToJson(report, true));
            Debug.Log("Rok world tile particle native witness: " + runDir);
        }
        finally { UnityEngine.Object.DestroyImmediate(instance); }
    }
}
