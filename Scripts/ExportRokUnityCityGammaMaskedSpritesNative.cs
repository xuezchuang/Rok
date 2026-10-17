using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;

// Isolated Unity 2019 prefab/default export. No GameApp or hotfix lifecycle runs.
public static class ExportRokUnityCityGammaMaskedSpritesNative
{
    private const string ProbeSuffix = "/Saved/UnityCityGammaMaskedSpritesProbeV2/Assets";
    private const string ShaderName = "custom/map/map_building_with_mask-5";

    [Serializable] private sealed class Contract
    {
        public string schema, repo_root, probe_root, unity_version, source_color_space;
        public string closure_sha256, exporter_sha256, runner_sha256, source_settings_sha256;
        public SourceFile[] source_files;
        public RowInput[] rows;
    }
    [Serializable] private sealed class SourceFile
    {
        public string source_path, source_sha256, meta_sha256, source_guid, probe_path;
    }
    [Serializable] private sealed class RowInput
    {
        public string source_key, category, model_id, renderer_hint;
        public string prefab, sprite, mask, material, shader;
        public int source_queue, source_sorting_layer, source_sorting_order;
    }
    [Serializable] private sealed class ColorInputs
    {
        public Color color_1, color_2;
        public string sprite_color_scope, city_transparency_scope;
    }
    [Serializable] private sealed class RowOutput
    {
        public string source_key, category, model_id, prefab_path, renderer_path, source_renderer_hint;
        public string renderer_sprite_path, mask_input_path, material_path, shader_path;
        public string prefab_guid, sprite_guid, mask_guid, material_guid, shader_guid;
        public string renderer_file_id_decimal, transform_file_id_decimal, sprite_local_file_id_decimal;
        public string mask_binding_scope, property_block_scope, sprite_gpu_png, sprite_gpu_png_sha256;
        public string mask_gpu_png, mask_gpu_png_sha256, shader_name, gpu_readback_method;
        public bool prefab_default_only, renderer_enabled, active_self, active_in_hierarchy;
        public bool flip_x, flip_y, prefab_property_block_empty;
        public int source_queue, material_render_queue, sorting_layer_id, sorting_order;
        public int sprite_texture_width, sprite_texture_height, mask_texture_width, mask_texture_height;
        public int sprite_texture_mips, mask_texture_mips;
        public Vector3 root_local_position, root_local_scale, renderer_local_position, renderer_local_scale;
        public Quaternion root_local_rotation, renderer_local_rotation;
        public Matrix4x4 renderer_local_to_world_in_prefab;
        public Vector2[] sprite_vertices, sprite_uv;
        public int[] sprite_triangles;
        public Color renderer_color;
        public ColorInputs serialized_material_inputs;
    }
    [Serializable] private sealed class Report
    {
        public string schema = "rok.city_gamma_masked_sprites_native.v1";
        public string run_directory, unity_version, color_space, graphics_device;
        public string source_contract_sha256, exporter_sha256, scope, failures;
        public bool complete, game_app_loaded, runtime_property_blocks_observed, actual_equal_queue_order_observed;
        public RowOutput[] rows;
    }

    private static void Need(bool condition, string reason)
    {
        if (!condition) throw new InvalidOperationException(reason);
    }

