using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

// Unity 2019.4 source-prefab oracle. Runs only inside the isolated checkpoint project.
public static class ExportRokUnityCheckpointMesh
{
    const string Root = "D:/ueDemo/Rok";
    const string Project = Root + "/Saved/UnityCheckpointMeshProbe";
    const string PrefabPath = "Assets/BundleAssets/Scene/Map_building/map_building/build_Checkpoint_Big.prefab";

    [Serializable] public class MaterialRecord
    {
        public string name, path, shader, shaderPath, mainTexturePath;
        public int renderQueue;
        public string[] keywords;
        public Color color;
        public bool hasSpriteColor;
        public Color spriteColor;
    }
    [Serializable] public class MeshRecord
    {
        public string gameObject, sourceMeshName, sourceMeshGuid, sourceMeshFileId;
        public string sourceFilterGuid, sourceFilterFileId, sourceRendererGuid, sourceRendererFileId;
        public bool activeSelf, activeInHierarchy, rendererEnabled;
        public bool groundLod0Only;
        public int vertexCount, triangleCount, subMeshCount;
        public Vector3[] vertices, normals;
        public Vector2[] uv0;
        public Color[] colors;
        public int[] triangles;
        public MaterialRecord[] materials;
    }
    [Serializable] public class CameraRecord
    {
        public Vector3 position, target;
        public float fieldOfView;
        public int width, height;
    }
    [Serializable] public class Report
    {
        public string schema = "rok.checkpoint_mesh_native.v1";
        public string unityVersion, prefabPath, coordinateSpace, screenshotPath;
        public Color spriteColorGlobal;
        public CameraRecord camera;
        public MeshRecord[] meshes;
    }

    static string Id(UnityEngine.Object obj, out string guid)
    {
        guid = "";
        if (!obj) return "";
        long id;
        if (!AssetDatabase.TryGetGUIDAndLocalFileIdentifier(obj, out guid, out id))
            throw new Exception("Could not resolve source local ID: " + obj.name);
        return id.ToString();
    }

    static string PathOf(UnityEngine.Object obj) { return obj ? AssetDatabase.GetAssetPath(obj) : ""; }

