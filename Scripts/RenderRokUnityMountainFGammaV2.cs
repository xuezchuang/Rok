using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

// Run only in the isolated V2 probe. Explicitly sample each original GPU mip
// into an ARGB32 target; the imported BC1 bytes provide an independent oracle.
public static class RenderRokUnityMountainFGammaV2 {
    const string Root = "D:/ueDemo/Rok";
    const string Probe = Root + "/Saved/UnityMountainFGammaV2Probe";
    const string Output = Root + "/Saved/RokUnityMigration/MountainFGammaV2";
    const string TexturePath = "Assets/BundleAssets/land/Texture/build_mountain_F.png";

    [Serializable] public class Mip {
        public int level, width, height, compressedBytes, rgbaBytes;
        public int alphaZero, alphaOpaque, alphaPartial;
        public string rgbaPath, bc1Path;
    }
    [Serializable] public class Report {
        public string unityVersion, colorSpace, device, texturePath, textureGuid;
        public string materialGuid, meshGuid, meshLocalId, textureFormat, graphicsFormat;
        public string sourceShader, filterMode;
        public int width, height, mipCount, vertices, triangles;
        public bool materialMainTextureIsSource, importerReadableForRawBlockAudit;
        public Mip[] mips;
    }
    public static void Run() {
        try {
            Export();
            if (File.Exists(Output + "/native_failure.txt")) File.Delete(Output + "/native_failure.txt");
            Debug.Log("ROK_MOUNTAIN_F_GAMMA_V2_COMPLETE"); EditorApplication.Exit(0);
        }
        catch (Exception error) {
            Directory.CreateDirectory(Output);
            File.WriteAllText(Output + "/native_failure.txt", error.ToString());
            Debug.LogException(error); EditorApplication.Exit(1);
        }
    }
    static void Require(bool condition, string message) { if (!condition) throw new Exception(message); }
    static void Export() {
        Require(Application.dataPath.Replace('\\','/') == Probe + "/Assets", "Wrong isolated project");
        Require(Application.unityVersion == "2019.4.28f1c1" && QualitySettings.activeColorSpace == ColorSpace.Gamma, "Wrong Unity version/color space");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/BundleAssets/land/Tile/AutoMake/Mountain/build_mountain_F.prefab");
        Require(prefab != null, "Source prefab missing");
        var renderer = prefab.GetComponentsInChildren<MeshRenderer>(true).Single(r => r.name == "build_mountain_F");
        var mesh = renderer.GetComponent<MeshFilter>().sharedMesh;
        Require(renderer != null && mesh != null && mesh.vertexCount == 1002 && mesh.triangles.Length == 1870 * 3, "Source body geometry mismatch");
        var material = renderer.sharedMaterial;
        Require(material != null && material.shader.name == "custom/landmap/z_cm_slg_mountain", "Source shader mismatch");
        var texture = material.GetTexture("_MainTex") as Texture2D;
        Require(texture != null && AssetDatabase.GetAssetPath(texture) == TexturePath, "Source material _MainTex binding mismatch");
        string meshGuid, materialGuid; long meshId, materialId;
        Require(AssetDatabase.TryGetGUIDAndLocalFileIdentifier(mesh, out meshGuid, out meshId) && meshGuid == "aee73fed0485b1244aea2938ba78f8ee" && meshId == 3827334489394421038L, "Source mesh identity mismatch");
        Require(AssetDatabase.TryGetGUIDAndLocalFileIdentifier(material, out materialGuid, out materialId) && materialGuid == "13645cd34a40fc8449f0396b7785e3a1", "Source material identity mismatch");
        Require(AssetDatabase.AssetPathToGUID(TexturePath) == "b9d81a5ad9e692c499abcf99c1cabd63", "Source texture GUID mismatch");

        // Readability changes only the isolated copy's importer and permits an
        // independent audit of the exact BC1 blocks imported for this run.
        var importer = (TextureImporter)AssetImporter.GetAtPath(TexturePath);
        Require(importer != null && importer.mipmapEnabled && importer.sRGBTexture, "Source importer mip/sRGB mismatch");
        if (!importer.isReadable) { importer.isReadable = true; importer.SaveAndReimport(); }
        texture = AssetDatabase.LoadAssetAtPath<Texture2D>(TexturePath);
        material = AssetDatabase.LoadAssetAtPath<Material>(AssetDatabase.GetAssetPath(material));
        Require(texture != null && material != null && material.GetTexture("_MainTex") == texture, "Material source texture unbound after readable reimport");
        Require(texture.format == TextureFormat.DXT1 && texture.width == 1024 && texture.height == 1024 && texture.mipmapCount == 11, "Imported texture is not expected DXT1 mip chain");
        Require(SystemInfo.graphicsDeviceType != GraphicsDeviceType.Null, "GPU required; do not use -nographics");
        var copyShader = Shader.Find("Hidden/RokMountainFMipDecodeV2");
        Require(copyShader != null && copyShader.isSupported, "V2 mip sample shader unavailable");
        var copyMaterial = new Material(copyShader);
        Require(copyMaterial.HasProperty("_MainTex") && copyMaterial.HasProperty("_Mip"), "V2 shader binding properties missing");
        copyMaterial.SetTexture("_MainTex", texture);
        Require(copyMaterial.GetTexture("_MainTex") == texture, "V2 source texture not bound");
        Directory.CreateDirectory(Output);
        var rows = new Mip[texture.mipmapCount];
        byte[] allBlocks = texture.GetRawTextureData<byte>().ToArray();
        int blockOffset = 0;
        // Save the unmodified importer's compressed chain even if the GPU
        // sampling stage later fails; no exported PNG is accepted without it.
        var blocksByLevel = new byte[rows.Length][];
        for (int level = 0; level < rows.Length; ++level) {
            int width = Math.Max(1, texture.width >> level), height = Math.Max(1, texture.height >> level);
            int expectedBlocks = Math.Max(1, (width + 3) / 4) * Math.Max(1, (height + 3) / 4) * 8;
            Require(blockOffset + expectedBlocks <= allBlocks.Length, "Imported DXT1 block count mismatch at mip " + level);
            byte[] bc1 = new byte[expectedBlocks];
            Array.Copy(allBlocks, blockOffset, bc1, 0, expectedBlocks);
            blockOffset += expectedBlocks;
            blocksByLevel[level] = bc1;
            File.WriteAllBytes(Output + "/native_mip" + level + ".bc1", bc1);
        }
        Require(blockOffset == allBlocks.Length, "Unexpected DXT1 raw tail bytes");
        for (int level = 0; level < rows.Length; ++level) {
            int width = Math.Max(1, texture.width >> level), height = Math.Max(1, texture.height >> level);
            byte[] bc1 = blocksByLevel[level];
            var row = new Mip { level = level, width = width, height = height, compressedBytes = bc1.Length,
                bc1Path = Output + "/native_mip" + level + ".bc1",
                rgbaPath = Output + "/native_mip" + level + ".png" };
            var target = new RenderTexture(width, height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
            target.filterMode = FilterMode.Point; target.wrapMode = TextureWrapMode.Clamp;
            target.useMipMap = false; target.autoGenerateMips = false; target.Create();
            Require(target.IsCreated(), "V2 RGBA render target not created at mip " + level);
            Texture2D image = null;
            var previous = RenderTexture.active;
            var previousSRGB = GL.sRGBWrite;
            try {
                GL.sRGBWrite = false;
                copyMaterial.SetTexture("_MainTex", texture);
                copyMaterial.SetFloat("_Mip", level);
                Require(copyMaterial.GetTexture("_MainTex") == texture, "V2 source texture lost at mip " + level);
                Graphics.Blit(texture, target, copyMaterial, 0);
                RenderTexture.active = target;
                image = new Texture2D(width, height, TextureFormat.RGBA32, false, true);
                image.ReadPixels(new Rect(0, 0, width, height), 0, 0, false);
                image.Apply(false, false);
                row.rgbaBytes = image.GetRawTextureData().Length;
                Require(row.rgbaBytes == width * height * 4, "V2 RGBA byte count mismatch at mip " + level);
                var pixels = image.GetPixels32();
                int minRgb = 255, maxRgb = 0;
                foreach (var pixel in pixels) {
                    if (pixel.a == 0) row.alphaZero++;
                    else if (pixel.a == 255) row.alphaOpaque++;
                    else row.alphaPartial++;
                    minRgb = Math.Min(minRgb, Math.Min(pixel.r, Math.Min(pixel.g, pixel.b)));
                    maxRgb = Math.Max(maxRgb, Math.Max(pixel.r, Math.Max(pixel.g, pixel.b)));
                }
                Require(row.alphaPartial == 0, "GPU DXT1 readback has fractional alpha at mip " + level);
                if (level == 0) Require(maxRgb - minRgb >= 16, "GPU DXT1 readback is flat/unbound");
                File.WriteAllBytes(row.rgbaPath, image.EncodeToPNG());
                rows[level] = row;
            }
            finally {
                RenderTexture.active = previous; GL.sRGBWrite = previousSRGB;
                if (image != null) UnityEngine.Object.DestroyImmediate(image);
                target.Release(); UnityEngine.Object.DestroyImmediate(target);
            }
        }
        UnityEngine.Object.DestroyImmediate(copyMaterial);
        var report = new Report { unityVersion = Application.unityVersion, colorSpace = QualitySettings.activeColorSpace.ToString(),
            device = SystemInfo.graphicsDeviceName, texturePath = TexturePath, textureGuid = AssetDatabase.AssetPathToGUID(TexturePath),
            materialGuid = materialGuid, meshGuid = meshGuid, meshLocalId = meshId.ToString(), textureFormat = texture.format.ToString(),
            graphicsFormat = texture.graphicsFormat.ToString(), sourceShader = material.shader.name, filterMode = texture.filterMode.ToString(),
            width = texture.width, height = texture.height, mipCount = texture.mipmapCount, vertices = mesh.vertexCount,
            triangles = mesh.triangles.Length / 3, materialMainTextureIsSource = material.GetTexture("_MainTex") == texture,
            importerReadableForRawBlockAudit = importer.isReadable, mips = rows };
        File.WriteAllText(Output + "/native_render.json", JsonUtility.ToJson(report, true));
    }
}