    private static string Normal(string path) { return Path.GetFullPath(path).Replace('\\', '/').TrimEnd('/'); }
    private static string Sha(string path)
    {
        using (var file = File.OpenRead(path))
        using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(file)).Replace("-", "").ToLowerInvariant();
    }
    private static string AssetGuid(string path) { return AssetDatabase.AssetPathToGUID(path).ToLowerInvariant(); }
    private static string LocalId(UnityEngine.Object asset)
    {
        string guid;
        long id;
        Need(AssetDatabase.TryGetGUIDAndLocalFileIdentifier(asset, out guid, out id),
            "Unity persistent GUID/fileId unavailable for " + asset.name);
        return id.ToString(CultureInfo.InvariantCulture);
    }
    private static string AbsoluteAsset(string probe, string path)
    {
        Need(path.StartsWith("Assets/Source/", StringComparison.Ordinal) && path.IndexOf("..", StringComparison.Ordinal) < 0,
            "Asset escapes isolated source tree: " + path);
        return Path.Combine(probe, path.Replace('/', Path.DirectorySeparatorChar));
    }
    private static string HierarchyPath(Transform transform, Transform root)
    {
        var parts = new List<string>();
        for (var node = transform; node != null; node = node.parent)
        {
            parts.Add(node.name);
            if (node == root) { parts.Reverse(); return string.Join("/", parts.ToArray()); }
        }
        throw new InvalidOperationException("Renderer is not inside the source prefab root");
    }

    private static string SaveGpuPng(string output, string name, Texture source, out int width, out int height,
        out int mips)
    {
        Need(source != null && source.width > 0 && source.height > 0, "Source GPU texture missing: " + name);
        width = source.width; height = source.height;
        var source2D = source as Texture2D;
        Need(source2D != null, "Expected Texture2D GPU source: " + name);
        mips = source2D.mipmapCount;
        RenderTexture target = null;
        Texture2D readback = null;
        var prior = RenderTexture.active;
        try
        {
            target = RenderTexture.GetTemporary(width, height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
            target.antiAliasing = 1;
            Graphics.Blit(source, target);
            RenderTexture.active = target;
            readback = new Texture2D(width, height, TextureFormat.RGBA32, false, false);
            readback.ReadPixels(new Rect(0, 0, width, height), 0, 0);
            readback.Apply(false, false);
            File.WriteAllBytes(Path.Combine(output, name), readback.EncodeToPNG());
            return Sha(Path.Combine(output, name));
        }
        finally
        {
            RenderTexture.active = prior;
            if (readback) UnityEngine.Object.DestroyImmediate(readback);
            if (target != null) RenderTexture.ReleaseTemporary(target);
        }
    }

    private static RowOutput Capture(RowInput input, string probe, string output, int index)
    {
        string expectedLeaf = input.renderer_hint.Replace('\\', '/').Split('/').Last();
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(input.prefab);
        var spriteTexture = AssetDatabase.LoadAssetAtPath<Texture2D>(input.sprite);
        var maskTexture = AssetDatabase.LoadAssetAtPath<Texture2D>(input.mask);
        var material = AssetDatabase.LoadAssetAtPath<Material>(input.material);
        var shader = AssetDatabase.LoadAssetAtPath<Shader>(input.shader);
        Need(prefab && spriteTexture && maskTexture && material && shader,
            "Copied prefab/sprite/mask/material/shader not imported: " + input.source_key);
        Need(PrefabUtility.GetPrefabAssetType(prefab) != PrefabAssetType.NotAPrefab,
            "Source asset is not a Unity prefab: " + input.source_key);
        Need(material.shader == shader && shader.name == ShaderName && material.renderQueue == input.source_queue &&
            material.HasProperty("_Color_1") && material.HasProperty("_Color_2") &&
            !material.HasProperty("_SpriteColor") && !material.HasProperty("_CityTransparency"),
            "Source material/shader/queue differs: " + input.source_key);
        var renderers = prefab.GetComponentsInChildren<SpriteRenderer>(true)
            .Where(sr => sr.gameObject.name == expectedLeaf && sr.sprite && sr.sprite.texture == spriteTexture &&
                         sr.sharedMaterial == material).ToArray();
        Need(renderers.Length == 1, "Source prefab renderer is absent or ambiguous: " + input.source_key +
            " candidates=" + renderers.Length);
        var renderer = renderers[0];
        Need(renderer.enabled && renderer.sprite && renderer.sprite.vertices.Length > 0 &&
            renderer.sprite.uv.Length == renderer.sprite.vertices.Length &&
            renderer.sprite.triangles.Length % 3 == 0 && renderer.sortingLayerID == input.source_sorting_layer &&
            renderer.sortingOrder == input.source_sorting_order,
            "Source prefab renderer topology/sorting differs: " + input.source_key);
        string spritePath = AssetDatabase.GetAssetPath(renderer.sprite.texture);
        Need(spritePath == input.sprite, "Sprite.texture asset path differs: " + input.source_key);
        var block = new MaterialPropertyBlock();
        renderer.GetPropertyBlock(block);
        int spriteWidth, spriteHeight, spriteMips, maskWidth, maskHeight, maskMips;
        string stem = index.ToString("D2", CultureInfo.InvariantCulture) + "_" +
            input.source_key.Replace(':', '_');
        string spritePng = stem + "_sprite_gpu.png", maskPng = stem + "_mask_gpu.png";
        string spriteHash = SaveGpuPng(output, spritePng, spriteTexture,
            out spriteWidth, out spriteHeight, out spriteMips);
        string maskHash = SaveGpuPng(output, maskPng, maskTexture,
            out maskWidth, out maskHeight, out maskMips);
        return new RowOutput {
            source_key = input.source_key, category = input.category, model_id = input.model_id,
            prefab_path = input.prefab,
            renderer_path = HierarchyPath(renderer.transform, prefab.transform),
            source_renderer_hint = input.renderer_hint,
            renderer_sprite_path = spritePath, mask_input_path = input.mask,
            material_path = input.material, shader_path = input.shader,
            prefab_guid = AssetGuid(input.prefab), sprite_guid = AssetGuid(input.sprite),
            mask_guid = AssetGuid(input.mask), material_guid = AssetGuid(input.material),
            shader_guid = AssetGuid(input.shader), shader_name = shader.name,
            renderer_file_id_decimal = LocalId(renderer),
            transform_file_id_decimal = LocalId(renderer.transform),
            sprite_local_file_id_decimal = LocalId(renderer.sprite),
            mask_binding_scope = "closure_source_path_not_prefab_property_block_binding",
            property_block_scope = "prefab_default_only",
            prefab_default_only = true, renderer_enabled = renderer.enabled,
            active_self = renderer.gameObject.activeSelf,
            active_in_hierarchy = renderer.gameObject.activeInHierarchy,
            flip_x = renderer.flipX, flip_y = renderer.flipY,
            prefab_property_block_empty = block.isEmpty,
            source_queue = input.source_queue, material_render_queue = material.renderQueue,
            sorting_layer_id = renderer.sortingLayerID, sorting_order = renderer.sortingOrder,
            sprite_texture_width = spriteWidth, sprite_texture_height = spriteHeight,
            mask_texture_width = maskWidth, mask_texture_height = maskHeight,
            sprite_texture_mips = spriteMips, mask_texture_mips = maskMips,
            root_local_position = prefab.transform.localPosition,
            root_local_rotation = prefab.transform.localRotation,
            root_local_scale = prefab.transform.localScale,
            renderer_local_position = renderer.transform.localPosition,
            renderer_local_rotation = renderer.transform.localRotation,
            renderer_local_scale = renderer.transform.localScale,
            renderer_local_to_world_in_prefab = renderer.transform.localToWorldMatrix,
            sprite_vertices = renderer.sprite.vertices, sprite_uv = renderer.sprite.uv,
            sprite_triangles = renderer.sprite.triangles.Select(value => (int)value).ToArray(),
            renderer_color = renderer.color,
            serialized_material_inputs = new ColorInputs {
                color_1 = material.GetColor("_Color_1"), color_2 = material.GetColor("_Color_2"),
                sprite_color_scope = "source shader uniform; not a serialized Material property",
                city_transparency_scope = "source shader uniform; no GameApp runtime value observed" },
            sprite_gpu_png = spritePng, sprite_gpu_png_sha256 = spriteHash,
            mask_gpu_png = maskPng, mask_gpu_png_sha256 = maskHash,
            gpu_readback_method = "Graphics.Blit imported Texture2D to ARGB32 RenderTexture in Gamma project, then RGBA32 ReadPixels/EncodeToPNG; not raw PNG bytes"
        };
    }

    public static void Run()
    {
        string assets = Normal(Application.dataPath);
        Need(assets.EndsWith(ProbeSuffix, StringComparison.OrdinalIgnoreCase) &&
            Application.unityVersion == "2019.4.28f1c1" &&
            QualitySettings.activeColorSpace == ColorSpace.Gamma,
            "Wrong isolated Unity 2019 Gamma project identity");
        string probe = assets.Substring(0, assets.Length - "/Assets".Length);
        string repo = assets.Substring(0, assets.Length - ProbeSuffix.Length);
        string contractPath = Path.Combine(probe, "source_contract.json");
        string exporterPath = Path.Combine(repo, "Scripts/ExportRokUnityCityGammaMaskedSpritesNative.cs");
        string localExporter = Path.Combine(probe, "Assets/Editor/ExportRokUnityCityGammaMaskedSpritesNative.cs");
        var contract = JsonUtility.FromJson<Contract>(File.ReadAllText(contractPath, Encoding.UTF8));
        Need(contract != null && contract.schema == "rok.city_gamma_masked_sprites_probe_contract.v1" &&
            Normal(contract.repo_root) == repo && Normal(contract.probe_root) == probe &&
            contract.unity_version == Application.unityVersion && contract.source_color_space == "Gamma" &&
            Sha(exporterPath) == contract.exporter_sha256 && Sha(localExporter) == contract.exporter_sha256 &&
            contract.rows != null && contract.rows.Length == 27 &&
            contract.source_files != null && contract.source_files.Length >= 5,
            "Probe source contract or exporter hash differs");
        foreach (var file in contract.source_files)
        {
            string source = Path.Combine(repo, file.source_path);
            string copy = AbsoluteAsset(probe, file.probe_path);
            Need(Sha(source) == file.source_sha256 && Sha(copy) == file.source_sha256 &&
                Sha(source + ".meta") == file.meta_sha256 && Sha(copy + ".meta") == file.meta_sha256 &&
                AssetGuid(file.probe_path) == file.source_guid,
                "Source/copy asset or GUID changed: " + file.source_path);
        }
        string output = Path.Combine(probe, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") +
            "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        Need(!Directory.Exists(output), "Native output already exists");
        Directory.CreateDirectory(output);
        var report = new Report {
            run_directory = Normal(output), unity_version = Application.unityVersion,
            color_space = QualitySettings.activeColorSpace.ToString(),
            graphics_device = SystemInfo.graphicsDeviceType.ToString(),
            source_contract_sha256 = Sha(contractPath), exporter_sha256 = contract.exporter_sha256,
            scope = "Isolated prefab/default AssetDatabase and GPU texture readback. No GameApp runtime, runtime PropertyBlocks or actual equal-queue draw order.",
            game_app_loaded = false, runtime_property_blocks_observed = false,
            actual_equal_queue_order_observed = false
        };
        try
        {
            Need(contract.rows.Select(row => row.source_key).Distinct().Count() == 27 &&
                contract.rows.Count(row => row.category == "initial_source_building") == 23 &&
                contract.rows.Count(row => row.category == "city_gate_flag") == 2 &&
                contract.rows.Count(row => row.category == "dynamic_local_new") == 2,
                "Native allowlist category cardinality differs");
            var rows = new List<RowOutput>();
            for (int i = 0; i < contract.rows.Length; i++)
                rows.Add(Capture(contract.rows[i], probe, output, i));
            report.rows = rows.ToArray();
            report.complete = true;
            File.WriteAllText(Path.Combine(output, "native_masked_sprites.json"),
                JsonUtility.ToJson(report, true), new UTF8Encoding(false));
            Debug.Log("ROK_CITY_GAMMA_MASKED_NATIVE_OUTPUT " + output);
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            report.complete = false;
            report.failures = error.ToString();
            File.WriteAllText(Path.Combine(output, "native_masked_sprites.json"),
                JsonUtility.ToJson(report, true), new UTF8Encoding(false));
            File.WriteAllText(Path.Combine(output, "failure.txt"), error.ToString(), new UTF8Encoding(false));
            Debug.LogError(error);
            EditorApplication.Exit(1);
        }
    }
}
