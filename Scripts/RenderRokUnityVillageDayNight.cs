using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Client;
using UnityEditor;
using UnityEngine;

public class RenderRokUnityVillageDayNight : MonoBehaviour {
    const string Root="D:/ueDemo/Rok";
    const string Output=Root+"/Saved/RokUnityMigration/VillageDayNightReference";
    [Serializable] public class Ref { public string path; }
    [Serializable] public class Sprite { public Ref mask; }
    [Serializable] public class Item { public string source_transform_id; public int sibling_index; public Sprite[] sprites; }
    [Serializable] public class Library { public Item[] items; }
    [Serializable] public class Preset { public float[] ambient,directional; public float intensity; }
    [Serializable] public class Sample { public string name; public int from_preset,to_preset; public float duration,elapsed,fixed_delay; public bool resolved_night; }
    [Serializable] public class Manifest { public Preset[] presets; public Sample[] samples; }
    [Serializable] public class Frame { public string sample,path; public float elapsed,fixedDelay,dxf; public bool resolvedNight; public float[] spriteColor; public int nightMaterials; public string[] active; }
    [Serializable] public class Report { public string unityVersion,colorSpace,graphicsDevice; public Frame[] frames; public string scope="Original village prefab, source day/night materials and shaders. Explicit post-deadline snapshots use fixed delay inputs; no Random, coroutine, async service, authority clock or full GameView emulation. Source camera at prefab origin; mask.B is zero so shader clock has no emissive effect."; }
    struct CameraKey { public double distance,fov,dxf; }
    public static void Run() {
        if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityVillageDayNightProbe/Assets" || Application.unityVersion!="2019.4.28f1c1")throw new Exception("Wrong native target");
        SessionState.SetBool("RokVillageDayNight",true);EditorApplication.isPlaying=true;
    }
    [InitializeOnLoadMethod] static void Install() {EditorApplication.playModeStateChanged+=state=> {
        if(state==PlayModeStateChange.EnteredPlayMode && SessionState.GetBool("RokVillageDayNight",false))new GameObject("VillageDayNightProbe").AddComponent<RenderRokUnityVillageDayNight>();
    };}
    static void CameraParameters(float dxf, out float distance, out float fov)
    {
        var keys = File.ReadAllLines(Root+"/ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv").Skip(2).Where(l=>!String.IsNullOrWhiteSpace(l))
            .Select(l=>l.Split(',').Select(v=>v.Trim('"')).ToArray()).Select(columns=>new CameraKey {distance=Double.Parse(columns[1],System.Globalization.CultureInfo.InvariantCulture), fov=Double.Parse(columns[2],System.Globalization.CultureInfo.InvariantCulture), dxf=Double.Parse(columns[3],System.Globalization.CultureInfo.InvariantCulture)}).OrderBy(k=>k.dxf).ToArray();
        var hi = Array.FindIndex(keys,k=>k.dxf>=dxf);
        if (hi<=0) throw new Exception("Reference dxf outside interpolation keys");
        var a=keys[hi-1]; var b=keys[hi];
        double t=0, low=0, high=1;
        if (a.fov==b.fov) { distance=(float)(dxf/a.fov); fov=(float)a.fov; return; }
        for (int i=0;i<24;++i)
        {
            t=(low+high)/2;
            double value=(a.distance+t*(b.distance-a.distance))*(a.fov+t*(b.fov-a.fov));
            if (value==dxf) break;
            if (value<dxf) low=t; else high=t;
            t=low;
        }
        distance=(float)(a.distance+t*(b.distance-a.distance)); fov=(float)(a.fov+t*(b.fov-a.fov));
    }
    IEnumerator Start() {
        Directory.CreateDirectory(Output);
        var manifest=JsonUtility.FromJson<Manifest>(File.ReadAllText(Root+"/Saved/RokUnityMigration/unity_village_day_night.json"));
        var library=JsonUtility.FromJson<Library>(File.ReadAllText(Root+"/Saved/RokUnityMigration/unity_village_presentation.json"));
        var prefab=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/build_RssVillage_Big.prefab");
        Common.Distance=480;var obj=Instantiate(prefab);obj.transform.position=Vector3.zero;obj.transform.localScale=Vector3.one;
        yield return null;yield return null;
        var day=AssetDatabase.LoadAssetAtPath<Material>("Assets/Source/map_building.mat");
        var night=AssetDatabase.LoadAssetAtPath<Material>("Assets/Source/city_building_night.mat");
        if(!day || !night || day.shader.name!="custom/map/map_building_with_mask-5" || night.shader.name!="custom/map/cm_city_mask_night")throw new Exception("Source materials/shaders missing");
        var renderers=new List<SpriteRenderer>();var masks=new List<Texture2D>();
        foreach(var item in library.items) {
            var child=obj.transform.GetChild(item.sibling_index);
            if(ProbeRokUnityVillagePresentation.SourceId(prefab,child.GetComponent<GridCollideItem>())!=item.source_transform_id)throw new Exception("Source identity mismatch");
            renderers.Add(child.GetComponentInChildren<SpriteRenderer>(true));
            masks.Add(AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Source/"+Path.GetFileName(item.sprites[0].mask.path)));
        }
        if(renderers.Count!=13 || renderers.Any(r=>!r) || masks.Any(m=>!m))throw new Exception("Incomplete source visual");
        var camera=new GameObject("SourceWorldCamera").AddComponent<Camera>();camera.orthographic=false;camera.nearClipPlane=.01f;camera.farClipPlane=1000;camera.aspect=1280f/720;
        camera.allowHDR=false;camera.allowMSAA=false;camera.clearFlags=CameraClearFlags.SolidColor;
        camera.transform.rotation=Quaternion.LookRotation(new Vector3(0,-.7f,.7f).normalized,Vector3.up);
        float distance,fov;CameraParameters(480,out distance,out fov);camera.transform.position=-camera.transform.forward*distance;camera.fieldOfView=fov;
        var target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var frames=new List<Frame>();
        foreach(var sample in manifest.samples) {
            var a=manifest.presets[sample.from_preset];var b=manifest.presets[sample.to_preset];
            float t=Mathf.Clamp01(sample.elapsed/sample.duration);float intensity=Mathf.Lerp(a.intensity,b.intensity,t);
            var light=new float[3];for(int k=0;k<3;k++)light[k]=Mathf.Lerp(a.ambient[k],b.ambient[k],t)+Mathf.Lerp(a.directional[k],b.directional[k],t)*intensity*.75f;
            Shader.SetGlobalColor("_SpriteColor",new Color(light[0],light[1],light[2],1));Shader.SetGlobalFloat("_CityTransparency",1);
            camera.backgroundColor=new Color(.5255f*light[0],.6314f*light[1],.3216f*light[2],1);
            for(int i=0;i<renderers.Count;i++) {
                renderers[i].sharedMaterial=sample.resolved_night?night:day;
                var block=new MaterialPropertyBlock();renderers[i].GetPropertyBlock(block);block.SetTexture("_Mask",masks[i]);renderers[i].SetPropertyBlock(block);
            }
            yield return null;
            camera.Render();RenderTexture.active=target;
            var image=new Texture2D(1280,720,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,1280,720),0,0);image.Apply();
            string path=Output+"/"+sample.name+".png";File.WriteAllBytes(path,image.EncodeToPNG());Destroy(image);RenderTexture.active=null;
            frames.Add(new Frame{sample=sample.name,path=path,elapsed=sample.elapsed,fixedDelay=sample.fixed_delay,dxf=480,resolvedNight=sample.resolved_night,spriteColor=light,nightMaterials=renderers.Count(r=>r.sharedMaterial==night),active=obj.GetComponentsInChildren<GridCollideItem>().Where(i=>i.m_is_active).Select(i=>ProbeRokUnityVillagePresentation.SourceId(prefab,i)).ToArray()});
        }
        File.WriteAllText(Output+"/native_render.json",JsonUtility.ToJson(new Report{unityVersion=Application.unityVersion,colorSpace=QualitySettings.activeColorSpace.ToString(),graphicsDevice=SystemInfo.graphicsDeviceName,frames=frames.ToArray()},true));
        SessionState.SetBool("RokVillageDayNight",false);Debug.Log("ROK_VILLAGE_DAY_NIGHT_NATIVE_COMPLETE");EditorApplication.Exit(0);
    }
}
