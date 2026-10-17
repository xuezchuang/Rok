using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public static class RenderRokUnityMountainFGamma {
    const string Root="D:/ueDemo/Rok";
    const string Output=Root+"/Saved/RokUnityMigration/MountainFGamma";
    [Serializable] public class Sample { public string name; public float[] spriteColor; }
    [Serializable] public class Input { public Sample[] samples; }
    [Serializable] public class Frame { public string name,path; public float[] spriteColor; public int width,height; }
    [Serializable] public class Mip {public int level,width,height,alphaZero,alphaOpaque,alphaPartial;public string path;}
    [Serializable] public class TextureInfo {public string assetPath,graphicsFormat,compression,alphaSource,wrapU,wrapV;public int importerMaxSize,compressionQuality,standaloneMaxSize;public bool srgb,mipEnabled,alphaIsTransparency,standaloneOverridden;public Mip[] decodedMips;}
    [Serializable] public class Report {
        public string unityVersion,colorSpace,device,shader,meshName,textureFormat,filterMode;
        public int vertices,triangles,textureWidth,textureHeight,mipmapCount;
        public float[] cameraPosition,cameraRotation,boundsCenter,boundsSize;
        public float fov;
        public Frame[] frames;
        public TextureInfo texture;public string meshGuid,meshLocalId,materialGuid;public int subMeshes;public string[] excludedRenderers;
        public Vector3[] sourcePositions;public Vector2[] sourceUV0;public int[] sourceTriangles;
        public string scope="Original opaque prefab body, source transforms/material/FBX/texture. Edge renderer excluded to match UE body-only scope. Bounds-framed diagnostic source-direction camera; not complete world GameView.";
    }
    static float[] V(Vector3 v) {return new[]{v.x,v.y,v.z};}
    public static void Run() {
        try { Render(); Debug.Log("ROK_MOUNTAIN_F_GAMMA_NATIVE_COMPLETE"); EditorApplication.Exit(0); }
        catch(Exception e) {Directory.CreateDirectory(Output);File.WriteAllText(Output+"/native_failure.txt",e.ToString());Debug.LogException(e);EditorApplication.Exit(1);}
    }
    static void Render() {
        if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityMountainFGammaProbe/Assets" || Application.unityVersion!="2019.4.28f1c1" || QualitySettings.activeColorSpace!=ColorSpace.Gamma)throw new Exception("Wrong native identity/color space");
        var input=JsonUtility.FromJson<Input>(File.ReadAllText(Output+"/manifest.json"));
        var prefab=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/BundleAssets/land/Tile/AutoMake/Mountain/build_mountain_F.prefab");
        if(!prefab)throw new Exception("Source prefab missing");
        var obj=UnityEngine.Object.Instantiate(prefab);
        var renderers=obj.GetComponentsInChildren<MeshRenderer>(true);
        var body=renderers.Single(r=>r.name=="build_mountain_F");
        foreach(var renderer in renderers) renderer.enabled=renderer==body;
        if(body.sharedMaterial.shader.name!="custom/landmap/z_cm_slg_mountain" || !body.sharedMaterial.shader.isSupported)throw new Exception("Wrong/unsupported source shader");
        var mesh=body.GetComponent<MeshFilter>().sharedMesh;
        var texture=(Texture2D)body.sharedMaterial.mainTexture;
        if(!mesh || !texture || mesh.vertexCount==0)throw new Exception("Incomplete source geometry/art");
        string meshGuid,materialGuid;long meshId,materialId;
        if(!AssetDatabase.TryGetGUIDAndLocalFileIdentifier(mesh,out meshGuid,out meshId)||meshGuid!="aee73fed0485b1244aea2938ba78f8ee"||meshId!=3827334489394421038L||mesh.subMeshCount!=1)throw new Exception("Exact source F body mesh identity/submesh mismatch");
        if(!AssetDatabase.TryGetGUIDAndLocalFileIdentifier(body.sharedMaterial,out materialGuid,out materialId)||materialGuid!="13645cd34a40fc8449f0396b7785e3a1")throw new Exception("Exact source F material mismatch");
        string texturePath=AssetDatabase.GetAssetPath(texture);var importer=(TextureImporter)AssetImporter.GetAtPath(texturePath);var platform=importer.GetPlatformTextureSettings("Standalone");
        var textureInfo=new TextureInfo{assetPath=texturePath,graphicsFormat=texture.graphicsFormat.ToString(),compression=importer.textureCompression.ToString(),alphaSource=importer.alphaSource.ToString(),wrapU=texture.wrapModeU.ToString(),wrapV=texture.wrapModeV.ToString(),importerMaxSize=importer.maxTextureSize,compressionQuality=importer.compressionQuality,standaloneMaxSize=platform.maxTextureSize,srgb=importer.sRGBTexture,mipEnabled=importer.mipmapEnabled,alphaIsTransparency=importer.alphaIsTransparency,standaloneOverridden=platform.overridden};
        var decodeShader=Shader.Find("Hidden/RokMountainFMipDecode");if(!decodeShader||!decodeShader.isSupported)throw new Exception("Mip readback shader unavailable");
        var decodeMaterial=new Material(decodeShader);
        if(!decodeMaterial.HasProperty("_MainTex")||!decodeMaterial.HasProperty("_Mip"))throw new Exception("Diagnostic mip shader properties missing");
        decodeMaterial.SetTexture("_MainTex",texture);if(decodeMaterial.GetTexture("_MainTex")!=texture)throw new Exception("Diagnostic source texture binding mismatch");
        var mipRows=new System.Collections.Generic.List<Mip>();
        for(int level=0;level<texture.mipmapCount;level++){
            int width=Mathf.Max(1,texture.width>>level),height=Mathf.Max(1,texture.height>>level);
            var target=RenderTexture.GetTemporary(width,height,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);
            decodeMaterial.SetFloat("_Mip",level);Graphics.Blit(texture,target,decodeMaterial);RenderTexture.active=target;
            var decoded=new Texture2D(width,height,TextureFormat.RGBA32,false,true);decoded.ReadPixels(new Rect(0,0,width,height),0,0);decoded.Apply();
            var mip=new Mip{level=level,width=width,height=height,path=Output+"/native_texture_mip"+level+".png"};
            var pixels=decoded.GetPixels32();int minRgb=255,maxRgb=0;
            foreach(var pixel in pixels){if(pixel.a==0)mip.alphaZero++;else if(pixel.a==255)mip.alphaOpaque++;else mip.alphaPartial++;minRgb=Math.Min(minRgb,Math.Min(pixel.r,Math.Min(pixel.g,pixel.b)));maxRgb=Math.Max(maxRgb,Math.Max(pixel.r,Math.Max(pixel.g,pixel.b)));}
            if(mip.alphaPartial!=0)throw new Exception("DXT1 mip unexpectedly contains fractional alpha");
            if(level==0){
                if(maxRgb-minRgb<16)throw new Exception("Source F mip0 unexpectedly lacks texture variation");
                var direct=RenderTexture.GetTemporary(width,height,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);
                Graphics.Blit(texture,direct);RenderTexture.active=direct;var oracle=new Texture2D(width,height,TextureFormat.RGBA32,false,true);oracle.ReadPixels(new Rect(0,0,width,height),0,0);oracle.Apply();
                var directPixels=oracle.GetPixels32();for(int i=0;i<pixels.Length;i++){var a=pixels[i];var b=directPixels[i];if(Math.Abs(a.r-b.r)>1||Math.Abs(a.g-b.g)>1||Math.Abs(a.b-b.b)>1||a.a!=b.a)throw new Exception("Explicit mip0 differs from independent default Blit texture decode");}
                UnityEngine.Object.DestroyImmediate(oracle);RenderTexture.active=null;RenderTexture.ReleaseTemporary(direct);
            }
            File.WriteAllBytes(mip.path,decoded.EncodeToPNG());mipRows.Add(mip);UnityEngine.Object.DestroyImmediate(decoded);RenderTexture.active=null;RenderTexture.ReleaseTemporary(target);
        }
        textureInfo.decodedMips=mipRows.ToArray();UnityEngine.Object.DestroyImmediate(decodeMaterial);
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
        report.texture=textureInfo;report.meshGuid=meshGuid;report.meshLocalId=meshId.ToString();report.materialGuid=materialGuid;report.subMeshes=mesh.subMeshCount;report.excludedRenderers=renderers.Where(r=>r!=body).Select(r=>r.name).ToArray();report.sourcePositions=mesh.vertices;report.sourceUV0=mesh.uv;report.sourceTriangles=mesh.triangles;
        File.WriteAllText(Output+"/native_render.json",JsonUtility.ToJson(report,true));
        UnityEngine.Object.DestroyImmediate(obj);UnityEngine.Object.DestroyImmediate(camera.gameObject);rt.Release();
    }
}
