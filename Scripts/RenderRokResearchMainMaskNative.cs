using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.Rendering;
using UnityEngine.UI;

// The source UIRoot/ResearchMain prefab and root Animator run in PlayMode.
// UIManager.AddMask is reconstructed in isolation; no GameApp is invoked.
[InitializeOnLoad]
public static class RenderRokResearchMainMaskNative
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityResearchMainMaskProbe";
    const string SessionKey = "RokResearchMainMaskNative.Session";
    [Serializable] public class Inputs
    {
        public string schema, scope, uiRootPrefab, mainPrefab, sourceResearchInputsSha256;
        public string animationDataSha256, rendererSha256, prepareSha256, viewBinderAdapterSha256;
        public int width, height;
    }
    [Serializable] public class SourceInputs { public string uiRootPrefab, mainPrefab; }
    [Serializable] public class Clip { public string phase, stateName; public float durationSeconds; }
    [Serializable] public class AnimationData { public string schema; public Clip[] clips; }
    [Serializable] public class Node
    {
        public string path;
        public bool activeSelf, activeInHierarchy;
        public Vector2 anchoredPosition;
        public Vector3 localScale;
        public Rect screenRect;
        public float canvasGroupAlpha;
    }
    [Serializable] public class Sample
    {
        public string key, phase, png, sha256, animatorState;
        public float elapsedSeconds, normalizedTime, outgoingOpenScale;
        public int unityFrame, maskPointerClicks, closeStartCount, duplicateCloseIgnoredCount;
        public int closeAnimatorEventCount, managerCallbackCount;
        public bool mainExists, maskExists, isAniClosing, managerRemovalRequested;
        public bool simulatedRemoval, sourceButtonPointerClick, actualUiManagerInvoked;
        public bool maskHasButton, maskNullSprite, maskBelowMain, maskRaycastTarget;
        public Color maskImageColor;
        public Node mainWindow, mask;
    }
    [Serializable] public class Report
    {
        public string schema, scope, unityVersion, inputSha256, sourceResearchInputsSha256;
        public string animationDataSha256, rendererSha256, viewBinderAdapterSha256, failure;
        public bool complete, playMode, originalUiRoot, originalMainPrefab, originalAnimatorController;
        public bool recreatedSourceMask, actualUiManagerInvoked, sourceCloseEventMethodAdapted;
        public bool managerRemovalSimulated, forceCloseOnlySourceContract, noAccountOrNetworkAction;
        public int width, height, cameraPixelWidth, cameraPixelHeight;
        public Sample[] samples;
    }

    static readonly string[] Phases = { "mask_close_normal", "mask_close_interrupt_open" };
    static readonly float[] Times = { 0f, .03f, .075f, .12f, .15f };
    static string Output;
    static Inputs Input;
    static AnimationData Animation;
    static Report Evidence;
    static GameObject Root, Main, Mask;
    static Camera Cam;
    static RenderTexture Target;
    static Animator MainAnimator;
    static ViewBinder Binder;
    static Button MaskButton;
    static RectTransform MaskRect;
    static readonly List<Sample> Samples = new List<Sample>();
    static int PhaseIndex, TimeIndex, MaskClicks, CloseStarts, DuplicateIgnored, ManagerCallbackCount;
    static bool ManagerRemovalRequested;

    static RenderRokResearchMainMaskNative()
    {
        EditorApplication.update -= Tick;
        EditorApplication.update += Tick;
    }
    static string Hash(string path)
    {
        using (var h = SHA256.Create())
            return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant();
    }
    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase) ||
            Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong isolated Unity project/version");
        if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new Exception("D3D11 required; do not use -nographics");
        if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Expected Gamma color space");
    }
    public static void Run()
    {
        Guard();
        if (EditorApplication.isPlaying) throw new Exception("Start this sampler in Edit mode");
        Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
        Directory.CreateDirectory(Output);
        foreach (var name in new[] { "ReferenceInputs.json", "SourceResearchInputs.json", "AnimationData.json" })
            File.Copy(Project + "/" + name, Output + "/" + name);
        ReadInput();
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var uiRoot = AssetDatabase.LoadAssetAtPath<GameObject>(Input.uiRootPrefab);
        var researchMain = AssetDatabase.LoadAssetAtPath<GameObject>(Input.mainPrefab);
        if (!uiRoot || !researchMain) throw new Exception("Original UIRoot/ResearchMain prefab missing");
        Root = (GameObject)PrefabUtility.InstantiatePrefab(uiRoot);
        Main = (GameObject)PrefabUtility.InstantiatePrefab(researchMain);
        var layer = Root.transform.Find("Container/WindowLayer");
        if (!layer) throw new Exception("Original UIRoot WindowLayer missing");
        var mainRect = Main.GetComponent<RectTransform>();
        var p = mainRect.anchoredPosition; var size = mainRect.sizeDelta; var scale = mainRect.localScale;
        mainRect.SetParent(layer); mainRect.anchoredPosition = p; mainRect.sizeDelta = size; mainRect.localScale = scale;
        CreateSourceRuleMask(layer);
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene(), Project + "/Assets/ResearchMainMaskProbe.unity");
        EditorPrefs.SetString(SessionKey, Output);
        EditorApplication.isPlaying = true;
    }
    static void ReadInput()
    {
        Input = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
        var source = JsonUtility.FromJson<SourceInputs>(File.ReadAllText(Output + "/SourceResearchInputs.json"));
        Animation = JsonUtility.FromJson<AnimationData>(File.ReadAllText(Output + "/AnimationData.json"));
        if (Input.schema != "rok.research_main_mask.inputs.v1" || Input.width != 1440 || Input.height != 810 ||
            Animation.schema != "rok.research.animation_data.v2" || Animation.clips.Length != 4 ||
            Animation.clips[0].stateName != "Open" || Animation.clips[0].durationSeconds != .25f ||
            Animation.clips[3].stateName != "Close" || Animation.clips[3].durationSeconds != .15f ||
            source.uiRootPrefab != Input.uiRootPrefab || source.mainPrefab != Input.mainPrefab ||
            Hash(Output + "/SourceResearchInputs.json") != Input.sourceResearchInputsSha256 ||
            Hash(Output + "/AnimationData.json") != Input.animationDataSha256)
            throw new Exception("ResearchMain mask input source/hash mismatch");
    }
    static void CreateSourceRuleMask(Transform layer)
    {
        // UIManager.AddMask: sibling immediately before the window, stretched
        // RectTransform, null-sprite Image(0,0,0,.8), Button.onClick.
        Mask = new GameObject("ResearchMainMask", typeof(RectTransform));
        Mask.layer = LayerMask.NameToLayer("UI");
        Mask.transform.SetParent(layer, false);
        Mask.transform.SetSiblingIndex(Main.transform.GetSiblingIndex());
        Mask.transform.localScale = Vector3.one;
        MaskRect = Mask.GetComponent<RectTransform>();
        MaskRect.anchorMin = Vector2.zero; MaskRect.anchorMax = Vector2.one;
        MaskRect.sizeDelta = Vector2.zero; MaskRect.anchoredPosition = Vector2.zero;
        var image = Mask.AddComponent<Image>(); image.sprite = null;
        image.color = new Color(0, 0, 0, .8f);
        MaskButton = Mask.AddComponent<Button>();
        MaskButton.onClick.AddListener(() => { MaskClicks++; BeginClose(); });
    }
    static void Establish()
    {
        Output = EditorPrefs.GetString(SessionKey); ReadInput();
        Root = GameObject.Find("UIRoot"); Main = GameObject.Find("UI_Win_ResearchMain");
        Mask = GameObject.Find("ResearchMainMask");
        if (!Root || !Main || !Mask) throw new Exception("PlayMode lost source ResearchMain or recreated mask");
        Cam = Root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        if (!Cam || !Root.GetComponent<Canvas>()) throw new Exception("Original UIRoot camera/canvas missing");
        Target = new RenderTexture(Input.width, Input.height, 24, RenderTextureFormat.ARGB32);
        Target.Create(); Cam.targetTexture = Target;
        MainAnimator = Main.GetComponent<Animator>(); Binder = Main.GetComponent<ViewBinder>();
        MaskRect = Mask.GetComponent<RectTransform>(); MaskButton = Mask.GetComponent<Button>();
        if (!MainAnimator || !MainAnimator.runtimeAnimatorController || !Binder || !MaskRect || !MaskButton ||
            !Main.GetComponent<CanvasGroup>()) throw new Exception("Original Main Animator/CanvasGroup or adapter missing");
        MaskButton.onClick.AddListener(() => { MaskClicks++; BeginClose(); });
        Evidence = new Report {
            schema = "rok.research_main_mask.native.v1", scope = Input.scope,
            unityVersion = Application.unityVersion, inputSha256 = Hash(Output + "/ReferenceInputs.json"),
            sourceResearchInputsSha256 = Input.sourceResearchInputsSha256,
            animationDataSha256 = Input.animationDataSha256,
            rendererSha256 = Input.rendererSha256,
            viewBinderAdapterSha256 = Input.viewBinderAdapterSha256,
            playMode = true, originalUiRoot = true, originalMainPrefab = true,
            originalAnimatorController = true, recreatedSourceMask = true,
            actualUiManagerInvoked = false, sourceCloseEventMethodAdapted = true,
            managerRemovalSimulated = true, forceCloseOnlySourceContract = true,
            noAccountOrNetworkAction = true, width = Input.width, height = Input.height,
        };
    }
    static void BeginClose()
    {
        // IsAniCloseing is the source UIManager guard; repeated mask clicks
        // must not reset Animator.Play or the Close clock.
        if (Binder.IsAniCloseing) { DuplicateIgnored++; return; }
        Binder.IsAniCloseing = true;
        Binder.onCloseAnimatorCallback = () => { ManagerCallbackCount++; ManagerRemovalRequested = true; };
        CloseStarts++;
        // This source controller has one layer. Select layer 0 explicitly so
        // an interrupted Open is replaced by the Close clip's first key.
        MainAnimator.Play("Close", 0, 0);
        MainAnimator.speed = 1f;
        MainAnimator.Update(0f); // Commit the state switch before elapsed time.
    }
    static void PointerClickMask()
    {
        Canvas.ForceUpdateCanvases();
        var system = EventSystem.current;
        if (!system) system = new GameObject("IsolatedEventSystem", typeof(EventSystem)).GetComponent<EventSystem>();
        var pointer = new PointerEventData(system) { button = PointerEventData.InputButton.Left,
            position = new Vector2(80, 405) };
        if (!RectTransformUtility.RectangleContainsScreenPoint(MaskRect, pointer.position, Cam))
        {
            var corners = new Vector3[4]; MaskRect.GetWorldCorners(corners);
            var projected = corners.Select(p => RectTransformUtility.WorldToScreenPoint(Cam, p)).ToArray();
            throw new Exception("Pointer not inside source-rule mask: rect=" + MaskRect.rect +
                " projected=" + String.Join(",", projected.Select(x => x.ToString()).ToArray()) +
                " camera=" + Cam.name + " pointer=" + pointer.position);
        }
        MaskButton.OnPointerClick(pointer); // Original Unity UI Button path, not UnityEvent.Invoke.
    }
    static float SetSample(string phase, float t)
    {
        Binder.CloseAnimatorEventCount = 0; Binder.IsAniCloseing = false;
        Binder.onCloseAnimatorCallback = null;
        MaskClicks = 0; CloseStarts = 0; DuplicateIgnored = 0;
        ManagerCallbackCount = 0; ManagerRemovalRequested = false;
        MainAnimator.Rebind();
        MainAnimator.Play("Open", 0, 0); MainAnimator.Update(0f);
        float openElapsed = phase == "mask_close_interrupt_open" ? .05f : .25f;
        MainAnimator.Update(openElapsed);
        float outgoing = Main.transform.localScale.x;
        PointerClickMask();
        if (t >= .03f && t < .15f) PointerClickMask();
        MainAnimator.Update(Mathf.Max(t, .000001f));
        return outgoing;
    }
    static Node Capture(RectTransform r)
    {
        var corners = new Vector3[4]; r.GetWorldCorners(corners);
        var points = corners.Select(p => { var v = RectTransformUtility.WorldToScreenPoint(Cam, p);
            return new Vector2(v.x, Input.height - v.y); }).ToArray();
        var group = r.GetComponent<CanvasGroup>();
        return new Node { path = PathOf(r), activeSelf = r.gameObject.activeSelf,
            activeInHierarchy = r.gameObject.activeInHierarchy,
            anchoredPosition = r.anchoredPosition, localScale = r.localScale,
            canvasGroupAlpha = group ? group.alpha : -1f,
            screenRect = new Rect(points.Min(x => x.x), points.Min(x => x.y),
                points.Max(x => x.x) - points.Min(x => x.x),
                points.Max(x => x.y) - points.Min(x => x.y)) };
    }
    static string PathOf(Transform t)
    {
        string s = t.name; while (t.parent) { t = t.parent; s = t.name + "/" + s; } return s;
    }
    static void RenderPng(string name)
    {
        Canvas.ForceUpdateCanvases();
        var previous = RenderTexture.active; RenderTexture.active = Target;
        GL.Clear(true, true, Color.clear); RenderTexture.active = previous;
        Cam.Render();
        Evidence.cameraPixelWidth = Cam.pixelWidth; Evidence.cameraPixelHeight = Cam.pixelHeight;
        if (Cam.pixelWidth != Input.width || Cam.pixelHeight != Input.height)
            throw new Exception("Camera resolution changed");
        var image = new Texture2D(Input.width, Input.height, TextureFormat.RGBA32, false);
        RenderTexture.active = Target;
        image.ReadPixels(new Rect(0, 0, Input.width, Input.height), 0, 0); image.Apply();
        File.WriteAllBytes(Output + "/" + name, image.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = null;
    }
    static void Tick()
    {
        if (!EditorApplication.isPlaying || !EditorPrefs.HasKey(SessionKey)) return;
        try
        {
            Guard();
            if (Evidence == null) Establish();
            if (PhaseIndex > Phases.Length) return;
            if (PhaseIndex == Phases.Length)
            {
                if (Main || Mask) throw new Exception("Simulated manager removal did not complete");
                const string removed = "main_removed_after_callback.png";
                RenderPng(removed);
                Samples.Add(new Sample { key = "main_removed_after_callback", phase = "main_removed_after_callback",
                    png = removed, sha256 = Hash(Output + "/" + removed), elapsedSeconds = .15f,
                    mainExists = false, maskExists = false, simulatedRemoval = true,
                    managerRemovalRequested = true, closeAnimatorEventCount = 1,
                    managerCallbackCount = 1, actualUiManagerInvoked = false,
                    unityFrame = Time.frameCount });
                Evidence.samples = Samples.ToArray(); Evidence.complete = true;
                File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
                PhaseIndex++; EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(0); return;
            }
            string phase = Phases[PhaseIndex]; float t = Times[TimeIndex];
            float outgoing = SetSample(phase, t);
            string key = phase + "_t" + Mathf.RoundToInt(t * 10000).ToString("D4");
            string png = key + ".png"; RenderPng(png);
            var state = MainAnimator.GetCurrentAnimatorStateInfo(0);
            var sample = new Sample {
                key = key, phase = phase, png = png, sha256 = Hash(Output + "/" + png),
                elapsedSeconds = t, normalizedTime = state.normalizedTime,
                animatorState = state.IsName("Close") ? "Close" : "other",
                outgoingOpenScale = outgoing, unityFrame = Time.frameCount,
                maskPointerClicks = MaskClicks, closeStartCount = CloseStarts,
                duplicateCloseIgnoredCount = DuplicateIgnored,
                closeAnimatorEventCount = Binder.CloseAnimatorEventCount,
                managerCallbackCount = ManagerCallbackCount,
                mainExists = true, maskExists = true, isAniClosing = Binder.IsAniCloseing,
                managerRemovalRequested = ManagerRemovalRequested,
                simulatedRemoval = false, sourceButtonPointerClick = true,
                actualUiManagerInvoked = false,
                maskHasButton = MaskButton != null,
                maskNullSprite = Mask.GetComponent<Image>().sprite == null,
                maskBelowMain = Mask.transform.GetSiblingIndex() < Main.transform.GetSiblingIndex(),
                maskRaycastTarget = Mask.GetComponent<Image>().raycastTarget,
                maskImageColor = Mask.GetComponent<Image>().color,
                mainWindow = Capture(Main.GetComponent<RectTransform>()), mask = Capture(MaskRect),
            };
            Samples.Add(sample); Evidence.samples = Samples.ToArray();
            File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            TimeIndex++;
            if (TimeIndex >= Times.Length) { TimeIndex = 0; PhaseIndex++; }
            if (PhaseIndex == Phases.Length)
            {
                if (Binder.CloseAnimatorEventCount != 1 || ManagerCallbackCount != 1 || !ManagerRemovalRequested)
                    throw new Exception("Original Close AnimationEvent never reached manager-equivalent callback");
                UnityEngine.Object.Destroy(Main); UnityEngine.Object.Destroy(Mask);
            }
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            if (Output != null) {
                if (Evidence == null) Evidence = new Report();
                Evidence.failure = error.ToString(); Evidence.samples = Samples.ToArray();
                File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            }
            EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(1);
        }
    }
}
