using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

[InitializeOnLoad]
public static class RepairInitialCityLayoutVisibility
{
    private const string ScenePath = "Assets/Scenes/UL_InitialCityLayout.unity";
    private const string MarkerPath = "Assets/Scenes/UL_InitialCityLayout.visibility_repaired";

    static RepairInitialCityLayoutVisibility()
    {
        EditorApplication.delayCall += RepairOnceAfterReload;
    }

    private static void RepairOnceAfterReload()
    {
        if (File.Exists(MarkerPath) || !File.Exists(ScenePath))
        {
            return;
        }
        Repair();
        File.WriteAllText(MarkerPath, "Visibility repair applied.\n");
        AssetDatabase.Refresh();
    }

    [MenuItem("UnityLearning/Repair Initial City Layout Visibility")]
    public static void Repair()
    {
        var scene = EditorSceneManager.OpenScene(ScenePath);
        var spriteMaterial = new Material(Shader.Find("Sprites/Default"))
        {
            name = "UL_SpritesDefault_Visible"
        };
        var meshMaterialTemplate = new Material(Shader.Find("Standard"))
        {
            name = "UL_Standard_Visible"
        };

        var spriteCount = 0;
        var meshCount = 0;
        foreach (var root in scene.GetRootGameObjects())
        {
            foreach (var spriteRenderer in root.GetComponentsInChildren<SpriteRenderer>(true))
            {
                spriteRenderer.enabled = true;
                spriteRenderer.sharedMaterial = spriteMaterial;
                spriteCount++;
            }

            foreach (var meshRenderer in root.GetComponentsInChildren<MeshRenderer>(true))
            {
                meshRenderer.enabled = true;
                var oldMaterial = meshRenderer.sharedMaterial;
                var fixedMaterial = new Material(meshMaterialTemplate)
                {
                    name = "UL_Visible_" + meshRenderer.gameObject.name
                };
                if (oldMaterial != null && oldMaterial.mainTexture != null)
                {
                    fixedMaterial.mainTexture = oldMaterial.mainTexture;
                }
                meshRenderer.sharedMaterial = fixedMaterial;
                meshCount++;
            }
        }

        EnsureCameraAndLight();
        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene);
        AssetDatabase.Refresh();
        Debug.Log("Repaired initial city layout visibility: sprites=" + spriteCount + " meshes=" + meshCount + " scene=" + ScenePath);
    }

    private static void EnsureCameraAndLight()
    {
        var camera = Object.FindObjectOfType<Camera>();
        if (camera == null)
        {
            var cameraObject = new GameObject("UL_Reference_Camera_16x10");
            cameraObject.transform.position = new Vector3(0.0f, 15.0f, -15.0f);
            cameraObject.transform.rotation = Quaternion.Euler(45.0f, 0.0f, 0.0f);
            camera = cameraObject.AddComponent<Camera>();
            camera.tag = "MainCamera";
        }
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = new Color(0.15f, 0.17f, 0.17f, 1.0f);
        camera.orthographic = true;
        camera.orthographicSize = 8.0f;
        camera.nearClipPlane = 0.01f;
        camera.farClipPlane = 200.0f;

        var light = Object.FindObjectOfType<Light>();
        if (light == null)
        {
            var lightObject = new GameObject("UL_Reference_DirectionalLight");
            lightObject.transform.rotation = Quaternion.Euler(50.0f, -30.0f, 0.0f);
            light = lightObject.AddComponent<Light>();
        }
        light.type = LightType.Directional;
        light.intensity = 1.1f;
    }
}
