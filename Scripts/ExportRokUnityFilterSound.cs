using System;
using System.IO;
using UnityEditor;
using UnityEngine;

// Run in the isolated Unity 2019 probe project, never the live game project.
public static class ExportRokUnityFilterSound
{
    public static void Run()
    {
        const string root = "D:/ueDemo/Rok";
        if (Path.GetFullPath(Application.dataPath).Replace('\\', '/') !=
            root + "/Saved/UnityBuiltinShaderProbe/Assets")
            throw new InvalidOperationException("Wrong Unity project");
        const string name = "Sound_Ui_CommonSidePage.mp3";
        string source = root + "/ref/client-unity/Assets/BundleAssets/Souds/" + name;
        File.Copy(source, "Assets/" + name, true);
        File.Copy(source + ".meta", "Assets/" + name + ".meta", true);
        AssetDatabase.ImportAsset("Assets/" + name, ImportAssetOptions.ForceSynchronousImport);
        var importer = (AudioImporter)AssetImporter.GetAtPath("Assets/" + name);
        if (!importer.forceToMono)
            throw new InvalidOperationException("Source mono import setting was lost");
        var clip = AssetDatabase.LoadAssetAtPath<AudioClip>("Assets/" + name);
        if (clip == null || clip.channels != 1)
            throw new InvalidOperationException("Mono source clip unavailable");
        clip.LoadAudioData();
        var samples = new float[clip.samples];
        if (!clip.GetData(samples, 0))
            throw new InvalidOperationException("Cannot decode source clip");
        string output = root + "/Saved/RokUnityMigration/Sound_Ui_CommonSidePage_mono.wav";
        using (var writer = new BinaryWriter(File.Create(output)))
        {
            writer.Write(System.Text.Encoding.ASCII.GetBytes("RIFF"));
            writer.Write(36 + samples.Length * 2);
            writer.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt "));
            writer.Write(16); writer.Write((short)1); writer.Write((short)1);
            writer.Write(clip.frequency); writer.Write(clip.frequency * 2);
            writer.Write((short)2); writer.Write((short)16);
            writer.Write(System.Text.Encoding.ASCII.GetBytes("data"));
            writer.Write(samples.Length * 2);
            foreach (float sample in samples)
                writer.Write((short)Mathf.RoundToInt(Mathf.Clamp(sample, -1, 1) * 32767));
        }
        Debug.Log("ROK_FILTER_AUDIO channels=" + clip.channels + " samples=" + clip.samples +
            " frequency=" + clip.frequency + " output=" + output);
    }
}
