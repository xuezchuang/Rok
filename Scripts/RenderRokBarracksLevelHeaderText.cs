using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;

// Source prefab LanguageText -> native UGUI mesh/atlas; isolated presentation export.
public static class RenderRokBarracksLevelHeaderText
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityBarracksLevelHeaderTextProbe";
    const string Repo = "D:/ueDemo/Rok/";
    [Serializable] public class Source { public string path, sha256; }
    [Serializable] public class Spec
    {
        public string key, text, sourceNode;
        public int fontSize, style, alignment, horizontalOverflow, verticalOverflow, bestFitMinSize, bestFitMaxSize, languageId;
        public float x, y, width, height, lineSpacing;
        public bool rich, bestFit;
        public Color color;
    }
    [Serializable] public class Inputs
    {
        public string schema, renderer_sha256, copied_renderer_sha256, scope, prefab;
        public Source[] sources;
        public Spec[] sourceTexts;
    }
    [Serializable] public class Glyph
    {
        public int codepoint, advance, minX, maxX, minY, maxY;
        public Vector2 uvBottomLeft, uvBottomRight, uvTopLeft, uvTopRight;
    }
    [Serializable] public class TextMesh
    {
        public string key, text, atlas, atlasSha256, sourceNode;
        public float width, height, preferredWidth, preferredHeight, pixelsPerUnit, x, y;
        public int fontSize, effectiveFontSize, style, alignment, lineCount;
        public Vector2[] xy, uv;
        public Color32[] colors;
        public int[] triangles;
        public Glyph[] glyphs;
    }
    [Serializable] public class Report
    {
        public string schema = "rok.barracks.source_text.v1", inputSha256, unityVersion, prefab;
        public TextMesh[] texts;
    }
    static Inputs Data;
    static string Output;
    static Camera Cam;
    static Canvas Canvas;
    static RenderTexture Target;
    static GameObject Title;
    static int Wait;
    static readonly List<Text> Labels = new List<Text>();
    static readonly MethodInfo Populate = typeof(Text).GetMethod("OnPopulateMesh", BindingFlags.NonPublic | BindingFlags.Instance,
        null, new[] { typeof(VertexHelper) }, null);

    static string Hash(string path)
    {
        using (var h = SHA256.Create()) return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant();
    }
    public static void Run()
    {
        try
        {
            if (Application.dataPath.Replace('\\', '/').TrimEnd('/') != Project + "/Assets" ||
                Application.unityVersion != "2019.4.28f1c1" || QualitySettings.activeColorSpace != ColorSpace.Gamma)
                throw new Exception("Wrong isolated project/version/color space");
            if (SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Null)
                throw new Exception("Native UGUI font atlas requires graphics; launch without -nographics");
            Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Project + "/ReferenceInputs.json"));
            if (Data.schema != "rok.barracks.source_text.inputs.v1" || Data.sourceTexts.Length != 4 || Populate == null)
                throw new Exception("Wrong four-header source coverage or Text mesh method unavailable");
            foreach (var s in Data.sources)
                if (Hash(Repo + s.path) != s.sha256) throw new Exception("Source drift " + s.path);
            if (Hash(Project + "/Assets/Editor/RenderRokBarracksLevelHeaderText.cs") != Data.copied_renderer_sha256)
                throw new Exception("Executed renderer drift");
            Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
            Directory.CreateDirectory(Output);
            Directory.CreateDirectory(Output + "/SourceTextAtlases");
            File.Copy(Project + "/ReferenceInputs.json", Output + "/ReferenceInputs.json");
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            Cam = new GameObject("Camera").AddComponent<Camera>();
            Cam.clearFlags = CameraClearFlags.SolidColor;
            Cam.backgroundColor = new Color(.2f, .2f, .2f, 1);
            Cam.allowHDR = false;
            Cam.allowMSAA = false;
            Target = new RenderTexture(1440, 810, 24, RenderTextureFormat.ARGB32);
            Cam.targetTexture = Target;
            Canvas = new GameObject("Canvas", typeof(RectTransform)).AddComponent<Canvas>();
            Canvas.renderMode = RenderMode.ScreenSpaceCamera;
            Canvas.worldCamera = Cam;
            Canvas.planeDistance = 1;
            Canvas.referencePixelsPerUnit = 100;
            var scaler = Canvas.gameObject.AddComponent<CanvasScaler>();
            scaler.uiScaleMode = CanvasScaler.ScaleMode.ConstantPixelSize;
            scaler.scaleFactor = 1;
            scaler.referencePixelsPerUnit = 100;
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab);
            if (!prefab) throw new Exception("Original Title_Four prefab missing");
            Title = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
            var root = Title.GetComponent<RectTransform>();
            root.SetParent(Canvas.transform, false);
            root.anchoredPosition = new Vector2(0, -132.8499755859375f); // Actual native detail header parent y.
            root.localScale = Vector3.one;
            foreach (var arab in Title.GetComponentsInChildren<ArabLayoutCompment>(true))
            {
                int flags = arab.SourceFlags;
                if ((flags & ~635) != 0) throw new Exception("Unsupported source Arab flags");
                if ((flags & 1) != 0) continue;
                if ((flags & 2) != 0)
                {
                    var r = arab.GetComponent<RectTransform>();
                    var min = r.anchorMin; var max = r.anchorMax;
                    r.anchorMin = new Vector2(1 - max.x, min.y);
                    r.anchorMax = new Vector2(1 - min.x, max.y);
                    r.pivot = new Vector2(1 - r.pivot.x, r.pivot.y);
                    r.anchoredPosition = new Vector2(-r.anchoredPosition.x, r.anchoredPosition.y);
                }
            }
            foreach (var spec in Data.sourceTexts)
            {
                var matches = Title.GetComponentsInChildren<Text>(true).Where(label => label.name == spec.sourceNode).ToArray();
                if (matches.Length != 1 || matches[0].GetType().Name != "LanguageText")
                    throw new Exception("Original LanguageText missing " + spec.key);
                var t = matches[0];
                if (!t.font || t.font.name != "Arial" || t.fontSize != 22 || (int)t.fontStyle != 0 ||
                    (int)t.alignment != 4 || !t.resizeTextForBestFit || t.resizeTextMinSize != 2 ||
                    t.resizeTextMaxSize != 24 || !t.supportRichText ||
                    (int)t.horizontalOverflow != 0 || (int)t.verticalOverflow != 0)
                    throw new Exception("Source Title_Four typography drift " + spec.key);
                var observed = (Color32)t.color; var expected = (Color32)spec.color;
                if (observed.r != expected.r || observed.g != expected.g || observed.b != expected.b || observed.a != expected.a)
                    throw new Exception("Source header color drift " + spec.key);
                t.text = spec.text;
                Labels.Add(t);
                t.font.RequestCharactersInTexture(spec.text, spec.fontSize, t.fontStyle);
            }
            EditorApplication.update += Tick;
        }
        catch (Exception e) { Fail(e); }
    }
    static void Tick()
    {
        if (++Wait < 40) return;
        try
        {
            var result = new List<TextMesh>();
            var mesh = new Mesh();
            for (int i = 0; i < Labels.Count; i++)
            {
                foreach (var label in Labels) label.SetAllDirty();
                Canvas.ForceUpdateCanvases();
                Cam.Render();
                var t = Labels[i]; var spec = Data.sourceTexts[i];
                var corners = new Vector3[4]; t.rectTransform.GetWorldCorners(corners);
                var points = corners.Select(p => RectTransformUtility.WorldToScreenPoint(Cam, p)).ToArray();
                float x = points.Min(p => p.x), y = 810 - points.Max(p => p.y);
                if (Mathf.Abs(x - spec.x) > .05f || Mathf.Abs(y - spec.y) > .05f ||
                    Mathf.Abs(t.rectTransform.rect.width - spec.width) > .05f ||
                    Mathf.Abs(t.rectTransform.rect.height - spec.height) > .05f)
                    throw new Exception("Original header geometry differs from native level evidence " + spec.key);
                using (var vh = new VertexHelper())
                {
                    Populate.Invoke(t, new object[] { vh });
                    vh.FillMesh(mesh);
                }
                if (mesh.vertexCount == 0 || mesh.vertexCount % 4 != 0 || t.pixelsPerUnit != 1)
                    throw new Exception("Invalid native header glyph mesh " + spec.key);
                int effective = t.cachedTextGenerator.fontSizeUsedForBestFit;
                if (effective < 2 || effective > 24) throw new Exception("Unexpected best-fit font size " + spec.key);
                var glyphs = new List<Glyph>();
                foreach (char c in spec.text.Distinct())
                {
                    CharacterInfo g;
                    if (!t.font.GetCharacterInfo(c, out g, effective, t.fontStyle))
                        throw new Exception("Missing source font glyph " + spec.key + ":" + (int)c);
                    glyphs.Add(new Glyph { codepoint = c, advance = g.advance, minX = g.minX, maxX = g.maxX,
                        minY = g.minY, maxY = g.maxY, uvBottomLeft = g.uvBottomLeft,
                        uvBottomRight = g.uvBottomRight, uvTopLeft = g.uvTopLeft, uvTopRight = g.uvTopRight });
                }
                string pending = Output + "/pending.png";
                SaveTexture(t.font.material.mainTexture, pending);
                string hash = Hash(pending), atlas = "SourceTextAtlases/" + hash + ".png";
                if (!File.Exists(Output + "/" + atlas)) File.Move(pending, Output + "/" + atlas);
                else File.Delete(pending);
                result.Add(new TextMesh { key = spec.key, text = spec.text, sourceNode = spec.sourceNode,
                    width = t.rectTransform.rect.width, height = t.rectTransform.rect.height,
                    x = x, y = y, fontSize = spec.fontSize, effectiveFontSize = effective,
                    style = spec.style, alignment = spec.alignment,
                    lineCount = t.cachedTextGenerator.lineCount, pixelsPerUnit = t.pixelsPerUnit,
                    preferredWidth = t.preferredWidth, preferredHeight = t.preferredHeight,
                    xy = mesh.vertices.Select(v => new Vector2(v.x, -v.y)).ToArray(),
                    uv = mesh.uv, colors = mesh.colors32, triangles = mesh.triangles,
                    glyphs = glyphs.ToArray(), atlas = atlas, atlasSha256 = hash });
            }
            foreach (var t in Labels) t.SetAllDirty();
            Canvas.ForceUpdateCanvases(); Cam.Render();
            SaveTexture(Target, Output + "/headers_preview.png");
            File.WriteAllText(Output + "/source_text_meshes.json", JsonUtility.ToJson(new Report {
                inputSha256 = Hash(Output + "/ReferenceInputs.json"), unityVersion = Application.unityVersion,
                prefab = Data.prefab, texts = result.ToArray() }, true));
            UnityEngine.Object.DestroyImmediate(mesh);
            EditorApplication.update -= Tick;
            EditorApplication.Exit(0);
        }
        catch (Exception e) { Fail(e); }
    }
    static void SaveTexture(Texture texture, string path)
    {
        var previous = RenderTexture.active;
        var rt = RenderTexture.GetTemporary(texture.width, texture.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
        Graphics.Blit(texture, rt); RenderTexture.active = rt;
        var image = new Texture2D(texture.width, texture.height, TextureFormat.RGBA32, false);
        image.ReadPixels(new Rect(0, 0, texture.width, texture.height), 0, 0); image.Apply();
        File.WriteAllBytes(path, image.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = previous;
        RenderTexture.ReleaseTemporary(rt);
    }
    static void Fail(Exception e)
    {
        Debug.LogException(e);
        if (Output != null) File.WriteAllText(Output + "/failure.txt", e.ToString());
        EditorApplication.update -= Tick;
        EditorApplication.Exit(1);
    }
}
