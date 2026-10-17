using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.UI;

// Isolated source-prefab/Animator sampler. The mediator's activation/Play
// sequence is reproduced explicitly; no account, network or GameApp is used.
[InitializeOnLoad]
public static class RenderRokResearchAnimationNative
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityResearchAnimationProbe";
    const string SessionKey = "RokResearchAnimationNative.Session";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Study { public int studyType, studyLv, maxLevel, preconditionStudy1, l_buffDesID, power; public string icon, name, description, desData; }
    [Serializable] public class Fixture { public Study[] nodes, moreLevels; public int moreDetailStudyType; public Entry[] language; }
    [Serializable] public class SourceInputs { public string uiRootPrefab, mainPrefab, detailPrefab, detailAttrItemPrefab, powerItemPrefab; public string[] atlasPaths; public Entry[] prefabTexts; public Fixture fixture; }
    [Serializable] public class Inputs { public string schema, sourceResearchInputsSha256, sourceFixtureSha256, animationDataSha256, rendererSha256, prepareSha256, instrumentedViewBinderSha256, scope; public int width, height; }
    [Serializable] public class Clip { public string phase, controller, clip, animatorTargetPath, stateName; public float durationSeconds; public bool loop; public float[] sampleTimesSeconds; }
    [Serializable] public class AnimationData { public string schema; public Clip[] clips; }
    [Serializable] public class Node { public string path, text; public bool activeSelf, activeInHierarchy; public Vector2 anchoredPosition; public Vector3 localScale; public Rect screenRect; public float canvasGroupAlpha; }
    [Serializable] public class Sample { public string key, phase, png, sha256, rootState, updateState, powerState; public float elapsedSeconds, rootElapsedSeconds, updateElapsedSeconds, powerElapsedSeconds, rootNormalizedTime, updateNormalizedTime, powerNormalizedTime, interruptedOutgoingPowerX, interruptedOutgoingPowerAlpha, interruptedOutgoingRootScale; public int unityFrame, closeAnimatorEventCount, managerCallbackCount; public bool updateActive, powerActive, backActive, originalRootAnimator, originalUpdateAnimator, originalPowerAnimator, isAniClosing, managerDestroyRequested, duplicateCloseIgnored, detailExists, simulatedRemoval, actualUiManagerInvoked; public Node detailWindow, updateRoot, powerRoot, title, backButton, powerViewport; }
    [Serializable] public class Report { public string schema, scope, unityVersion, inputSha256, sourceResearchInputsSha256, sourceFixtureSha256, animationDataSha256, rendererSha256, instrumentedViewBinderSha256, failure; public bool complete, playMode, originalUiRoot, originalMainPrefab, originalDetailPrefab, originalAnimatorControllers, syntheticFixture, noAccountOrNetworkAction, actualUiManagerInvoked, sourceCloseEventMethodAdapted; public int width, height, cameraPixelWidth, cameraPixelHeight; public Sample[] samples; }

    static string Output;
    static Inputs Input;
    static SourceInputs Source;
    static AnimationData Animation;
    static Report Evidence;
    static GameObject Root, Main, Detail;
    static Camera Cam;
    static RenderTexture Target;
    static Animator RootAnimator, UpdateAnimator, PowerAnimator;
    static ViewBinder Binder;
    static RectTransform UpdateRoot, PowerRoot, BackButton;
    static Vector2 UpdateInitialPosition, PowerInitialPosition;
    static float UpdateInitialAlpha, PowerInitialAlpha;
    static readonly HashSet<ArabLayoutCompment> Applied = new HashSet<ArabLayoutCompment>();
    static readonly List<Sample> Samples = new List<Sample>();
    static int PhaseIndex, TimeIndex, ManagerCallbackCount;
    static bool ManagerDestroyRequested;
    static readonly string[] Phases = { "detail_open", "power_show_128", "detail_back_128", "open_then_more_overlap_128", "power_interrupted_to_back_128",
        "detail_close_normal", "open_then_close_128", "power_then_close_128", "close_then_more_128", "close_then_back_128" };
    static readonly float[] Grid = { 0f, .05f, .1f, .15f, .2f, .25f, .3f };
    static readonly float[] CloseGrid = { 0f, .05f, .1f, .15f, .2f };

    static RenderRokResearchAnimationNative() { EditorApplication.update -= Tick; EditorApplication.update += Tick; }
    static string Hash(string path) { using (var h = SHA256.Create()) return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase) ||
            Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong isolated Unity project/version");
        if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new Exception("D3D11 required; no -nographics");
        if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Expected Gamma color space");
    }
    static RectTransform At(Transform parent, string path)
    {
        var value = parent.Find(path) as RectTransform;
        if (!value) throw new Exception("Missing exact source path " + path);
        return value;
    }
    static T AtComponent<T>(Transform parent, string path) where T : Component
    {
        var result = At(parent, path).GetComponent<T>();
        if (!result) throw new Exception("Missing " + typeof(T).Name + " at " + path);
        return result;
    }
    static string PathOf(Transform t) { string s = t.name; while (t.parent) { t=t.parent; s=t.name+"/"+s; } return s; }
    static void ApplyTexts(GameObject parent)
    {
        foreach (var label in parent.GetComponentsInChildren<LanguageText>(true))
        { var row = Source.prefabTexts.FirstOrDefault(x => x.id == label.SourceLanguageId); if (row != null) label.text = row.text; }
    }
    static void ApplyChineseLayout(GameObject parent)
    {
        foreach (var arab in parent.GetComponentsInChildren<ArabLayoutCompment>(true))
        {
            int flags = arab.SourceFlags;
            if ((flags & ~891) != 0) throw new Exception("Unsupported ArabLayout flags " + flags + " at " + PathOf(arab.transform));
            if (!Applied.Add(arab) || (flags & 1) != 0) continue;
            if ((flags & 2) != 0) { var r = arab.GetComponent<RectTransform>(); var min = r.anchorMin; var max = r.anchorMax; r.anchorMin = new Vector2(1-max.x,min.y); r.anchorMax = new Vector2(1-min.x,max.y); r.pivot = new Vector2(1-r.pivot.x,r.pivot.y); r.anchoredPosition = new Vector2(-r.anchoredPosition.x,r.anchoredPosition.y); }
            if ((flags & 8) != 0) { var t = arab.GetComponent<Text>(); if (t) { int v = (int)t.alignment; t.alignment = (TextAnchor)(v/3*3+2-v%3); } }
            if ((flags & 16) != 0) { var i = arab.GetComponent<Image>(); if (i) { var s = i.transform.localScale; s.x = -s.x; i.transform.localScale = s; } }
            if ((flags & 32) != 0) { var s = arab.GetComponent<Slider>(); if (s && (s.direction == Slider.Direction.LeftToRight || s.direction == Slider.Direction.RightToLeft)) s.direction = s.direction == Slider.Direction.LeftToRight ? Slider.Direction.RightToLeft : Slider.Direction.LeftToRight; }
            if ((flags & 64) != 0) { var r = arab.GetComponent<RectTransform>(); if (r) { var s = r.localScale; s.x = -s.x; r.localScale = s; } }
            if ((flags & 256) != 0) { var list = arab.GetComponent<ListView>(); if (!list || list.layoutType != 3) throw new Exception("Expected RTL ListView"); list.layoutType = 2; }
            if ((flags & 512) != 0) { var g = arab.GetComponent<GridLayoutGroup>(); if (g) { g.startCorner = (GridLayoutGroup.Corner)((int)g.startCorner ^ 1); int v = (int)g.childAlignment; g.childAlignment = (TextAnchor)(v/3*3+2-v%3); } }
        }
    }
    static Sprite Sprite(string reference)
    {
        int i = reference.IndexOf('[');
        if (i <= 0 || !reference.EndsWith("]")) throw new Exception("Bad source sprite " + reference);
        string atlas = reference.Substring(0,i), name = reference.Substring(i+1,reference.Length-i-2);
        string path = Source.atlasPaths.Single(p => Path.GetFileNameWithoutExtension(p) == atlas);
        var found = AssetDatabase.LoadAllAssetsAtPath(path).OfType<Sprite>().Where(s => s.name == name).ToArray();
        if (found.Length != 1) throw new Exception("Missing original sprite " + reference);
        return found[0];
    }
    public static void Run()
    {
        Guard();
        if (EditorApplication.isPlaying) throw new Exception("Start in Edit mode");
        Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
        Directory.CreateDirectory(Output);
        foreach (var file in new[] { "ReferenceInputs.json", "SourceResearchInputs.json", "ResearchPowerFixture.json", "AnimationData.json" })
            File.Copy(Project + "/" + file, Output + "/" + file);
        ReadInput();
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var root = AssetDatabase.LoadAssetAtPath<GameObject>(Source.uiRootPrefab);
        var main = AssetDatabase.LoadAssetAtPath<GameObject>(Source.mainPrefab);
        var detail = AssetDatabase.LoadAssetAtPath<GameObject>(Source.detailPrefab);
        if (!root || !main || !detail) throw new Exception("Original source prefabs missing");
        Root = (GameObject)PrefabUtility.InstantiatePrefab(root);
        Cam = Root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        if (!Cam || !Root.GetComponent<Canvas>()) throw new Exception("Original UIRoot camera/canvas missing");
        Main = (GameObject)PrefabUtility.InstantiatePrefab(main);
        ParentPreserving(Main.GetComponent<RectTransform>(), Root.transform.Find("Container/WindowLayer"));
        ApplyTexts(Main); ApplyChineseLayout(Main);
        Detail = (GameObject)PrefabUtility.InstantiatePrefab(detail);
        ParentPreserving(Detail.GetComponent<RectTransform>(), Root.transform.Find("Container/WindowPopLayer"));
        ApplyTexts(Detail); ApplyChineseLayout(Detail);
        PopulateDetail();
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene(), Project + "/Assets/ResearchAnimationProbe.unity");
        EditorPrefs.SetString(SessionKey, Output);
        EditorApplication.isPlaying = true;
    }
    static void ParentPreserving(RectTransform rect, Transform parent)
    {
        if (!parent) throw new Exception("Source UIRoot layer missing");
        var pos=rect.anchoredPosition; var size=rect.sizeDelta; var scale=rect.localScale;
        rect.SetParent(parent); rect.anchoredPosition=pos; rect.sizeDelta=size; rect.localScale=scale;
    }
    static void ReadInput()
    {
        Input = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
        Source = JsonUtility.FromJson<SourceInputs>(File.ReadAllText(Output + "/SourceResearchInputs.json"));
        Animation = JsonUtility.FromJson<AnimationData>(File.ReadAllText(Output + "/AnimationData.json"));
        if (Input.schema != "rok.research.animation_native_inputs.v1" || Animation.schema != "rok.research.animation_data.v2" ||
            Input.width != 1440 || Input.height != 810 || Animation.clips.Length != 4 ||
            Hash(Output + "/SourceResearchInputs.json") != Input.sourceResearchInputsSha256 ||
            Hash(Output + "/ResearchPowerFixture.json") != Input.sourceFixtureSha256 ||
            Hash(Output + "/AnimationData.json") != Input.animationDataSha256 ||
            Source.fixture.moreDetailStudyType != 128 || Source.fixture.moreLevels.Length != 10)
            throw new Exception("Animation input hash/schema/fixture mismatch");
        if (Animation.clips[0].phase != "detail_open" || Animation.clips[1].phase != "power_show_128" ||
            Animation.clips[2].phase != "detail_back_128" || Animation.clips[3].phase != "detail_close")
            throw new Exception("Unexpected source clip phase order");
    }
    static void PopulateDetail()
    {
        var study = Source.fixture.nodes.Single(n => n.studyType == 128);
        if (study.preconditionStudy1 == 0 || study.maxLevel != 10) throw new Exception("Expected locked Study128 fixture");
        const string left = "rect/img_update/pl_left/", right = "rect/img_update/pl_right/";
        AtComponent<Image>(Detail.transform,left+"img_icon").sprite = Sprite(study.icon);
        AtComponent<LanguageText>(Detail.transform,left+"lbl_des").text = study.description;
        AtComponent<Slider>(Detail.transform,left+"pb_bar").value = 0;
        AtComponent<LanguageText>(Detail.transform,left+"pb_bar/lbl_barText").text = "0/10";
        AtComponent<LanguageText>(Detail.transform,right+"lbl_leveLow").text = String.Format(Source.fixture.language.Single(x=>x.id==180306).text,0);
        AtComponent<LanguageText>(Detail.transform,right+"lbl_leveLow/lbl_leveHight").text = "1";
        At(Detail.transform,right+"lbl_leveLow/img_up").gameObject.SetActive(true);
        At(Detail.transform,right+"pl_upPlane").gameObject.SetActive(true);
        AtComponent<LanguageText>(Detail.transform,right+"lbl_unlock").text = Source.fixture.language.Single(x=>x.id==402019).text;
        At(Detail.transform,right+"lbl_unlock").gameObject.SetActive(true);
        At(Detail.transform,right+"pl_condition").gameObject.SetActive(false);
        At(Detail.transform,right+"pl_speedUp").gameObject.SetActive(false);
        AtComponent<LanguageText>(Detail.transform,"UI_Model_Window_Type1/title/lbl_title").text = study.name;
        PopulateAttributes(At(Detail.transform,right+"UI_Model_AttrList"),study);
        PopulatePower();
        At(Detail.transform,"rect/pl_power").gameObject.SetActive(false);
        At(Detail.transform,"UI_Model_Window_Type1/title/btn_back").gameObject.SetActive(false);
    }
    static void PopulateAttributes(RectTransform list, Study study)
    {
        var level = Source.fixture.moreLevels.Single(x=>x.studyType==128 && x.studyLv==1);
        var prefab=AssetDatabase.LoadAssetAtPath<GameObject>(Source.detailAttrItemPrefab);
        if (!prefab) throw new Exception("Original attr item missing");
        list.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical,100);
        var content=At(list,"sv_list_view/v_list_view/c_list_view"); var old=content.rect.size;
        content.pivot=new Vector2(.5f,1); content.anchorMin=content.pivot; content.anchorMax=content.pivot;
        content.sizeDelta=new Vector2(old.x,80); content.anchoredPosition=Vector2.zero;
        var names=new[] { Source.fixture.language.Single(x=>x.id==level.l_buffDesID).text, Source.fixture.language.Single(x=>x.id==300005).text };
        var values=new[] { String.Format(Source.fixture.language.Single(x=>x.id==300103).text,"0.0",level.desData.Replace("%","")),
                           String.Format(Source.fixture.language.Single(x=>x.id==300104).text,"0",level.power.ToString("N0",System.Globalization.CultureInfo.InvariantCulture)) };
        for (int i=0;i<2;i++)
        {
            var row=UnityEngine.Object.Instantiate(prefab,content,false); row.name="UI_Model_AttrItem"+i;
            var r=row.GetComponent<RectTransform>(); var size=r.rect.size; r.anchorMin=new Vector2(.5f,1); r.anchorMax=r.anchorMin;
            r.pivot=r.anchorMin; r.sizeDelta=size; r.anchoredPosition=new Vector2(0,-40*i); r.localScale=Vector3.one;
            ApplyTexts(row); ApplyChineseLayout(row);
            AtComponent<LanguageText>(r,"lbl_name").text=names[i]; AtComponent<LanguageText>(r,"layout/lbl_crrVaule").text=values[i];
            At(r,"pl_line").gameObject.SetActive(i==0);
        }
    }
    static void PopulatePower()
    {
        var levels=Source.fixture.moreLevels.OrderBy(x=>x.studyLv).ToArray();
        AtComponent<LanguageText>(Detail.transform,"rect/pl_power/lbl_subDes").text=Source.fixture.language.Single(x=>x.id==levels[0].l_buffDesID).text;
        var content=At(Detail.transform,"rect/pl_power/sv_power/v_list_view/c_list_view"); var size=content.rect.size;
        content.pivot=new Vector2(.5f,1); content.anchorMin=content.pivot; content.anchorMax=content.pivot;
        content.sizeDelta=size; content.anchoredPosition=Vector2.zero;
        var prefab=AssetDatabase.LoadAssetAtPath<GameObject>(Source.powerItemPrefab);
        if (!prefab || Mathf.Abs(prefab.GetComponent<RectTransform>().rect.height-50)>.01f) throw new Exception("Original power item size missing");
        var rowSize=prefab.GetComponent<RectTransform>().rect.size;
        content.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical,levels.Length*rowSize.y);
        for (int i=0;i<levels.Length;i++)
        {
            var row=UnityEngine.Object.Instantiate(prefab,content,false); row.name="UI_ResearchPowerItem"+i;
            var r=row.GetComponent<RectTransform>(); r.anchorMin=new Vector2(.5f,1); r.anchorMax=r.anchorMin;
            r.pivot=r.anchorMin; r.sizeDelta=rowSize; r.anchoredPosition=new Vector2(0,-i*rowSize.y); r.localScale=Vector3.one;
            ApplyTexts(row); ApplyChineseLayout(row);
            AtComponent<LanguageText>(r,"lbl_lv").text=levels[i].studyLv.ToString();
            AtComponent<LanguageText>(r,"lbl_ack").text=levels[i].desData;
            AtComponent<LanguageText>(r,"lbl_power").text="+"+levels[i].power.ToString("N0",System.Globalization.CultureInfo.InvariantCulture);
        }
    }
    static void Establish()
    {
        Output=EditorPrefs.GetString(SessionKey); ReadInput();
        Root=GameObject.Find("UIRoot"); Main=GameObject.Find("UI_Win_ResearchMain"); Detail=GameObject.Find("UI_Win_ResearchUpate");
        if (!Root || !Main || !Detail) throw new Exception("PlayMode lost source prefabs");
        Cam=Root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        Target=new RenderTexture(Input.width,Input.height,24,RenderTextureFormat.ARGB32); Target.Create(); Cam.targetTexture=Target;
        RootAnimator=Detail.GetComponent<Animator>(); UpdateRoot=At(Detail.transform,"rect/img_update"); PowerRoot=At(Detail.transform,"rect/pl_power");
        BackButton=At(Detail.transform,"UI_Model_Window_Type1/title/btn_back");
        UpdateAnimator=UpdateRoot.GetComponent<Animator>(); PowerAnimator=PowerRoot.GetComponent<Animator>();
        Binder=Detail.GetComponent<ViewBinder>();
        if (!Binder || !RootAnimator || !UpdateAnimator || !PowerAnimator || !RootAnimator.runtimeAnimatorController ||
            !UpdateAnimator.runtimeAnimatorController || !PowerAnimator.runtimeAnimatorController)
            throw new Exception("Original source Animator/controllers missing");
        UpdateInitialPosition=UpdateRoot.anchoredPosition; PowerInitialPosition=PowerRoot.anchoredPosition;
        UpdateInitialAlpha=UpdateRoot.GetComponent<CanvasGroup>().alpha; PowerInitialAlpha=PowerRoot.GetComponent<CanvasGroup>().alpha;
        Evidence=new Report { schema="rok.research.animation_native.v2", scope=Input.scope, unityVersion=Application.unityVersion,
            inputSha256=Hash(Output+"/ReferenceInputs.json"), sourceResearchInputsSha256=Input.sourceResearchInputsSha256,
            sourceFixtureSha256=Input.sourceFixtureSha256, animationDataSha256=Input.animationDataSha256,
            rendererSha256=Input.rendererSha256, instrumentedViewBinderSha256=Input.instrumentedViewBinderSha256,
            playMode=true, originalUiRoot=true, originalMainPrefab=true,
            originalDetailPrefab=true, originalAnimatorControllers=true, syntheticFixture=true, noAccountOrNetworkAction=true,
            actualUiManagerInvoked=false, sourceCloseEventMethodAdapted=true,
            width=Input.width,height=Input.height };
    }
    static void Seek(Animator animator, string state, float elapsed)
    {
        if (!animator.gameObject.activeInHierarchy) throw new Exception("Cannot seek inactive source Animator " + state);
        // In Unity 2019 the first Update after Play commits the state switch;
        // elapsed time is applied by the following evaluation step.
        animator.Rebind(); animator.Play(state,0,0); animator.Update(0f); animator.Update(Mathf.Max(elapsed,0.000001f));
    }
    static void ResetNodes()
    {
        UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(true);
        UpdateRoot.anchoredPosition=UpdateInitialPosition; PowerRoot.anchoredPosition=PowerInitialPosition;
        UpdateRoot.GetComponent<CanvasGroup>().alpha=UpdateInitialAlpha;
        PowerRoot.GetComponent<CanvasGroup>().alpha=PowerInitialAlpha;
        UpdateAnimator.Rebind(); PowerAnimator.Rebind(); RootAnimator.Rebind();
        Binder.CloseAnimatorEventCount=0; Binder.IsAniCloseing=false; Binder.onCloseAnimatorCallback=null;
        ManagerCallbackCount=0; ManagerDestroyRequested=false;
    }
    static void BeginClose(float elapsed)
    {
        // UIManager.CloseAnimatorUI: guard, callback, Play Close from zero,
        // speed=1. The callback is the original ViewBinder method reached by
        // the original clip AnimationEvent; full UIManager is not running.
        Binder.IsAniCloseing=true;
        Binder.onCloseAnimatorCallback=()=>{ ManagerCallbackCount++; ManagerDestroyRequested=true; };
        RootAnimator.Play("Close",-1,0); RootAnimator.speed=1f;
        RootAnimator.Update(0f); RootAnimator.Update(Mathf.Max(elapsed,0.000001f));
    }
    static void SetSampleState(string phase, float t, out float rootElapsed, out float updateElapsed, out float powerElapsed, out float interruptX, out float interruptAlpha, out float interruptScale, out bool duplicateIgnored)
    {
        ResetNodes(); rootElapsed=.25f; updateElapsed=-1; powerElapsed=-1; interruptX=-999; interruptAlpha=-999; interruptScale=-999; duplicateIgnored=false;
        if (phase=="detail_open")
        {
            UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(false); BackButton.gameObject.SetActive(false);
            rootElapsed=t; Seek(RootAnimator,"Open",rootElapsed); return;
        }
        if (phase=="power_show_128")
        {
            Seek(RootAnimator,"Open",.25f);
            UpdateRoot.gameObject.SetActive(false); PowerRoot.gameObject.SetActive(true); BackButton.gameObject.SetActive(true);
            powerElapsed=t; Seek(PowerAnimator,"Show",powerElapsed); return;
        }
        if (phase=="detail_back_128")
        {
            Seek(RootAnimator,"Open",.25f);
            UpdateRoot.gameObject.SetActive(false); PowerRoot.gameObject.SetActive(true); Seek(PowerAnimator,"Show",.25f);
            UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(false); BackButton.gameObject.SetActive(false);
            updateElapsed=t; Seek(UpdateAnimator,"Show",updateElapsed); return;
        }
        if (phase=="open_then_more_overlap_128")
        {
            rootElapsed=Mathf.Min(.3f,.05f+t); Seek(RootAnimator,"Open",rootElapsed);
            UpdateRoot.gameObject.SetActive(false); PowerRoot.gameObject.SetActive(true); BackButton.gameObject.SetActive(true);
            powerElapsed=t; Seek(PowerAnimator,"Show",powerElapsed); return;
        }
        if (phase=="power_interrupted_to_back_128")
        {
            Seek(RootAnimator,"Open",.25f);
            UpdateRoot.gameObject.SetActive(false); PowerRoot.gameObject.SetActive(true); BackButton.gameObject.SetActive(true);
            Seek(PowerAnimator,"Show",.1f);
            interruptX=PowerRoot.anchoredPosition.x; interruptAlpha=PowerRoot.GetComponent<CanvasGroup>().alpha;
            UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(false); BackButton.gameObject.SetActive(false);
            updateElapsed=t; Seek(UpdateAnimator,"Show",updateElapsed); return;
        }
        if (phase=="detail_close_normal")
        {
            UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(false); BackButton.gameObject.SetActive(false);
            Seek(RootAnimator,"Open",.25f); rootElapsed=t;
            BeginClose(t);
            if (t>=.05f && Binder.IsAniCloseing) duplicateIgnored=true; // UIManager's second CloseUI returns without replay.
            return;
        }
        if (phase=="open_then_close_128")
        {
            UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(false); BackButton.gameObject.SetActive(false);
            Seek(RootAnimator,"Open",.05f); interruptScale=Detail.transform.localScale.x;
            rootElapsed=t; BeginClose(t); return;
        }
        if (phase=="power_then_close_128")
        {
            Seek(RootAnimator,"Open",.25f);
            UpdateRoot.gameObject.SetActive(false); PowerRoot.gameObject.SetActive(true); BackButton.gameObject.SetActive(true);
            Seek(PowerAnimator,"Show",.1f); interruptX=PowerRoot.anchoredPosition.x;
            interruptAlpha=PowerRoot.GetComponent<CanvasGroup>().alpha;
            rootElapsed=t; powerElapsed=.1f+t; BeginClose(t);
            Seek(PowerAnimator,"Show",Mathf.Min(.3f,powerElapsed)); return;
        }
        if (phase=="close_then_more_128")
        {
            UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(false); BackButton.gameObject.SetActive(false);
            Seek(RootAnimator,"Open",.25f); rootElapsed=t; BeginClose(t);
            if (t>=.05f)
            {
                UpdateRoot.gameObject.SetActive(false); PowerRoot.gameObject.SetActive(true); BackButton.gameObject.SetActive(true);
                powerElapsed=t-.05f; Seek(PowerAnimator,"Show",powerElapsed);
            }
            return;
        }
        if (phase=="close_then_back_128")
        {
            Seek(RootAnimator,"Open",.25f);
            UpdateRoot.gameObject.SetActive(false); PowerRoot.gameObject.SetActive(true); BackButton.gameObject.SetActive(true);
            Seek(PowerAnimator,"Show",.25f); rootElapsed=t; BeginClose(t);
            if (t>=.05f)
            {
                UpdateRoot.gameObject.SetActive(true); PowerRoot.gameObject.SetActive(false); BackButton.gameObject.SetActive(false);
                updateElapsed=t-.05f; Seek(UpdateAnimator,"Show",updateElapsed);
            }
            return;
        }
        throw new Exception("Unknown animation phase " + phase);
    }
    static Node Capture(RectTransform r)
    {
        var corners=new Vector3[4]; r.GetWorldCorners(corners);
        var pts=corners.Select(p=>{ var v=RectTransformUtility.WorldToScreenPoint(Cam,p); return new Vector2(v.x,Input.height-v.y); }).ToArray();
        var text=r.GetComponent<Text>(); var group=r.GetComponent<CanvasGroup>();
        return new Node { path=PathOf(r), text=text?text.text:"", activeSelf=r.gameObject.activeSelf,
            activeInHierarchy=r.gameObject.activeInHierarchy, anchoredPosition=r.anchoredPosition, localScale=r.localScale,
            canvasGroupAlpha=group?group.alpha:-1f,
            screenRect=new Rect(pts.Min(p=>p.x),pts.Min(p=>p.y),pts.Max(p=>p.x)-pts.Min(p=>p.x),pts.Max(p=>p.y)-pts.Min(p=>p.y)) };
    }
    static float Normalized(Animator animator)
    { return animator.gameObject.activeInHierarchy ? animator.GetCurrentAnimatorStateInfo(0).normalizedTime : -1f; }
    static string State(Animator animator, string expected)
    { return animator.gameObject.activeInHierarchy && animator.GetCurrentAnimatorStateInfo(0).IsName(expected) ? expected : "inactive_or_other"; }
    static void Tick()
    {
        if (!EditorApplication.isPlaying || !EditorPrefs.HasKey(SessionKey)) return;
        try
        {
            Guard();
            if (Evidence==null) Establish();
            if (PhaseIndex>Phases.Length) return;
            if (PhaseIndex==Phases.Length)
            {
                // Source manager calls Destroy(uiObj) from the event callback.
                // The isolated harness defers that equivalent removal until
                // all independent time samples are captured; this final frame
                // distinguishes transparent-but-alive from removed.
                if (Detail) throw new Exception("Isolated manager-equivalent Destroy did not run");
                var old=RenderTexture.active; RenderTexture.active=Target; GL.Clear(true,true,Color.clear); RenderTexture.active=old;
                Cam.Render();
                var removedImage=new Texture2D(Input.width,Input.height,TextureFormat.RGBA32,false);
                RenderTexture.active=Target; removedImage.ReadPixels(new Rect(0,0,Input.width,Input.height),0,0); removedImage.Apply();
                const string removedPng="detail_removed_after_callback.png";
                File.WriteAllBytes(Output+"/"+removedPng,removedImage.EncodeToPNG());
                UnityEngine.Object.DestroyImmediate(removedImage); RenderTexture.active=null;
                Samples.Add(new Sample { key="detail_removed_after_callback", phase="detail_removed_after_callback", png=removedPng,
                    sha256=Hash(Output+"/"+removedPng), elapsedSeconds=.2f, detailExists=false, simulatedRemoval=true,
                    managerDestroyRequested=true, closeAnimatorEventCount=1, managerCallbackCount=1, actualUiManagerInvoked=false,
                    unityFrame=Time.frameCount });
                Evidence.samples=Samples.ToArray(); Evidence.complete=true;
                File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));
                PhaseIndex++; EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(0); return;
            }
            string phase=Phases[PhaseIndex];
            float[] times=PhaseIndex==0?Animation.clips[0].sampleTimesSeconds:(PhaseIndex>=5?CloseGrid:Grid);
            float t=times[TimeIndex];
            float rootT,updateT,powerT,interruptX,interruptAlpha,interruptScale;
            bool duplicateIgnored;
            SetSampleState(phase,t,out rootT,out updateT,out powerT,out interruptX,out interruptAlpha,out interruptScale,out duplicateIgnored);
            Canvas.ForceUpdateCanvases();
            var prev=RenderTexture.active; RenderTexture.active=Target; GL.Clear(true,true,Color.clear); RenderTexture.active=prev;
            Cam.Render(); Evidence.cameraPixelWidth=Cam.pixelWidth; Evidence.cameraPixelHeight=Cam.pixelHeight;
            if (Cam.pixelWidth!=Input.width || Cam.pixelHeight!=Input.height) throw new Exception("Camera resolution changed");
            var image=new Texture2D(Input.width,Input.height,TextureFormat.RGBA32,false);
            RenderTexture.active=Target; image.ReadPixels(new Rect(0,0,Input.width,Input.height),0,0); image.Apply();
            string key=phase+"_t"+Mathf.RoundToInt(t*10000).ToString("D4"); string png=key+".png";
            File.WriteAllBytes(Output+"/"+png,image.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(image); RenderTexture.active=null;
            var sample=new Sample { key=key, phase=phase, png=png, sha256=Hash(Output+"/"+png), elapsedSeconds=t,
                rootElapsedSeconds=rootT, updateElapsedSeconds=updateT, powerElapsedSeconds=powerT,
                rootNormalizedTime=Normalized(RootAnimator), updateNormalizedTime=Normalized(UpdateAnimator), powerNormalizedTime=Normalized(PowerAnimator),
                rootState=State(RootAnimator,PhaseIndex>=5?"Close":"Open"), updateState=State(UpdateAnimator,"Show"), powerState=State(PowerAnimator,"Show"),
                interruptedOutgoingPowerX=interruptX, interruptedOutgoingPowerAlpha=interruptAlpha,
                interruptedOutgoingRootScale=interruptScale, unityFrame=Time.frameCount,
                closeAnimatorEventCount=Binder.CloseAnimatorEventCount, managerCallbackCount=ManagerCallbackCount,
                isAniClosing=Binder.IsAniCloseing, managerDestroyRequested=ManagerDestroyRequested,
                duplicateCloseIgnored=duplicateIgnored, detailExists=true, simulatedRemoval=false, actualUiManagerInvoked=false,
                updateActive=UpdateRoot.gameObject.activeSelf, powerActive=PowerRoot.gameObject.activeSelf, backActive=BackButton.gameObject.activeSelf,
                originalRootAnimator=true, originalUpdateAnimator=true, originalPowerAnimator=true,
                detailWindow=Capture(Detail.GetComponent<RectTransform>()), updateRoot=Capture(UpdateRoot), powerRoot=Capture(PowerRoot),
                title=Capture(At(Detail.transform,"UI_Model_Window_Type1/title/lbl_title")), backButton=Capture(BackButton),
                powerViewport=Capture(At(Detail.transform,"rect/pl_power/sv_power/v_list_view")) };
            Samples.Add(sample); Evidence.samples=Samples.ToArray();
            File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));
            TimeIndex++;
            if (TimeIndex>=times.Length) { TimeIndex=0; PhaseIndex++; }
            if (PhaseIndex==Phases.Length)
            {
                if (!ManagerDestroyRequested || Binder.CloseAnimatorEventCount!=1)
                    throw new Exception("Final source Close event never requested removal");
                UnityEngine.Object.Destroy(Detail);
            }
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            if (Output!=null) { if (Evidence==null) Evidence=new Report(); Evidence.failure=error.ToString(); Evidence.samples=Samples.ToArray();
                File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true)); }
            EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(1);
        }
    }
}
