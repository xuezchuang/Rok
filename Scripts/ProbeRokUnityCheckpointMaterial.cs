using System;
using System.Collections;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

// Isolated Unity 2019.4 inspection of checkpoint sprite shader and activation.
public class ProbeRokUnityCheckpointMaterial : MonoBehaviour
{
    const string Root = "D:/ueDemo/Rok";
    [Serializable] public class RendererState
    {
        public string name, shader, sharedShader, materialName, sourceRendererId;
        public bool enabled, activeInHierarchy;
        public int renderQueue, sortingLayerId, sortingOrder;
        public string[] keywords;
    }
    [Serializable] public class Report
    {
        public string unityVersion;
        public RendererState[] renderers;
        public string scope = "Isolated source prefab instantiated in Play Mode, sampled after two frames; no GameApp, AllianceProxy or StrongHold runtime mutation.";
    }

    public static void Run()
    {
        if (Application.dataPath.Replace('\\','/') != Root+"/Saved/UnityGoldDepositProbe/Assets") throw new Exception("Wrong probe project");
        if (Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong source engine");
        SessionState.SetString("RokCheckpointMaterialMode", "native");
        EditorApplication.isPlaying = true;
    }
    [InitializeOnLoadMethod] static void Install() { EditorApplication.playModeStateChanged += OnPlay; }
    static void OnPlay(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode && SessionState.GetString("RokCheckpointMaterialMode", "") == "native")
            new GameObject("CheckpointMaterialProbe").AddComponent<ProbeRokUnityCheckpointMaterial>();
    }
    IEnumerator Start()
    {
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/build_Checkpoint_Big.prefab");
        if (!prefab) throw new Exception("Missing source checkpoint prefab");
        var obj = Instantiate(prefab);
        obj.transform.localScale = Vector3.one;
        yield return null;
        yield return null;
        var renderers = obj.GetComponentsInChildren<SpriteRenderer>(true)
            .Where(r => r.sprite != null)
            .Select(r => {
                string guid; long sourceId;
                var source = PrefabUtility.GetCorrespondingObjectFromSource(r);
                if (!source || !AssetDatabase.TryGetGUIDAndLocalFileIdentifier(source, out guid, out sourceId))
                    sourceId = 0;
                var material = r.material;
                var shared = r.sharedMaterial;
                return new RendererState {
                    name = r.gameObject.name, shader = material && material.shader ? material.shader.name : "",
                    sharedShader = shared && shared.shader ? shared.shader.name : "",
                    materialName = material ? material.name : "", sourceRendererId = sourceId.ToString(),
                    enabled = r.enabled, activeInHierarchy = r.gameObject.activeInHierarchy,
                    renderQueue = material ? material.renderQueue : -1,
                    sortingLayerId = r.sortingLayerID, sortingOrder = r.sortingOrder,
                    keywords = material ? material.shaderKeywords : new string[0],
                };
            }).ToArray();
        if (renderers.Length != 4) throw new Exception("Expected 4 source checkpoint SpriteRenderers, got " + renderers.Length);
        var report = new Report {unityVersion=Application.unityVersion, renderers=renderers};
        File.WriteAllText(Root+"/Saved/RokUnityMigration/unity_checkpoint_material_native_probe.json", JsonUtility.ToJson(report,true));
        Debug.Log("ROK_CHECKPOINT_MATERIAL_PROBE_COMPLETE");
        EditorApplication.Exit(0);
    }
}
