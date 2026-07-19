using System;
using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

[ExecuteInEditMode]
public class UnityLearningCityCameraController : MonoBehaviour
{
    private struct CameraInfo
    {
        public string id;
        public float dist;
        public float fov;
        public float dxf;

        public CameraInfo(string id, float dist, float fov, float dxf)
        {
            this.id = id;
            this.dist = dist;
            this.fov = fov;
            this.dxf = dxf;
        }
    }

    private static readonly CameraInfo[] CityCameraInfos =
    {
        new CameraInfo("limit_min", 14.47f, 5.3f, 76.691f),
        new CameraInfo("min", 15.47f, 5.33f, 82.4551f),
        new CameraInfo("city_default", 20.0f, 7.5f, 150.0f),
        new CameraInfo("city_bound", 25.0f, 10.0f, 250.0f),
        new CameraInfo("city", 24.693f, 11.0f, 271.623f),
    };

    public Vector3 defaultTarget = Vector3.zero;
    public Vector3 target = Vector3.zero;

    public float sourceWorldScale = 5.0f;
    public float defaultDxf = 150.0f;
    public float currentDxf = 150.0f;
    public float minDxf = 76.691f;
    public float maxDxf = 271.623f;

    public float defaultDistance = 100.0f;
    public float distance = 90.0f;
    public float minDistance = 24.0f;
    public float maxDistance = 180.0f;
    public float fieldOfView = 7.5f;
    public float wheelZoomRate = 0.18f;
    public float keyZoomRate = 0.18f;

    private Camera cachedCamera;
    private bool editorDragging;
    private Vector3 dragStartTerrainPos;
    private Vector3 dragStartViewCenter;
    private bool dragStartValid;

    private void OnEnable()
    {
        cachedCamera = GetComponent<Camera>();
        minDxf = CityCameraInfos[0].dxf;
        maxDxf = CityCameraInfos[CityCameraInfos.Length - 1].dxf;
        if (currentDxf <= 0.0f)
        {
            currentDxf = defaultDxf;
        }
        currentDxf = Mathf.Clamp(currentDxf, minDxf, maxDxf);
        ApplyDxf(currentDxf, false);
        ApplyCamera();
    }

    private void OnGUI()
    {
        if (Application.isPlaying)
        {
            return;
        }

        Event evt = Event.current;
        if (evt == null)
        {
            return;
        }

        if (evt.type == EventType.ScrollWheel)
        {
            float screenY = Screen.height - evt.mousePosition.y;
            ZoomByRate(1.0f + evt.delta.y * wheelZoomRate, new Vector2(evt.mousePosition.x, screenY));
            evt.Use();
        }
        else if (evt.type == EventType.MouseDown && (evt.button == 1 || evt.button == 2))
        {
            BeginDrag(new Vector2(evt.mousePosition.x, Screen.height - evt.mousePosition.y));
            editorDragging = dragStartValid;
            evt.Use();
        }
        else if (evt.type == EventType.MouseDrag && editorDragging)
        {
            DragTo(new Vector2(evt.mousePosition.x, Screen.height - evt.mousePosition.y));
            evt.Use();
        }
        else if (evt.type == EventType.MouseUp && (evt.button == 1 || evt.button == 2))
        {
            editorDragging = false;
            evt.Use();
        }
        else if (evt.type == EventType.KeyDown)
        {
            if (evt.keyCode == KeyCode.Equals || evt.keyCode == KeyCode.KeypadPlus || evt.keyCode == KeyCode.PageUp)
            {
                ZoomInAtScreenCenter();
                evt.Use();
            }
            else if (evt.keyCode == KeyCode.Minus || evt.keyCode == KeyCode.KeypadMinus || evt.keyCode == KeyCode.PageDown)
            {
                ZoomOutAtScreenCenter();
                evt.Use();
            }
            else if (evt.keyCode == KeyCode.Home || evt.keyCode == KeyCode.R)
            {
                ResetView();
                evt.Use();
            }
            else if (evt.keyCode == KeyCode.LeftArrow || evt.keyCode == KeyCode.A)
            {
                PanByWorldDelta(-GetCameraRightOnGround() * Mathf.Max(0.4f, distance * 0.018f));
                evt.Use();
            }
            else if (evt.keyCode == KeyCode.RightArrow || evt.keyCode == KeyCode.D)
            {
                PanByWorldDelta(GetCameraRightOnGround() * Mathf.Max(0.4f, distance * 0.018f));
                evt.Use();
            }
            else if (evt.keyCode == KeyCode.UpArrow || evt.keyCode == KeyCode.W)
            {
                PanByWorldDelta(GetCameraForwardOnGround() * Mathf.Max(0.4f, distance * 0.018f));
                evt.Use();
            }
            else if (evt.keyCode == KeyCode.DownArrow || evt.keyCode == KeyCode.S)
            {
                PanByWorldDelta(-GetCameraForwardOnGround() * Mathf.Max(0.4f, distance * 0.018f));
                evt.Use();
            }
        }
    }

