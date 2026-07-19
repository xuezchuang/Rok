using UnityEngine;

[ExecuteInEditMode]
public sealed class UnityLearningCityPlaceable : MonoBehaviour
{
    public int initId;
    public int buildingType;
    public int gridX;
    public int gridY;
    public int width;
    public int length;
    public bool movable = true;
    public float cellSize = 0.5f;
    public Vector3 originalLocalPosition;
    public Vector3 originalGridCenterLocal;
    public Vector3 gridOriginLocal = Vector3.zero;
    public Vector3 gridAxisXLocal = Vector3.right;
    public Vector3 gridAxisYLocal = Vector3.forward;

    private Renderer[] cachedRenderers;
    private MaterialPropertyBlock propertyBlock;

    public void Configure(int sourceInitId, int sourceBuildingType, int sourceGridX, int sourceGridY, int sourceWidth, int sourceLength, float sourceCellSize, bool sourceMovable)
    {
        initId = sourceInitId;
        buildingType = sourceBuildingType;
        gridX = sourceGridX;
        gridY = sourceGridY;
        width = Mathf.Max(0, sourceWidth);
        length = Mathf.Max(0, sourceLength);
        cellSize = sourceCellSize;
        movable = sourceMovable && width > 0 && length > 0;
        originalLocalPosition = transform.localPosition;
        originalGridCenterLocal = GridCenterLocal(gridX, gridY);
        RefreshRenderers();
    }

    public void SetGridBasis(Vector3 originLocal, Vector3 axisXLocal, Vector3 axisYLocal)
    {
        gridOriginLocal = originLocal;
        gridAxisXLocal = NormalizePlanar(axisXLocal, Vector3.right);
        gridAxisYLocal = NormalizePlanar(axisYLocal, Vector3.forward);
        originalGridCenterLocal = GridCenterLocal(gridX, gridY);
    }

    public void RefreshRenderers()
    {
        cachedRenderers = GetComponentsInChildren<Renderer>(true);
    }

    public Vector3 GridCenterLocal(int sourceGridX, int sourceGridY)
    {
        float x = sourceGridX * cellSize + width * cellSize * 0.5f - cellSize * 0.5f;
        float z = sourceGridY * cellSize + length * cellSize * 0.5f - cellSize * 0.5f;
        Vector3 planar = gridOriginLocal + gridAxisXLocal * x + gridAxisYLocal * z;
        return new Vector3(planar.x, originalLocalPosition.y, planar.z);
    }

    public void PreviewGrid(int targetGridX, int targetGridY)
    {
        Vector3 targetCenter = GridCenterLocal(targetGridX, targetGridY);
        transform.localPosition = originalLocalPosition + (targetCenter - originalGridCenterLocal);
        RefreshSortingOrder();
    }

    public void CommitGrid(int targetGridX, int targetGridY)
    {
        PreviewGrid(targetGridX, targetGridY);
        gridX = targetGridX;
        gridY = targetGridY;
        originalLocalPosition = transform.localPosition;
        originalGridCenterLocal = GridCenterLocal(gridX, gridY);
        RefreshSortingOrder();
    }

    public void CancelPreview()
    {
        transform.localPosition = originalLocalPosition;
        RefreshSortingOrder();
    }

    public void RefreshSortingOrder()
    {
        if (cachedRenderers == null || cachedRenderers.Length == 0)
        {
            RefreshRenderers();
        }

        int order = 30000 + 1200 - Mathf.RoundToInt(transform.position.z * 100.0f) - Mathf.RoundToInt(transform.position.y * 20.0f);
        for (int i = 0; i < cachedRenderers.Length; i++)
        {
            if (cachedRenderers[i] != null)
            {
                cachedRenderers[i].sortingOrder = order;
            }
        }
    }

    public void SetTint(Color color)
    {
        if (cachedRenderers == null || cachedRenderers.Length == 0)
        {
            RefreshRenderers();
        }

        if (propertyBlock == null)
        {
            propertyBlock = new MaterialPropertyBlock();
        }

        for (int i = 0; i < cachedRenderers.Length; i++)
        {
            Renderer renderer = cachedRenderers[i];
            if (renderer == null)
            {
                continue;
            }

            renderer.GetPropertyBlock(propertyBlock);
            propertyBlock.SetColor("_Color", color);
            propertyBlock.SetColor("_TintColor", color);
            renderer.SetPropertyBlock(propertyBlock);
        }
    }

    public void ClearTint()
    {
        if (cachedRenderers == null || cachedRenderers.Length == 0)
        {
            RefreshRenderers();
        }

        for (int i = 0; i < cachedRenderers.Length; i++)
        {
            Renderer renderer = cachedRenderers[i];
            if (renderer != null)
            {
                renderer.SetPropertyBlock(null);
            }
        }
    }

    private static Vector3 NormalizePlanar(Vector3 value, Vector3 fallback)
    {
        value.y = 0.0f;
        if (value.sqrMagnitude < 0.0001f)
        {
            return fallback;
        }
        return value.normalized;
    }
}
