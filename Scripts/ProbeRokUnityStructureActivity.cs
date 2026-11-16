using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Client;
using UnityEditor;
using UnityEngine;

// Isolated source Start/SetScale activity probe for initially false GridCollideItem flags.
public class ProbeRokUnityStructureActivity : MonoBehaviour
{
    const string Root = "D:/ueDemo/Rok";
    static readonly string[] Models = {"build_BarbarianCitadel_Big", "build_HolyLand1_Big", "build_HolyLand2_Big"};
    [Serializable] public class Sample
    {
        public string model;
        public float dxf, scale;
        public bool gridFlag, groupGameObjectActive, spriteRendererEnabled, spriteGameObjectActive;
    }
    [Serializable] public class Report
    {
        public string unityVersion;
        public Sample[] samples;
        public string scope = "Source prefab instantiated in isolated Unity 2019.4 Play Mode. Real GridCollideItem.Start registration and LevelDetailDeposit.UpdateLod; no GameApp or server.";
    }
    public static void Run()
    {
        if (Application.dataPath.Replace('\\','/') != Root+"/Saved/UnityGoldDepositProbe/Assets" || Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong probe project");
        SessionState.SetString("RokStructureActivityMode", "native");
        EditorApplication.isPlaying = true;
    }
    [InitializeOnLoadMethod] static void Install() { EditorApplication.playModeStateChanged += OnPlay; }
    static void OnPlay(PlayModeStateChange state)
    {
        if (state == PlayModeStateChange.EnteredPlayMode && SessionState.GetString("RokStructureActivityMode", "") == "native")
            new GameObject("StructureActivityProbe").AddComponent<ProbeRokUnityStructureActivity>();
    }
    IEnumerator Start()
    {
        var records = new List<Sample>();
        foreach (var model in Models)
        {
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Source/"+model+".prefab");
            if (!prefab) throw new Exception("Missing source prefab: "+model);
            Common.Distance = 480;
            var obj = Instantiate(prefab);
            obj.transform.localScale = Vector3.one;
            yield return null; yield return null;
            var group = obj.GetComponentInChildren<GridCollideItem>(true);
            var lod = obj.GetComponent<LevelDetailDeposit>();
            var renderer = obj.GetComponentsInChildren<SpriteRenderer>(true).Single(r => r.sprite != null);
            if (!group || !lod) throw new Exception("Missing source behavior: "+model);
            foreach (float dxf in new float[] {480, 900, 2500, 900, 480})
            {
                Common.Previous = Common.Distance;
                Common.Distance = dxf;
                lod.UpdateLod();
                yield return null;
                records.Add(new Sample {model=model, dxf=dxf, scale=lod.m_scale_curve.Evaluate(dxf),
                    gridFlag=group.m_is_active, groupGameObjectActive=group.gameObject.activeInHierarchy,
                    spriteRendererEnabled=renderer.enabled, spriteGameObjectActive=renderer.gameObject.activeInHierarchy});
            }
            Destroy(obj);
            yield return null;
        }
        File.WriteAllText(Root+"/Saved/RokUnityMigration/unity_structure_activity_native_probe.json", JsonUtility.ToJson(new Report {unityVersion=Application.unityVersion, samples=records.ToArray()},true));
        Debug.Log("ROK_STRUCTURE_ACTIVITY_PROBE_COMPLETE");
        EditorApplication.Exit(0);
    }
}
