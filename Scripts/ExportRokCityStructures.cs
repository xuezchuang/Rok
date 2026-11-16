using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

// Run in the isolated project prepared by PrepareRokCityStructures.py.
public static class ExportRokCityStructures
{
    [Serializable] class Input { public string id, path; }
    [Serializable] class Contract { public string output; public Input[] models; }
    [Serializable] class Section
    {
        public string obj, material, shader, texture, mask, maskSource, sprite, node;
        public bool translucent;
        public int order;
        public Color color, team1, team2;
        public Vector2 uvScale, uvOffset;
        public Vector3 center, size;
        public int vertices, triangles;
        public Pose[] poses;
    }
    [Serializable] class Pose { public Vector3 translation, scale; }
    [Serializable] class Model
    {
        public string id, path;
        public Section[] sections;
        public string[] omitted;
    }
    [Serializable] class Report { public Model[] models; }

    static string Asset(UnityEngine.Object value)
    {
        return value ? AssetDatabase.GetAssetPath(value) : "";
    }

    static string Texture(Texture texture, string output, bool raw)
    {
        if (!texture) return "";
        string guid = AssetDatabase.AssetPathToGUID(Asset(texture));
        if (string.IsNullOrEmpty(guid)) throw new InvalidOperationException("Texture without source GUID: " + texture.name);
        string path = Path.Combine(output, guid + (raw ? "_raw.png" : ".png"));
        if (File.Exists(path)) return path;
        var target = RenderTexture.GetTemporary(texture.width, texture.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
        var previous = RenderTexture.active;
        var pixels = new Texture2D(texture.width, texture.height, TextureFormat.RGBA32, false, true);
        try
        {
            Graphics.Blit(texture, target);
            RenderTexture.active = target;
            pixels.ReadPixels(new Rect(0, 0, texture.width, texture.height), 0, 0);
            pixels.Apply();
            File.WriteAllBytes(path, pixels.EncodeToPNG());
            return path;
        }
        finally
        {
            UnityEngine.Object.DestroyImmediate(pixels);
            RenderTexture.active = previous;
            RenderTexture.ReleaseTemporary(target);
        }
    }

    static string Node(Transform item, Transform root)
    {
        var names = new List<string>();
        for (; item != root; item = item.parent)
            names.Add(item.name + "[" + item.GetSiblingIndex() + "]");
        names.Reverse();
        return string.Join("/", names.ToArray());
    }

    static Vector3 UE(Vector3 v) { return new Vector3(-v.x, v.z, v.y) * 500f; }
    static string F(float value) { return value.ToString("R", CultureInfo.InvariantCulture); }

    static Section Write(string folder, int index, Transform root, Renderer renderer, Material material,
        Vector3[] points, Vector2[] uv, int[] triangles, SpriteRenderer sprite)
    {
        if (!material || uv.Length != points.Length || triangles.Length == 0)
            throw new InvalidOperationException("Invalid render section " + renderer.name);
        string path = Path.Combine(folder, "part_" + index + ".obj");
        var bounds = new Bounds(UE(points[0]), Vector3.zero);
        using (var stream = new StreamWriter(path))
        {
            stream.WriteLine("o part_" + index);
            foreach (var point in points)
            {
                var p = UE(point);
                bounds.Encapsulate(p);
                // UE's legacy OBJ import negates Y.
                stream.WriteLine("v " + F(p.x) + " " + F(-p.y) + " " + F(p.z));
            }
            foreach (var v in uv) stream.WriteLine("vt " + F(v.x) + " " + F(v.y));
            for (int i = 0; i < triangles.Length; i += 3)
            {
                var a = triangles[i] + 1; var b = triangles[i + 1] + 1; var c = triangles[i + 2] + 1;
                if (!sprite) { var reversed = a; a = c; c = reversed; }
                stream.WriteLine("f " + a + "/" + a + " " + b + "/" + b + " " + c + "/" + c);
            }
        }
        string textures = Path.Combine(Directory.GetParent(folder).FullName, "textures");
        Directory.CreateDirectory(textures);
        var main = sprite ? sprite.sprite.texture : material.mainTexture;
        Texture mask = null;
        if (sprite)
        {
            var original = PrefabUtility.GetCorrespondingObjectFromOriginalSource(sprite);
            var source = AssetDatabase.GetAssetPath(original);
            if (!string.IsNullOrEmpty(source))
            {
                var yaml = File.ReadAllText(source);
                string spriteGuid;
                long fileId;
                AssetDatabase.TryGetGUIDAndLocalFileIdentifier(original.gameObject, out spriteGuid, out fileId);
                var match = System.Text.RegularExpressions.Regex.Match(yaml,
                    @"(?m)^  m_GameObject: \{fileID: " + fileId + @"\}(?:(?!^---)[\s\S])*?m_mask_sprite: \{fileID: -?\d+, guid: ([a-f0-9]{32})");
                if (match.Success)
                {
                    var maskPath = AssetDatabase.GUIDToAssetPath(match.Groups[1].Value);
                    mask = AssetDatabase.LoadAssetAtPath<Texture2D>(maskPath);
                }
            }
        }
        if (sprite && !mask) throw new InvalidOperationException("Missing source mask: " + Node(sprite.transform, root));
        return new Section { obj = path, material = Asset(material), shader = material.shader.name,
            node = Node(renderer.transform, root), texture = Texture(main, textures, sprite),
            mask = Texture(mask, textures, true), maskSource = Asset(mask), sprite = sprite ? Asset(sprite.sprite) : "",
            translucent = sprite || material.renderQueue >= 3000,
            uvScale = sprite ? Vector2.one : material.mainTextureScale,
            uvOffset = sprite ? Vector2.zero : material.mainTextureOffset,
            color = sprite ? sprite.color : material.HasProperty("_Color") ? material.color : Color.white,
            team1 = sprite && !root.name.StartsWith("Build_shelf_")
                ? new Color(62f / 255f, 125f / 255f, 195f / 255f, 1f)
                : material.HasProperty("_Color_1") ? material.GetColor("_Color_1") : Color.gray,
            team2 = material.HasProperty("_Color_2") ? material.GetColor("_Color_2") : Color.gray,
            order = sprite ? sprite.sortingOrder : 0, vertices = points.Length,
            triangles = triangles.Length / 3, center = bounds.center, size = bounds.size };
    }

    static List<Section> Read(Transform root, string folder, bool shelf, List<string> omitted)
    {
        Directory.CreateDirectory(folder);
        var result = new List<Section>();
        foreach (var renderer in root.GetComponentsInChildren<Renderer>())
        {
            if (!renderer.enabled) continue;
            var sprite = renderer as SpriteRenderer;
            bool gateFlag = sprite && !shelf && sprite.name.StartsWith("sprite", StringComparison.OrdinalIgnoreCase)
                && sprite.transform.parent && (sprite.transform.parent.name.StartsWith("flag_", StringComparison.OrdinalIgnoreCase)
                || sprite.transform.parent.name.StartsWith("CityGate", StringComparison.OrdinalIgnoreCase));
            if (sprite && sprite.sprite && (shelf || gateFlag))
            {
                var points = sprite.sprite.vertices.Select(v => sprite.transform.TransformPoint(
                    new Vector3(sprite.flipX ? -v.x : v.x, sprite.flipY ? -v.y : v.y, 0f))).ToArray();
                var section = Write(folder, result.Count, root, sprite, sprite.sharedMaterial, points,
                    sprite.sprite.uv, sprite.sprite.triangles.Select(i => (int)i).ToArray(), sprite);
                var animation = sprite.GetComponentInParent<Animation>();
                if (shelf && animation && animation.clip)
                {
                    var item = sprite.transform;
                    var basePosition = item.localPosition;
                    var baseScale = item.localScale;
                    var baseRotation = item.localRotation;
                    var inverseBase = item.worldToLocalMatrix;
                    // The clips animate the position and scale of sprite1. Sample Unity's
                    // legacy interpolation at 60 Hz for the mediator's two-second window.
                    section.poses = new Pose[121];
                    for (int frame = 0; frame < section.poses.Length; ++frame)
                    {
                        animation.clip.SampleAnimation(animation.gameObject, Mathf.Min(frame / 60f, animation.clip.length));
                        var delta = item.localToWorldMatrix * inverseBase;
                        var x = UE(delta.MultiplyVector(new Vector3(-0.002f, 0, 0)));
                        var y = UE(delta.MultiplyVector(new Vector3(0, 0, 0.002f)));
                        var z = UE(delta.MultiplyVector(new Vector3(0, 0.002f, 0)));
                        if (Mathf.Abs(x.y) + Mathf.Abs(x.z) + Mathf.Abs(y.x) + Mathf.Abs(y.z) + Mathf.Abs(z.x) + Mathf.Abs(z.y) > 0.0001f)
                        {
                            // All animated axes use uniform scale in this shelf family.
                            throw new InvalidOperationException("Shelf needs non-diagonal animation: " + section.node);
                        }
                        section.poses[frame] = new Pose { translation = UE(delta.GetColumn(3)),
                            scale = new Vector3(x.x, y.y, z.z) };
                    }
                    item.localPosition = basePosition;
                    item.localScale = baseScale;
                    item.localRotation = baseRotation;
                }
                result.Add(section);
            }
            var filter = renderer.GetComponent<MeshFilter>();
            if (!filter || !filter.sharedMesh || !(renderer is MeshRenderer)) continue;
            // The map already owns the ground, city surface, shadow and road renderers.
            if (shelf || !(renderer.name.StartsWith("CityWall", StringComparison.OrdinalIgnoreCase)
                || renderer.name.StartsWith("CityGate", StringComparison.OrdinalIgnoreCase)
                || renderer.name.StartsWith("CityTower", StringComparison.OrdinalIgnoreCase))) continue;
            var mesh = filter.sharedMesh;
            var points3d = mesh.vertices.Select(renderer.transform.TransformPoint).ToArray();
            var materials = renderer.sharedMaterials;
            for (int slot = 0; slot < mesh.subMeshCount; ++slot)
                result.Add(Write(folder, result.Count, root, renderer, materials[Math.Min(slot, materials.Length - 1)],
                    points3d, mesh.uv, mesh.GetTriangles(slot), null));
        }
        if (result.Count == 0) omitted.Add("No supported renderer on the instantiated prefab");
        return result;
    }

    static void Capture(Camera camera, RenderTexture target, string path)
    {
        camera.Render();
        var previous = RenderTexture.active;
        RenderTexture.active = target;
        var image = new Texture2D(target.width, target.height, TextureFormat.RGB24, false);
        image.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0);
        image.Apply();
        File.WriteAllBytes(path, image.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(image);
        RenderTexture.active = previous;
    }

    public static void Run()
    {
        try
        {
            var contract = JsonUtility.FromJson<Contract>(File.ReadAllText("structures.json"));
            var models = new List<Model>();
            var camera = new GameObject("Source camera").AddComponent<Camera>();
            camera.clearFlags = CameraClearFlags.SolidColor;
            camera.backgroundColor = new Color(0.57f, 0.65f, 0.34f);
            camera.transform.rotation = Quaternion.Euler(45f, 0f, 0f);
            camera.fieldOfView = 7.5f;
            var target = new RenderTexture(1280, 800, 24);
            camera.targetTexture = target;
            foreach (var input in contract.models)
            {
                var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(input.path);
                if (!prefab) throw new FileNotFoundException(input.path);
                var root = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
                bool shelf = input.id.StartsWith("Build_shelf_");
                var city = Quaternion.Euler(0f, -45f, 0f);
                // CityObjData uses the building's tilted shelf Transform as parent.
                var shelfPose = new Quaternion(0.35355338f, 0.35355338f, -0.1464466f, 0.8535535f);
                root.transform.SetPositionAndRotation(Vector3.zero, shelf ? city * shelfPose : city);
                root.transform.localScale = Vector3.one;
                camera.transform.position = -camera.transform.forward * (shelf ? 13f : 85f);
                var omitted = new List<string>();
                string folder = Path.Combine(contract.output, input.id);
                var sections = Read(root.transform, folder, shelf, omitted);
                foreach (var sprite in root.GetComponentsInChildren<SpriteRenderer>())
                {
                    var section = sections.Find(s => s.node == Node(sprite.transform, root.transform) && s.mask != "");
                    if (section == null) continue;
                    var block = new MaterialPropertyBlock();
                    var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(section.maskSource);
                    block.SetTexture("_Mask", texture);
                    block.SetTexture("_MainTex", sprite.sprite.texture);
                    block.SetColor("_Color_1", section.team1);
                    block.SetColor("_SpriteColor", new Color(1.07203974f, 1.07203974f, 1.07203974f, 1));
                    block.SetFloat("_CityTransparency", 1f);
                    sprite.SetPropertyBlock(block);
                }
                Shader.SetGlobalFloat("_CityTransparency", 1f);
                Shader.SetGlobalColor("_SpriteColor", new Color(1.07203974f, 1.07203974f, 1.07203974f, 1));
                if (shelf)
                {
                    foreach (var animation in root.GetComponentsInChildren<Animation>())
                        if (animation.clip) animation.clip.SampleAnimation(animation.gameObject, animation.clip.length);
                }
                Capture(camera, target, Path.Combine(folder, "unity.png"));
                models.Add(new Model { id = input.id, path = input.path, sections = sections.ToArray(),
                    omitted = omitted.ToArray() });
                UnityEngine.Object.DestroyImmediate(root);
            }
            File.WriteAllText(Path.Combine(contract.output, "unity_structures.json"),
                JsonUtility.ToJson(new Report { models = models.ToArray() }, true));
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            EditorApplication.Exit(1);
        }
    }
}
