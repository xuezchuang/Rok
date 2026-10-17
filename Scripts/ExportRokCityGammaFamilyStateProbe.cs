using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using Client;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

// This is a source-material GPU control in an isolated Gamma project, not a GameApp renderer.
public static class ExportRokCityGammaFamilyStateProbe
{
    private const string ProbeSuffix = "/Saved/UnityCityGammaFamilyStateProbeV2/Assets";
    [Serializable] private sealed class SourceFile { public string source_path, source_sha256, meta_sha256, source_guid, probe_path; }
    [Serializable] private sealed class Role { public string role, probe_path; }
    [Serializable] private sealed class Input
    {
        public string source_key, category, model_id, renderer_hint, source_material, source_prefab, shader_kind;
        public int source_queue;
        public Role[] roles;
    }
    [Serializable] private sealed class Contract
    {
        public string schema, repo_root, probe_root, unity_version, source_color_space;
        public string base_contract_sha256, base_native_sha256, exporter_sha256, runner_sha256, stub_sha256, mip_shader_sha256, scope;
        public SourceFile[] source_files;
        public Input[] rows;
    }
    [Serializable] private sealed class Level
    {
        public int level, width, height, byte_count;
        public string file, sha256, png, png_sha256, readback_format, readback_scope;
        public string gpu_sample_method, gpu_source_asset_path, gpu_source_guid, pixel_center_uv_scope, transfer_scope;
    }
    [Serializable] private sealed class TextureRecord
    {
        public string source_asset_path, source_guid, filter, wrap, importer_json;
        public int width, height, mip_count;
        public bool srgb;
        public Level[] levels;
    }
    [Serializable] private sealed class Frame
    {
        public string name, png, sha256, material_scope, geometry_scope, depth_setup;
        public int[] center_rgba8;
        public float realtime_before, realtime_after;
    }
    [Serializable] private sealed class Row
    {
        public string source_key, source_material_guid, renderer_file_id_decimal, source_mesh_file_id_decimal;
        public string source_renderer_path, shader_name, source_mask_sprite_asset_path, source_mask_sprite_file_id_decimal;
        public string source_material_mask_binding_scope, source_mask_start_scope, time_scope, globals_scope;
        public string night_light_time_formula;
        public float night_light_multiplier_min, night_light_multiplier_max;
        public bool actual_shader_time_observed;
        public int source_material_queue, source_vertex_count, source_triangle_count, source_vertex_color_count;
        public int source_vertex_alpha_min, source_vertex_alpha_max;
        public bool source_mask_sprite_bound, source_mask_property_block_matches, source_material_mask_unbound;
        public Frame[] frames;
        public TextureRecord[] textures;
    }
    [Serializable] private sealed class Output
    {
        public string schema = "rok.city_gamma_family_state_native.v2";
        public string run_directory, unity_version, color_space, graphics_device, source_contract_sha256;
        public string exporter_sha256, scope, failures;
        public bool complete, game_app_loaded;
        public Row[] rows;
    }

