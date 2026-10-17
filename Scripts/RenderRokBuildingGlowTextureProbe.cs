using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

public static class RenderRokBuildingGlowTextureProbe
{
    const string Root = "D:/ueDemo/Rok";
    const string Project = Root + "/Saved/UnityBuildingGlowTextureProbe";
    const string Output = Root + "/Saved/RokUnityMigration/BuildingGlowTexture";
    const string TexturePath = "Assets/BundleAssets/Effect/UIsprite/shared_uisprite/star003.png";
    const string MaterialPath = "Assets/BundleAssets/Effect/Materials/shared_effectmaterials/0UIEffect.mat";
    const string ExpectedHash = "9f159150cc1614e2eb38f88217433d2c68b1d28d0b73910ecd249f8f39c40388";

    [Serializable] public class TextureInfo
    {
        public int width, height, mipmapCount, anisoLevel;
        public string format, graphicsFormat, filterMode, wrapMode, wrapModeU, wrapModeV;
        public bool isReadable, importerSRGB, alphaIsTransparency, importerMipmaps;
        public string importerCompression, platformFormat, platformCompression;
    }
    [Serializable] public class MaterialInfo
    {
        public string name, shader, textureSampleAdd, globalTextureSampleAdd, mainTexturePath;
        public bool hasTextureSampleAdd, etc1ExternalAlphaKeyword;
    }
    [Serializable] public class Report
    {
        public string schema, unityVersion, colorSpace, graphicsDevice, graphicsAPI, sourceSha256;
        public string importedRawSha256, importedRawFormat, sampledRgbaSha256, sampledPngSha256;
        public string failure, rawReadbackFailure, sampleContract;
        public int importedRawByteCount, sampledRgbaByteCount;
        public bool actualGPUTextureReadback, passThroughGPUReadback, rgbWriteDisabled;
        public TextureInfo texture;
        public MaterialInfo material;
    }
    static string Hash(byte[] bytes) { using (var h = SHA256.Create()) return BitConverter.ToString(h.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant(); }
    static string HashFile(string path) { return Hash(File.ReadAllBytes(path)); }
    static byte[] NativeArrayBytes(Unity.Collections.NativeArray<byte> data)
    {
        var bytes = new byte[data.Length];
        for (int i = 0; i < bytes.Length; ++i) bytes[i] = data[i];
        return bytes;
    }
    public static void Run()
    {
        Directory.CreateDirectory(Output);
        var report = new Report { schema = "rok.building.glow_texture.native.v1", unityVersion = Application.unityVersion,
            colorSpace = QualitySettings.activeColorSpace.ToString(), graphicsDevice = SystemInfo.graphicsDeviceName,
            graphicsAPI = SystemInfo.graphicsDeviceType.ToString(),
            sampleContract = "Original importer unchanged; original GPU texture raw bytes and a tex2D pass-through ARGB32 mip0 sample at exact texel centers in Gamma, GL.sRGBWrite=false. No forced alpha, no runtime texture setting changes." };
        try
        {
            if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase)) throw new Exception("Wrong isolated project");
            if (Application.unityVersion != "2019.4.28f1c1" || QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Wrong Unity version or color space");
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new Exception("GPU required; do not use -nographics");
            report.sourceSha256 = HashFile(Project + "/" + TexturePath);
            if (report.sourceSha256 != ExpectedHash) throw new Exception("Source PNG hash changed");
            var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(TexturePath);
            var sprite = AssetDatabase.LoadAssetAtPath<Sprite>(TexturePath);
            var sourceMaterial = AssetDatabase.LoadAssetAtPath<Material>(MaterialPath);
            if (!texture || !sprite || sprite.texture != texture || !sourceMaterial) throw new Exception("Original sprite/texture/material not imported");
            if (sourceMaterial.shader == null || sourceMaterial.shader.name != "UI/Add") throw new Exception("Original material shader is not UI/Add");
            var importer = (TextureImporter)AssetImporter.GetAtPath(TexturePath);
            if (importer == null) throw new Exception("TextureImporter missing");
            var platform = importer.GetPlatformTextureSettings("Standalone");
            report.texture = new TextureInfo { width = texture.width, height = texture.height, mipmapCount = texture.mipmapCount,
                anisoLevel = texture.anisoLevel, format = texture.format.ToString(), graphicsFormat = texture.graphicsFormat.ToString(),
                filterMode = texture.filterMode.ToString(), wrapMode = texture.wrapMode.ToString(), wrapModeU = texture.wrapModeU.ToString(),
                wrapModeV = texture.wrapModeV.ToString(), isReadable = texture.isReadable, importerSRGB = importer.sRGBTexture,
                alphaIsTransparency = importer.alphaIsTransparency, importerMipmaps = importer.mipmapEnabled,
                importerCompression = importer.textureCompression.ToString(), platformFormat = platform.format.ToString(),
                platformCompression = platform.textureCompression.ToString() };
            report.material = new MaterialInfo { name = sourceMaterial.name, shader = sourceMaterial.shader.name,
                hasTextureSampleAdd = sourceMaterial.HasProperty("_TextureSampleAdd"),
                textureSampleAdd = sourceMaterial.GetVector("_TextureSampleAdd").ToString("F9"),
                globalTextureSampleAdd = Shader.GetGlobalVector("_TextureSampleAdd").ToString("F9"),
                mainTexturePath = AssetDatabase.GetAssetPath(sourceMaterial.mainTexture),
                etc1ExternalAlphaKeyword = sourceMaterial.IsKeywordEnabled("ETC1_EXTERNAL_ALPHA") };

            try
            {
                var rawRequest = AsyncGPUReadback.Request(texture, 0);
                rawRequest.WaitForCompletion();
                if (rawRequest.hasError) throw new Exception("Native GPU texture raw readback failed");
                var raw = NativeArrayBytes(rawRequest.GetData<byte>());
                File.WriteAllBytes(Output + "/imported_gpu_mip0.bin", raw);
                report.importedRawSha256 = Hash(raw); report.importedRawByteCount = raw.Length;
                report.importedRawFormat = texture.graphicsFormat.ToString(); report.actualGPUTextureReadback = true;
            }
            catch (Exception error) { report.rawReadbackFailure = error.ToString(); }

            var copyShader = Shader.Find("Hidden/RokGlowTextureCopy");
            if (!copyShader || !copyShader.isSupported) throw new Exception("Pass-through shader missing/unsupported");
            var copyMaterial = new Material(copyShader);
            var target = new RenderTexture(texture.width, texture.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
            target.filterMode = FilterMode.Point; target.wrapMode = TextureWrapMode.Clamp;
            target.useMipMap = false; target.autoGenerateMips = false; target.Create();
            if (!target.IsCreated()) throw new Exception("Readback target not created");
            var previous = RenderTexture.active; var previousSRGB = GL.sRGBWrite;
            Texture2D pixels = null;
            try
            {
                GL.sRGBWrite = false;
                Graphics.Blit(texture, target, copyMaterial, 0);
                RenderTexture.active = target;
                pixels = new Texture2D(texture.width, texture.height, TextureFormat.RGBA32, false, true);
                pixels.ReadPixels(new Rect(0, 0, texture.width, texture.height), 0, 0, false);
                pixels.Apply(false, false);
                var rgba = pixels.GetRawTextureData();
                File.WriteAllBytes(Output + "/sampled_rgba_bottom_up.bin", rgba);
                report.sampledRgbaSha256 = Hash(rgba); report.sampledRgbaByteCount = rgba.Length;
                var png = pixels.EncodeToPNG(); File.WriteAllBytes(Output + "/sampled_gpu_mip0.png", png);
                report.sampledPngSha256 = Hash(png);
                report.passThroughGPUReadback = true; report.rgbWriteDisabled = !GL.sRGBWrite;
            }
            finally
            {
                RenderTexture.active = previous; GL.sRGBWrite = previousSRGB;
                if (pixels) UnityEngine.Object.DestroyImmediate(pixels);
                target.Release(); UnityEngine.Object.DestroyImmediate(target); UnityEngine.Object.DestroyImmediate(copyMaterial);
            }
        }
        catch (Exception error) { report.failure = error.ToString(); Debug.LogException(error); }
        File.WriteAllText(Output + "/native.json", JsonUtility.ToJson(report, true));
        if (report.failure != null) EditorApplication.Exit(1);
        else EditorApplication.Exit(0);
    }
}
