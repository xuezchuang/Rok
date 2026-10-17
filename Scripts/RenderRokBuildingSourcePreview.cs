using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Security.Cryptography;
using UnityEngine;
using UnityEngine.UI;
using UnityEditor;
using UnityEditor.SceneManagement;
// Read-only mesh observer receives the actual native Graphic pipeline vertex helper.
// Unity2019 CanvasRenderer has no public GetMesh; this modifier never mutates the helper.
public class BuildingPreviewMeshObserver : MonoBehaviour, IMeshModifier
{
    public Mesh Captured;
    public void ModifyMesh(VertexHelper helper){if(!Captured)Captured=new Mesh();helper.FillMesh(Captured);}
    public void ModifyMesh(Mesh mesh){if(!Captured)Captured=new Mesh();Captured.vertices=mesh.vertices;Captured.uv=mesh.uv;Captured.colors32=mesh.colors32;Captured.triangles=mesh.triangles;}
}
public static class RenderRokBuildingSourcePreview
{
    const string Project="D:/ueDemo/Rok/Saved/UnityBuildingSourcePreviewProbe";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Inputs { public string scope,prefab,buildingPrefab,modelId;public int width,height;public Entry[] texts; }
    [Serializable] public class Node { public string name;public Rect rect;public Vector3 localScale;public Vector2 size,pivot,anchoredPosition;public float alpha; }
    [Serializable] public class Frame { public string png,sha256;public bool buildingVisible; }
    [Serializable] public class Report { public string scope,modelId,inputSha256,unityVersion,failure,spriteGuid,spritePath,materialName,shaderName;public bool nativeRendered;public Vector2 spriteSize;public Node[] ancestors;public Rect imageRect;public Vector3[] vertices;public Vector2[] uv,screenVertices;public Color32[] colors;public int[] triangles;public Frame[] frames; }
    static Inputs Data;static Report Evidence;static string Output;static GameObject Window,Building;static Image Sprite;
    static Camera Camera;static Canvas Canvas;static RenderTexture Target;static int Wait,Step;
    static readonly HashSet<ArabLayoutCompment> Applied=new HashSet<ArabLayoutCompment>();
    static readonly List<Frame> Frames=new List<Frame>();
    static Transform Find(string p) {var t=Window.transform.Find(p);if(!t)throw new Exception("Missing source node "+p);return t;}
    static string Hash(string p){using(var s=SHA256.Create())return BitConverter.ToString(s.ComputeHash(File.ReadAllBytes(p))).Replace("-","").ToLowerInvariant();}
    static void Guard(){if(!String.Equals(Application.dataPath.Replace('\\','/').TrimEnd('/'),Project+"/Assets",StringComparison.OrdinalIgnoreCase)||Application.unityVersion!="2019.4.28f1c1")throw new Exception("Wrong isolated project/version");}
    public static void Run(){try{
        Guard();if(QualitySettings.activeColorSpace!=ColorSpace.Gamma)throw new Exception("Expected source Gamma");
        Output=Project+"/Artifacts/native_"+DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");Directory.CreateDirectory(Output);File.Copy(Project+"/ReferenceInputs.json",Output+"/ReferenceInputs.json");Data=JsonUtility.FromJson<Inputs>(File.ReadAllText(Output+"/ReferenceInputs.json"));
        if(Data.modelId!="build_9_101_1"||Data.width!=1440||Data.height!=810)throw new Exception("Unexpected explicit fixture");
        Evidence=new Report{scope=Data.scope,modelId=Data.modelId,inputSha256=Hash(Output+"/ReferenceInputs.json"),unityVersion=Application.unityVersion};
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
        Camera=new GameObject("ReferenceCamera").AddComponent<Camera>();Camera.clearFlags=CameraClearFlags.SolidColor;Camera.backgroundColor=Color.clear;Camera.allowHDR=false;Camera.allowMSAA=false;
        Target=new RenderTexture(Data.width,Data.height,24,RenderTextureFormat.ARGB32);Target.Create();Camera.targetTexture=Target;
        Canvas=new GameObject("ReferenceCanvas",typeof(RectTransform),typeof(Canvas)).GetComponent<Canvas>();Canvas.renderMode=RenderMode.ScreenSpaceCamera;Canvas.worldCamera=Camera;Canvas.planeDistance=1;
        Window=(GameObject)PrefabUtility.InstantiatePrefab(AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab));Window.transform.SetParent(Canvas.transform,false);
        foreach(var a in Window.GetComponentsInChildren<Animator>(true))a.enabled=false;
        var clip=AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim");if(!clip)throw new Exception("Missing source clip");clip.SampleAnimation(Window,clip.length);
        Find("Rect/pl_content").gameObject.SetActive(true);Find("Rect/pl_sv_level").gameObject.SetActive(false);
        foreach(var t in Window.GetComponentsInChildren<LanguageText>(true)){var x=Data.texts.FirstOrDefault(v=>v.id==t.SourceLanguageId);if(x!=null)t.text=x.text;}
        var parent=Find("Rect/pl_content/Left/pl_buildImg");for(int i=parent.childCount-1;i>=0;i--)UnityEngine.Object.DestroyImmediate(parent.GetChild(i).gameObject);
        Building=(GameObject)PrefabUtility.InstantiatePrefab(AssetDatabase.LoadAssetAtPath<GameObject>(Data.buildingPrefab));Building.transform.SetParent(parent);Building.transform.localPosition=Vector3.zero;Building.transform.localScale=Vector3.one;
        Sprite=Building.transform.Find("img").GetComponent<Image>();if(!Sprite||!Sprite.sprite)throw new Exception("Missing source image/sprite");
        Sprite.gameObject.AddComponent<BuildingPreviewMeshObserver>();
        ApplyChinese();Building.SetActive(false);EditorApplication.update+=Tick;
    }catch(Exception e){Fail(e);}}
    static Vector2 ProjectPoint(Vector3 p){var v=RectTransformUtility.WorldToScreenPoint(Camera,p);return new Vector2(v.x,Data.height-v.y);}
    static Rect ScreenRect(RectTransform r){var v=new Vector3[4];r.GetWorldCorners(v);var p=v.Select(ProjectPoint).ToArray();return new Rect(p.Min(x=>x.x),p.Min(x=>x.y),p.Max(x=>x.x)-p.Min(x=>x.x),p.Max(x=>x.y)-p.Min(x=>x.y));}
    static void Tick(){try{
        Guard();UnityEngine.Canvas.ForceUpdateCanvases();EditorApplication.QueuePlayerLoopUpdate();if(++Wait<12)return;
        foreach(var g in Window.GetComponentsInChildren<Graphic>(true))g.SetAllDirty();UnityEngine.Canvas.ForceUpdateCanvases();Camera.Render();
        string name=Step==0?"01_source_container_without_building.png":"02_source_building_preview.png";
        RenderTexture.active=Target;var png=new Texture2D(Data.width,Data.height,TextureFormat.RGBA32,false);png.ReadPixels(new Rect(0,0,Data.width,Data.height),0,0);png.Apply();File.WriteAllBytes(Output+"/"+name,png.EncodeToPNG());UnityEngine.Object.DestroyImmediate(png);RenderTexture.active=null;
        Frames.Add(new Frame{png=name,sha256=Hash(Output+"/"+name),buildingVisible=Building.activeSelf});
        if(Step++==0){Building.SetActive(true);Wait=0;return;}
        var mesh=Sprite.GetComponent<BuildingPreviewMeshObserver>().Captured;if(!mesh)throw new Exception("Actual native mesh not observed");Evidence.vertices=mesh.vertices;Evidence.triangles=mesh.triangles;Evidence.uv=mesh.uv;Evidence.colors=mesh.colors32;Evidence.screenVertices=mesh.vertices.Select(v=>ProjectPoint(Sprite.transform.TransformPoint(v))).ToArray();
        if(Evidence.vertices.Length!=4||Evidence.triangles.Length!=6)throw new Exception("Source native simple image mesh changed");
        Evidence.spritePath=AssetDatabase.GetAssetPath(Sprite.sprite);Evidence.spriteGuid=AssetDatabase.AssetPathToGUID(Evidence.spritePath);Evidence.spriteSize=Sprite.sprite.rect.size;
        Evidence.materialName=Sprite.materialForRendering.name;Evidence.shaderName=Sprite.materialForRendering.shader.name;Evidence.imageRect=ScreenRect(Sprite.rectTransform);
        var nodes=new List<Node>();for(Transform t=Sprite.transform;t&&t!=Canvas.transform;t=t.parent){var r=t as RectTransform;if(!r)continue;var cg=t.GetComponent<CanvasGroup>();nodes.Add(new Node{name=t.name,rect=ScreenRect(r),localScale=t.localScale,size=r.rect.size,pivot=r.pivot,anchoredPosition=r.anchoredPosition,alpha=cg?cg.alpha:1});}Evidence.ancestors=nodes.ToArray();Evidence.frames=Frames.ToArray();Evidence.nativeRendered=true;
        File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));EditorApplication.update-=Tick;EditorApplication.Exit(0);
    }catch(Exception e){Fail(e);}}
    static void Fail(Exception e){Debug.LogException(e);EditorApplication.update-=Tick;if(Output!=null){if(Evidence==null)Evidence=new Report();Evidence.failure=e.ToString();Evidence.frames=Frames.ToArray();File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));}EditorApplication.Exit(1);}
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