    public static void Run()
    {
        if (Application.dataPath.Replace('\\', '/') != Project + "/Assets")
            throw new Exception("Wrong isolated Unity project");
        if (Application.unityVersion != "2019.4.28f1c1")
            throw new Exception("Wrong Unity version: " + Application.unityVersion);
        var output = Project + "/Output";
        Directory.CreateDirectory(output);
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(PrefabPath);
        if (!prefab) throw new Exception("Missing source checkpoint prefab");
        var root = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
        root.transform.position = Vector3.zero;
        root.transform.rotation = Quaternion.identity;
        root.transform.localScale = Vector3.one;

        // The source FBX importer specifies isReadable=0. Enable readback only in
        // this isolated copy; vertices and triangle topology remain importer-derived.
        var fbxPath = "Assets/BundleAssets/Map/FBX/build_Checkpoint_Big.FBX";
        var importer = AssetImporter.GetAtPath(fbxPath) as ModelImporter;
        if (!importer) throw new Exception("Missing source FBX importer");
        if (!importer.isReadable) { importer.isReadable = true; importer.SaveAndReimport(); }

        var filters = root.GetComponentsInChildren<MeshFilter>(true)
            .Where(f => f.sharedMesh != null && AssetDatabase.GetAssetPath(f.sharedMesh) == fbxPath)
            .OrderBy(f => f.gameObject.name).ToArray();
        if (filters.Length != 4) throw new Exception("Expected four checkpoint FBX meshes; found " + filters.Length + ": " +
            string.Join(", ", filters.Select(f => f.gameObject.name + "/" + f.sharedMesh.name).ToArray()));
        var records = filters.Select(f => {
            var renderer = f.GetComponent<MeshRenderer>();
            if (!renderer) throw new Exception("No MeshRenderer on " + f.name);
            var mesh = f.sharedMesh;
            string meshGuid, filterGuid, rendererGuid;
            var sourceFilter = PrefabUtility.GetCorrespondingObjectFromSource(f);
            var sourceRenderer = PrefabUtility.GetCorrespondingObjectFromSource(renderer);
            var xform = root.transform.worldToLocalMatrix * f.transform.localToWorldMatrix;
            var normalMatrix = xform.inverse.transpose;
            var vertices = mesh.vertices.Select(v => xform.MultiplyPoint3x4(v)).ToArray();
            var normals = mesh.normals.Select(n => normalMatrix.MultiplyVector(n).normalized).ToArray();
            var colors = mesh.colors;
            if (colors.Length == 0) colors = Enumerable.Repeat(Color.white, vertices.Length).ToArray();
            var materials = renderer.sharedMaterials.Select(m => {
                if (!m) throw new Exception("Missing source material on " + f.name);
                var shader = m.shader;
                var mainTex = m.HasProperty("_MainTex") ? m.GetTexture("_MainTex") : null;
                return new MaterialRecord {
                    name=m.name, path=PathOf(m), shader=shader ? shader.name : "",
                    shaderPath=PathOf(shader), mainTexturePath=PathOf(mainTex),
                    renderQueue=m.renderQueue, keywords=m.shaderKeywords,
                    color=m.HasProperty("_Color") ? m.color : Color.white,
                    hasSpriteColor=m.HasProperty("_SpriteColor"),
                    spriteColor=m.HasProperty("_SpriteColor") ? m.GetColor("_SpriteColor") : Color.white
                };
            }).ToArray();
            return new MeshRecord {
                gameObject=f.gameObject.name, sourceMeshName=mesh.name,
                sourceMeshFileId=Id(mesh,out meshGuid), sourceMeshGuid=meshGuid,
                sourceFilterFileId=Id(sourceFilter,out filterGuid), sourceFilterGuid=filterGuid,
                sourceRendererFileId=Id(sourceRenderer,out rendererGuid), sourceRendererGuid=rendererGuid,
                activeSelf=f.gameObject.activeSelf, activeInHierarchy=f.gameObject.activeInHierarchy,
                rendererEnabled=renderer.enabled, groundLod0Only=false,
                vertexCount=vertices.Length,
                triangleCount=mesh.triangles.Length/3, subMeshCount=mesh.subMeshCount,
                vertices=vertices, normals=normals, uv0=mesh.uv, colors=colors,
                triangles=mesh.triangles, materials=materials
            };
        }).ToArray();
        foreach (var r in records)
            if (r.vertices.Length != r.normals.Length || r.vertices.Length != r.uv0.Length || r.triangles.Length != r.triangleCount*3)
                throw new Exception("Incomplete mesh arrays: " + r.gameObject);

        var renderers = root.GetComponentsInChildren<Renderer>(true).Where(r => r.enabled && r.gameObject.activeInHierarchy).ToArray();
        if (renderers.Length == 0) throw new Exception("No active source renderers");
        var bounds = renderers[0].bounds;
        foreach (var renderer in renderers.Skip(1)) bounds.Encapsulate(renderer.bounds);
        var cameraObject = new GameObject("CheckpointSourceCamera");
        var camera = cameraObject.AddComponent<Camera>();
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = new Color(0.21f,0.29f,0.20f,1);
        camera.fieldOfView = 35;
        camera.nearClipPlane = 0.01f;
        camera.farClipPlane = 1000;
        var size = Mathf.Max(bounds.size.x,bounds.size.y,bounds.size.z);
        var target = bounds.center;
        camera.transform.position = target + new Vector3(size*0.8f,size*0.62f,-size*1.5f);
        camera.transform.LookAt(target);
        // Source world-day render color observed in the existing Unity probe.
        var spriteColor = new Color(1.07203974f,1.07203974f,1.07203974f,1);
        Shader.SetGlobalColor("_SpriteColor", spriteColor);
        var rt = new RenderTexture(1024,768,24,RenderTextureFormat.ARGB32);
        camera.targetTexture = rt;
        camera.Render();
        var previous = RenderTexture.active;
        RenderTexture.active = rt;
        var shot = new Texture2D(1024,768,TextureFormat.RGBA32,false);
        shot.ReadPixels(new Rect(0,0,1024,768),0,0);
        shot.Apply();
        File.WriteAllBytes(output+"/checkpoint_mesh_near_native.png",shot.EncodeToPNG());
        RenderTexture.active = previous;
        camera.targetTexture = null;
        UnityEngine.Object.DestroyImmediate(rt);
        UnityEngine.Object.DestroyImmediate(shot);

        var report = new Report {
            unityVersion=Application.unityVersion, prefabPath=PrefabPath,
            coordinateSpace="Unity native prefab root: root position=(0,0,0), rotation=identity, scale=(1,1,1); mesh vertices transformed through child hierarchy",
            screenshotPath="Output/checkpoint_mesh_near_native.png",
            spriteColorGlobal=spriteColor,
            camera=new CameraRecord {position=camera.transform.position,target=target,
                fieldOfView=camera.fieldOfView,width=1024,height=768},
            meshes=records
        };
        File.WriteAllText(output+"/checkpoint_mesh_native.json",JsonUtility.ToJson(report,true));
        Debug.Log("ROK_CHECKPOINT_MESH_EXPORT_COMPLETE meshes="+records.Length+" triangles="+records.Sum(r=>r.triangleCount));
        UnityEngine.Object.DestroyImmediate(root);
        UnityEngine.Object.DestroyImmediate(cameraObject);
    }
}
