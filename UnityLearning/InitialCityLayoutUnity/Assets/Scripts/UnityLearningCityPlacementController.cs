using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public sealed class UnityLearningCityPlacementController : MonoBehaviour
{
    public Camera sceneCamera;
    public Transform buildingRoot;
    public float cellSize = 0.5f;
    public int cityMinGrid = -24;
    public int cityMaxGrid = 24;
    public float gridYawDegrees = -45.0f;
    public float dragThresholdPixels = 8.0f;
    public bool stagedPlacementMode = true;
    public string stagedNextButtonLabel = "\u4E0B\u4E00\u6B65\u5EFA\u7B51";
    public Color selectedTint = new Color(1.0f, 1.0f, 1.0f, 1.0f);
    public Color validTint = new Color(0.74f, 1.0f, 0.74f, 1.0f);
    public Color invalidTint = new Color(1.0f, 0.55f, 0.55f, 1.0f);

    private readonly List<UnityLearningCityPlaceable> placeables = new List<UnityLearningCityPlaceable>();
    private UnityLearningCityPlaceable activePlaceable;
    private bool isDragging;
    private bool isPointerDownForDrag;
    private bool pointerDownSelectionChanged;
    private bool activePlacementValid;
    private int activeGridX;
    private int activeGridY;
    private float dragGridOffsetX;
    private float dragGridOffsetY;
    private Vector2 pointerDownScreen;
    private UnityLearningCityPlaceable pointerDownPlaceable;
    private GameObject feedbackRoot;
    private Transform gridRoot;
    private Transform moveArrowRoot;
    private readonly Transform[] footprintBorders = new Transform[4];
    private readonly Transform[] moveArrows = new Transform[4];
    private readonly List<Transform> gridLines = new List<Transform>();
    private readonly Vector2[] guiArrowPositions = new Vector2[4];
    private readonly float[] guiArrowRotations = new float[4];
    private readonly Vector2[] guiFootprintCorners = new Vector2[4];
    private Material validMaterial;
    private Material invalidMaterial;
    private Material gridMaterial;
    private Material arrowMaterial;
    private Sprite[] moveArrowSprites = new Sprite[4];
    private static readonly string[] MoveArrowSpriteNames = new string[] { "Arrow_up", "Arrow_right", "Arrow_down", "Arrow_left" };
    private const string MoveArrowAtlasResourcePath = "RokRef/ui_hud";
    private Texture2D guiArrowTexture;
    private Texture2D guiLineTexture;
    private bool guiArrowsVisible;
    private bool guiFootprintVisible;
    private Color guiFootprintColor = Color.green;

    public int PlaceableCount
    {
        get
        {
            RefreshPlaceables();
            return placeables.Count;
        }
    }

    private void OnEnable()
    {
        RefreshPlaceables();
        HideFeedback();
    }

    private void OnDisable()
    {
        ClearActiveTint();
        HideFeedback();
    }

    private void Update()
    {
        if (Application.isPlaying)
        {
            HandlePlayInput();
        }
    }

    private void OnGUI()
    {
        if (Application.isPlaying)
        {
            DrawStagedPlacementGui();
            DrawGuiFootprint(Event.current);
            DrawGuiArrows(Event.current);
            return;
        }

        Event current = Event.current;
        if (current == null)
        {
            return;
        }

        Vector2 screenPosition = GuiPointToScreen(current.mousePosition);
        if (current.type == EventType.MouseDown && current.button == 0)
        {
            if (HandlePrimaryDown(screenPosition))
            {
                current.Use();
            }
        }
        else if ((current.type == EventType.MouseDrag || current.type == EventType.MouseMove) && (isDragging || isPointerDownForDrag))
        {
            if (HandlePrimaryHeld(screenPosition))
            {
                current.Use();
            }
        }
        else if (current.type == EventType.MouseUp && current.button == 0 && (isDragging || isPointerDownForDrag || pointerDownSelectionChanged))
        {
            HandlePrimaryUp(screenPosition);
            current.Use();
        }
        else if (current.type == EventType.MouseDown && current.button == 1 && activePlaceable != null)
        {
            CancelDragOrClearSelection();
            current.Use();
        }
        else if (current.type == EventType.KeyDown && current.keyCode == KeyCode.Escape && activePlaceable != null)
        {
            CancelDragOrClearSelection();
            current.Use();
        }

        DrawStagedPlacementGui();
        DrawGuiFootprint(current);
        DrawGuiArrows(current);
    }

    private void DrawStagedPlacementGui()
    {
        if (!stagedPlacementMode || buildingRoot == null)
        {
            return;
        }

        int total;
        int inactive;
        CountStagedPlaceables(out total, out inactive);

        Rect rect = new Rect(18.0f, 18.0f, 220.0f, 86.0f);
        GUILayout.BeginArea(rect, GUI.skin.box);
        GUILayout.Label("建筑摆放 " + (total - inactive) + "/" + total);
        GUI.enabled = inactive > 0;
        if (GUILayout.Button(stagedNextButtonLabel, GUILayout.Height(34.0f)))
        {
            ActivateNextStagedPlaceable();
        }
        GUI.enabled = true;
        GUILayout.EndArea();
    }

    private void CountStagedPlaceables(out int total, out int inactive)
    {
        total = 0;
        inactive = 0;
        UnityLearningCityPlaceable[] found = buildingRoot.GetComponentsInChildren<UnityLearningCityPlaceable>(true);
        for (int i = 0; i < found.Length; i++)
        {
            if (found[i] == null || !found[i].movable)
            {
                continue;
            }

            total++;
            if (!found[i].gameObject.activeSelf)
            {
                inactive++;
            }
        }
    }

    private void ActivateNextStagedPlaceable()
    {
        if (buildingRoot == null)
        {
            return;
        }

        UnityLearningCityPlaceable[] found = buildingRoot.GetComponentsInChildren<UnityLearningCityPlaceable>(true);
        for (int i = 0; i < found.Length; i++)
        {
            UnityLearningCityPlaceable placeable = found[i];
            if (placeable == null || !placeable.movable || placeable.gameObject.activeSelf)
            {
                continue;
            }

            placeable.gameObject.SetActive(true);
            placeable.SetGridBasis(GridOriginLocal(), GridAxisXLocal(), GridAxisYLocal());
            placeable.CommitGrid(placeable.gridX, placeable.gridY);
            SelectPlaceable(placeable);
            Debug.LogFormat("[UnityLearningPlacement] staged next building: {0} initId={1} type={2} grid=({3},{4}) size=({5},{6})",
                placeable.name, placeable.initId, placeable.buildingType, placeable.gridX, placeable.gridY, placeable.width, placeable.length);
            return;
        }
    }

    private bool TryFindReferenceTempBuildGrid(UnityLearningCityPlaceable placeable, out int targetX, out int targetY)
    {
        targetX = -placeable.width;
        targetY = -placeable.length;

        int hsize = Mathf.Max(1, (cityMaxGrid - cityMinGrid + 1) / 2);
        int[,] offBuildPos = new int[,]
        {
            { 1, 1 },
            { -1, 1 },
            { -1, -1 },
            { 1, -1 }
        };

        for (int i = 1; i < hsize; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                int sx = offBuildPos[j, 0] * i;
                int sy = offBuildPos[j, 1] * i;
                int tt = j + 1;
                if (tt >= 4)
                {
                    tt = 0;
                }

                int ex = offBuildPos[tt, 0] * i;
                int ey = offBuildPos[tt, 1] * i;
                int minsx = Mathf.Min(sx, ex);
                int maxex = Mathf.Max(sx, ex);
                int minsy = Mathf.Min(sy, ey);
                int maxey = Mathf.Max(sy, ey);

                for (int xx = minsx; xx < maxex; xx++)
                {
                    if (IsPlacementValid(placeable, xx, ey))
                    {
                        targetX = xx;
                        targetY = ey;
                        return true;
                    }
                }

                for (int yy = minsy; yy < maxey; yy++)
                {
                    if (IsPlacementValid(placeable, ex, yy))
                    {
                        targetX = ex;
                        targetY = yy;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    public bool TryMoveFirstMovableForVerification(out string actorName, out string fromGrid, out string toGrid)
    {
        actorName = string.Empty;
        fromGrid = string.Empty;
        toGrid = string.Empty;
        RefreshPlaceables();

        for (int i = 0; i < placeables.Count; i++)
        {
            UnityLearningCityPlaceable placeable = placeables[i];
            if (placeable == null || !placeable.movable)
            {
                continue;
            }

            int sourceX = placeable.gridX;
            int sourceY = placeable.gridY;
            for (int radius = 1; radius <= 12; radius++)
            {
                for (int dx = -radius; dx <= radius; dx++)
                {
                    for (int dy = -radius; dy <= radius; dy++)
                    {
                        if (Mathf.Abs(dx) != radius && Mathf.Abs(dy) != radius)
                        {
                            continue;
                        }

                        int targetX = sourceX + dx;
                        int targetY = sourceY + dy;
                        if (!IsPlacementValid(placeable, targetX, targetY))
                        {
                            continue;
                        }

                        actorName = placeable.name;
                        fromGrid = sourceX + "," + sourceY;
                        toGrid = targetX + "," + targetY;
                        SelectPlaceable(placeable);
                        placeable.CommitGrid(targetX, targetY);
                        activeGridX = targetX;
                        activeGridY = targetY;
                        activePlacementValid = true;
                        ShowFeedback(placeable, targetX, targetY, true);
                        return true;
                    }
                }
            }
        }

        return false;
    }

    public bool TrySelectFirstMovableForVerification(out string actorName)
    {
        actorName = string.Empty;
        RefreshPlaceables();
        for (int i = 0; i < placeables.Count; i++)
        {
            UnityLearningCityPlaceable placeable = placeables[i];
            if (placeable == null || !placeable.movable)
            {
                continue;
            }

            SelectPlaceable(placeable);
            actorName = placeable.name;
            return activePlaceable == placeable && !isDragging;
        }
        return false;
    }

    public bool HasActiveSelectionForVerification()
    {
        return activePlaceable != null && !isDragging;
    }

    public bool TryVerifyInteractionFlowForVerification(out string actorName, out bool clickSelected, out bool clickDidNotMove, out bool dragMoved, out bool invalidRejected)
    {
        actorName = string.Empty;
        clickSelected = false;
        clickDidNotMove = false;
        dragMoved = false;
        invalidRejected = false;

        Camera targetCamera = ResolveCamera();
        if (targetCamera == null)
        {
            return false;
        }

        RefreshPlaceables();
        UnityLearningCityPlaceable placeable = FirstRaycastReachablePlaceable(targetCamera);
        if (placeable == null)
        {
            return false;
        }

        actorName = placeable.name;
        ClearSelection();

        Vector2 clickScreen;
        if (!TryGetPlaceableScreenPoint(targetCamera, placeable, out clickScreen))
        {
            return false;
        }

        Vector3 originalLocalPosition = placeable.transform.localPosition;
        int originalGridX = placeable.gridX;
        int originalGridY = placeable.gridY;

        bool downAccepted = HandlePrimaryDown(clickScreen);
        HandlePrimaryUp(clickScreen);
        clickSelected = downAccepted && activePlaceable == placeable && !isDragging;
        clickDidNotMove = clickSelected
            && placeable.gridX == originalGridX
            && placeable.gridY == originalGridY
            && (placeable.transform.localPosition - originalLocalPosition).sqrMagnitude < 0.0001f;

        int validTargetX;
        int validTargetY;
        if (!TryFindValidTarget(placeable, out validTargetX, out validTargetY))
        {
            return clickSelected && clickDidNotMove;
        }

        Vector2 dragStart = clickScreen;
        Vector2 dragEnd;
        if (!TryGetDragTargetScreen(targetCamera, placeable, dragStart, validTargetX, validTargetY, out dragEnd))
        {
            return clickSelected && clickDidNotMove;
        }

        HandlePrimaryDown(dragStart);
        HandlePrimaryHeld(dragEnd);
        HandlePrimaryUp(dragEnd);
        dragMoved = placeable.gridX == validTargetX && placeable.gridY == validTargetY;

        Vector3 beforeInvalidLocalPosition = placeable.transform.localPosition;
        int beforeInvalidGridX = placeable.gridX;
        int beforeInvalidGridY = placeable.gridY;
        int invalidTargetX;
        int invalidTargetY;
        if (!TryFindOverlappingInvalidTarget(placeable, out invalidTargetX, out invalidTargetY))
        {
            invalidTargetX = cityMaxGrid + placeable.width + 2;
            invalidTargetY = cityMaxGrid + placeable.length + 2;
        }

        Vector2 invalidStart;
        Vector2 invalidEnd;
        if (TryGetPlaceableScreenPoint(targetCamera, placeable, out invalidStart)
            && TryGetDragTargetScreen(targetCamera, placeable, invalidStart, invalidTargetX, invalidTargetY, out invalidEnd))
        {
            HandlePrimaryDown(invalidStart);
            HandlePrimaryHeld(invalidEnd);
            HandlePrimaryUp(invalidEnd);
            invalidRejected = placeable.gridX == beforeInvalidGridX
                && placeable.gridY == beforeInvalidGridY
                && (placeable.transform.localPosition - beforeInvalidLocalPosition).sqrMagnitude < 0.0001f;
        }

        ShowSelectedFeedback();
        return clickSelected && clickDidNotMove && dragMoved && invalidRejected;
    }

    public void EnsureFeedbackForVerification()
    {
        EnsureFeedback();
        HideFeedback();
    }

    public bool ShowFeedbackForVerification(string actorName)
    {
        RefreshPlaceables();
        for (int i = 0; i < placeables.Count; i++)
        {
            UnityLearningCityPlaceable placeable = placeables[i];
            if (placeable == null)
            {
                continue;
            }
            if (!string.IsNullOrEmpty(actorName) && placeable.name != actorName)
            {
                continue;
            }

            ShowFeedback(placeable, placeable.gridX, placeable.gridY, true);
            return true;
        }
        return false;
    }

    private void HandlePlayInput()
    {
        if (Input.GetMouseButtonDown(1) || Input.GetKeyDown(KeyCode.Escape))
        {
            CancelDragOrClearSelection();
            return;
        }

        if (Input.GetMouseButtonDown(0))
        {
            HandlePrimaryDown(Input.mousePosition);
        }
        else if (Input.GetMouseButton(0))
        {
            HandlePrimaryHeld(Input.mousePosition);
        }
        else if (Input.GetMouseButtonUp(0))
        {
            HandlePrimaryUp(Input.mousePosition);
        }
    }

    private bool HandlePrimaryDown(Vector2 screenPosition)
    {
        RefreshPlaceables();
        UnityLearningCityPlaceable placeable = RaycastPlaceable(screenPosition);
        if (placeable == null || !placeable.movable)
        {
            pointerDownScreen = screenPosition;
            pointerDownPlaceable = activePlaceable;
            isPointerDownForDrag = activePlaceable != null;
            pointerDownSelectionChanged = false;
            return activePlaceable != null;
        }

        bool changedSelection = activePlaceable != placeable;
        if (changedSelection)
        {
            SelectPlaceable(placeable);
        }
        else
        {
            ShowSelectedFeedback();
        }

        pointerDownScreen = screenPosition;
        pointerDownPlaceable = placeable;
        pointerDownSelectionChanged = changedSelection;
        isPointerDownForDrag = !changedSelection && activePlaceable == placeable;
        return true;
    }

    private bool HandlePrimaryHeld(Vector2 screenPosition)
    {
        if (isDragging)
        {
            UpdatePlacement(screenPosition);
            return true;
        }

        if (!isPointerDownForDrag || pointerDownSelectionChanged || activePlaceable == null || pointerDownPlaceable != activePlaceable)
        {
            return false;
        }

        if ((screenPosition - pointerDownScreen).sqrMagnitude < dragThresholdPixels * dragThresholdPixels)
        {
            return true;
        }

        BeginDragSelected(screenPosition);
        return true;
    }

    private void HandlePrimaryUp(Vector2 screenPosition)
    {
        if (isDragging)
        {
            UpdatePlacement(screenPosition);
            EndPlacement();
        }

        isPointerDownForDrag = false;
        pointerDownSelectionChanged = false;
        pointerDownPlaceable = null;
    }

    private bool SelectPlaceable(UnityLearningCityPlaceable placeable)
    {
        if (placeable == null || !placeable.movable)
        {
            return false;
        }

        if (activePlaceable != placeable)
        {
            ClearActiveTint();
            if (activePlaceable != null && isDragging)
            {
                activePlaceable.CancelPreview();
            }
        }

        activePlaceable = placeable;
        isDragging = false;
        isPointerDownForDrag = false;
        pointerDownSelectionChanged = false;
        pointerDownPlaceable = null;
        activeGridX = placeable.gridX;
        activeGridY = placeable.gridY;
        activePlacementValid = true;
        activePlaceable.SetTint(selectedTint);
        ShowSelectedFeedback();
        return true;
    }

    private bool BeginDragSelected(Vector2 screenPosition)
    {
        if (activePlaceable == null || !activePlaceable.movable)
        {
            return false;
        }

        float gridXFloat;
        float gridYFloat;
        if (ScreenToGridOriginFloat(pointerDownScreen, activePlaceable, out gridXFloat, out gridYFloat))
        {
            dragGridOffsetX = gridXFloat - activePlaceable.gridX;
            dragGridOffsetY = gridYFloat - activePlaceable.gridY;
        }
        else
        {
            dragGridOffsetX = 0.0f;
            dragGridOffsetY = 0.0f;
        }

        isDragging = true;
        activeGridX = activePlaceable.gridX;
        activeGridY = activePlaceable.gridY;
        activePlacementValid = true;
        UpdatePlacement(screenPosition);
        return true;
    }

    private void UpdatePlacement(Vector2 screenPosition)
    {
        if (activePlaceable == null)
        {
            return;
        }

        float gridXFloat;
        float gridYFloat;
        if (!ScreenToGridOriginFloat(screenPosition, activePlaceable, out gridXFloat, out gridYFloat))
        {
            return;
        }

        int targetGridX = SnapGridFromReferenceRule(gridXFloat - dragGridOffsetX);
        int targetGridY = SnapGridFromReferenceRule(gridYFloat - dragGridOffsetY);

        activeGridX = targetGridX;
        activeGridY = targetGridY;
        activePlacementValid = IsPlacementValid(activePlaceable, activeGridX, activeGridY, true);
        activePlaceable.PreviewGrid(activeGridX, activeGridY);
        activePlaceable.SetTint(activePlacementValid ? validTint : invalidTint);
        ShowFeedback(activePlaceable, activeGridX, activeGridY, activePlacementValid);
    }

    private void EndPlacement()
    {
        if (activePlaceable != null)
        {
            if (activePlacementValid)
            {
                activePlaceable.CommitGrid(activeGridX, activeGridY);
            }
            else
            {
                activePlaceable.CancelPreview();
            }

            activeGridX = activePlaceable.gridX;
            activeGridY = activePlaceable.gridY;
            activePlacementValid = true;
            activePlaceable.SetTint(selectedTint);
            ShowSelectedFeedback();
        }

        isDragging = false;
        isPointerDownForDrag = false;
    }

    private void CancelPlacement()
    {
        if (activePlaceable != null)
        {
            activePlaceable.CancelPreview();
            activeGridX = activePlaceable.gridX;
            activeGridY = activePlaceable.gridY;
            activePlacementValid = true;
            activePlaceable.SetTint(selectedTint);
            ShowSelectedFeedback();
        }

        isDragging = false;
        isPointerDownForDrag = false;
        pointerDownSelectionChanged = false;
        pointerDownPlaceable = null;
    }

    private void CancelDragOrClearSelection()
    {
        if (isDragging)
        {
            CancelPlacement();
            return;
        }

        ClearSelection();
    }

    private void ClearSelection()
    {
        ClearActiveTint();
        if (activePlaceable != null)
        {
            activePlaceable.CancelPreview();
        }

        activePlaceable = null;
        isDragging = false;
        isPointerDownForDrag = false;
        pointerDownSelectionChanged = false;
        pointerDownPlaceable = null;
        HideFeedback();
    }

    private void ShowSelectedFeedback()
    {
        if (activePlaceable == null)
        {
            HideFeedback();
            return;
        }

        EnsureFeedback();
        feedbackRoot.SetActive(true);
        SetFootprintVisible(false);
        guiFootprintVisible = false;
        guiArrowsVisible = false;
        SetMoveArrowsFromReferenceRule(activePlaceable, activePlaceable.gridX, activePlaceable.gridY);
    }

    private void ClearActiveTint()
    {
        if (activePlaceable != null)
        {
            activePlaceable.ClearTint();
        }
    }

    private bool IsPlacementValid(UnityLearningCityPlaceable placeable, int targetGridX, int targetGridY, bool logInvalid = false)
    {
        if (placeable == null || !placeable.movable || placeable.width <= 0 || placeable.length <= 0)
        {
            if (logInvalid)
            {
                Debug.LogWarningFormat("[UnityLearningPlacement] invalid: selected placeable is null/non-movable/bad-size. placeable={0}", placeable != null ? placeable.name : "<null>");
            }
            return false;
        }

        if (targetGridX < cityMinGrid || targetGridY < cityMinGrid)
        {
            if (logInvalid)
            {
                Debug.LogWarningFormat("[UnityLearningPlacement] invalid: below city min. selected={0} initId={1} type={2} target=({3},{4}) size=({5},{6}) min={7}",
                    placeable.name, placeable.initId, placeable.buildingType, targetGridX, targetGridY, placeable.width, placeable.length, cityMinGrid);
            }
            return false;
        }

        if (targetGridX + placeable.width - 1 > cityMaxGrid || targetGridY + placeable.length - 1 > cityMaxGrid)
        {
            if (logInvalid)
            {
                Debug.LogWarningFormat("[UnityLearningPlacement] invalid: above city max. selected={0} initId={1} type={2} target=({3},{4}) end=({5},{6}) size=({7},{8}) max={9}",
                    placeable.name, placeable.initId, placeable.buildingType, targetGridX, targetGridY,
                    targetGridX + placeable.width - 1, targetGridY + placeable.length - 1, placeable.width, placeable.length, cityMaxGrid);
            }
            return false;
        }

        RefreshPlaceables();
        for (int i = 0; i < placeables.Count; i++)
        {
            UnityLearningCityPlaceable other = placeables[i];
            if (other == null || other == placeable || other.width <= 0 || other.length <= 0)
            {
                continue;
            }

            if (RectOverlaps(targetGridX, targetGridY, placeable.width, placeable.length, other.gridX, other.gridY, other.width, other.length))
            {
                if (logInvalid)
                {
                    Debug.LogWarningFormat("[UnityLearningPlacement] invalid: overlap. selected={0} initId={1} type={2} target=({3},{4}) size=({5},{6}) other={7} otherInitId={8} otherType={9} otherGrid=({10},{11}) otherSize=({12},{13})",
                        placeable.name, placeable.initId, placeable.buildingType, targetGridX, targetGridY, placeable.width, placeable.length,
                        other.name, other.initId, other.buildingType, other.gridX, other.gridY, other.width, other.length);
                }
                return false;
            }
        }

        return true;
    }

    private static bool RectOverlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
    {
        return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
    }

    private UnityLearningCityPlaceable RaycastPlaceable(Vector2 screenPosition)
    {
        Camera targetCamera = ResolveCamera();
        if (targetCamera == null)
        {
            return null;
        }

        Ray ray = targetCamera.ScreenPointToRay(screenPosition);
        RaycastHit[] hits = Physics.RaycastAll(ray, 10000.0f);
        if (hits == null || hits.Length == 0)
        {
            return null;
        }

        System.Array.Sort(hits, delegate (RaycastHit a, RaycastHit b)
        {
            return a.distance.CompareTo(b.distance);
        });

        for (int i = 0; i < hits.Length; i++)
        {
            UnityLearningCityPlaceable placeable = hits[i].collider.GetComponentInParent<UnityLearningCityPlaceable>();
            if (placeable != null && placeable.movable)
            {
                return placeable;
            }
        }

        return null;
    }

    private UnityLearningCityPlaceable FirstRaycastReachablePlaceable(Camera targetCamera)
    {
        if (targetCamera == null)
        {
            return null;
        }

        RefreshPlaceables();
        for (int i = 0; i < placeables.Count; i++)
        {
            UnityLearningCityPlaceable placeable = placeables[i];
            Vector2 screenPoint;
            if (placeable == null || !TryGetPlaceableScreenPoint(targetCamera, placeable, out screenPoint))
            {
                continue;
            }

            if (RaycastPlaceable(screenPoint) == placeable)
            {
                return placeable;
            }
        }
        return null;
    }

    private bool TryGetPlaceableScreenPoint(Camera targetCamera, UnityLearningCityPlaceable placeable, out Vector2 screenPoint)
    {
        screenPoint = Vector2.zero;
        if (targetCamera == null || placeable == null)
        {
            return false;
        }

        Collider collider = placeable.GetComponent<Collider>();
        Vector3 worldPoint;
        if (collider != null && collider.enabled)
        {
            worldPoint = collider.bounds.center;
        }
        else
        {
            Bounds bounds;
            if (!TryGetVisualBounds(placeable, out bounds))
            {
                return false;
            }
            worldPoint = bounds.center;
        }

        Vector3 screen = targetCamera.WorldToScreenPoint(worldPoint);
        if (screen.z <= 0.0f)
        {
            return false;
        }

        screenPoint = new Vector2(screen.x, screen.y);
        return true;
    }

    private bool TryFindValidTarget(UnityLearningCityPlaceable placeable, out int targetX, out int targetY)
    {
        targetX = 0;
        targetY = 0;
        if (placeable == null)
        {
            return false;
        }

        int sourceX = placeable.gridX;
        int sourceY = placeable.gridY;
        for (int radius = 1; radius <= 12; radius++)
        {
            for (int dx = -radius; dx <= radius; dx++)
            {
                for (int dy = -radius; dy <= radius; dy++)
                {
                    if (Mathf.Abs(dx) != radius && Mathf.Abs(dy) != radius)
                    {
                        continue;
                    }

                    int candidateX = sourceX + dx;
                    int candidateY = sourceY + dy;
                    if (!IsPlacementValid(placeable, candidateX, candidateY))
                    {
                        continue;
                    }

                    targetX = candidateX;
                    targetY = candidateY;
                    return true;
                }
            }
        }
        return false;
    }

    private bool TryFindOverlappingInvalidTarget(UnityLearningCityPlaceable placeable, out int targetX, out int targetY)
    {
        targetX = 0;
        targetY = 0;
        if (placeable == null)
        {
            return false;
        }

        RefreshPlaceables();
        for (int i = 0; i < placeables.Count; i++)
        {
            UnityLearningCityPlaceable other = placeables[i];
            if (other == null || other == placeable || other.width <= 0 || other.length <= 0)
            {
                continue;
            }

            if (IsPlacementValid(placeable, other.gridX, other.gridY))
            {
                continue;
            }

            targetX = other.gridX;
            targetY = other.gridY;
            return true;
        }
        return false;
    }

    private bool TryGetDragTargetScreen(Camera targetCamera, UnityLearningCityPlaceable placeable, Vector2 dragStartScreen, int targetGridX, int targetGridY, out Vector2 dragTargetScreen)
    {
        dragTargetScreen = Vector2.zero;
        if (targetCamera == null || placeable == null)
        {
            return false;
        }

        float startGridXFloat;
        float startGridYFloat;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        if (ScreenToGridOriginFloat(dragStartScreen, placeable, out startGridXFloat, out startGridYFloat))
        {
            offsetX = startGridXFloat - placeable.gridX;
            offsetY = startGridYFloat - placeable.gridY;
        }

        float axisXDistance = (targetGridX + offsetX) * cellSize + FootprintHalfOffset(placeable.width);
        float axisYDistance = (targetGridY + offsetY) * cellSize + FootprintHalfOffset(placeable.length);
        Vector3 localPoint = GridSpaceToLocal(axisXDistance, axisYDistance, 0.0f);
        Vector3 worldPoint = buildingRoot != null ? buildingRoot.TransformPoint(localPoint) : transform.TransformPoint(localPoint);
        Vector3 screen = targetCamera.WorldToScreenPoint(worldPoint);
        if (screen.z <= 0.0f)
        {
            return false;
        }

        dragTargetScreen = new Vector2(screen.x, screen.y);
        if ((dragTargetScreen - dragStartScreen).sqrMagnitude < dragThresholdPixels * dragThresholdPixels)
        {
            dragTargetScreen += Vector2.right * (dragThresholdPixels + 2.0f);
        }
        return true;
    }

    private bool ScreenToGround(Vector2 screenPosition, out Vector3 worldPoint)
    {
        worldPoint = Vector3.zero;
        Camera targetCamera = ResolveCamera();
        if (targetCamera == null)
        {
            return false;
        }

        Plane ground = new Plane(Vector3.up, Vector3.zero);
        Ray ray = targetCamera.ScreenPointToRay(screenPosition);
        float enter;
        if (!ground.Raycast(ray, out enter))
        {
            return false;
        }

        worldPoint = ray.GetPoint(enter);
        return true;
    }

    private bool ScreenToGridOriginFloat(Vector2 screenPosition, UnityLearningCityPlaceable placeable, out float gridXFloat, out float gridYFloat)
    {
        gridXFloat = 0.0f;
        gridYFloat = 0.0f;
        if (placeable == null)
        {
            return false;
        }

        Vector3 worldPoint;
        if (!ScreenToGround(screenPosition, out worldPoint))
        {
            return false;
        }

        Vector3 localPoint = buildingRoot != null ? buildingRoot.InverseTransformPoint(worldPoint) : transform.InverseTransformPoint(worldPoint);
        Vector3 delta = localPoint - GridOriginLocal();
        float axisX = Vector3.Dot(delta, GridAxisXLocal());
        float axisY = Vector3.Dot(delta, GridAxisYLocal());
        gridXFloat = (axisX - FootprintHalfOffset(placeable.width)) / cellSize;
        gridYFloat = (axisY - FootprintHalfOffset(placeable.length)) / cellSize;
        return true;
    }

    private int SnapGridFromReferenceRule(float gridValue)
    {
        return Mathf.FloorToInt(gridValue + 0.05f);
    }

    private Camera ResolveCamera()
    {
        if (sceneCamera != null)
        {
            return sceneCamera;
        }

        if (Camera.main != null)
        {
            sceneCamera = Camera.main;
            return sceneCamera;
        }

        sceneCamera = FindObjectOfType<Camera>();
        return sceneCamera;
    }

    private void RefreshPlaceables()
    {
        placeables.Clear();
        UnityLearningCityPlaceable[] found;
        if (buildingRoot != null)
        {
            found = buildingRoot.GetComponentsInChildren<UnityLearningCityPlaceable>(true);
        }
        else
        {
            found = FindObjectsOfType<UnityLearningCityPlaceable>();
        }

        for (int i = 0; i < found.Length; i++)
        {
            if (found[i] != null && found[i].movable && found[i].gameObject.activeInHierarchy)
            {
                found[i].SetGridBasis(GridOriginLocal(), GridAxisXLocal(), GridAxisYLocal());
                placeables.Add(found[i]);
            }
        }
    }

    private void EnsureFeedback()
    {
        if (feedbackRoot != null)
        {
            return;
        }

        feedbackRoot = FindSceneGameObject("UnityLearningPlacementFeedback_Runtime");
        if (feedbackRoot == null)
        {
            feedbackRoot = new GameObject("UnityLearningPlacementFeedback_Runtime");
        }

        feedbackRoot.transform.SetParent(transform, false);
        DestroyLegacyMoveArrows(feedbackRoot.transform);
        if (buildingRoot != null)
        {
            DestroyLegacyMoveArrows(buildingRoot);
        }

        Transform fill = feedbackRoot.transform.Find("FootprintFill");
        if (fill != null)
        {
            DestroyUnityObject(fill.gameObject);
        }

        Transform border = feedbackRoot.transform.Find("FootprintBorder");
        if (border != null)
        {
            DestroyUnityObject(border.gameObject);
        }

        validMaterial = CreateOverlayFeedbackMaterial("UnityLearningPlacementValid", new Color(0.0f, 1.0f, 0.12f, 0.95f));
        invalidMaterial = CreateOverlayFeedbackMaterial("UnityLearningPlacementInvalid", new Color(1.0f, 0.0f, 0.0f, 0.95f));
        gridMaterial = CreateFeedbackMaterial("UnityLearningPlacementGrid", new Color(1.0f, 1.0f, 1.0f, 0.24f));
        arrowMaterial = CreateOverlayFeedbackMaterial("UnityLearningPlacementArrow", new Color(1.0f, 1.0f, 1.0f, 1.0f));
        arrowMaterial.renderQueue = 5000;
        EnsureMoveArrowSprites();

        gridRoot = EnsureChildTransform("CityMoveGrid_From_GridLinesReference");
        CreateGridLines();
        moveArrowRoot = EnsureMoveArrowRoot();

        for (int i = 0; i < footprintBorders.Length; i++)
        {
            footprintBorders[i] = EnsureFeedbackQuad("FootprintBorder_" + i);
        }

        for (int i = 0; i < moveArrows.Length; i++)
        {
            moveArrows[i] = EnsureMoveArrow("arrow" + (i + 1));
            moveArrows[i].SetParent(moveArrowRoot, false);
        }

        for (int i = 0; i < footprintBorders.Length; i++)
        {
            Renderer borderRenderer = footprintBorders[i].GetComponent<Renderer>();
            if (borderRenderer != null)
            {
                borderRenderer.sharedMaterial = validMaterial;
                borderRenderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
                borderRenderer.receiveShadows = false;
            }
        }
        for (int i = 0; i < moveArrows.Length; i++)
        {
            Renderer arrowRenderer = moveArrows[i].GetComponent<Renderer>();
            if (arrowRenderer != null)
            {
                arrowRenderer.sharedMaterial = arrowMaterial;
                arrowRenderer.enabled = false;
                arrowRenderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
                arrowRenderer.receiveShadows = false;
            }
        }
    }

    private Transform EnsureChildTransform(string objectName)
    {
        Transform existing = feedbackRoot.transform.Find(objectName);
        if (existing != null)
        {
            return existing;
        }

        GameObject child = new GameObject(objectName);
        child.transform.SetParent(feedbackRoot.transform, false);
        return child.transform;
    }

    private Transform EnsureMoveArrowRoot()
    {
        Transform parent = buildingRoot != null ? buildingRoot : feedbackRoot.transform;
        if (feedbackRoot != null && feedbackRoot.transform != parent)
        {
            Transform staleRoot = feedbackRoot.transform.Find("building_move_arrow");
            if (staleRoot != null)
            {
                DestroyUnityObject(staleRoot.gameObject);
            }
        }

        Transform existing = parent.Find("building_move_arrow");
        if (existing != null)
        {
            return existing;
        }

        GameObject root = new GameObject("building_move_arrow");
        root.transform.SetParent(parent, false);
        return root.transform;
    }

    private void DestroyLegacyMoveArrows(Transform parent)
    {
        if (parent == null)
        {
            return;
        }

        for (int i = parent.childCount - 1; i >= 0; i--)
        {
            Transform child = parent.GetChild(i);
            DestroyLegacyMoveArrows(child);
            if (child != null && child.name.StartsWith("MoveArrow_"))
            {
                DestroyUnityObject(child.gameObject);
            }
        }
    }

    private GameObject FindSceneGameObject(string objectName)
    {
        GameObject[] allObjects = Resources.FindObjectsOfTypeAll<GameObject>();
        for (int i = 0; i < allObjects.Length; i++)
        {
            GameObject go = allObjects[i];
            if (go != null && go.scene.IsValid() && go.name == objectName)
            {
                return go;
            }
        }
        return null;
    }

    private Transform EnsureFeedbackQuad(string objectName)
    {
        Transform existing = feedbackRoot.transform.Find(objectName);
        if (existing != null)
        {
            return existing;
        }

        GameObject quad = GameObject.CreatePrimitive(PrimitiveType.Quad);
        quad.name = objectName;
        quad.transform.SetParent(feedbackRoot.transform, false);
        Collider collider = quad.GetComponent<Collider>();
        if (collider != null)
        {
            DestroyUnityObject(collider);
        }
        return quad.transform;
    }

    private Transform EnsureGridLine(string objectName)
    {
        Transform existing = gridRoot.Find(objectName);
        if (existing != null)
        {
            return existing;
        }

        GameObject quad = GameObject.CreatePrimitive(PrimitiveType.Quad);
        quad.name = objectName;
        quad.transform.SetParent(gridRoot, false);
        Collider collider = quad.GetComponent<Collider>();
        if (collider != null)
        {
            DestroyUnityObject(collider);
        }
        Renderer renderer = quad.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
            renderer.receiveShadows = false;
        }
        return quad.transform;
    }

    private Transform EnsureMoveArrow(string objectName)
    {
        Transform existing = moveArrowRoot != null ? moveArrowRoot.Find(objectName) : feedbackRoot.transform.Find(objectName);
        if (existing != null)
        {
            ConfigureMoveArrowSprite(existing, objectName);
            return existing;
        }

        GameObject arrow = new GameObject(objectName);
        arrow.transform.SetParent(moveArrowRoot != null ? moveArrowRoot : feedbackRoot.transform, false);
        ConfigureMoveArrowSprite(arrow.transform, objectName);
        return arrow.transform;
    }

    private void ConfigureMoveArrowSprite(Transform arrow, string objectName)
    {
        MeshFilter meshFilter = arrow.GetComponent<MeshFilter>();
        if (meshFilter != null)
        {
            DestroyUnityObject(meshFilter);
        }
        MeshRenderer renderer = arrow.GetComponent<MeshRenderer>();
        if (renderer != null)
        {
            DestroyUnityObject(renderer);
        }

        SpriteRenderer spriteRenderer = arrow.GetComponent<SpriteRenderer>();
        if (spriteRenderer == null)
        {
            spriteRenderer = arrow.gameObject.AddComponent<SpriteRenderer>();
        }

        EnsureMoveArrowSprites();
        int index = MoveArrowIndexFromName(objectName);
        spriteRenderer.sprite = index >= 0 && index < moveArrowSprites.Length ? moveArrowSprites[index] : null;
        spriteRenderer.sharedMaterial = arrowMaterial;
        spriteRenderer.color = Color.white;
        spriteRenderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
        spriteRenderer.receiveShadows = false;
        spriteRenderer.sortingOrder = 500;
        spriteRenderer.enabled = false;
    }

    private int MoveArrowIndexFromName(string objectName)
    {
        if (objectName == "arrow1") return 0;
        if (objectName == "arrow2") return 1;
        if (objectName == "arrow3") return 2;
        if (objectName == "arrow4") return 3;
        return -1;
    }

    private void EnsureMoveArrowSprites()
    {
        bool loaded = true;
        for (int i = 0; i < moveArrowSprites.Length; i++)
        {
            if (moveArrowSprites[i] == null)
            {
                loaded = false;
                break;
            }
        }
        if (loaded)
        {
            return;
        }

        Sprite[] sprites = Resources.LoadAll<Sprite>(MoveArrowAtlasResourcePath);
        for (int i = 0; i < MoveArrowSpriteNames.Length; i++)
        {
            moveArrowSprites[i] = null;
            for (int j = 0; j < sprites.Length; j++)
            {
                if (sprites[j] != null && sprites[j].name == MoveArrowSpriteNames[i])
                {
                    moveArrowSprites[i] = sprites[j];
                    break;
                }
            }
        }
    }

    private void CreateGridLines()
    {
        if (gridRoot == null)
        {
            return;
        }

        gridLines.Clear();
        float start = (cityMinGrid - 0.5f) * cellSize;
        float end = (cityMaxGrid + 0.5f) * cellSize;
        float length = end - start;
        float center = (start + end) * 0.5f;
        float width = Mathf.Max(0.012f, cellSize * 0.026f);
        float y = 0.075f;
        int index = 0;

        for (int grid = cityMinGrid; grid <= cityMaxGrid + 1; grid++)
        {
            float line = (grid - 0.5f) * cellSize;
            Transform xLine = EnsureGridLine("GridLine_X_" + index);
            SetHorizontalQuad(xLine, LocalToFeedbackWorld(GridSpaceToLocal(center, line, y)), new Vector3(length, width, 1.0f), gridMaterial);
            gridLines.Add(xLine);

            Transform zLine = EnsureGridLine("GridLine_Z_" + index);
            SetHorizontalQuad(zLine, LocalToFeedbackWorld(GridSpaceToLocal(line, center, y)), new Vector3(width, length, 1.0f), gridMaterial);
            gridLines.Add(zLine);
            index++;
        }
    }

    private void DestroyUnityObject(UnityEngine.Object target)
    {
        if (target == null)
        {
            return;
        }
        if (Application.isPlaying)
        {
            Destroy(target);
        }
        else
        {
            DestroyImmediate(target);
        }
    }

    private Material CreateFeedbackMaterial(string materialName, Color color)
    {
        Shader shader = Shader.Find("Sprites/Default");
        if (shader == null)
        {
            shader = Shader.Find("Unlit/Transparent");
        }
        if (shader == null)
        {
            shader = Shader.Find("Particles/Alpha Blended");
        }
        if (shader == null)
        {
            shader = Shader.Find("Unlit/Texture");
        }

        Material material = new Material(shader);
        material.name = materialName;
        Texture2D texture = new Texture2D(1, 1, TextureFormat.RGBA32, false);
        texture.hideFlags = HideFlags.HideAndDontSave;
        texture.SetPixel(0, 0, Color.white);
        texture.Apply();
        material.mainTexture = texture;
        material.color = color;
        if (material.HasProperty("_Color"))
        {
            material.SetColor("_Color", color);
        }
        if (material.HasProperty("_TintColor"))
        {
            material.SetColor("_TintColor", color);
        }
        if (material.HasProperty("_Cull"))
        {
            material.SetFloat("_Cull", 0.0f);
        }
        material.renderQueue = 3050;
        return material;
    }

    private Material CreateOverlayFeedbackMaterial(string materialName, Color color)
    {
        Shader shader = Shader.Find("GUI/Text Shader");
        if (shader == null)
        {
            shader = Shader.Find("Sprites/Default");
        }
        Material material = CreateMaterialWithTexture(shader, materialName, color);
        material.renderQueue = 4000;
        return material;
    }

    private Material CreateMaterialWithTexture(Shader shader, string materialName, Color color)
    {
        if (shader == null)
        {
            shader = Shader.Find("Unlit/Transparent");
        }
        if (shader == null)
        {
            shader = Shader.Find("Unlit/Texture");
        }

        Material material = new Material(shader);
        material.name = materialName;
        Texture2D texture = new Texture2D(1, 1, TextureFormat.RGBA32, false);
        texture.hideFlags = HideFlags.HideAndDontSave;
        texture.SetPixel(0, 0, Color.white);
        texture.Apply();
        material.mainTexture = texture;
        material.color = color;
        if (material.HasProperty("_Color"))
        {
            material.SetColor("_Color", color);
        }
        if (material.HasProperty("_TintColor"))
        {
            material.SetColor("_TintColor", color);
        }
        if (material.HasProperty("_Cull"))
        {
            material.SetFloat("_Cull", 0.0f);
        }
        return material;
    }

    private void ShowFeedback(UnityLearningCityPlaceable placeable, int targetGridX, int targetGridY, bool valid)
    {
        if (placeable == null)
        {
            HideFeedback();
            return;
        }

        EnsureFeedback();
        feedbackRoot.SetActive(true);

        Vector3 centerLocal = FeedbackGridCenterLocal(placeable, targetGridX, targetGridY);
        Vector3 centerWorld = buildingRoot != null ? buildingRoot.TransformPoint(centerLocal) : transform.TransformPoint(centerLocal);
        float sizeX = Mathf.Max(1, placeable.width) * cellSize;
        float sizeZ = Mathf.Max(1, placeable.length) * cellSize;
        float halfX = sizeX * 0.5f;
        float halfZ = sizeZ * 0.5f;
        float y = Mathf.Max(0.08f, centerWorld.y + 0.08f);
        Vector3 axisXWorld = GridAxisXWorld();
        Vector3 axisYWorld = GridAxisYWorld();

        Material material = valid ? validMaterial : invalidMaterial;
        float borderWidth = 0.08f;
        Vector3 liftedCenter = new Vector3(centerWorld.x, y + 0.02f, centerWorld.z);
        SetHorizontalQuad(footprintBorders[0], liftedCenter - axisYWorld * halfZ, new Vector3(sizeX + borderWidth, borderWidth, 1.0f), material);
        SetHorizontalQuad(footprintBorders[1], liftedCenter + axisYWorld * halfZ, new Vector3(sizeX + borderWidth, borderWidth, 1.0f), material);
        SetHorizontalQuad(footprintBorders[2], liftedCenter - axisXWorld * halfX, new Vector3(borderWidth, sizeZ + borderWidth, 1.0f), material);
        SetHorizontalQuad(footprintBorders[3], liftedCenter + axisXWorld * halfX, new Vector3(borderWidth, sizeZ + borderWidth, 1.0f), material);

        Vector3[] footprintCorners = new Vector3[]
        {
            liftedCenter - axisXWorld * halfX - axisYWorld * halfZ,
            liftedCenter + axisXWorld * halfX - axisYWorld * halfZ,
            liftedCenter - axisXWorld * halfX + axisYWorld * halfZ,
            liftedCenter + axisXWorld * halfX + axisYWorld * halfZ
        };
        guiFootprintVisible = false;
        guiArrowsVisible = false;
        SetMoveArrowsFromReferenceRule(placeable, targetGridX, targetGridY);
    }

    private void SetGuiArrow(int index, Vector2 screenPosition, float rotation)
    {
        if (index < 0 || index >= guiArrowPositions.Length)
        {
            return;
        }
        guiArrowPositions[index] = screenPosition;
        guiArrowRotations[index] = rotation;
    }

    private bool TryGetScreenBounds(Camera camera, Bounds bounds, out Rect rect, out float depth)
    {
        Vector3 min = bounds.min;
        Vector3 max = bounds.max;
        Vector3[] corners = new Vector3[]
        {
            new Vector3(min.x, min.y, min.z),
            new Vector3(min.x, min.y, max.z),
            new Vector3(min.x, max.y, min.z),
            new Vector3(min.x, max.y, max.z),
            new Vector3(max.x, min.y, min.z),
            new Vector3(max.x, min.y, max.z),
            new Vector3(max.x, max.y, min.z),
            new Vector3(max.x, max.y, max.z)
        };
        return TryGetScreenBounds(camera, corners, out rect, out depth);
    }

    private bool TryGetScreenBounds(Camera camera, Vector3[] corners, out Rect rect, out float depth)
    {
        rect = new Rect();
        depth = 0.0f;
        if (camera == null || corners == null || corners.Length == 0)
        {
            return false;
        }

        bool hasPoint = false;
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        float depthSum = 0.0f;
        int depthCount = 0;
        for (int i = 0; i < corners.Length; i++)
        {
            Vector3 screen = camera.WorldToScreenPoint(corners[i]);
            if (screen.z <= 0.0f)
            {
                continue;
            }

            if (!hasPoint)
            {
                minX = maxX = screen.x;
                minY = maxY = screen.y;
                hasPoint = true;
            }
            else
            {
                minX = Mathf.Min(minX, screen.x);
                minY = Mathf.Min(minY, screen.y);
                maxX = Mathf.Max(maxX, screen.x);
                maxY = Mathf.Max(maxY, screen.y);
            }
            depthSum += screen.z;
            depthCount++;
        }

        if (!hasPoint || depthCount == 0)
        {
            return false;
        }

        rect = Rect.MinMaxRect(minX, minY, maxX, maxY);
        depth = depthSum / depthCount;
        return true;
    }

    private Vector2 ClampArrowScreen(Camera camera, Vector2 screenPosition)
    {
        float margin = 48.0f;
        float width = camera != null ? Mathf.Max(margin * 2.0f, camera.pixelWidth) : Screen.width;
        float height = camera != null ? Mathf.Max(margin * 2.0f, camera.pixelHeight) : Screen.height;
        return new Vector2(
            Mathf.Clamp(screenPosition.x, margin, width - margin),
            Mathf.Clamp(screenPosition.y, margin, height - margin));
    }

    private Vector3 GridOriginLocal()
    {
        return Vector3.zero;
    }

    private Vector3 GridAxisXLocal()
    {
        return (Quaternion.AngleAxis(gridYawDegrees, Vector3.up) * Vector3.right).normalized;
    }

    private Vector3 GridAxisYLocal()
    {
        return (Quaternion.AngleAxis(gridYawDegrees, Vector3.up) * Vector3.forward).normalized;
    }

    private Vector3 GridAxisXWorld()
    {
        Vector3 axis = buildingRoot != null ? buildingRoot.TransformDirection(GridAxisXLocal()) : transform.TransformDirection(GridAxisXLocal());
        axis.y = 0.0f;
        return axis.sqrMagnitude > 0.0001f ? axis.normalized : Vector3.right;
    }

    private Vector3 GridAxisYWorld()
    {
        Vector3 axis = buildingRoot != null ? buildingRoot.TransformDirection(GridAxisYLocal()) : transform.TransformDirection(GridAxisYLocal());
        axis.y = 0.0f;
        return axis.sqrMagnitude > 0.0001f ? axis.normalized : Vector3.forward;
    }

    private Vector3 GridSpaceToLocal(float axisXDistance, float axisYDistance, float y)
    {
        Vector3 planar = GridOriginLocal() + GridAxisXLocal() * axisXDistance + GridAxisYLocal() * axisYDistance;
        return new Vector3(planar.x, y, planar.z);
    }

    private Quaternion GridQuadRotation()
    {
        return Quaternion.AngleAxis(gridYawDegrees, Vector3.up) * Quaternion.Euler(90.0f, 0.0f, 0.0f);
    }

    private bool TryGetVisualBounds(UnityLearningCityPlaceable placeable, out Bounds bounds)
    {
        bounds = new Bounds(Vector3.zero, Vector3.zero);
        if (placeable == null)
        {
            return false;
        }

        Collider collider = placeable.GetComponent<Collider>();
        if (collider != null && collider.enabled)
        {
            bounds = collider.bounds;
            return true;
        }

        Renderer[] renderers = placeable.GetComponentsInChildren<Renderer>(true);
        bool hasBounds = false;
        for (int i = 0; i < renderers.Length; i++)
        {
            Renderer renderer = renderers[i];
            if (renderer == null || !renderer.enabled)
            {
                continue;
            }
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
        return hasBounds;
    }

    private void SetHorizontalQuad(Transform quad, Vector3 position, Vector3 scale, Material material)
    {
        if (quad == null)
        {
            return;
        }

        quad.position = position;
        quad.rotation = GridQuadRotation();
        quad.localScale = scale;
        Renderer renderer = quad.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.sharedMaterial = material;
            renderer.enabled = true;
        }
    }

    private void SetFootprintVisible(bool visible)
    {
        for (int i = 0; i < footprintBorders.Length; i++)
        {
            if (footprintBorders[i] == null)
            {
                continue;
            }

            Renderer renderer = footprintBorders[i].GetComponent<Renderer>();
            if (renderer != null)
            {
                renderer.enabled = visible;
            }
        }
    }

    private void SetMoveArrow(int index, Vector3 position, float screenRotation)
    {
        if (index < 0 || index >= moveArrows.Length || moveArrows[index] == null)
        {
            return;
        }

        Camera targetCamera = ResolveCamera();
        Transform arrow = moveArrows[index];
        float scaleCompensation = 1.0f;
        if (targetCamera != null)
        {
            Vector3 screenPoint = targetCamera.WorldToScreenPoint(position);
            if (screenPoint.z > 1.0f)
            {
                float originalDepth = screenPoint.z;
                screenPoint.z = Mathf.Max(1.0f, originalDepth - 14.0f);
                position = targetCamera.ScreenToWorldPoint(screenPoint);
                scaleCompensation = screenPoint.z / originalDepth;
            }
        }
        arrow.position = position;
        if (targetCamera != null)
        {
            arrow.rotation = Quaternion.LookRotation(-targetCamera.transform.forward, targetCamera.transform.up) * Quaternion.Euler(0.0f, 0.0f, screenRotation);
        }
        else
        {
            arrow.rotation = Quaternion.Euler(45.0f, 0.0f, screenRotation);
        }
        arrow.localScale = Vector3.one * Mathf.Max(0.72f, cellSize * 1.35f) * scaleCompensation;
        Renderer renderer = arrow.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.sharedMaterial = arrowMaterial;
            renderer.enabled = true;
        }
    }

    private void SetMoveArrowsFromReferenceRule(UnityLearningCityPlaceable placeable, int targetGridX, int targetGridY)
    {
        if (placeable == null)
        {
            return;
        }

        if (moveArrowRoot == null || (buildingRoot != null && moveArrowRoot.parent != buildingRoot))
        {
            moveArrowRoot = EnsureMoveArrowRoot();
        }

        moveArrowRoot.localPosition = FeedbackGridCenterLocal(placeable, targetGridX, targetGridY);
        moveArrowRoot.localRotation = Quaternion.AngleAxis(gridYawDegrees, Vector3.up);
        moveArrowRoot.localScale = Vector3.one;
        moveArrowRoot.gameObject.SetActive(true);

        Vector2 size = new Vector2(Mathf.Max(1, placeable.width), Mathf.Max(1, placeable.length));
        float sourceCellHalfSize = placeable.cellSize * 0.5f;
        float arrowScale = placeable.cellSize;
        float w = (size.x + 1.0f) * sourceCellHalfSize;
        float h = (size.y + 1.0f) * sourceCellHalfSize;
        SetReferenceMoveArrow(0, new Vector3(0.0f, 0.0f, w), arrowScale);
        SetReferenceMoveArrow(1, new Vector3(h, 0.0f, 0.0f), arrowScale);
        SetReferenceMoveArrow(2, new Vector3(0.0f, 0.0f, -w), arrowScale);
        SetReferenceMoveArrow(3, new Vector3(-h, 0.0f, 0.0f), arrowScale);
    }

    private void SetReferenceMoveArrow(int index, Vector3 localPosition, float arrowScale)
    {
        if (index < 0 || index >= moveArrows.Length || moveArrows[index] == null)
        {
            return;
        }

        Transform arrow = moveArrows[index];
        arrow.localPosition = localPosition;
        arrow.localRotation = Quaternion.Euler(90.0f, 0.0f, 0.0f);
        arrow.localScale = Vector3.one * arrowScale;

        SpriteRenderer renderer = arrow.GetComponent<SpriteRenderer>();
        if (renderer != null)
        {
            renderer.sprite = index < moveArrowSprites.Length ? moveArrowSprites[index] : null;
            renderer.sharedMaterial = arrowMaterial;
            renderer.enabled = renderer.sprite != null;
            renderer.sortingOrder = 500;
        }
    }

    private Vector3 FeedbackGridCenterLocal(UnityLearningCityPlaceable placeable, int targetGridX, int targetGridY)
    {
        Vector3 center = placeable.GridCenterLocal(targetGridX, targetGridY);
        center.y = Mathf.Max(center.y, Mathf.Max(0.12f, placeable.cellSize * 0.24f));
        return center;
    }

    private void SetMoveArrowScreen(int index, Vector2 screenPosition, float sourceDepth, float screenRotation)
    {
        if (index < 0 || index >= moveArrows.Length || moveArrows[index] == null)
        {
            return;
        }

        Camera targetCamera = ResolveCamera();
        if (targetCamera == null)
        {
            return;
        }

        float depth = Mathf.Max(1.0f, sourceDepth * 0.45f);
        Vector3 position = targetCamera.ScreenToWorldPoint(new Vector3(screenPosition.x, screenPosition.y, depth));
        Transform arrow = moveArrows[index];
        arrow.position = position;
        arrow.rotation = Quaternion.LookRotation(-targetCamera.transform.forward, targetCamera.transform.up) * Quaternion.Euler(0.0f, 0.0f, screenRotation);

        float pixelHeight = Mathf.Max(1.0f, targetCamera.pixelHeight);
        float worldHeightAtDepth = 2.0f * depth * Mathf.Tan(targetCamera.fieldOfView * 0.5f * Mathf.Deg2Rad);
        float desiredWorldHeight = worldHeightAtDepth * 38.0f / pixelHeight;
        arrow.localScale = Vector3.one * Mathf.Max(0.08f, desiredWorldHeight / 0.58f);

        Renderer renderer = arrow.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.sharedMaterial = arrowMaterial;
        }
    }

    private void HideFeedback()
    {
        guiArrowsVisible = false;
        guiFootprintVisible = false;
        if (feedbackRoot != null)
        {
            feedbackRoot.SetActive(false);
        }
    }

    private bool TrySetGuiFootprint(Camera camera, Vector3[] corners, Color color)
    {
        if (camera == null || corners == null || corners.Length < 4)
        {
            return false;
        }

        for (int i = 0; i < 4; i++)
        {
            Vector3 screen = camera.WorldToScreenPoint(corners[i]);
            if (screen.z <= 0.0f)
            {
                return false;
            }
            guiFootprintCorners[i] = new Vector2(screen.x, screen.y);
        }

        guiFootprintColor = color;
        return true;
    }

    private void DrawGuiFootprint(Event current)
    {
        if (!guiFootprintVisible || current == null || current.type != EventType.Repaint)
        {
            return;
        }

        EnsureGuiLineTexture();
        if (guiLineTexture == null)
        {
            return;
        }

        Color previousColor = GUI.color;
        GUI.color = guiFootprintColor;
        DrawGuiLine(guiFootprintCorners[0], guiFootprintCorners[1], 5.0f);
        DrawGuiLine(guiFootprintCorners[1], guiFootprintCorners[3], 5.0f);
        DrawGuiLine(guiFootprintCorners[3], guiFootprintCorners[2], 5.0f);
        DrawGuiLine(guiFootprintCorners[2], guiFootprintCorners[0], 5.0f);
        GUI.color = previousColor;
    }

    private void DrawGuiLine(Vector2 startScreen, Vector2 endScreen, float width)
    {
        Vector2 start = new Vector2(startScreen.x, Screen.height - startScreen.y);
        Vector2 end = new Vector2(endScreen.x, Screen.height - endScreen.y);
        Vector2 delta = end - start;
        float length = delta.magnitude;
        if (length <= 0.01f)
        {
            return;
        }

        Matrix4x4 oldMatrix = GUI.matrix;
        float angle = Mathf.Atan2(delta.y, delta.x) * Mathf.Rad2Deg;
        GUIUtility.RotateAroundPivot(angle, start);
        GUI.DrawTexture(new Rect(start.x, start.y - width * 0.5f, length, width), guiLineTexture, ScaleMode.StretchToFill, true);
        GUI.matrix = oldMatrix;
    }

    private void DrawGuiArrows(Event current)
    {
        if (!guiArrowsVisible || current == null || current.type != EventType.Repaint)
        {
            return;
        }

        EnsureGuiArrowTexture();
        if (guiArrowTexture == null)
        {
            return;
        }

        float size = 62.0f;
        Color previousColor = GUI.color;
        GUI.color = Color.white;
        for (int i = 0; i < guiArrowPositions.Length; i++)
        {
            Vector2 position = guiArrowPositions[i];
            Rect rect = new Rect(position.x - size * 0.5f, Screen.height - position.y - size * 0.5f, size, size);
            Matrix4x4 oldMatrix = GUI.matrix;
            GUIUtility.RotateAroundPivot(guiArrowRotations[i], rect.center);
            GUI.DrawTexture(rect, guiArrowTexture, ScaleMode.StretchToFill, true);
            GUI.matrix = oldMatrix;
        }
        GUI.color = previousColor;
    }

    private void EnsureGuiArrowTexture()
    {
        if (guiArrowTexture != null)
        {
            return;
        }

        int size = 64;
        guiArrowTexture = new Texture2D(size, size, TextureFormat.RGBA32, false);
        guiArrowTexture.hideFlags = HideFlags.HideAndDontSave;
        Color clear = new Color(1.0f, 1.0f, 1.0f, 0.0f);
        Color white = Color.white;
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                float row = y / (float)(size - 1);
                float halfWidth = Mathf.Lerp(0.48f, 0.03f, row);
                float nx = Mathf.Abs((x / (float)(size - 1)) - 0.5f);
                guiArrowTexture.SetPixel(x, y, nx <= halfWidth && row > 0.08f ? white : clear);
            }
        }
        guiArrowTexture.Apply();
    }

    private void EnsureGuiLineTexture()
    {
        if (guiLineTexture != null)
        {
            return;
        }

        guiLineTexture = new Texture2D(1, 1, TextureFormat.RGBA32, false);
        guiLineTexture.hideFlags = HideFlags.HideAndDontSave;
        guiLineTexture.SetPixel(0, 0, Color.white);
        guiLineTexture.Apply();
    }

    private float FootprintHalfOffset(int cellCount)
    {
        return cellCount * cellSize * 0.5f - cellSize * 0.5f;
    }

    private Vector3 LocalToFeedbackWorld(Vector3 local)
    {
        return buildingRoot != null ? buildingRoot.TransformPoint(local) : transform.TransformPoint(local);
    }

    private Vector2 GuiPointToScreen(Vector2 guiPoint)
    {
        return new Vector2(guiPoint.x, Screen.height - guiPoint.y);
    }
}
