using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Spine;
using Spine.Unity;
using UnityEditor;
using UnityEngine;
using UnityEngine.UI;

// Isolated source SkeletonGraphic reference, never compiled into Rok UE.
public class RenderRokTrainSpineNative : MonoBehaviour {
    const string Root="D:/ueDemo/Rok";
    [Serializable] public class Capture { public int armsId; public string model,path; public float time,canvasPPU; public Vector3[] vertices; public Vector2[] uv; public int[] triangles; public Color32[] colors; public Vector3[] screenCorners; }
    [Serializable] public class Report { public string unityVersion,colorSpace,graphicsDevice; public string scope="Source SkeletonGraphic/PMA material, original skin/transform. Source-sized pl_left centered in isolated canvas; not full TrainArmy GameView."; public Capture[] captures; }
    public static void Run() {
        if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityTrainSpineProbe/Assets" || Application.unityVersion!="2019.4.28f1c1")throw new Exception("Wrong native reference project/version");
        SessionState.SetBool("RokTrainNative",true);EditorApplication.isPlaying=true;
    }
    [InitializeOnLoadMethod] static void Install() { EditorApplication.playModeStateChanged+=s=> {
        if(s==PlayModeStateChange.EnteredPlayMode && SessionState.GetBool("RokTrainNative",false))new GameObject("NativeTrainCapture").AddComponent<RenderRokTrainSpineNative>();
    }; }
    IEnumerator Start() {
        var camera=new GameObject("NativeUICamera").AddComponent<Camera>();
        camera.transform.position=new Vector3(0,0,-10);camera.orthographic=true;camera.orthographicSize=405;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.12f,.16f,.20f,1);
        var target=new RenderTexture(1440,810,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var canvasObject=new GameObject("NativeCanvas",typeof(RectTransform),typeof(Canvas),typeof(CanvasScaler));
        var canvas=canvasObject.GetComponent<Canvas>();canvas.renderMode=RenderMode.ScreenSpaceCamera;canvas.worldCamera=camera;canvas.planeDistance=1;
        var scaler=canvasObject.GetComponent<CanvasScaler>();scaler.uiScaleMode=CanvasScaler.ScaleMode.ScaleWithScreenSize;scaler.referenceResolution=new Vector2(1440,810);scaler.screenMatchMode=CanvasScaler.ScreenMatchMode.Expand;scaler.referencePixelsPerUnit=100;
        var source=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/UI_Win_TrainArmy.prefab").GetComponentsInChildren<SkeletonGraphic>(true).Single(g=>g.name=="pl_spine");
        var parent=new GameObject("SourceSize_pl_left",typeof(RectTransform)).GetComponent<RectTransform>();parent.SetParent(canvas.transform,false);parent.anchorMin=parent.anchorMax=new Vector2(.5f,.5f);parent.pivot=new Vector2(.5f,.5f);parent.sizeDelta=((RectTransform)source.transform.parent).sizeDelta;
        var material=AssetDatabase.LoadAssetAtPath<Material>("Assets/Source/SkeletonGraphicDefault_1.mat");
        if(!material || material.shader.name!="Spine/SkeletonGraphic (Premultiply Alpha)")throw new Exception("Source PMA material missing");
        var captures=new List<Capture>();
        var ids=new[]{10101,10101,10201,20131};var models=new[]{"Arms1_1_SD","Arms1_1_SD","Arms1_2_SD","ArmsSpecial3_3_SD"};var times=new[]{0f,.5f,.5f,.5f};
        for(int i=0;i<ids.Length;i++) {
            var graphic=Instantiate(source.gameObject,parent,false).GetComponent<SkeletonGraphic>();graphic.freeze=true;
            graphic.material=material;graphic.skeletonDataAsset=AssetDatabase.LoadAssetAtPath<SkeletonDataAsset>("Assets/Models/"+models[i]+"/"+models[i]+".asset");
            graphic.Initialize(true);graphic.freeze=true;
            yield return null;
            var data=graphic.skeletonDataAsset.GetSkeletonData(true);graphic.Skeleton.SetSkin(data.DefaultSkin??data.Skins.Items[0]);graphic.Skeleton.SetToSetupPose();
            var animation=data.FindAnimation("idle");if(animation==null)throw new Exception("Missing source idle");
            animation.Apply(graphic.Skeleton,-1,times[i],false,null,1,MixPose.Setup,MixDirection.In);graphic.Skeleton.UpdateWorldTransform();
            Canvas.ForceUpdateCanvases();graphic.UpdateMesh();
            var mesh=graphic.GetLastMesh();if(!mesh || mesh.vertexCount==0)throw new Exception("Empty native SkeletonGraphic mesh");
            camera.Render();RenderTexture.active=target;
            var image=new Texture2D(1440,810,TextureFormat.RGBA32,false);image.ReadPixels(new Rect(0,0,1440,810),0,0);image.Apply();
            string dir=Root+"/Saved/RokUnityMigration/TrainArmy/NativeReference";Directory.CreateDirectory(dir);
            string path=dir+"/arms_"+ids[i]+"_"+(times[i]*1000).ToString("0")+"ms.png";File.WriteAllBytes(path,image.EncodeToPNG());Destroy(image);RenderTexture.active=null;
            var corners=new Vector3[4];graphic.rectTransform.GetWorldCorners(corners);corners=corners.Select(p=>camera.WorldToScreenPoint(p)).ToArray();
            captures.Add(new Capture{armsId=ids[i],model=models[i],time=times[i],path=path,canvasPPU=canvas.referencePixelsPerUnit,vertices=mesh.vertices,uv=mesh.uv,triangles=mesh.triangles,colors=mesh.colors32,screenCorners=corners});
            Destroy(graphic.gameObject);yield return null;
        }
        File.WriteAllText(Root+"/Saved/RokUnityMigration/TrainArmy/NativeReference/native_render.json",JsonUtility.ToJson(new Report{unityVersion=Application.unityVersion,colorSpace=QualitySettings.activeColorSpace.ToString(),graphicsDevice=SystemInfo.graphicsDeviceName,captures=captures.ToArray()},true));
        SessionState.SetBool("RokTrainNative",false);Debug.Log("ROK_TRAIN_SPINE_NATIVE_COMPLETE");EditorApplication.Exit(0);
    }
}
