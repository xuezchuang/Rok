using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

// Run only in the isolated project. Uses normal Animator/ParticleSystem updates
// and the copied ORIGINAL AutoDestory component, not a probe lifetime imitation.
[InitializeOnLoad]
public static class ProbeRokUnityPowerReplayVisual
{
    const string Key = "RokPowerReplayVisualActive";
    static double watchdog;
    static ProbeRokUnityPowerReplayVisual() { EditorApplication.update += Watch; }
    public static void Run()
    {
        if (Application.unityVersion != "2019.4.28f1c1" || Application.dataPath.Replace('\\','/') != "D:/ueDemo/Rok/Saved/UnityBuiltinShaderProbe/Assets")
            throw new Exception("Wrong Unity version/project");
        // CPU geometry inspection only: retain a readable CPU mesh copy in this
        // isolated import. No source asset, transform, mesh data, or shader changes.
        const string modelPath="Assets/PowerGain/BundleAssets/Effect/Meshes/m_liupian001.FBX";
        var importer=AssetImporter.GetAtPath(modelPath) as ModelImporter;
        if(importer==null)throw new Exception("Missing isolated source model importer");
        if(!importer.isReadable){importer.isReadable=true;importer.SaveAndReimport();}
        if(!importer.isReadable)throw new Exception("Isolated mesh Read/Write adaptation failed");
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
        SessionState.SetBool(Key, true); EditorApplication.isPlaying = true;
    }
    static void Watch()
    {
        if (!SessionState.GetBool(Key,false) || !EditorApplication.isPlaying || EditorApplication.isCompiling) return;
        if (!UnityEngine.Object.FindObjectOfType<RokPowerReplayVisualRunner>())
        {
            watchdog=EditorApplication.timeSinceStartup;
            new GameObject("Bounded source replay visual probe").AddComponent<RokPowerReplayVisualRunner>();
        }
        if (EditorApplication.timeSinceStartup-watchdog > 180) Done(false,"Real Play Mode watchdog expired");
    }
    public static void Done(bool success,string message)
    {
        Debug.Log("ROK_POWER_REPLAY_VISUAL " + success + " " + message);
        SessionState.SetBool(Key,false);EditorApplication.Exit(success?0:1);
    }
}

