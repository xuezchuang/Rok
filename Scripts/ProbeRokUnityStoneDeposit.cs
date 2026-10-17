using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Client;
using UnityEditor;
using UnityEngine;

// Compiled into an isolated Unity 2019.4 project only. GridCollideMgr,
// GridCollideItem, RotateSquar and all three LOD classes are unchanged copies.
public class ProbeRokUnityStoneDeposit : MonoBehaviour
{
    const string Root = "D:/ueDemo/Rok";
    [Serializable] public class Sample { public float dxf, scale; public string[] order, active; }
    [Serializable] public class RunRecord { public int instance; public Sample[] samples; }
    [Serializable] public class Mapping { public string sourceTransformId, sourceName, sourceRendererId; public int siblingIndex; }
    [Serializable] public class Report
    {
        public string unityVersion;
        public string scope = "Observed native Start registration in this isolated run, not a Unity cross-run ordering guarantee. Source geometry and GridCollide/LOD scripts copied unchanged. Lighting/pooling dependencies omitted.";
        public RunRecord[] runs;
        public Mapping[] mappings;
    }
    public static string SourceId(GameObject prefab, GridCollideItem item)
    {
        // Instantiate preserves sibling positions. Resolve the corresponding
        // imported asset Transform, never Find(name): all 18 names are stone_1.
        var source = prefab.transform.GetChild(item.transform.GetSiblingIndex());
        if ((source.localPosition-item.transform.localPosition).sqrMagnitude > 1e-10f)
            throw new Exception("Source sibling mapping position mismatch");
        string guid; long id;
        if (!AssetDatabase.TryGetGUIDAndLocalFileIdentifier(source, out guid, out id)) throw new Exception("Source Transform identity unavailable");
        return id.ToString(System.Globalization.CultureInfo.InvariantCulture);
    }
    public static void Run()
    {
        if (Application.dataPath.Replace('\\','/') != Root+"/Saved/UnityStoneDepositProbe/Assets") throw new Exception("Wrong probe project");
        if (Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong source engine");
        SessionState.SetString("RokStoneProbeMode", "native");
        EditorApplication.isPlaying = true;
    }
    [InitializeOnLoadMethod] static void Install() { EditorApplication.playModeStateChanged += OnPlay; }
    static void OnPlay(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode && SessionState.GetString("RokStoneProbeMode", "") == "native")
        {
            new GameObject("StoneNativeProbe").AddComponent<ProbeRokUnityStoneDeposit>();
        }
    }
    IEnumerator Start()
    {
        var reports = new List<RunRecord>();
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/build_Rssstone_Big.prefab");
        if (!prefab) throw new Exception("Missing exact source prefab");
        for (int run = 0; run < 3; ++run)
        {
            Common.Distance = 480;
            var obj = Instantiate(prefab);
            obj.transform.position = Vector3.zero;
            obj.transform.localScale = Vector3.one;
            yield return null;
            yield return null;
            var mgr = obj.GetComponent<GridCollideMgr>();
            var lod = obj.GetComponent<LevelDetailDeposit>();
            if (!mgr || !lod) throw new Exception("Missing source runtime classes");
            var snapshots = new List<Sample>();
            foreach (float dxf in new float[] {480, 594.4174f, 900, 1500, 2500, 2999, 3000, 1500, 900, 594.4174f, 480})
            {
                Common.Previous = Common.Distance;
                Common.Distance = dxf;
                lod.UpdateLod();
                yield return null;
                var list = (List<GridCollideItem>) typeof(GridCollideMgr).GetField("tileCollision", BindingFlags.NonPublic|BindingFlags.Instance).GetValue(mgr);
                if (list.Count != 18) throw new Exception("Expected eighteen auto-registered source items");
                snapshots.Add(new Sample { dxf=dxf, scale=lod.m_scale_curve.Evaluate(dxf),
                    order=list.Select(i=>SourceId(prefab,i)).ToArray(), active=list.Where(i=>i.m_is_active).Select(i=>SourceId(prefab,i)).ToArray() });
            }
            reports.Add(new RunRecord {instance=run, samples=snapshots.ToArray()});
            Destroy(obj);
            yield return null;
        }
        var mappings = prefab.GetComponentsInChildren<GridCollideItem>(true).Select(i=> {
            string guid; long rendererId;
            var renderer=i.GetComponentInChildren<SpriteRenderer>(true);
            if (!renderer || !AssetDatabase.TryGetGUIDAndLocalFileIdentifier(renderer,out guid,out rendererId)) throw new Exception("Source renderer identity unavailable");
            return new Mapping {sourceTransformId=SourceId(prefab,i), sourceName=i.name, siblingIndex=i.transform.GetSiblingIndex(), sourceRendererId=rendererId.ToString(System.Globalization.CultureInfo.InvariantCulture)};
        }).ToArray();
        var report = new Report {unityVersion=Application.unityVersion, runs=reports.ToArray(),mappings=mappings};
        File.WriteAllText(Root+"/Saved/RokUnityMigration/unity_stone_deposit_native_probe.json", JsonUtility.ToJson(report,true));
        Debug.Log("ROK_STONE_NATIVE_PROBE_COMPLETE");
        EditorApplication.Exit(0);
    }
}
