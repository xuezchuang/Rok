using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using UnityEngine;
using UnityEngine.UI;
using UnityEditor;
using UnityEditor.SceneManagement;

public static class RenderRokBarracksSourceText
{
    const string Project="D:/ueDemo/Rok/Saved/UnityBarracksSourceTextProbe",Repo="D:/ueDemo/Rok/";
    [Serializable] public class Source { public string path,sha256; }
    [Serializable] public class Spec { public string key,text; public int fontSize,style,alignment,horizontalOverflow,verticalOverflow; public float x,y,width,height,lineSpacing; public bool rich,bestFit; public Color color; }
    [Serializable] public class Inputs { public string schema,renderer_sha256,copied_renderer_sha256,scope; public Source[] sources; public Spec[] sourceTexts; }
    [Serializable] public class Glyph { public int codepoint,advance,minX,maxX,minY,maxY; public Vector2 uvBottomLeft,uvBottomRight,uvTopLeft,uvTopRight; }
    [Serializable] public class TextMesh { public string key,text,atlas,atlasSha256; public float width,height,preferredWidth,preferredHeight,pixelsPerUnit; public int fontSize,style,alignment,lineCount; public Vector2[] xy,uv; public Color32[] colors; public int[] triangles; public Glyph[] glyphs; }
    [Serializable] public class Report { public string schema="rok.barracks.source_text.v1",inputSha256,unityVersion; public TextMesh[] texts; }
    static Inputs Data;static string Output;static Camera Camera;static Canvas Canvas;static RenderTexture Target;static Font Font;static int Wait;
    static readonly List<BarracksSourceExportText> Labels=new List<BarracksSourceExportText>();
    static string Hash(string path){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(path))).Replace("-","").ToLowerInvariant();}
    public static void Run()
    {
        try {
            if(Application.dataPath.Replace('\\','/').TrimEnd('/')!=Project+"/Assets"||Application.unityVersion!="2019.4.28f1c1"||QualitySettings.activeColorSpace!=ColorSpace.Gamma)throw new Exception("Wrong isolated project/version/color space");
            Data=JsonUtility.FromJson<Inputs>(File.ReadAllText(Project+"/ReferenceInputs.json"));
            if(Data.schema!="rok.barracks.source_text.inputs.v1"||Data.sourceTexts.Length!=6)throw new Exception("Wrong source coverage");
            foreach(var s in Data.sources)if(Hash(Repo+s.path)!=s.sha256)throw new Exception("Source drift "+s.path);
            if(Hash(Project+"/Assets/Editor/RenderRokBarracksSourceText.cs")!=Data.copied_renderer_sha256)throw new Exception("Executed renderer drift");
            Output=Project+"/Artifacts/native_"+DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");Directory.CreateDirectory(Output);Directory.CreateDirectory(Output+"/SourceTextAtlases");
            File.Copy(Project+"/ReferenceInputs.json",Output+"/ReferenceInputs.json");
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
            Camera=new GameObject("Camera").AddComponent<Camera>();Camera.clearFlags=CameraClearFlags.SolidColor;Camera.backgroundColor=new Color(.2f,.2f,.2f,1);Camera.allowHDR=false;Camera.allowMSAA=false;
            Target=new RenderTexture(1440,810,24,RenderTextureFormat.ARGB32);Camera.targetTexture=Target;
            Canvas=new GameObject("Canvas",typeof(RectTransform)).AddComponent<Canvas>();Canvas.renderMode=RenderMode.ScreenSpaceCamera;Canvas.worldCamera=Camera;Canvas.planeDistance=1;Canvas.referencePixelsPerUnit=100;
            var scaler=Canvas.gameObject.AddComponent<CanvasScaler>();scaler.uiScaleMode=CanvasScaler.ScaleMode.ConstantPixelSize;scaler.scaleFactor=1;scaler.referencePixelsPerUnit=100;
            Font=Resources.GetBuiltinResource<Font>("Arial.ttf");if(!Font)throw new Exception("Missing original built-in Arial");
            foreach(var spec in Data.sourceTexts)
            {
                var go=new GameObject(spec.key,typeof(RectTransform),typeof(CanvasRenderer),typeof(BarracksSourceExportText));go.transform.SetParent(Canvas.transform,false);
                var t=go.GetComponent<BarracksSourceExportText>();t.font=Font;t.fontSize=spec.fontSize;t.fontStyle=(FontStyle)spec.style;t.alignment=(TextAnchor)spec.alignment;t.color=spec.color;t.supportRichText=spec.rich;t.lineSpacing=spec.lineSpacing;t.resizeTextForBestFit=spec.bestFit;
                t.horizontalOverflow=(HorizontalWrapMode)spec.horizontalOverflow;t.verticalOverflow=(VerticalWrapMode)spec.verticalOverflow;t.text=spec.text;
                t.rectTransform.anchorMin=t.rectTransform.anchorMax=t.rectTransform.pivot=new Vector2(0,1);t.rectTransform.anchoredPosition=new Vector2(spec.x,-spec.y);t.rectTransform.sizeDelta=new Vector2(spec.width,spec.height);Labels.Add(t);
                Font.RequestCharactersInTexture(Regex.Replace(spec.text,"<.*?>",""),spec.fontSize,(FontStyle)spec.style);
            }
            EditorApplication.update+=Tick;
        }catch(Exception e){Fail(e);}
    }
    static void Tick()
    {
        if(++Wait<40)return;
        try {
            var result=new List<TextMesh>();var mesh=new Mesh();
            for(int i=0;i<Labels.Count;i++)
            {
                var t=Labels[i];var spec=Data.sourceTexts[i];foreach(var label in Labels)label.SetAllDirty();Canvas.ForceUpdateCanvases();Camera.Render();t.CaptureMesh(mesh);
                if(mesh.vertexCount==0||mesh.vertexCount%4!=0||t.pixelsPerUnit!=1)throw new Exception("Invalid glyph mesh "+spec.key);
                var chars=Regex.Replace(spec.text,"<.*?>","").Distinct();var glyphs=new List<Glyph>();
                foreach(char c in chars){if(Char.IsControl(c))continue;CharacterInfo g;if(!Font.GetCharacterInfo(c,out g,spec.fontSize,(FontStyle)spec.style))throw new Exception("Missing glyph "+spec.key+":"+(int)c);glyphs.Add(new Glyph{codepoint=c,advance=g.advance,minX=g.minX,maxX=g.maxX,minY=g.minY,maxY=g.maxY,uvBottomLeft=g.uvBottomLeft,uvBottomRight=g.uvBottomRight,uvTopLeft=g.uvTopLeft,uvTopRight=g.uvTopRight});}
                string temporary=Output+"/pending.png";SaveTexture(Font.material.mainTexture,temporary);string hash=Hash(temporary),atlas="SourceTextAtlases/"+hash+".png";if(!File.Exists(Output+"/"+atlas))File.Move(temporary,Output+"/"+atlas);else File.Delete(temporary);
                result.Add(new TextMesh{key=spec.key,text=spec.text,width=t.rectTransform.rect.width,height=t.rectTransform.rect.height,fontSize=spec.fontSize,style=spec.style,alignment=spec.alignment,lineCount=t.cachedTextGenerator.lineCount,pixelsPerUnit=t.pixelsPerUnit,preferredWidth=t.preferredWidth,preferredHeight=t.preferredHeight,xy=mesh.vertices.Select(v=>new Vector2(v.x,-v.y)).ToArray(),uv=mesh.uv,colors=mesh.colors32,triangles=mesh.triangles,glyphs=glyphs.ToArray(),atlas=atlas,atlasSha256=hash});
            }
            foreach(var t in Labels)t.SetAllDirty();Canvas.ForceUpdateCanvases();Camera.Render();SaveTexture(Target,Output+"/six_source_texts.png");
            File.WriteAllText(Output+"/source_text_meshes.json",JsonUtility.ToJson(new Report{inputSha256=Hash(Output+"/ReferenceInputs.json"),unityVersion=Application.unityVersion,texts=result.ToArray()},true));
            UnityEngine.Object.DestroyImmediate(mesh);EditorApplication.update-=Tick;EditorApplication.Exit(0);
        }catch(Exception e){Fail(e);}
    }
    static void SaveTexture(Texture texture,string path){var previous=RenderTexture.active;var rt=RenderTexture.GetTemporary(texture.width,texture.height,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);Graphics.Blit(texture,rt);RenderTexture.active=rt;var image=new Texture2D(texture.width,texture.height,TextureFormat.RGBA32,false);image.ReadPixels(new Rect(0,0,texture.width,texture.height),0,0);image.Apply();File.WriteAllBytes(path,image.EncodeToPNG());UnityEngine.Object.DestroyImmediate(image);RenderTexture.active=previous;RenderTexture.ReleaseTemporary(rt);}
    static void Fail(Exception e){Debug.LogException(e);if(Output!=null)File.WriteAllText(Output+"/failure.txt",e.ToString());EditorApplication.update-=Tick;EditorApplication.Exit(1);}
}
public class BarracksSourceExportText:Text
{
    public void CaptureMesh(Mesh mesh){using(var vh=new VertexHelper()){base.OnPopulateMesh(vh);vh.FillMesh(mesh);}}
}
