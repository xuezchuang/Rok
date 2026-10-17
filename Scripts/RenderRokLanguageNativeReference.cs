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
public static class RenderRokLanguageNativeReference
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityLanguageNativeReferenceProbe";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Language { public int id, textId; public string language; }
    [Serializable] public class Inputs { public string scope, prefab, rowPrefab; public int width, height; public Entry[] texts; public Language[] languages; public int[] selectedExamples; }
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
    [Serializable] public class Frame { public int selectedLanguage; public string png; public Node[] nodes; public float[] backgroundProbeAlpha; }
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
    static readonly List<GameToggle> Toggles = new List<GameToggle>();
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
            Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Project + "/ReferenceInputs.json"));
            if (Data.width != 1440 || Data.height != 810 || !Data.languages.Select(x => x.id).SequenceEqual(new[] { 1, 10, 37, 40 })) throw new Exception("Unexpected source window/config");
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
            var scroll = Window.GetComponentInChildren<ScrollRect>(true);
            if (scroll == null || scroll.content == null) throw new Exception("Missing source scroll/content");
            var container = scroll.content;
            // ListView.SetContainerLayout preserves actual width and uses TopToDownPreset.
            Vector2 oldSize = container.rect.size;
            container.pivot = container.anchorMin = container.anchorMax = new Vector2(.5f, 1);
            container.sizeDelta = oldSize;
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.rowPrefab);
            float height = prefab.GetComponent<RectTransform>().rect.height;
            if (height != 111) throw new Exception("Unexpected source row height");
            container.sizeDelta = new Vector2(oldSize.x, height * 2);
            container.anchoredPosition = Vector2.zero;
            var group = scroll.gameObject.AddComponent<ToggleGroup>();
            var strings = Data.texts.ToDictionary(x => x.id, x => x.text);
            Toggles.Clear();
            for (int row = 0; row < 2; ++row)
            {
                var item = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
                var rect = item.GetComponent<RectTransform>(); rect.SetParent(container, false);
                rect.pivot = rect.anchorMin = rect.anchorMax = new Vector2(.5f, 1);
                rect.anchoredPosition = new Vector2(0, -row * height); rect.localScale = Vector3.one;
                for (int col = 0; col < 2; ++col)
                {
                    var child = rect.GetChild(col);
                    var toggle = child.GetComponentInChildren<GameToggle>(true);
                    var label = child.GetComponentInChildren<LanguageText>(true);
                    if (!toggle || !label) throw new Exception("Missing source option subtree");
                    toggle.isOn = false; toggle.group = group;
                    label.text = strings[Data.languages[row * 2 + col].textId]; Toggles.Add(toggle);
                }
            }
            ApplyChinese();
            foreach (var label in Window.GetComponentsInChildren<LanguageText>(true))
            {
                if (label.SourceLanguageId >= 0) label.text = strings[label.SourceLanguageId];
                if (label.font == null) throw new Exception("Missing source font: " + label.name);
                label.font.RequestCharactersInTexture(label.text, label.fontSize, label.fontStyle);
            }
            foreach (var node in Window.GetComponentsInChildren<Transform>(true))
                if (GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(node.gameObject) != 0) throw new Exception("Missing source script: " + node.name);
            Evidence = new Report { scope = Data.scope, unityVersion = Application.unityVersion, inputSha256 = Hash(Project + "/ReferenceInputs.json") };
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
        for (int i = 0; i < Toggles.Count; ++i) Toggles[i].isOn = Data.languages[i].id == Data.selectedExamples[Example];
        Wait = 0;
    }
    static void Tick()
    {
        try
        {
            Guard(); Canvas.ForceUpdateCanvases(); EditorApplication.QueuePlayerLoopUpdate();
            if (++Wait < 8) return;
            foreach (var label in Window.GetComponentsInChildren<LanguageText>(true))
                if (ArabicSupport.ArabicFixer.IsRtl(label.text) && (label.text.Contains("\n") || label.preferredWidth > label.rectTransform.rect.width))
                    throw new Exception("Arabic single-line adapter cannot represent source wrapping");
            if (Toggles.Count(x => x.isOn) != 1) throw new Exception("Expected exactly one native selected toggle");
            // Editor Camera.Render clears its RT; explicitly resubmit static UGUI
            // batches as well as the selection's dirty subtree on every capture.
            foreach (var graphic in Window.GetComponentsInChildren<Graphic>(true)) graphic.SetAllDirty();
            Canvas.ForceUpdateCanvases();
            Camera.Render(); var previous = RenderTexture.active; RenderTexture.active = Target;
            var image = new Texture2D(Data.width, Data.height, TextureFormat.RGBA32, false);
            image.ReadPixels(new Rect(0, 0, Data.width, Data.height), 0, 0); image.Apply();
            var background = Window.transform.Find("UI_Model_Window_TypeMid/bg").GetComponent<RectTransform>();
            var probes = new[] { new Vector2(.5f, .45f), new Vector2(.1f, .1f), new Vector2(.9f, .1f) };
            var alphas = probes.Select(p => {
                var point = Camera.WorldToScreenPoint(background.TransformPoint(new Vector3(
                    background.rect.xMin + background.rect.width * p.x, background.rect.yMin + background.rect.height * p.y, 0)));
                return image.GetPixel(Mathf.RoundToInt(point.x), Mathf.RoundToInt(point.y)).a;
            }).ToArray();
            if (alphas.Any(a => a < .05f)) throw new Exception("Native window background missing from capture; alpha probes=" + String.Join(",", alphas));
            string png = Output + "/language_selected_" + Data.selectedExamples[Example] + ".png";
            File.WriteAllBytes(png, image.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = previous;
            Frames.Add(new Frame { selectedLanguage = Data.selectedExamples[Example], png = png, nodes = Capture(), backgroundProbeAlpha = alphas });
            if (++Example < Data.selectedExamples.Length) { Select(); return; }
            Evidence.nativeRendered = true; Evidence.frames = Frames.ToArray(); Evidence.cameraPixelRect = Camera.pixelRect;
            Evidence.screenSize = new Vector2(Screen.width, Screen.height); Evidence.targetSize = new Vector2(Target.width, Target.height);
            File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
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
    static string Hash(string path) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static void Fail(Exception e)
    {
        EditorApplication.update -= Tick; Debug.LogException(e);
        if (Output != null) File.WriteAllText(Output + "/render_failure.txt", e.ToString());
        EditorApplication.Exit(1);
    }
}
