using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.UI;

// Isolated source-derived UGUI fixture. It does not execute GameApp, HUDManager,
// client MonoBehaviours, account state, building commands, or popup animation.
[InitializeOnLoad]
public static class RenderRokPlacementMenuNative
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityPlacementMenuProbeV3";
    const string SessionKey = "RokPlacementMenuNativeV3.Output";
    const int Width = 1440, Height = 810;
    [Serializable] public class SourceFile { public string path, sha256, probeAsset; }
    [Serializable] public class Input { public string schema, scope, runnerSha256, rendererSha256, spriteAtlasSha256, spriteSheetSha256, grayShaderSha256, grayMaterialSha256; public int width, height; public SourceFile[] sourceFiles; }
    [Serializable] public class Node { public string name, sprite, material; public bool active, interactable; public Rect screenRect; public Vector2 anchoredPosition, sizeDelta; public Color graphicColor, rendererColor, buttonDisabledColor; }
    [Serializable] public class Sample { public string key, png, sha256; public float dxf, uiScale; public bool confirmEnabled; public Vector2 targetScreen, popupAnchoredPosition; public int unityFrame; public Node popup, featureBtns, confirmSlot, cancelSlot, confirmImage, cancelImage; }
    [Serializable] public class Report { public string schema, scope, unityVersion, rendererSha256, inputSha256, failure, graphicsDevice, colorSpace, canvasRenderMode, canvasScaleMode, screenMatchMode, cameraClearFlags, targetFormat, targetReadWrite, sourceHudPositionRule, sourceHudScaleRule, sourceDynamicButtonIds, gameViewSizeMethod; public bool complete, sourceDerivedFixture, originalPrefabInstantiated, fullGameAppExecuted, sourceGrayMaterialUsed, noAccountOrServerBehavior, fixedProjectedTargetNotWorldCamera; public int width, height, unityScreenWidth, unityScreenHeight, cameraPixelWidth, cameraPixelHeight; public float canvasScaleFactor, canvasReferencePixelsPerUnit, canvasMatchWidthOrHeight, cameraOrthographicSize, buttonFadeSeconds; public Rect cameraPixelRect, canvasPixelRect; public Color cameraBackground, buttonNormalColor, buttonHighlightedColor, buttonPressedColor, buttonDisabledColor; public Sample[] samples; }

    static Input Config;
    static Report Evidence;
    static string Output;
    static Camera Cam;
    static Canvas UiCanvas;
    static CanvasScaler Scaler;
    static RectTransform Popup, Feature, ConfirmSlot, CancelSlot, ConfirmImage, CancelImage;
    static Image ConfirmGraphic, CancelGraphic;
    static Button ConfirmButton, CancelButton;
    static Material Gray;
    static RenderTexture Target;
    static readonly List<Sample> Samples = new List<Sample>();
    static readonly float[] Dxf = { 255f, 255f, 240.600006f, 240.600006f };
    static readonly bool[] Enabled = { true, false, true, false };
    static readonly string[] Keys = { "dxf255_valid", "dxf255_disabled", "dxf240600006_valid", "dxf240600006_disabled" };
    static int NextSample;
    static double StateChangedAt;
    const string GameViewSizeMethod = "UnityEditor.GameView fixed 1440x810 custom size + SetMainPlayModeViewSize before PlayMode";
    static void ConfigureFixedGameView()
    {
        // Batch mode defaults the editor GameView to 1440x480. A camera RT
        // alone does not change Screen.height, so MatchHeight gave 480/810.
        // Request an actual fixed-resolution Editor GameView before PlayMode.
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
        Assembly editor = typeof(Editor).Assembly;
        Type viewType = editor.GetType("UnityEditor.GameView", true);
        Type sizesType = editor.GetType("UnityEditor.GameViewSizes", true);
        Type groupType = editor.GetType("UnityEditor.GameViewSizeGroupType", true);
        Type sizeClass = editor.GetType("UnityEditor.GameViewSize", true);
        Type sizeType = editor.GetType("UnityEditor.GameViewSizeType", true);
        Type singleton = typeof(ScriptableSingleton<>).MakeGenericType(sizesType);
        object sizes = singleton.GetProperty("instance", flags).GetValue(null, null);
        object currentGroup = viewType.GetProperty("currentSizeGroupType", flags).GetValue(null, null);
        Require(currentGroup != null && currentGroup.GetType() == groupType,
            "Editor GameView size group unavailable");
        object group = sizesType.GetMethod("GetGroup", flags).Invoke(sizes, new[] { currentGroup });
        int index = (int)group.GetType().GetMethod("GetTotalCount", flags).Invoke(group, null);
        object fixedSize = Activator.CreateInstance(sizeClass, flags, null,
            new object[] { Enum.Parse(sizeType, "FixedResolution"), Width, Height, "Rok placement 1440x810" }, null);
        group.GetType().GetMethod("AddCustomSize", flags).Invoke(group, new[] { fixedSize });
        EditorWindow view = EditorWindow.GetWindow(viewType);
        Require(view != null, "Editor GameView unavailable in batch mode");
        viewType.GetProperty("selectedSizeIndex", flags).SetValue(view, index, null);
        viewType.GetMethod("SetMainPlayModeViewSize", flags).Invoke(view, new object[] { new Vector2(Width, Height) });
        view.Repaint();
    }
    static bool StatePrepared;

    static RenderRokPlacementMenuNative() { EditorApplication.update -= Tick; EditorApplication.update += Tick; }
    static string Hash(string path) { using (var h = SHA256.Create()) return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static void Require(bool pass, string reason) { if (!pass) throw new Exception(reason); }
    static void Guard()
    {
        Require(String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase), "Wrong isolated Unity project");
        Require(Application.unityVersion == "2019.4.28f1c1", "Wrong Unity version");
        Require(SystemInfo.graphicsDeviceType != GraphicsDeviceType.Null, "Graphics device required; do not use -nographics");
        Require(QualitySettings.activeColorSpace == ColorSpace.Gamma, "Source fixture requires Gamma color space");
    }
    static RectTransform NodeRect(string name, RectTransform parent, Vector2 min, Vector2 max, Vector2 pivot, Vector2 position, Vector2 size)
    {
        var obj = new GameObject(name, typeof(RectTransform)); obj.layer = 5;
        var rect = obj.GetComponent<RectTransform>(); rect.SetParent(parent, false);
        rect.anchorMin = min; rect.anchorMax = max; rect.pivot = pivot;
        rect.anchoredPosition = position; rect.sizeDelta = size; rect.localScale = Vector3.one;
        return rect;
    }
    static Sprite SourceSprite(Texture2D texture, string name, Rect rect)
    {
        // Source ui_hud.tpsheet rectangles are bottom-left pixel coordinates.
        var sprite = Sprite.Create(texture, rect, new Vector2(.5f, .5f), 100f, 0, SpriteMeshType.FullRect);
        sprite.name = name;
        return sprite;
    }
    static ColorBlock SourceButtonColors()
    {
        var c = ColorBlock.defaultColorBlock;
        c.normalColor = Color.white;
        c.highlightedColor = new Color(.9607843f, .9607843f, .9607843f, 1f);
        c.pressedColor = new Color(.78431374f, .78431374f, .78431374f, 1f);
        c.selectedColor = c.highlightedColor;
        c.disabledColor = new Color(.78431374f, .78431374f, .78431374f, .5019608f);
        c.colorMultiplier = 1f; c.fadeDuration = .1f;
        return c;
    }
    static void MakeCommand(RectTransform slot, string name, Sprite sprite, out RectTransform rect, out Image image, out Button button)
    {
        rect = NodeRect(name, slot, new Vector2(.5f,.5f), new Vector2(.5f,.5f), new Vector2(.5f,.5f), Vector2.zero, new Vector2(87,87));
        image = rect.gameObject.AddComponent<Image>(); image.sprite = sprite; image.type = Image.Type.Simple;
        image.preserveAspect = false; image.color = Color.white; image.raycastTarget = true;
        button = rect.gameObject.AddComponent<Button>(); button.targetGraphic = image;
        button.transition = Selectable.Transition.ColorTint; button.colors = SourceButtonColors();
    }
    public static void Run()
    {
        Guard(); Require(!EditorApplication.isPlaying, "Run from Edit mode");
        ConfigureFixedGameView();
        Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
        Directory.CreateDirectory(Output);
        string inputPath = Project + "/source_contract.json";
        Config = JsonUtility.FromJson<Input>(File.ReadAllText(inputPath));
        Require(Config != null && Config.schema == "rok.placement_menu_native_input.v1" && Config.width == Width && Config.height == Height, "Input schema/size mismatch");
        foreach (var f in Config.sourceFiles)
            Require(File.Exists(Project + "/" + f.probeAsset) && Hash(Project + "/" + f.probeAsset) == f.sha256, "Source asset hash differs: " + f.path);
        File.Copy(inputPath, Output + "/source_contract.json");
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var camObj = new GameObject("PlacementMenuCaptureCamera", typeof(Camera));
        Cam = camObj.GetComponent<Camera>(); Cam.orthographic = true; Cam.orthographicSize = Height / 2f;
        Cam.transform.position = new Vector3(0,0,-100); Cam.clearFlags = CameraClearFlags.SolidColor;
        Cam.backgroundColor = Color.clear; Cam.cullingMask = 1 << 5; Cam.allowHDR = false; Cam.allowMSAA = false;
        var root = new GameObject("SourceDerivedUIRoot", typeof(RectTransform), typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
        root.layer = 5; UiCanvas = root.GetComponent<Canvas>(); UiCanvas.renderMode = RenderMode.ScreenSpaceCamera;
        UiCanvas.worldCamera = Cam; UiCanvas.planeDistance = 10;
        Scaler = root.GetComponent<CanvasScaler>(); Scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        Scaler.referenceResolution = new Vector2(Width, Height); Scaler.screenMatchMode = CanvasScaler.ScreenMatchMode.MatchWidthOrHeight;
        Scaler.matchWidthOrHeight = 1f; Scaler.referencePixelsPerUnit = 100f;
        Popup = NodeRect("UI_Pop_BuildingMenu", root.GetComponent<RectTransform>(), new Vector2(.5f,.5f), new Vector2(.5f,.5f), new Vector2(.5f,.5f), Vector2.zero, new Vector2(472.7f,220));
        Feature = NodeRect("UI_Item_CMDBtns", Popup, new Vector2(.5f,0), new Vector2(.5f,0), new Vector2(.5f,1), new Vector2(0,59), new Vector2(472.7f,225.04f));
        ConfirmSlot = NodeRect("pl_tp21", Feature, Vector2.one, Vector2.one, new Vector2(.5f,.5f), new Vector2(-150,-50), new Vector2(100,100));
        CancelSlot = NodeRect("pl_tp22", Feature, Vector2.one, Vector2.one, new Vector2(.5f,.5f), new Vector2(-250,-50), new Vector2(100,100));
        var atlas = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Source/ui_hud.png");
        Gray = AssetDatabase.LoadAssetAtPath<Material>("Assets/Source/UI_GrayWithMask.mat");
        Require(atlas && atlas.width == 1024 && atlas.height == 1024, "Source ui_hud atlas unavailable");
        Require(Gray && Gray.shader && Gray.shader.name == "UI/Default Gray with Mask", "Source gray material/shader unavailable");
        MakeCommand(ConfirmSlot, "confirm_btn_hud_1008", SourceSprite(atlas,"btn_hud_1008",new Rect(721,575,88,88)), out ConfirmImage, out ConfirmGraphic, out ConfirmButton);
        MakeCommand(CancelSlot, "cancel_btn_hud_1004", SourceSprite(atlas,"btn_hud_1004",new Rect(1,435,88,88)), out CancelImage, out CancelGraphic, out CancelButton);
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene(), Project + "/Assets/PlacementMenuProbe.unity");
        EditorPrefs.SetString(SessionKey, Output); EditorApplication.isPlaying = true;
    }
    static void Establish()
    {
        Output = EditorPrefs.GetString(SessionKey);
        Config = JsonUtility.FromJson<Input>(File.ReadAllText(Project + "/source_contract.json"));
        Cam = GameObject.Find("PlacementMenuCaptureCamera").GetComponent<Camera>();
        UiCanvas = GameObject.Find("SourceDerivedUIRoot").GetComponent<Canvas>(); Scaler = UiCanvas.GetComponent<CanvasScaler>();
        Popup = GameObject.Find("UI_Pop_BuildingMenu").GetComponent<RectTransform>();
        Feature = GameObject.Find("UI_Item_CMDBtns").GetComponent<RectTransform>();
        ConfirmSlot = GameObject.Find("pl_tp21").GetComponent<RectTransform>(); CancelSlot = GameObject.Find("pl_tp22").GetComponent<RectTransform>();
        ConfirmImage = GameObject.Find("confirm_btn_hud_1008").GetComponent<RectTransform>(); CancelImage = GameObject.Find("cancel_btn_hud_1004").GetComponent<RectTransform>();
        ConfirmGraphic = ConfirmImage.GetComponent<Image>(); CancelGraphic = CancelImage.GetComponent<Image>();
        ConfirmButton = ConfirmImage.GetComponent<Button>(); CancelButton = CancelImage.GetComponent<Button>();
        // Sprite.Create instances are transient across the EditMode -> PlayMode
        // scene reload. Recreate them from the exact original atlas in PlayMode.
        var atlas = AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Source/ui_hud.png");
        Require(atlas && atlas.width == 1024 && atlas.height == 1024, "PlayMode source atlas missing");
        ConfirmGraphic.sprite = SourceSprite(atlas,"btn_hud_1008",new Rect(721,575,88,88));
        CancelGraphic.sprite = SourceSprite(atlas,"btn_hud_1004",new Rect(1,435,88,88));
        Gray = AssetDatabase.LoadAssetAtPath<Material>("Assets/Source/UI_GrayWithMask.mat");
        Target = new RenderTexture(Width,Height,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
        Target.Create(); Cam.targetTexture = Target;
        Evidence = new Report { schema="rok.placement_menu.native.v1", scope=Config.scope, unityVersion=Application.unityVersion,
            inputSha256=Hash(Project+"/source_contract.json"), rendererSha256=Config.rendererSha256,
            sourceDerivedFixture=true, originalPrefabInstantiated=false, fullGameAppExecuted=false, sourceGrayMaterialUsed=true,
            noAccountOrServerBehavior=true, fixedProjectedTargetNotWorldCamera=true, width=Width, height=Height,
            sourceHudPositionRule="HUDManager AutoAnchorUI overwrites root anchoredPosition with projected target; fixture target=(720,405)",
            sourceHudScaleRule="172/Dxf", sourceDynamicButtonIds="[2,1] => pl_tp21/pl_tp22",
            gameViewSizeMethod=GameViewSizeMethod,
            buttonFadeSeconds=ConfirmButton.colors.fadeDuration,
            buttonNormalColor=ConfirmButton.colors.normalColor,
            buttonHighlightedColor=ConfirmButton.colors.highlightedColor,
            buttonPressedColor=ConfirmButton.colors.pressedColor,
            buttonDisabledColor=ConfirmButton.colors.disabledColor,
            graphicsDevice=SystemInfo.graphicsDeviceType.ToString(), colorSpace=QualitySettings.activeColorSpace.ToString(),
            canvasRenderMode=UiCanvas.renderMode.ToString(), canvasScaleMode=Scaler.uiScaleMode.ToString(), screenMatchMode=Scaler.screenMatchMode.ToString(),
            canvasScaleFactor=UiCanvas.scaleFactor, canvasReferencePixelsPerUnit=Scaler.referencePixelsPerUnit,
            unityScreenWidth=Screen.width, unityScreenHeight=Screen.height,
            cameraPixelWidth=Cam.pixelWidth, cameraPixelHeight=Cam.pixelHeight,
            cameraPixelRect=Cam.pixelRect, canvasPixelRect=UiCanvas.pixelRect,
            canvasMatchWidthOrHeight=Scaler.matchWidthOrHeight, cameraOrthographicSize=Cam.orthographicSize,
            cameraClearFlags=Cam.clearFlags.ToString(), cameraBackground=Cam.backgroundColor,
            targetFormat=Target.format.ToString(), targetReadWrite="Default" };
    }
    static Rect ScreenRect(RectTransform rect)
    {
        var c = new Vector3[4]; rect.GetWorldCorners(c);
        float left=float.PositiveInfinity, right=float.NegativeInfinity;
        float bottomY=float.PositiveInfinity, topY=float.NegativeInfinity;
        foreach (var corner in c)
        {
            Vector2 point = RectTransformUtility.WorldToScreenPoint(Cam,corner);
            left=Mathf.Min(left,point.x); right=Mathf.Max(right,point.x);
            bottomY=Mathf.Min(bottomY,point.y); topY=Mathf.Max(topY,point.y);
        }
        float top=Height-topY, bottom=Height-bottomY;
        return new Rect(left,top,right-left,bottom-top);
    }
    static Node Record(RectTransform rect, string sprite="", Button button=null, Image image=null)
    {
        return new Node { name=rect.name, sprite=sprite, active=rect.gameObject.activeInHierarchy,
            interactable=button && button.interactable, screenRect=ScreenRect(rect),
            anchoredPosition=rect.anchoredPosition, sizeDelta=rect.sizeDelta,
            material=image && image.material && image.material != Graphic.defaultGraphicMaterial ? image.material.name : "UI/Default",
            graphicColor=image ? image.color : Color.clear,
            rendererColor=image ? image.canvasRenderer.GetColor() : Color.clear,
            buttonDisabledColor=button ? button.colors.disabledColor : Color.clear };
    }
    static void SetState(int index)
    {
        float dxf=Dxf[index]; bool enabled=Enabled[index];
        Popup.anchoredPosition=Vector2.zero; // HUDManager AutoAnchorUI projected target (720,405).
        Popup.localScale=Vector3.one*(172f/dxf); // HUDManager scale, not the serialized prefab (0,+35).
        ConfirmGraphic.material=enabled ? null : Gray;
        ConfirmButton.interactable=enabled; CancelButton.interactable=true;
        StateChangedAt=EditorApplication.timeSinceStartup;
        StatePrepared=true;
    }
    static void Capture(int index)
    {
        Canvas.ForceUpdateCanvases();
        var before=RenderTexture.active; RenderTexture.active=Target; GL.Clear(true,true,Color.clear); RenderTexture.active=before;
        Cam.Render();
        Require(Cam.pixelWidth == Width && Cam.pixelHeight == Height, "Capture camera resolution changed");
        var image=new Texture2D(Width,Height,TextureFormat.RGBA32,false,false);
        RenderTexture.active=Target; image.ReadPixels(new Rect(0,0,Width,Height),0,0); image.Apply();
        string png=Keys[index]+".png"; File.WriteAllBytes(Output+"/"+png,image.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(image); RenderTexture.active=null;
        Samples.Add(new Sample { key=Keys[index], png=png, sha256=Hash(Output+"/"+png), dxf=Dxf[index], uiScale=172f/Dxf[index],
            confirmEnabled=Enabled[index], targetScreen=new Vector2(720,405), popupAnchoredPosition=Popup.anchoredPosition, unityFrame=Time.frameCount,
            popup=Record(Popup), featureBtns=Record(Feature), confirmSlot=Record(ConfirmSlot), cancelSlot=Record(CancelSlot),
            confirmImage=Record(ConfirmImage,"ui_hud[btn_hud_1008]",ConfirmButton,ConfirmGraphic),
            cancelImage=Record(CancelImage,"ui_hud[btn_hud_1004]",CancelButton,CancelGraphic) });
    }
    static void Tick()
    {
        if (!EditorApplication.isPlaying || !EditorPrefs.HasKey(SessionKey)) return;
        try
        {
            Guard(); if (Evidence==null) Establish();
            Evidence.unityScreenWidth=Screen.width; Evidence.unityScreenHeight=Screen.height;
            Evidence.cameraPixelWidth=Cam.pixelWidth; Evidence.cameraPixelHeight=Cam.pixelHeight;
            Evidence.cameraPixelRect=Cam.pixelRect; Evidence.canvasPixelRect=UiCanvas.pixelRect;
            Evidence.canvasScaleFactor=UiCanvas.scaleFactor;
            // The 1440x810 RT must not mask a 480-high Editor GameView. The
            // real CanvasScaler must itself report one at the source height.
            if (Time.frameCount < 3) return;
            Require(Screen.width == Width && Screen.height == Height,
                "Editor GameView resolution is " + Screen.width + "x" + Screen.height + ", expected 1440x810");
            Require(Mathf.Abs(UiCanvas.scaleFactor - 1f) < .0001f,
                "Source MatchHeight CanvasScaler is " + UiCanvas.scaleFactor + ", expected 1");
            if (NextSample == Keys.Length)
            {
                Evidence.samples=Samples.ToArray(); Evidence.complete=true;
                File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));
                EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(0); return;
            }
            if (!StatePrepared) { SetState(NextSample); return; }
            // Let the original Button ColorTint 0.1 s fade settle naturally.
            if (EditorApplication.timeSinceStartup-StateChangedAt < .25) return;
            Capture(NextSample); NextSample++; StatePrepared=false;
        }
        catch (Exception error)
        {
            if (Evidence==null) Evidence=new Report { schema="rok.placement_menu.native.v1", scope="isolated_source_derived_fixture" };
            Evidence.failure=error.ToString(); Evidence.samples=Samples.ToArray(); Evidence.complete=false;
            if (!String.IsNullOrEmpty(Output)) File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));
            Debug.LogException(error); EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(1);
        }
    }
}
