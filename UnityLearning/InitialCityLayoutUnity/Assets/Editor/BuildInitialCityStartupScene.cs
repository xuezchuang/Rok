using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

[InitializeOnLoad]
public static class BuildInitialCityStartupScene
{
    private const string MarkerPath = "Assets/Scenes/UL_InitialCityLayout.startup_built_v12";
    private const string ScenePath = "Assets/Scenes/UL_InitialCityLayout.unity";
    private const string SavedRoot = "D:/ueDemo/Rok/Saved/RokUnityMigration";
    private const string SavedAssetRoot = "Assets/RokUnityMigration";
    private const string LiveUnityAssetsRoot = "D:/ueDemo/Rok-unity/client-unity/Assets";
    private const string RefUnityAssetsRoot = "D:/ueDemo/Rok/ref/client-unity/Assets";
    private const string SceneAssetRoot = "Assets";
    private const string RuntimeManifestPath = "D:/ueDemo/Rok/Saved/RokUnityMigration/unity_runtime_city_all_buildings_scene_manifest_GameAppRuntimeV6Color.json";
    private const string InitialCityStatePath = "D:/ueDemo/Rok/Saved/RokUnityMigration/unity_initial_city_state.json";
    private const string GeneratedTextureRoot = "Assets/RokUnityMigration/UnityLearningGenerated";
    private const bool ShowExtraInteriorBuildings = false;
    private const bool ShowDecorationBuildings = false;
    private const bool ShowBuildingMaskOverlays = false;
    private const string SurroundingTerrainTexturePath = "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeGroundObj/01_ground_2_CityTranslucent_101_1_BakedGround.png";
    private const string RefCityWallPrefabPath = "Assets/BundleAssets/Scene/City_building/city_building_1/build_6_101_1.prefab";
    private const string RefGuardTowerPrefabPath = "Assets/BundleAssets/Scene/City_building/city_building_1/build_8_101_1.prefab";
    private const string RefCityWallMaterialPath = "Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityWall_101_1_mat.mat";
    private const string RefCityGateMaterialPath = "Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityGate_101_1_mat.mat";
    private const string RefCityTower101_1MaterialPath = "Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_1_mat.mat";
    private const string RefCityTower101_2MaterialPath = "Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_2.mat";
    private const float RuntimeLodScale = 1.121089f;
    private const float RuntimeUnityToLearningScale = 5.0f;
    private const int RuntimeGroundRenderQueue = 2990;
    private const int RuntimeBuildingRenderQueue = 2996;

    private struct SourceEntry
    {
        public string Kind;
        public string Name;
        public string ObjPath;
        public string TexturePath;
        public SourceEntry(string kind, string name, string objPath, string texturePath)
        {
            Kind = kind;
            Name = name;
            ObjPath = objPath;
            TexturePath = texturePath;
        }
    }

    private struct InitialBuildingInfo
    {
        public int InitId;
        public int BuildingType;
        public int GridX;
        public int GridY;
        public int Width;
        public int Length;
    }

