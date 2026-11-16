using System;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEditor.Media;
using UnityEngine;
using UnityEngine.SceneManagement;
using Spine.Unity;

public static class RokCommanderSceneBake
{
    public static void Run()
    {
        const int width = 1440, height = 900;
        const string path = "Assets/BundleAssets/Scene/City/city_heroshow";
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var camera = CreateCaptainCamera();
        var target = new RenderTexture(width, height, 24, RenderTextureFormat.ARGB32);
        var image = new Texture2D(width, height, TextureFormat.RGB24, false);
        camera.targetTexture = target;
        var output = Path.GetFullPath(Path.Combine(Application.dataPath, "../../UnityBackgrounds"));
        Directory.CreateDirectory(output);
        for (int i = 1; i <= 4; i++)
        {
            var name = $"HeroShow_{i:00}";
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>($"{path}/{name}.prefab");
            if (!prefab) throw new InvalidOperationException($"Missing {name}");
            var stage = UnityEngine.Object.Instantiate(prefab, new Vector3(0, 1.44f, 0), prefab.transform.rotation);
            stage.transform.localScale = Vector3.one;
            RestoreTexturePackerMeshes(stage);
            foreach (var animation in stage.GetComponentsInChildren<SkeletonAnimator>())
            {
                animation.Initialize(true);
                animation.Update();
                animation.LateUpdate();
            }
            camera.Render();
            RenderTexture.active = target;
            image.ReadPixels(new Rect(0, 0, width, height), 0, 0);
            File.WriteAllBytes(Path.Combine(output, name + ".png"), image.EncodeToPNG());
            RenderTexture.active = null;
            UnityEngine.Object.DestroyImmediate(stage);
        }
    }

    private static Camera CreateCaptainCamera()
    {
        var camera = new GameObject("Captain camera").AddComponent<Camera>();
        camera.transform.position = new Vector3(0, 0, -9.2f);
        camera.fieldOfView = 41.7f;
        camera.nearClipPlane = .01f;
        camera.farClipPlane = 1000f;
        camera.cullingMask = 1 << 9;
        camera.clearFlags = CameraClearFlags.SolidColor;
        camera.backgroundColor = Color.black;
        return camera;
    }

    private const string MotionBakePending = "Rok.CommanderSceneMotionBakePending";
    private const int MotionFps = 30;
    private const int MotionSeconds = 18;

    public static void StartMotion()
    {
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        SessionState.SetBool(MotionBakePending, true);
        EditorApplication.EnterPlaymode();
    }

    [InitializeOnLoadMethod]
    private static void RegisterMotionBake()
    {
        EditorApplication.playModeStateChanged += state =>
        {
            if (state != PlayModeStateChange.EnteredPlayMode || !SessionState.GetBool(MotionBakePending, false))
                return;
            SessionState.EraseBool(MotionBakePending);
            EditorApplication.delayCall += () =>
            {
                try
                {
                    BakeMotion();
                    EditorApplication.Exit(0);
                }
                catch (Exception error)
                {
                    Debug.LogException(error);
                    EditorApplication.Exit(1);
                }
            };
        };
    }

    private static void BakeMotion()
    {
        const int width = 1440, height = 900;
        const string path = "Assets/BundleAssets/Scene/City/city_heroshow";
        Time.timeScale = 0f;
        var camera = CreateCaptainCamera();
        var target = new RenderTexture(width, height, 24, RenderTextureFormat.ARGB32);
        var image = new Texture2D(width, height, TextureFormat.RGBA32, false);
        camera.targetTexture = target;
        var output = Path.GetFullPath(Path.Combine(Application.dataPath, "../../UnityMotion"));
        Directory.CreateDirectory(output);
        var track = new VideoTrackAttributes
        {
            frameRate = new MediaRational(MotionFps),
            width = width,
            height = height,
            includeAlpha = false,
            bitRateMode = VideoBitrateMode.High
        };
        for (int scene = 1; scene <= 4; ++scene)
        {
            var name = $"HeroShow_{scene:00}";
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>($"{path}/{name}.prefab");
            if (!prefab) throw new InvalidOperationException($"Missing {name}");

            // Disable the authored auto-prewarm before activating the instance. Some
            // continuous snow emitters use a 99999 second duration. Warm their live
            // particle state for a bounded interval without changing emission curves.
            var parent = new GameObject(name + " capture root");
            parent.SetActive(false);
            var stage = UnityEngine.Object.Instantiate(prefab, parent.transform, false);
            stage.transform.localPosition = new Vector3(0, 1.44f, 0);
            stage.transform.localScale = Vector3.one;
            var particles = stage.GetComponentsInChildren<ParticleSystem>(true);
            for (int i = 0; i < particles.Length; i++)
            {
                var settings = particles[i].main;
                settings.prewarm = false;
                settings.playOnAwake = false;
                particles[i].useAutoRandomSeed = false;
                particles[i].randomSeed = 157u + (uint)i;
            }
            parent.SetActive(true);
            RestoreTexturePackerMeshes(stage);
            var spines = stage.GetComponentsInChildren<SkeletonAnimator>();
            foreach (var spine in spines)
            {
                var animator = spine.GetComponent<Animator>();
                animator.Rebind();
                animator.Update(0);
                spine.Initialize(true);
            }
            var scrolls = stage.GetComponentsInChildren<UVAnimation>();
            var materials = Array.ConvertAll(scrolls, scroll => scroll.GetComponent<MeshRenderer>().material);
            var offsets = Array.ConvertAll(materials, material => material.mainTextureOffset);
            foreach (var particle in particles)
                if (particle.gameObject.activeInHierarchy) particle.Simulate(8f, false, true, false);
            Debug.Log($"Baking {name}: {spines.Length} Spine layers, {scrolls.Length} UV layers, {particles.Length} particle systems");

            // Overlap one second of the source intro and tail. The first encoded
            // frame is just after the intro, so playback advances through the seam
            // without resetting wind, foliage poses or the live snow field.
            const int overlap = MotionFps;
            const int frameCount = MotionFps * MotionSeconds;
            var intro = new byte[overlap][];
            using (var encoder = new MediaEncoder(Path.Combine(output, name + ".mp4"), track))
            {
                for (int frame = 0; frame < frameCount + overlap; frame++)
                {
                    const float step = 1f / MotionFps;
                    foreach (var spine in spines)
                    {
                        spine.GetComponent<Animator>().Update(frame == 0 ? 0f : step);
                        spine.Update();
                        spine.LateUpdate();
                    }
                    if (frame > 0)
                        foreach (var particle in particles)
                            if (particle.gameObject.activeInHierarchy) particle.Simulate(step, false, false, false);
                    for (int i = 0; i < materials.Length; i++)
                        materials[i].mainTextureOffset = offsets[i] + scrolls[i].speed * (frame * step);

                    camera.Render();
                    RenderTexture.active = target;
                    image.ReadPixels(new Rect(0, 0, width, height), 0, 0);
                    RenderTexture.active = null;
                    var pixels = image.GetRawTextureData<byte>();
                    if (frame < overlap)
                    {
                        intro[frame] = pixels.ToArray();
                        continue;
                    }
                    if (frame >= frameCount)
                    {
                        int tail = frame - frameCount;
                        int weight = Mathf.RoundToInt(256f * Mathf.SmoothStep(0, 1, (tail + 1f) / overlap));
                        var start = intro[tail];
                        for (int i = 0; i < start.Length; i++)
                            pixels[i] = (byte)((pixels[i] * (256 - weight) + start[i] * weight + 128) >> 8);
                    }
                    if (frame == overlap || frame == overlap + MotionFps * 4)
                        File.WriteAllBytes(Path.Combine(output, $"{name}_{(frame - overlap) / MotionFps}s.png"), image.EncodeToPNG());
                    if (!encoder.AddFrame(image))
                        throw new InvalidOperationException($"Could not encode {name} frame {frame - overlap}");
                }
            }
            Debug.Log($"Baked {name}: {frameCount} frames at {MotionFps} fps");
            UnityEngine.Object.DestroyImmediate(parent);
        }
    }

