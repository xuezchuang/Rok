using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class RenderRokMailAttachmentPulseProbe
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityMailAttachmentPulseProbe";
    [Serializable] public class Inputs
    {
        public string scope, clip;
        public Vector2 size, pivot, anchorMin, anchorMax, position;
        public int arabFlags;
        public float[] times;
    }
    [Serializable] public class Sample { public float requestedSeconds, sourceLoopPhase; public Vector3 scale; public Vector2 pivot, anchoredPosition; }
    [Serializable] public class Report
    {
        public string scope, unityVersion, inputSha256, clipWrapMode;
        public bool nativeClipSampled, fullSpineChild = false, fullMediator = false;
        public float nativeClipLength;
        public Vector2 serializedPivot, chinesePivot;
        public Sample[] samples;
    }
    public static void Run()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase)
            || Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong isolated pulse probe project/version");
        string output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
        Directory.CreateDirectory(output);
        try
        {
            var data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Project + "/ReferenceInputs.json"));
            if (data.arabFlags != 2 || data.size != new Vector2(28, 25) || data.pivot != new Vector2(0, 1)) throw new Exception("Unexpected extracted source parent");
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var parent = new GameObject("Source_img_bag", typeof(RectTransform));
            var rect = parent.GetComponent<RectTransform>();
            rect.anchorMin = data.anchorMin; rect.anchorMax = data.anchorMax;
            rect.pivot = data.pivot; rect.anchoredPosition = data.position; rect.sizeDelta = data.size;
            // Exact Chinese Awake PosX branch, including pivot and anchor flip.
            rect.anchorMin = new Vector2(1 - data.anchorMax.x, data.anchorMin.y);
            rect.anchorMax = new Vector2(1 - data.anchorMin.x, data.anchorMax.y);
            rect.pivot = new Vector2(1 - data.pivot.x, data.pivot.y);
            rect.anchoredPosition = new Vector2(-data.position.x, data.position.y);
            var clip = AssetDatabase.LoadAssetAtPath<AnimationClip>(data.clip);
            if (clip == null || !clip.legacy || clip.wrapMode != WrapMode.Loop || Mathf.Abs(clip.length - 2) > .000001f)
                throw new Exception("Original legacy loop clip did not import faithfully");
            var report = new Report { scope = data.scope, unityVersion = Application.unityVersion,
                inputSha256 = Hash(Project + "/ReferenceInputs.json"), clipWrapMode = clip.wrapMode.ToString(),
                nativeClipLength = clip.length, serializedPivot = data.pivot, chinesePivot = rect.pivot };
            report.samples = data.times.Select(time =>
            {
                // Explicit modulo is the imported legacy Loop policy; native clip
                // evaluates the curve. No reconstructed interpolation in this probe.
                float phase = Mathf.Repeat(time, clip.length);
                clip.SampleAnimation(parent, phase);
                if (rect.pivot != new Vector2(1, 1) || rect.anchoredPosition != new Vector2(191.8f, 82.8f))
                    throw new Exception("Scale-only clip altered source parent layout");
                if (Mathf.Abs(rect.localScale.x - rect.localScale.y) > .000001f || rect.localScale.z != 1)
                    throw new Exception("Original clip unexpected axis behavior");
                return new Sample { requestedSeconds = time, sourceLoopPhase = phase, scale = rect.localScale,
                    pivot = rect.pivot, anchoredPosition = rect.anchoredPosition };
            }).ToArray();
            report.nativeClipSampled = true;
            File.WriteAllText(output + "/native_pulse_samples.json", JsonUtility.ToJson(report, true));
            Debug.Log("Mail parent pulse native clip samples complete. Spine child and authority excluded.");
            EditorApplication.Exit(0);
        }
        catch (Exception error)
        {
            File.WriteAllText(output + "/probe_failure.txt", error.ToString());
            Debug.LogException(error); EditorApplication.Exit(1);
        }
    }
    static string Hash(string path)
    {
        using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant();
    }
}