    private void Update()
    {
        if (cachedCamera == null)
        {
            cachedCamera = GetComponent<Camera>();
        }

        if (Application.isPlaying)
        {
            float wheel = Input.GetAxis("Mouse ScrollWheel");
            if (Mathf.Abs(wheel) > 0.0001f)
            {
                ZoomByRate(1.0f - wheel * wheelZoomRate, Input.mousePosition);
            }

            if (Input.GetMouseButtonDown(1) || Input.GetMouseButtonDown(2))
            {
                BeginDrag(Input.mousePosition);
            }

            if (Input.GetMouseButton(1) || Input.GetMouseButton(2))
            {
                DragTo(Input.mousePosition);
            }

            if (Input.GetKeyDown(KeyCode.Home))
            {
                ResetView();
            }
        }

        ApplyCamera();
    }

    public void ZoomIn()
    {
        ZoomInAtScreenCenter();
    }

    public void ZoomOut()
    {
        ZoomOutAtScreenCenter();
    }

    public void ResetView()
    {
        target = defaultTarget;
        currentDxf = defaultDxf;
        ApplyDxf(currentDxf, false);
        ApplyCamera();
        MarkDirtyInEditor();
    }

    public void SetZoom01(float normalized)
    {
        currentDxf = Mathf.Lerp(minDxf, maxDxf, Mathf.Clamp01(normalized));
        ApplyDxf(currentDxf, false);
        ApplyCamera();
        MarkDirtyInEditor();
    }

    public float GetCurrentDxf()
    {
        return currentDxf;
    }

    private void ZoomInAtScreenCenter()
    {
        ZoomByRate(1.0f - keyZoomRate, new Vector2(Screen.width * 0.5f, Screen.height * 0.5f));
    }

    private void ZoomOutAtScreenCenter()
    {
        ZoomByRate(1.0f + keyZoomRate, new Vector2(Screen.width * 0.5f, Screen.height * 0.5f));
    }

    private void ZoomByRate(float scrollRate, Vector2 screenPos)
    {
        Vector3 before = GetTouchTerrainPos(screenPos);
        currentDxf = Mathf.Clamp(currentDxf * Mathf.Max(0.05f, scrollRate), minDxf, maxDxf);
        ApplyDxf(currentDxf, false);
        ApplyCamera();
        Vector3 after = GetTouchTerrainPos(screenPos);
        if (IsValidTerrainPos(before) && IsValidTerrainPos(after))
        {
            Vector3 delta = after - before;
            target -= new Vector3(delta.x, 0.0f, delta.z);
            ApplyCamera();
        }
        MarkDirtyInEditor();
    }

    private void BeginDrag(Vector2 screenPos)
    {
        dragStartTerrainPos = GetTouchTerrainPos(screenPos);
        dragStartViewCenter = target;
        dragStartValid = IsValidTerrainPos(dragStartTerrainPos);
    }

    private void DragTo(Vector2 screenPos)
    {
        if (!dragStartValid)
        {
            return;
        }

        Vector3 touchTerrainPos = GetTouchTerrainPos(screenPos);
        if (!IsValidTerrainPos(touchTerrainPos))
        {
            return;
        }

        Vector3 delta = -1.0f * (touchTerrainPos - dragStartTerrainPos);
        target = dragStartViewCenter + new Vector3(delta.x, 0.0f, delta.z);
        ApplyCamera();
        MarkDirtyInEditor();
    }

    private Vector3 GetTouchTerrainPos(Vector2 screenPos)
    {
        if (cachedCamera == null)
        {
            cachedCamera = GetComponent<Camera>();
        }
        if (cachedCamera == null)
        {
            return InvalidTerrainPos();
        }

        Ray ray = cachedCamera.ScreenPointToRay(new Vector3(screenPos.x, screenPos.y, 0.0f));
        Plane terrainPlane = new Plane(Vector3.up, Vector3.zero);
        float enter = 0.0f;
        if (terrainPlane.Raycast(ray, out enter))
        {
            return ray.GetPoint(enter);
        }
        return InvalidTerrainPos();
    }

    private void PanByWorldDelta(Vector3 delta)
    {
        target += delta;
        ApplyCamera();
        MarkDirtyInEditor();
    }

    private Vector3 GetCameraRightOnGround()
    {
        Vector3 right = Vector3.ProjectOnPlane(transform.right, Vector3.up);
        return right.sqrMagnitude > 0.0001f ? right.normalized : Vector3.right;
    }

