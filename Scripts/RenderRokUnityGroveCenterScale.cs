using System;
using System.IO;
using System.Linq;
using System.Globalization;
using UnityEditor;
using UnityEngine;
public static class RenderRokUnityGroveCenterScale {
 const string Root="D:/ueDemo/Rok";
 const string Output=Root+"/Saved/RokUnityMigration/GroveCenterScale";
 [Serializable] public class Key {public float dxf,scale,in_weight,out_weight;public string in_slope,out_slope;public int weighted_mode;}
 [Serializable] public class Sample {public string name;public float dxf,scale;}
 [Serializable] public class Input {public Key[] curve;public Sample[] samples;}
 [Serializable] public class Frame {public string name,path;public float dxf,scale;public Vector3[] evaluatedLocalPositions;}
 [Serializable] public class Report {public string unityVersion,shader,colorSpace;public Vector3[] positions;public Vector2[] uv0;public Color[] colors;public int[] triangles;public Frame[] frames;public Vector3 cameraPosition,cameraEuler;public float fov;public string scope="Original mesh COLOR/UV and native AnimationCurve evaluation. Fixed source-direction camera separates tree size from camera zoom. GPU screenshots use original shader; evaluatedLocalPositions are CPU reference formula, not GPU readback. Original prefab and placement hierarchy retained; no full map/server state.";}
 static float Slope(string s){return s=="inf"?float.PositiveInfinity:s=="-inf"?float.NegativeInfinity:float.Parse(s,CultureInfo.InvariantCulture);}
 public static void Run(){try{Render();Debug.Log("ROK_GROVE_CENTER_SCALE_NATIVE_COMPLETE");EditorApplication.Exit(0);}catch(Exception e){Directory.CreateDirectory(Output);File.WriteAllText(Output+"/native_failure.txt",e.ToString());Debug.LogException(e);EditorApplication.Exit(1);}}
 static void Render(){
  if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityGroveCenterScaleProbe/Assets"||Application.unityVersion!="2019.4.28f1c1"||QualitySettings.activeColorSpace!=ColorSpace.Gamma)throw new Exception("Wrong native identity");
  var input=JsonUtility.FromJson<Input>(File.ReadAllText(Output+"/manifest.json"));
  var curve=new AnimationCurve(input.curve.Select(k=>new Keyframe(k.dxf,k.scale,Slope(k.in_slope),Slope(k.out_slope),k.in_weight,k.out_weight){weightedMode=(WeightedMode)k.weighted_mode}).ToArray());
  var prefab=AssetDatabase.LoadAssetAtPath<GameObject>("Assets/BundleAssets/land/Tile/Grove_co_tree_01_adorning.prefab");if(!prefab)throw new Exception("Source prefab missing");
  var obj=UnityEngine.Object.Instantiate(prefab);var renderer=obj.GetComponentsInChildren<MeshRenderer>().Single();var mesh=renderer.GetComponent<MeshFilter>().sharedMesh;
  if(mesh.vertexCount!=377||mesh.colors.Length!=377||renderer.sharedMaterial.shader.name!="custom/landmap/tree-16-nomask"||!renderer.sharedMaterial.shader.isSupported)throw new Exception("Source mesh/shader mismatch");
  var positions=mesh.vertices;var colors=mesh.colors;
  Shader.SetGlobalColor("_SpriteColor",new Color(1.07203974f,1.07203974f,1.07203974f,1));
  var camera=new GameObject("SourceGroveCamera").AddComponent<Camera>();camera.allowHDR=false;camera.allowMSAA=false;camera.nearClipPlane=.01f;camera.farClipPlane=1000;camera.aspect=1280f/720;camera.fieldOfView=12;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.13f,.13f,.13f,1);
  camera.transform.rotation=Quaternion.LookRotation(new Vector3(0,-.7f,.7f).normalized,Vector3.up);
  var bounds=renderer.bounds;camera.transform.position=bounds.center-camera.transform.forward*(bounds.extents.magnitude/Mathf.Sin(6*Mathf.Deg2Rad)*1.4f);
  var rt=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);rt.Create();camera.targetTexture=rt;
  var frames=new System.Collections.Generic.List<Frame>();
  foreach(var sample in input.samples){
   float scale=curve.Evaluate(sample.dxf);Shader.SetGlobalFloat("_TreeScale",scale);camera.Render();RenderTexture.active=rt;
   var image=new Texture2D(1280,720,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,1280,720),0,0);image.Apply();var path=Output+"/native_"+sample.name+".png";File.WriteAllBytes(path,image.EncodeToPNG());UnityEngine.Object.DestroyImmediate(image);RenderTexture.active=null;
   var evaluated=new Vector3[positions.Length];for(int i=0;i<positions.Length;i++){var center=new Vector3(colors[i].r,colors[i].g,colors[i].b)*180;evaluated[i]=center+(positions[i]-center)*scale;}
   frames.Add(new Frame{name=sample.name,path=path,dxf=sample.dxf,scale=scale,evaluatedLocalPositions=evaluated});
  }
  var report=new Report{unityVersion=Application.unityVersion,shader=renderer.sharedMaterial.shader.name,colorSpace=QualitySettings.activeColorSpace.ToString(),positions=positions,colors=colors,uv0=mesh.uv,triangles=mesh.triangles,frames=frames.ToArray(),cameraPosition=camera.transform.position,cameraEuler=camera.transform.eulerAngles,fov=camera.fieldOfView};
  File.WriteAllText(Output+"/native_render.json",JsonUtility.ToJson(report,true));UnityEngine.Object.DestroyImmediate(obj);UnityEngine.Object.DestroyImmediate(camera.gameObject);rt.Release();
 }
}
