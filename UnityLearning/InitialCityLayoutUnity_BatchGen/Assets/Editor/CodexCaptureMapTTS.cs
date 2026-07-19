using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class CodexCaptureMapTTS
{
    private const string ScenePath = "Assets/BundleAssets/Map/Scenes/Map_TTS.unity";
    private const int Width = 1600;
    private const int Height = 900;

    public static void Capture()
    {
        try
        {
            EditorSceneManager.OpenScene(ScenePath, OpenSceneMode.Single);
            var camera = Resources.FindObjectsOfTypeAll<Camera>()
                .FirstOrDefault(item => item.gameObject.scene.IsValid()
                    && item.gameObject.activeInHierarchy
                    && item.enabled
                    && item.gameObject.name == "Main Camera");
            if (camera == null)
            {
                throw new InvalidOperationException("Map_TTS active Main Camera was not found");
            }

            var outputPath = Path.GetFullPath(Path.Combine(
                Application.dataPath,
                "..", "..", "..",
                "Saved", "RokUnityPlacement", "Map_TTS_Unity_GameView_1600x900.png"
            ));
            Directory.CreateDirectory(Path.GetDirectoryName(outputPath));

            var previousTarget = camera.targetTexture;
            var previousAspect = camera.aspect;
            var previousActive = RenderTexture.active;
            var renderTexture = new RenderTexture(Width, Height, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
            renderTexture.Create();
            var image = new Texture2D(Width, Height, TextureFormat.RGBA32, false, false);
            try
            {
                camera.aspect = (float)Width / Height;
                camera.targetTexture = renderTexture;
                camera.Render();
                RenderTexture.active = renderTexture;
                image.ReadPixels(new Rect(0, 0, Width, Height), 0, 0, false);
                image.Apply(false, false);
                File.WriteAllBytes(outputPath, image.EncodeToPNG());
            }
            finally
            {
                camera.targetTexture = previousTarget;
                camera.aspect = previousAspect;
                RenderTexture.active = previousActive;
                UnityEngine.Object.DestroyImmediate(image);
                renderTexture.Release();
                UnityEngine.Object.DestroyImmediate(renderTexture);
            }

            Debug.Log("MapTTS capture saved: " + outputPath);
            EditorApplication.Exit(0);
        }
        catch (Exception exception)
        {
            Debug.LogException(exception);
            EditorApplication.Exit(1);
        }
    }
}
