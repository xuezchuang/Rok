using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Spine;
using Spine.Unity;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UI;

// Isolated source-derived reference. No game services or mailbox authority.
public static class RenderRokMailSpineNative {
    const string Root="D:/ueDemo/Rok";
    [Serializable] public class Sample { public float time,canvasPPU; public Vector3[] vertices; public Vector2[] uv; public int[] triangles; public Color32[] colors; public string image; }
    [Serializable] public class Report { public string scope="Original mail nested SkeletonGraphic gift5/stay, parent pulse frozen at 1, source local Rects; camera magnification 10x. No full mediator."; public string unityVersion,colorSpace; public float duration; public Sample[] samples; }
    public static void Run() {
        try { Capture(); EditorApplication.Exit(0); }
        catch(Exception e) { Debug.LogException(e); EditorApplication.Exit(1); }
    }
    static void Capture() {
        if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityMailSpineProbe/Assets"||Application.unityVersion!="2019.4.28f1c1")throw new Exception("Wrong isolated project/version");
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
        var source=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/UI_Item_MailBtn.prefab").GetComponentsInChildren<SkeletonGraphic>(true).Single();
        if(source.initialSkinName!="gift5"||source.startingAnimation!="UE_TreasureBox_stay"||!source.startingLoop||source.timeScale!=1||source.unscaledTime)throw new Exception("Source mail Spine contract changed");
        var camera=new GameObject("ReferenceCamera").AddComponent<Camera>();camera.transform.position=new Vector3(0,0,-10);camera.orthographic=true;camera.orthographicSize=25;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.12f,.16f,.2f,1);
        var target=new RenderTexture(560,500,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var canvasObject=new GameObject("Canvas",typeof(RectTransform),typeof(Canvas));var canvas=canvasObject.GetComponent<Canvas>();canvas.renderMode=RenderMode.WorldSpace;canvas.referencePixelsPerUnit=100;
        ((RectTransform)canvas.transform).sizeDelta=new Vector2(56,50);
        var parent=new GameObject("Source_img_bag",typeof(RectTransform)).GetComponent<RectTransform>();parent.SetParent(canvas.transform,false);parent.anchorMin=parent.anchorMax=new Vector2(.5f,.5f);parent.pivot=new Vector2(.5f,.5f);parent.sizeDelta=((RectTransform)source.transform.parent).sizeDelta;
        if(parent.sizeDelta!=new Vector2(28,25))throw new Exception("Source parent size changed");
        var graphic=UnityEngine.Object.Instantiate(source.gameObject,parent,false).GetComponent<SkeletonGraphic>();graphic.freeze=true;graphic.Initialize(true);
        if(graphic.rectTransform.localScale!=new Vector3(.06f,.06f,.06f)||graphic.rectTransform.anchoredPosition!=new Vector2(0,-5.1f))throw new Exception("Source child transform changed");
        var data=graphic.skeletonDataAsset.GetSkeletonData(false);var animation=data.FindAnimation("UE_TreasureBox_stay");
        var directory=Root+"/Saved/RokUnityMigration/MailSpine/NativeReference/"+DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");Directory.CreateDirectory(directory);
        var samples=new List<Sample>();
        foreach(float time in new[]{0f,.1f,.2f,.4f,.8f,1.2f,1.5833334f,1.6f}) {
            graphic.Skeleton.SetSkin("gift5");graphic.Skeleton.SetToSetupPose();animation.Apply(graphic.Skeleton,-1,time,true,null,1,MixPose.Setup,MixDirection.In);graphic.Skeleton.UpdateWorldTransform();
            graphic.SetAllDirty();Canvas.ForceUpdateCanvases();graphic.UpdateMesh();var mesh=graphic.GetLastMesh();if(!mesh||mesh.vertexCount==0)throw new Exception("Empty source mesh");
            camera.Render();RenderTexture.active=target;var image=new Texture2D(560,500,TextureFormat.RGBA32,false);image.ReadPixels(new Rect(0,0,560,500),0,0);image.Apply();
            var file=directory+"/stay_"+time.ToString("0.000",System.Globalization.CultureInfo.InvariantCulture)+".png";File.WriteAllBytes(file,image.EncodeToPNG());UnityEngine.Object.DestroyImmediate(image);RenderTexture.active=null;
            samples.Add(new Sample{time=time,canvasPPU=canvas.referencePixelsPerUnit,vertices=mesh.vertices,uv=mesh.uv,triangles=mesh.triangles,colors=mesh.colors32,image=file});
        }
        File.WriteAllText(directory+"/native_mesh.json",JsonUtility.ToJson(new Report{unityVersion=Application.unityVersion,colorSpace=QualitySettings.activeColorSpace.ToString(),duration=animation.Duration,samples=samples.ToArray()},true));
        Debug.Log("ROK_MAIL_SPINE_NATIVE_COMPLETE "+directory);
    }
}
