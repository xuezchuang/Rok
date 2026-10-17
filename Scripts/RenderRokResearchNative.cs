using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.UI;

// Isolated source-prefab presentation fixture. Never starts GameApp or sends research commands.
[InitializeOnLoad]
public static class RenderRokResearchNative
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityResearchNativeProbe";
    const string SessionKey = "RokResearchNative.Session";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Study { public int ID, l_nameID, l_des, type, studyType, studyLv, campusLv;
        public int preconditionStudy1, preconditionLv1, preconditionStudy2, preconditionLv2, preconditionStudy3, preconditionLv3, preconditionStudy4, preconditionLv4;
        public int needFood, needWood, needStone, needGold, l_buffDesID, costTime, power, columns, location, maxLevel, currentLevel;
        public string desData, icon, name, description, source; }
    [Serializable] public class FixtureState { public string name; public bool synthetic, accountBacked; }
    [Serializable] public class Fixture { public string schema, scope; public Study[] nodes, detailLevels, moreLevels; public int nodeCount, studyRowCount, detailStudyType, moreDetailStudyType, detailLockedLanguageId; public Entry[] language; public FixtureState state; }
    [Serializable] public class Inputs { public string schema, scope, uiRootPrefab, mainPrefab, detailPrefab, itemPrefab, subItemPrefab, powerItemPrefab, detailAttrItemPrefab, grayMaterialPath, fixtureSha256; public int width, height, probeVersion; public string[] atlasPaths; public Entry[] prefabTexts; public Fixture fixture; }
    [Serializable] public class Node { public string path, text, graphicType, spriteName, spriteAssetPath, materialName, shaderName, fontName, fontStyle, alignment;
        public bool activeSelf, activeInHierarchy, graphicEnabled, fontBestFit; public int fontSize, fontSizeUsed, siblingIndex;
        public Color color; public Vector2 anchorMin, anchorMax, anchoredPosition, sizeDelta, pivot; public Vector3 localScale, lossyScale;
        public Rect localRect, screenRect, glyphBoundsScreen; public Vector2[] screenCorners; }
    [Serializable] public class ResearchNode { public int studyId, studyType, category, column, location, currentLevel, maxLevel; public string iconReference, levelText;
        public bool hasReachedPrerequisites, sourceGrayShaderEmulated; public Node root, button, icon, iconBackground, name, level, progress, line, line2; }
    [Serializable] public class PowerRow { public int studyType, level, power; public string desData; public Node root, levelLabel, effectLabel, powerLabel, line; }
    [Serializable] public class Sample { public string key, png, sha256, title; public int category, visibleNodeCount, totalNodeCount, unityFrame;
        public float scrollX, contentWidth, viewportWidth, canvasScaleFactor, animatorNormalizedTime, detailAnimatorNormalizedTime;
        public string animatorState, detailAnimatorState; public bool detailUpdateActive, detailPowerActive, detailBackActive;
        public Node uiRoot, mainWindow, windowLayer, sidePanel, economyTab, militaryTab, economyScroll, economyViewport,
            economyContent, militaryScroll, militaryViewport, militaryContent, detailWindow;
        public ResearchNode[] nodes; public PowerRow[] powerRows; public Node[] mainGeometry, detailGeometry; }
    [Serializable] public class Report { public string schema, scope, unityVersion, inputSha256, fixtureSha256, failure;
        public bool complete, playMode, originalUiRoot, originalMainPrefab, originalItemPrefab, originalSubItemPrefab,
            originalDetailPrefab, originalPowerItemPrefab, syntheticUnresearched, noAccountOrNetworkAction; public int width, height, cameraPixelWidth, cameraPixelHeight;
        public Sample[] samples; }

    static string Output;
    static Inputs Data;
    static Report Evidence;
    static readonly List<Sample> Samples = new List<Sample>();
    static readonly HashSet<ArabLayoutCompment> Applied = new HashSet<ArabLayoutCompment>();
    static GameObject Root, Main, Detail;
    static Canvas Canvas;
    static Camera Cam;
    static RenderTexture Target;
    static RectTransform EconomyContent, MilitaryContent, EconomyViewport, MilitaryViewport;
    static readonly Dictionary<int, GameObject> NodeObjects = new Dictionary<int, GameObject>();
    static readonly int[][] Positions = { new[] {0,210,105,0,-105,-210,160,0,0,0,0,0},
        new[] {0,228,158,150,76,0,-76,-150,-158,-228,0,0,0,0,0,0} };
    static readonly string[][][] Lines = {
        new[] { new[] {"", "line_research_101", "line_research_110", "line_research_109", "line_research_104"},
                new[] {"", "line_research_201", "line_research_202", "line_research_203", "line_research_204"} },
        new[] { new[] {"", "line_research_101", "line_research_102", "line_research_103", "line_research_104"},
                new[] {"", "line_research_201", "line_research_202", "line_research_214", "line_research_204"} }
    };
    static readonly string[,] Sloped = { {"line_research_114", "line_research_114", "line_research_114", "line_research_112", "line_research_113"},
        {"line_research_210", "line_research_210", "line_research_210", "line_research_207", "line_research_209"} };
    static int Phase;
    static float EstablishedAt;
    static float DetailOpenedAt;
    static float DetailModeChangedAt;
    static readonly List<RectTransform> PowerRowObjects = new List<RectTransform>();

    static RenderRokResearchNative() { EditorApplication.update -= Tick; EditorApplication.update += Tick; }
    static string Hash(string path) { using (var h = SHA256.Create()) return BitConverter.ToString(h.ComputeHash(File.ReadAllBytes(path))).Replace("-", "").ToLowerInvariant(); }
    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase)
            || Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong isolated Unity project/version");
        if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new Exception("Camera.Render requires D3D11; do not pass -nographics");
        if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Expected source Gamma color space");
    }
    static string PathOf(Transform t) { string p = t.name; for (var a = t.parent; a; a = a.parent) p = a.name + "/" + p; return p; }
    static RectTransform Child(Transform parent, string name)
    {
        var found = parent.GetComponentsInChildren<RectTransform>(true).Where(t => t.name == name).ToArray();
        if (found.Length != 1) throw new Exception("Expected one source " + name + " below " + PathOf(parent) + "; got " + found.Length);
        return found[0];
    }
    static RectTransform At(Transform parent, string relative)
    {
        var target = parent.Find(relative);
        if (!target || !(target is RectTransform)) throw new Exception("Missing ResearchUpateView source path " + relative);
        return (RectTransform)target;
    }
    static T AtComponent<T>(Transform parent, string relative) where T : Component
    {
        var component = At(parent, relative).GetComponent<T>();
        if (!component) throw new Exception("Missing source " + typeof(T).Name + " at " + relative);
        return component;
    }
    static T One<T>(Transform parent, string name) where T : Component
    {
        var component = Child(parent, name).GetComponent<T>();
        if (!component) throw new Exception("Missing source " + typeof(T).Name + " at " + name);
        return component;
    }
    static Sprite Sprite(string reference)
    {
        var start = reference.IndexOf('[');
        if (start <= 0 || !reference.EndsWith("]")) throw new Exception("Invalid source sprite reference " + reference);
        var atlas = reference.Substring(0, start);
        var name = reference.Substring(start + 1, reference.Length - start - 2);
        var path = Data.atlasPaths.Single(p => Path.GetFileNameWithoutExtension(p) == atlas);
        var sprites = AssetDatabase.LoadAllAssetsAtPath(path).OfType<Sprite>().Where(s => s.name == name).ToArray();
        if (sprites.Length != 1) throw new Exception("Missing original sprite " + reference + " in " + path + "; got " + sprites.Length);
        return sprites[0];
    }
    static void SetSprite(Image image, string reference, bool nativeSize = false)
    {
        image.sprite = Sprite(reference);
        if (nativeSize) image.SetNativeSize();
        image.SetAllDirty();
    }
    static void SetLine(Image image, string spriteName, bool nativeSize)
    {
        if (String.IsNullOrEmpty(spriteName)) throw new Exception("Line sprite missing in source relationship");
        SetSprite(image, "ui_research[" + spriteName + "]", nativeSize);
    }
    static void ApplyTexts(GameObject parent)
    {
        foreach (var label in parent.GetComponentsInChildren<LanguageText>(true))
        {
            var row = Data.prefabTexts.FirstOrDefault(x => x.id == label.SourceLanguageId);
            if (row != null) label.text = row.text;
        }
    }
    static void ApplyChineseLayout(GameObject parent)
    {
        foreach (var arab in parent.GetComponentsInChildren<ArabLayoutCompment>(true))
        {
            int flags = arab.SourceFlags;
            if ((flags & ~891) != 0) throw new Exception("Unsupported ArabLayout flags " + flags + " at " + PathOf(arab.transform));
            if (!Applied.Add(arab) || (flags & 1) != 0) continue;
            if ((flags & 2) != 0) { var r = arab.GetComponent<RectTransform>(); var min = r.anchorMin; var max = r.anchorMax; r.anchorMin = new Vector2(1-max.x,min.y); r.anchorMax = new Vector2(1-min.x,max.y); r.pivot = new Vector2(1-r.pivot.x,r.pivot.y); r.anchoredPosition = new Vector2(-r.anchoredPosition.x,r.anchoredPosition.y); }
            if ((flags & 8) != 0) { var t = arab.GetComponent<Text>(); if (t) { int v = (int)t.alignment; t.alignment = (TextAnchor)(v/3*3+2-v%3); } }
            if ((flags & 16) != 0) { var i = arab.GetComponent<Image>(); if (i) { var s = i.transform.localScale; s.x = -s.x; i.transform.localScale = s; } }
            if ((flags & 32) != 0) { var s = arab.GetComponent<Slider>(); if (s && (s.direction == Slider.Direction.LeftToRight || s.direction == Slider.Direction.RightToLeft)) s.direction = s.direction == Slider.Direction.LeftToRight ? Slider.Direction.RightToLeft : Slider.Direction.LeftToRight; }
            if ((flags & 64) != 0) { var r = arab.GetComponent<RectTransform>(); if (r) { var s = r.localScale; s.x = -s.x; r.localScale = s; } }
            if ((flags & 256) != 0) { var list = arab.GetComponent<ListView>(); if (!list || list.layoutType != 3) throw new Exception("Expected source RightToLeft ListView before Chinese conversion"); list.layoutType = 2; }
            if ((flags & 512) != 0) { var g = arab.GetComponent<GridLayoutGroup>(); if (g) { g.startCorner = (GridLayoutGroup.Corner)((int)g.startCorner ^ 1); int v = (int)g.childAlignment; g.childAlignment = (TextAnchor)(v/3*3+2-v%3); } }
        }
    }
    public static void Run()
    {
        Guard();
        if (EditorApplication.isPlaying) throw new Exception("Start in Edit mode");
        Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
        Directory.CreateDirectory(Output);
        File.Copy(Project + "/ReferenceInputs.json", Output + "/ReferenceInputs.json");
        Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
        if (Data.probeVersion != 2 || Data.width != 1440 || Data.height != 810 || Data.fixture.nodeCount != 71 ||
            Data.fixture.moreDetailStudyType != 128 || Data.fixture.moreLevels.Length != 10 ||
            !Data.fixture.state.synthetic || Data.fixture.state.accountBacked || Hash(Project + "/ResearchPowerFixture.json") != Data.fixtureSha256)
            throw new Exception("Invalid immutable research fixture contract");
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var root = AssetDatabase.LoadAssetAtPath<GameObject>(Data.uiRootPrefab);
        var main = AssetDatabase.LoadAssetAtPath<GameObject>(Data.mainPrefab);
        if (!root || !main) throw new Exception("Original UIRoot/research prefabs did not import");
        Root = (GameObject)PrefabUtility.InstantiatePrefab(root);
        Canvas = Root.GetComponent<Canvas>(); Cam = Root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        var layer = Root.transform.Find("Container/WindowLayer");
        if (!Canvas || !Cam || !layer) throw new Exception("Original UIRoot camera/window layer missing");
        Canvas.worldCamera = Cam;
        Main = (GameObject)PrefabUtility.InstantiatePrefab(main);
        var r = Main.GetComponent<RectTransform>(); var pos = r.anchoredPosition; var size = r.sizeDelta; var scale = r.localScale;
        r.SetParent(layer); r.anchoredPosition = pos; r.sizeDelta = size; r.localScale = scale;
        ApplyTexts(Main); ApplyChineseLayout(Main);
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene(), Project + "/Assets/ResearchNativeProbe.unity");
        EditorPrefs.SetString(SessionKey, Output);
        EditorApplication.isPlaying = true;
    }
    static RectTransform ConfigureContent(RectTransform scroll, out RectTransform viewport)
    {
        viewport = Child(scroll, "v_list_view");
        var content = Child(viewport, "c_list_view");
        var old = content.rect.size;
        content.pivot = new Vector2(0, .5f); content.anchorMin = new Vector2(0, .5f); content.anchorMax = new Vector2(0, .5f);
        content.sizeDelta = old; content.anchoredPosition = Vector2.zero;
        return content;
    }
    static void Establish()
    {
        Output = EditorPrefs.GetString(SessionKey);
        Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
        Root = GameObject.Find("UIRoot"); Main = GameObject.Find("UI_Win_ResearchMain");
        if (!Root || !Main) throw new Exception("PlayMode lost original source prefabs");
        Canvas = Root.GetComponent<Canvas>(); Cam = Root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        Target = new RenderTexture(Data.width, Data.height, 24, RenderTextureFormat.ARGB32); Target.Create(); Cam.targetTexture = Target;
        var econ = Child(Main.transform, "sv_economy"); var mil = Child(Main.transform, "sv_military");
        EconomyContent = ConfigureContent(econ, out EconomyViewport);
        MilitaryContent = ConfigureContent(mil, out MilitaryViewport);
        FillCategory(1, EconomyContent);
        FillCategory(2, MilitaryContent);
        Child(Main.transform, "pl_speedUp").gameObject.SetActive(false); // no synthetic research queue
        Evidence = new Report { schema = "rok.research.native.v2", scope = Data.scope, unityVersion = Application.unityVersion,
            inputSha256 = Hash(Output + "/ReferenceInputs.json"), fixtureSha256 = Data.fixtureSha256,
            playMode = true, width = Data.width, height = Data.height, originalUiRoot = true, originalMainPrefab = true,
            originalItemPrefab = true, originalSubItemPrefab = true, originalDetailPrefab = false, originalPowerItemPrefab = false,
            syntheticUnresearched = true, noAccountOrNetworkAction = true };
        EstablishedAt = Time.realtimeSinceStartup;
    }
    static void FillCategory(int category, RectTransform content)
    {
        var grouped = Data.fixture.nodes.Where(n => n.type == category).GroupBy(n => n.columns).OrderBy(g => g.Key).ToArray();
        if (grouped.Length != 15 || grouped.Select(g => g.Key).Where((key, i) => key != i+1).Any()) throw new Exception("Study columns not contiguous 1..15");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.itemPrefab);
        var itemSize = prefab.GetComponent<RectTransform>().rect.size;
        content.SetSizeWithCurrentAnchors(RectTransform.Axis.Horizontal, itemSize.x * 15);
        foreach (var group in grouped)
        {
            if (group.Count() > 4) throw new Exception("Source UI_ResearchItem supports only four nodes");
            var go = UnityEngine.Object.Instantiate(prefab, content, false); go.name = "UI_ResearchItem_Column" + group.Key;
            var rect = go.GetComponent<RectTransform>(); rect.anchorMin = new Vector2(0,.5f); rect.anchorMax = rect.anchorMin;
            rect.pivot = new Vector2(0,.5f); rect.sizeDelta = itemSize; rect.anchoredPosition = new Vector2(itemSize.x*(group.Key-1),0);
            rect.localScale = Vector3.one;
            ApplyTexts(go); ApplyChineseLayout(go);
            var rows = group.ToArray();
            for (int j = 0; j < 4; j++)
            {
                var child = Child(rect, "item" + (j+1));
                child.gameObject.SetActive(j < rows.Length);
                if (j >= rows.Length) continue;
                var study = rows[j];
                child.localPosition = new Vector3(child.localPosition.x, Positions[category-1][study.location], child.localPosition.z);
                NodeObjects.Add(study.studyType, child.gameObject);
                SetNode(study, child);
            }
        }
    }
    static List<Study> ChildrenOf(Study study)
    {
        return Data.fixture.nodes.Where(s => s.preconditionStudy1 == study.studyType || s.preconditionStudy2 == study.studyType ||
            s.preconditionStudy3 == study.studyType || s.preconditionStudy4 == study.studyType).ToList();
    }
    static void SetNode(Study study, RectTransform node)
    {
        bool reached = study.preconditionStudy1 == 0 && study.preconditionStudy2 == 0 &&
            study.preconditionStudy3 == 0 && study.preconditionStudy4 == 0;
        SetSprite(One<Image>(node, "btn_research"), reached ? "ui_research[img_research_btnBgLight]" : "ui_research[img_research_btnBgDark]");
        SetSprite(One<Image>(node, "img_icon"), study.icon);
        var iconBackground = Child(node, "img_iconBg");
        var gray = AssetDatabase.LoadAssetAtPath<Material>(Data.grayMaterialPath);
        if (!gray || !gray.shader || gray.shader.name != "UI/Default Gray with Mask")
            throw new Exception("Original UI_GrayWithMask material/shader missing");
        foreach (var image in iconBackground.GetComponentsInChildren<Image>(true))
            image.material = reached ? image.defaultMaterial : gray;
        One<LanguageText>(node, "lbl_name").text = study.name;
        One<LanguageText>(node, "lbl_barText").text = "0/" + study.maxLevel;
        One<Slider>(node, "pb_bar").value = 0;
        Child(node, "pl_effect").gameObject.SetActive(false);
        var sons = ChildrenOf(study);
        var line1 = One<Image>(node, "img_line"); var line2 = One<Image>(node, "img_line2");
        line1.gameObject.SetActive(false); line2.gameObject.SetActive(false);
        if (sons.Count == 0) return;
        int line = 0; // all-unresearched source fixture; CheckSonReachPreStudy returns 0 for every required predecessor.
        var next = sons.Where(s => s.columns == study.columns+1).ToArray();
        var cross = sons.Where(s => s.columns == study.columns+2).ToArray();
        if (cross.Length == 0)
        {
            if (next.Length == 0) return;
            line1.gameObject.SetActive(true);
            if (next.Length == 1 && next[0].location != study.location)
            {
                int gap = Math.Abs(next[0].location - study.location);
                if (gap < 0 || gap >= 5) throw new Exception("Source sloped line array has no index " + gap);
                SetLine(line1, Sloped[line,gap], true);
                bool mirror = study.location > next[0].location;
                var scale = line1.rectTransform.localScale; scale.y = mirror ? -1 : Math.Abs(scale.y); line1.rectTransform.localScale = scale;
                float sign = mirror ? 1 : -1;
                var pos = line1.rectTransform.localPosition;
                pos.y += sign * (line1.sprite.rect.height / 2f) - sign * 2.5f;
                line1.rectTransform.localPosition = pos;
            }
            else SetLine(line1, Lines[study.type-1][line][next.Length], true);
        }
        else
        {
            line1.gameObject.SetActive(next.Length > 0); line2.gameObject.SetActive(true);
            if (next.Length > 0)
            {
                string name = study.studyType == 121 ? "line_research_105" :
                    (next.Length == 1 ? "line_research_108" : "line_research_115");
                SetLine(line1, name, true);
                if (next.Length == 1) { var p = line1.rectTransform.localPosition; p.y += line1.sprite.rect.height / 2f; line1.rectTransform.localPosition = p; }
            }
            if (cross.Length == 1 && cross[0].location == study.location)
            { SetLine(line2, Lines[study.type-1][line][1], false); line2.rectTransform.sizeDelta = new Vector2(540,5); }
            else if (cross.Length == 4)
            { SetLine(line2, Lines[study.type-1][line][4], false); line2.rectTransform.sizeDelta = new Vector2(540,446); }
            else line2.gameObject.SetActive(false); // Source does not bind another cross-column shape.
        }
    }
    static void SetCategory(int category, bool end)
    {
        var economy = Child(Main.transform, "sv_economy"); var military = Child(Main.transform, "sv_military");
        economy.gameObject.SetActive(category == 1); military.gameObject.SetActive(category == 2);
        var title = Data.fixture.language.First(x => x.id == (category == 1 ? 402017 : 402018)).text;
        var windowTitle = Main.GetComponentsInChildren<LanguageText>(true).FirstOrDefault(x => x.name == "lbl_title");
        if (!windowTitle) throw new Exception("Original research window title missing");
        windowTitle.text = title;
        var side = Child(Main.transform, "UI_SideBtns");
        var first = Child(side, "pl_side1"); var second = Child(side, "pl_side2");
        var toggles = new[] { first, second };
        for (int i = 0; i < 2; i++)
        {
            var toggle = toggles[i].GetComponentInChildren<GameToggle>(true);
            if (!toggle) throw new Exception("Original research side toggle missing");
            toggle.SetIsOnWithoutNotify(i == category-1);
        }
        var content = category == 1 ? EconomyContent : MilitaryContent;
        var viewport = category == 1 ? EconomyViewport : MilitaryViewport;
        Canvas.ForceUpdateCanvases();
        float endX = Mathf.Min(0, viewport.rect.width - content.rect.width);
        content.anchoredPosition = new Vector2(end ? endX : 0, content.anchoredPosition.y);
        var scroll = (category == 1 ? economy : military).GetComponent<ScrollRect>();
        if (scroll) scroll.StopMovement();
    }
    static void OpenDetail(int studyType)
    {
        if (Detail) UnityEngine.Object.DestroyImmediate(Detail);
        PowerRowObjects.Clear();
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.detailPrefab);
        var layer = Root.transform.Find("Container/WindowPopLayer");
        if (!prefab || !layer) throw new Exception("Original detail prefab/window layer missing");
        Detail = UnityEngine.Object.Instantiate(prefab);
        var r = Detail.GetComponent<RectTransform>(); var pos = r.anchoredPosition; var size = r.sizeDelta; var scale = r.localScale;
        r.SetParent(layer); r.anchoredPosition = pos; r.sizeDelta = size; r.localScale = scale;
        Detail.name = "UI_Win_ResearchUpate";
        ApplyTexts(Detail); ApplyChineseLayout(Detail);
        var study = Data.fixture.nodes.Single(n => n.studyType == studyType);
        if (study.preconditionStudy1 == 0) throw new Exception("Selected fixture detail should be prerequisite locked");
        var left = "rect/img_update/pl_left/";
        var right = "rect/img_update/pl_right/";
        SetSprite(AtComponent<Image>(Detail.transform, left + "img_icon"), study.icon);
        AtComponent<LanguageText>(Detail.transform, left + "lbl_des").text = study.description;
        AtComponent<Slider>(Detail.transform, left + "pb_bar").value = 0;
        AtComponent<LanguageText>(Detail.transform, left + "pb_bar/lbl_barText").text = "0/" + study.maxLevel;
        AtComponent<LanguageText>(Detail.transform, right + "lbl_leveLow").text = String.Format(Data.fixture.language.Single(x => x.id == 180306).text, 0);
        AtComponent<LanguageText>(Detail.transform, right + "lbl_leveLow/lbl_leveHight").text = "1";
        At(Detail.transform, right + "lbl_leveLow/img_up").gameObject.SetActive(true);
        At(Detail.transform, right + "pl_upPlane").gameObject.SetActive(true);
        AtComponent<LanguageText>(Detail.transform, right + "lbl_unlock").text = Data.fixture.language.Single(x => x.id == 402019).text;
        At(Detail.transform, right + "lbl_unlock").gameObject.SetActive(true);
        At(Detail.transform, right + "pl_condition").gameObject.SetActive(false);
        At(Detail.transform, right + "pl_speedUp").gameObject.SetActive(false);
        AtComponent<LanguageText>(Detail.transform, "UI_Model_Window_Type1/title/lbl_title").text = study.name;
        PopulateDetailAttributes(At(Detail.transform, right + "UI_Model_AttrList"), study);
        Evidence.originalDetailPrefab = true;
        DetailOpenedAt = Time.realtimeSinceStartup;
    }
    static void PopulateDetailAttributes(RectTransform attrList, Study study)
    {
        var levels = study.studyType == Data.fixture.detailStudyType ? Data.fixture.detailLevels : Data.fixture.moreLevels;
        var source = levels.Single(x => x.studyLv == 1 && x.studyType == study.studyType);
        if (source.power <= 0 || !source.desData.EndsWith("%") || source.l_buffDesID <= 0)
            throw new Exception("Fixture detail source Study level1 invalid");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.detailAttrItemPrefab);
        if (!prefab) throw new Exception("Source UI_Model_AttrItem prefab missing");
        attrList.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical, 100); // UI_Model_AttrList.RefreshUI: 40*2+20.
        var content = At(attrList, "sv_list_view/v_list_view/c_list_view");
        var old = content.rect.size;
        content.anchorMin = new Vector2(.5f,1); content.anchorMax = content.anchorMin; content.pivot = new Vector2(.5f,1);
        content.sizeDelta = new Vector2(old.x,80); content.anchoredPosition = Vector2.zero;
        var names = new[] { Data.fixture.language.Single(x => x.id == source.l_buffDesID).text,
                            Data.fixture.language.Single(x => x.id == 300005).text };
        var values = new[] { String.Format(Data.fixture.language.Single(x => x.id == 300103).text, "0.0", source.desData.Replace("%", "")),
                             String.Format(Data.fixture.language.Single(x => x.id == 300104).text, "0", source.power.ToString("N0", System.Globalization.CultureInfo.InvariantCulture)) };
        for (int i = 0; i < 2; i++)
        {
            var row = UnityEngine.Object.Instantiate(prefab, content, false);
            row.name = "UI_Model_AttrItem" + i;
            var r = row.GetComponent<RectTransform>(); var size = r.rect.size;
            r.anchorMin = new Vector2(.5f,1); r.anchorMax = r.anchorMin; r.pivot = new Vector2(.5f,1);
            r.sizeDelta = size; r.anchoredPosition = new Vector2(0,-40*i); r.localScale = Vector3.one;
            ApplyTexts(row); ApplyChineseLayout(row);
            AtComponent<LanguageText>(r,"lbl_name").text = names[i];
            AtComponent<LanguageText>(r,"layout/lbl_crrVaule").text = values[i];
            At(r,"pl_line").gameObject.SetActive(i == 0);
        }
    }
    static void OpenPower()
    {
        var study = Data.fixture.nodes.Single(n => n.studyType == Data.fixture.moreDetailStudyType);
        var levels = Data.fixture.moreLevels.OrderBy(x => x.studyLv).ToArray();
        if (levels.Length != study.maxLevel || levels.Where((x, i) => x.studyLv != i + 1).Any())
            throw new Exception("Source power levels must be complete 1..MaxLevel");
        At(Detail.transform, "rect/img_update").gameObject.SetActive(false);
        var power = At(Detail.transform, "rect/pl_power"); power.gameObject.SetActive(true);
        var powerAnimator = power.GetComponent<Animator>();
        if (!powerAnimator || !powerAnimator.runtimeAnimatorController) throw new Exception("Source pl_power Show animator missing");
        powerAnimator.Play("Show", 0, 0);
        At(Detail.transform, "UI_Model_Window_Type1/title/btn_back").gameObject.SetActive(true);
        AtComponent<LanguageText>(Detail.transform, "rect/pl_power/lbl_subDes").text =
            Data.fixture.language.Single(x => x.id == levels[0].l_buffDesID).text;
        var list = AtComponent<ListView>(Detail.transform, "rect/pl_power/sv_power");
        if (list.layoutType != 0) throw new Exception("Source sv_power must be TopToBottom");
        var content = At(Detail.transform, "rect/pl_power/sv_power/v_list_view/c_list_view");
        var size = content.rect.size;
        content.pivot = new Vector2(.5f, 1); content.anchorMin = content.pivot; content.anchorMax = content.pivot;
        content.sizeDelta = size; content.anchoredPosition = Vector2.zero;
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.powerItemPrefab);
        if (!prefab) throw new Exception("Original UI_ResearchPowerItem prefab missing");
        var rowSize = prefab.GetComponent<RectTransform>().rect.size;
        if (Mathf.Abs(rowSize.x - 1002) > .1f || Mathf.Abs(rowSize.y - 50) > .1f)
            throw new Exception("Original UI_ResearchPowerItem size changed");
        content.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical, levels.Length * rowSize.y);
        PowerRowObjects.Clear();
        for (int i = 0; i < levels.Length; i++)
        {
            var source = levels[i];
            if (String.IsNullOrEmpty(source.desData)) throw new Exception("Unexpected power item fallback text");
            var row = UnityEngine.Object.Instantiate(prefab, content, false);
            row.name = "UI_ResearchPowerItem" + i;
            var r = row.GetComponent<RectTransform>();
            r.anchorMin = new Vector2(.5f,1); r.anchorMax = r.anchorMin; r.pivot = r.anchorMin;
            r.sizeDelta = rowSize; r.anchoredPosition = new Vector2(0, -i*rowSize.y); r.localScale = Vector3.one;
            ApplyTexts(row); ApplyChineseLayout(row);
            AtComponent<LanguageText>(r, "lbl_lv").text = source.studyLv.ToString();
            AtComponent<LanguageText>(r, "lbl_ack").text = source.desData;
            AtComponent<LanguageText>(r, "lbl_power").text = "+" + source.power.ToString("N0", System.Globalization.CultureInfo.InvariantCulture);
            PowerRowObjects.Add(r);
        }
        Evidence.originalPowerItemPrefab = true;
        DetailModeChangedAt = Time.realtimeSinceStartup;
    }
    static void BackFromPower()
    {
        var update = At(Detail.transform, "rect/img_update"); update.gameObject.SetActive(true);
        At(Detail.transform, "rect/pl_power").gameObject.SetActive(false);
        var animator = update.GetComponent<Animator>();
        if (!animator || !animator.runtimeAnimatorController) throw new Exception("Source img_update Show animator missing");
        animator.Play("Show", 0, 0);
        At(Detail.transform, "UI_Model_Window_Type1/title/btn_back").gameObject.SetActive(false);
        DetailModeChangedAt = Time.realtimeSinceStartup;
    }
    static PowerRow CapturePowerRow(RectTransform r, Study level)
    {
        return new PowerRow { studyType = level.studyType, level = level.studyLv, power = level.power,
            desData = level.desData, root = Capture(r), levelLabel = Capture(At(r, "lbl_lv")),
            effectLabel = Capture(At(r, "lbl_ack")), powerLabel = Capture(At(r, "lbl_power")),
            line = Capture(At(r, "line")) };
    }
    static void Tick()
    {
        if (!EditorApplication.isPlaying || !EditorPrefs.HasKey(SessionKey)) return;
        try
        {
            Guard(); if (Evidence == null) Establish();
            if (Phase >= 8) return;
            var animator = Main.GetComponent<Animator>();
            var state = animator ? animator.GetCurrentAnimatorStateInfo(0) : new AnimatorStateInfo();
            if (animator && animator.runtimeAnimatorController && (!state.IsName("Open") || state.normalizedTime < .99f))
            {
                if (Time.realtimeSinceStartup - EstablishedAt > 15) throw new Exception("Original research Open animation not terminal");
                return;
            }
            int category = Phase < 2 ? 1 : Phase < 4 ? 2 : 1;
            bool end = Phase == 1 || Phase == 3 || Phase >= 5;
            SetCategory(category, end);
            if (Phase == 4)
            {
                if (!Detail) { OpenDetail(Data.fixture.detailStudyType); return; }
                // The source img_update has an Animator; let its native state and
                // layout settle before reading its screen geometry and pixels.
                if (Time.realtimeSinceStartup - DetailOpenedAt < .8f) return;
            }
            if (Phase == 5)
            {
                if (Detail && AtComponent<LanguageText>(Detail.transform, "UI_Model_Window_Type1/title/lbl_title").text !=
                    Data.fixture.nodes.Single(n => n.studyType == Data.fixture.moreDetailStudyType).name)
                { OpenDetail(Data.fixture.moreDetailStudyType); return; }
                if (Time.realtimeSinceStartup - DetailOpenedAt < .8f) return;
            }
            if (Phase == 6)
            {
                var power = At(Detail.transform, "rect/pl_power");
                if (!power.gameObject.activeSelf) { OpenPower(); return; }
                var powerState = power.GetComponent<Animator>().GetCurrentAnimatorStateInfo(0);
                if (!powerState.IsName("Show") || powerState.normalizedTime < .99f)
                {
                    if (Time.realtimeSinceStartup - DetailModeChangedAt > 15) throw new Exception("Source pl_power Show not terminal");
                    return;
                }
            }
            if (Phase == 7)
            {
                var power = At(Detail.transform, "rect/pl_power");
                if (power.gameObject.activeSelf) { BackFromPower(); return; }
                var updateState = AtComponent<Animator>(Detail.transform,"rect/img_update").GetCurrentAnimatorStateInfo(0);
                if (!updateState.IsName("Show") || updateState.normalizedTime < .99f)
                {
                    if (Time.realtimeSinceStartup - DetailModeChangedAt > 15) throw new Exception("Source img_update Show after Back not terminal");
                    return;
                }
            }
            foreach (var g in Root.GetComponentsInChildren<Graphic>(true)) g.SetAllDirty();
            Canvas.ForceUpdateCanvases();
            var previous = RenderTexture.active; RenderTexture.active = Target; GL.Clear(true,true,Color.clear); RenderTexture.active = previous;
            Cam.Render();
            Evidence.cameraPixelWidth = Cam.pixelWidth; Evidence.cameraPixelHeight = Cam.pixelHeight;
            if (Cam.pixelWidth != Data.width || Cam.pixelHeight != Data.height) throw new Exception("Native target resolution changed");
            var image = new Texture2D(Data.width,Data.height,TextureFormat.RGBA32,false);
            RenderTexture.active = Target; image.ReadPixels(new Rect(0,0,Data.width,Data.height),0,0); image.Apply();
            string key = Phase == 4 ? "detail_locked_102" : Phase == 5 ? "detail_locked_128" :
                Phase == 6 ? "detail_power_128" : Phase == 7 ? "detail_back_128" :
                (category == 1 ? "economy" : "military") + (end ? "_end" : "_start");
            File.WriteAllBytes(Output + "/" + key + ".png", image.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = null;
            var nodes = Data.fixture.nodes.Where(n => n.type == category).Select(n => CaptureStudy(n)).ToArray();
            var content = category == 1 ? EconomyContent : MilitaryContent;
            var viewport = category == 1 ? EconomyViewport : MilitaryViewport;
            var activeScroll = Child(Main.transform, category == 1 ? "sv_economy" : "sv_military");
            var all = Main.GetComponentsInChildren<RectTransform>(true).Select(Capture).ToArray();
            var detailAnimator = Detail ? AtComponent<Animator>(Detail.transform,
                Phase == 6 ? "rect/pl_power" : "rect/img_update") : null;
            var detailState = detailAnimator ? detailAnimator.GetCurrentAnimatorStateInfo(0) : new AnimatorStateInfo();
            var sample = new Sample { key = key, png = key + ".png", sha256 = Hash(Output + "/" + key + ".png"),
                title = Data.fixture.language.Single(x => x.id == (category == 1 ? 402017 : 402018)).text,
                category = category, totalNodeCount = nodes.Length, visibleNodeCount = nodes.Count(n => Overlap(n.root.screenRect, Capture(viewport).screenRect)),
                unityFrame = Time.frameCount, scrollX = content.anchoredPosition.x,
                contentWidth = content.rect.width, viewportWidth = viewport.rect.width, canvasScaleFactor = Canvas.scaleFactor,
                animatorNormalizedTime = state.normalizedTime, animatorState = animator && animator.runtimeAnimatorController ? "Open" : "none",
                detailAnimatorNormalizedTime = detailState.normalizedTime,
                detailAnimatorState = detailAnimator && detailAnimator.runtimeAnimatorController ? (Phase == 6 || Phase == 7 ? "Show" : "default") : "none",
                detailUpdateActive = Detail && At(Detail.transform,"rect/img_update").gameObject.activeSelf,
                detailPowerActive = Detail && At(Detail.transform,"rect/pl_power").gameObject.activeSelf,
                detailBackActive = Detail && At(Detail.transform,"UI_Model_Window_Type1/title/btn_back").gameObject.activeSelf,
                uiRoot = Capture(Root.GetComponent<RectTransform>()), mainWindow = Capture(Main.GetComponent<RectTransform>()),
                windowLayer = Capture((RectTransform)Root.transform.Find("Container/WindowLayer")),
                sidePanel = Capture(Child(Main.transform,"pl_side")), economyTab = Capture(Child(Child(Main.transform,"UI_SideBtns"),"pl_side1")),
                militaryTab = Capture(Child(Child(Main.transform,"UI_SideBtns"),"pl_side2")),
                economyScroll = Capture(Child(Main.transform,"sv_economy")), economyViewport = Capture(EconomyViewport), economyContent = Capture(EconomyContent),
                militaryScroll = Capture(Child(Main.transform,"sv_military")), militaryViewport = Capture(MilitaryViewport), militaryContent = Capture(MilitaryContent),
                detailWindow = Detail ? Capture(Detail.GetComponent<RectTransform>()) : null,
                nodes = nodes, powerRows = PowerRowObjects.Select((r,i) => CapturePowerRow(r, Data.fixture.moreLevels[i])).ToArray(),
                mainGeometry = all, detailGeometry = Detail ? Detail.GetComponentsInChildren<RectTransform>(true).Select(Capture).ToArray() : new Node[0] };
            Samples.Add(sample); Evidence.samples = Samples.ToArray();
            File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence,true));
            Phase++;
            if (Phase == 8) { Evidence.complete = true; File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence,true)); EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(0); }
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            if (Output != null) { if (Evidence == null) Evidence = new Report(); Evidence.failure = error.ToString(); Evidence.samples = Samples.ToArray();
                File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence,true)); }
            EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(1);
        }
    }
    static bool Overlap(Rect a, Rect b) { return a.xMax > b.xMin && a.xMin < b.xMax && a.yMax > b.yMin && a.yMin < b.yMax; }
    static ResearchNode CaptureStudy(Study study)
    {
        var root = NodeObjects[study.studyType].GetComponent<RectTransform>();
        bool reached = study.preconditionStudy1 == 0 && study.preconditionStudy2 == 0 && study.preconditionStudy3 == 0 && study.preconditionStudy4 == 0;
        return new ResearchNode { studyId = study.ID, studyType = study.studyType, category = study.type,
            column = study.columns, location = study.location, currentLevel = 0, maxLevel = study.maxLevel,
            iconReference = study.icon, levelText = "0/" + study.maxLevel,
            hasReachedPrerequisites = reached, sourceGrayShaderEmulated = true,
            root = Capture(root), button = Capture(Child(root,"btn_research")), icon = Capture(Child(root,"img_icon")),
            iconBackground = Capture(Child(root,"img_iconBg")), name = Capture(Child(root,"lbl_name")),
            level = Capture(Child(root,"lbl_barText")), progress = Capture(Child(root,"pb_bar")),
            line = Capture(Child(root,"img_line")), line2 = Capture(Child(root,"img_line2")) };
    }
    static Node Capture(RectTransform r)
    {
        var corners = new Vector3[4]; r.GetWorldCorners(corners);
        var quad = corners.Select(p => { var s = RectTransformUtility.WorldToScreenPoint(Cam,p); return new Vector2(s.x, Data.height-s.y); }).ToArray();
        float x0 = quad.Min(p => p.x), y0 = quad.Min(p => p.y), x1 = quad.Max(p => p.x), y1 = quad.Max(p => p.y);
        var g = r.GetComponent<Graphic>(); var image = r.GetComponent<Image>(); var label = r.GetComponent<Text>();
        var material = g ? g.materialForRendering : null;
        var node = new Node { path = PathOf(r), text = label ? label.text : "", graphicType = g ? g.GetType().FullName : "",
            spriteName = image && image.sprite ? image.sprite.name : "", spriteAssetPath = image && image.sprite ? AssetDatabase.GetAssetPath(image.sprite) : "",
            materialName = material ? material.name : "", shaderName = material && material.shader ? material.shader.name : "",
            fontName = label && label.font ? label.font.name : "", fontStyle = label ? label.fontStyle.ToString() : "",
            alignment = label ? label.alignment.ToString() : "", fontSize = label ? label.fontSize : 0,
            fontBestFit = label && label.resizeTextForBestFit,
            activeSelf = r.gameObject.activeSelf, activeInHierarchy = r.gameObject.activeInHierarchy,
            graphicEnabled = g && g.enabled, siblingIndex = r.GetSiblingIndex(), color = g ? g.color : Color.clear,
            anchorMin = r.anchorMin, anchorMax = r.anchorMax, anchoredPosition = r.anchoredPosition,
            sizeDelta = r.sizeDelta, pivot = r.pivot, localScale = r.localScale, lossyScale = r.lossyScale,
            localRect = r.rect, screenRect = new Rect(x0,y0,x1-x0,y1-y0), screenCorners = quad };
        if (label && label.text.Length > 0)
        {
            var custom = label as LanguageText;
            var generator = custom ? custom.CaptureGenerator() : label.cachedTextGenerator;
            node.fontSizeUsed = generator.fontSizeUsedForBestFit;
            var positions = generator.verts.Select(v => v.position).ToArray();
            if (positions.Length > 0)
            {
                var pts = positions.Select(v => { var s = RectTransformUtility.WorldToScreenPoint(Cam,r.TransformPoint(v)); return new Vector2(s.x,Data.height-s.y); }).ToArray();
                float gx = pts.Min(p=>p.x), gy=pts.Min(p=>p.y), gX=pts.Max(p=>p.x), gY=pts.Max(p=>p.y);
                node.glyphBoundsScreen = new Rect(gx,gy,gX-gx,gY-gy);
            }
        }
        return node;
    }
}
