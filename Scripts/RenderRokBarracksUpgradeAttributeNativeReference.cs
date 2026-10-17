using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;

// Native original prefab geometry; only RefreshDescAttr presentation is bound locally.
// Surrounding prefab placeholders are outside this attribute-only acceptance scope.
public static class RenderRokBarracksUpgradeAttributeNativeReference
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityBarracksUpgradeAttributeNativeReferenceProbe";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Row { public int label_id, current, delta; public string label, value; public bool separator; }
    [Serializable] public class Inputs { public string scope, prefab, itemPrefab, levelBefore, levelAfter; public int width, height; public float rowHeight, attributeHeight; public Entry[] texts; public Row[] rows; }
    [Serializable] public class TextEvidence { public string text, font; public int fontSize, fontStyle, alignment, lineCount, generatedVertexCount, nondegenerateQuadCount; public Vector3[] generatedLocalVertices; public Vector2[] generatedScreenVertices; public bool richText, allGlyphsFound; public Rect rect, generatedGlyphRect; public Color color; }
    [Serializable] public class RowEvidence { public TextEvidence label, value; public Rect rect; public bool separator; }
    [Serializable] public class Report { public string scope, inputSha256, unityVersion, png, sha256, failure; public bool nativeRendered; public float rowHeight, attributeHeight; public Rect attributeRect, levelRootRect, levelArrowRect, levelArrowDirectedRect; public Vector3[] levelArrowWorldCorners; public Vector2[] levelArrowScreenCorners; public bool levelArrowMirrored; public TextEvidence levelBefore, levelAfter; public float levelBeforePreferredWidth, levelAfterPreferredWidth; public int beforeHorizontalFit, afterHorizontalFit; public RowEvidence[] rows; }
    static Inputs Data; static Report Evidence; static string Output; static GameObject Window;
    static Camera Camera; static Canvas Canvas; static RenderTexture Target; static RectTransform Attribute;
    static readonly List<GameObject> Items = new List<GameObject>();
    static readonly HashSet<ArabLayoutCompment> Applied = new HashSet<ArabLayoutCompment>();
    static int Wait;
    static string Hash(string path) { using(var s=SHA256.Create()) return BitConverter.ToString(s.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static Transform Find(string p) { var t=Window.transform.Find(p); if(!t) throw new Exception("Missing source node "+p); return t; }
    static void Guard() { if(!String.Equals(Application.dataPath.Replace('\\','/').TrimEnd('/'),Project+"/Assets",StringComparison.OrdinalIgnoreCase) || Application.unityVersion!="2019.4.28f1c1") throw new Exception("Wrong isolated project/version"); }
    public static void Run()
    {
        try {
            Guard(); if(QualitySettings.activeColorSpace!=ColorSpace.Gamma) throw new Exception("Expected source Gamma");
            Output=Project+"/Artifacts/native_"+DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff"); Directory.CreateDirectory(Output);
            File.Copy(Project+"/ReferenceInputs.json",Output+"/ReferenceInputs.json");
            Data=JsonUtility.FromJson<Inputs>(File.ReadAllText(Output+"/ReferenceInputs.json"));
            if(Data.rows.Length!=2 || Data.width!=1440 || Data.height!=810) throw new Exception("Wrong bounded inputs");
            Evidence=new Report{scope=Data.scope,inputSha256=Hash(Output+"/ReferenceInputs.json"),unityVersion=Application.unityVersion};
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
            Camera=new GameObject("ReferenceCamera").AddComponent<Camera>(); Camera.clearFlags=CameraClearFlags.SolidColor; Camera.backgroundColor=Color.clear; Camera.allowHDR=false; Camera.allowMSAA=false;
            Target=new RenderTexture(Data.width,Data.height,24,RenderTextureFormat.ARGB32); Target.Create(); Camera.targetTexture=Target;
            Canvas=new GameObject("ReferenceCanvas",typeof(RectTransform),typeof(Canvas)).GetComponent<Canvas>(); Canvas.renderMode=RenderMode.ScreenSpaceCamera; Canvas.worldCamera=Camera; Canvas.planeDistance=1;
            Window=(GameObject)PrefabUtility.InstantiatePrefab(AssetDatabase.LoadAssetAtPath<GameObject>(Data.prefab)); Window.transform.SetParent(Canvas.transform,false);
            foreach(var a in Window.GetComponentsInChildren<Animator>(true)) a.enabled=false;
            var clip=AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim"); if(!clip) throw new Exception("Missing source open clip"); clip.SampleAnimation(Window,clip.length);
            Find("Rect/pl_content").gameObject.SetActive(true); Find("Rect/pl_sv_level").gameObject.SetActive(false);
            Attribute=(RectTransform)Find("Rect/pl_content/pl_Right/pl_attr"); Attribute.gameObject.SetActive(true);
            for(int i=Attribute.childCount-1;i>=0;i--) UnityEngine.Object.DestroyImmediate(Attribute.GetChild(i).gameObject);
            var item=AssetDatabase.LoadAssetAtPath<GameObject>(Data.itemPrefab); if(!item) throw new Exception("Missing source attribute prefab");
            foreach(var row in Data.rows) {
                var go=UnityEngine.Object.Instantiate(item,Attribute); go.transform.localScale=Vector3.one; Items.Add(go);
                float h=((RectTransform)go.transform).rect.height; if(Math.Abs(h-Data.rowHeight)>.001f) throw new Exception("Source item height changed"); Evidence.rowHeight=h;
                go.transform.Find("lbl_name").GetComponent<Text>().text=row.label;
                var value=go.transform.Find("layout/lbl_crrVaule").GetComponent<Text>(); value.supportRichText=true; value.text=row.value;
                go.transform.Find("layout/lbl_addVaule").GetComponent<Text>().text="";
                go.transform.Find("pl_line").gameObject.SetActive(row.separator);
            }
            foreach(var t in Window.GetComponentsInChildren<LanguageText>(true)) {
                if(Items.Any(i=>t.transform.IsChildOf(i.transform))) continue;
                var e=Data.texts.FirstOrDefault(x=>x.id==t.SourceLanguageId); if(e!=null)t.text=e.text;
            }
            Find("Rect/pl_content/pl_Right/UI_Item_Buildinglevelup/lbl_before").GetComponent<Text>().text=Data.levelBefore;
            Find("Rect/pl_content/pl_Right/UI_Item_Buildinglevelup/lbl_before/lbl_after").GetComponent<Text>().text=Data.levelAfter;
            ApplyChinese();
            Attribute.sizeDelta=new Vector2(Attribute.sizeDelta.x,Evidence.rowHeight*Items.Count+30);
            LayoutRebuilder.ForceRebuildLayoutImmediate((RectTransform)Find("Rect/pl_content/pl_Right"));
            foreach(var t in Window.GetComponentsInChildren<Transform>(true)) if(GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(t.gameObject)>0) throw new Exception("Missing source adapter "+t.name);
            EditorApplication.update+=Tick;
        } catch(Exception e){Fail(e);}
    }
    static Rect ScreenRect(RectTransform r) { var v=new Vector3[4];r.GetWorldCorners(v);var a=RectTransformUtility.WorldToScreenPoint(Camera,v[0]);var b=RectTransformUtility.WorldToScreenPoint(Camera,v[2]);return new Rect(a.x,Data.height-b.y,b.x-a.x,b.y-a.y); }
    static TextEvidence ReadText(Text t) {
        if(!t.font)throw new Exception("Missing source font");
        string plain=System.Text.RegularExpressions.Regex.Replace(t.text,"<[^>]+>","");t.font.RequestCharactersInTexture(plain,t.fontSize,t.fontStyle);
        bool found=true;foreach(char ch in plain) {if(char.IsWhiteSpace(ch))continue;CharacterInfo info;found &= t.font.GetCharacterInfo(ch,out info,t.fontSize,t.fontStyle);}
        var gen=new TextGenerator();gen.Populate(t.text,t.GetGenerationSettings(t.rectTransform.rect.size));
        var vertices=gen.verts.ToArray();var visible=new List<Vector2>();int quadCount=0;
        Vector2 first=vertices.Length>0 ? (Vector2)vertices[0].position/t.pixelsPerUnit : Vector2.zero;
        Vector2 rounding=t.PixelAdjustPoint(first)-first;
        var projected=vertices.Select(v=>RectTransformUtility.WorldToScreenPoint(Camera,t.rectTransform.TransformPoint((Vector3)((Vector2)v.position/t.pixelsPerUnit+rounding)))).ToArray();
        for(int q=0;q+3<vertices.Length;q+=4) {
            var quad=projected.Skip(q).Take(4).ToArray();
            float twiceArea=0;for(int k=0;k<4;k++){var a=quad[k];var b=quad[(k+1)%4];twiceArea+=a.x*b.y-b.x*a.y;}
            if(Math.Abs(twiceArea)>0.001f && vertices.Skip(q).Take(4).Any(v=>v.color.a>0)){visible.AddRange(quad);quadCount++;}
        }
        var points=visible.ToArray();
        var glyphRect=points.Length==0 ? new Rect() : new Rect(points.Min(v=>v.x),Data.height-points.Max(v=>v.y),points.Max(v=>v.x)-points.Min(v=>v.x),points.Max(v=>v.y)-points.Min(v=>v.y));
        return new TextEvidence{generatedVertexCount=vertices.Length,nondegenerateQuadCount=quadCount,generatedLocalVertices=vertices.Select(v=>v.position).ToArray(),generatedScreenVertices=projected,generatedGlyphRect=glyphRect,fontStyle=(int)t.fontStyle,text=t.text,font=t.font.name,fontSize=t.fontSize,alignment=(int)t.alignment,lineCount=gen.lineCount,richText=t.supportRichText,allGlyphsFound=found,rect=ScreenRect(t.rectTransform),color=t.color};
    }
    static void Tick() {
        try {
            Guard();UnityEngine.Canvas.ForceUpdateCanvases();EditorApplication.QueuePlayerLoopUpdate();if(++Wait<12)return;
            foreach(var g in Window.GetComponentsInChildren<Graphic>(true))g.SetAllDirty();UnityEngine.Canvas.ForceUpdateCanvases();Camera.Render();
            var level=(RectTransform)Find("Rect/pl_content/pl_Right/UI_Item_Buildinglevelup");
            var before=level.Find("lbl_before").GetComponent<Text>();var after=level.Find("lbl_before/lbl_after").GetComponent<Text>();
            Evidence.levelRootRect=ScreenRect(level);Evidence.levelBefore=ReadText(before);Evidence.levelAfter=ReadText(after);
            var arrow=(RectTransform)level.Find("lbl_before/img_arrow");var corners=new Vector3[4];arrow.GetWorldCorners(corners);
            var screen=corners.Select(v=>RectTransformUtility.WorldToScreenPoint(Camera,v)).Select(v=>new Vector2(v.x,Data.height-v.y)).ToArray();
            Evidence.levelArrowWorldCorners=corners;Evidence.levelArrowScreenCorners=screen;Evidence.levelArrowDirectedRect=ScreenRect(arrow);
            Evidence.levelArrowRect=new Rect(screen.Min(v=>v.x),screen.Min(v=>v.y),screen.Max(v=>v.x)-screen.Min(v=>v.x),screen.Max(v=>v.y)-screen.Min(v=>v.y));
            Evidence.levelArrowMirrored=Evidence.levelArrowDirectedRect.width<0;
            Evidence.levelBeforePreferredWidth=before.preferredWidth;Evidence.levelAfterPreferredWidth=after.preferredWidth;
            Evidence.beforeHorizontalFit=(int)before.GetComponent<ContentSizeFitter>().horizontalFit;Evidence.afterHorizontalFit=(int)after.GetComponent<ContentSizeFitter>().horizontalFit;
            Evidence.attributeHeight=Attribute.rect.height;Evidence.attributeRect=ScreenRect(Attribute);
            Evidence.rows=Items.Select(i=>new RowEvidence{label=ReadText(i.transform.Find("lbl_name").GetComponent<Text>()),value=ReadText(i.transform.Find("layout/lbl_crrVaule").GetComponent<Text>()),rect=ScreenRect((RectTransform)i.transform),separator=i.transform.Find("pl_line").gameObject.activeSelf}).ToArray();
            if(Math.Abs(Evidence.attributeHeight-Data.attributeHeight)>.05f || Evidence.rows.Any(r=>!r.label.allGlyphsFound || !r.value.allGlyphsFound))throw new Exception("Native source height/glyph mismatch");
            RenderTexture.active=Target;var png=new Texture2D(Data.width,Data.height,TextureFormat.RGBA32,false);png.ReadPixels(new Rect(0,0,Data.width,Data.height),0,0);png.Apply();
            Evidence.png="01_barracks_level1_to2_attributes.png";File.WriteAllBytes(Output+"/"+Evidence.png,png.EncodeToPNG());UnityEngine.Object.DestroyImmediate(png);RenderTexture.active=null;
            Evidence.sha256=Hash(Output+"/"+Evidence.png);Evidence.nativeRendered=true;File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));EditorApplication.update-=Tick;EditorApplication.Exit(0);
        }catch(Exception e){Fail(e);}
    }
    static void Fail(Exception e) { Debug.LogException(e);EditorApplication.update-=Tick;if(Output!=null){if(Evidence==null)Evidence=new Report();Evidence.failure=e.ToString();File.WriteAllText(Output+"/render_evidence.json",JsonUtility.ToJson(Evidence,true));}EditorApplication.Exit(1); }
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
