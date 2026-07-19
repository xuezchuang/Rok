using System;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class CreateInitialCityLayoutScene
{
    private const string InitialStatePath = @"D:\ueDemo\Rok\Saved\RokUnityMigration\unity_initial_city_state.json";
    private const string ScenePath = "Assets/Scenes/UL_InitialCityLayout.unity";
    private const string CityBuildingPrefabRoot = "Assets/BundleAssets/Scene/City_building/city_building_1";
    private const float RuntimeLodScale = 1.1210888f;
    private const float ContainerYaw = -45.0f;
    private const float CityGridSize = 0.1f;
    private const float CityGridSizeHalf = 0.05f;

    [Serializable]
    private sealed class InitialCityState
    {
        public int building_count;
        public BuildingRecord[] buildings;
    }

    [Serializable]
    private sealed class BuildingRecord
    {
        public int init_id;
        public int type_id;
        public string ue_type;
        public GridRecord grid;
        public VisualRecord visual;
        public LevelConfigRecord level_config;
    }

    [Serializable]
    private sealed class GridRecord
    {
        public int x;
        public int y;
    }

    [Serializable]
    private sealed class VisualRecord
    {
        public string model_id;
        public string sprite_name;
    }

    [Serializable]
    private sealed class LevelConfigRecord
    {
        public int width;
        public int length;
    }

    [MenuItem("UnityLearning/Create Original Initial City Layout Scene")]
    public static void Create()
    {
        var state = LoadInitialState();
        var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);

        RenderSettings.ambientMode = UnityEngine.Rendering.AmbientMode.Flat;
        RenderSettings.ambientLight = new Color(0.6470588f, 0.6470588f, 0.6470588f, 1.0f);

        var root = new GameObject("UL_OriginalInitialCityLayout");
        var container = new GameObject("OriginalRuntimePrefabs_OnlyInitialCity");
        container.transform.SetParent(root.transform, false);
        container.transform.localPosition = Vector3.zero;
        container.transform.localRotation = Quaternion.Euler(0.0f, ContainerYaw, 0.0f);
        container.transform.localScale = Vector3.one;

        var spawned = 0;
        var skipped = 0;
        foreach (var building in state.buildings ?? new BuildingRecord[0])
        {
            var prefabPath = PrefabPathForBuilding(building);
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(prefabPath);
            if (prefab == null)
            {
                skipped++;
                continue;
            }

            var instance = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
            instance.name = string.Format(
                CultureInfo.InvariantCulture,
                "Init_{0:000}_T{1}_{2}_{3}",
                building.init_id,
                building.type_id,
                SafeName(building.ue_type),
                SafeName(ModelId(building)));
            instance.transform.SetParent(container.transform, false);
            instance.transform.localPosition = IsBoundaryBuilding(building.type_id)
                ? Vector3.zero
                : ConvertCityObjTileToLocal(building);
            instance.transform.localRotation = Quaternion.identity;
            instance.transform.localScale = Vector3.one * RuntimeLodScale;
            ConfigureSimplifiedOriginal(instance, building.type_id);
            spawned++;
        }

        CreateCameraAndLight(root.transform);
        Directory.CreateDirectory("Assets/Scenes");
        EditorSceneManager.SaveScene(scene, ScenePath);
        AssetDatabase.Refresh();

        Debug.Log("UnityLearning original initial city layout scene saved: " + ScenePath
            + " spawned=" + spawned
            + " skipped=" + skipped);
        EditorApplication.Exit(spawned > 0 ? 0 : 1);
    }

    private static InitialCityState LoadInitialState()
    {
        if (!File.Exists(InitialStatePath))
        {
            throw new Exception("Missing initial city state: " + InitialStatePath);
        }
        return JsonUtility.FromJson<InitialCityState>(File.ReadAllText(InitialStatePath, Encoding.UTF8));
    }

    private static string PrefabPathForBuilding(BuildingRecord building)
    {
        return CityBuildingPrefabRoot + "/" + ModelId(building) + ".prefab";
    }

    private static string ModelId(BuildingRecord building)
    {
        if (building.visual != null && !string.IsNullOrEmpty(building.visual.model_id))
        {
            return building.visual.model_id;
        }
        if (building.visual != null && !string.IsNullOrEmpty(building.visual.sprite_name))
        {
            return building.visual.sprite_name;
        }
        return "build_unknown";
    }

    private static bool IsBoundaryBuilding(int typeId)
    {
        return typeId == 6 || typeId == 8;
    }

    private static Vector3 ConvertCityObjTileToLocal(BuildingRecord building)
    {
        var width = building.level_config != null ? building.level_config.width : 0;
        var length = building.level_config != null ? building.level_config.length : 0;
        var gridX = building.grid != null ? building.grid.x : 0;
        var gridY = building.grid != null ? building.grid.y : 0;
        return new Vector3(
            gridX * CityGridSize + width * CityGridSizeHalf - CityGridSizeHalf,
            0.01f,
            gridY * CityGridSize + length * CityGridSizeHalf - CityGridSizeHalf
        );
    }

    private static void ConfigureSimplifiedOriginal(GameObject instance, int typeId)
    {
        foreach (var particleSystem in instance.GetComponentsInChildren<ParticleSystem>(true))
        {
            particleSystem.gameObject.SetActive(false);
        }
        foreach (var canvas in instance.GetComponentsInChildren<Canvas>(true))
        {
            canvas.gameObject.SetActive(false);
        }
        foreach (var audioSource in instance.GetComponentsInChildren<AudioSource>(true))
        {
            audioSource.enabled = false;
        }

        if (!IsBoundaryBuilding(typeId))
        {
            return;
        }

        foreach (var spriteRenderer in instance.GetComponentsInChildren<SpriteRenderer>(true))
        {
            spriteRenderer.enabled = false;
        }
        foreach (var renderer in instance.GetComponentsInChildren<MeshRenderer>(true))
        {
            renderer.enabled = IsCityBoundaryOrGround(renderer);
        }
    }

    private static bool IsCityBoundaryOrGround(MeshRenderer renderer)
    {
        var meshFilter = renderer.GetComponent<MeshFilter>();
        var meshPath = meshFilter != null && meshFilter.sharedMesh != null
            ? AssetDatabase.GetAssetPath(meshFilter.sharedMesh)
            : "";
        var stem = Path.GetFileNameWithoutExtension(meshPath);
        return stem.StartsWith("CityWall_101_1", StringComparison.Ordinal)
            || stem.StartsWith("CityGate_101_1", StringComparison.Ordinal)
            || stem.StartsWith("CityTower_101_1", StringComparison.Ordinal)
            || stem.StartsWith("CityWall_Ground_1_1", StringComparison.Ordinal);
    }

    private static void CreateCameraAndLight(Transform root)
    {
        var lightObject = new GameObject("Reference_DirectionalLight");
        lightObject.transform.SetParent(root, false);
        lightObject.transform.rotation = Quaternion.Euler(50.0f, -30.0f, 0.0f);
        var light = lightObject.AddComponent<Light>();
        light.type = LightType.Directional;
        light.color = new Color(0.5450981f, 0.3098039f, 0.1411765f, 1.0f);
        light.intensity = 0.85f;

        var cameraObject = new GameObject("Reference_Camera_16x10");
        cameraObject.transform.SetParent(root, false);
        cameraObject.transform.position = new Vector3(0.0f, 8.0f, -8.0f);
        cameraObject.transform.rotation = Quaternion.Euler(45.0f, 0.0f, 0.0f);
        var camera = cameraObject.AddComponent<Camera>();
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = new Color(0.02f, 0.02f, 0.02f, 1.0f);
        camera.orthographic = true;
        camera.orthographicSize = 5.0f;
        camera.nearClipPlane = 0.01f;
        camera.farClipPlane = 100.0f;
        camera.tag = "MainCamera";
    }

    private static string SafeName(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            return "Unknown";
        }
        var builder = new StringBuilder(value.Length);
        foreach (var c in value)
        {
            builder.Append(char.IsLetterOrDigit(c) || c == '_' || c == '-' ? c : '_');
        }
        return builder.ToString();
    }
}
