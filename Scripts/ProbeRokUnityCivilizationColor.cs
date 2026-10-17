using System;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class ProbeRokUnityCivilizationColor
{
    public static void Run()
    {
        const string root = "D:/ueDemo/Rok";
        if (Path.GetFullPath(Application.dataPath).Replace('\\', '/') != root + "/Saved/UnityBuiltinShaderProbe/Assets")
            throw new InvalidOperationException("Wrong Unity probe project");
        Color value;
        bool valid = ColorUtility.TryParseHtmlString("", out value);
        string result = string.Format(System.Globalization.CultureInfo.InvariantCulture,
            "empty_valid={0} rgba={1},{2},{3},{4}", valid, value.r, value.g, value.b, value.a);
        File.WriteAllText(root + "/Saved/RokUnityMigration/civilization_empty_color_probe.txt", result);
        Debug.Log("ROK_CIVILIZATION_COLOR " + result);
    }
}
