using System;
using System.IO;
using UnityEditor;
using UnityEngine;

// Source materials are copied into an isolated Unity 2019.4 probe project.
// No changes to the reference Unity project or materials are saved.
public static class ProbeRokUnityPowerEffect
{
    [Serializable] public class MaterialResult
    {
        public string material, shader, shaderGuid, keywords;
        public long shaderFileId;
        public int renderQueue;
    }
    [Serializable] public class Result
    {
        public string unityVersion;
        public MaterialResult[] materials;
    }
    public static void Run()
    {
        const string root = "D:/ueDemo/Rok";
        if (Path.GetFullPath(Application.dataPath).Replace('\\', '/') != root + "/Saved/UnityBuiltinShaderProbe/Assets")
            throw new InvalidOperationException("Wrong Unity probe project");
        string[] names = { "tile003", "glow002" };
        var result = new Result { unityVersion = Application.unityVersion, materials = new MaterialResult[2] };
        for (int i = 0; i < names.Length; i++)
        {
            var material = AssetDatabase.LoadAssetAtPath<Material>("Assets/" + names[i] + ".mat");
            if (material == null || material.shader == null)
                throw new InvalidOperationException("Missing material/shader: " + names[i]);
            string guid;
            long fileId;
            if (!AssetDatabase.TryGetGUIDAndLocalFileIdentifier(material.shader, out guid, out fileId))
                throw new InvalidOperationException("Cannot resolve shader identity");
            result.materials[i] = new MaterialResult {
                material = names[i], shader = material.shader.name, shaderGuid = guid,
                shaderFileId = fileId, renderQueue = material.renderQueue,
                keywords = string.Join(" ", material.shaderKeywords)
            };
        }
        string json = JsonUtility.ToJson(result, true);
        File.WriteAllText(root + "/Saved/RokUnityMigration/power_effect_shader_probe.json", json);
        Debug.Log("ROK_POWER_EFFECT_SHADER " + json);
    }
}
