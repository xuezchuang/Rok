using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Rendering;

// Isolated presentation probe. The source controller is evaluated by Unity PlayMode.
[InitializeOnLoad]
public static class RenderRokBarracksParentWindowV3
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityBarracksParentWindowV3Probe";
    const string SessionKey = "RokBarracksParentWindowV3Probe.Session";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class LanguageBinding { public string path, text; public int id; }
    [Serializable] public class Inputs { public string scope, prefab, uiRootPrefab, fixtureTitle; public int width, height, probeVersion; public float[] sampleThresholdsSeconds; public Entry[] texts; }
    [Serializable] public class LayoutFlag { public string path; public int flags; public bool applied, arabicOnly; }
    [Serializable] public class RootCheckpoint { public string label; public Vector3 localScale, lossyScale; public Rect localRect, screenRect, windowLayerScreenRect; public float canvasScaleFactor; public string canvasRenderMode; public int cameraPixelWidth, cameraPixelHeight; }
    [Serializable] public class Setup { public LayoutFlag[] layoutFlagsApplied; public LanguageBinding[] languageBindings; public string fixtureTitleApplied; public RootCheckpoint rootAfterLoad; }
    [Serializable] public class Node { public string path, text; public bool active, visible; public Vector2 anchorMin, anchorMax, anchoredPosition, sizeDelta, pivot; public Vector3 localScale; public Vector2[] screenQuad; public Rect screenRect; }
    [Serializable] public class Sample { public string label, png, sha256, stateName; public float elapsedRealtime, elapsedGameTime, gameDeltaTime, animatorNormalizedTime, animatorStateLength, rootScale; public int unityFrame; public Node[] nodes; public RootCheckpoint afterForceUpdate, beforeCameraRender, afterCameraRender; }
    [Serializable] public class Report { public string scope, unityVersion, inputSha256, failure, fixture, fixtureTitleApplied, nativeCameraClearFlags, clearPolicy, setupSha256; public bool playMode, nativeAnimatorEnabled, projectionInitialized, complete; public int clearCount; public LayoutFlag[] layoutFlagsApplied; public LanguageBinding[] languageBindings; public Sample[] samples; }
    static string Output;
    static Inputs Data;
    static Report Evidence;
    static readonly List<Sample> Samples = new List<Sample>();
    static float Started;
    static float StartedGameTime;
    static int StartedFrame;
    static Camera Cam;
    static GameObject Root;
    static RenderTexture Target;
    static GameObject Window;
    static readonly HashSet<ArabLayoutCompment> Applied = new HashSet<ArabLayoutCompment>();
    static readonly string[] RequiredNames = { "UI_Win_BuildingUpdate", "UI_Model_Window_Type1", "win", "frame1", "frame2", "frame3", "frame4", "title", "lbl_title", "pl_upgrade", "UI_Item_BuildingUpgrade", "img_resBg", "pl_btns" };

    static RenderRokBarracksParentWindowV3()
    {
        EditorApplication.update -= Tick;
        EditorApplication.update += Tick;
    }
    static string Hash(string path) { using (var s = SHA256.Create()) return BitConverter.ToString(s.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase) || Application.unityVersion != "2019.4.28f1c1")
            throw new Exception("Wrong isolated Unity project/version");
    }
    public static void Run()
    {
        Guard();
        if (EditorApplication.isPlaying) throw new Exception("Probe must begin in edit mode");
        if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new Exception("Native Camera.Render requires a graphics device; launch without -nographics");
        if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Expected source Gamma color space");
        Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
        Directory.CreateDirectory(Output);
        File.Copy(Project + "/ReferenceInputs.json", Output + "/ReferenceInputs.json");
        Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
        if (Data.probeVersion != 3 || Data.width != 1440 || Data.height != 810 || Data.sampleThresholdsSeconds.Length != 4 || Data.texts == null) throw new Exception("Unexpected V3 source probe contract");
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var rootPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.uiRootPrefab);
        var windowPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab);
        if (!rootPrefab || !windowPrefab) throw new Exception("Missing original UI prefabs");
        var root = (GameObject)PrefabUtility.InstantiatePrefab(rootPrefab);
        Root = root; // Keep original serialized localScale; V3 A/B control has no root-scale assignment.
        var canvas = root.GetComponent<Canvas>();
        var layer = root.transform.Find("Container/WindowLayer");
        if (!canvas || !layer) throw new Exception("Original Canvas/Container/WindowLayer missing");
        Cam = root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        if (!Cam) throw new Exception("Original UIRoot camera missing");
        if (Cam.clearFlags != CameraClearFlags.Depth) throw new Exception("Original UIRoot camera clearFlags changed");
        canvas.worldCamera = Cam;
        var rootAfterLoad = RootState("after_prefab_load_before_force_update");
        Window = (GameObject)PrefabUtility.InstantiatePrefab(windowPrefab);
        var rect = Window.GetComponent<RectTransform>();
        var position = rect.anchoredPosition; var size = rect.sizeDelta; var scale = rect.localScale;
        rect.SetParent(layer); // UIManager.AddToLayer uses SetParent(layer), then restores these three values.
        rect.anchoredPosition = position; rect.sizeDelta = size; rect.localScale = scale;
        var animator = Window.GetComponent<Animator>();
        if (!animator || !animator.enabled || !animator.runtimeAnimatorController || animator.runtimeAnimatorController.name != "UA_WinAnime_T1")
            throw new Exception("Original T1 Animator/controller not enabled");
        var setup = new Setup();
        setup.languageBindings = ApplyChineseTexts();
        var titleLabels = Window.GetComponentsInChildren<LanguageText>(true).Where(t => t.name == "lbl_title").ToArray();
        if (titleLabels.Length != 1 || String.IsNullOrEmpty(Data.fixtureTitle)) throw new Exception("Source parent-window title fixture missing");
        titleLabels[0].text = Data.fixtureTitle;
        setup.fixtureTitleApplied = titleLabels[0].text;
        setup.layoutFlagsApplied = ApplyChineseLayout();
        setup.rootAfterLoad = rootAfterLoad;
        File.WriteAllText(Output + "/setup_evidence.json", JsonUtility.ToJson(setup, true));
        var scene = EditorSceneManager.GetActiveScene();
        EditorSceneManager.SaveScene(scene, Project + "/Assets/ParentWindowProbe.unity");
        EditorPrefs.SetString(SessionKey, Output);
        EditorApplication.isPlaying = true;
    }
    static void Tick()
    {
        if (!EditorApplication.isPlaying || !EditorPrefs.HasKey(SessionKey)) return;
        try
        {
            Guard();
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new Exception("Graphics device became Null; native PNG/quad invalid");
            if (Output == null)
            {
                Output = EditorPrefs.GetString(SessionKey);
                Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
                var setup = JsonUtility.FromJson<Setup>(File.ReadAllText(Output + "/setup_evidence.json"));
                Evidence = new Report { scope = Data.scope, unityVersion = Application.unityVersion, inputSha256 = Hash(Output + "/ReferenceInputs.json"),
                    fixture = "source_prefab_presentation_only", playMode = true, setupSha256 = Hash(Output + "/setup_evidence.json"),
                    layoutFlagsApplied = setup.layoutFlagsApplied, languageBindings = setup.languageBindings,
                    fixtureTitleApplied = setup.fixtureTitleApplied,
                    clearPolicy = "GL.Clear(true,true,Color.clear)" };
                Window = GameObject.Find("UI_Win_BuildingUpdate");
                var root = GameObject.Find("UIRoot");
                if (!Window || !root) throw new Exception("PlayMode lost original prefab hierarchy");
                Root = root;
                Cam = root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
                if (!Cam) throw new Exception("PlayMode UIRoot camera missing");
                Evidence.nativeCameraClearFlags = Cam.clearFlags.ToString();
                if (Cam.clearFlags != CameraClearFlags.Depth) throw new Exception("Native UIRoot clearFlags changed in PlayMode");
                Target = new RenderTexture(Data.width, Data.height, 24, RenderTextureFormat.ARGB32); Target.Create();
                Cam.targetTexture = Target;
                Started = Time.realtimeSinceStartup;
                StartedGameTime = Time.time;
                StartedFrame = Time.frameCount;
            }
            var elapsed = Time.realtimeSinceStartup - Started;
            if (elapsed > 15.0f) throw new Exception("Native Animator did not reach required frame/game-time phases within 15 seconds");
            var gameElapsed = Time.time - StartedGameTime;
            var index = Samples.Count;
            if (index >= 4 || (index > 0 && (Time.frameCount <= StartedFrame + (index == 1 ? 6 : Samples[index - 1].unityFrame)
                || gameElapsed < Data.sampleThresholdsSeconds[index]))) return;
            Canvas.ForceUpdateCanvases();
            foreach (var g in Window.GetComponentsInChildren<Graphic>(true)) g.SetAllDirty();
            Canvas.ForceUpdateCanvases();
            var afterForceUpdate = RootState("after_force_update");
            var previousTarget = RenderTexture.active;
            RenderTexture.active = Target;
            GL.Clear(true, true, Color.clear); // Source camera clears depth only; reset the isolated capture target each frame.
            RenderTexture.active = previousTarget;
            Evidence.clearCount++;
            var beforeCameraRender = RootState("before_camera_render");
            Cam.Render(); // Initialize projection before any absolute WorldToScreenPoint call.
            var afterCameraRender = RootState("after_camera_render");
            Evidence.projectionInitialized = Cam.pixelWidth > 0 && Cam.pixelHeight > 0;
            if (!Evidence.projectionInitialized) throw new Exception("Camera projection was not initialized");
            var animator = Window.GetComponent<Animator>();
            Evidence.nativeAnimatorEnabled = animator && animator.enabled && animator.runtimeAnimatorController && animator.runtimeAnimatorController.name == "UA_WinAnime_T1";
            if (!Evidence.nativeAnimatorEnabled) throw new Exception("Native Animator disabled or replaced");
            var state = animator.GetCurrentAnimatorStateInfo(0);
            if (index == 0 && !state.IsName("Open")) return; // Wait for the first actual PlayMode animator evaluation.
            if (!state.IsName("Open")) throw new Exception("Original T1 controller did not evaluate its serialized Open state");
            if (index == 1 && state.normalizedTime < 7.0f / 15.0f) return;
            if (index >= 2 && state.normalizedTime < 0.99f) return;
            var nodes = new List<Node>();
            foreach (var name in RequiredNames)
            {
                var matches = Window.GetComponentsInChildren<RectTransform>(true).Where(r => r.name == name).ToArray();
                if (matches.Length == 0) throw new Exception("Missing source control " + name);
                foreach (var r in matches) nodes.Add(Capture(r));
            }
            for (var t = Window.transform.parent; t; t = t.parent)
            {
                var r = t as RectTransform;
                if (r) nodes.Add(Capture(r));
            }
            var filename = "frame_" + index.ToString("00") + ".png";
            RenderTexture.active = Target;
            var image = new Texture2D(Data.width, Data.height, TextureFormat.RGBA32, false);
            image.ReadPixels(new Rect(0, 0, Data.width, Data.height), 0, 0); image.Apply();
            File.WriteAllBytes(Output + "/" + filename, image.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = null;
            Samples.Add(new Sample { label = new[] { "first_rendered_frame", "at_or_after_7f", "at_or_after_0_25s", "at_or_after_0_5s" }[index],
                png = filename, sha256 = Hash(Output + "/" + filename), elapsedRealtime = elapsed, elapsedGameTime = gameElapsed,
                gameDeltaTime = Time.deltaTime, unityFrame = Time.frameCount - StartedFrame,
                stateName = state.IsName("Open") ? "Open" : state.IsName("Close") ? "Close" : "other", animatorNormalizedTime = state.normalizedTime,
                animatorStateLength = state.length,
                rootScale = Window.transform.localScale.x, nodes = nodes.ToArray(),
                afterForceUpdate = afterForceUpdate, beforeCameraRender = beforeCameraRender, afterCameraRender = afterCameraRender });
            Evidence.samples = Samples.ToArray();
            File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            if (Samples.Count == 4)
            {
                Evidence.complete = true;
                File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
                EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(0);
            }
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            if (Output != null)
            {
                if (Evidence == null) Evidence = new Report();
                Evidence.failure = error.ToString(); Evidence.samples = Samples.ToArray();
                File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            }
            EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(1);
        }
    }
    static Node Capture(RectTransform r)
    {
        var corners = new Vector3[4]; r.GetWorldCorners(corners);
        var quad = corners.Select(p => { var s = RectTransformUtility.WorldToScreenPoint(Cam, p); return new Vector2(s.x, Data.height - s.y); }).ToArray();
        var x0 = quad.Min(v => v.x); var y0 = quad.Min(v => v.y);
        var x1 = quad.Max(v => v.x); var y1 = quad.Max(v => v.y);
        var path = r.name;
        for (var p = r.parent; p; p = p.parent) path = p.name + "/" + path;
        var label = r.GetComponent<Text>();
        return new Node { path = path, text = label ? label.text : "", active = r.gameObject.activeSelf, visible = r.gameObject.activeInHierarchy,
            anchorMin = r.anchorMin, anchorMax = r.anchorMax, anchoredPosition = r.anchoredPosition, sizeDelta = r.sizeDelta,
            pivot = r.pivot, localScale = r.localScale, screenQuad = quad, screenRect = new Rect(x0, y0, x1 - x0, y1 - y0) };
    }
    static RootCheckpoint RootState(string label)
    {
        var canvas = Root.GetComponent<Canvas>();
        var rootRect = Root.GetComponent<RectTransform>();
        var layer = Root.transform.Find("Container/WindowLayer") as RectTransform;
        if (!canvas || !rootRect || !layer || !Cam) throw new Exception("V3 root checkpoint hierarchy missing");
        return new RootCheckpoint { label = label, localScale = rootRect.localScale, lossyScale = rootRect.lossyScale,
            localRect = rootRect.rect, screenRect = ScreenRect(rootRect), windowLayerScreenRect = ScreenRect(layer),
            canvasScaleFactor = canvas.scaleFactor, canvasRenderMode = canvas.renderMode.ToString(),
            cameraPixelWidth = Cam.pixelWidth, cameraPixelHeight = Cam.pixelHeight };
    }
    static Rect ScreenRect(RectTransform rect)
    {
        var corners = new Vector3[4]; rect.GetWorldCorners(corners);
        var points = corners.Select(p => { var s = RectTransformUtility.WorldToScreenPoint(Cam, p); return new Vector2(s.x, Data.height - s.y); }).ToArray();
        float x0 = points.Min(p => p.x), y0 = points.Min(p => p.y), x1 = points.Max(p => p.x), y1 = points.Max(p => p.y);
        return new Rect(x0, y0, x1 - x0, y1 - y0);
    }
    static string PathOf(Transform t)
    {
        var path = t.name;
        for (var p = t.parent; p; p = p.parent) path = p.name + "/" + path;
        return path;
    }
    static LanguageBinding[] ApplyChineseTexts()
    {
        var bindings = new List<LanguageBinding>();
        foreach (var label in Window.GetComponentsInChildren<LanguageText>(true))
        {
            var entry = Data.texts.FirstOrDefault(x => x.id == label.SourceLanguageId);
            if (entry == null) continue;
            label.text = entry.text;
            bindings.Add(new LanguageBinding { path = PathOf(label.transform), id = entry.id, text = label.text });
        }
        if (bindings.Count == 0) throw new Exception("No source Chinese LanguageText was bound");
        return bindings.ToArray();
    }
    static LayoutFlag[] ApplyChineseLayout()
    {
        var trace = new List<LayoutFlag>();
        foreach (var arab in Window.GetComponentsInChildren<ArabLayoutCompment>(true))
        {
            int flags = arab.SourceFlags;
            if ((flags & ~635) != 0) throw new Exception("Unsupported source Arab flags " + flags);
            if (!Applied.Add(arab)) throw new Exception("Chinese layout applied twice to " + PathOf(arab.transform));
            var row = new LayoutFlag { path = PathOf(arab.transform), flags = flags, arabicOnly = (flags & 1) != 0, applied = (flags & 1) == 0 };
            trace.Add(row);
            if (row.arabicOnly) continue;
            if ((flags & 2) != 0) { var r = arab.GetComponent<RectTransform>(); var min = r.anchorMin; var max = r.anchorMax; r.anchorMin = new Vector2(1-max.x,min.y); r.anchorMax = new Vector2(1-min.x,max.y); r.pivot = new Vector2(1-r.pivot.x,r.pivot.y); r.anchoredPosition = new Vector2(-r.anchoredPosition.x,r.anchoredPosition.y); }
            if ((flags & 8) != 0) { var t = arab.GetComponent<Text>(); int v = (int)t.alignment; t.alignment = (TextAnchor)(v/3*3+2-v%3); }
            if ((flags & 16) != 0) { var i = arab.GetComponent<Image>(); if (i) { var s = i.transform.localScale; s.x = -s.x; i.transform.localScale = s; } }
            if ((flags & 32) != 0) { var s = arab.GetComponent<Slider>(); if (s && (s.direction == Slider.Direction.LeftToRight || s.direction == Slider.Direction.RightToLeft)) s.direction = s.direction == Slider.Direction.LeftToRight ? Slider.Direction.RightToLeft : Slider.Direction.LeftToRight; }
            if ((flags & 64) != 0) { var r = arab.GetComponent<RectTransform>(); if (r) { var s = r.localScale; s.x = -s.x; r.localScale = s; } }
            if ((flags & 512) != 0) { var g = arab.GetComponent<GridLayoutGroup>(); g.startCorner = (GridLayoutGroup.Corner)((int)g.startCorner ^ 1); int v = (int)g.childAlignment; g.childAlignment = (TextAnchor)(v/3*3+2-v%3); }
        }
        if (trace.Count == 0) throw new Exception("No source ArabLayout flags found");
        return trace.ToArray();
    }
}
