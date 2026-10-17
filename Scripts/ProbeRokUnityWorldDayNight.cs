using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;
using Client;

// These adapters isolate dependencies. The copied source methods/components are
// unmodified. Asset callback delays below are test inputs, not loader measurements.
public static class Common { public static float Distance=480,Previous=480; public static float GetLodDistance(){return Distance;} }
public static class ClientUtils { public static ProbeLodManager lodManager=new ProbeLodManager(); public static LightManager lightingManager {get{return LightManager.Instance();}} }
public class ProbeLodManager { public float GetLodDistance(){return Common.Distance;} public float GetPreviousLodDistance(){return Common.Previous;} public void AddHandler(Action a){} public void RemoveHandler(Action a){} }
public static class GuideProxy { public static bool IsLightingGuideControl=false; }
public enum DayNightSwitch { AUTO=7 }
public class ServerTimeModule { public static ServerTimeModule Instance=new ServerTimeModule(); public long Base; public float Anchor; public long GetServerTime(){return Base+(long)(Time.time-Anchor);} }
public static class CmdConstant { public const string DayNightChange="DayNightChange"; }
public class AppFacade { static AppFacade instance=new AppFacade(); public static AppFacade GetInstance(){return instance;} public void SendNotification(string name,object value){ProbeRokUnityWorldDayNight.Event("phase_notify",name,value.ToString());} }
namespace Skyunion {
 public class ProbeAsset { public Material Value; public UnityEngine.Object asset(){return Value;} }
 public static class CoreUtils { public static ProbeAssets assetService=new ProbeAssets(); }
 public class ProbeAssets {
  public static bool Hold;
  public class Request { public string Name,Owner; public Action Action; public int Frame; }
  public static List<Request> Pending=new List<Request>();
  public void LoadAssetAsync<T>(string name,Action<ProbeAsset> callback,UnityEngine.Object owner) where T:UnityEngine.Object {
   var material=AssetDatabase.LoadAssetAtPath<Material>("Assets/Source/"+name+".mat");
   if(!material)throw new Exception("Missing source material "+name);
   ProbeRokUnityWorldDayNight.Event("asset_request",owner.name,name);
   Pending.Add(new Request{Name=name,Owner=owner.name,Frame=Time.frameCount,Action=()=> {callback(new ProbeAsset{Value=material});ProbeRokUnityWorldDayNight.Event("asset_complete",owner?owner.name:"destroyed",name);}});
  }
  public static void Flush(bool reverse=false) {
   var list=Pending.Where(x=>Time.frameCount>x.Frame).ToList();if(reverse)list.Reverse();
   foreach(var r in list){Pending.Remove(r);r.Action();}
  }
 }
}

