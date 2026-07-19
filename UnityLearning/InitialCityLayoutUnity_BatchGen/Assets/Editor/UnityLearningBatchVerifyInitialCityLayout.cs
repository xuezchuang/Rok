using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class UnityLearningBatchVerifyInitialCityLayout
{
    private const string ScenePath = "Assets/Scenes/UL_InitialCityLayout.unity";
    private const string OutputRoot = "D:/ueDemo/Rok/Saved/RokUnityMigration/UnityLearningVerification";

    public static void RebuildAndCapture()
    {
        Directory.CreateDirectory(OutputRoot);

        BuildInitialCityStartupScene.RebuildFromMenu();
        EditorSceneManager.OpenScene(ScenePath, OpenSceneMode.Single);

        Camera camera = Camera.main;
        if (camera == null)
        {
            GameObject cameraObject = GameObject.Find("WorldCamera_city_default_from_CameraParamLod");
            camera = cameraObject != null ? cameraObject.GetComponent<Camera>() : null;
        }
        if (camera == null)
        {
            throw new InvalidOperationException("No camera found in " + ScenePath);
        }

        string screenshotPath = Path.Combine(OutputRoot, "initial_city_layout_game.png").Replace('\\', '/');
        string zoomInScreenshotPath = Path.Combine(OutputRoot, "initial_city_layout_zoom_in.png").Replace('\\', '/');
        string zoomOutScreenshotPath = Path.Combine(OutputRoot, "initial_city_layout_zoom_out.png").Replace('\\', '/');
        string placementScreenshotPath = Path.Combine(OutputRoot, "initial_city_layout_placement_moved.png").Replace('\\', '/');
        string reportPath = Path.Combine(OutputRoot, "initial_city_layout_verify.json").Replace('\\', '/');

        UnityLearningCityCameraController controller = camera.GetComponent<UnityLearningCityCameraController>();
        float defaultDistance = controller != null ? controller.distance : 0.0f;
        float defaultFov = camera.fieldOfView;
        float defaultDxf = controller != null ? controller.GetCurrentDxf() : 0.0f;
        RenderCameraToPng(camera, screenshotPath, 1600, 900);

        float zoomInDistance = defaultDistance;
        float zoomOutDistance = defaultDistance;
        float zoomInFov = defaultFov;
        float zoomOutFov = defaultFov;
        float zoomInDxf = defaultDxf;
        float zoomOutDxf = defaultDxf;
        if (controller != null)
        {
            controller.SetZoom01(0.22f);
            zoomInDistance = controller.distance;
            zoomInFov = camera.fieldOfView;
            zoomInDxf = controller.GetCurrentDxf();
            RenderCameraToPng(camera, zoomInScreenshotPath, 1600, 900);

            controller.SetZoom01(0.92f);
            zoomOutDistance = controller.distance;
            zoomOutFov = camera.fieldOfView;
            zoomOutDxf = controller.GetCurrentDxf();
            RenderCameraToPng(camera, zoomOutScreenshotPath, 1600, 900);

            controller.ResetView();
        }
        else
        {
            RenderCameraToPng(camera, zoomInScreenshotPath, 1600, 900);
            RenderCameraToPng(camera, zoomOutScreenshotPath, 1600, 900);
        }

        UnityLearningCityPlacementController placementController = UnityEngine.Object.FindObjectOfType<UnityLearningCityPlacementController>();
        int placeableCount = placementController != null ? placementController.PlaceableCount : 0;
        if (placementController != null)
        {
            placementController.EnsureFeedbackForVerification();
        }
        PlacementAudit placementAudit = AuditPlacement(camera);
        bool interactionFlowPassed = false;
        string interactionActor = "";
        bool interactionClickSelected = false;
        bool interactionClickDidNotMove = false;
        bool interactionDragMoved = false;
        bool interactionInvalidRejected = false;
        bool placementSelected = false;
        bool placementTwoStage = false;
        bool placementMoved = false;
        float placementGridYawDegrees = placementController != null ? placementController.gridYawDegrees : 0.0f;
        string placementSelectedActor = "";
        string placementActor = "";
        string placementFromGrid = "";
        string placementToGrid = "";
        if (placementController != null)
        {
            interactionFlowPassed = placementController.TryVerifyInteractionFlowForVerification(out interactionActor, out interactionClickSelected, out interactionClickDidNotMove, out interactionDragMoved, out interactionInvalidRejected);
            placementSelected = placementController.TrySelectFirstMovableForVerification(out placementSelectedActor);
            placementTwoStage = placementSelected && placementController.HasActiveSelectionForVerification();
            placementMoved = placementController.TryMoveFirstMovableForVerification(out placementActor, out placementFromGrid, out placementToGrid);
            placementTwoStage = placementTwoStage && placementController.HasActiveSelectionForVerification();
            placementController.ShowFeedbackForVerification(placementActor);
        }
        RenderCameraToPng(camera, placementScreenshotPath, 1600, 900);

        string report = BuildReport(camera, screenshotPath, zoomInScreenshotPath, zoomOutScreenshotPath, placementScreenshotPath, defaultDistance, zoomInDistance, zoomOutDistance, defaultFov, zoomInFov, zoomOutFov, defaultDxf, zoomInDxf, zoomOutDxf, placementController != null, placeableCount, placementAudit, interactionFlowPassed, interactionActor, interactionClickSelected, interactionClickDidNotMove, interactionDragMoved, interactionInvalidRejected, placementSelected, placementTwoStage, placementGridYawDegrees, placementSelectedActor, placementMoved, placementActor, placementFromGrid, placementToGrid);
        File.WriteAllText(reportPath, report);

        Debug.Log("UnityLearning initial city verification screenshot: " + screenshotPath);
        Debug.Log("UnityLearning initial city zoom-in screenshot: " + zoomInScreenshotPath);
        Debug.Log("UnityLearning initial city zoom-out screenshot: " + zoomOutScreenshotPath);
        Debug.Log("UnityLearning initial city placement screenshot: " + placementScreenshotPath);
        Debug.Log("UnityLearning initial city verification report: " + reportPath);
    }

    private static void RenderCameraToPng(Camera camera, string path, int width, int height)
    {
        Texture2D screenshot = RenderCamera(camera, width, height);
        File.WriteAllBytes(path, screenshot.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(screenshot);
    }

    private static Texture2D RenderCamera(Camera camera, int width, int height)
    {
        RenderTexture previousTarget = camera.targetTexture;
        RenderTexture previousActive = RenderTexture.active;
        RenderTexture renderTexture = new RenderTexture(width, height, 24, RenderTextureFormat.ARGB32);
        Texture2D texture = new Texture2D(width, height, TextureFormat.RGB24, false);
        try
        {
            camera.targetTexture = renderTexture;
            RenderTexture.active = renderTexture;
            camera.Render();
            texture.ReadPixels(new Rect(0, 0, width, height), 0, 0);
            texture.Apply();
            return texture;
        }
        finally
        {
            camera.targetTexture = previousTarget;
            RenderTexture.active = previousActive;
            renderTexture.Release();
            UnityEngine.Object.DestroyImmediate(renderTexture);
        }
    }

    private static string BuildReport(Camera camera, string screenshotPath, string zoomInScreenshotPath, string zoomOutScreenshotPath, string placementScreenshotPath, float defaultDistance, float zoomInDistance, float zoomOutDistance, float defaultFov, float zoomInFov, float zoomOutFov, float defaultDxf, float zoomInDxf, float zoomOutDxf, bool placementControllerExists, int placeableCount, PlacementAudit placementAudit, bool interactionFlowPassed, string interactionActor, bool interactionClickSelected, bool interactionClickDidNotMove, bool interactionDragMoved, bool interactionInvalidRejected, bool placementSelected, bool placementTwoStage, float placementGridYawDegrees, string placementSelectedActor, bool placementMoved, string placementActor, string placementFromGrid, string placementToGrid)
    {
        GameObject boundaryRoot = GameObject.Find("RuntimeBoundary_From_RefUnityPrefabs");
        GameObject groundAuditRoot = GameObject.Find("RuntimeGroundObj_SourceMeshes_HiddenForAudit");
        Renderer[] boundaryRenderers = boundaryRoot != null ? boundaryRoot.GetComponentsInChildren<Renderer>(true) : new Renderer[0];

        int enabledBoundaryRenderers = 0;
        int disabledBoundaryRenderers = 0;
        int missingScriptCount = 0;
        Bounds boundaryBounds = new Bounds(Vector3.zero, Vector3.zero);
        bool hasBounds = false;
        List<string> enabledRendererNames = new List<string>();

        foreach (Renderer renderer in boundaryRenderers)
        {
            if (renderer.enabled && renderer.gameObject.activeInHierarchy)
            {
                enabledBoundaryRenderers++;
                enabledRendererNames.Add(HierarchyPath(renderer.transform));
                if (!hasBounds)
                {
                    boundaryBounds = renderer.bounds;
                    hasBounds = true;
                }
                else
                {
                    boundaryBounds.Encapsulate(renderer.bounds);
                }
            }
            else
            {
                disabledBoundaryRenderers++;
            }
        }

        if (boundaryRoot != null)
        {
            foreach (Transform transform in boundaryRoot.GetComponentsInChildren<Transform>(true))
            {
                missingScriptCount += GameObjectUtility.GetMonoBehavioursWithMissingScriptCount(transform.gameObject);
            }
        }

        return "{\n"
            + JsonPair("scene", ScenePath, true)
            + JsonPair("screenshot", screenshotPath, true)
            + JsonPair("zoom_in_screenshot", zoomInScreenshotPath, true)
            + JsonPair("zoom_out_screenshot", zoomOutScreenshotPath, true)
            + JsonPair("placement_screenshot", placementScreenshotPath, true)
            + JsonPair("camera", HierarchyPath(camera.transform), true)
            + JsonPair("default_distance", defaultDistance, true)
            + JsonPair("zoom_in_distance", zoomInDistance, true)
            + JsonPair("zoom_out_distance", zoomOutDistance, true)
            + JsonPair("default_fov", defaultFov, true)
            + JsonPair("zoom_in_fov", zoomInFov, true)
            + JsonPair("zoom_out_fov", zoomOutFov, true)
            + JsonPair("default_dxf", defaultDxf, true)
            + JsonPair("zoom_in_dxf", zoomInDxf, true)
            + JsonPair("zoom_out_dxf", zoomOutDxf, true)
            + JsonPair("boundary_root_exists", boundaryRoot != null, true)
            + JsonPair("boundary_root_local_scale", boundaryRoot != null ? Vec(boundaryRoot.transform.localScale) : "null", true, false)
            + JsonPair("ground_audit_root_exists", groundAuditRoot != null, true)
            + JsonPair("placement_controller_exists", placementControllerExists, true)
            + JsonPair("placeable_count", placeableCount, true)
            + JsonPair("placeable_with_colliders", placementAudit.PlaceableWithColliders, true)
            + JsonPair("boundary_placeable_count", placementAudit.BoundaryPlaceableCount, true)
            + JsonPair("raycast_selectable_hits", placementAudit.RaycastSelectableHits, true)
            + JsonPair("placement_grid_line_count", placementAudit.GridLineCount, true)
            + JsonPair("placement_arrow_count", placementAudit.ArrowCount, true)
            + JsonPair("placement_fill_count", placementAudit.FillCount, true)
            + JsonPair("interaction_flow_passed", interactionFlowPassed, true)
            + JsonPair("interaction_actor", interactionActor, true)
            + JsonPair("interaction_click_selected", interactionClickSelected, true)
            + JsonPair("interaction_click_did_not_move", interactionClickDidNotMove, true)
            + JsonPair("interaction_drag_moved", interactionDragMoved, true)
            + JsonPair("interaction_invalid_rejected", interactionInvalidRejected, true)
            + JsonPair("placement_selected", placementSelected, true)
            + JsonPair("placement_two_stage", placementTwoStage, true)
            + JsonPair("placement_grid_yaw_degrees", placementGridYawDegrees, true)
            + JsonPair("placement_selected_actor", placementSelectedActor, true)
            + JsonPair("placement_moved", placementMoved, true)
            + JsonPair("placement_actor", placementActor, true)
            + JsonPair("placement_from_grid", placementFromGrid, true)
            + JsonPair("placement_to_grid", placementToGrid, true)
            + JsonPair("enabled_boundary_renderers", enabledBoundaryRenderers, true)
            + JsonPair("disabled_boundary_renderers", disabledBoundaryRenderers, true)
            + JsonPair("missing_boundary_scripts", missingScriptCount, true)
            + JsonPair("boundary_bounds_center", hasBounds ? Vec(boundaryBounds.center) : "null", true, false)
            + JsonPair("boundary_bounds_size", hasBounds ? Vec(boundaryBounds.size) : "null", true, false)
            + JsonStringArray("enabled_boundary_renderer_names", enabledRendererNames, false)
            + "}\n";
    }

    private struct PlacementAudit
    {
        public int PlaceableWithColliders;
        public int BoundaryPlaceableCount;
        public int RaycastSelectableHits;
        public int GridLineCount;
        public int ArrowCount;
        public int FillCount;
    }

    private static PlacementAudit AuditPlacement(Camera camera)
    {
        PlacementAudit audit = new PlacementAudit();
        GameObject boundaryRoot = GameObject.Find("RuntimeBoundary_From_RefUnityPrefabs");
        UnityLearningCityPlaceable[] placeables = UnityEngine.Object.FindObjectsOfType<UnityLearningCityPlaceable>();
        foreach (UnityLearningCityPlaceable placeable in placeables)
        {
            Collider collider = placeable.GetComponent<Collider>();
            if (collider != null && collider.enabled)
            {
                audit.PlaceableWithColliders++;
                if (camera != null && RaycastHitsPlaceable(camera, placeable, collider))
                {
                    audit.RaycastSelectableHits++;
                }
            }

            if (boundaryRoot != null && placeable.transform.IsChildOf(boundaryRoot.transform))
            {
                audit.BoundaryPlaceableCount++;
            }
        }
        foreach (GameObject go in Resources.FindObjectsOfTypeAll<GameObject>())
        {
            if (go == null || !go.scene.IsValid())
            {
                continue;
            }
            if (go.name.StartsWith("GridLine_", StringComparison.Ordinal))
            {
                audit.GridLineCount++;
            }
            else if (go.name.StartsWith("MoveArrow_", StringComparison.Ordinal))
            {
                audit.ArrowCount++;
            }
            else if (go.name == "FootprintFill")
            {
                audit.FillCount++;
            }
        }
        return audit;
    }

    private static bool RaycastHitsPlaceable(Camera camera, UnityLearningCityPlaceable placeable, Collider collider)
    {
        Vector3 screenPoint = camera.WorldToScreenPoint(collider.bounds.center);
        if (screenPoint.z <= 0.0f)
        {
            return false;
        }

        Ray ray = camera.ScreenPointToRay(new Vector3(screenPoint.x, screenPoint.y, 0.0f));
        RaycastHit[] hits = Physics.RaycastAll(ray, 10000.0f);
        Array.Sort(hits, delegate (RaycastHit left, RaycastHit right)
        {
            return left.distance.CompareTo(right.distance);
        });

        for (int i = 0; i < hits.Length; i++)
        {
            UnityLearningCityPlaceable hitPlaceable = hits[i].collider.GetComponentInParent<UnityLearningCityPlaceable>();
            if (hitPlaceable == placeable)
            {
                return true;
            }
        }
        return false;
    }

    private static string JsonPair(string key, string value, bool comma)
    {
        return "  \"" + key + "\": \"" + Escape(value) + "\"" + (comma ? "," : "") + "\n";
    }

    private static string JsonPair(string key, bool value, bool comma)
    {
        return "  \"" + key + "\": " + (value ? "true" : "false") + (comma ? "," : "") + "\n";
    }

    private static string JsonPair(string key, int value, bool comma)
    {
        return "  \"" + key + "\": " + value + (comma ? "," : "") + "\n";
    }

    private static string JsonPair(string key, float value, bool comma)
    {
        return "  \"" + key + "\": " + value.ToString("0.######", System.Globalization.CultureInfo.InvariantCulture) + (comma ? "," : "") + "\n";
    }

    private static string JsonPair(string key, string value, bool comma, bool quote)
    {
        if (quote)
        {
            return JsonPair(key, value, comma);
        }
        return "  \"" + key + "\": " + value + (comma ? "," : "") + "\n";
    }

    private static string JsonStringArray(string key, List<string> values, bool comma)
    {
        List<string> escaped = new List<string>();
        foreach (string value in values)
        {
            escaped.Add("    \"" + Escape(value) + "\"");
        }
        return "  \"" + key + "\": [\n" + string.Join(",\n", escaped.ToArray()) + "\n  ]" + (comma ? "," : "") + "\n";
    }

    private static string Vec(Vector3 vector)
    {
        return "["
            + vector.x.ToString("0.######", System.Globalization.CultureInfo.InvariantCulture) + ", "
            + vector.y.ToString("0.######", System.Globalization.CultureInfo.InvariantCulture) + ", "
            + vector.z.ToString("0.######", System.Globalization.CultureInfo.InvariantCulture) + "]";
    }

    private static string Escape(string value)
    {
        return (value ?? "").Replace("\\", "\\\\").Replace("\"", "\\\"");
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
}
