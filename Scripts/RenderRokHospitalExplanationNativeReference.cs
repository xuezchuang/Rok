using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;

// Original prefab and native UGUI, with only the source presentation method bodies bound locally.
// No complete mediator, inventory, treatment command, account state or physical input claim.
public static class RenderRokHospitalExplanationNativeReference
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityHospitalExplanationNativeReferenceProbe";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Inputs { public string scope, prefab; public int width, height; public Entry[] texts; }
    [Serializable] public class Glyph { public int code; public bool found; public int advance, minX, maxX, minY, maxY; public Vector2 uvBottomLeft, uvTopRight; }
    [Serializable] public class Node { public string path, text, font, sprite; public bool active; public Rect rect; public Vector2 anchoredPosition, sizeDelta; public Vector3 localScale; public int fontSize; public float alpha; }
    [Serializable] public class Frame
    {
        public string name, png, sha256, text, font, fontPath;
        public int windowInstance, labelInstance, fontSize, alignment, lineCount, characterCount;
        public float preferredHeight, contentHeight, normalizedScroll, lineSpacing;
        public bool explanationActive, rightActive, countActive, backActive, richText;
        public Vector2 contentPosition, labelPosition, viewportSize;
        public Node[] nodes; public Glyph[] glyphs; public Vector2[] vertices; public Color32[] colors;
    }
    [Serializable] public class Report { public string scope, unityVersion, inputSha256; public bool nativeRendered, fullMediator, sourcePrefabModified; public Frame[] frames; public string failure; }
    static Inputs Data; static GameObject Window; static Camera Camera; static RenderTexture Target;
    static Canvas Canvas; static string Output; static bool Showed; static int Step, Wait;
    static readonly List<Frame> Frames = new List<Frame>();
    static readonly HashSet<ArabLayoutCompment> Applied = new HashSet<ArabLayoutCompment>();
    static readonly string[] Names = { "01_explanation_top", "02_explanation_bottom", "03_back_main", "04_explanation_preserved", "05_fresh_window_explanation" };
    static Report Evidence;
    static Transform Find(string path) { var t = Window.transform.Find(path); if (!t) throw new Exception("Missing original node " + path); return t; }
    static Text Label { get { return Find("Rect/Right/pl_explain/sv_explain_view/v_explain_view/c_explain_view/lbl_explain").GetComponent<Text>(); } }
    static ScrollRect Scroll { get { return Find("Rect/Right/pl_explain/sv_explain_view").GetComponent<ScrollRect>(); } }
    static Transform Back { get { return Find("UI_Model_Window/title/btn_back"); } }
    static string Hash(string path) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase)
            || Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong isolated project/version");
    }
    public static void Run()
    {
        try
        {
            Guard(); if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Source Gamma required");
            Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff"); Directory.CreateDirectory(Output);
            File.Copy(Project + "/ReferenceInputs.json", Output + "/ReferenceInputs.json");
            Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
            Evidence = new Report { scope = Data.scope, unityVersion = Application.unityVersion, inputSha256 = Hash(Output + "/ReferenceInputs.json") };
            if (Data.width != 1440 || Data.height != 810 || !Data.texts.Any(x => x.id == 181001)) throw new Exception("Invalid inputs");
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            Camera = new GameObject("ReferenceCamera").AddComponent<Camera>(); Camera.clearFlags = CameraClearFlags.SolidColor;
            Camera.backgroundColor = Color.clear; Camera.allowHDR = false; Camera.allowMSAA = false;
            Target = new RenderTexture(Data.width, Data.height, 24, RenderTextureFormat.ARGB32); Target.Create(); Camera.targetTexture = Target;
            Canvas = new GameObject("ReferenceCanvas", typeof(RectTransform), typeof(Canvas)).GetComponent<Canvas>();
            Canvas.renderMode = RenderMode.ScreenSpaceCamera; Canvas.worldCamera = Camera; Canvas.planeDistance = 1;
            Instantiate(); Explain(); Frames.Clear(); Step = 0; Wait = 0; EditorApplication.update += Tick;
        }
        catch (Exception e) { Fail(e); }
    }
    static void Instantiate()
    {
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab); if (!prefab) throw new Exception("Missing source prefab");
        Window = (GameObject)PrefabUtility.InstantiatePrefab(prefab); Window.transform.SetParent(Canvas.transform, false); Showed = false;
        foreach (var a in Window.GetComponentsInChildren<Animator>(true)) a.enabled = false;
        var clip = AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim");
        if (!clip) throw new Exception("Missing source T1 clip"); clip.SampleAnimation(Window, clip.length);
        foreach (var label in Window.GetComponentsInChildren<LanguageText>(true))
        {
            var e = Data.texts.FirstOrDefault(x => x.id == label.SourceLanguageId); if (e != null) label.text = e.text;
            if (!label.font) throw new Exception("Missing source font " + label.name);
        }
        ApplyChinese();
        foreach (var t in Window.GetComponentsInChildren<Transform>(true))
            if (GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(t.gameObject) != 0) throw new Exception("Missing script " + t.name);
        OnBack(); UnityEngine.Canvas.ForceUpdateCanvases();
    }
    static void Explain()
    {
        Find("Rect/Right/pl_explain").gameObject.SetActive(true); Find("Rect/Right/pl_right_content").gameObject.SetActive(false);
        Find("Rect/Left/pl_count").gameObject.SetActive(false); Back.gameObject.SetActive(true);
        if (!Showed)
        {
            Showed = true; Label.text = Data.texts.Single(x => x.id == 181001).text;
            float height = Label.preferredHeight - Label.rectTransform.anchoredPosition.y;
            Scroll.content.sizeDelta = new Vector2(Scroll.content.sizeDelta.x, height);
        }
    }
    static void OnBack()
    {
        Find("Rect/Right/pl_explain").gameObject.SetActive(false); Find("Rect/Right/pl_right_content").gameObject.SetActive(true);
        Find("Rect/Left/pl_count").gameObject.SetActive(true); Back.gameObject.SetActive(false);
    }
    static void Tick()
    {
        try
        {
            Guard(); UnityEngine.Canvas.ForceUpdateCanvases(); EditorApplication.QueuePlayerLoopUpdate(); if (++Wait < 8) return;
            foreach (var g in Window.GetComponentsInChildren<Graphic>(true)) g.SetAllDirty();
            UnityEngine.Canvas.ForceUpdateCanvases(); Camera.Render();
            var old = RenderTexture.active; RenderTexture.active = Target;
            var image = new Texture2D(Data.width, Data.height, TextureFormat.RGBA32, false);
            image.ReadPixels(new Rect(0, 0, Data.width, Data.height), 0, 0); image.Apply();
            string path = Output + "/" + Names[Step] + ".png"; File.WriteAllBytes(path, image.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = old;
            Frames.Add(Capture(path));
            if (++Step == Names.Length)
            {
                Validate(); Evidence.nativeRendered = true; Evidence.frames = Frames.ToArray();
                File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
                EditorApplication.update -= Tick; EditorApplication.Exit(0); return;
            }
            if (Step == 1) { Scroll.StopMovement(); Scroll.verticalNormalizedPosition = 0; }
            if (Step == 2) OnBack();
            if (Step == 3) Explain();
            if (Step == 4) { UnityEngine.Object.DestroyImmediate(Window); Instantiate(); Explain(); }
            Wait = 0;
        }
        catch (Exception e) { Fail(e); }
    }
    static Frame Capture(string path)
    {
        var l = Label; l.font.RequestCharactersInTexture(l.text, l.fontSize, l.fontStyle);
        var settings = l.GetGenerationSettings(l.rectTransform.rect.size); var gen = new TextGenerator();
        if (!gen.Populate(l.text, settings)) throw new Exception("TextGenerator failed");
        var glyphs = new List<Glyph>();
        foreach (char c in l.text.Distinct())
        {
            if (Char.IsControl(c)) continue; CharacterInfo info; bool found = l.font.GetCharacterInfo(c, out info, l.fontSize, l.fontStyle);
            glyphs.Add(new Glyph { code = c, found = found, advance = info.advance, minX = info.minX, maxX = info.maxX, minY = info.minY, maxY = info.maxY, uvBottomLeft = info.uvBottomLeft, uvTopRight = info.uvTopRight });
        }
        return new Frame { name = Names[Step], png = path, sha256 = Hash(path), windowInstance = Window.GetInstanceID(), labelInstance = l.GetInstanceID(),
            text = l.text, font = l.font.name, fontPath = AssetDatabase.GetAssetPath(l.font), fontSize = l.fontSize, alignment = (int)l.alignment,
            lineSpacing = l.lineSpacing, richText = l.supportRichText, preferredHeight = l.preferredHeight, contentHeight = Scroll.content.rect.height,
            normalizedScroll = Scroll.verticalNormalizedPosition, contentPosition = Scroll.content.anchoredPosition, labelPosition = l.rectTransform.anchoredPosition,
            viewportSize = Scroll.viewport.rect.size, lineCount = gen.lineCount, characterCount = gen.characterCount,
            vertices = gen.verts.Select(v => new Vector2(v.position.x, v.position.y)).ToArray(), colors = gen.verts.Select(v => v.color).ToArray(), glyphs = glyphs.ToArray(),
            explanationActive = Find("Rect/Right/pl_explain").gameObject.activeInHierarchy, rightActive = Find("Rect/Right/pl_right_content").gameObject.activeInHierarchy,
            countActive = Find("Rect/Left/pl_count").gameObject.activeInHierarchy, backActive = Back.gameObject.activeInHierarchy,
            nodes = Window.GetComponentsInChildren<RectTransform>(true).Select(r => {
                var corners = new Vector3[4]; r.GetWorldCorners(corners); var p = corners.Select(c => Camera.WorldToScreenPoint(c)).ToArray();
                var text = r.GetComponent<Text>(); var img = r.GetComponent<Image>();
                return new Node { path = AnimationUtility.CalculateTransformPath(r, Window.transform), active = r.gameObject.activeInHierarchy,
                    rect = new Rect(p.Min(c => c.x), Data.height - p.Max(c => c.y), p.Max(c => c.x) - p.Min(c => c.x), p.Max(c => c.y) - p.Min(c => c.y)),
                    anchoredPosition = r.anchoredPosition, sizeDelta = r.sizeDelta, localScale = r.localScale,
                    text = text ? text.text : null, font = text && text.font ? text.font.name : null, fontSize = text ? text.fontSize : 0,
                    sprite = img && img.sprite ? img.sprite.name : null, alpha = img ? img.color.a * img.canvasRenderer.GetAlpha() : 0 }; }).ToArray() };
    }
    static void Validate()
    {
        if (Frames.Count != 5) throw new Exception("Incomplete states");
        foreach (var f in Frames)
        {
            bool explain = f.name != Names[2];
            if (f.explanationActive != explain || f.rightActive == explain || f.countActive == explain || f.backActive != explain) throw new Exception("Incorrect source visibility " + f.name);
            if (f.text != Data.texts.Single(x => x.id == 181001).text || !f.richText || f.lineCount < 2 || f.vertices.Length == 0) throw new Exception("Missing actual runtime text/glyphs");
            if (Math.Abs(f.contentHeight - (f.preferredHeight - f.labelPosition.y)) > .05f) throw new Exception("OnExplain source height mismatch");
        }
        if (Frames[0].contentHeight <= Frames[0].viewportSize.y || Math.Abs(Frames[1].normalizedScroll) > .001f) throw new Exception("Scroll bottom not reached");
        if (Frames[1].windowInstance != Frames[3].windowInstance || Frames[1].labelInstance != Frames[3].labelInstance || Vector2.Distance(Frames[1].contentPosition, Frames[3].contentPosition) > .05f) throw new Exception("Same-view return lost scroll");
        if (Frames[4].windowInstance == Frames[0].windowInstance || Math.Abs(Frames[4].normalizedScroll - Frames[0].normalizedScroll) > .001f) throw new Exception("Fresh original prefab did not reset");
    }
    static void Fail(Exception e)
    {
        Debug.LogException(e); EditorApplication.update -= Tick;
        if (Output != null) { if (Evidence == null) Evidence = new Report(); Evidence.failure = e.ToString(); Evidence.frames = Frames.ToArray(); File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true)); }
        EditorApplication.Exit(1);
    }
    // Filled from the independently verified Help probe's source ArabLayout flag handling.
    static void ApplyChinese()
    {
        foreach (var arab in Window.GetComponentsInChildren<ArabLayoutCompment>(true))
        {
            int flags = arab.SourceFlags; if ((flags & ~635) != 0) throw new Exception("Unsupported source Arab flags " + flags);
            if (!Applied.Add(arab) || (flags & 1) != 0) continue;
            if ((flags & 2) != 0) { var r = arab.GetComponent<RectTransform>(); var min = r.anchorMin; var max = r.anchorMax; r.anchorMin = new Vector2(1-max.x,min.y); r.anchorMax = new Vector2(1-min.x,max.y); r.pivot = new Vector2(1-r.pivot.x,r.pivot.y); r.anchoredPosition = new Vector2(-r.anchoredPosition.x,r.anchoredPosition.y); }
            if ((flags & 8) != 0) { var t = arab.GetComponent<Text>(); int v = (int)t.alignment; t.alignment = (TextAnchor)(v/3*3+2-v%3); }
            if ((flags & 16) != 0) { var i = arab.GetComponent<Image>(); if (i) { var s = i.transform.localScale; s.x = -s.x; i.transform.localScale = s; } }
            if ((flags & 32) != 0) { var s = arab.GetComponent<Slider>(); if (s && (s.direction == Slider.Direction.LeftToRight || s.direction == Slider.Direction.RightToLeft)) s.direction = s.direction == Slider.Direction.LeftToRight ? Slider.Direction.RightToLeft : Slider.Direction.LeftToRight; }
            if ((flags & 64) != 0) { var r = arab.GetComponent<RectTransform>(); if (r) { var s = r.localScale; s.x = -s.x; r.localScale = s; } }
            if ((flags & 512) != 0) { var g = arab.GetComponent<GridLayoutGroup>(); g.startCorner = (GridLayoutGroup.Corner)((int)g.startCorner ^ 1); int v = (int)g.childAlignment; g.childAlignment = (TextAnchor)(v/3*3+2-v%3); }
        }
    }
}