public class ProbeRokUnityWorldDayNight:MonoBehaviour {
 const string Root="D:/ueDemo/Rok",Output=Root+"/Saved/RokUnityMigration/WorldDayNightContinuous";
 [Serializable] public class E { public string scenario,kind,owner,value; public int frame; public float time; }
 [Serializable] public class Layer { public string id,material; public bool active,enabled,migratedGridLayer; }
 [Serializable] public class F { public string scenario; public int frame; public float time,dt; public long server; public bool night; public float[] rgb; public Layer[] layers; }
 [Serializable] public class Report { public string scope="Original Unity components/Timer/extracted GEM methods. Server seconds and asset completion are explicit adapters; callback ordering is not Addressables load latency evidence.";public E[] events;public F[] frames; }
 static string Scenario="setup"; static List<E> Events=new List<E>();List<F> Frames=new List<F>(); GameObject Village;SpriteRenderer[] Renderers;bool Recording;float WatchdogStart;static bool Failed;
 public static void Event(string kind,string owner,string value){Events.Add(new E{scenario=Scenario,kind=kind,owner=owner,value=value,frame=Time.frameCount,time=Time.time});}
 public static void Run(){if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityWorldDayNightProbe/Assets")throw new Exception("Wrong isolated project");SessionState.SetBool("RokContinuousNight",true);EditorApplication.isPlaying=true;}
 static void Failure(string message,string stack,LogType type){if(Failed||(type!=LogType.Exception&&type!=LogType.Error&&type!=LogType.Assert))return;Failed=true;Directory.CreateDirectory(Output);File.WriteAllText(Output+"/native_failure.txt",message+"\n"+stack);SessionState.SetBool("RokContinuousNight",false);EditorApplication.Exit(1);}
 [InitializeOnLoadMethod] static void Install(){EditorApplication.playModeStateChanged+=s=>{if(s==PlayModeStateChange.EnteredPlayMode&&SessionState.GetBool("RokContinuousNight",false)){Application.logMessageReceived+=Failure;new GameObject("ContinuousNightProbe").AddComponent<ProbeRokUnityWorldDayNight>();}};}
 void LateUpdate(){if(Time.realtimeSinceStartup-WatchdogStart>90){Failure("Native continuous probe watchdog exceeded 90 seconds","",LogType.Error);return;}if(!Skyunion.ProbeAssets.Hold)Skyunion.ProbeAssets.Flush();if(!Recording)return;var c=Shader.GetGlobalColor("_SpriteColor");Frames.Add(new F{scenario=Scenario,frame=Time.frameCount,time=Time.time,dt=Time.deltaTime,server=ServerTimeModule.Instance.GetServerTime(),night=LightManager.Instance().isNight,rgb=new[]{c.r,c.g,c.b},layers=Renderers.Select(r=>new Layer{id=r.gameObject.name,material=r.sharedMaterial?r.sharedMaterial.name:"null",active=r.gameObject.activeInHierarchy,enabled=r.GetComponent<NightingMask>().enabled,migratedGridLayer=r.name!="6942459152414132847"}).ToArray()});}
 IEnumerator ResetScene(string name,long seconds){
  Recording=false;Skyunion.Timer.CancelAllRegisteredTimers();Skyunion.ProbeAssets.Pending.Clear();Skyunion.ProbeAssets.Hold=false;
  if(Village)Destroy(Village);var prior=FindObjectOfType<LightManager>();if(prior)Destroy(prior.gameObject);yield return null;
  Scenario=name;UnityEngine.Random.InitState(49731);ServerTimeModule.Instance.Base=seconds;ServerTimeModule.Instance.Anchor=Time.time;
  var prefab=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/build_RssVillage_Big.prefab");Village=Instantiate(prefab);
  Renderers=Village.GetComponentsInChildren<SpriteRenderer>(true).Where(r=>r.GetComponent<NightingMask>()!=null).ToArray();
  // The source also has one permanently inactive root sprite outside GridCollide.
  // Preserve it in native evidence; it is not one of the thirteen migrated cards.
  if(Renderers.Length!=14)throw new Exception("Source prefab component inventory changed");
  foreach(var r in Renderers){var path=new List<int>();for(var t=r.transform;t!=Village.transform;t=t.parent)path.Add(t.GetSiblingIndex());path.Reverse();var originalTransform=prefab.transform;foreach(var index in path)originalTransform=originalTransform.GetChild(index);var original=originalTransform.GetComponent<SpriteRenderer>();string guid;long id;if(!original||!AssetDatabase.TryGetGUIDAndLocalFileIdentifier(original,out guid,out id))throw new Exception("Source hierarchy renderer identity unresolved");r.gameObject.name=id.ToString();}
  var extra=Renderers.Where(r=>r.GetComponentInParent<GridCollideItem>()==null).ToArray();
  if(extra.Length!=1||extra[0].name!="6942459152414132847"||extra[0].gameObject.activeSelf)throw new Exception("Inactive extra source renderer identity changed");
  Renderers=Renderers.OrderBy(r=>r==extra[0]?1:0).ToArray();
  var gem=new ProbeSourceDayNight();gem.DayNightInit();gem.DayNightNtf();Recording=true;Event("scenario_start","clock",seconds.ToString());
 }
 IEnumerator Start(){
  WatchdogStart=Time.realtimeSinceStartup;Directory.CreateDirectory(Output);Application.targetFrameRate=60;QualitySettings.vSyncCount=0;new GameObject("SourceLight").AddComponent<Light>().type=LightType.Directional;
  foreach(long seconds in new long[]{1796,2396,56,1798}){yield return ResetScene("cycle_"+seconds,seconds);yield return new WaitForSeconds(8);}
  yield return ResetScene("lifecycle",1801);yield return new WaitForSeconds(1.2f);
  var item=Renderers[0];var component=item.GetComponent<NightingMask>();component.lightOnRandomTime=1;
  component.SetLightOn(false);Event("request_day",item.name,"");yield return new WaitForSeconds(.05f);
  item.gameObject.SetActive(false);Event("gameobject_off",item.name,"");yield return new WaitForSeconds(1.1f);
  item.gameObject.SetActive(true);Event("gameobject_on",item.name,"");yield return new WaitForSeconds(1.1f);
  component.SetLightOn(false);component.enabled=false;Event("component_disabled_pending_day",item.name,"");yield return new WaitForSeconds(1.1f);
  component.enabled=true;Event("component_enabled",item.name,"");yield return new WaitForSeconds(1.1f);
  // Original MaskSprite callbacks accept either completion order. Demonstrate
  // explicit reverse completion rather than assuming a latest-request rule.
  Skyunion.ProbeAssets.Hold=true;component.lightOnRandomTime=0;
  component.SetLightOn(true);component.SetLightOn(false);Event("two_pending_opposites",item.name,"");yield return new WaitForSeconds(.1f);
  Skyunion.ProbeAssets.Flush(true);Skyunion.ProbeAssets.Hold=false;Event("reverse_completion_done",item.name,item.sharedMaterial.name);yield return null;
  Recording=false;File.WriteAllText(Output+"/native_timeline.json",JsonUtility.ToJson(new Report{events=Events.ToArray(),frames=Frames.ToArray()},true));
  SessionState.SetBool("RokContinuousNight",false);Debug.Log("ROK_WORLD_DAY_NIGHT_CONTINUOUS_COMPLETE");EditorApplication.Exit(0);
 }
}
