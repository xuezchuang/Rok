using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;

// Isolated import/default inspection only. This does not run GameApp or infer its draw state.
public static class ExportRokCityGammaRemainingFamiliesNative
{
    private const string ProbeSuffix = "/Saved/UnityCityGammaRemainingFamiliesProbe/Assets";
    [Serializable] private sealed class SourceFile { public string source_path, source_sha256, meta_sha256, source_guid, probe_path; }
    [Serializable] private sealed class Role { public string role, probe_path; }
    [Serializable] private sealed class FileId { public string name, @decimal; }
    [Serializable] private sealed class Input
    {
        public string source_key, category, model_id, renderer_hint, source_material, source_prefab, shader_kind;
        public int source_queue;
        public Role[] roles;
        public FileId[] known_file_ids;
    }
    [Serializable] private sealed class Contract
    {
        public string schema, repo_root, probe_root, unity_version, source_color_space;
        public string closure_sha256, exporter_sha256, runner_sha256, source_settings_sha256, scope;
        public SourceFile[] source_files;
        public Input[] rows;
    }
    [Serializable] private sealed class UvChannel { public int channel; public Vector4[] values; }
    [Serializable] private sealed class Submesh { public int index; public string topology; public int[] indices; }
    [Serializable] private sealed class MeshData
    {
        public string asset_path, name, normals_scope, vertex_colors_scope, local_file_id_decimal;
        public Vector3[] vertices, normals;
        public Color32[] colors;
        public UvChannel[] uv_channels;
        public Submesh[] submeshes;
        public Bounds bounds;
    }
    [Serializable] private sealed class MaterialSlot
    {
        public int slot;
        public string asset_path, shader_name, local_file_id_decimal;
    }
    [Serializable] private sealed class MaterialProperty
    {
        public string name, kind, texture_asset_path, texture_binding_scope, default_texture_scope;
        public float number;
        public Color color;
        public Vector4 vector;
        public Vector2 texture_scale, texture_offset;
    }
    [Serializable] private sealed class TextureData
    {
        public string role, binding_scope, default_texture_scope, source_asset_path, source_guid;
        public string gpu_png, gpu_png_sha256, readback_method, filter, wrap, importer_json;
        public int width, height, mip_count;
        public bool srgb;
    }
    [Serializable] private sealed class Row
    {
        public string source_key, category, model_id, renderer_hint, renderer_path, renderer_file_id_decimal;
        public string source_prefab, source_material, prefab_guid, source_material_guid, shader_name, shader_asset_path;
        public string shader_source_scope, active_pass_selection, active_blend_state, active_zwrite_state;
        public string runtime_globals_scope, property_block_scope, source_material_binding_scope;
        public bool prefab_default_only, renderer_enabled, renderer_active_self, source_material_bound_in_prefab;
        public int source_material_queue, source_material_pass_count, renderer_sorting_order, renderer_sorting_layer_id;
        public string[] source_material_pass_names, source_material_keywords;
        public MaterialSlot[] material_slots;
        public MaterialProperty[] declared_properties;
        public TextureData[] textures;
        public MeshData mesh;
        public Vector3 root_position, root_scale, renderer_local_position, renderer_local_scale;
        public Quaternion root_rotation, renderer_local_rotation;
        public Matrix4x4 renderer_local_to_world_in_prefab;
        public Color renderer_color;
    }
    [Serializable] private sealed class Output
    {
        public string schema = "rok.city_gamma_remaining_families_native.v1";
        public string run_directory, unity_version, color_space, graphics_device, source_contract_sha256;
        public string exporter_sha256, scope, failures;
        public bool complete, game_app_loaded;
        public Row[] rows;
    }

