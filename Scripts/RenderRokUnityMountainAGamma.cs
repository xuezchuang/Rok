using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public static class RenderRokUnityMountainAGamma {
    const string Root="D:/ueDemo/Rok";
    const string Output=Root+"/Saved/RokUnityMigration/MountainAGamma";
    [Serializable] public class Sample { public string name; public float[] spriteColor; }
    [Serializable] public class Input { public Sample[] samples; }
    [Serializable] public class Frame { public string name,path; public float[] spriteColor; public int width,height; }
    [Serializable] public class Report {
        public string unityVersion,colorSpace,device,shader,meshName,textureFormat,filterMode;
        public int vertices,triangles,textureWidth,textureHeight,mipmapCount;
        public float[] cameraPosition,cameraRotation,boundsCenter,boundsSize;
        public float fov;
        public Frame[] frames;
        public string scope="Original opaque prefab body, source transforms/material/FBX/texture. Edge renderer excluded to match UE body-only scope. Bounds-framed diagnostic source-direction camera; not complete world GameView.";
    }
    static float[] V(Vector3 v) {return new[]{v.x,v.y,v.z};}
    public static void Run() {
        try { Render(); Debug.Log("ROK_MOUNTAIN_A_GAMMA_NATIVE_COMPLETE"); EditorApplication.Exit(0); }
        catch(Exception e) {Directory.CreateDirectory(Output);File.WriteAllText(Output+"/native_failure.txt",e.ToString());Debug.LogException(e);EditorApplication.Exit(1);}
    }
    static void Render() {
        if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityMountainAGammaProbe/Assets" || Application.unityVersion!="2019.4.28f1c1" || QualitySettings.activeColorSpace!=ColorSpace.Gamma)throw new Exception("Wrong native identity/color space");
        var input=JsonUtility.FromJson<Input>(File.ReadAllText(Output+"/manifest.json"));
        var prefab=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/BundleAssets/land/Tile/AutoMake/Mountain/build_mountain_A.prefab");
        if(!prefab)throw new Exception("Source prefab missing");
        var obj=UnityEngine.Object.Instantiate(prefab);
        var renderers=obj.GetComponentsInChildren<MeshRenderer>(true);
        var body=renderers.Single(r=>r.name=="build_mountain_A");
        foreach(var renderer in renderers) renderer.enabled=renderer==body;
        if(body.sharedMaterial.shader.name!="custom/landmap/z_cm_slg_mountain" || !body.sharedMaterial.shader.isSupported)throw new Exception("Wrong/unsupported source shader");
        var mesh=body.GetComponent<MeshFilter>().sharedMesh;
        var texture=(Texture2D)body.sharedMaterial.mainTexture;
        if(!mesh || !texture || mesh.vertexCount==0)throw new Exception("Incomplete source geometry/art");
        var camera=new GameObject("MountainGammaDiagnosticCamera").AddComponent<Camera>();
        camera.allowHDR=false;camera.allowMSAA=false;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.13f,.13f,.13f,1);
        camera.nearClipPlane=.01f;camera.farClipPlane=10000;camera.aspect=1280f/720;camera.fieldOfView=12;
        camera.transform.rotation=Quaternion.LookRotation(new Vector3(0,-.7f,.7f).normalized,Vector3.up);
        var bounds=body.bounds;
        float distance=bounds.extents.magnitude/Mathf.Sin(camera.fieldOfView*.5f*Mathf.Deg2Rad)*1.15f;
        camera.transform.position=bounds.center-camera.transform.forward*distance;
        var rt=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);rt.Create();camera.targetTexture=rt;
        var frames=new System.Collections.Generic.List<Frame>();
        foreach(var sample in input.samples) {
            Shader.SetGlobalColor("_SpriteColor",new Color(sample.spriteColor[0],sample.spriteColor[1],sample.spriteColor[2],1));
            camera.Render();RenderTexture.active=rt;
            var image=new Texture2D(1280,720,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,1280,720),0,0);image.Apply();
            string path=Output+"/native_"+sample.name+".png";File.WriteAllBytes(path,image.EncodeToPNG());UnityEngine.Object.DestroyImmediate(image);RenderTexture.active=null;
            frames.Add(new Frame{name=sample.name,path=path,spriteColor=sample.spriteColor,width=1280,height=720});
        }
        var report=new Report{unityVersion=Application.unityVersion,colorSpace=QualitySettings.activeColorSpace.ToString(),device=SystemInfo.graphicsDeviceName,shader=body.sharedMaterial.shader.name,meshName=mesh.name,vertices=mesh.vertexCount,triangles=mesh.triangles.Length/3,textureFormat=texture.format.ToString(),filterMode=texture.filterMode.ToString(),textureWidth=texture.width,textureHeight=texture.height,mipmapCount=texture.mipmapCount,cameraPosition=V(camera.transform.position),cameraRotation=V(camera.transform.eulerAngles),boundsCenter=V(bounds.center),boundsSize=V(bounds.size),fov=camera.fieldOfView,frames=frames.ToArray()};
        File.WriteAllText(Output+"/native_render.json",JsonUtility.ToJson(report,true));
        UnityEngine.Object.DestroyImmediate(obj);UnityEngine.Object.DestroyImmediate(camera.gameObject);rt.Release();
    }
}
