using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;

// Presentation-only bindings for unchanged original prefabs. Never saves language,
// sends notifications, invokes audio, or connects full client services.
public static class RenderRokHelpNativeReference
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityHelpNativeReferenceProbe";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Question { public int id, group_id, question_id, answer_id; public string group, question, answer; }
    [Serializable] public class Inputs { public string scope, prefab, rowPrefab, questionPrefab; public int width, height; public Entry[] texts; public Question[] questions; public SourceTextSpec[] sourceTexts; }
    [Serializable] public class Node
    {
        public string path, sprite, text, font;
        public Rect screenBounds;
        public Vector2 anchorsMin, anchorsMax, pivot, sizeDelta, anchoredPosition;
        public Vector3 localScale;
        public int fontSize, alignment;
        public bool active, isOn;
        public float imageAlpha;
    }
    [Serializable] public class Frame { public int example; public string png; public Node[] nodes; public float[] backgroundProbeAlpha; }
    [Serializable] public class ArabNode { public string path; public int flags; public bool chineseAwakeWillFlip; public string[] components; }
    [Serializable] public class ArabInventory { public ArabNode[] nodes; }
    [Serializable] public class Report
    {
        public string scope, unityVersion, inputSha256;
        public bool nativeRendered, fullMediator = false, sourcePrefabModified = false;
        public Rect cameraPixelRect;
        public Vector2 screenSize, targetSize;
        public Frame[] frames;
    }
    static Inputs Data;
    static GameObject Window;
    static Camera Camera;
    static RenderTexture Target;
    static string Output;
    static int Example, Wait;
    static readonly List<Frame> Frames = new List<Frame>();
    static readonly HashSet<ArabLayoutCompment> Applied = new HashSet<ArabLayoutCompment>();
    static Report Evidence;
    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase)
            || Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong isolated project or Unity version");
    }
    public static void Run()
    {
        Guard();
        try
        {
            Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
            Directory.CreateDirectory(Output);
            File.Copy(Project + "/ReferenceInputs.json", Output + "/ReferenceInputs.json", false);
            string inputHash = Hash(Output + "/ReferenceInputs.json");
            if (inputHash != Hash(Project + "/ReferenceInputs.json")) throw new Exception("Input changed while snapshotting");
            Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
            if (Data.width != 1440 || Data.height != 810 || Data.questions.Length != 70) throw new Exception("Unexpected source window/config");
            if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Source requires Gamma");
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            Camera = new GameObject("ReferenceCamera").AddComponent<Camera>();
            Camera.clearFlags = CameraClearFlags.SolidColor; Camera.backgroundColor = Color.clear;
            Camera.allowHDR = false; Camera.allowMSAA = false;
            Target = new RenderTexture(Data.width, Data.height, 24, RenderTextureFormat.ARGB32);
            Target.Create(); Camera.targetTexture = Target;
            var canvas = new GameObject("ReferenceCanvas", typeof(RectTransform), typeof(Canvas)).GetComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceCamera; canvas.worldCamera = Camera; canvas.planeDistance = 1;
            Window = (GameObject)PrefabUtility.InstantiatePrefab(AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab));
            Window.transform.SetParent(canvas.transform, false);
            foreach (var animator in Window.GetComponentsInChildren<Animator>(true)) animator.enabled = false;
            // Sample the actual source T1 final pose, without full ViewBinder lifecycle.
            var clip = AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim");
            if (clip == null) throw new Exception("Missing original T1 clip");
            clip.SampleAnimation(Window, clip.length);
            Canvas.ForceUpdateCanvases();
            foreach (var label in Window.GetComponentsInChildren<LanguageText>(true))
            {
                var entry=Data.texts.FirstOrDefault(x=>x.id==label.SourceLanguageId);
                if(entry!=null)label.text=entry.text;
            }
            ApplyChinese();
            foreach (var label in Window.GetComponentsInChildren<LanguageText>(true))
            {
                if (label.font == null) throw new Exception("Missing source font: " + label.name);
                label.font.RequestCharactersInTexture(label.text, label.fontSize, label.fontStyle);
            }
            foreach (var node in Window.GetComponentsInChildren<Transform>(true))
                if (GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(node.gameObject) != 0) throw new Exception("Missing source script: " + node.name);
            Evidence = new Report { scope = Data.scope, unityVersion = Application.unityVersion, inputSha256 = inputHash };
            Frames.Clear(); Example = 0; Select(); EditorApplication.update += Tick;
        }
        catch (Exception e) { Fail(e); }
    }
    static void ApplyChinese()
    {
        var components = Window.GetComponentsInChildren<ArabLayoutCompment>(true);
        var inventory = new ArabInventory { nodes = components.Select(a => new ArabNode {
            path = AnimationUtility.CalculateTransformPath(a.transform, Window.transform), flags = a.SourceFlags,
            chineseAwakeWillFlip = (a.SourceFlags & 1) == 0,
            components = a.GetComponents<Component>().Select(c => c ? c.GetType().FullName : "missing").ToArray()
        }).ToArray() };
        File.WriteAllText(Output + "/loaded_arab_inventory.json", JsonUtility.ToJson(inventory, true));
        // Validate the entire instantiated/nested tree first, not one flag per retry.
        var unsupported = inventory.nodes.Where(n => (n.flags & ~(1 | 2 | 8 | 16 | 512)) != 0).ToArray();
        if (unsupported.Length != 0) throw new Exception("Unhandled source Arab nodes: " + String.Join(", ", unsupported.Select(n => n.path + "=" + n.flags)));
        foreach (var arab in components)
        {
            if (!Applied.Add(arab)) continue;
            int flags = arab.SourceFlags;
            if ((flags & 1) != 0) continue;
            if ((flags & 2) != 0)
            {
                var r = arab.GetComponent<RectTransform>(); var min = r.anchorMin; var max = r.anchorMax;
                r.anchorMin = new Vector2(1 - max.x, min.y); r.anchorMax = new Vector2(1 - min.x, max.y);
                r.pivot = new Vector2(1 - r.pivot.x, r.pivot.y); r.anchoredPosition = new Vector2(-r.anchoredPosition.x, r.anchoredPosition.y);
            }
            if ((flags & 8) != 0)
            {
                var t = arab.GetComponent<Text>(); int value = (int)t.alignment;
                t.alignment = (TextAnchor)(value / 3 * 3 + 2 - value % 3);
            }
            if ((flags & 16) != 0)
            {
                // ArabLayoutCompment.UpdateArabLayout(ImageFlipX): mirror image scale.
                var image = arab.GetComponent<Image>();
                if (image != null)
                {
                    var scale = image.transform.localScale;
                    scale.x = -scale.x; image.transform.localScale = scale;
                }
            }
            if ((flags & 512) != 0)
            {
                var grid = arab.GetComponent<GridLayoutGroup>();
                grid.startCorner = (GridLayoutGroup.Corner)((int)grid.startCorner ^ 1);
                int value = (int)grid.childAlignment;
                grid.childAlignment = (TextAnchor)(value / 3 * 3 + 2 - value % 3);
            }
        }
    }
    static void Select()
    {
        bool detail=Example==2;
        Window.transform.Find("pl_detial").gameObject.SetActive(detail);
        Window.transform.Find("pl_list").gameObject.SetActive(!detail);
        Window.transform.Find("UI_Model_Window_Type3/title/btn_back").gameObject.SetActive(detail);
        if(detail)
        {
            var q=Data.questions.First(x=>x.id==5001);
            Window.transform.Find("pl_detial/lbl_question").GetComponent<Text>().text=q.question;
            Window.transform.Find("pl_detial/sv_list/v/c/lbl_detial").GetComponent<Text>().text=q.answer;
        }
        else
        {
            var scroll=Window.transform.Find("pl_list/sv_helperlist").GetComponent<ScrollRect>();
            var container=scroll.content;
            for(int i=container.childCount-1;i>=0;--i)UnityEngine.Object.DestroyImmediate(container.GetChild(i).gameObject);
            Vector2 old=container.rect.size; container.anchorMin=container.anchorMax=container.pivot=new Vector2(.5f,1);container.sizeDelta=old;float y=0;
            // Original mediator flat tree: a parent is inserted only when its title is first seen.
            var seen=new HashSet<int>();var parentById=new Dictionary<int,int>();int currentParent=0;
            foreach(var q in Data.questions){if(seen.Add(q.group_id))currentParent=q.group_id;parentById.Add(q.id,currentParent);}
            foreach(int group in Data.questions.Select(x=>x.group_id).Distinct())
            {
                bool expanded=Example!=0&&(group==900005||group==900004);
                var prefab=AssetDatabase.LoadAssetAtPath<GameObject>(Data.rowPrefab);
                var row=(GameObject)PrefabUtility.InstantiatePrefab(prefab);var r=row.GetComponent<RectTransform>();r.SetParent(container,false);
                Vector2 size=r.rect.size;if(Mathf.Abs(size.x-1074)>0.01f||Mathf.Abs(size.y-43)>0.01f)throw new Exception("Source group dimensions not initialized");r.anchorMin=r.anchorMax=r.pivot=new Vector2(.5f,1);r.sizeDelta=size;r.anchoredPosition=new Vector2(0,-y);y+=size.y;
                row.transform.Find("btn_btn/lbl_title").GetComponent<Text>().text=Data.questions.First(x=>x.group_id==group).group;
                row.transform.Find("btn_btn/img_arrowDown").gameObject.SetActive(expanded);row.transform.Find("btn_btn/img_arrowUp").gameObject.SetActive(!expanded);
                if(!expanded)continue;
                foreach(var q in Data.questions.Where(x=>parentById[x.id]==group))
                {
                    var qr=(GameObject)PrefabUtility.InstantiatePrefab(AssetDatabase.LoadAssetAtPath<GameObject>(Data.questionPrefab));var rt=qr.GetComponent<RectTransform>();rt.SetParent(container,false);
                    Vector2 qs=rt.rect.size;rt.anchorMin=rt.anchorMax=rt.pivot=new Vector2(.5f,1);rt.sizeDelta=qs;rt.anchoredPosition=new Vector2(0,-y);y+=qs.y;
                    qr.transform.Find("btn_btn/lbl_title").GetComponent<Text>().text=q.question;
                }
            }
            container.sizeDelta=new Vector2(old.x,y);container.anchoredPosition=Vector2.zero;
        }
        ApplyChinese();Wait=0;
    }
    static void Tick()
    {
        try
        {
            Guard(); Canvas.ForceUpdateCanvases(); EditorApplication.QueuePlayerLoopUpdate();
            if (++Wait < 8) return;
            // Editor Camera.Render clears its RT; explicitly resubmit static UGUI
            // batches as well as the selection's dirty subtree on every capture.
            foreach (var graphic in Window.GetComponentsInChildren<Graphic>(true)) graphic.SetAllDirty();
            Canvas.ForceUpdateCanvases();
            Camera.Render(); var previous = RenderTexture.active; RenderTexture.active = Target;
            var image = new Texture2D(Data.width, Data.height, TextureFormat.RGBA32, false);
            image.ReadPixels(new Rect(0, 0, Data.width, Data.height), 0, 0); image.Apply();
            var background = Window.transform.Find("UI_Model_Window_Type3/win/frame1").GetComponent<RectTransform>();
            var probes = new[] { new Vector2(.5f, .45f), new Vector2(.1f, .1f), new Vector2(.9f, .1f) };
            var alphas = probes.Select(p => {
                var point = Camera.WorldToScreenPoint(background.TransformPoint(new Vector3(
                    background.rect.xMin + background.rect.width * p.x, background.rect.yMin + background.rect.height * p.y, 0)));
                return image.GetPixel(Mathf.RoundToInt(point.x), Mathf.RoundToInt(point.y)).a;
            }).ToArray();
            if (alphas.Any(a => a < .05f)) throw new Exception("Native window background missing from capture; alpha probes=" + String.Join(",", alphas));
            string png = Output + "/help_example_" + Example + ".png";
            File.WriteAllBytes(png, image.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = previous;
            Frames.Add(new Frame { example = Example, png = png, nodes = Capture(), backgroundProbeAlpha = alphas });
            if (++Example < 4) { Select(); return; }
            Evidence.nativeRendered = true; Evidence.frames = Frames.ToArray(); Evidence.cameraPixelRect = Camera.pixelRect;
            Evidence.screenSize = new Vector2(Screen.width, Screen.height); Evidence.targetSize = new Vector2(Target.width, Target.height);
            if (Hash(Output + "/ReferenceInputs.json") != Evidence.inputSha256) throw new Exception("Input snapshot hash changed");
            File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            CaptureSourceTextMeshes();
            CaptureFontEvidence();
            EditorApplication.update -= Tick; EditorApplication.Exit(0);
        }
        catch (Exception e) { Fail(e); }
    }
    static Node[] Capture()
    {
        return Window.GetComponentsInChildren<RectTransform>(true).Select(r =>
        {
            var corners = new Vector3[4]; r.GetWorldCorners(corners);
            var projected = corners.Select(c => Camera.WorldToScreenPoint(c)).ToArray();
            float left = projected.Min(c => c.x), right = projected.Max(c => c.x);
            float bottom = projected.Min(c => c.y), top = projected.Max(c => c.y);
            var n = new Node { path = AnimationUtility.CalculateTransformPath(r, Window.transform),
                screenBounds = new Rect(left, Data.height - top, right - left, top - bottom),
                anchorsMin = r.anchorMin, anchorsMax = r.anchorMax, pivot = r.pivot, sizeDelta = r.sizeDelta,
                anchoredPosition = r.anchoredPosition, localScale = r.localScale, active = r.gameObject.activeInHierarchy };
            var text = r.GetComponent<Text>(); if (text) { n.text = text.text; n.font = text.font.name; n.fontSize = text.fontSize; n.alignment = (int)text.alignment; }
            var image = r.GetComponent<Image>(); if (image) { n.sprite = image.sprite ? image.sprite.name : "none"; n.imageAlpha = image.canvasRenderer.GetAlpha(); }
            var toggle = r.GetComponent<Toggle>(); if (toggle) n.isOn = toggle.isOn;
            return n;
        }).ToArray();
    }
    [Serializable] public class SourceTextSpec
    {
        public string key, text;
        public float width, height;
        public int fontSize, style, alignment, minSize, maxSize, horizontalOverflow, verticalOverflow;
        public float lineSpacing;
        public bool autoHeight, bestFit;
    }
    [Serializable] public class SourceTextMesh
    {
        public string key, text, atlas, atlasSha256;
        public float width, height, preferredWidth, preferredHeight, lineSpacing, pixelsPerUnit;
        public int fontSize, style, alignment, lineCount;
        public Vector2[] xy, uv;
        public int[] triangles;
        public GlyphEvidence[] glyphs;
    }
    [Serializable] public class SourceTextReport
    {
        public string schema = "rok.help.source_text.v1", inputSha256;
        public string source = "Native UGUI Text CanvasRenderer glyph mesh. XY top-left design pixels; UV remains Unity bottom-left; atlas alpha only. Semantic source text retained.";
        public SourceTextMesh[] texts;
    }
    static void CaptureSourceTextMeshes()
    {
        if (Data.sourceTexts == null || Data.sourceTexts.Length != 222) throw new Exception("Incomplete semantic text export");
        var font = Window.transform.Find("pl_detial/lbl_question").GetComponent<Text>().font;
        var parent = Window.transform.parent; Window.SetActive(false);
        var go = new GameObject("NativeSemanticTextExport", typeof(RectTransform), typeof(CanvasRenderer), typeof(HelpSourceExportText));
        go.transform.SetParent(parent, false); var label = go.GetComponent<HelpSourceExportText>(); label.font = font;
        label.color = Color.white; label.horizontalOverflow = HorizontalWrapMode.Wrap; label.verticalOverflow = VerticalWrapMode.Overflow;
        label.supportRichText = false; label.lineSpacing = 1; label.rectTransform.anchorMin = label.rectTransform.anchorMax = new Vector2(0, 1);
        label.rectTransform.pivot = new Vector2(0, 1); label.rectTransform.anchoredPosition = new Vector2(80, -80);
        foreach (var spec in Data.sourceTexts) font.RequestCharactersInTexture(spec.text, spec.fontSize, (FontStyle)spec.style);
        var results = new List<SourceTextMesh>(); var mesh = new Mesh();
        string atlasDir = Output + "/SourceTextAtlases"; Directory.CreateDirectory(atlasDir);
        foreach (var spec in Data.sourceTexts)
        {
            label.fontSize = spec.fontSize; label.fontStyle = (FontStyle)spec.style; label.alignment = (TextAnchor)spec.alignment;
            label.resizeTextForBestFit = spec.bestFit; label.resizeTextMinSize = spec.minSize; label.resizeTextMaxSize = spec.maxSize;
            label.horizontalOverflow = (HorizontalWrapMode)spec.horizontalOverflow; label.verticalOverflow = (VerticalWrapMode)spec.verticalOverflow; label.lineSpacing = spec.lineSpacing;
            label.text = spec.text; label.rectTransform.sizeDelta = new Vector2(spec.width, spec.height);
            if (spec.autoHeight) label.rectTransform.sizeDelta = new Vector2(spec.width, label.preferredHeight);
            font.RequestCharactersInTexture(spec.text, spec.fontSize, (FontStyle)spec.style);
            label.SetAllDirty(); Canvas.ForceUpdateCanvases(); Camera.Render(); label.CaptureMesh(mesh);
            if (mesh.vertexCount == 0 || mesh.vertexCount % 4 != 0) throw new Exception("Invalid source glyph geometry " + spec.key);
            var glyphs = new List<GlyphEvidence>();
            foreach (char c in spec.text.Distinct())
            {
                if (Char.IsControl(c)) continue;
                CharacterInfo info; if (!font.GetCharacterInfo(c, out info, spec.fontSize, (FontStyle)spec.style)) throw new Exception("Missing source glyph " + spec.key + ":" + (int)c);
                glyphs.Add(new GlyphEvidence { codepoint = c, advance = info.advance, minX = info.minX, maxX = info.maxX, minY = info.minY, maxY = info.maxY,
                    uvBottomLeft = info.uvBottomLeft, uvBottomRight = info.uvBottomRight, uvTopLeft = info.uvTopLeft, uvTopRight = info.uvTopRight });
            }
            string temporary = atlasDir + "/pending.png"; SaveTexture(font.material.mainTexture, temporary);
            string hash = Hash(temporary); string atlas = atlasDir + "/" + hash + ".png";
            if (!File.Exists(atlas)) File.Move(temporary, atlas); else File.Delete(temporary);
            results.Add(new SourceTextMesh { key = spec.key, text = spec.text, width = label.rectTransform.rect.width, height = label.rectTransform.rect.height,
                fontSize = spec.fontSize, style = spec.style, alignment = spec.alignment, preferredWidth = label.preferredWidth, preferredHeight = label.preferredHeight,
                lineSpacing = label.lineSpacing, pixelsPerUnit = label.pixelsPerUnit, lineCount = label.cachedTextGenerator.lineCount,
                xy = mesh.vertices.Select(v => new Vector2(v.x, -v.y)).ToArray(), uv = mesh.uv, triangles = mesh.triangles,
                glyphs = glyphs.ToArray(), atlas = "SourceTextAtlases/" + hash + ".png", atlasSha256 = hash });
        }
        File.WriteAllText(Output + "/source_text_meshes.json", JsonUtility.ToJson(new SourceTextReport { inputSha256 = Evidence.inputSha256, texts = results.ToArray() }, true));
        UnityEngine.Object.DestroyImmediate(mesh); UnityEngine.Object.DestroyImmediate(go); Window.SetActive(true);
    }
    [Serializable] public class GlyphEvidence
    {
        public int codepoint, advance, minX, maxX, minY, maxY;
        public Vector2 uvBottomLeft, uvBottomRight, uvTopLeft, uvTopRight;
    }
    [Serializable] public class FontSample
    {
        public string key, text, font, fontAssetPath, style, sourceStyle, png, atlasPng, atlasSha256;
        public string[] requestedFontNames;
        public int size, atlasWidth, atlasHeight;
        public float preferredWidth, preferredHeight;
        public Vector2 sourceRect;
        public GlyphEvidence[] glyphs;
    }
    [Serializable] public class FontReport
    {
        public string inputSha256, scope = "Native UGUI same-font Normal/Bold glyph metrics and isolated strings; requested font names do not identify the physical CJK fallback face.";
        public bool physicalFallbackFaceResolved = false;
        public FontSample[] samples;
    }
    static void SaveTexture(Texture texture, string path)
    {
        var previous = RenderTexture.active;
        var rt = RenderTexture.GetTemporary(texture.width, texture.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Default);
        Graphics.Blit(texture, rt); RenderTexture.active = rt;
        var copy = new Texture2D(texture.width, texture.height, TextureFormat.RGBA32, false);
        copy.ReadPixels(new Rect(0, 0, texture.width, texture.height), 0, 0); copy.Apply();
        File.WriteAllBytes(path, copy.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(copy);
        RenderTexture.active = previous; RenderTexture.ReleaseTemporary(rt);
    }
    static void CaptureFontEvidence()
    {
        var group = AssetDatabase.LoadAssetAtPath<GameObject>(Data.rowPrefab).transform.Find("btn_btn/lbl_title").GetComponent<Text>();
        var question = Window.transform.Find("pl_detial/lbl_question").GetComponent<Text>();
        var button = Window.transform.Find("pl_list/btn_head/text").GetComponent<Text>();
        var service = Window.transform.Find("pl_list/lbl_service").GetComponent<Text>();
        var sources = new List<Text>(); var texts = new List<string>(); var keys = new List<string>();
        foreach (var q in Data.questions.GroupBy(x => x.group_id).Select(x => x.First())) { sources.Add(group); texts.Add(q.group); keys.Add("group_" + q.group_id); }
        sources.Add(question); texts.Add(Data.questions.First(x => x.id == 5001).question); keys.Add("question_5001");
        sources.Add(button); texts.Add(button.text); keys.Add("service_button");
        sources.Add(service); texts.Add(service.text); keys.Add("service_description");
        var parent = Window.transform.parent; Window.SetActive(false);
        var go = new GameObject("IsolatedNativeFontSample", typeof(RectTransform), typeof(CanvasRenderer), typeof(Text));
        go.transform.SetParent(parent, false); var label = go.GetComponent<Text>();
        label.color = Color.white; label.alignment = TextAnchor.UpperLeft; label.horizontalOverflow = HorizontalWrapMode.Wrap;
        label.verticalOverflow = VerticalWrapMode.Overflow; label.supportRichText = false;
        var records = new List<FontSample>();
        for (int i = 0; i < sources.Count; ++i) foreach (var style in new[] { FontStyle.Normal, FontStyle.Bold })
        {
            var source = sources[i]; label.font = source.font; label.fontSize = source.fontSize; label.fontStyle = style; label.text = texts[i];
            label.lineSpacing = source.lineSpacing; label.rectTransform.anchorMin = label.rectTransform.anchorMax = new Vector2(0, 1);
            label.rectTransform.pivot = new Vector2(0, 1); label.rectTransform.anchoredPosition = new Vector2(80, -80);
            label.rectTransform.sizeDelta = source.rectTransform.rect.size;
            label.font.RequestCharactersInTexture(label.text, label.fontSize, style); label.SetAllDirty(); Canvas.ForceUpdateCanvases(); Camera.Render();
            string stem = "font_" + keys[i] + "_" + style;
            SaveTexture(Target, Output + "/" + stem + ".png");
            var glyphs = new List<GlyphEvidence>();
            foreach (char c in label.text.Distinct())
            {
                if (Char.IsControl(c)) continue;
                CharacterInfo info; if (!label.font.GetCharacterInfo(c, out info, label.fontSize, style)) throw new Exception("Missing native glyph " + (int)c);
                glyphs.Add(new GlyphEvidence { codepoint = c, advance = info.advance, minX = info.minX, maxX = info.maxX, minY = info.minY, maxY = info.maxY,
                    uvBottomLeft = info.uvBottomLeft, uvBottomRight = info.uvBottomRight, uvTopLeft = info.uvTopLeft, uvTopRight = info.uvTopRight });
            }
            var atlas = label.font.material.mainTexture; string atlasPath = Output + "/" + stem + "_atlas.png"; SaveTexture(atlas, atlasPath);
            records.Add(new FontSample { key = keys[i], text = label.text, font = label.font.name, fontAssetPath = AssetDatabase.GetAssetPath(label.font), requestedFontNames = label.font.fontNames,
                style = style.ToString(), sourceStyle = source.fontStyle.ToString(), size = label.fontSize, sourceRect = label.rectTransform.sizeDelta,
                preferredWidth = label.preferredWidth, preferredHeight = label.preferredHeight, png = Output + "/" + stem + ".png", atlasPng = atlasPath,
                atlasSha256 = Hash(atlasPath), atlasWidth = atlas.width, atlasHeight = atlas.height, glyphs = glyphs.ToArray() });
        }
        File.WriteAllText(Output + "/font_evidence.json", JsonUtility.ToJson(new FontReport { inputSha256 = Evidence.inputSha256, samples = records.ToArray() }, true));
        UnityEngine.Object.DestroyImmediate(go);
    }
    static string Hash(string path) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static void Fail(Exception e)
    {
        EditorApplication.update -= Tick; Debug.LogException(e);
        if (Output != null) File.WriteAllText(Output + "/render_failure.txt", e.ToString());
        EditorApplication.Exit(1);
    }
}

// Uses the original UGUI Text.OnPopulateMesh path including pixelsPerUnit and pixel-adjust rounding.
public sealed class HelpSourceExportText : Text
{
    public void CaptureMesh(Mesh mesh)
    {
        using (var vertices = new VertexHelper()) { base.OnPopulateMesh(vertices); vertices.FillMesh(mesh); }
    }
}