    private static readonly SourceEntry[] Entries = new SourceEntry[]
    {
        new SourceEntry("Ground", "UnityRuntimeCityAllBuildings_ground_1_0", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeGroundObj/00_ground_1_CitySurface_101_1.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeGroundObj/00_ground_1_CitySurface_101_1_BakedGround.png"),
        new SourceEntry("Ground", "UnityRuntimeCityAllBuildings_ground_2_1", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeGroundObj/01_ground_2_CityTranslucent_101_1.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeGroundObj/01_ground_2_CityTranslucent_101_1_BakedGround.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_6_101_1_CityWall_1_0", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/00_CityWall_1_CityWall_101_1.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityWall_101_1_mat.PNG"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityGate_1_1", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/01_CityGate_1_CityGate_101_1.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityGate_101_1_mat.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_1_2", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/02_CityTower_1_1_CityTower_101_1_1.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_1_mat.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_2_3", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/03_CityTower_1_2_CityTower_101_1_2.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_1_mat.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_3_4", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/04_CityTower_1_3_CityTower_101_1_3.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_1_mat.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_4_5", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/05_CityTower_1_4_CityTower_101_1_4.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_2.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_4_1__6", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/06_CityTower_1_4__1__CityTower_101_1_4.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_2.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_4_2__7", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/07_CityTower_1_4__2__CityTower_101_1_4.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_2.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_5_8", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/08_CityTower_1_5_CityTower_101_1_5.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_1_mat.png"),
        new SourceEntry("Boundary", "UnityRuntimeCityAllBuildings_build_8_101_1_CityTower_1_7_9", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeBoundaryObj/09_CityTower_1_7_CityTower_101_1_7.obj", "D:/ueDemo/Rok-unity/client-unity/Assets/BundleAssets/Map/Materials/shared_mapmaterials/CityTower_101_1_mat.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_024_build_53_101_1_0", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/21_Init_024_build_53_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/21_Init_024_build_53_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_022_build_53_101_1_1", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/19_Init_022_build_53_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/19_Init_022_build_53_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_011_build_51_101_1_2", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/08_Init_011_build_51_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/08_Init_011_build_51_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_016_build_52_101_1_3", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/13_Init_016_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/13_Init_016_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_017_build_52_101_1_4", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/14_Init_017_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/14_Init_017_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_018_build_52_101_1_5", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/15_Init_018_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/15_Init_018_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_006_build_9_101_1_6", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/04_Init_006_build_9_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/04_Init_006_build_9_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_010_build_51_101_1_7", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/07_Init_010_build_51_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/07_Init_010_build_51_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_004_build_14_101_1_8", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/02_Init_004_build_14_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/02_Init_004_build_14_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_008_build_29_101_1_9", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/05_Init_008_build_29_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/05_Init_008_build_29_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_012_build_51_101_1_10", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/09_Init_012_build_51_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/09_Init_012_build_51_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_026_build_53_101_1_11", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/23_Init_026_build_53_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/23_Init_026_build_53_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_025_build_53_101_1_12", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/22_Init_025_build_53_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/22_Init_025_build_53_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_002_build_7_101_1_13", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/00_Init_002_build_7_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/00_Init_002_build_7_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_005_build_1_101_1_14", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/03_Init_005_build_1_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/03_Init_005_build_1_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_019_build_52_101_1_15", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/16_Init_019_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/16_Init_019_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_003_build_15_101_1_16", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/01_Init_003_build_15_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/01_Init_003_build_15_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_020_build_53_101_1_17", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/17_Init_020_build_53_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/17_Init_020_build_53_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_015_build_52_101_1_18", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/12_Init_015_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/12_Init_015_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_021_build_52_101_1_19", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/18_Init_021_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/18_Init_021_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_014_build_52_101_1_20", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/11_Init_014_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/11_Init_014_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_013_build_52_101_1_21", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/10_Init_013_build_52_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/10_Init_013_build_52_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_023_build_53_101_1_22", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/20_Init_023_build_53_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/20_Init_023_build_53_101_1_sprite_BakedSprite.png"),
        new SourceEntry("Building", "UnityRuntimeCityAllBuildings_Init_009_build_51_101_1_23", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/06_Init_009_build_51_101_1_sprite.obj", "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/06_Init_009_build_51_101_1_sprite_BakedSprite.png"),
    };

    static BuildInitialCityStartupScene()
    {
        EditorApplication.delayCall += AutoBuildOnce;
    }

    [MenuItem("UnityLearning/Rebuild Initial City From Runtime Export")]
    public static void RebuildFromMenu()
    {
        BuildScene(true);
    }

    private static void AutoBuildOnce()
    {
        if (File.Exists(MarkerPath)) return;
        BuildScene(false);
    }

    private static void BuildScene(bool force)
    {
        try
        {
            AssetDatabase.Refresh();
            Scene scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            scene.name = "UL_InitialCityLayout";

            GameObject root = new GameObject("UL_InitialCityLayout_SourceFrom_GameApp_InitialCity");
            GameObject source = Child(root.transform, "SourceFrom_Rok_ref_client_unity_GameApp_RuntimeInitialCity");
            GameObject clientApp = Child(root.transform, "ClientApp_reference_root_only");
            GameObject sceneObject = Child(root.transform, "SceneObject");
            GameObject landRoot = Child(sceneObject.transform, "land_root");
            GameObject groundRoot = Child(landRoot.transform, "RuntimeGroundPreview_From_UnityRuntimeBakedTerrain");
            GameObject groundSourceRoot = Child(landRoot.transform, "RuntimeGroundObj_SourceMeshes_HiddenForAudit");
            GameObject boundaryRoot = Child(landRoot.transform, "RuntimeBoundary_From_RefUnityPrefabs");
            boundaryRoot.transform.localScale = new Vector3(-RuntimeUnityToLearningScale, RuntimeUnityToLearningScale, RuntimeUnityToLearningScale);
            GameObject buildingRoot = Child(landRoot.transform, "RuntimeInitBuildings_From_InitBuilding_24VisibleSprites");
            GameObject extraBuildingRoot = Child(landRoot.transform, "RuntimeExtraInterior_From_GameAppRuntimeV6Color_24Sprites");
            GameObject overlayRoot = Child(landRoot.transform, "RuntimeBuildingShadowGroundPatches_From_MaskLayers");
            clientApp.SetActive(true);
            source.SetActive(true);
            Dictionary<int, InitialBuildingInfo> initialBuildings = LoadInitialBuildingInfoByInitId();

            bool hasCameraBounds = false;
            Bounds cameraBounds = new Bounds(Vector3.zero, Vector3.zero);
            int groundCount = 0;
            int boundaryCount = 0;
            int buildingCount = 0;
            foreach (SourceEntry entry in Entries)
            {
                if (entry.Kind == "Building" && !ShouldShowBuildingType(BuildingTypeFromName(entry.Name)))
                {
                    continue;
                }
                if (entry.Kind == "Boundary")
                {
                    continue;
                }
                Transform parent = entry.Kind == "Ground" ? groundSourceRoot.transform : (entry.Kind == "Boundary" ? boundaryRoot.transform : buildingRoot.transform);
                GameObject instance = SpawnObj(entry, parent);
                if (instance == null) continue;
                if (entry.Kind == "Ground")
                {
                    instance.SetActive(false);
                }
                else
                {
                    if (entry.Kind == "Building")
                    {
                        ConfigurePlaceableBuilding(instance, entry, initialBuildings);
                    }
                    EncapsulateRenderers(instance, ref hasCameraBounds, ref cameraBounds);
                }
                if (entry.Kind == "Ground") groundCount++;
                else if (entry.Kind == "Boundary") boundaryCount++;
                else buildingCount++;
            }
            boundaryCount = SpawnRefUnityBoundaryPrefabs(boundaryRoot.transform, ref hasCameraBounds, ref cameraBounds);
            SpawnSurroundingTerrainPlate(groundRoot.transform, ref hasCameraBounds, ref cameraBounds);
            int extraBuildingCount = ShowExtraInteriorBuildings ? SpawnManifestSpriteCards("extra_interior_building_actors", extraBuildingRoot.transform, ref hasCameraBounds, ref cameraBounds) : 0;
            int overlayCount = ShowBuildingMaskOverlays
                ? SpawnManifestOverlays("building_actors", overlayRoot.transform)
                    + (ShowExtraInteriorBuildings ? SpawnManifestOverlays("extra_interior_building_actors", overlayRoot.transform) : 0)
                : 0;

            Camera sceneCamera = CreateCamera(root.transform, hasCameraBounds ? cameraBounds : new Bounds(Vector3.zero, new Vector3(70.0f, 1.0f, 70.0f)));
            CreatePlacementController(sceneObject.transform, buildingRoot.transform, sceneCamera);
            CreateLight(root.transform);
            CreateReadme(root.transform, groundCount, boundaryCount, buildingCount, extraBuildingCount, overlayCount);

            EditorSceneManager.SaveScene(scene, ScenePath);
            File.WriteAllText(MarkerPath, "Generated from Saved/RokUnityMigration runtime city exports plus supplemental interior sprites and mask-derived overlays. Do not hand-place. Use UnityLearning/Rebuild Initial City From Runtime Export to regenerate.\n");
            AssetDatabase.Refresh();
            Debug.Log("Built UnityLearning initial city runtime scene: ground=" + groundCount + " boundary=" + boundaryCount + " buildings=" + buildingCount + " extra=" + extraBuildingCount + " overlays=" + overlayCount + " scene=" + ScenePath);
        }
        catch (Exception ex)
        {
            Debug.LogError("BuildInitialCityStartupScene failed: " + ex);
        }
    }

    private static GameObject SpawnObj(SourceEntry entry, Transform parent)
    {
        string objAssetPath = ToAssetPath(entry.ObjPath);
        if (string.IsNullOrEmpty(objAssetPath))
        {
            Debug.LogWarning("Skip source outside known asset roots: " + entry.ObjPath);
            return null;
        }
        AssetDatabase.ImportAsset(objAssetPath, ImportAssetOptions.ForceUpdate);
        GameObject prefab = AssetDatabase.LoadAssetAtPath<GameObject>(objAssetPath);
        if (prefab == null)
        {
            Debug.LogWarning("Missing imported OBJ asset: " + objAssetPath);
            return null;
        }

        GameObject instance = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
        if (instance == null) instance = UnityEngine.Object.Instantiate(prefab);
        instance.name = entry.Name;
        instance.transform.SetParent(parent, false);
        instance.transform.localPosition = Vector3.zero;
        instance.transform.localRotation = Quaternion.identity;
        instance.transform.localScale = Vector3.one;
        ConvertImportedObjMeshesFromCentimetersToMeters(instance);

        Material mat = CreateMaterial(entry);
            if (mat != null)
            {
                foreach (Renderer renderer in instance.GetComponentsInChildren<Renderer>(true))
                {
                    renderer.sharedMaterial = mat;
                    ConfigureRenderer(renderer, entry.Kind, instance.transform.position);
                }
            }
            return instance;
        }

    private static void ConvertImportedObjMeshesFromCentimetersToMeters(GameObject instance)
    {
        foreach (MeshFilter meshFilter in instance.GetComponentsInChildren<MeshFilter>(true))
        {
            Mesh source = meshFilter.sharedMesh;
            if (source == null)
            {
                continue;
            }

            Mesh mesh = UnityEngine.Object.Instantiate(source);
            mesh.name = source.name + "_UnityLearningUeCmToUnityMeters";

            Vector3[] vertices = mesh.vertices;
            for (int i = 0; i < vertices.Length; i++)
            {
                Vector3 v = vertices[i];
                vertices[i] = new Vector3(v.x, v.z, v.y) * 0.01f;
            }
            mesh.vertices = vertices;

            Vector3[] normals = mesh.normals;
            if (normals != null && normals.Length == vertices.Length)
            {
                for (int i = 0; i < normals.Length; i++)
                {
                    Vector3 n = normals[i];
                    normals[i] = new Vector3(n.x, n.z, n.y).normalized;
                }
                mesh.normals = normals;
            }
            else
            {
                mesh.RecalculateNormals();
            }

            mesh.RecalculateBounds();
            meshFilter.sharedMesh = mesh;
        }
    }

    private static void ConfigurePlaceableBuilding(GameObject instance, SourceEntry entry, Dictionary<int, InitialBuildingInfo> initialBuildings)
    {
        int initId = InitIdFromName(entry.Name);
        int buildingType = BuildingTypeFromName(entry.Name);
        InitialBuildingInfo info;
        if (!initialBuildings.TryGetValue(initId, out info))
        {
            info = FallbackInitialBuildingInfo(initId, buildingType);
        }

        bool movable = info.Width > 0 && info.Length > 0 && buildingType != 6 && buildingType != 8 && buildingType != 51 && buildingType != 52 && buildingType != 53;
        if (!movable)
        {
            return;
        }

        UnityLearningCityPlaceable placeable = instance.AddComponent<UnityLearningCityPlaceable>();
        Vector3 referenceGridCenterLocal = ReferenceGridCenterLocal(info.GridX, info.GridY, info.Width, info.Length);
        NormalizeVisualMeshesAroundRoot(instance);
        instance.transform.localPosition = referenceGridCenterLocal;
        placeable.Configure(initId, buildingType, info.GridX, info.GridY, info.Width, info.Length, 0.1f * RuntimeUnityToLearningScale, true);
        placeable.SetGridBasis(Vector3.zero, ReferenceGridAxisXLocal(), ReferenceGridAxisYLocal());
        placeable.RefreshSortingOrder();
        AddSelectionCollider(instance);
        instance.SetActive(false);
    }

    private static Vector3 ReferenceGridCenterLocal(int gridX, int gridY, int width, int length)
    {
        float cellSize = 0.1f * RuntimeUnityToLearningScale;
        float x = gridX * cellSize + width * cellSize * 0.5f - cellSize * 0.5f;
        float z = gridY * cellSize + length * cellSize * 0.5f - cellSize * 0.5f;
        Vector3 planar = ReferenceGridAxisXLocal() * x + ReferenceGridAxisYLocal() * z;
        return new Vector3(planar.x, 0.01f * RuntimeUnityToLearningScale, planar.z);
    }

    private static Vector3 ReferenceGridAxisXLocal()
    {
        return (Quaternion.AngleAxis(-45.0f, Vector3.up) * Vector3.right).normalized;
    }

    private static Vector3 ReferenceGridAxisYLocal()
    {
        return (Quaternion.AngleAxis(-45.0f, Vector3.up) * Vector3.forward).normalized;
    }

    private static void NormalizeVisualMeshesAroundRoot(GameObject instance)
    {
        Bounds localBounds;
        if (!TryGetInstanceLocalMeshBounds(instance, out localBounds))
        {
            return;
        }

        Vector3 rootOffsetLocal = new Vector3(localBounds.center.x, 0.0f, localBounds.center.z);
        Vector3 rootOffsetWorld = instance.transform.TransformVector(rootOffsetLocal);
        foreach (MeshFilter meshFilter in instance.GetComponentsInChildren<MeshFilter>(true))
        {
            Mesh source = meshFilter.sharedMesh;
            if (source == null)
            {
                continue;
            }

            Mesh mesh = UnityEngine.Object.Instantiate(source);
            mesh.name = source.name + "_VisualRootNormalized";
            Vector3 localOffset = meshFilter.transform.InverseTransformVector(rootOffsetWorld);
            Vector3[] vertices = mesh.vertices;
            for (int i = 0; i < vertices.Length; i++)
            {
                vertices[i] -= localOffset;
            }
            mesh.vertices = vertices;
            mesh.RecalculateBounds();
            meshFilter.sharedMesh = mesh;
        }
    }

    private static bool TryGetInstanceLocalMeshBounds(GameObject instance, out Bounds bounds)
    {
        bounds = new Bounds(Vector3.zero, Vector3.zero);
        bool hasBounds = false;
        foreach (MeshFilter meshFilter in instance.GetComponentsInChildren<MeshFilter>(true))
        {
            Mesh mesh = meshFilter.sharedMesh;
            if (mesh == null)
            {
                continue;
            }

            Bounds meshBounds = mesh.bounds;
            Vector3 min = meshBounds.min;
            Vector3 max = meshBounds.max;
            for (int ix = 0; ix < 2; ix++)
            {
                for (int iy = 0; iy < 2; iy++)
                {
                    for (int iz = 0; iz < 2; iz++)
                    {
                        Vector3 meshCorner = new Vector3(
                            ix == 0 ? min.x : max.x,
                            iy == 0 ? min.y : max.y,
                            iz == 0 ? min.z : max.z);
                        Vector3 localCorner = instance.transform.InverseTransformPoint(meshFilter.transform.TransformPoint(meshCorner));
                        if (!hasBounds)
                        {
                            bounds = new Bounds(localCorner, Vector3.zero);
                            hasBounds = true;
                        }
                        else
                        {
                            bounds.Encapsulate(localCorner);
                        }
                    }
                }
            }
        }

        return hasBounds;
    }

    private static void AddSelectionCollider(GameObject instance)
    {
        Bounds bounds;
        if (!TryGetInstanceLocalMeshBounds(instance, out bounds))
        {
            return;
        }

        BoxCollider collider = instance.GetComponent<BoxCollider>();
        if (collider == null)
        {
            collider = instance.AddComponent<BoxCollider>();
        }

        collider.center = bounds.center;
        collider.size = new Vector3(Mathf.Abs(bounds.size.x), Mathf.Max(2.0f, Mathf.Abs(bounds.size.y)), Mathf.Abs(bounds.size.z));
    }

    private static void CreatePlacementController(Transform sceneRoot, Transform buildingRoot, Camera sceneCamera)
    {
        UnityLearningCityPlacementController controller = sceneRoot.gameObject.AddComponent<UnityLearningCityPlacementController>();
        controller.sceneCamera = sceneCamera;
        controller.buildingRoot = buildingRoot;
        controller.cellSize = 0.1f * RuntimeUnityToLearningScale;
        controller.cityMinGrid = -24;
        controller.cityMaxGrid = 24;
        controller.gridYawDegrees = -45.0f;
        controller.dragThresholdPixels = 8.0f;
        controller.stagedPlacementMode = true;
        controller.stagedNextButtonLabel = "\u4E0B\u4E00\u6B65\u5EFA\u7B51";
        controller.EnsureFeedbackForVerification();
    }

    private static int SpawnRefUnityBoundaryPrefabs(Transform parent, ref bool hasCameraBounds, ref Bounds cameraBounds)
    {
        int count = 0;
        count += SpawnRefUnityBoundaryPrefab(parent, RefCityWallPrefabPath, "UnityRefRuntimeBoundary_build_6_101_1_CityWall", ref hasCameraBounds, ref cameraBounds);
        count += SpawnRefUnityBoundaryPrefab(parent, RefGuardTowerPrefabPath, "UnityRefRuntimeBoundary_build_8_101_1_GuardTower", ref hasCameraBounds, ref cameraBounds);
        return count;
    }

    private static int SpawnRefUnityBoundaryPrefab(Transform parent, string assetPath, string name, ref bool hasCameraBounds, ref Bounds cameraBounds)
    {
        AssetDatabase.ImportAsset(assetPath, ImportAssetOptions.ForceUpdate);
        GameObject prefab = AssetDatabase.LoadAssetAtPath<GameObject>(assetPath);
        if (prefab == null)
        {
            Debug.LogWarning("Missing ref Unity boundary prefab: " + assetPath);
            return 0;
        }

        GameObject instance = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
        if (instance == null) instance = UnityEngine.Object.Instantiate(prefab);
        Vector3 sourceLocalPosition = instance.transform.localPosition;
        Quaternion sourceLocalRotation = instance.transform.localRotation;
        Vector3 sourceLocalScale = instance.transform.localScale;
        instance.name = name;
        instance.transform.SetParent(parent, false);
        instance.transform.localPosition = sourceLocalPosition;
        instance.transform.localRotation = sourceLocalRotation;
        instance.transform.localScale = sourceLocalScale * RuntimeLodScale;
        PrefabUtility.UnpackPrefabInstance(instance, PrefabUnpackMode.Completely, InteractionMode.AutomatedAction);
        RemoveMissingScriptsRecursive(instance);

        foreach (Renderer renderer in instance.GetComponentsInChildren<Renderer>(true))
        {
            Material material = ResolveRefUnityBoundaryMaterial(renderer);
            if (material == null)
            {
                renderer.enabled = false;
                continue;
            }
            else
            {
                AssignMaterialToAllSlots(renderer, material);
            }
            ConfigureRenderer(renderer, "Boundary", renderer.transform.position);
        }
        foreach (Collider collider in instance.GetComponentsInChildren<Collider>(true))
        {
            UnityEngine.Object.DestroyImmediate(collider);
        }

        EncapsulateRenderers(instance, ref hasCameraBounds, ref cameraBounds);
        return 1;
    }

    private static void AssignMaterialToAllSlots(Renderer renderer, Material material)
    {
        Material[] materials = renderer.sharedMaterials;
        if (materials == null || materials.Length == 0)
        {
            renderer.sharedMaterial = material;
            return;
        }
        for (int i = 0; i < materials.Length; i++)
        {
            materials[i] = material;
        }
        renderer.sharedMaterials = materials;
    }

    private static Material ResolveRefUnityBoundaryMaterial(Renderer renderer)
    {
        MeshFilter meshFilter = renderer.GetComponent<MeshFilter>();
        if (meshFilter == null || meshFilter.sharedMesh == null)
        {
            return null;
        }

        string meshName = meshFilter.sharedMesh.name ?? "";
        string matchText = renderer.name + "/" + meshName;
        if (matchText.IndexOf("CityWall", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return CreateUnityLearningBoundaryMaterial(RefCityWallMaterialPath, "MAT_UnityLearning_CityWall_101_1");
        }
        if (matchText.IndexOf("CityGate", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return CreateUnityLearningBoundaryMaterial(RefCityGateMaterialPath, "MAT_UnityLearning_CityGate_101_1");
        }
        if (matchText.IndexOf("CityTower_1_4", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return CreateUnityLearningBoundaryMaterial(RefCityTower101_2MaterialPath, "MAT_UnityLearning_CityTower_101_2");
        }
        if (matchText.IndexOf("CityTower", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return CreateUnityLearningBoundaryMaterial(RefCityTower101_1MaterialPath, "MAT_UnityLearning_CityTower_101_1");
        }
        return null;
    }

    private static void RemoveMissingScriptsRecursive(GameObject root)
    {
        foreach (Transform transform in root.GetComponentsInChildren<Transform>(true))
        {
            GameObjectUtility.RemoveMonoBehavioursWithMissingScript(transform.gameObject);
        }
    }

    private static Material CreateUnityLearningBoundaryMaterial(string sourceMaterialPath, string name)
    {
        Material source = AssetDatabase.LoadAssetAtPath<Material>(sourceMaterialPath);
        Texture texture = source != null ? source.GetTexture("_MainTex") : null;
        string texturePath = texture != null ? AssetDatabase.GetAssetPath(texture) : null;
        if (!string.IsNullOrEmpty(texturePath))
        {
            ConfigureTextureImporter(texturePath, true);
            AssetDatabase.ImportAsset(texturePath, ImportAssetOptions.ForceUpdate);
            texture = AssetDatabase.LoadAssetAtPath<Texture2D>(texturePath);
        }

        Shader shader = Shader.Find("Unlit/Transparent Cutout");
        if (shader == null) shader = Shader.Find("Unlit/Transparent");
        if (shader == null) shader = Shader.Find("Unlit/Texture");
        Material material = new Material(shader);
        material.name = name;
        material.mainTexture = texture;
        material.color = Color.white;
        material.renderQueue = 2994;
        if (material.HasProperty("_Cutoff")) material.SetFloat("_Cutoff", 0.02f);
        if (material.HasProperty("_BlackCutoff") && sourceMaterialPath.IndexOf("CityGate", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            material.SetFloat("_BlackCutoff", 0.06f);
        }
        if (material.HasProperty("_ZWrite")) material.SetFloat("_ZWrite", 1.0f);
        return material;
    }

    private static string HierarchyPath(Transform transform)
    {
        string path = transform.name;
        Transform cursor = transform.parent;
        while (cursor != null)
        {
            path = cursor.name + "/" + path;
            cursor = cursor.parent;
        }
        return path;
    }

    private static Material CreateMaterial(SourceEntry entry)
    {
        Shader shader = entry.Kind == "Boundary"
            ? Shader.Find("RokUnityLearning/Unlit Alpha ZWrite TwoSided")
            : Shader.Find("Unlit/Transparent");
        if (shader == null) shader = Shader.Find("Unlit/Transparent");
        if (shader == null) shader = Shader.Find("Unlit/Texture");
        if (shader == null) shader = Shader.Find("Standard");
        Material mat = new Material(shader);
        mat.name = "MAT_" + Sanitize(entry.Name);
        Texture2D tex = null;
        string texAssetPath = ToAssetPath(entry.TexturePath);
        if (!string.IsNullOrEmpty(texAssetPath))
        {
            ConfigureTextureImporter(texAssetPath, entry.Kind == "Boundary");
            AssetDatabase.ImportAsset(texAssetPath, ImportAssetOptions.ForceUpdate);
            tex = AssetDatabase.LoadAssetAtPath<Texture2D>(texAssetPath);
        }
        if (tex != null) mat.mainTexture = tex;
        if (entry.Kind == "Ground") mat.color = Color.white;
        if (entry.Kind == "Boundary") mat.color = Color.white;
        if (entry.Kind == "Building") mat.color = Color.white;
        if (entry.Kind == "Boundary")
        {
            mat.renderQueue = 2994;
            if (mat.HasProperty("_ZWrite")) mat.SetFloat("_ZWrite", 1.0f);
            if (mat.HasProperty("_Cutoff")) mat.SetFloat("_Cutoff", 0.02f);
        }
        else
        {
            mat.renderQueue = entry.Kind == "Ground" ? RuntimeGroundRenderQueue : RuntimeBuildingRenderQueue;
            ConfigureNoDepthMaterial(mat);
        }
        return mat;
    }

    private static string ToAssetPath(string absoluteOrAssetPath)
    {
        if (string.IsNullOrEmpty(absoluteOrAssetPath)) return null;
        string p = absoluteOrAssetPath.Replace('\\', '/');
        if (p.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase)) return p;
        string saved = SavedRoot.Replace('\\', '/');
        string liveRoot = LiveUnityAssetsRoot.Replace('\\', '/');
        string refRoot = RefUnityAssetsRoot.Replace('\\', '/');
        if (p.StartsWith(saved, StringComparison.OrdinalIgnoreCase))
        {
            return SavedAssetRoot + p.Substring(saved.Length);
        }
        if (p.StartsWith(liveRoot, StringComparison.OrdinalIgnoreCase))
        {
            return SceneAssetRoot + p.Substring(liveRoot.Length);
        }
        if (p.StartsWith(refRoot, StringComparison.OrdinalIgnoreCase))
        {
            return SceneAssetRoot + p.Substring(refRoot.Length);
        }
        return null;
    }

    private static GameObject Child(Transform parent, string name)
    {
        GameObject go = new GameObject(name);
        go.transform.SetParent(parent, false);
        return go;
    }

    private static Camera CreateCamera(Transform root, Bounds focusBounds)
    {
        GameObject go = new GameObject("WorldCamera_city_default_from_CameraParamLod");
        go.transform.SetParent(root, false);
        Camera cam = go.AddComponent<Camera>();
        cam.clearFlags = CameraClearFlags.Skybox;
        cam.fieldOfView = 7.5f;
        cam.nearClipPlane = 0.01f;
        cam.farClipPlane = 420f;
        Vector3 target = new Vector3(focusBounds.center.x, 0.0f, focusBounds.center.z);
        float distance = 20.0f * RuntimeUnityToLearningScale;
        Vector3 forward = new Vector3(0.0f, -0.7f, 0.7f).normalized;
        go.transform.position = target - forward * distance;
        go.transform.rotation = Quaternion.LookRotation(forward, Vector3.up);
        cam.tag = "MainCamera";
        UnityLearningCityCameraController controller = go.AddComponent<UnityLearningCityCameraController>();
        controller.defaultTarget = target;
        controller.sourceWorldScale = RuntimeUnityToLearningScale;
        controller.defaultDxf = 150.0f;
        controller.currentDxf = 150.0f;
        controller.minDxf = 76.691f;
        controller.maxDxf = 271.623f;
        controller.defaultDistance = distance;
        controller.target = target;
        controller.distance = distance;
        controller.minDistance = 14.47f * RuntimeUnityToLearningScale;
        controller.maxDistance = 25.0f * RuntimeUnityToLearningScale;
        controller.fieldOfView = 7.5f;
        Selection.activeObject = go;
        return cam;
    }

    private static void CreateLight(Transform root)
    {
        GameObject go = new GameObject("Directional Light_GameApp_reference");
        go.transform.SetParent(root, false);
        Light light = go.AddComponent<Light>();
        light.type = LightType.Directional;
        light.intensity = 1.15f;
        light.color = new Color(1f, 0.92f, 0.82f, 1f);
        go.transform.rotation = Quaternion.Euler(50f, -30f, 0f);
        RenderSettings.ambientLight = new Color(0.70f, 0.72f, 0.75f, 1f);
    }

    private static int SpawnManifestSpriteCards(string arrayKey, Transform parent, ref bool hasCameraBounds, ref Bounds cameraBounds)
    {
        int count = 0;
        foreach (string obj in ManifestObjects(arrayKey))
        {
            string actor = JsonString(obj, "actor");
            if (!ShouldShowBuildingType(JsonInt(obj, "building_type")))
            {
                continue;
            }
            string spritePath = JsonString(obj, "sprite_path");
            string maskPath = JsonString(obj, "mask_path");
            Vector3 center = JsonVector(obj, "ue_expected_bounds_center");
            Vector3 extents = JsonVector(obj, "ue_expected_bounds_extents");
            if (string.IsNullOrEmpty(actor) || string.IsNullOrEmpty(spritePath) || extents == Vector3.zero)
            {
                continue;
            }
            Texture2D texture = LoadMaskedTexture(spritePath, maskPath, "Baked_" + Sanitize(actor));
            if (texture == null)
            {
                continue;
            }
            Material material = CreateTransparentMaterial("MAT_" + Sanitize(actor), texture, 2996);
            GameObject quad = GameObject.CreatePrimitive(PrimitiveType.Quad);
            quad.name = actor;
            quad.transform.SetParent(parent, false);
            ApplyManifestCardTransform(quad.transform, center, extents, 0.0f, 1.0f);
            Renderer renderer = quad.GetComponent<Renderer>();
            if (renderer != null)
            {
                renderer.sharedMaterial = material;
                ConfigureRenderer(renderer, "Building", quad.transform.position);
            }
            Collider collider = quad.GetComponent<Collider>();
            if (collider != null)
            {
                UnityEngine.Object.DestroyImmediate(collider);
            }
            EncapsulateRenderers(quad, ref hasCameraBounds, ref cameraBounds);
            count++;
        }
        return count;
    }

    private static int SpawnManifestOverlays(string arrayKey, Transform parent)
    {
        int count = 0;
        foreach (string obj in ManifestObjects(arrayKey))
        {
            string actor = JsonString(obj, "actor");
            if (!ShouldShowBuildingType(JsonInt(obj, "building_type")))
            {
                continue;
            }
            string spriteName = JsonString(obj, "visual_sprite_name");
            Vector3 center = JsonVector(obj, "ue_expected_bounds_center");
            Vector3 extents = JsonVector(obj, "ue_expected_bounds_extents");
            if (string.IsNullOrEmpty(actor) || string.IsNullOrEmpty(spriteName) || extents == Vector3.zero)
            {
                continue;
            }

            int shadowCount = SpawnOverlayQuad(parent, actor + "_Shadow", "D:/ueDemo/Rok/Saved/RokUnityMigration/CityBuildingVisualLayers/" + spriteName + "_shadow.png", center, extents, -0.04f, 0.95f, 2994);
            if (shadowCount == 0)
            {
                shadowCount = SpawnGeneratedShadowQuad(parent, actor + "_GeneratedMaskShadow", JsonString(obj, "mask_path"), center, extents, -0.04f, 0.95f, 2994);
            }
            count += shadowCount;
            count += SpawnOverlayQuad(parent, actor + "_GroundPatch", "D:/ueDemo/Rok/Saved/RokUnityMigration/CityBuildingBodyGroundLayers/" + spriteName + "_ground_patch.png", center, extents, -0.02f, 0.90f, 2993);
        }
        return count;
    }

    private static int SpawnGeneratedShadowQuad(Transform parent, string name, string maskSourcePath, Vector3 center, Vector3 extents, float depthOffset, float scale, int renderQueue)
    {
        Texture2D texture = LoadShadowTexture(maskSourcePath, "Shadow_" + Sanitize(name));
        if (texture == null)
        {
            return 0;
        }
        Material material = CreateTransparentMaterial("MAT_" + Sanitize(name), texture, renderQueue);
        GameObject quad = GameObject.CreatePrimitive(PrimitiveType.Quad);
        quad.name = name;
        quad.transform.SetParent(parent, false);
        ApplyManifestCardTransform(quad.transform, center + new Vector3(18.0f, 0.0f, -12.0f), extents, depthOffset, scale);
        Renderer renderer = quad.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.sharedMaterial = material;
            ConfigureRenderer(renderer, "Shadow", quad.transform.position);
        }
        Collider collider = quad.GetComponent<Collider>();
        if (collider != null)
        {
            UnityEngine.Object.DestroyImmediate(collider);
        }
        return 1;
    }

    private static int SpawnOverlayQuad(Transform parent, string name, string sourcePath, Vector3 center, Vector3 extents, float depthOffset, float scale, int renderQueue)
    {
        string assetPath = ToAssetPath(sourcePath);
        if (string.IsNullOrEmpty(assetPath) || !File.Exists(AbsolutePathForAsset(assetPath)))
        {
            return 0;
        }
        AssetDatabase.ImportAsset(assetPath, ImportAssetOptions.ForceUpdate);
        Texture2D texture = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
        if (texture == null)
        {
            return 0;
        }
        Material material = CreateTransparentMaterial("MAT_" + Sanitize(name), texture, renderQueue);
        GameObject quad = GameObject.CreatePrimitive(PrimitiveType.Quad);
        quad.name = name;
        quad.transform.SetParent(parent, false);
        ApplyManifestCardTransform(quad.transform, center, extents, depthOffset, scale);
        Renderer renderer = quad.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.sharedMaterial = material;
            ConfigureRenderer(renderer, renderQueue == 2993 ? "GroundPatch" : "Shadow", quad.transform.position);
        }
        Collider collider = quad.GetComponent<Collider>();
        if (collider != null)
        {
            UnityEngine.Object.DestroyImmediate(collider);
        }
        return 1;
    }

    private static void ApplyManifestCardTransform(Transform transform, Vector3 center, Vector3 extents, float depthOffset, float scale)
    {
        transform.localPosition = new Vector3(center.x * 0.01f, center.z * 0.01f + depthOffset, center.y * 0.01f);
        transform.localRotation = Quaternion.Euler(45.0f, 0.0f, 0.0f);
        transform.localScale = new Vector3(Mathf.Max(0.1f, extents.x * 0.02f * scale), Mathf.Max(0.1f, Mathf.Max(extents.y, extents.z) * 0.028f * scale), 1.0f);
    }

    private static Material CreateTransparentMaterial(string name, Texture2D texture, int renderQueue)
    {
        Shader shader = Shader.Find("Unlit/Transparent");
        if (shader == null) shader = Shader.Find("Unlit/Texture");
        if (shader == null) shader = Shader.Find("Standard");
        Material material = new Material(shader);
        material.name = name;
        material.mainTexture = texture;
        material.renderQueue = renderQueue;
        ConfigureNoDepthMaterial(material);
        return material;
    }

    private static Material CreateNoDepthTextureMaterial(string name, Texture2D texture, int renderQueue)
    {
        Shader shader = Shader.Find("Unlit/Transparent");
        if (shader == null) shader = Shader.Find("Unlit/Texture");
        if (shader == null) shader = Shader.Find("Standard");
        Material material = new Material(shader);
        material.name = name;
        material.mainTexture = texture;
        material.color = Color.white;
        material.renderQueue = renderQueue;
        ConfigureNoDepthMaterial(material);
        return material;
    }

    private static void ConfigureNoDepthMaterial(Material material)
    {
        if (material == null)
        {
            return;
        }
        if (material.HasProperty("_ZWrite"))
        {
            material.SetFloat("_ZWrite", 0.0f);
        }
        if (material.HasProperty("_Cull"))
        {
            material.SetFloat("_Cull", 0.0f);
        }
    }

    private static void ConfigureRenderer(Renderer renderer, string kind, Vector3 sortPosition)
    {
        renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
        renderer.receiveShadows = false;

        int layerOffset = 0;
        if (kind == "Ground")
        {
            layerOffset = -3000;
        }
        else if (kind == "GroundPatch")
        {
            layerOffset = -1200;
        }
        else if (kind == "Shadow")
        {
            layerOffset = -800;
        }
        else if (kind == "Boundary")
        {
            layerOffset = 400;
        }
        else if (kind == "BoundaryPreview")
        {
            layerOffset = 700;
        }
        else
        {
            layerOffset = 1200;
        }

        renderer.sortingLayerName = "Default";
        renderer.sortingOrder = 30000 + layerOffset - Mathf.RoundToInt(sortPosition.z * 100.0f) - Mathf.RoundToInt(sortPosition.y * 20.0f);
    }

    private static void ConfigureTextureImporter(string assetPath, bool alphaCutout)
    {
        TextureImporter importer = AssetImporter.GetAtPath(assetPath) as TextureImporter;
        if (importer == null)
        {
            return;
        }
        importer.textureType = TextureImporterType.Default;
        importer.alphaIsTransparency = alphaCutout;
        importer.wrapMode = TextureWrapMode.Clamp;
        importer.filterMode = FilterMode.Trilinear;
        importer.mipmapEnabled = true;
        importer.maxTextureSize = 4096;
        importer.textureCompression = TextureImporterCompression.Uncompressed;
        importer.SaveAndReimport();
    }

    private static void SpawnSurroundingTerrainPlate(Transform parent, ref bool hasCameraBounds, ref Bounds cameraBounds)
    {
        string assetPath = ToAssetPath(SurroundingTerrainTexturePath);
        if (string.IsNullOrEmpty(assetPath) || !File.Exists(AbsolutePathForAsset(assetPath)))
        {
            return;
        }
        AssetDatabase.ImportAsset(assetPath, ImportAssetOptions.ForceUpdate);
        Texture2D texture = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
        if (texture == null)
        {
            return;
        }

        Material material = CreateNoDepthTextureMaterial("MAT_UnityRuntime_SurroundingTerrain_SourceBaked", texture, RuntimeGroundRenderQueue);
        GameObject quad = GameObject.CreatePrimitive(PrimitiveType.Quad);
        quad.name = "UnityRuntimeCityAllBuildings_SurroundingTerrain_SourceBaked";
        quad.transform.SetParent(parent, false);
        quad.transform.localPosition = new Vector3(0.0f, -0.10f, 0.0f);
        quad.transform.localRotation = Quaternion.Euler(90.0f, 0.0f, 0.0f);
        quad.transform.localScale = new Vector3(78.0f, 78.0f, 1.0f);
        Renderer renderer = quad.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.sharedMaterial = material;
            ConfigureRenderer(renderer, "Ground", quad.transform.position);
        }
        Collider collider = quad.GetComponent<Collider>();
        if (collider != null)
        {
            UnityEngine.Object.DestroyImmediate(collider);
        }
        EncapsulateRenderers(quad, ref hasCameraBounds, ref cameraBounds);
    }

    private static void EncapsulateRenderers(GameObject go, ref bool hasBounds, ref Bounds bounds)
    {
        foreach (Renderer renderer in go.GetComponentsInChildren<Renderer>(true))
        {
            if (!hasBounds)
            {
                bounds = renderer.bounds;
                hasBounds = true;
            }
            else
            {
                bounds.Encapsulate(renderer.bounds);
            }
        }
    }

    private static bool ShouldShowBuildingType(int buildingType)
    {
        if (buildingType <= 0)
        {
            return true;
        }
        if (!ShowDecorationBuildings && (buildingType == 51 || buildingType == 52 || buildingType == 53))
        {
            return false;
        }
        return true;
    }

    private static int BuildingTypeFromName(string name)
    {
        Match match = Regex.Match(name ?? "", "_build_([0-9]+)_");
        return match.Success ? int.Parse(match.Groups[1].Value) : 0;
    }

    private static int InitIdFromName(string name)
    {
        Match match = Regex.Match(name ?? "", "_Init_([0-9]+)_");
        return match.Success ? int.Parse(match.Groups[1].Value) : 0;
    }

    private static Dictionary<int, InitialBuildingInfo> LoadInitialBuildingInfoByInitId()
    {
        Dictionary<int, InitialBuildingInfo> result = new Dictionary<int, InitialBuildingInfo>();
        foreach (string obj in JsonObjectsFromArray(InitialCityStatePath, "buildings"))
        {
            InitialBuildingInfo info = new InitialBuildingInfo();
            info.InitId = JsonInt(obj, "init_id");
            info.BuildingType = JsonInt(obj, "type_id");
            info.GridX = JsonInt(obj, "x");
            info.GridY = JsonInt(obj, "y");
            info.Width = JsonInt(obj, "width");
            info.Length = JsonInt(obj, "length");
            if (info.InitId > 0)
            {
                result[info.InitId] = info;
            }
        }
        return result;
    }

    private static InitialBuildingInfo FallbackInitialBuildingInfo(int initId, int buildingType)
    {
        InitialBuildingInfo info = new InitialBuildingInfo();
        info.InitId = initId;
        info.BuildingType = buildingType;

        if (initId == 2)
        {
            info.GridX = 4; info.GridY = -2; info.Width = 6; info.Length = 6;
        }
        else if (initId == 3)
        {
            info.GridX = 5; info.GridY = 5; info.Width = 6; info.Length = 6;
        }
        else if (initId == 4)
        {
            info.GridX = -1; info.GridY = -1; info.Width = 5; info.Length = 5;
        }
        else if (initId == 5)
        {
            info.GridX = -5; info.GridY = 5; info.Width = 9; info.Length = 9;
        }
        else if (initId == 6)
        {
            info.GridX = -9; info.GridY = -3; info.Width = 7; info.Length = 7;
        }
        else if (initId == 8)
        {
            info.GridX = 6; info.GridY = -6; info.Width = 3; info.Length = 3;
        }

        return info;
    }

    private static Texture2D LoadMaskedTexture(string colorSourcePath, string maskSourcePath, string assetName)
    {
        string colorAssetPath = ToAssetPath(colorSourcePath);
        string maskAssetPath = ToAssetPath(maskSourcePath);
        if (string.IsNullOrEmpty(colorAssetPath))
        {
            return null;
        }
        AssetDatabase.ImportAsset(colorAssetPath, ImportAssetOptions.ForceUpdate);
        Texture2D color = AssetDatabase.LoadAssetAtPath<Texture2D>(colorAssetPath);
        Texture2D mask = null;
        if (!string.IsNullOrEmpty(maskAssetPath))
        {
            AssetDatabase.ImportAsset(maskAssetPath, ImportAssetOptions.ForceUpdate);
            mask = AssetDatabase.LoadAssetAtPath<Texture2D>(maskAssetPath);
        }
        if (color == null)
        {
            return null;
        }
        if (mask == null)
        {
            return color;
        }

        MakeReadable(colorAssetPath);
        MakeReadable(maskAssetPath);
        color = AssetDatabase.LoadAssetAtPath<Texture2D>(colorAssetPath);
        mask = AssetDatabase.LoadAssetAtPath<Texture2D>(maskAssetPath);
        Texture2D baked = new Texture2D(color.width, color.height, TextureFormat.RGBA32, false);
        Color[] colors = color.GetPixels();
        Color[] masks = mask.GetPixels();
        for (int i = 0; i < colors.Length && i < masks.Length; i++)
        {
            colors[i].a = masks[i].r;
        }
        baked.SetPixels(colors);
        baked.Apply();

        Directory.CreateDirectory(GeneratedTextureRoot);
        string generatedPath = GeneratedTextureRoot + "/" + assetName + ".png";
        File.WriteAllBytes(generatedPath, baked.EncodeToPNG());
        AssetDatabase.ImportAsset(generatedPath, ImportAssetOptions.ForceUpdate);
        return AssetDatabase.LoadAssetAtPath<Texture2D>(generatedPath);
    }

    private static Texture2D LoadShadowTexture(string maskSourcePath, string assetName)
    {
        string maskAssetPath = ToAssetPath(maskSourcePath);
        if (string.IsNullOrEmpty(maskAssetPath))
        {
            return null;
        }
        AssetDatabase.ImportAsset(maskAssetPath, ImportAssetOptions.ForceUpdate);
        Texture2D mask = AssetDatabase.LoadAssetAtPath<Texture2D>(maskAssetPath);
        if (mask == null)
        {
            return null;
        }
        MakeReadable(maskAssetPath);
        mask = AssetDatabase.LoadAssetAtPath<Texture2D>(maskAssetPath);
        Texture2D shadow = new Texture2D(mask.width, mask.height, TextureFormat.RGBA32, false);
        Color[] masks = mask.GetPixels();
        Color[] pixels = new Color[masks.Length];
        for (int i = 0; i < masks.Length; i++)
        {
            pixels[i] = new Color(0.05f, 0.035f, 0.02f, Mathf.Clamp01(masks[i].r * 0.32f));
        }
        shadow.SetPixels(pixels);
        shadow.Apply();

        Directory.CreateDirectory(GeneratedTextureRoot);
        string generatedPath = GeneratedTextureRoot + "/" + assetName + ".png";
        File.WriteAllBytes(generatedPath, shadow.EncodeToPNG());
        AssetDatabase.ImportAsset(generatedPath, ImportAssetOptions.ForceUpdate);
        return AssetDatabase.LoadAssetAtPath<Texture2D>(generatedPath);
    }

    private static void MakeReadable(string assetPath)
    {
        TextureImporter importer = AssetImporter.GetAtPath(assetPath) as TextureImporter;
        if (importer == null)
        {
            return;
        }
        if (!importer.isReadable || importer.textureCompression != TextureImporterCompression.Uncompressed)
        {
            importer.isReadable = true;
            importer.textureCompression = TextureImporterCompression.Uncompressed;
            importer.SaveAndReimport();
        }
    }

    private static IEnumerable<string> ManifestObjects(string arrayKey)
    {
        return JsonObjectsFromArray(RuntimeManifestPath, arrayKey);
    }

    private static IEnumerable<string> JsonObjectsFromArray(string jsonPath, string arrayKey)
    {
        if (!File.Exists(jsonPath))
        {
            yield break;
        }
        string json = File.ReadAllText(jsonPath);
        int keyIndex = json.IndexOf("\"" + arrayKey + "\"", StringComparison.Ordinal);
        if (keyIndex < 0)
        {
            yield break;
        }
        int arrayStart = json.IndexOf('[', keyIndex);
        if (arrayStart < 0)
        {
            yield break;
        }
        int depth = 0;
        int objectStart = -1;
        for (int i = arrayStart + 1; i < json.Length; i++)
        {
            char c = json[i];
            if (c == '{')
            {
                if (depth == 0)
                {
                    objectStart = i;
                }
                depth++;
            }
            else if (c == '}')
            {
                depth--;
                if (depth == 0 && objectStart >= 0)
                {
                    yield return json.Substring(objectStart, i - objectStart + 1);
                    objectStart = -1;
                }
            }
            else if (c == ']' && depth == 0)
            {
                yield break;
            }
        }
    }

    private static string JsonString(string obj, string key)
    {
        Match match = Regex.Match(obj, "\"" + Regex.Escape(key) + "\"\\s*:\\s*\"([^\"]*)\"");
        return match.Success ? match.Groups[1].Value.Replace("\\\\", "/") : "";
    }

    private static Vector3 JsonVector(string obj, string key)
    {
        Match match = Regex.Match(obj, "\"" + Regex.Escape(key) + "\"\\s*:\\s*\\[\\s*([-0-9.eE]+)\\s*,\\s*([-0-9.eE]+)\\s*,\\s*([-0-9.eE]+)");
        if (!match.Success)
        {
            return Vector3.zero;
        }
        return new Vector3(ParseFloat(match.Groups[1].Value), ParseFloat(match.Groups[2].Value), ParseFloat(match.Groups[3].Value));
    }

    private static int JsonInt(string obj, string key)
    {
        Match match = Regex.Match(obj, "\"" + Regex.Escape(key) + "\"\\s*:\\s*([-0-9]+)");
        int result;
        return match.Success && int.TryParse(match.Groups[1].Value, out result) ? result : 0;
    }

    private static float ParseFloat(string value)
    {
        float result;
        return float.TryParse(value, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out result) ? result : 0.0f;
    }

    private static string AbsolutePathForAsset(string assetPath)
    {
        if (!assetPath.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase))
        {
            return assetPath;
        }
        return Path.GetFullPath(Path.Combine(Application.dataPath, "..", assetPath)).Replace('\\', '/');
    }

    private static void CreateReadme(Transform root, int groundCount, int boundaryCount, int buildingCount, int extraBuildingCount, int overlayCount)
    {
        GameObject go = new GameObject("README_SourceTrace_DoNotHandPlace");
        go.transform.SetParent(root, false);
        TextMesh text = go.AddComponent<TextMesh>();
        text.text = "Initial city generated from ref/client-unity GameApp runtime exports\nGround " + groundCount + " | Boundary " + boundaryCount + " | Init buildings " + buildingCount + " | Extra " + extraBuildingCount + " | Overlays " + overlayCount + "\nSource: Saved/RokUnityMigration runtime manifests";
        text.characterSize = 0.35f;
        text.anchor = TextAnchor.MiddleCenter;
        go.transform.position = new Vector3(0f, 0.05f, -17f);
        go.transform.rotation = Quaternion.Euler(60f, 0f, 0f);
    }

    private static string Sanitize(string s)
    {
        foreach (char c in Path.GetInvalidFileNameChars()) s = s.Replace(c, '_');
        return s;
    }
}