    private static void Need(bool value, string why) { if (!value) throw new InvalidOperationException(why); }
    private static string Normal(string path) { return Path.GetFullPath(path).Replace('\\', '/').TrimEnd('/'); }
    private static string Sha(string path)
    {
        using (var stream = File.OpenRead(path))
        using (var hash = SHA256.Create())
            return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }
    private static string GuidFor(string asset) { return AssetDatabase.AssetPathToGUID(asset).ToLowerInvariant(); }
    private static string LocalId(UnityEngine.Object value)
    {
        string guid; long id;
        Need(AssetDatabase.TryGetGUIDAndLocalFileIdentifier(value, out guid, out id), "persistent file ID unavailable: " + value.name);
        return id.ToString(CultureInfo.InvariantCulture);
    }
    private static string AbsAsset(string probe, string asset)
    {
        Need(asset.StartsWith("Assets/Source/", StringComparison.Ordinal) && asset.IndexOf("..", StringComparison.Ordinal) < 0,
            "asset escapes isolated source: " + asset);
        return Path.Combine(probe, asset.Replace('/', Path.DirectorySeparatorChar));
    }
    private static string NodePath(Transform node, Transform root)
    {
        var parts = new List<string>();
        for (var current = node; current != null; current = current.parent)
        {
            parts.Add(current.name);
            if (current == root) { parts.Reverse(); return string.Join("/", parts.ToArray()); }
        }
        throw new InvalidOperationException("renderer outside source prefab");
    }
    private static MeshData ReadMesh(Renderer renderer)
    {
        var sr = renderer as SpriteRenderer;
        if (sr != null)
        {
            Need(sr.sprite != null, "sprite renderer has no sprite");
            var sprite = sr.sprite;
            var uv = new UvChannel[8];
            for (int i = 0; i < 8; i++) uv[i] = new UvChannel { channel = i,
                values = i == 0 ? sprite.uv.Select(v => new Vector4(v.x, v.y, 0, 0)).ToArray() : new Vector4[0] };
            return new MeshData { asset_path = AssetDatabase.GetAssetPath(sprite), name = sprite.name,
                local_file_id_decimal = LocalId(sprite), vertices = sprite.vertices.Select(v => new Vector3(v.x, v.y, 0)).ToArray(),
                normals = new Vector3[0], normals_scope = "not_exposed_by_Unity2019_Sprite_vertices_api",
                colors = new Color32[0], vertex_colors_scope = "not_exposed_by_Unity2019_Sprite_vertices_api",
                uv_channels = uv, submeshes = new [] { new Submesh { index = 0, topology = "Triangles",
                    indices = sprite.triangles.Select(i => (int)i).ToArray() } }, bounds = sprite.bounds };
        }
        var filter = renderer.GetComponent<MeshFilter>();
        Need(filter != null && filter.sharedMesh != null, "MeshRenderer has no MeshFilter/sharedMesh: " + renderer.name);
        var mesh = filter.sharedMesh;
        var channels = new UvChannel[8];
        for (int i = 0; i < 8; i++)
        {
            var values = new List<Vector4>();
            mesh.GetUVs(i, values);
            channels[i] = new UvChannel { channel = i, values = values.ToArray() };
        }
        var subs = new Submesh[mesh.subMeshCount];
        for (int i = 0; i < mesh.subMeshCount; i++)
            subs[i] = new Submesh { index = i, topology = mesh.GetTopology(i).ToString(), indices = mesh.GetIndices(i) };
        return new MeshData { asset_path = AssetDatabase.GetAssetPath(mesh), name = mesh.name,
            local_file_id_decimal = LocalId(mesh), vertices = mesh.vertices, normals = mesh.normals,
            normals_scope = "Mesh.normals imported", colors = mesh.colors32,
            vertex_colors_scope = "Mesh.colors32 imported; empty means no vertex color channel",
            uv_channels = channels, submeshes = subs, bounds = mesh.bounds };
    }
    private static TextureData SaveGpu(string output, string stem, int index, string role, string scope, Texture2D source)
    {
        Need(source != null && source.width > 0 && source.height > 0, "GPU texture missing: " + role);
        var name = stem + "_" + index.ToString("D2", CultureInfo.InvariantCulture) + "_gpu.png";
        RenderTexture target = null;
        Texture2D pixels = null;
        var prior = RenderTexture.active;
        try
        {
            target = RenderTexture.GetTemporary(source.width, source.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
            target.antiAliasing = 1;
            Graphics.Blit(source, target);
            RenderTexture.active = target;
            pixels = new Texture2D(source.width, source.height, TextureFormat.RGBA32, false, false);
            pixels.ReadPixels(new Rect(0, 0, source.width, source.height), 0, 0);
            pixels.Apply(false, false);
            File.WriteAllBytes(Path.Combine(output, name), pixels.EncodeToPNG());
        }
        finally
        {
            RenderTexture.active = prior;
            if (pixels) UnityEngine.Object.DestroyImmediate(pixels);
            if (target != null) RenderTexture.ReleaseTemporary(target);
        }
        string path = AssetDatabase.GetAssetPath(source);
        var importer = AssetImporter.GetAtPath(path) as TextureImporter;
        return new TextureData { role = role, binding_scope = scope, default_texture_scope = "not_applicable_bound_or_contract_asset",
            source_asset_path = path, source_guid = string.IsNullOrEmpty(path) ? "" : GuidFor(path),
            gpu_png = name, gpu_png_sha256 = Sha(Path.Combine(output, name)), width = source.width, height = source.height,
            mip_count = source.mipmapCount, filter = source.filterMode.ToString(), wrap = source.wrapMode.ToString(),
            srgb = importer != null && importer.sRGBTexture,
            importer_json = importer != null ? EditorJsonUtility.ToJson(importer, true) : "",
            readback_method = "Graphics.Blit imported Texture2D to ARGB32 RT in Gamma project; RGBA32 ReadPixels EncodeToPNG; not source bytes" };
    }
    private static Row Capture(Input input, string output, int index)
    {
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(input.source_prefab);
        var material = AssetDatabase.LoadAssetAtPath<Material>(input.source_material);
        Need(prefab && material && material.shader, "source prefab/material/shader not imported: " + input.source_key);
        Need(material.renderQueue == input.source_queue, "material render queue drift: " + input.source_key);
        string leaf = input.renderer_hint.Replace('\\', '/').Split('/').Last();
        var candidates = prefab.GetComponentsInChildren<Renderer>(true).Where(r => r.name == leaf &&
            (r is SpriteRenderer || r is MeshRenderer)).ToArray();
        Need(candidates.Length == 1, "source renderer missing or ambiguous: " + input.source_key + " count=" + candidates.Length);
        var renderer = candidates[0];
        var sourceShader = material.shader;
        var expectedShader = input.roles.FirstOrDefault(r => r.role == "shader");
        if (input.shader_kind == "custom")
            Need(expectedShader != null && AssetDatabase.GetAssetPath(sourceShader) == expectedShader.probe_path,
                "source custom shader binding drift: " + input.source_key);
        else Need(expectedShader == null, "builtin shader row unexpectedly has copied shader source");
        var knownRenderer = input.known_file_ids == null ? null :
            input.known_file_ids.FirstOrDefault(x => x.name == "mesh_renderer_file_id");
        if (knownRenderer != null)
            Need(LocalId(renderer) == knownRenderer.@decimal, "pinned renderer file ID differs: " + input.source_key);
        var slotMaterials = renderer.sharedMaterials;
        var slots = new MaterialSlot[slotMaterials.Length];
        for (int i = 0; i < slots.Length; i++)
        {
            var value = slotMaterials[i];
            slots[i] = new MaterialSlot { slot = i, asset_path = value ? AssetDatabase.GetAssetPath(value) : "",
                shader_name = value && value.shader ? value.shader.name : "", local_file_id_decimal = value ? LocalId(value) : "" };
        }
        var properties = new List<MaterialProperty>();
        var textures = new List<TextureData>();
        string stem = index.ToString("D2", CultureInfo.InvariantCulture) + "_" + input.source_key.Replace(':', '_');
        for (int i = 0; i < ShaderUtil.GetPropertyCount(sourceShader); i++)
        {
            string name = ShaderUtil.GetPropertyName(sourceShader, i);
            var type = ShaderUtil.GetPropertyType(sourceShader, i);
            var p = new MaterialProperty { name = name, kind = type.ToString() };
            if (type == ShaderUtil.ShaderPropertyType.Color) p.color = material.GetColor(name);
            else if (type == ShaderUtil.ShaderPropertyType.Vector) p.vector = material.GetVector(name);
            else if (type == ShaderUtil.ShaderPropertyType.Float || type == ShaderUtil.ShaderPropertyType.Range)
                p.number = material.GetFloat(name);
            else if (type == ShaderUtil.ShaderPropertyType.TexEnv)
            {
                var texture = material.GetTexture(name);
                p.texture_asset_path = texture ? AssetDatabase.GetAssetPath(texture) : "";
                p.texture_scale = material.GetTextureScale(name); p.texture_offset = material.GetTextureOffset(name);
                p.texture_binding_scope = texture ? "material_GetTexture" : "unbound_declared_sampler";
                p.default_texture_scope = texture ? "not_applicable_bound" : "unknown_api_unavailable";
                if (texture)
                {
                    var tex2d = texture as Texture2D;
                    Need(tex2d != null, "declared sampler bound to non-Texture2D: " + name);
                    textures.Add(SaveGpu(output, stem, textures.Count, "declared_sampler:" + name, "material_GetTexture", tex2d));
                }
                else textures.Add(new TextureData { role = "declared_sampler:" + name,
                    binding_scope = "unbound_declared_sampler", default_texture_scope = "unknown_api_unavailable" });
            }
            properties.Add(p);
        }
        foreach (var role in input.roles)
        {
            string ext = Path.GetExtension(role.probe_path).ToLowerInvariant();
            if (ext != ".png" && ext != ".tif" && ext != ".tga" && ext != ".psd" && ext != ".jpg") continue;
            var tex = AssetDatabase.LoadAssetAtPath<Texture2D>(role.probe_path);
            Need(tex != null, "source-role texture did not import: " + role.probe_path);
            textures.Add(SaveGpu(output, stem, textures.Count, "source_role:" + role.role, "source_contract_asset", tex));
        }
        var passNames = new string[material.passCount];
        for (int i = 0; i < passNames.Length; i++) passNames[i] = material.GetPassName(i);
        var sr = renderer as SpriteRenderer;
        string shaderPath = AssetDatabase.GetAssetPath(sourceShader);
        var result = new Row { source_key = input.source_key, category = input.category, model_id = input.model_id,
            renderer_hint = input.renderer_hint, renderer_path = NodePath(renderer.transform, prefab.transform),
            renderer_file_id_decimal = LocalId(renderer), source_prefab = input.source_prefab,
            source_material = input.source_material, prefab_guid = GuidFor(input.source_prefab),
            source_material_guid = GuidFor(input.source_material), shader_name = sourceShader.name,
            shader_asset_path = shaderPath, shader_source_scope = input.shader_kind == "builtin" ? "builtin_unavailable" : "copied_custom_shader",
            active_pass_selection = input.shader_kind == "builtin" ? "unknown_builtin_shader" : "unknown_no_public_active_pass_selection_api",
            active_blend_state = "unknown_no_public_pass_state_api", active_zwrite_state = "unknown_no_public_pass_state_api",
            runtime_globals_scope = "not_observed_isolated_defaults_only", property_block_scope = "not_GameApp_runtime",
            source_material_binding_scope = "material asset inspected separately from prefab renderer slots",
            prefab_default_only = true, renderer_enabled = renderer.enabled, renderer_active_self = renderer.gameObject.activeSelf,
            source_material_bound_in_prefab = slotMaterials.Any(m => m == material),
            source_material_queue = material.renderQueue, source_material_pass_count = material.passCount,
            source_material_pass_names = passNames, source_material_keywords = material.shaderKeywords,
            renderer_sorting_order = renderer.sortingOrder, renderer_sorting_layer_id = renderer.sortingLayerID,
            material_slots = slots, declared_properties = properties.ToArray(), textures = textures.ToArray(),
            mesh = ReadMesh(renderer), root_position = prefab.transform.localPosition, root_rotation = prefab.transform.localRotation,
            root_scale = prefab.transform.localScale, renderer_local_position = renderer.transform.localPosition,
            renderer_local_rotation = renderer.transform.localRotation, renderer_local_scale = renderer.transform.localScale,
            renderer_local_to_world_in_prefab = renderer.transform.localToWorldMatrix,
            renderer_color = sr ? sr.color : Color.white };
        Need(result.mesh.vertices.Length > 0 && result.mesh.submeshes.Length > 0, "mesh vertices/indices unavailable");
        var knownMesh = input.known_file_ids == null ? null :
            input.known_file_ids.FirstOrDefault(x => x.name == "mesh_file_id");
        if (knownMesh != null)
            Need(result.mesh.local_file_id_decimal == knownMesh.@decimal, "pinned mesh file ID differs: " + input.source_key);
        var expectedFbx = input.roles.FirstOrDefault(r => r.role == "source_fbx");
        if (expectedFbx != null)
            Need(result.mesh.asset_path == expectedFbx.probe_path, "renderer mesh not bound to pinned source FBX: " + input.source_key);
        return result;
    }
    public static void Run()
    {
        string assets = Normal(Application.dataPath);
        Need(assets.EndsWith(ProbeSuffix, StringComparison.OrdinalIgnoreCase) &&
            Application.unityVersion == "2019.4.28f1c1" && QualitySettings.activeColorSpace == ColorSpace.Gamma,
            "wrong Unity 2019 Gamma isolated project");
        string probe = assets.Substring(0, assets.Length - "/Assets".Length);
        string repo = assets.Substring(0, assets.Length - ProbeSuffix.Length);
        string contractPath = Path.Combine(probe, "source_contract.json");
        var contract = JsonUtility.FromJson<Contract>(File.ReadAllText(contractPath, Encoding.UTF8));
        Need(contract != null && contract.schema == "rok.city_gamma_remaining_families_contract.v1" &&
            Normal(contract.repo_root) == repo && Normal(contract.probe_root) == probe &&
            contract.unity_version == Application.unityVersion && contract.source_color_space == "Gamma" &&
            contract.rows != null && contract.rows.Length == 5 && contract.source_files != null &&
            Sha(Path.Combine(repo, "Scripts/ExportRokCityGammaRemainingFamiliesNative.cs")) == contract.exporter_sha256 &&
            Sha(Path.Combine(probe, "Assets/Editor/ExportRokCityGammaRemainingFamiliesNative.cs")) == contract.exporter_sha256,
            "source contract/exporter identity differs");
        foreach (var file in contract.source_files)
        {
            var original = Path.Combine(repo, file.source_path);
            var copy = AbsAsset(probe, file.probe_path);
            Need(Sha(original) == file.source_sha256 && Sha(copy) == file.source_sha256 &&
                 Sha(original + ".meta") == file.meta_sha256 && Sha(copy + ".meta") == file.meta_sha256 &&
                 GuidFor(file.probe_path) == file.source_guid, "source/copy/GUID drift: " + file.source_path);
        }
        string outputDir = Path.Combine(probe, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") +
            "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        Need(!Directory.Exists(outputDir), "native output run already exists");
        Directory.CreateDirectory(outputDir);
        var report = new Output { run_directory = Normal(outputDir), unity_version = Application.unityVersion,
            color_space = QualitySettings.activeColorSpace.ToString(), graphics_device = SystemInfo.graphicsDeviceType.ToString(),
            source_contract_sha256 = Sha(contractPath), exporter_sha256 = contract.exporter_sha256,
            scope = "isolated_prefab_material_defaults_only", game_app_loaded = false };
        try
        {
            string[] keys = { "initial:5", "ground:ground_1", "ground:ground_2",
                "support:boundary_support_road", "support:boundary_support_wall_shadow" };
            Need(contract.rows.Select(x => x.source_key).SequenceEqual(keys), "five-row allowlist differs");
            report.rows = contract.rows.Select((row, index) => Capture(row, outputDir, index)).ToArray();
            report.complete = true;
            File.WriteAllText(Path.Combine(outputDir, "native_families.json"), JsonUtility.ToJson(report, true), new UTF8Encoding(false));
            Debug.Log("ROK_CITY_GAMMA_REMAINING_FAMILIES_NATIVE_OUTPUT " + outputDir);
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            report.complete = false; report.failures = error.ToString();
            File.WriteAllText(Path.Combine(outputDir, "native_families.json"), JsonUtility.ToJson(report, true), new UTF8Encoding(false));
            File.WriteAllText(Path.Combine(outputDir, "failure.txt"), error.ToString(), new UTF8Encoding(false));
            Debug.LogException(error);
            EditorApplication.Exit(1);
        }
    }
}
