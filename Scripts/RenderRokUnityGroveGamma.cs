using System;
using System.IO;
using System.Linq;
using System.Globalization;
using UnityEditor;
using UnityEngine;
public static class RenderRokUnityGroveGamma {
 const string Root="D:/ueDemo/Rok", Output=Root+"/Saved/RokUnityMigration/GroveGamma";
 [Serializable] public class Key {public float dxf,scale,in_weight,out_weight;public string in_slope,out_slope;public int weighted_mode;}
 [Serializable] public class Sample {public string name;public int preset;public float[] spriteColor;}
 [Serializable] public class Input {public Key[] curve;public Sample[] samples;public float dxf,tree_scale;}
 [Serializable] public class Frame {public string name,path;public float[] spriteColor;}
 [Serializable] public class TextureInfo {
  public string assetPath,format,graphicsFormat,filter,wrapU,wrapV,importerCompression,defaultCompression,standaloneCompression;
  public int width,height,mips,importerMaxSize,defaultMaxSize,standaloneMaxSize,importerCompressionQuality;
  public bool importerSRGB,importerMipMap,importerAlphaIsTransparency,standaloneOverridden;
  public string importerAlphaSource;public int gpuAlphaZero,gpuAlphaOpaque,gpuAlphaPartial;
 }
 [Serializable] public class Report {
  public string unityVersion,colorSpace,device,shader,prefab;
  public int vertices,triangles,renderQueue;public float dxf,treeScale,treeRot,fov;
  public Vector3 cameraPosition,cameraEuler;public TextureInfo texture;public Frame[] frames;
  public string scope="Original isolated prefab mesh/material/shader, fixed camera/dxf1509, 3 stable source SpriteColor presets. RGB captures composite over fixed background. Alpha blending/edges and whole-map equivalence remain separate. GPU texture alpha histogram is a one-to-one Blit readback, not source PNG alpha or screenshot coverage.";
 }
 static float Slope(string s){return s=="inf"?float.PositiveInfinity:s=="-inf"?float.NegativeInfinity:float.Parse(s,CultureInfo.InvariantCulture);}
 public static void Run(){try{Render();Debug.Log("ROK_GROVE_GAMMA_NATIVE_COMPLETE");EditorApplication.Exit(0);}catch(Exception e){Directory.CreateDirectory(Output);File.WriteAllText(Output+"/native_failure.txt",e.ToString());Debug.LogException(e);EditorApplication.Exit(1);}}
 static void Render(){
  if(Application.dataPath.Replace('\\','/')!=Root+"/Saved/UnityGroveGammaProbe/Assets"||Application.unityVersion!="2019.4.28f1c1"||QualitySettings.activeColorSpace!=ColorSpace.Gamma)throw new Exception("Wrong GroveGamma native identity");
  var input=JsonUtility.FromJson<Input>(File.ReadAllText(Output+"/manifest.json"));
  var curve=new AnimationCurve(input.curve.Select(k=>new Keyframe(k.dxf,k.scale,Slope(k.in_slope),Slope(k.out_slope),k.in_weight,k.out_weight){weightedMode=(WeightedMode)k.weighted_mode}).ToArray());
  float scale=curve.Evaluate(input.dxf);if(input.samples.Length!=3||input.dxf!=1509||Mathf.Abs(scale-input.tree_scale)>0.000002f)throw new Exception("Fixed source scale/sample contract mismatch");
  const string prefabPath="Assets/BundleAssets/land/Tile/Grove_co_tree_01_adorning.prefab";
  var prefab=AssetDatabase.LoadAssetAtPath<GameObject>(prefabPath);var obj=UnityEngine.Object.Instantiate(prefab);
  var renderer=obj.GetComponentsInChildren<MeshRenderer>().Single();var mesh=renderer.GetComponent<MeshFilter>().sharedMesh;var material=renderer.sharedMaterial;
  if(mesh.vertexCount!=377||mesh.colors.Length!=377||material.shader.name!="custom/landmap/tree-16-nomask"||!material.shader.isSupported)throw new Exception("Source mesh/shader mismatch");
  var texture=(Texture2D)material.GetTexture("_MainTex");string texturePath=AssetDatabase.GetAssetPath(texture);var importer=(TextureImporter)AssetImporter.GetAtPath(texturePath);
  var defaults=importer.GetDefaultPlatformTextureSettings();var standalone=importer.GetPlatformTextureSettings("Standalone");
  var info=new TextureInfo{assetPath=texturePath,format=texture.format.ToString(),graphicsFormat=texture.graphicsFormat.ToString(),width=texture.width,height=texture.height,mips=texture.mipmapCount,filter=texture.filterMode.ToString(),wrapU=texture.wrapModeU.ToString(),wrapV=texture.wrapModeV.ToString(),importerSRGB=importer.sRGBTexture,importerMipMap=importer.mipmapEnabled,importerMaxSize=importer.maxTextureSize,importerCompression=importer.textureCompression.ToString(),importerCompressionQuality=importer.compressionQuality,importerAlphaIsTransparency=importer.alphaIsTransparency,importerAlphaSource=importer.alphaSource.ToString(),defaultMaxSize=defaults.maxTextureSize,defaultCompression=defaults.textureCompression.ToString(),standaloneMaxSize=standalone.maxTextureSize,standaloneCompression=standalone.textureCompression.ToString(),standaloneOverridden=standalone.overridden};
  var copy=RenderTexture.GetTemporary(texture.width,texture.height,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);Graphics.Blit(texture,copy);RenderTexture.active=copy;
  var pixels=new Texture2D(texture.width,texture.height,TextureFormat.RGBA32,false,true);pixels.ReadPixels(new Rect(0,0,texture.width,texture.height),0,0);pixels.Apply();
  foreach(var pixel in pixels.GetPixels32()){if(pixel.a==0)info.gpuAlphaZero++;else if(pixel.a==255)info.gpuAlphaOpaque++;else info.gpuAlphaPartial++;}
  File.WriteAllBytes(Output+"/native_imported_texture.png",pixels.EncodeToPNG());UnityEngine.Object.DestroyImmediate(pixels);RenderTexture.active=null;RenderTexture.ReleaseTemporary(copy);
  Shader.SetGlobalFloat("_TreeScale",scale);
  var camera=new GameObject("SourceGroveGammaCamera").AddComponent<Camera>();camera.allowHDR=false;camera.allowMSAA=false;camera.nearClipPlane=.01f;camera.farClipPlane=1000;camera.aspect=1280f/720;camera.fieldOfView=12;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.13f,.13f,.13f,1);
  camera.transform.rotation=Quaternion.LookRotation(new Vector3(0,-.7f,.7f).normalized,Vector3.up);
  var bounds=renderer.bounds;camera.transform.position=bounds.center-camera.transform.forward*(bounds.extents.magnitude/Mathf.Sin(6*Mathf.Deg2Rad)*1.4f);
  var rt=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);rt.Create();camera.targetTexture=rt;
  var frames=new System.Collections.Generic.List<Frame>();
  foreach(var sample in input.samples){
   var c=sample.spriteColor;Shader.SetGlobalColor("_SpriteColor",new Color(c[0],c[1],c[2],1));camera.Render();RenderTexture.active=rt;
   var image=new Texture2D(1280,720,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,1280,720),0,0);image.Apply();string path=Output+"/native_"+sample.name+".png";File.WriteAllBytes(path,image.EncodeToPNG());UnityEngine.Object.DestroyImmediate(image);RenderTexture.active=null;
   frames.Add(new Frame{name=sample.name,path=path,spriteColor=c});
  }
  var report=new Report{unityVersion=Application.unityVersion,colorSpace=QualitySettings.activeColorSpace.ToString(),device=SystemInfo.graphicsDeviceName,shader=material.shader.name,prefab=prefabPath,vertices=mesh.vertexCount,triangles=mesh.triangles.Length/3,renderQueue=material.renderQueue,dxf=input.dxf,treeScale=scale,treeRot=material.GetFloat("_TreeRot"),fov=camera.fieldOfView,cameraPosition=camera.transform.position,cameraEuler=camera.transform.eulerAngles,texture=info,frames=frames.ToArray()};
  File.WriteAllText(Output+"/native_render.json",JsonUtility.ToJson(report,true));UnityEngine.Object.DestroyImmediate(obj);UnityEngine.Object.DestroyImmediate(camera.gameObject);rt.Release();
 }
}
