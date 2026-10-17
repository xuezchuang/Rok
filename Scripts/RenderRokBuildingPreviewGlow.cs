using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;
public class GlowNativeMeshObserver:MonoBehaviour,IMeshModifier
{
    public Mesh Mesh;
    public void ModifyMesh(VertexHelper helper){if(!Mesh)Mesh=new Mesh();helper.FillMesh(Mesh);}
    public void ModifyMesh(Mesh mesh){if(!Mesh)Mesh=new Mesh();Mesh.vertices=mesh.vertices;Mesh.uv=mesh.uv;Mesh.triangles=mesh.triangles;Mesh.colors32=mesh.colors32;}
}
public class GlowNativeRunner:MonoBehaviour { public void LateUpdate(){RenderRokBuildingPreviewGlow.Sample();} }
public static class RenderRokBuildingPreviewGlow
{
    const string Project="D:/ueDemo/Rok/Saved/UnityBuildingPreviewGlowProbe";
    const string Pending="Rok.BuildingPreviewGlow.Pending";
    [Serializable] public class Entry{public int id;public string text;}
    [Serializable] public class Inputs{public string prefab,scope;public int width,height;public Entry[] texts;}
    [Serializable] public class Layer{public string name,material,shader;public Color tint;public Quaternion localRotation;public Vector3 localScale;public Vector3[] vertices;public Vector2[] uv,screenVertices;public int[] triangles;public Color32[] colors;}
    [Serializable] public class State{public int frame;public float time,elapsed,deltaTime,animatorNormalizedTime;public Layer[] layers;}
    [Serializable] public class Capture{public string name,png,sha256;public int sample;public bool[] visible;public State state;}
    [Serializable] public class Report{public string scope,inputSha256,unityVersion,failure;public bool nativeRendered,playMode,visibilityRestored;public State[] trace;public Capture[] captures;}
    static GameObject Window;static Camera Camera;static RenderTexture Target;static Image[] Images;static Animator EffectAnimator;static Inputs Data;static Report Evidence;static string Output;static float StartTime;
    static readonly List<State> Trace=new List<State>();static readonly List<Capture> Captures=new List<Capture>();static readonly HashSet<ArabLayoutCompment> Applied=new HashSet<ArabLayoutCompment>();
    static string Hash(string p){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(p))).Replace("-","").ToLowerInvariant();}
    static void Guard(){if(!String.Equals(Application.dataPath.Replace('\\','/').TrimEnd('/'),Project+"/Assets",StringComparison.OrdinalIgnoreCase)||Application.unityVersion!="2019.4.28f1c1")throw new Exception("Wrong isolated project/version");}
    [InitializeOnLoadMethod] static void Register(){EditorApplication.playModeStateChanged-=OnMode;EditorApplication.playModeStateChanged+=OnMode;}
    static void OnMode(PlayModeStateChange state){if(state==PlayModeStateChange.EnteredPlayMode&&SessionState.GetBool(Pending,false)){SessionState.SetBool(Pending,false);try{Begin();}catch(Exception e){Fail(e);}}}
    public static void Run(){try{Guard();if(EditorApplication.isPlaying)throw new Exception("Expected isolated editor before PlayMode");SessionState.SetBool(Pending,true);EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);EditorApplication.isPlaying=true;}catch(Exception e){Fail(e);}}
    static void Begin(){
        Guard();if(!Application.isPlaying||QualitySettings.activeColorSpace!=ColorSpace.Gamma)throw new Exception("PlayMode Gamma required");
        Output=Project+"/Artifacts/native_"+DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");Directory.CreateDirectory(Output);File.Copy(Project+"/ReferenceInputs.json",Output+"/ReferenceInputs.json");Data=JsonUtility.FromJson<Inputs>(File.ReadAllText(Output+"/ReferenceInputs.json"));
        Evidence=new Report{scope=Data.scope,inputSha256=Hash(Output+"/ReferenceInputs.json"),unityVersion=Application.unityVersion,playMode=true};
        Camera=new GameObject("ReferenceCamera").AddComponent<Camera>();Camera.clearFlags=CameraClearFlags.SolidColor;Camera.backgroundColor=Color.clear;Camera.allowHDR=false;Camera.allowMSAA=false;
        Target=new RenderTexture(1440,810,24,RenderTextureFormat.ARGB32);Target.Create();Camera.targetTexture=Target;
        var canvas=new GameObject("ReferenceCanvas",typeof(RectTransform),typeof(Canvas)).GetComponent<Canvas>();canvas.renderMode=RenderMode.ScreenSpaceCamera;canvas.worldCamera=Camera;canvas.planeDistance=1;
        Window=UnityEngine.Object.Instantiate(AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab),canvas.transform);
        EffectAnimator=Window.transform.Find("Rect/pl_content/Left/UI_Effect_BuildShow").GetComponent<Animator>();
        foreach(var a in Window.GetComponentsInChildren<Animator>(true))if(a!=EffectAnimator)a.enabled=false;
        var clip=AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim");clip.SampleAnimation(Window,clip.length);
        Window.transform.Find("Rect/pl_content").gameObject.SetActive(true);Window.transform.Find("Rect/pl_sv_level").gameObject.SetActive(false);
        foreach(var t in Window.GetComponentsInChildren<LanguageText>(true)){var x=Data.texts.FirstOrDefault(v=>v.id==t.SourceLanguageId);if(x!=null)t.text=x.text;}
        ApplyChinese();
        Images=new[]{EffectAnimator.transform.Find("img_star003_2").GetComponent<Image>(),EffectAnimator.transform.Find("img_star003_1").GetComponent<Image>()};
        foreach(var i in Images){i.gameObject.AddComponent<GlowNativeMeshObserver>();if(!i.GetComponent<DeemoRotate>().enabled)throw new Exception("Original rotation disabled");}
        if(!EffectAnimator.enabled)throw new Exception("Original effect Animator disabled");
        StartTime=Time.time;new GameObject("NativeTraceRunner").AddComponent<GlowNativeRunner>();
    }
    static State Read(){return new State{frame=Time.frameCount,time=Time.time,elapsed=Time.time-StartTime,deltaTime=Time.deltaTime,animatorNormalizedTime=EffectAnimator.GetCurrentAnimatorStateInfo(0).normalizedTime,layers=Images.Select(i=>{
        var m=i.GetComponent<GlowNativeMeshObserver>().Mesh;if(!m)throw new Exception("Native mesh not generated");
        return new Layer{name=i.name,material=i.materialForRendering.name,shader=i.materialForRendering.shader.name,tint=i.color,localRotation=i.transform.localRotation,localScale=i.transform.localScale,vertices=m.vertices,uv=m.uv,triangles=m.triangles,colors=m.colors32,screenVertices=m.vertices.Select(v=>{var s=RectTransformUtility.WorldToScreenPoint(Camera,i.transform.TransformPoint(v));return new Vector2(s.x,810-s.y);}).ToArray()};}).ToArray()};}
    static readonly float[] thresholds={0,.5f,1,1.5f,2,2.25f};static int next;static bool done;
    public static void Sample(){
        if(done)return;
        {
            try{
                Guard();Canvas.ForceUpdateCanvases();var state=Read();Trace.Add(state);
                if(state.elapsed<thresholds[next])return;
                bool[] saved=Images.Select(i=>i.enabled).ToArray();
                try{foreach(string mode in new[]{"none","both","layer0","layer1"}){
                    Images[0].enabled=mode=="both"||mode=="layer0";Images[1].enabled=mode=="both"||mode=="layer1";
                    Canvas.ForceUpdateCanvases();Camera.Render();RenderTexture.active=Target;
                    var image=new Texture2D(1440,810,TextureFormat.RGBA32,false);image.ReadPixels(new Rect(0,0,1440,810),0,0);image.Apply();
                    string file=String.Format("{0:00}_{1}.png",next,mode);File.WriteAllBytes(Output+"/"+file,image.EncodeToPNG());UnityEngine.Object.Destroy(image);RenderTexture.active=null;
                    Captures.Add(new Capture{sample=next,name=mode,png=file,sha256=Hash(Output+"/"+file),visible=Images.Select(i=>i.enabled).ToArray(),state=state});
                }}finally{for(int n=0;n<Images.Length;n++)Images[n].enabled=saved[n];Canvas.ForceUpdateCanvases();}
                next++;
            }catch(Exception e){done=true;Fail(e);return;}
        }
        if(next<thresholds.Length)return;done=true;
        Evidence.trace=Trace.ToArray();Evidence.captures=Captures.ToArray();Evidence.visibilityRestored=Images.All(i=>i.enabled);Evidence.nativeRendered=true;
        File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));EditorApplication.Exit(0);
    }
    static void Fail(Exception e){Debug.LogException(e);if(Output!=null){if(Evidence==null)Evidence=new Report();Evidence.failure=e.ToString();Evidence.trace=Trace.ToArray();Evidence.captures=Captures.ToArray();File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));}EditorApplication.Exit(1);}
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
