using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

// Isolated Hall source-material fixture. No GameApp MonoBehaviour is executed.
public static class RenderRokUnityCityHallCameraMaterialNativeV7
{
    [Serializable] private sealed class FilePin { public string path, sha256; }
    [Serializable] private sealed class Contract
    {
        public string schema, probe_root, exporter_sha256, time_shader_sha256, qualifier;
        public string diagnostic_day_sha256, diagnostic_night_sha256;
        public string hall_prefab_asset, source_renderer_guid, source_renderer_file_id;
        public string sprite_guid, mask_guid, day_material_asset, night_material_asset;
        public string source_input_path, source_input_sha256, v1_native_manifest_path, v1_native_manifest_sha256;
        public FilePin[] v1_case_raw;
        public FilePin[] input_files;
    }
    [Serializable] private sealed class Input
    {
        public string schema, hall_world_matrix_path, native_camera_path;
        public string positions_path, uv0_path, indices_path;
        public string camera_world_to_camera_path, camera_projection_path, camera_gpu_projection_rt_path;
        public int width, height;
        public float[] camera_position_xyz, camera_rotation_xyzw, sprite_color_rgba;
        public float fov, near_clip, far_clip, city_transparency, expected_shader_time_y;
        public string[] phase_order;
    }
    [Serializable] private sealed class Artifact { public string path, sha256; public int byte_count; }
    [Serializable] private sealed class TextureProbe
    {
        public string role, asset_path, guid, format_before, format_after, filter_before, filter_after;
        public string wrap_before, wrap_after;
        public string source_meta_sha256, project_meta_after_sha256;
        public bool readable_before, readable_after, srgb_before, srgb_after;
        public int width_before, height_before, mips_before, width_after, height_after, mips_after, block_bytes;
        public Artifact compressed_blocks, importer_before_json, importer_after_json;
    }
    [Serializable] private sealed class Case
    {
        public string phase, material_asset, material_shader, background, source_sprite_asset, source_mask_asset;
        public string main_texture_format, mask_texture_format, main_filter, mask_filter, main_wrap, mask_wrap;
        public bool main_srgb, mask_srgb, property_block_mask_matches, renderer_enabled, active_in_hierarchy;
        public int render_queue, sorting_layer_id, sorting_order, sprite_vertex_count, sprite_triangle_count;
        public int main_width, main_height, main_mips, mask_width, mask_height, mask_mips;
        public int changed_rgb_pixels, blended_rgb_pixels;
        public float[] renderer_color, sprite_color_global, gpu_shader_time_before, gpu_shader_time_after;
        public float city_transparency_global;
        public Artifact raw_top_left_rgba8, png, shader_time_before_f32le, shader_time_after_f32le;
    }
    [Serializable] private sealed class Output
    {
        public string schema = "rok.unity_city_hall_camera_material_native.v7";
        public string contract_sha256, input_sha256, exporter_sha256, time_shader_sha256;
        public string graphics_device, color_space, qualifier, target_format, readback_row_order, source_camera_scope;
        public bool complete, game_app_loaded, source_behaviors_executed, lod_qualified, main_view_qualified;
        public int width, height, anti_aliasing, case_count;
        public float[] camera_position_xyz, camera_rotation_xyzw, sprite_color_global;
        public float camera_fov, camera_aspect, camera_near, camera_far, city_transparency_global;
        public string renderer_world_matrix_sha256, sprite_guid, mask_guid, source_renderer_file_id;
        public Artifact actual_renderer_world_matrix;
        public TextureProbe main_texture, mask_texture;
        public int v1_raw_exact_cases;
        public int clone_raw_exact_cases;
        public bool float_diagnostic_hdr, packed_source_time_zero_bound;
        public string float_diagnostic_target_format, packed_target_format, packed_readback_scope;
        public Artifact float_diagnostic_mode0_top_left_rgba32f;
        public int float_diagnostic_nonclear_rgb_pixels;
        public string diagnostic_shader_qualification;
        public float gpu_object_vs_csharp_max_abs, gpu_vp_vs_source_derived_max_abs;
        public Artifact gpu_object_to_world_rows_f32le, gpu_view_projection_rows_f32le;
        public PackedWitness[] packed_witnesses;
        public Artifact camera_world_to_camera, camera_projection, camera_gpu_projection_rt;
        public Case[] cases;
        public Case[] cloned_cases;
    }
    [Serializable] private sealed class PackedWitness
    {
        public string phase, semantic; public int mode, x, y;
        public int[] rgba8; public float decoded_float; public long decoded_bits_u32; public Artifact raw_rgba8;
        public Artifact full_top_left_rgba8, full_png;
    }
    private static void Need(bool ok, string why) { if (!ok) throw new InvalidOperationException(why); }
    private static string Normal(string path) { return Path.GetFullPath(path).Replace('\\', '/').TrimEnd('/'); }
    private static string Sha(byte[] data)
    {
        using (var digest = SHA256.Create()) return BitConverter.ToString(digest.ComputeHash(data)).Replace("-", "").ToLowerInvariant();
    }
    private static string ShaFile(string path) { return Sha(File.ReadAllBytes(path)); }
    private static float[] V7(Vector3 value) { return new[] { value.x, value.y, value.z }; }
    private static float[] Q(Quaternion value) { return new[] { value.x, value.y, value.z, value.w }; }
    private static float[] C(Color value) { return new[] { value.r, value.g, value.b, value.a }; }
    private static Vector3 V7(float[] value) { Need(value != null && value.Length == 3, "Vector3 input absent"); return new Vector3(value[0], value[1], value[2]); }
    private static Quaternion Q(float[] value) { Need(value != null && value.Length == 4, "Quaternion input absent"); return new Quaternion(value[0], value[1], value[2], value[3]); }
    private static Color C(float[] value) { Need(value != null && value.Length == 4, "Color input absent"); return new Color(value[0], value[1], value[2], value[3]); }
    private static Matrix4x4 ReadMatrix(string path)
    {
        Need(File.Exists(path) && new FileInfo(path).Length == 64 && BitConverter.IsLittleEndian, "Native3 matrix absent");
        var value = new Matrix4x4();
        using (var reader = new BinaryReader(File.OpenRead(path)))
            for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) value[r, c] = reader.ReadSingle();
        return value;
    }
    private static Artifact SaveMatrix(string run, string name, Matrix4x4 matrix)
    {
        byte[] bytes = new byte[64];
        for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col)
            Buffer.BlockCopy(BitConverter.GetBytes(matrix[row, col]), 0, bytes, 4 * (row * 4 + col), 4);
        return Write(run, "matrices/" + name + ".f32le", bytes);
    }
    private static void MatrixNear(Matrix4x4 actual, Matrix4x4 expected, float tolerance, string why)
    {
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
            Need(Mathf.Abs(actual[r, c] - expected[r, c]) <= tolerance, why + " [" + r + "," + c + "]");
    }
    private static Artifact Write(string run, string name, byte[] data)
    {
        string destination = Path.Combine(run, name);
        Directory.CreateDirectory(Path.GetDirectoryName(destination));
        using (var stream = new FileStream(destination, FileMode.CreateNew)) stream.Write(data, 0, data.Length);
        return new Artifact { path = name.Replace('\\', '/'), sha256 = Sha(data), byte_count = data.Length };
    }
    private static TextureProbe MakeReadableAndExport(string project, string run, string role, string assetPath,
                                                       string expectedGuid, TextureFormat expectedFormat, int blockBytes)
    {
        string guid = AssetDatabase.AssetPathToGUID(assetPath);
        Need(guid == expectedGuid, "Compressed texture source GUID differs: " + role);
        var importer = AssetImporter.GetAtPath(assetPath) as TextureImporter;
        var original = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
        Need(importer != null && original != null && original.format == expectedFormat &&
             !importer.isReadable && original.mipmapCount == 1, "Original compressed import state differs: " + role);
        string metaPath = project + "/" + assetPath + ".meta";
        byte[] metaBefore = File.ReadAllBytes(metaPath);
        var probe = new TextureProbe {
            role = role, asset_path = assetPath, guid = guid, format_before = original.format.ToString(),
            filter_before = original.filterMode.ToString(), wrap_before = original.wrapMode.ToString(),
            readable_before = importer.isReadable, srgb_before = importer.sRGBTexture,
            width_before = original.width, height_before = original.height, mips_before = original.mipmapCount,
            source_meta_sha256 = Sha(metaBefore), block_bytes = blockBytes,
            importer_before_json = Write(run, "textures/" + role + "_importer_before.json",
                                         Encoding.UTF8.GetBytes(EditorJsonUtility.ToJson(importer, true) + "\n"))
        };
        importer.isReadable = true; // Isolated V2 copy only; never mutate ref or V1.
        importer.SaveAndReimport();
        importer = AssetImporter.GetAtPath(assetPath) as TextureImporter;
        var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
        Need(importer != null && texture != null && importer.isReadable && texture.format == expectedFormat &&
             texture.mipmapCount == 1 && texture.width == probe.width_before && texture.height == probe.height_before &&
             texture.filterMode.ToString() == probe.filter_before && texture.wrapMode.ToString() == probe.wrap_before &&
             importer.sRGBTexture == probe.srgb_before, "Readable reimport changed source texture sampling state: " + role);
        byte[] blocks = texture.GetRawTextureData();
        int expectedLength = ((texture.width + 3) / 4) * ((texture.height + 3) / 4) * blockBytes;
        Need(blocks != null && blocks.Length == expectedLength, "Compressed block bytes/size differ: " + role);
        probe.format_after = texture.format.ToString(); probe.filter_after = texture.filterMode.ToString();
        probe.wrap_after = texture.wrapMode.ToString(); probe.readable_after = importer.isReadable;
        probe.srgb_after = importer.sRGBTexture; probe.width_after = texture.width; probe.height_after = texture.height;
        probe.mips_after = texture.mipmapCount; probe.project_meta_after_sha256 = ShaFile(metaPath);
        probe.importer_after_json = Write(run, "textures/" + role + "_importer_after.json",
                                          Encoding.UTF8.GetBytes(EditorJsonUtility.ToJson(importer, true) + "\n"));
        probe.compressed_blocks = Write(run, "textures/" + role + ".dxt_blocks.bin", blocks);
        return probe;
    }
    private static byte[] FlipRows(byte[] raw, int width, int height)
    {
        var top = new byte[raw.Length];
        for (int y = 0; y < height; ++y) Buffer.BlockCopy(raw, (height - 1 - y) * width * 4, top, y * width * 4, width * 4);
        return top;
    }
    private static byte[] ReadRgba(RenderTexture target, int width, int height, out byte[] png)
    {
        var previous = RenderTexture.active;
        var readback = new Texture2D(width, height, TextureFormat.RGBA32, false, true);
        try {
            RenderTexture.active = target;
            readback.ReadPixels(new Rect(0, 0, width, height), 0, 0, false);
            readback.Apply(false, false);
            byte[] raw = readback.GetRawTextureData();
            Need(raw.Length == width * height * 4, "RGBA8 readback length differs");
            png = readback.EncodeToPNG();
            Need(png != null && png.Length > 32, "Unity PNG encoder failed");
            return FlipRows(raw, width, height);
        } finally { RenderTexture.active = previous; UnityEngine.Object.DestroyImmediate(readback); }
    }
    private static Artifact ReadTime(RenderTexture target, string run, string path, out float[] values)
    {
        var previous = RenderTexture.active;
        var readback = new Texture2D(1, 1, TextureFormat.RGBAFloat, false, true);
        try {
            RenderTexture.active = target;
            readback.ReadPixels(new Rect(0, 0, 1, 1), 0, 0, false);
            byte[] raw = readback.GetRawTextureData();
            Need(raw.Length == 16, "GPU time readback requires RGBAFloat 16B");
            values = new[] { BitConverter.ToSingle(raw, 0), BitConverter.ToSingle(raw, 4), BitConverter.ToSingle(raw, 8), BitConverter.ToSingle(raw, 12) };
            return Write(run, path, raw);
        } finally { RenderTexture.active = previous; UnityEngine.Object.DestroyImmediate(readback); }
    }
    private static float MatrixMaxAbs(byte[] actualRows, Matrix4x4 expected)
    {
        Need(actualRows.Length == 64, "GPU matrix witness size differs");
        float maximum = 0f;
        for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col)
            maximum = Mathf.Max(maximum, Mathf.Abs(BitConverter.ToSingle(actualRows, 4*(row*4+col)) - expected[row,col]));
        return maximum;
    }
    private static PackedWitness RenderPackedWitness(int mode, string semantic, string phase, int x, int y,
                                                     Material material, SpriteRenderer renderer, Texture2D mask,
                                                     Camera camera, RenderTexture target, string run)
    {
        Need(x >= 0 && x < target.width && y >= 0 && y < target.height &&
             (semantic == "matrix" || semantic == "pixel" || semantic == "sentinel"),
             "Packed witness coordinate/semantic differs");
        renderer.material = material;
        var property = new MaterialPropertyBlock();
        renderer.GetPropertyBlock(property); property.SetTexture("_Mask", mask); renderer.SetPropertyBlock(property);
        renderer.GetPropertyBlock(property);
        Need(property.GetTexture("_Mask") == mask, "Packed witness MaskSprite binding differs");
        material.SetFloat("_RokProbeMode", mode);
        material.SetFloat("_RokSrcBlend", 1f); material.SetFloat("_RokDstBlend", 0f);
        Shader.SetGlobalFloat("_RokProbeMode", mode);
        camera.backgroundColor = Color.black;
        camera.Render();
        RenderTexture previous = RenderTexture.active;
        var readback = new Texture2D(target.width, target.height, TextureFormat.RGBA32, false, true);
        try {
            RenderTexture.active = target;
            readback.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0, false);
            byte[] full = readback.GetRawTextureData();
            Need(full != null && full.Length == target.width * target.height * 4,
                 "Packed GPU full-frame RGBA8 readback length differs");
            var raw = new byte[4];
            Buffer.BlockCopy(full, ((target.height - 1 - y) * target.width + x) * 4, raw, 0, 4);
            Artifact fullRaw = null, fullPng = null;
            if (semantic == "sentinel" && phase == "day_black" && y == 127) {
                fullRaw = Write(run, "packed_first_sentinel.top_left.rgba8.bin",
                                FlipRows(full, target.width, target.height));
                readback.Apply(false, false);
                byte[] png = readback.EncodeToPNG();
                Need(png != null && png.Length > 32, "Packed first-sentinel PNG encoder failed");
                fullPng = Write(run, "packed_first_sentinel.png", png);
            }
            float value = BitConverter.ToSingle(raw, 0);
            Need(!float.IsNaN(value) && !float.IsInfinity(value), "Packed GPU witness nonfinite");
            string path = "packed/" + phase + "_mode" + mode.ToString("D2", CultureInfo.InvariantCulture) +
                          "_x" + x + "_y" + y + ".rgba8.bin";
            Artifact artifact = Write(run, path, raw);
            if (semantic == "sentinel")
                Need(raw.SequenceEqual(BitConverter.GetBytes(0.75f)),
                     "Packed source-Hall draw/Blend One Zero/byte channels absent at known covered pixel");
            return new PackedWitness { phase = phase, semantic = semantic, mode = mode, x = x, y = y,
                                       rgba8 = raw.Select(v => (int)v).ToArray(), decoded_float = value,
                                       decoded_bits_u32 = (long)BitConverter.ToUInt32(raw, 0),
                                       raw_rgba8 = artifact, full_top_left_rgba8 = fullRaw, full_png = fullPng };
        } finally { RenderTexture.active = previous; UnityEngine.Object.DestroyImmediate(readback); }
    }
    private static Artifact ReadFloatDiagnostic(RenderTexture target, string run, out int nonclear)
    {
        RenderTexture previous = RenderTexture.active;
        var readback = new Texture2D(target.width, target.height, TextureFormat.RGBAFloat, false, true);
        try {
            RenderTexture.active = target;
            readback.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0, false);
            byte[] bottom = readback.GetRawTextureData();
            Need(bottom != null && bottom.Length == target.width * target.height * 16,
                 "Full Float RT diagnostic byte length differs");
            var top = new byte[bottom.Length]; nonclear = 0;
            for (int y = 0; y < target.height; ++y)
                Buffer.BlockCopy(bottom, (target.height - 1 - y) * target.width * 16,
                                 top, y * target.width * 16, target.width * 16);
            for (int i = 0; i < top.Length; i += 16)
                if (Mathf.Abs(BitConverter.ToSingle(top, i)) > 0.01f ||
                    Mathf.Abs(BitConverter.ToSingle(top, i+4)) > 0.01f ||
                    Mathf.Abs(BitConverter.ToSingle(top, i+8)) > 0.01f) nonclear++;
            return Write(run, "float_diagnostic_mode0.top_left.rgba32f.bin", top);
        } finally { RenderTexture.active = previous; UnityEngine.Object.DestroyImmediate(readback); }
    }
    private static Mesh Quad()
    {
        var mesh = new Mesh { name = "time_witness_only_not_source_geometry" };
        mesh.vertices = new[] { new Vector3(-1,-1,0), new Vector3(1,-1,0), new Vector3(1,1,0), new Vector3(-1,1,0) };
        mesh.triangles = new[] { 0,1,2,0,2,3 };
        return mesh;
    }
    private static CommandBuffer TimeCommand(Camera camera, CameraEvent at, RenderTexture target, Material material, Mesh quad, bool setZero)
    {
        var commands = new CommandBuffer { name = "Hall source shader _Time GPU witness " + at };
        if (setZero) commands.SetGlobalVector("_Time", Vector4.zero);
        commands.SetRenderTarget(target);
        commands.ClearRenderTarget(false, true, Color.magenta);
        commands.DrawMesh(quad, Matrix4x4.identity, material, 0, 0);
        commands.SetRenderTarget(BuiltinRenderTextureType.CameraTarget);
        camera.AddCommandBuffer(at, commands);
        return commands;
    }
    private static SpriteRenderer ExactRenderer(GameObject source, Contract contract)
    {
        var renderers = source.GetComponentsInChildren<SpriteRenderer>(true);
        SpriteRenderer matched = null;
        foreach (var renderer in renderers) {
            string guid; long fileId;
            if (AssetDatabase.TryGetGUIDAndLocalFileIdentifier(renderer, out guid, out fileId) &&
                guid == contract.source_renderer_guid && fileId.ToString(CultureInfo.InvariantCulture) == contract.source_renderer_file_id) {
                Need(matched == null, "Duplicate renderer fileID"); matched = renderer;
            }
        }
        Need(matched != null, "Exact source renderer GUID/fileID absent");
        return matched;
    }
    private static void CompareSourceGeometry(Sprite sprite, string project, Input input)
    {
        Need(sprite.vertices.Length == 240 && sprite.uv.Length == 240 && sprite.triangles.Length == 714,
             "Source Hall Sprite mesh counts differ");
        byte[] position = File.ReadAllBytes(project + "/" + input.positions_path);
        byte[] uv = File.ReadAllBytes(project + "/" + input.uv0_path);
        byte[] index = File.ReadAllBytes(project + "/" + input.indices_path);
        Need(position.Length == 240 * 12 && uv.Length == 240 * 16 && index.Length == 714 * 4,
             "Source Hall typed geometry byte sizes differ");
        for (int i = 0; i < 240; ++i) {
            int p = i * 12, u = i * 16;
            Vector2 vertex = sprite.vertices[i], tex = sprite.uv[i];
            Need(Mathf.Abs(vertex.x - BitConverter.ToSingle(position, p)) < 1e-6f &&
                 Mathf.Abs(vertex.y - BitConverter.ToSingle(position, p + 4)) < 1e-6f &&
                 BitConverter.ToSingle(position, p + 8) == 0f &&
                 Mathf.Abs(tex.x - BitConverter.ToSingle(uv, u)) < 1e-6f &&
                 Mathf.Abs(tex.y - BitConverter.ToSingle(uv, u + 4)) < 1e-6f &&
                 BitConverter.ToSingle(uv, u + 8) == 0f && BitConverter.ToSingle(uv, u + 12) == 0f,
                 "Imported Sprite vertex/UV differs from pinned Hall row27: " + i);
        }
        for (int i = 0; i < 714; ++i)
            Need(sprite.triangles[i] == BitConverter.ToUInt32(index, i * 4),
                 "Imported Sprite triangle differs from pinned Hall row27: " + i);
    }
    private static Case RenderCase(string phase, string prefix, string background, Material material, SpriteRenderer renderer,
                                   Texture2D mask, Camera camera, RenderTexture target, RenderTexture before,
                                   RenderTexture after, string run, Input input)
    {
        bool night = phase.StartsWith("night_", StringComparison.Ordinal);
        bool white = background == "white";
        camera.backgroundColor = white ? Color.white : Color.black;
        renderer.material = material; // Source MaskSprite.UpdatedMaterial assigns material, then rebinds MPB.
        var property = new MaterialPropertyBlock();
        renderer.GetPropertyBlock(property);
        property.SetTexture("_Mask", mask);
        renderer.SetPropertyBlock(property);
        renderer.GetPropertyBlock(property);
        Need(property.GetTexture("_Mask") == mask && renderer.sprite != null && renderer.sprite.texture != null,
             "Source-equivalent MaskSprite property block failed");
        Shader.SetGlobalColor("_SpriteColor", C(input.sprite_color_rgba));
        Shader.SetGlobalFloat("_CityTransparency", input.city_transparency);
        camera.Render();
        byte[] png;
        byte[] raw = ReadRgba(target, input.width, input.height, out png);
        float[] timeBefore, timeAfter;
        var t0 = ReadTime(before, run, prefix + "_time_before.rgba32f.bin", out timeBefore);
        var t1 = ReadTime(after, run, prefix + "_time_after.rgba32f.bin", out timeAfter);
        Need(timeBefore[0] == input.expected_shader_time_y && timeAfter[0] == input.expected_shader_time_y &&
             timeBefore[3] == 1f && timeAfter[3] == 1f && t0.sha256 == t1.sha256,
             "Source shader GPU _Time.y not fixed over this camera draw");
        int changed = 0, blended = 0;
        byte channel = white ? (byte)255 : (byte)0;
        for (int i = 0; i < raw.Length; i += 4) {
            if (raw[i] != channel || raw[i+1] != channel || raw[i+2] != channel) changed++;
            if ((raw[i] > 0 && raw[i] < 255) || (raw[i+1] > 0 && raw[i+1] < 255) ||
                (raw[i+2] > 0 && raw[i+2] < 255)) blended++;
        }
        Need(changed > 100 && blended > 10, "Hall source shader produced no visible blended coverage");
        var main = renderer.sprite.texture;
        string mainPath = AssetDatabase.GetAssetPath(renderer.sprite);
        string maskPath = AssetDatabase.GetAssetPath(mask);
        var mainImporter = AssetImporter.GetAtPath(mainPath) as TextureImporter;
        var maskImporter = AssetImporter.GetAtPath(maskPath) as TextureImporter;
        Need(mainImporter != null && maskImporter != null, "Source TextureImporter absent");
        return new Case {
            phase = phase, material_asset = AssetDatabase.GetAssetPath(material), material_shader = material.shader.name,
            background = background, source_sprite_asset = mainPath, source_mask_asset = maskPath,
            main_texture_format = main.format.ToString(), mask_texture_format = mask.format.ToString(),
            main_filter = main.filterMode.ToString(), mask_filter = mask.filterMode.ToString(),
            main_wrap = main.wrapMode.ToString(), mask_wrap = mask.wrapMode.ToString(),
            main_srgb = mainImporter.sRGBTexture, mask_srgb = maskImporter.sRGBTexture,
            render_queue = material.renderQueue, sorting_layer_id = renderer.sortingLayerID,
            sorting_order = renderer.sortingOrder, sprite_vertex_count = renderer.sprite.vertices.Length,
            sprite_triangle_count = renderer.sprite.triangles.Length / 3,
            main_width = main.width, main_height = main.height, main_mips = main.mipmapCount,
            mask_width = mask.width, mask_height = mask.height, mask_mips = mask.mipmapCount,
            property_block_mask_matches = true, renderer_enabled = renderer.enabled,
            active_in_hierarchy = renderer.gameObject.activeInHierarchy, changed_rgb_pixels = changed,
            blended_rgb_pixels = blended, renderer_color = C(renderer.color),
            sprite_color_global = C(Shader.GetGlobalColor("_SpriteColor")),
            city_transparency_global = Shader.GetGlobalFloat("_CityTransparency"),
            gpu_shader_time_before = timeBefore, gpu_shader_time_after = timeAfter,
            raw_top_left_rgba8 = Write(run, prefix + ".top_left.rgba8.bin", raw),
            png = Write(run, prefix + ".png", png),
            shader_time_before_f32le = t0, shader_time_after_f32le = t1
        };
    }
    private static void RunInner()
    {
        string contractPath = null;
        var args = Environment.GetCommandLineArgs();
        for (int i = 0; i + 1 < args.Length; ++i) if (args[i] == "-probeContract") contractPath = args[i + 1];
        string project = Normal(Path.Combine(Application.dataPath, ".."));
        Need(!String.IsNullOrEmpty(contractPath), "Explicit -probeContract required");
        contractPath = Normal(contractPath);
        Need(contractPath == project + "/hall_camera_material_contract_v7.json", "Wrong isolated contract path");
        byte[] contractBytes = File.ReadAllBytes(contractPath);
        Contract contract = JsonUtility.FromJson<Contract>(Encoding.UTF8.GetString(contractBytes));
        Need(contract != null && contract.schema == "rok.hall_camera_material_contract.v7" &&
             contract.qualifier == "source_camera_material_instrumented_diagnostic_only; original and clone mode0 must byteexact V1" &&
             Normal(contract.probe_root) == project,
             "Contract/project identity differs");
        string exporter = project + "/Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV7.cs";
        string timeShader = project + "/Assets/ProbeHallTimeWitness.shader";
        string diagnosticDay = project + "/Assets/ProbeHallDiagnosticDayV7.shader";
        string diagnosticNight = project + "/Assets/ProbeHallDiagnosticNightV7.shader";
        Need(ShaFile(exporter) == contract.exporter_sha256 && ShaFile(timeShader) == contract.time_shader_sha256 &&
             ShaFile(diagnosticDay) == contract.diagnostic_day_sha256 &&
             ShaFile(diagnosticNight) == contract.diagnostic_night_sha256,
             "Exporter/diagnostic/time shader source hash differs");
        foreach (var pin in contract.input_files) {
            Need(!String.IsNullOrEmpty(pin.path) && !pin.path.Contains("..") && !Path.IsPathRooted(pin.path), "Unsafe pinned input");
            Need(ShaFile(project + "/" + pin.path) == pin.sha256, "Pinned probe input differs: " + pin.path);
        }
        Need(!String.IsNullOrEmpty(contract.v1_native_manifest_path) &&
             ShaFile(contract.v1_native_manifest_path) == contract.v1_native_manifest_sha256 &&
             contract.v1_case_raw != null && contract.v1_case_raw.Length == 4,
             "Frozen V1 oracle identity differs");
        string inputPath = project + "/" + contract.source_input_path;
        Need(ShaFile(inputPath) == contract.source_input_sha256, "Typed source input differs");
        Input input = JsonUtility.FromJson<Input>(File.ReadAllText(inputPath));
        Need(input != null && input.schema == "rok.hall_camera_material_source_input.v1" &&
             input.width == 1440 && input.height == 810 && input.fov == 7.5f &&
             input.near_clip == 2f && input.far_clip == 3350f &&
             input.city_transparency == 1f && input.expected_shader_time_y == 0f &&
             input.phase_order != null && input.phase_order.SequenceEqual(new[] { "day_black", "day_white", "night_black", "night_white" }),
             "Typed fixture values differ");
        Need(SystemInfo.graphicsDeviceType.ToString() == "Direct3D11" && QualitySettings.activeColorSpace == ColorSpace.Gamma &&
             SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.ARGBFloat), "Unity D3D11/Gamma/float target required");
        var hallWorld = ReadMatrix(project + "/" + input.hall_world_matrix_path);
        var cameraNative = File.ReadAllText(project + "/" + input.native_camera_path);
        Need(cameraNative.Contains("\"complete\": true") || cameraNative.Contains("\"complete\":true"), "NativeCamera2 input incomplete");
        string run = Path.Combine(project, "OutputRuns", DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff", CultureInfo.InvariantCulture) + "_" + System.Diagnostics.Process.GetCurrentProcess().Id);
        Need(!Directory.Exists(run) && !File.Exists(run), "Run path exists");
        Directory.CreateDirectory(run);
        GameObject root = null, land = null, container = null, hall = null, cameraObject = null;
        RenderTexture target = null, before = null, after = null, floatTarget = null;
        Material witnessMaterial = null, probeDay = null, probeNight = null; Mesh witnessQuad = null;
        CommandBuffer timeBeforeCommands = null, timeAfterCommands = null, floatTimeCommands = null;
        try {
            const string hallTextureDir = "Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/";
            TextureProbe mainProbe = MakeReadableAndExport(project, run, "main", hallTextureDir + "build_1_101_1.png",
                                                            contract.sprite_guid, TextureFormat.DXT5, 16);
            TextureProbe maskProbe = MakeReadableAndExport(project, run, "mask", hallTextureDir + "build_1_101_1_mask.png",
                                                            contract.mask_guid, TextureFormat.DXT1, 8);
            var source = AssetDatabase.LoadAssetAtPath<GameObject>(contract.hall_prefab_asset);
            var containerSource = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/BundleAssets/Scene/City/city_function/CityBuildingContainer.prefab");
            var day = AssetDatabase.LoadAssetAtPath<Material>(contract.day_material_asset);
            var night = AssetDatabase.LoadAssetAtPath<Material>(contract.night_material_asset);
            var maskSprite = AssetDatabase.LoadAssetAtPath<Sprite>("Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1_mask.png");
            var witnessShader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/ProbeHallTimeWitness.shader");
            var probeDayShader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/ProbeHallDiagnosticDayV7.shader");
            var probeNightShader = AssetDatabase.LoadAssetAtPath<Shader>("Assets/ProbeHallDiagnosticNightV7.shader");
            Need(source && containerSource && day && night && maskSprite && witnessShader && witnessShader.isSupported,
                 "Source prefab/material/mask/time shader missing");
            Need(probeDayShader && probeNightShader && probeDayShader.isSupported && probeNightShader.isSupported,
                 "Diagnostic source shader clones unsupported");
            Need(day.shader != null && night.shader != null && day.shader.isSupported && night.shader.isSupported,
                 "Original day/night source shader unsupported on D3D11");
            SpriteRenderer sourceRenderer = ExactRenderer(source, contract);
            string spriteGuid, maskGuid; long spriteId, maskId;
            bool spriteIdentified = AssetDatabase.TryGetGUIDAndLocalFileIdentifier(sourceRenderer.sprite, out spriteGuid, out spriteId);
            bool maskIdentified = AssetDatabase.TryGetGUIDAndLocalFileIdentifier(maskSprite, out maskGuid, out maskId);
            Need(spriteIdentified && maskIdentified && spriteGuid == contract.sprite_guid &&
                 maskGuid == contract.mask_guid && spriteId == 21300000 && maskId == 21300000,
                 "Source sprite/mask GUID/fileID differs");
            Need(sourceRenderer.sharedMaterial == night && sourceRenderer.color == Color.white &&
                 sourceRenderer.sprite.vertices.Length == 240 && sourceRenderer.sprite.triangles.Length == 714,
                 "Original Hall renderer/material/mesh differs");
            CompareSourceGeometry(sourceRenderer.sprite, project, input);
            root = new GameObject("SceneObject"); land = new GameObject("land_root");
            land.transform.SetParent(root.transform, false);
            container = (GameObject)PrefabUtility.InstantiatePrefab(containerSource);
            container.transform.SetParent(land.transform);
            container.transform.localPosition = Vector3.zero;
            container.transform.localEulerAngles = new Vector3(0, -45, 0);
            hall = (GameObject)PrefabUtility.InstantiatePrefab(source);
            hall.transform.SetParent(container.transform);
            hall.transform.localPosition = new Vector3(-.1f, .01f, .9f);
            hall.transform.localEulerAngles = Vector3.zero;
            SpriteRenderer renderer = hall.GetComponentsInChildren<SpriteRenderer>(true).Single(r => {
                var original = PrefabUtility.GetCorrespondingObjectFromSource(r) as SpriteRenderer;
                return original == sourceRenderer;
            });
            MatrixNear(renderer.localToWorldMatrix, hallWorld, 1e-5f, "Hall Native3 world matrix differs");
            Artifact actualWorld = SaveMatrix(run, "actual_renderer_world", renderer.localToWorldMatrix);
            foreach (var r in root.GetComponentsInChildren<Renderer>(true)) r.gameObject.layer = 0;
            renderer.gameObject.layer = 30;
            Need(renderer.enabled && renderer.gameObject.activeInHierarchy, "Hall source renderer inactive");
            cameraObject = new GameObject("source_configured_city_default_camera");
            Camera camera = cameraObject.AddComponent<Camera>();
            camera.enabled = false; camera.orthographic = false; camera.clearFlags = CameraClearFlags.SolidColor;
            camera.transform.position = V7(input.camera_position_xyz);
            camera.transform.rotation = Q(input.camera_rotation_xyzw);
            camera.fieldOfView = input.fov; camera.nearClipPlane = input.near_clip; camera.farClipPlane = input.far_clip;
            camera.aspect = (float)input.width / input.height; camera.cullingMask = 1 << 30;
            camera.allowHDR = false; camera.allowMSAA = false;
            target = new RenderTexture(input.width, input.height, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
            target.antiAliasing = 1; target.useMipMap = false; target.Create(); camera.targetTexture = target;
            camera.rect = new Rect(0f, 0f, 1f, 1f); camera.ResetProjectionMatrix();
            MatrixNear(camera.worldToCameraMatrix, ReadMatrix(project + "/" + input.camera_world_to_camera_path),
                       1e-5f, "NativeCamera2 worldToCamera differs");
            MatrixNear(camera.projectionMatrix, ReadMatrix(project + "/" + input.camera_projection_path),
                       1e-5f, "NativeCamera2 projection differs");
            MatrixNear(GL.GetGPUProjectionMatrix(camera.projectionMatrix, true),
                       ReadMatrix(project + "/" + input.camera_gpu_projection_rt_path), 1e-5f,
                       "NativeCamera2 GPU projection differs");
            before = new RenderTexture(1,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);
            after = new RenderTexture(1,1,0,RenderTextureFormat.ARGBFloat,RenderTextureReadWrite.Linear);
            before.Create(); after.Create(); witnessMaterial = new Material(witnessShader); witnessQuad = Quad();
            timeBeforeCommands = TimeCommand(camera, CameraEvent.BeforeForwardAlpha, before, witnessMaterial, witnessQuad, true);
            timeAfterCommands = TimeCommand(camera, CameraEvent.AfterForwardAlpha, after, witnessMaterial, witnessQuad, false);
            var phases = new List<Case>();
            foreach (string phase in input.phase_order) {
                bool isNight = phase.StartsWith("night_", StringComparison.Ordinal);
                phases.Add(RenderCase(phase, phase, phase.EndsWith("white", StringComparison.Ordinal) ? "white" : "black",
                                      isNight ? night : day, renderer, maskSprite.texture, camera, target, before, after, run, input));
            }
            int exactCount = 0;
            foreach (var phase in phases) {
                var oracle = contract.v1_case_raw.SingleOrDefault(pin => pin.path == phase.phase);
                Need(oracle != null && oracle.sha256 == phase.raw_top_left_rgba8.sha256,
                     "V2 frame differs byte-for-byte from sealed V1: " + phase.phase);
                exactCount++;
            }
            probeDay = new Material(probeDayShader); probeDay.CopyPropertiesFromMaterial(day);
            probeNight = new Material(probeNightShader); probeNight.CopyPropertiesFromMaterial(night);
            foreach (var probe in new[] { probeDay, probeNight }) {
                probe.SetFloat("_RokProbeMode", 0f);
                probe.SetFloat("_RokSrcBlend", 5f);
                probe.SetFloat("_RokDstBlend", 10f);
            }
            probeDay.renderQueue = day.renderQueue; probeNight.renderQueue = night.renderQueue;
            Need(probeDay.shader == probeDayShader && probeNight.shader == probeNightShader &&
                 probeDay.renderQueue == 2995 && probeNight.renderQueue == 2996,
                 "Instrumented source material queue or shader differs");
            Shader.SetGlobalFloat("_RokProbeMode", 0f);
            var cloned = new List<Case>();
            foreach (string phase in input.phase_order) {
                bool isNight = phase.StartsWith("night_", StringComparison.Ordinal);
                cloned.Add(RenderCase(phase, "clone_" + phase,
                                      phase.EndsWith("white", StringComparison.Ordinal) ? "white" : "black",
                                      isNight ? probeNight : probeDay, renderer, maskSprite.texture,
                                      camera, target, before, after, run, input));
            }
            foreach (var phase in cloned) {
                var oracle = contract.v1_case_raw.SingleOrDefault(pin => pin.path == phase.phase);
                Need(oracle != null && oracle.sha256 == phase.raw_top_left_rgba8.sha256,
                     "Instrumented mode0 differs byte-for-byte from Native1: " + phase.phase);
            }
            camera.RemoveCommandBuffer(CameraEvent.BeforeForwardAlpha, timeBeforeCommands);
            camera.RemoveCommandBuffer(CameraEvent.AfterForwardAlpha, timeAfterCommands);
            floatTimeCommands = new CommandBuffer { name = "Hall V7 float-draw source _Time.y=0 fixture" };
            floatTimeCommands.SetGlobalVector("_Time", Vector4.zero);
            camera.AddCommandBuffer(CameraEvent.BeforeForwardAlpha, floatTimeCommands);
            camera.allowHDR = true;
            floatTarget = new RenderTexture(input.width, input.height, 24, RenderTextureFormat.ARGBFloat,
                                            RenderTextureReadWrite.Linear);
            floatTarget.antiAliasing = 1; floatTarget.useMipMap = false; floatTarget.Create();
            Need(floatTarget.IsCreated() && floatTarget.format == RenderTextureFormat.ARGBFloat,
                 "Float diagnostic render target absent");
            camera.targetTexture = floatTarget;
            renderer.material = probeDay;
            probeDay.SetFloat("_RokProbeMode", 0f);
            probeDay.SetFloat("_RokSrcBlend", 5f); probeDay.SetFloat("_RokDstBlend", 10f);
            camera.backgroundColor = Color.black;
            camera.Render();
            int floatNonclear;
            Artifact floatFull = ReadFloatDiagnostic(floatTarget, run, out floatNonclear);
            camera.targetTexture = target;
            camera.allowHDR = false;
            var witnesses = new List<PackedWitness>();
            foreach (string phase in new[] { "day_black", "night_black" }) {
                Material probe = phase.StartsWith("night_", StringComparison.Ordinal) ? probeNight : probeDay;
                foreach (int y in new[] { 127, 138 })
                    witnesses.Add(RenderPackedWitness(48, "sentinel", phase, 598, y, probe,
                                                      renderer, maskSprite.texture, camera, target, run));
            }
            for (int mode = 1; mode <= 32; ++mode)
                witnesses.Add(RenderPackedWitness(mode, "matrix", "day_black", 598, 138, probeDay,
                                                  renderer, maskSprite.texture, camera, target, run));
            var objectRows = new byte[64]; var vpRows = new byte[64];
            for (int index = 0; index < 16; ++index) {
                Buffer.BlockCopy(BitConverter.GetBytes(witnesses[4+index].decoded_float), 0,
                                 objectRows, index*4, 4);
                Buffer.BlockCopy(BitConverter.GetBytes(witnesses[20+index].decoded_float), 0,
                                 vpRows, index*4, 4);
            }
            Need(MatrixMaxAbs(objectRows, Matrix4x4.zero) > 0.1f &&
                 MatrixMaxAbs(vpRows, Matrix4x4.zero) > 0.1f,
                 "Packed GPU matrix witness is all clear/zero rather than shader output");
            foreach (string phase in new[] { "day_black", "night_black" }) {
                Material probe = phase.StartsWith("night_", StringComparison.Ordinal) ? probeNight : probeDay;
                foreach (int y in new[] { 127, 138 })
                    for (int mode = 33; mode <= 47; ++mode)
                        witnesses.Add(RenderPackedWitness(mode, "pixel", phase, 598, y, probe,
                                                          renderer, maskSprite.texture, camera, target, run));
            }
            foreach (var point in witnesses.Where(p => p.mode == 33 || p.mode == 34 ||
                                                           p.mode == 35 || p.mode == 39 || p.mode == 43))
                Need(point.decoded_float > 0.01f,
                     "Packed GPU UV/Main/Mask/preblend witness is clear/zero");
            foreach (var point in witnesses.Where(p => p.mode == 47))
                Need(point.decoded_float == 0f, "Packed GPU shader did not observe source _Time.y=0 fixture");
            Shader.SetGlobalFloat("_RokProbeMode", 0f);
            var output = new Output {
                contract_sha256 = Sha(contractBytes), input_sha256 = contract.source_input_sha256,
                exporter_sha256 = contract.exporter_sha256, time_shader_sha256 = contract.time_shader_sha256,
                graphics_device = SystemInfo.graphicsDeviceType.ToString(), color_space = QualitySettings.activeColorSpace.ToString(),
                qualifier = contract.qualifier, target_format = "ARGB32 RenderTextureReadWrite.Default Gamma",
                readback_row_order = "top_left_rgba8", source_camera_scope = "NativeCamera2 source-configured transform, not original WorldCamera MonoBehaviour",
                complete = true, game_app_loaded = false, source_behaviors_executed = false,
                lod_qualified = false, main_view_qualified = false, width = input.width, height = input.height,
                anti_aliasing = 1, case_count = phases.Count, camera_position_xyz = V7(camera.transform.position),
                camera_rotation_xyzw = Q(camera.transform.rotation), camera_fov = camera.fieldOfView,
                camera_aspect = camera.aspect, camera_near = camera.nearClipPlane, camera_far = camera.farClipPlane,
                sprite_color_global = C(Shader.GetGlobalColor("_SpriteColor")),
                city_transparency_global = Shader.GetGlobalFloat("_CityTransparency"),
                renderer_world_matrix_sha256 = ShaFile(project + "/" + input.hall_world_matrix_path),
                actual_renderer_world_matrix = actualWorld, main_texture = mainProbe, mask_texture = maskProbe,
                v1_raw_exact_cases = exactCount, clone_raw_exact_cases = cloned.Count,
                 float_diagnostic_hdr = true, packed_source_time_zero_bound = true,
                 float_diagnostic_target_format = "ARGBFloat RenderTextureReadWrite.Linear",
                 packed_target_format = "ARGB32 RenderTextureReadWrite.Default Gamma; Blend One Zero",
                 packed_readback_scope = "full_1440x810_RGBA32_bottom_left_buffer_then_top_left_pixel_index",
                 float_diagnostic_mode0_top_left_rgba32f = floatFull,
                 float_diagnostic_nonclear_rgb_pixels = floatNonclear,
                diagnostic_shader_qualification = "instrumented mode0 four phases byteexact Native1; GPU constants are measured on clone draws, not untouched source shader draw",
                gpu_object_vs_csharp_max_abs = MatrixMaxAbs(objectRows, renderer.localToWorldMatrix),
                gpu_vp_vs_source_derived_max_abs = MatrixMaxAbs(vpRows,
                    GL.GetGPUProjectionMatrix(camera.projectionMatrix, true) * camera.worldToCameraMatrix),
                gpu_object_to_world_rows_f32le = Write(run, "gpu_object_to_world_rows.f32le", objectRows),
                gpu_view_projection_rows_f32le = Write(run, "gpu_view_projection_rows.f32le", vpRows),
                 packed_witnesses = witnesses.ToArray(),
                camera_world_to_camera = SaveMatrix(run, "camera_world_to_camera", camera.worldToCameraMatrix),
                camera_projection = SaveMatrix(run, "camera_projection", camera.projectionMatrix),
                camera_gpu_projection_rt = SaveMatrix(run, "camera_gpu_projection_rt", GL.GetGPUProjectionMatrix(camera.projectionMatrix, true)),
                sprite_guid = spriteGuid, mask_guid = maskGuid, source_renderer_file_id = contract.source_renderer_file_id,
                cases = phases.ToArray(), cloned_cases = cloned.ToArray()
            };
            Write(run, "hall_camera_material_native_v7.json", Encoding.UTF8.GetBytes(JsonUtility.ToJson(output, true) + "\n"));
        } finally {
            if (cameraObject) {
                var activeCamera = cameraObject.GetComponent<Camera>();
                if (activeCamera) {
                    if (timeBeforeCommands != null) activeCamera.RemoveCommandBuffer(CameraEvent.BeforeForwardAlpha, timeBeforeCommands);
                    if (timeAfterCommands != null) activeCamera.RemoveCommandBuffer(CameraEvent.AfterForwardAlpha, timeAfterCommands);
                    if (floatTimeCommands != null) activeCamera.RemoveCommandBuffer(CameraEvent.BeforeForwardAlpha, floatTimeCommands);
                }
            }
            if (timeBeforeCommands != null) { timeBeforeCommands.Release(); timeBeforeCommands = null; }
            if (timeAfterCommands != null) { timeAfterCommands.Release(); timeAfterCommands = null; }
            if (floatTimeCommands != null) { floatTimeCommands.Release(); floatTimeCommands = null; }
            if (cameraObject) UnityEngine.Object.DestroyImmediate(cameraObject);
            if (root) UnityEngine.Object.DestroyImmediate(root);
            if (witnessMaterial) UnityEngine.Object.DestroyImmediate(witnessMaterial);
            if (probeDay) UnityEngine.Object.DestroyImmediate(probeDay);
            if (probeNight) UnityEngine.Object.DestroyImmediate(probeNight);
            if (witnessQuad) UnityEngine.Object.DestroyImmediate(witnessQuad);
            foreach (var rt in new[] { target, before, after, floatTarget }) if (rt) { rt.Release(); UnityEngine.Object.DestroyImmediate(rt); }
        }
    }
    public static void Run()
    {
        try { RunInner(); EditorApplication.Exit(0); }
        catch (Exception error) { Debug.LogException(error); EditorApplication.Exit(2); }
    }
}
