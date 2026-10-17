using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Client;
using UnityEditor;
using UnityEngine;

public class RenderRokUnityFoodDeposit : MonoBehaviour
{
    const string Root = "D:/ueDemo/Rok";
    const string Output = Root+"/Saved/RokUnityMigration/FoodReference";
    [Serializable] public class Ref { public string path; }
    [Serializable] public class Sprite { public Ref texture, mask; }
    [Serializable] public class Item { public string name; public Sprite[] sprites; }
    [Serializable] public class Library { public Item[] items; }
    [Serializable] public class Frame { public float dxf, scale, distance, verticalFov; public string path; public string[] active; }
    [Serializable] public class Report
    {
        public string unityVersion, colorSpace, graphicsDevice;
        public string scope = "Native isolated food prefab on source LightManager camera fill color. Original transforms, sprite assets/import settings and day shader. MaskSprite MPB and daylight globals applied explicitly from source; no full GameView, terrain, night, pool or server simulation. Camera uses CameraParamLod keys and WorldCamera 24-step solver; focus translated to prefab origin.";
        public Frame[] frames;
    }
    struct CameraKey { public double distance, fov, dxf; }
    [InitializeOnLoadMethod] static void Install() { EditorApplication.playModeStateChanged += OnPlay; }
    public static void Run()
    {
        if (Application.dataPath.Replace('\\','/') != Root+"/Saved/UnityFoodDepositProbe/Assets" || Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong native probe target");
        SessionState.SetString("RokFoodProbeMode", "render");
        EditorApplication.isPlaying = true;
    }
    static void OnPlay(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode && SessionState.GetString("RokFoodProbeMode", "") == "render")
            new GameObject("FoodNativeRender").AddComponent<RenderRokUnityFoodDeposit>();
    }
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
    IEnumerator Start()
    {
        Directory.CreateDirectory(Output);
        var prefab=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/build_Rssfood_Big.prefab");
        var library=JsonUtility.FromJson<Library>(File.ReadAllText(Root+"/Content/RokData/unity_food_deposit.json"));
        Common.Distance=480;
        var obj=Instantiate(prefab); obj.transform.position=Vector3.zero; obj.transform.localScale=Vector3.one;
        yield return null; yield return null;
        foreach (var item in library.items)
        {
            var child=obj.transform.Find(item.name);
            if (!child) throw new Exception("Missing source child "+item.name);
            var renderer=child.GetComponentInChildren<SpriteRenderer>(true);
            var mask=AssetDatabase.LoadAssetAtPath<Texture2D>("Assets/Source/"+Path.GetFileName(item.sprites[0].mask.path));
            if (!renderer || !mask || !renderer.sharedMaterial || renderer.sharedMaterial.shader.name!="custom/map/map_building_with_mask-5") throw new Exception("Missing source material chain");
            var block=new MaterialPropertyBlock(); renderer.GetPropertyBlock(block); block.SetTexture("_Mask",mask); renderer.SetPropertyBlock(block);
        }
        // GameEventGlobalMediator day row + LightManager.UpdateLighting globals.
        float day=0.53676474f+0.7137f*1f*.75f;
        Shader.SetGlobalColor("_SpriteColor",new Color(day,day,day,1));
        Shader.SetGlobalFloat("_CityTransparency",1);
        var camera=new GameObject("SourceWorldCamera").AddComponent<Camera>();
        camera.orthographic=false; camera.nearClipPlane=.01f; camera.farClipPlane=1000;
        camera.aspect=1280f/720; camera.clearFlags=CameraClearFlags.SolidColor;
        camera.backgroundColor=new Color(.5255f*day,.6314f*day,.3216f*day,1);
        camera.allowHDR=false; camera.allowMSAA=false;
        camera.transform.rotation=Quaternion.LookRotation(new Vector3(0,-.7f,.7f).normalized,Vector3.up);
        var target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Default);
        target.Create(); camera.targetTexture=target;
        var frames=new List<Frame>();
        var lod=obj.GetComponent<LevelDetailDeposit>();
        foreach (float dxf in new float[] {480,594.4174f,1500})
        {
            Common.Previous=Common.Distance; Common.Distance=dxf; lod.UpdateLod();
            float distance,fov; CameraParameters(dxf,out distance,out fov);
            camera.transform.position=-camera.transform.forward*distance; camera.fieldOfView=fov;
            yield return null;
            camera.Render(); RenderTexture.active=target;
            var image=new Texture2D(1280,720,TextureFormat.RGB24,false);
            image.ReadPixels(new Rect(0,0,1280,720),0,0); image.Apply();
            string path=Output+"/unity_food_"+Mathf.RoundToInt(dxf)+".png";
            File.WriteAllBytes(path,image.EncodeToPNG()); Destroy(image);
            frames.Add(new Frame {dxf=dxf,scale=lod.m_scale_curve.Evaluate(dxf),distance=distance,verticalFov=fov,path=path,
                active=obj.GetComponentsInChildren<GridCollideItem>().Where(i=>i.m_is_active).Select(i=>i.name).ToArray()});
        }
        var report=new Report {unityVersion=Application.unityVersion,colorSpace=QualitySettings.activeColorSpace.ToString(),graphicsDevice=SystemInfo.graphicsDeviceName,frames=frames.ToArray()};
        File.WriteAllText(Output+"/native_render.json",JsonUtility.ToJson(report,true));
        Debug.Log("ROK_FOOD_NATIVE_RENDER_COMPLETE"); EditorApplication.Exit(0);
    }
}
