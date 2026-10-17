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
public class ProbeRokUnityWoodDeposit : MonoBehaviour
{
    const string Root = "D:/ueDemo/Rok";
    [Serializable] public class Sample { public float dxf, scale; public string[] order, active; }
    [Serializable] public class RunRecord { public int instance; public Sample[] samples; }
    [Serializable] public class Report
    {
        public string unityVersion;
        public string scope = "Observed native Start registration in this isolated run, not a Unity cross-run ordering guarantee. Source geometry and GridCollide/LOD scripts copied unchanged. Lighting/pooling dependencies omitted.";
        public RunRecord[] runs;
    }
    public static void Run()
    {
        if (Application.dataPath.Replace('\\','/') != Root+"/Saved/UnityWoodDepositProbe/Assets") throw new Exception("Wrong probe project");
        if (Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong source engine");
        SessionState.SetString("RokWoodProbeMode", "native");
        EditorApplication.isPlaying = true;
    }
    [InitializeOnLoadMethod] static void Install() { EditorApplication.playModeStateChanged += OnPlay; }
    static void OnPlay(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode && SessionState.GetString("RokWoodProbeMode", "") == "native")
        {
            new GameObject("WoodNativeProbe").AddComponent<ProbeRokUnityWoodDeposit>();
        }
    }
    IEnumerator Start()
    {
        var reports = new List<RunRecord>();
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/build_Rsswood_Big.prefab");
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
                    order=list.Select(i=>i.name).ToArray(), active=list.Where(i=>i.m_is_active).Select(i=>i.name).ToArray() });
            }
            reports.Add(new RunRecord {instance=run, samples=snapshots.ToArray()});
            Destroy(obj);
            yield return null;
        }
        var report = new Report {unityVersion=Application.unityVersion, runs=reports.ToArray()};
        File.WriteAllText(Root+"/Saved/RokUnityMigration/unity_wood_deposit_native_probe.json", JsonUtility.ToJson(report,true));
        Debug.Log("ROK_FOOD_NATIVE_PROBE_COMPLETE");
        EditorApplication.Exit(0);
    }
}