    private Vector3 GetCameraForwardOnGround()
    {
        Vector3 forward = Vector3.ProjectOnPlane(transform.forward, Vector3.up);
        return forward.sqrMagnitude > 0.0001f ? forward.normalized : Vector3.forward;
    }

    private static Vector3 InvalidTerrainPos()
    {
        return new Vector3(float.NaN, float.NaN, float.NaN);
    }

    private static bool IsValidTerrainPos(Vector3 value)
    {
        return !float.IsNaN(value.x) && !float.IsNaN(value.y) && !float.IsNaN(value.z);
    }

    private void ApplyDxf(float dxf, bool preserveDxf)
    {
        float refDist;
        float refFov;
        if (!GetDistFovByDxf(dxf, out refDist, out refFov))
        {
            refDist = 20.0f;
            refFov = 7.5f;
        }

        distance = Mathf.Clamp(refDist * sourceWorldScale, minDistance, maxDistance);
        fieldOfView = refFov;
        if (!preserveDxf)
        {
            currentDxf = Mathf.Clamp(dxf, minDxf, maxDxf);
        }
    }

    private static bool GetDistFovByDxf(float dxf, out float dist, out float fov)
    {
        dxf = Mathf.Clamp(dxf, CityCameraInfos[0].dxf, CityCameraInfos[CityCameraInfos.Length - 1].dxf);
        for (int i = 0; i < CityCameraInfos.Length - 1; i++)
        {
            CameraInfo min = CityCameraInfos[i];
            CameraInfo max = CityCameraInfos[i + 1];
            if (Mathf.Approximately(dxf, max.dxf))
            {
                dist = max.dist;
                fov = max.fov;
                return true;
            }
            if (dxf >= min.dxf && dxf <= max.dxf)
            {
                double outDist;
                double outFov;
                if (GetDistFovWithTwoCameraInfo(min.dist, min.fov, max.dist, max.fov, dxf, out outDist, out outFov))
                {
                    dist = (float)outDist;
                    fov = (float)outFov;
                    return true;
                }
            }
        }

        CameraInfo fallback = CityCameraInfos[2];
        dist = fallback.dist;
        fov = fallback.fov;
        return false;
    }

    private static bool GetDistFovWithTwoCameraInfo(double distMin, double fovMin, double distMax, double fovMax, double dxf, out double dist, out double fov)
    {
        dist = 0.0;
        fov = 0.0;
        if (distMin * fovMin > distMax * fovMax || dxf < distMin * fovMin || dxf > distMax * fovMax)
        {
            return false;
        }
        if (Math.Abs(dxf - distMin * fovMin) < 0.000001)
        {
            dist = distMin;
            fov = fovMin;
            return true;
        }
        if (Math.Abs(dxf - distMax * fovMax) < 0.000001)
        {
            dist = distMax;
            fov = fovMax;
            return true;
        }
        if (Math.Abs(distMax - distMin) < 0.000001)
        {
            dist = distMin;
            fov = dxf / distMin;
            return true;
        }
        if (Math.Abs(fovMax - fovMin) < 0.000001)
        {
            fov = fovMin;
            dist = dxf / fovMin;
            return true;
        }

        double low = 0.0;
        double high = 1.0;
        double t = 0.0;
        for (int i = 0; i < 24; i++)
        {
            t = (low + high) * 0.5;
            double test = (distMin + t * (distMax - distMin)) * (fovMin + t * (fovMax - fovMin));
            if (Math.Abs(dxf - test) < 0.000001)
            {
                break;
            }
            if (dxf > test)
            {
                low = t;
            }
            else
            {
                high = t;
            }
        }

        dist = distMin + t * (distMax - distMin);
        fov = fovMin + t * (fovMax - fovMin);
        return true;
    }

    private void ApplyCamera()
    {
        if (cachedCamera != null)
        {
            cachedCamera.orthographic = false;
            cachedCamera.fieldOfView = fieldOfView;
            cachedCamera.nearClipPlane = 0.01f;
            cachedCamera.farClipPlane = 420.0f;
        }

        Vector3 forward = new Vector3(0.0f, -0.7f, 0.7f).normalized;
        Quaternion rotation = Quaternion.LookRotation(forward, Vector3.up);
        transform.rotation = rotation;
        transform.position = target - forward * distance;
    }

    private void MarkDirtyInEditor()
    {
#if UNITY_EDITOR
        if (!Application.isPlaying)
        {
            EditorUtility.SetDirty(this);
            if (cachedCamera != null)
            {
                EditorUtility.SetDirty(cachedCamera);
            }
            EditorUtility.SetDirty(transform);
        }
#endif
    }
}