public class RokPowerReplayVisualRunner : MonoBehaviour
{
    const string Output="D:/ueDemo/Rok/Saved/RokUnityMigration/PowerGain/ReplayVisual";
    [Serializable] public class State
    {
        public int run,frame,particleCount; public float elapsed,unityTime,delta,rootLifetime,particleTime,uTiling,materialUTiling,animatorNormalized;
        public bool rootAlive,playing,hasMaterialPropertyBlock; public Vector4 propertyBlockMainTexST; public string operation;
        public float[] remaining; public uint[] seeds;
    }
    [Serializable] public class Draw
    {
        public Vector3[] positions; public Vector2[] uv; public Color[] colors; public int[] indices;
    }
    [Serializable] public class Capture
    {
        public float target; public State state; public string[] images; public Draw mesh,particle;
    }
    [Serializable] public class Report
    {
        public string unityVersion,mode="Actual Play Mode ticks; no Simulate, Animator.Update, AnimationClip.SampleAnimation, or synthetic clock.";
        public string background="Gray and synthetic colored HUD test bands; not a source HUD reconstruction.";
        public string importAdaptation="Only isolated Assets/PowerGain/BundleAssets/Effect/Meshes/m_liupian001.FBX ModelImporter.isReadable=true for CPU geometry inspection; source meta remains isReadable=0. Vertex data, transform and shader unchanged.";
        public string scope="Original AutoDestory and source caller replay operations. One capture target per fresh run avoids heavy layer renders shifting later target frames; actual times are recorded. No server power events.";
        public List<State> frames=new List<State>(); public List<Capture> captures=new List<Capture>(); public List<string> failures=new List<string>();
        public float sourceLiveTime; public int randomSeed=10010; public bool sourceRootDestroyed;
    }
    Report report; GameObject effect,bands; Client.AutoDestory destroy;
    ParticleSystem ps; ParticleSystemRenderer particles; MeshFilter mesh; MeshRenderer meshRenderer; Animator animator;
    Camera camera; RenderTexture target; Mesh baked;
    FieldInfo liveField; float live=-1; int replay,captureIndex,createdFrame;
    bool finished;
    // Intentionally include off-120Hz targets and old-burst death boundaries.
    readonly float[] captureTimes={.037f,.091f,.217f,.493f,.719f,.991f,1.001f,1.009f,1.207f,1.219f,1.707f,1.719f,2.997f,3.017f};
    void Start()
    {
        try
        {
            Directory.CreateDirectory(Output); report=new Report{unityVersion=Application.unityVersion};
            QualitySettings.vSyncCount=0; Application.targetFrameRate=120;
            liveField=typeof(Client.AutoDestory).GetField("live",BindingFlags.Instance|BindingFlags.NonPublic);
            camera=new GameObject("Native UI XY camera").AddComponent<Camera>();camera.transform.position=new Vector3(0,0,-1000);
            camera.orthographic=true;camera.orthographicSize=180;camera.nearClipPlane=1;camera.farClipPlane=2000;
            camera.allowHDR=false;camera.allowMSAA=false;camera.clearFlags=CameraClearFlags.SolidColor;
            camera.backgroundColor=new Color(.035f,.035f,.035f,1);target=new RenderTexture(640,360,24,RenderTextureFormat.ARGB32);camera.targetTexture=target;
            baked=new Mesh(); BuildBands(); BeginRun();
        }
        catch(Exception e){Abort(e);}
    }
    void BeginRun()
    {
        var prefab=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/PowerGain/BundleAssets/UI/UIEffect/UI_10010.prefab");
        if(!prefab)throw new Exception("Export source assets before running replay probe");
        effect=Instantiate(prefab);destroy=effect.GetComponent<Client.AutoDestory>();
        if(!destroy||destroy.time!=1)throw new Exception("Original AutoDestory GUID did not resolve with source one-second lifetime");
        ps=effect.GetComponentInChildren<ParticleSystem>();particles=ps.GetComponent<ParticleSystemRenderer>();
        mesh=effect.GetComponentInChildren<MeshFilter>();meshRenderer=mesh.GetComponent<MeshRenderer>();animator=effect.GetComponent<Animator>();
        if(!ps||!mesh||!animator||!animator.enabled)throw new Exception("Missing real source renderer/Animator");
        if(!mesh.sharedMesh||!mesh.sharedMesh.isReadable)throw new Exception("Source model CPU copy unavailable; run isolated importer adaptation before Play Mode");
        ps.useAutoRandomSeed=false;ps.randomSeed=10010;
        if(meshRenderer.sharedMaterial.shader.name!="Mobile/Particles/Additive"||particles.sharedMaterial.shader.name!="Mobile/Particles/Additive")
            throw new Exception("Source additive shader did not resolve");
        live=-1;replay=0;createdFrame=Time.frameCount;
    }
    void BuildBands()
    {
        bands=GameObject.CreatePrimitive(PrimitiveType.Quad);bands.name="Synthetic colored HUD bands";
        bands.transform.position=new Vector3(0,0,10);bands.transform.localScale=new Vector3(640,360,1);
        var texture=new Texture2D(640,360,TextureFormat.RGB24,false);var pixels=new Color[640*360];
        Color[] palette={new Color(.08f,.19f,.27f),new Color(.29f,.16f,.06f),new Color(.12f,.31f,.17f)};
        for(int y=0;y<360;y++)for(int x=0;x<640;x++)pixels[y*640+x]=palette[(y/60+x/160)%3];
        texture.SetPixels(pixels);texture.Apply();var material=new Material(Shader.Find("Unlit/Texture"));material.mainTexture=texture;
        bands.GetComponent<MeshRenderer>().sharedMaterial=material;bands.SetActive(false);
    }
    State Read(string operation)
    {
        var state=new State{run=captureIndex,operation=operation,frame=Time.frameCount,elapsed=Time.time-live,unityTime=Time.time,delta=Time.deltaTime,rootAlive=effect!=null};
        if(effect)
        {
            state.rootLifetime=destroy.time;state.particleTime=ps.time;state.playing=ps.isPlaying;
            state.materialUTiling=meshRenderer.sharedMaterial.mainTextureScale.x;
            var block=new MaterialPropertyBlock();meshRenderer.GetPropertyBlock(block);
            state.hasMaterialPropertyBlock=!block.isEmpty;state.propertyBlockMainTexST=block.GetVector("_MainTex_ST");
            // Animator can override ST per renderer without changing the material
            // asset. Source V tiling is always one, so an absent zero vector is
            // distinguishable without a newer-Unity HasVector API.
            state.uTiling=state.propertyBlockMainTexST.y!=0?state.propertyBlockMainTexST.x:state.materialUTiling;
            state.animatorNormalized=animator.GetCurrentAnimatorStateInfo(0).normalizedTime;
            var values=new ParticleSystem.Particle[ps.particleCount];state.particleCount=ps.GetParticles(values);
            state.remaining=new float[state.particleCount];state.seeds=new uint[state.particleCount];
            for(int i=0;i<state.particleCount;i++){state.remaining[i]=values[i].remainingLifetime;state.seeds[i]=values[i].randomSeed;}
        }
        else {state.remaining=new float[0];state.seeds=new uint[0];}
        return state;
    }
    void LateUpdate()
    {
        if(finished||report==null||report.failures.Count>0)return;
        try
        {
            if(live<0)
            {
                if(Time.frameCount<=createdFrame)return;
                float sourceLive=(float)liveField.GetValue(destroy);
                live=sourceLive;report.sourceLiveTime=live;
            }
            float elapsed=Time.time-live;
            if(report.frames.Count>=12000)throw new Exception("Bounded frame budget exceeded; no unbounded JSON export");
            report.frames.Add(Read("normal LateUpdate"));
            float replayTime=replay==0?.203f:.707f;
            if(replay<2&&elapsed>=replayTime)
            {
                if(!effect)throw new Exception("Source root died before scheduled replay");
                report.frames.Add(Read("before source replay"));
                destroy.time+=1; animator.Play("UI_10010",-1,0);ps.time=0;ps.Play();
                report.frames.Add(Read("after source replay; animator pending real tick"));replay++;
            }
            if(report.captures.Count==captureIndex&&elapsed>=captureTimes[captureIndex])
                CaptureLayers(captureTimes[captureIndex]);
            if(elapsed>=3.08f)
            {
                report.sourceRootDestroyed=!effect;
                if(effect||replay!=2||report.captures.Count!=captureIndex+1)throw new Exception("Incomplete replay/destruction/capture sequence");
                captureIndex++;Save();
                if(captureIndex==captureTimes.Length){finished=true;ProbeRokUnityPowerReplayVisual.Done(true,Output);}
                else BeginRun();
            }
        }
        catch(Exception e){Abort(e);}
    }
    Draw Geometry(Mesh value,Matrix4x4 transform)
    {
        if(!value||!value.isReadable)throw new Exception("Geometry inspection requires an isolated readable mesh; stopping before Unity error spam");
        var positions=value.vertices;for(int i=0;i<positions.Length;i++)positions[i]=transform.MultiplyPoint3x4(positions[i]);
        return new Draw{positions=positions,uv=value.uv,colors=value.colors,indices=value.triangles};
    }
    void CaptureLayers(float requested)
    {
        var capture=new Capture{target=requested,state=Read("native render snapshot")};var names=new List<string>();
        if(effect)
        {
            capture.mesh=Geometry(mesh.sharedMesh,mesh.transform.localToWorldMatrix);
            particles.BakeMesh(baked,camera,true);capture.particle=Geometry(baked,Matrix4x4.Translate(ps.transform.position));
        }
        bool oldMesh=effect&&meshRenderer.enabled,oldParticles=effect&&particles.enabled;
        try
        {
            foreach(bool colored in new[]{false,true})
            {
                bands.SetActive(colored);
                foreach(string layer in new[]{"mesh","particles","combined","background"})
                {
                    if(effect){meshRenderer.enabled=layer=="mesh"||layer=="combined";particles.enabled=layer=="particles"||layer=="combined";}
                    camera.Render();RenderTexture.active=target;
                    var image=new Texture2D(640,360,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,640,360),0,0);image.Apply();
                    string path=Output+"/"+captureIndex.ToString("D2")+"_"+(colored?"colored_":"gray_")+layer+".png";
                    File.WriteAllBytes(path,image.EncodeToPNG());DestroyImmediate(image);names.Add(path);RenderTexture.active=null;
                }
            }
        }
        finally{if(effect){meshRenderer.enabled=oldMesh;particles.enabled=oldParticles;}bands.SetActive(false);}
        capture.images=names.ToArray();report.captures.Add(capture);Save();
    }
    void Save(){File.WriteAllText(Output+"/native_replay_visual.json",JsonUtility.ToJson(report,true));}
    void Abort(Exception error)
    {
        finished=true;
        Debug.LogException(error);if(report==null)report=new Report();report.failures.Add(error.ToString());
        Directory.CreateDirectory(Output);Save();ProbeRokUnityPowerReplayVisual.Done(false,error.Message);
    }
}