    // The reference importer's serialized sprite metadata omits the polygon
    // vertices. Read the authored TexturePacker mesh to exclude neighboring
    // atlas art outside each sprite polygon.
    private static void RestoreTexturePackerMeshes(GameObject stage)
    {
        var imported = new System.Collections.Generic.Dictionary<Sprite, Mesh>();
        foreach (var renderer in stage.GetComponentsInChildren<SpriteRenderer>())
        {
            var sprite = renderer.sprite;
            if (!sprite) continue;
            if (!imported.TryGetValue(sprite, out var mesh))
            {
                mesh = BuildTexturePackerMesh(sprite);
                imported.Add(sprite, mesh);
            }
            if (!mesh) continue;

            var holder = new GameObject(sprite.name + " polygon");
            holder.layer = renderer.gameObject.layer;
            holder.transform.SetParent(renderer.transform, false);
            holder.transform.localScale = new Vector3(renderer.flipX ? -1 : 1, renderer.flipY ? -1 : 1, 1);
            holder.AddComponent<MeshFilter>().sharedMesh = mesh;
            var polygon = holder.AddComponent<MeshRenderer>();
            polygon.sortingLayerID = renderer.sortingLayerID;
            polygon.sortingOrder = renderer.sortingOrder;
            polygon.sharedMaterial = renderer.sharedMaterial;
            var properties = new MaterialPropertyBlock();
            properties.SetTexture("_MainTex", sprite.texture);
            properties.SetColor("_Color", renderer.color);
            polygon.SetPropertyBlock(properties);
            renderer.enabled = false;
        }
    }

    private static Mesh BuildTexturePackerMesh(Sprite sprite)
    {
        var texture = AssetDatabase.GetAssetPath(sprite);
        var sheet = Array.Find(Directory.GetFiles(Path.GetDirectoryName(texture), "*.tpsheet"),
            path => Path.GetFileNameWithoutExtension(path).Equals(Path.GetFileNameWithoutExtension(texture),
                StringComparison.OrdinalIgnoreCase));
        if (sheet == null) return null;
        foreach (var line in File.ReadLines(sheet))
        {
            if (!line.StartsWith(sprite.name + ";", StringComparison.Ordinal)) continue;
            var fields = line.Split(';');
            if (fields.Length < 12) return null;
            var vertices = new Vector3[int.Parse(fields[11])];
            var uv = new Vector2[vertices.Length];
            var colors = new Color32[vertices.Length];
            var pos = 12;
            for (int i = 0; i < vertices.Length; ++i)
            {
                var pixel = new Vector2(int.Parse(fields[pos++]), int.Parse(fields[pos++]));
                vertices[i] = (pixel - sprite.pivot) / sprite.pixelsPerUnit;
                uv[i] = new Vector2((sprite.rect.x + pixel.x) / sprite.texture.width,
                    (sprite.rect.y + pixel.y) / sprite.texture.height);
                colors[i] = Color.white;
            }
            var indices = new int[int.Parse(fields[pos++]) * 3];
            for (int i = 0; i < indices.Length; ++i)
                indices[i] = int.Parse(fields[pos++]);
            return new Mesh { vertices = vertices, uv = uv, colors32 = colors, triangles = indices };
        }
        return null;
    }
}
