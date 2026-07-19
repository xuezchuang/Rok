using UnityEditor;
using UnityEngine;

public static class ArrowTransformProbe
{
    public static void Run()
    {
        Quaternion serializedPrefabRotation = new Quaternion(0.0f, -0.3826835f, 0.0f, 0.9238796f);
        Debug.LogFormat("ROK_ARROW QuaternionEulerPositive45={0:R},{1:R},{2:R},{3:R}",
            Quaternion.Euler(0.0f, 45.0f, 0.0f).x,
            Quaternion.Euler(0.0f, 45.0f, 0.0f).y,
            Quaternion.Euler(0.0f, 45.0f, 0.0f).z,
            Quaternion.Euler(0.0f, 45.0f, 0.0f).w);
        Debug.LogFormat("ROK_ARROW SerializedEuler={0}", serializedPrefabRotation.eulerAngles);

        GameObject container = new GameObject("CityBuildingContainer");
        container.transform.eulerAngles = new Vector3(0.0f, -45.0f, 0.0f);
        GameObject arrow = new GameObject("building_move_arrow");
        arrow.transform.rotation = serializedPrefabRotation;
        arrow.transform.SetParent(container.transform);

        Vector3 worldUpArrow = arrow.transform.TransformVector(Vector3.forward);
        Vector3 worldRightArrow = arrow.transform.TransformVector(Vector3.right);
        Debug.LogFormat("ROK_ARROW ParentWorldEuler={0} ArrowLocalEuler={1} ArrowWorldEuler={2}",
            container.transform.eulerAngles,
            arrow.transform.localEulerAngles,
            arrow.transform.eulerAngles);
        Debug.LogFormat("ROK_ARROW WorldUpArrow={0:R},{1:R},{2:R}",
            worldUpArrow.x, worldUpArrow.y, worldUpArrow.z);
        Debug.LogFormat("ROK_ARROW WorldRightArrow={0:R},{1:R},{2:R}",
            worldRightArrow.x, worldRightArrow.y, worldRightArrow.z);

        Object.DestroyImmediate(arrow);
        Object.DestroyImmediate(container);
        EditorApplication.Exit(0);
    }
}
