using System;
using System.IO;
using UnityEditor;
using UnityEngine;

// Run in an isolated Unity project with the original MP3s and their .meta files.
public static class ExportRokConstructionAudio
{
    public static void Run()
    {
        try
        {
            string output = Path.Combine(Directory.GetParent(Application.dataPath).FullName, "Wav");
            Directory.CreateDirectory(output);
            foreach (string name in new[] { "Sound_Ui_BuildingStartLevelup", "sfx_buildingUp", "Sound_Ui_Speedup" })
            {
                string path = "Assets/Original/" + name + ".mp3";
                var importer = AssetImporter.GetAtPath(path) as AudioImporter;
                var clip = AssetDatabase.LoadAssetAtPath<AudioClip>(path);
                if (importer == null || !importer.forceToMono || clip == null || clip.channels != 1)
                    throw new InvalidOperationException("Unity source did not import as mono: " + name);
                var samples = new float[clip.samples];
                if (!clip.LoadAudioData() || !clip.GetData(samples, 0))
                    throw new InvalidOperationException("Cannot decode " + name);
                WriteWave(Path.Combine(output, name + ".wav"), samples, clip.frequency);
                Debug.Log(name + ": " + clip.samples + " samples at " + clip.frequency + " Hz (mono)");
            }
            EditorApplication.Exit(0);
        }
        catch (Exception exception)
        {
            Debug.LogException(exception);
            EditorApplication.Exit(1);
        }
    }

    static void WriteWave(string path, float[] samples, int rate)
    {
        using (var writer = new BinaryWriter(File.Create(path)))
        {
            writer.Write(new[] { 'R', 'I', 'F', 'F' });
            writer.Write(36 + samples.Length * 2);
            writer.Write(new[] { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
            writer.Write(16);
            writer.Write((short)1);
            writer.Write((short)1);
            writer.Write(rate);
            writer.Write(rate * 2);
            writer.Write((short)2);
            writer.Write((short)16);
            writer.Write(new[] { 'd', 'a', 't', 'a' });
            writer.Write(samples.Length * 2);
            foreach (float sample in samples)
                writer.Write((short)Mathf.Clamp(Mathf.RoundToInt(sample * 32768), short.MinValue, short.MaxValue));
        }
    }
}