    private static void Need(bool value, string why) { if (!value) throw new InvalidOperationException(why); }
    private static string Normal(string p) { return Path.GetFullPath(p).Replace('\\', '/').TrimEnd('/'); }
    private static string Sha(string p)
    {
        using (var stream = File.OpenRead(p))
        using (var hash = SHA256.Create())
            return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }
    private static string Id(UnityEngine.Object value)
    {
        string guid = null; long local = 0;
        Need(value && AssetDatabase.TryGetGUIDAndLocalFileIdentifier(value, out guid, out local),
            "source persistent file ID unavailable");
        return local.ToString(CultureInfo.InvariantCulture);
    }
    private static string AssetGuid(string path) { return AssetDatabase.AssetPathToGUID(path).ToLowerInvariant(); }
    private static string Hierarchy(Transform node, Transform root)
    {
        var parts = new List<string>();
        for (var t = node; t != null; t = t.parent)
        {
            parts.Add(t.name);
            if (t == root) { parts.Reverse(); return string.Join("/", parts.ToArray()); }
        }
        throw new InvalidOperationException("source renderer outside source prefab");
    }
    private static Texture2D SolidTexture(string name, Color32 color)
    {
        var value = new Texture2D(2, 2, TextureFormat.RGBA32, false, false);
        value.name = name;
        value.filterMode = FilterMode.Point;
        value.wrapMode = TextureWrapMode.Clamp;
        value.SetPixels32(new [] { color, color, color, color });
        value.Apply(false, false);
        return value;
    }
    private static Mesh Quad(string name, byte vertexAlpha)
    {
        var mesh = new Mesh(); mesh.name = name;
        mesh.vertices = new [] { new Vector3(-1, -1, 0), new Vector3(1, -1, 0),
            new Vector3(1, 1, 0), new Vector3(-1, 1, 0) };
        mesh.uv = new [] { new Vector2(0, 0), new Vector2(1, 0), new Vector2(1, 1), new Vector2(0, 1) };
        mesh.colors32 = Enumerable.Repeat(new Color32(255, 255, 255, vertexAlpha), 4).ToArray();
        // Camera looks down +Z from -Z; clockwise in camera space is the front face.
        mesh.triangles = new [] { 0, 2, 1, 0, 3, 2 };
        mesh.RecalculateNormals();
        return mesh;
    }
    private static GameObject RenderObject(string name, Mesh mesh, Material material, float z)
    {
        var obj = new GameObject(name);
        obj.transform.position = new Vector3(0, 0, z);
        obj.AddComponent<MeshFilter>().sharedMesh = mesh;
        var renderer = obj.AddComponent<MeshRenderer>();
        renderer.sharedMaterial = material;
        renderer.shadowCastingMode = ShadowCastingMode.Off;
        renderer.receiveShadows = false;
        return obj;
    }
    private static TextureRecord ReadMips(Texture2D texture, string output, Dictionary<string, TextureRecord> cache,
        Material mipMaterial)
    {
        string path = AssetDatabase.GetAssetPath(texture);
        Need(!string.IsNullOrEmpty(path) && path.StartsWith("Assets/Source/", StringComparison.Ordinal),
            "mip input is not a pinned source asset: " + path);
        TextureRecord prior;
        if (cache.TryGetValue(path, out prior)) return prior;
        int id = cache.Count;
        var importer = AssetImporter.GetAtPath(path) as TextureImporter;
        Need(importer != null, "source texture importer absent: " + path);
        var record = new TextureRecord { source_asset_path = path, source_guid = AssetGuid(path),
            filter = texture.filterMode.ToString(), wrap = texture.wrapMode.ToString(),
            importer_json = EditorJsonUtility.ToJson(importer, true), width = texture.width, height = texture.height,
            mip_count = texture.mipmapCount, srgb = importer.sRGBTexture };
        var levels = new List<Level>();
        for (int mip = 0; mip < texture.mipmapCount; mip++)
        {
            int width = Math.Max(1, texture.width >> mip), height = Math.Max(1, texture.height >> mip);
            var target = RenderTexture.GetTemporary(width, height, 0, RenderTextureFormat.ARGB32,
                RenderTextureReadWrite.Default);
            var previous = RenderTexture.active;
            Texture2D pixels = null;
            try
            {
                target.filterMode = FilterMode.Point;
                target.wrapMode = TextureWrapMode.Clamp;
                mipMaterial.SetFloat("_MipLevel", mip);
                Graphics.Blit(texture, target, mipMaterial, 0);
                RenderTexture.active = target;
                pixels = new Texture2D(width, height, TextureFormat.RGBA32, false, false);
                pixels.ReadPixels(new Rect(0, 0, width, height), 0, 0, false);
                pixels.Apply(false, false);
                byte[] bytes = pixels.GetRawTextureData();
                Need(bytes.Length == width * height * 4,
                    "GPU mip RGBA32 returned unexpected stride/length: " + path + " level=" + mip + " bytes=" + bytes.Length);
                string stem = "texture_" + id.ToString("D2", CultureInfo.InvariantCulture) +
                    "_mip_" + mip.ToString("D2", CultureInfo.InvariantCulture);
                string filename = stem + ".rgba8.bin";
                string png = stem + ".png";
                string destination = Path.Combine(output, filename);
                string pngPath = Path.Combine(output, png);
                File.WriteAllBytes(destination, bytes);
                File.WriteAllBytes(pngPath, pixels.EncodeToPNG());
                levels.Add(new Level { level = mip, width = width, height = height, byte_count = bytes.Length,
                    file = filename, sha256 = Sha(destination), png = png, png_sha256 = Sha(pngPath),
                    readback_format = "RGBA32", gpu_sample_method = "Graphics.Blit plus explicit tex2Dlod mip into ARGB32 RT; Texture2D.ReadPixels RGBA32",
                    gpu_source_asset_path = path, gpu_source_guid = record.source_guid,
                    pixel_center_uv_scope = "full-screen Graphics.Blit UV at target pixel centers; orientation checked against pinned mip0 GPU PNG",
                    transfer_scope = "Unity2019 Gamma, source importer sRGBTexture recorded; ARGB32 RenderTextureReadWrite.Default; shader has no explicit encode/decode",
                    readback_scope = "expanded GPU shader sample, not source compressed bits; Unity Texture2D RGBA32 raw row order" });
            }
            finally
            {
                RenderTexture.active = previous;
                if (pixels) UnityEngine.Object.DestroyImmediate(pixels);
                RenderTexture.ReleaseTemporary(target);
            }
        }
        record.levels = levels.ToArray();
        cache.Add(path, record);
        return record;
    }
    private static Frame RenderFrame(string output, string stem, string name, Camera camera, RenderTexture target,
        GameObject family, GameObject front, GameObject late, Material material, Color background)
    {
        family.GetComponent<MeshRenderer>().sharedMaterial = material;
        family.SetActive(name != "background_a" && name != "background_b" &&
            name != "front_only" && name != "late_only");
        front.SetActive(name == "front_occluder" || name == "front_only");
        late.SetActive(name == "late_behind" || name == "late_only");
        camera.backgroundColor = background;
        RenderTexture prior = RenderTexture.active;
        Texture2D readback = null;
        float before = Time.realtimeSinceStartup;
        try
        {
            camera.Render();
            RenderTexture.active = target;
            readback = new Texture2D(128, 128, TextureFormat.RGBA32, false, false);
            readback.ReadPixels(new Rect(0, 0, 128, 128), 0, 0);
            readback.Apply(false, false);
            string filename = stem + "_" + name + ".png";
            string path = Path.Combine(output, filename);
            File.WriteAllBytes(path, readback.EncodeToPNG());
            Color32 center = readback.GetPixel(64, 64);
            return new Frame { name = name, png = filename, sha256 = Sha(path),
                center_rgba8 = new [] { (int)center.r, (int)center.g, (int)center.b, (int)center.a },
                realtime_before = before, realtime_after = Time.realtimeSinceStartup,
                material_scope = "source_shader_with_explicit_synthetic_RGBA_controls; source_material_not_mutated",
                geometry_scope = "diagnostic_XY_quad; source mesh separately identity-checked",
                depth_setup = name == "front_occluder" || name == "front_only" ?
                    "opaque_green_quad_in_front_z_minus_0_5_queue_2000" :
                    name == "late_behind" || name == "late_only" ?
                    "opaque_green_quad_behind_z_plus_0_5_queue_3100" : "no_control_occluder" };
        }
        finally
        {
            RenderTexture.active = prior;
            if (readback) UnityEngine.Object.DestroyImmediate(readback);
        }
    }
    private static Row Capture(Input input, string output, int index, Dictionary<string, TextureRecord> cache,
        Material mipMaterial)
    {
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(input.source_prefab);
        var sourceMaterial = AssetDatabase.LoadAssetAtPath<Material>(input.source_material);
        Need(prefab && sourceMaterial && sourceMaterial.shader && sourceMaterial.renderQueue == input.source_queue,
            "source prefab/material/queue missing: " + input.source_key);
        string leaf = input.renderer_hint.Replace('\\', '/').Split('/').Last();
        var candidates = prefab.GetComponentsInChildren<Renderer>(true).Where(r => r.name == leaf).ToArray();
        Need(candidates.Length == 1, "source renderer ambiguous: " + input.source_key);
        var renderer = candidates[0];
        var sprite = renderer as SpriteRenderer;
        var mesh = sprite ? null : renderer.GetComponent<MeshFilter>().sharedMesh;
        Need(sprite ? sprite.sprite != null : mesh != null, "source sprite/mesh missing");
        int vertices = sprite ? sprite.sprite.vertices.Length : mesh.vertexCount;
        int triangles = sprite ? sprite.sprite.triangles.Length / 3 : mesh.triangles.Length / 3;
        var colors = sprite ? new Color32[0] : mesh.colors32;
        var row = new Row { source_key = input.source_key, source_material_guid = AssetGuid(input.source_material),
            renderer_file_id_decimal = Id(renderer), source_mesh_file_id_decimal = Id(sprite ? (UnityEngine.Object)sprite.sprite : mesh),
            source_renderer_path = Hierarchy(renderer.transform, prefab.transform), shader_name = sourceMaterial.shader.name,
            source_material_queue = sourceMaterial.renderQueue, source_vertex_count = vertices,
            source_triangle_count = triangles, source_vertex_color_count = colors.Length,
            source_vertex_alpha_min = colors.Length > 0 ? colors.Min(c => (int)c.a) : -1,
            source_vertex_alpha_max = colors.Length > 0 ? colors.Max(c => (int)c.a) : -1,
            source_material_mask_unbound = !sourceMaterial.GetTexture("_Mask"),
            source_material_mask_binding_scope = "Material.GetTexture; per-renderer PropertyBlock separately probed",
            time_scope = "Time.realtimeSinceStartup bracket recorded; actual shader _Time.y not fixed or asserted",
            globals_scope = "diagnostic Shader.SetGlobalColor(_SpriteColor,white) and SetGlobalFloat(_CityTransparency,1); not GameApp" };
        var distinct = new Dictionary<string, Texture2D>();
        foreach (var role in input.roles)
        {
            string extension = Path.GetExtension(role.probe_path).ToLowerInvariant();
            if (extension != ".png" && extension != ".tif" && extension != ".tga" && extension != ".psd" && extension != ".jpg") continue;
            var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(role.probe_path);
            Need(texture != null, "source role texture not imported: " + role.probe_path);
            distinct[role.probe_path] = texture;
        }
        // Material samplers are authoritative even if a role is missing from an older closure.
        for (int i = 0; i < ShaderUtil.GetPropertyCount(sourceMaterial.shader); i++)
        {
            if (ShaderUtil.GetPropertyType(sourceMaterial.shader, i) != ShaderUtil.ShaderPropertyType.TexEnv) continue;
            var texture = sourceMaterial.GetTexture(ShaderUtil.GetPropertyName(sourceMaterial.shader, i)) as Texture2D;
            if (texture) distinct[AssetDatabase.GetAssetPath(texture)] = texture;
        }
        row.textures = distinct.OrderBy(p => p.Key).Select(p => ReadMips(p.Value, output, cache, mipMaterial)).ToArray();
        if (input.source_key == "initial:5")
        {
            row.night_light_time_formula = "clamp(abs(sin(_Time.y*0.5))+0.5,0,1)";
            row.night_light_multiplier_min = 0.5f;
            row.night_light_multiplier_max = 1.0f;
            row.actual_shader_time_observed = false;
            var instance = UnityEngine.Object.Instantiate(prefab);
            try
            {
                var sourceRenderer = instance.GetComponentsInChildren<SpriteRenderer>(true).Single(r => r.name == leaf);
                var component = sourceRenderer.GetComponent<MaskSprite>();
                Need(component != null && component.m_mask_sprite != null, "source MaskSprite/m_mask_sprite did not resolve");
                var method = typeof(MaskSprite).GetMethod("Start", BindingFlags.Instance | BindingFlags.NonPublic);
                Need(method != null, "source MaskSprite.Start unavailable");
                method.Invoke(component, null); // Invoke exact source Start, not a rewritten PropertyBlock substitute.
                var block = new MaterialPropertyBlock(); sourceRenderer.GetPropertyBlock(block);
                row.source_mask_sprite_bound = true;
                row.source_mask_property_block_matches = block.GetTexture("_Mask") == component.m_mask_sprite.texture;
                row.source_mask_sprite_asset_path = AssetDatabase.GetAssetPath(component.m_mask_sprite.texture);
                row.source_mask_sprite_file_id_decimal = Id(component.m_mask_sprite);
                row.source_mask_start_scope = "exact copied MaskSprite.Start invoked by reflection in isolation; GameApp lifecycle not observed";
                Need(row.source_mask_property_block_matches, "source MaskSprite did not bind mask to PropertyBlock");
            }
            finally { UnityEngine.Object.DestroyImmediate(instance); }
        }
        var colorMain = SolidTexture("diagnostic_half_alpha_main", new Color32(205, 83, 41, 128));
        var splat = SolidTexture("diagnostic_splat_red", new Color32(255, 0, 0, 255));
        var maskBlue = SolidTexture("diagnostic_mask_blue", new Color32(255, 0, 255, 255));
        var maskNoBlue = SolidTexture("diagnostic_mask_no_blue", new Color32(255, 0, 0, 255));
        var white = SolidTexture("diagnostic_white_mask", new Color32(255, 255, 255, 255));
        var familyMat = new Material(sourceMaterial) { name = "diagnostic_" + input.source_key };
        var explicitWhite = new Material(familyMat);
        var noBlue = new Material(familyMat);
        Material control = null;
        Camera camera = null;
        RenderTexture target = null;
        Mesh quad = null;
        GameObject family = null, front = null, late = null, cameraObject = null;
        try
        {
            if (input.source_key.StartsWith("ground:", StringComparison.Ordinal))
            {
                for (int i = 0; i < 4; i++) familyMat.SetTexture("_Tex" + i, colorMain);
                familyMat.SetTexture("_Splat", splat);
            }
            else familyMat.SetTexture("_MainTex", colorMain);
            if (input.source_key == "initial:5") familyMat.SetTexture("_Mask", maskBlue);
            // Road intentionally keeps its authored _Mask unbound for the default-white test.
            explicitWhite.CopyPropertiesFromMaterial(familyMat);
            explicitWhite.shader = familyMat.shader;
            explicitWhite.renderQueue = familyMat.renderQueue;
            explicitWhite.SetTexture("_Mask", white);
            noBlue.CopyPropertiesFromMaterial(familyMat);
            noBlue.shader = familyMat.shader;
            noBlue.renderQueue = familyMat.renderQueue;
            noBlue.SetTexture("_Mask", maskNoBlue);
            Shader.SetGlobalColor("_SpriteColor", Color.white);
            Shader.SetGlobalFloat("_CityTransparency", 1.0f);
            target = new RenderTexture(128, 128, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
            target.antiAliasing = 1; target.Create();
            cameraObject = new GameObject("family_probe_camera");
            camera = cameraObject.AddComponent<Camera>(); camera.enabled = false;
            camera.orthographic = true; camera.orthographicSize = 1.25f; camera.aspect = 1;
            camera.nearClipPlane = 0.1f; camera.farClipPlane = 10f;
            camera.transform.position = new Vector3(0, 0, -5);
            camera.transform.rotation = Quaternion.identity;
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.allowHDR = false; camera.allowMSAA = false; camera.targetTexture = target;
            quad = Quad("family_probe_control_quad", input.source_key == "ground:ground_2" ? (byte)128 : (byte)255);
            family = RenderObject("source_shader_diagnostic_quad", quad, familyMat, 0);
            control = new Material(Shader.Find("Unlit/Color"));
            Need(control.shader != null && control.HasProperty("_Color"), "Unity Unlit/Color control unavailable");
            control.color = new Color(0.03f, 0.94f, 0.07f, 1);
            var lateMaterial = new Material(control); lateMaterial.renderQueue = 3100;
            front = RenderObject("front_opaque_control", quad, control, -0.5f);
            late = RenderObject("late_behind_control", quad, lateMaterial, 0.5f);
            Color a = new Color(0.07f, 0.15f, 0.71f, 1);
            Color b = new Color(0.74f, 0.09f, 0.05f, 1);
            string stem = index.ToString("D2", CultureInfo.InvariantCulture) + "_" + input.source_key.Replace(':', '_');
            var frames = new List<Frame>();
            foreach (var name in new [] { "background_a", "background_b", "family_a", "family_b",
                "front_only", "front_occluder", "late_only", "late_behind" })
                frames.Add(RenderFrame(output, stem, name, camera, target, family, front, late, familyMat,
                    name == "background_b" || name == "family_b" ? b : a));
            if (input.source_key == "support:boundary_support_road")
                frames.Add(RenderFrame(output, stem, "road_explicit_white", camera, target, family, front, late, explicitWhite, a));
            if (input.source_key == "initial:5")
                frames.Add(RenderFrame(output, stem, "night_mask_b_zero", camera, target, family, front, late, noBlue, a));
            row.frames = frames.ToArray();
            UnityEngine.Object.DestroyImmediate(lateMaterial);
        }
        finally
        {
            if (family) UnityEngine.Object.DestroyImmediate(family);
            if (front) UnityEngine.Object.DestroyImmediate(front);
            if (late) UnityEngine.Object.DestroyImmediate(late);
            if (cameraObject) UnityEngine.Object.DestroyImmediate(cameraObject);
            if (quad) UnityEngine.Object.DestroyImmediate(quad);
            if (target) { target.Release(); UnityEngine.Object.DestroyImmediate(target); }
            if (control) UnityEngine.Object.DestroyImmediate(control);
            UnityEngine.Object.DestroyImmediate(familyMat);
            UnityEngine.Object.DestroyImmediate(explicitWhite);
            UnityEngine.Object.DestroyImmediate(noBlue);
            UnityEngine.Object.DestroyImmediate(colorMain);
            UnityEngine.Object.DestroyImmediate(splat);
            UnityEngine.Object.DestroyImmediate(maskBlue);
            UnityEngine.Object.DestroyImmediate(maskNoBlue);
            UnityEngine.Object.DestroyImmediate(white);
        }
        return row;
    }
    public static void Run()
    {
        string assets = Normal(Application.dataPath);
        Need(assets.EndsWith(ProbeSuffix, StringComparison.OrdinalIgnoreCase) &&
            Application.unityVersion == "2019.4.28f1c1" && QualitySettings.activeColorSpace == ColorSpace.Gamma &&
            SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D11,
            "wrong isolated Unity2019 Gamma/D3D11 target");
        string probe = assets.Substring(0, assets.Length - "/Assets".Length);
        string repo = assets.Substring(0, assets.Length - ProbeSuffix.Length);
        string contractPath = Path.Combine(probe, "state_contract.json");
        var contract = JsonUtility.FromJson<Contract>(File.ReadAllText(contractPath, Encoding.UTF8));
        Need(contract != null && contract.schema == "rok.city_gamma_family_state_contract.v2" &&
            Normal(contract.repo_root) == repo && Normal(contract.probe_root) == probe &&
            Sha(Path.Combine(repo, "Scripts/ExportRokCityGammaFamilyStateProbe.cs")) == contract.exporter_sha256 &&
            Sha(Path.Combine(probe, "Assets/Editor/ExportRokCityGammaFamilyStateProbe.cs")) == contract.exporter_sha256 &&
            Sha(Path.Combine(probe, "Assets/ProbeDependencyStubs.cs")) == contract.stub_sha256 &&
            Sha(Path.Combine(probe, "Assets/ProbeMipReadback.shader")) == contract.mip_shader_sha256 &&
            contract.rows != null && contract.rows.Length == 5, "state source/exporter contract differs");
        foreach (var file in contract.source_files)
        {
            string original = Path.Combine(repo, file.source_path);
            string copy = Path.Combine(probe, file.probe_path);
            Need(file.probe_path.StartsWith("Assets/Source/", StringComparison.Ordinal) &&
                Sha(original) == file.source_sha256 && Sha(copy) == file.source_sha256 &&
                Sha(original + ".meta") == file.meta_sha256 && Sha(copy + ".meta") == file.meta_sha256 &&
                AssetGuid(file.probe_path) == file.source_guid, "copied source/GUID changed: " + file.source_path);
        }
        string[] keys = { "initial:5", "ground:ground_1", "ground:ground_2",
            "support:boundary_support_road", "support:boundary_support_wall_shadow" };
        Need(contract.rows.Select(r => r.source_key).SequenceEqual(keys), "five-family allowlist differs");
        string outputDir = Path.Combine(probe, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff") +
            "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        Need(!Directory.Exists(outputDir), "native output run already exists");
        Directory.CreateDirectory(outputDir);
        var output = new Output { run_directory = Normal(outputDir), unity_version = Application.unityVersion,
            color_space = QualitySettings.activeColorSpace.ToString(), graphics_device = SystemInfo.graphicsDeviceType.ToString(),
            source_contract_sha256 = Sha(contractPath), exporter_sha256 = contract.exporter_sha256,
            scope = "isolated_controlled_gpu_state_only", game_app_loaded = false };
        Material mipMaterial = null;
        try
        {
            var mipShader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/ProbeMipReadback.shader");
            Need(mipShader && mipShader.name == "Hidden/RokCityFamilyMipReadback" && mipShader.isSupported,
                "isolated explicit-LOD GPU readback shader unavailable");
            mipMaterial = new Material(mipShader);
            var cache = new Dictionary<string, TextureRecord>();
            var rows = new List<Row>();
            for (int i = 0; i < contract.rows.Length; i++)
                rows.Add(Capture(contract.rows[i], outputDir, i, cache, mipMaterial));
            output.rows = rows.ToArray(); output.complete = true;
            File.WriteAllText(Path.Combine(outputDir, "native_state.json"), JsonUtility.ToJson(output, true), new UTF8Encoding(false));
            Debug.Log("ROK_CITY_GAMMA_FAMILY_STATE_NATIVE_OUTPUT " + outputDir);
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            output.complete = false; output.failures = error.ToString();
            File.WriteAllText(Path.Combine(outputDir, "native_state.json"), JsonUtility.ToJson(output, true), new UTF8Encoding(false));
            File.WriteAllText(Path.Combine(outputDir, "failure.txt"), error.ToString(), new UTF8Encoding(false));
            Debug.LogException(error); EditorApplication.Exit(1);
        }
        finally { if (mipMaterial) UnityEngine.Object.DestroyImmediate(mipMaterial); }
    }
}
