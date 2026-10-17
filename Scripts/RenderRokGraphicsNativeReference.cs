using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;

// Isolated native UGUI rendering of the unchanged source prefab. Language/Arab
// adapters are explicit source-derived presentation bindings, not full gameplay.
public static class RenderRokGraphicsNativeReference
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityGraphicsNativeReferenceProbe";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Inputs { public string scope, prefab, unity_version; public int width, height, nodes, framing_padding_pixels; public Entry[] texts; }
    [Serializable] public class SourceRect
    {
        public Vector2 anchorMin, anchorMax, pivot, anchoredPosition, sizeDelta;
        public Vector3 scale;
    }
    [Serializable] public class NodeEvidence
    {
        public string path, name, sprite, shader, text, font;
        public bool active;
        public int sibling, fontSize, textAlignment;
        public float x, y, width, height, imageAlpha;
    }
    [Serializable] public class FrameEvidence { public int referenceSliderLevel; public string png; public NodeEvidence[] nodes; }
    [Serializable] public class Report
    {
        public string scope, unityVersion, colorSpace, graphicsDevice, inputsSha256;
        public int systemMemoryMB, recommendedGraphicLevel;
        public bool nativeRendered, fullMediator = false, sourcePrefabModified = false;
        public string[] omissions = { "full mediator and live GameView", "audio and quality application", "saved player graphics preferences", "surrounding settings window" };
        public FrameEvidence[] frames;
        public Rect projectedCardBeforeFraming, projectedCardAfterFraming, cameraPixelRect;
        public Vector2 screenSize, renderTargetSize, framingTranslation;
        public SourceRect sourceRootBeforeFraming, sourceRootAfterFraming;
    }
    static Inputs Data;
    static Camera Camera;
    static RenderTexture Target;
    static RectTransform Card;
    static RectTransform FramingRoot, CanvasRect;
    static string OutputDirectory;
    static Report Evidence;
    static readonly List<FrameEvidence> Frames = new List<FrameEvidence>();
    static int Level, WaitFrames;

    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase)
            || Application.unityVersion != "2019.4.28f1c1")
            throw new Exception("Refusing graphics reference writes: wrong project or Unity version");
    }
    public static void Run()
    {
        Guard();
        try
        {
            Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Project + "/ReferenceInputs.json"));
            OutputDirectory = Project + "/Artifacts/framed_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
            Directory.CreateDirectory(OutputDirectory);
            if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Source PlayerSettings requires Gamma color space");
            if (Data.width != 1040 || Data.height != 407 || Data.nodes != 32 || Data.framing_padding_pixels != 20) throw new Exception("Unexpected extracted source shape");
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab);
            if (prefab == null) throw new Exception("Original graphics prefab did not import");
            var instance = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
            if (instance.GetComponentsInChildren<Transform>(true).Any(t => GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(t.gameObject) != 0))
                throw new Exception("Source prefab has unresolved component GUIDs");
            Camera = new GameObject("IsolatedReferenceCamera").AddComponent<Camera>();
            Camera.clearFlags = CameraClearFlags.SolidColor;
            Camera.backgroundColor = Color.clear;
            Camera.allowHDR = false; Camera.allowMSAA = false;
            Target = new RenderTexture(Data.width, Data.height, 24, RenderTextureFormat.ARGB32);
            Target.Create(); Camera.targetTexture = Target;
            var canvasObject = new GameObject("IsolatedUGUI", typeof(RectTransform), typeof(Canvas));
            var canvas = canvasObject.GetComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceCamera; canvas.worldCamera = Camera; canvas.planeDistance = 1;
            CanvasRect = canvasObject.GetComponent<RectTransform>();
            FramingRoot = new GameObject("CaptureOnlyFramingOffset", typeof(RectTransform)).GetComponent<RectTransform>();
            FramingRoot.SetParent(CanvasRect, false);
            FramingRoot.anchorMin = Vector2.zero; FramingRoot.anchorMax = Vector2.one;
            FramingRoot.sizeDelta = Vector2.zero;
            Card = instance.GetComponent<RectTransform>();
            Card.SetParent(FramingRoot, false);
            if (Card.sizeDelta != new Vector2(1000, 367)) throw new Exception("Original root size changed");
            ApplyChinese(instance);
            var strings = Data.texts.ToDictionary(e => e.id, e => e.text);
            int recommended = SystemInfo.systemMemorySize > 3000 ? 3 : SystemInfo.systemMemorySize > 2000 ? 2 : 1;
            foreach (var text in instance.GetComponentsInChildren<LanguageText>(true))
            {
                string value;
                if (!strings.TryGetValue(text.SourceLanguageId, out value)) throw new Exception("Missing source language ID: " + text.SourceLanguageId);
                text.text = text.gameObject.name == "lbl_dec2" ? String.Format(strings[300227], strings[300220 + recommended]) : value;
                if (text.font == null) throw new Exception("Missing original serialized font on " + text.name);
                text.font.RequestCharactersInTexture(text.text, text.fontSize, text.fontStyle);
            }
            Evidence = new Report { scope = Data.scope, unityVersion = Application.unityVersion,
                colorSpace = QualitySettings.activeColorSpace.ToString(), graphicsDevice = SystemInfo.graphicsDeviceName,
                systemMemoryMB = SystemInfo.systemMemorySize, recommendedGraphicLevel = recommended,
                inputsSha256 = Hash(Project + "/ReferenceInputs.json") };
            Evidence.sourceRootBeforeFraming = CaptureSourceRect();
            Frames.Clear(); Level = 1; SetLevel();
            EditorApplication.update += Tick;
        }
        catch (Exception error) { Fail(error); }
    }
    static void ApplyChinese(GameObject instance)
    {
        foreach (var layout in instance.GetComponentsInChildren<ArabLayoutCompment>(true))
        {
            int flags = layout.SourceFlags;
            if ((flags & 1) != 0) continue; // Source Awake toggles only when Preview == IsArabic(false).
            if ((flags & ~42) != 0) throw new Exception("Unsupported source ArabLayout flags: " + flags);
            if ((flags & 2) != 0)
            {
                var rect = layout.GetComponent<RectTransform>();
                Vector2 min = rect.anchorMin, max = rect.anchorMax, pivot = rect.pivot, position = rect.anchoredPosition;
                rect.anchorMin = new Vector2(1 - max.x, min.y); rect.anchorMax = new Vector2(1 - min.x, max.y);
                rect.pivot = new Vector2(1 - pivot.x, pivot.y); rect.anchoredPosition = new Vector2(-position.x, position.y);
            }
            if ((flags & 8) != 0)
            {
                var text = layout.GetComponent<Text>(); int alignment = (int)text.alignment;
                text.alignment = (TextAnchor)(alignment / 3 * 3 + 2 - alignment % 3);
            }
            if ((flags & 32) != 0)
            {
                var slider = layout.GetComponent<Slider>();
                if (slider.direction == Slider.Direction.LeftToRight) slider.direction = Slider.Direction.RightToLeft;
                else if (slider.direction == Slider.Direction.RightToLeft) slider.direction = Slider.Direction.LeftToRight;
                else throw new Exception("Unexpected source vertical graphics slider");
            }
        }
    }
    static void SetLevel()
    {
        foreach (var slider in Card.GetComponentsInChildren<Slider>(true))
        {
            if (!slider.wholeNumbers || slider.minValue != 1 || slider.maxValue != 3) throw new Exception("Unexpected source slider range");
            slider.SetValueWithoutNotify(Level); // Presentation fixtures, never application preference callbacks.
        }
        WaitFrames = 0;
    }
    static void Tick()
    {
        try
        {
            Guard(); Canvas.ForceUpdateCanvases(); EditorApplication.QueuePlayerLoopUpdate();
            if (++WaitFrames < 5) return;
            FrameSourceCard();
            Camera.Render(); var previous = RenderTexture.active; RenderTexture.active = Target;
            var image = new Texture2D(Data.width, Data.height, TextureFormat.RGBA32, false);
            image.ReadPixels(new Rect(0, 0, Data.width, Data.height), 0, 0); image.Apply();
            string path = OutputDirectory + "/graphics_native_level_" + Level + ".png";
            File.WriteAllBytes(path, image.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = previous;
            Frames.Add(new FrameEvidence { referenceSliderLevel = Level, png = path, nodes = CaptureNodes() });
            if (++Level <= 3) { SetLevel(); return; }
            Evidence.frames = Frames.ToArray(); Evidence.nativeRendered = true;
            File.WriteAllText(OutputDirectory + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            EditorApplication.update -= Tick;
            Debug.Log("Rok graphics source-derived native UGUI reference complete; not full mediator/GameView.");
            EditorApplication.Exit(0);
        }
        catch (Exception error) { Fail(error); }
    }
    static SourceRect CaptureSourceRect()
    {
        return new SourceRect { anchorMin = Card.anchorMin, anchorMax = Card.anchorMax,
            pivot = Card.pivot, anchoredPosition = Card.anchoredPosition, sizeDelta = Card.sizeDelta, scale = Card.localScale };
    }
    // Bounds use screenshot coordinates (top-left origin), measured by Unity's camera.
    static Rect ProjectCard()
    {
        var corners = new Vector3[4]; Card.GetWorldCorners(corners);
        var bottom = Camera.WorldToScreenPoint(corners[0]);
        var top = Camera.WorldToScreenPoint(corners[2]);
        return new Rect(bottom.x, Data.height - top.y, top.x - bottom.x, top.y - bottom.y);
    }
    static void FrameSourceCard()
    {
        var before = ProjectCard();
        if (Level == 1) Evidence.projectedCardBeforeFraming = before;
        Vector2 current, desired;
        Vector3 center = Camera.WorldToScreenPoint(Card.TransformPoint(Card.rect.center));
        if (!RectTransformUtility.ScreenPointToLocalPointInRectangle(CanvasRect, center, Camera, out current)
            || !RectTransformUtility.ScreenPointToLocalPointInRectangle(CanvasRect, new Vector2(Data.width / 2f, Data.height / 2f), Camera, out desired))
            throw new Exception("Cannot project capture framing into canvas");
        // Source root retains its list placement (-104.100006 Y). Only the capture wrapper moves.
        FramingRoot.anchoredPosition += desired - current;
        Canvas.ForceUpdateCanvases();
        var after = ProjectCard();
        if (Mathf.Abs(after.x - 20) > .01f || Mathf.Abs(after.y - 20) > .01f
            || Mathf.Abs(after.width - 1000) > .01f || Mathf.Abs(after.height - 367) > .01f)
            throw new Exception("Native card framing is not 20,20,1000,367: " + after);
        Evidence.sourceRootAfterFraming = CaptureSourceRect();
        if (JsonUtility.ToJson(Evidence.sourceRootBeforeFraming) != JsonUtility.ToJson(Evidence.sourceRootAfterFraming))
            throw new Exception("Capture framing altered source root RectTransform");
        Evidence.projectedCardAfterFraming = after; Evidence.cameraPixelRect = Camera.pixelRect;
        Evidence.screenSize = new Vector2(Screen.width, Screen.height);
        Evidence.renderTargetSize = new Vector2(Target.width, Target.height);
        Evidence.framingTranslation = FramingRoot.anchoredPosition;
    }
    static NodeEvidence[] CaptureNodes()
    {
        return Card.GetComponentsInChildren<RectTransform>(true).Select(rect =>
        {
            var corners = new Vector3[4]; rect.GetWorldCorners(corners);
            Vector3 bottom = Card.InverseTransformPoint(corners[0]), top = Card.InverseTransformPoint(corners[2]);
            var row = new NodeEvidence { name = rect.name, path = AnimationUtility.CalculateTransformPath(rect, Card),
                sibling = rect.GetSiblingIndex(), active = rect.gameObject.activeInHierarchy,
                x = bottom.x - Card.rect.xMin, y = Card.rect.yMax - top.y, width = top.x - bottom.x, height = top.y - bottom.y };
            var image = rect.GetComponent<Image>();
            if (image) { row.sprite = image.sprite ? image.sprite.name : "none"; row.shader = image.material.shader.name; row.imageAlpha = image.color.a; }
            var text = rect.GetComponent<Text>();
            if (text) { row.text = text.text; row.font = text.font ? text.font.name : "missing"; row.fontSize = text.fontSize; row.textAlignment = (int)text.alignment; }
            return row;
        }).ToArray();
    }
    static string Hash(string path)
    {
        using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant();
    }
    static void Fail(Exception error)
    {
        EditorApplication.update -= Tick;
        Debug.LogException(error);
        // Guard failure must not write into any other open project.
        if (String.Equals(Application.dataPath.Replace('\\', '/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase))
        {
            if (OutputDirectory == null) OutputDirectory = Project + "/Artifacts/failed_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
            Directory.CreateDirectory(OutputDirectory);
            File.WriteAllText(OutputDirectory + "/render_failure.txt", error.ToString());
        }
        EditorApplication.Exit(1);
    }
}
