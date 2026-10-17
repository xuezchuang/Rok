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

// Runs only in the isolated Unity 2019 mail reward fixture project.
// No MailMediator, network, claim button click, or account state is invoked.
[InitializeOnLoad]
public static class RenderRokMailRewardNative
{
    const string Project = "D:/ueDemo/Rok/Saved/UnityMailRewardNativeProbe";
    const string SessionKey = "RokMailRewardNative.Session";
    [Serializable] public class Entry { public int id; public string text; }
    [Serializable] public class Row { public string kind, name, icon; public int id, quantity, quality; public bool tipEnabled; }
    [Serializable] public class Fixture { public string key, role; public Row[] rows; }
    [Serializable] public class MailTemplate { public int mailDefineId, activityCalendarId, nameLocaleId, subtitleLocaleId, bodyLocaleId, activityLocaleId, claimLocaleId; public string title, subtitle, body, claim, sendTime, listIcon, poster, qualification; }
    [Serializable] public class Inputs { public string schema, scope, uiRootPrefab, windowPrefab, systemPrefab, giftPrefab, listRowPrefab, tipPrefab, qualitySprite, fixtureSourceSha256; public int width, height, probeVersion; public Entry[] texts; public Fixture[] fixtures; public string[] atlasPaths; public MailTemplate mailTemplate; }
    [Serializable] public class Node { public string path, text, graphicType, spriteName, spriteAssetPath, materialName, shaderName, buttonType, fontName, fontAssetPath, fontStyle, alignment; public bool activeSelf, activeInHierarchy, graphicEnabled, buttonInteractable, raycastTarget, canvasRendererCull, fontDynamic, fontBestFit; public int siblingIndex, fontSize, fontAssetSize, fontMinSize, fontMaxSize, generatorFontSizeUsed, generatorVertexCount, generatorCharacterCountVisible; public Color color; public Vector2 anchorMin, anchorMax, anchoredPosition, sizeDelta, pivot; public Vector3 localScale, lossyScale; public Vector2[] screenCorners; public Rect screenRect, localRect, generatorVertexBoundsLocal, generatorVertexBoundsScreen; public Vector3[] generatorVertices; }
    [Serializable] public class Reward { public string kind, iconReference, name, tipText, quantityText, clickMeaning; public int id, quantity, quality, index; public bool tipEnabled; public float tipOffsetLocalY; public Node cell, gift, icon, qualityBackground, countLabel, nameLabel, clickButton; public Rect tipAnchorScreenRect; }
    [Serializable] public class Sample { public string fixtureKey, fixtureRole, png, sha256; public int unityFrame, rowCount; public float canvasScaleFactor, windowAnimatorNormalizedTime; public string windowAnimatorState; public Node uiRoot, windowLayer, mailWindow, leftListScroll, leftListViewport, leftListContent, leftMailRow, mailScroll, mailViewport, mailContent, systemItem, giftGrid, claimButton; public Node[] selectedMailState, pageTabState, windowGeometry, leftMailRowGeometry, unselectedMailRowGeometry; public Reward[] rewards; public Node[] systemGeometry; }
    [Serializable] public class TipSample { public string rewardKind, rewardText, style, png, sha256, sourcePrefab; public int rewardIndex; public bool priorTipClosed, sourcePrefabInstantiated; public float offsetLocal, backgroundHalfHeightLocal, arrowHeightLocal; public Vector2 sourceGiftCenterScreen, sourceCanvasLocalPosition; public Node rewardGift, tipRoot, plPos, background, arrowDown, arrowUp, arrowLeft, arrowRight, textLabel; }
    [Serializable] public class Report { public string schema, scope, unityVersion, inputSha256, fixtureSourceSha256, failure; public bool complete, playMode, originalUiRoot, originalMailWindow, originalSystemItem, originalMailGift, claimNotInvoked, serverNotInvoked; public int width, height, cameraPixelWidth, cameraPixelHeight; public MailTemplate mailTemplate; public Sample[] samples; public TipSample[] tipSamples; }

    static string Output;
    static Inputs Data;
    static Report Evidence;
    static readonly List<Sample> Samples = new List<Sample>();
    static readonly List<TipSample> TipSamples = new List<TipSample>();
    static Camera Cam;
    static Canvas Canvas;
    static RenderTexture Target;
    static GameObject Root, Window, SystemItem, LeftRow, UnselectedRow;
    static GameObject ActiveTip;
    static RectTransform MailContent, GiftGrid, LeftContent;
    static int Phase;
    static float EstablishedAt;
    static float TipPreparedAt;
    static bool TipPrepared;
    static TipSample PendingTip;
    static readonly HashSet<ArabLayoutCompment> Applied = new HashSet<ArabLayoutCompment>();

    static RenderRokMailRewardNative() { EditorApplication.update -= Tick; EditorApplication.update += Tick; }
    static string Hash(string p) { using (var s = SHA256.Create()) return BitConverter.ToString(s.ComputeHash(File.ReadAllBytes(p))).Replace("-", "").ToLowerInvariant(); }
    static void Guard()
    {
        if (!String.Equals(Application.dataPath.Replace('\\', '/').TrimEnd('/'), Project + "/Assets", StringComparison.OrdinalIgnoreCase)
            || Application.unityVersion != "2019.4.28f1c1") throw new Exception("Wrong isolated Unity project/version");
        if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) throw new Exception("Native Camera.Render needs a graphics device");
        if (QualitySettings.activeColorSpace != ColorSpace.Gamma) throw new Exception("Expected source Gamma color space");
    }
    static string PathOf(Transform t)
    {
        var p = t.name;
        for (var a = t.parent; a; a = a.parent) p = a.name + "/" + p;
        return p;
    }
    static T One<T>(GameObject go, string name) where T : Component
    {
        var found = go.GetComponentsInChildren<Transform>(true).Where(t => t.name == name).ToArray();
        if (found.Length != 1) throw new Exception("Expected exactly one source " + name + " in " + go.name + "; found " + found.Length);
        var value = found[0].GetComponent<T>();
        if (!value) throw new Exception("Source " + name + " has no " + typeof(T).Name);
        return value;
    }
    static RectTransform ChildRect(Transform parent, string name)
    {
        var found = parent.GetComponentsInChildren<RectTransform>(true).Where(t => t.name == name).ToArray();
        if (found.Length != 1) throw new Exception("Expected one source " + name + " below " + PathOf(parent) + "; found " + found.Length);
        return found[0];
    }
    static Sprite SourceSprite(string reference)
    {
        var start = reference.IndexOf('[');
        if (start <= 0 || !reference.EndsWith("]")) throw new Exception("Invalid source atlas reference " + reference);
        var atlas = reference.Substring(0, start);
        var spriteName = reference.Substring(start + 1, reference.Length - start - 2);
        var path = Data.atlasPaths.Single(p => Path.GetFileNameWithoutExtension(p) == atlas);
        var choices = AssetDatabase.LoadAllAssetsAtPath(path).OfType<Sprite>().Where(s => s.name == spriteName).ToArray();
        if (choices.Length != 1) throw new Exception("Missing original atlas sprite " + reference + " in " + path + " (found " + choices.Length + ")");
        return choices[0];
    }
    static void SpriteInto(Image image, string reference)
    {
        image.sprite = SourceSprite(reference);
        image.SetAllDirty();
    }
    static void SetText(GameObject parent, string name, string text)
    {
        One<LanguageText>(parent, name).text = text;
    }
    static void SetSelectedMailWindow()
    {
        // MailMediator.InitData/UpdateMailDetail/ShowEmailContent selected system-mail state.
        One<RectTransform>(Window, "pl_mailNone").gameObject.SetActive(false);
        One<RectTransform>(Window, "pl_mailcontentbg").gameObject.SetActive(true);
        One<RectTransform>(Window, "sv_haveMail").gameObject.SetActive(true);
        One<RectTransform>(Window, "UI_Item_MailFightReport").gameObject.SetActive(false);
        One<RectTransform>(Window, "UI_Item_MailBtnOnButtomDeleteRead").gameObject.SetActive(true);
        One<RectTransform>(Window, "UI_Item_MailBtnOnButtomRead").gameObject.SetActive(true);
        // MailDefine 100030 operate = 1|2, selected EmailInfo is not collected.
        One<RectTransform>(Window, "UI_Item_MailBtnOnButtomDel").gameObject.SetActive(true);
        One<RectTransform>(Window, "UI_Item_MailBtnOnButtomCollect").gameObject.SetActive(true);
        One<RectTransform>(Window, "UI_Item_MailBtnOnButtomTrans").gameObject.SetActive(false);
        One<RectTransform>(Window, "UI_Item_MailBtnOnButtomRe").gameObject.SetActive(false);
        var tabs = new[] { "UI_Item_MailPageBtnSys", "UI_Item_MailPageBtnReport", "UI_Item_MailPageBtnGuild",
            "UI_Item_MailPageBtnCollect", "UI_Item_MailPageBtnPerson", "UI_Item_MailPageBtnSend" };
        Color selected, idle;
        if (!ColorUtility.TryParseHtmlString("#ffffff", out selected) || !ColorUtility.TryParseHtmlString("#a49d92", out idle))
            throw new Exception("Source page colors could not parse");
        for (int i = 0; i < tabs.Length; i++)
        {
            var tab = One<RectTransform>(Window, tabs[i]).gameObject;
            One<RectTransform>(tab, "img_active").gameObject.SetActive(i == 0);
            One<LanguageText>(tab, "lbl_languageText").color = i == 0 ? selected : idle;
        }
    }
    static void SetSelectedSystemMail()
    {
        // MailMediator.OnSystemEmail uses MailDefine + EmailProxy.OnTextFormat.
        // This fixed pairing is explicitly a display fixture; no EmailInfo exists.
        var title = One<RectTransform>(SystemItem, "UI_Item_MailTitle").gameObject;
        SetText(title, "lbl_title", Data.mailTemplate.title);
        SetText(title, "lbl_desc", Data.mailTemplate.subtitle);
        SetText(title, "lbl_time", Data.mailTemplate.sendTime);
        var poster = One<Image>(title, "img_bg");
        if (!String.IsNullOrEmpty(Data.mailTemplate.poster))
        {
            SpriteInto(poster, Data.mailTemplate.poster);
            poster.gameObject.SetActive(true);
        }
        else poster.gameObject.SetActive(false);
        One<RectTransform>(SystemItem, "lbl_Content_link").gameObject.SetActive(false);
        One<RectTransform>(SystemItem, "lbl_Content").gameObject.SetActive(true);
        SetText(SystemItem, "lbl_Content", Data.mailTemplate.body);
        One<RectTransform>(SystemItem, "lbl_Sender").gameObject.SetActive(false);
        One<RectTransform>(SystemItem, "pl_res").gameObject.SetActive(false);
        One<RectTransform>(SystemItem, "pl_gift").gameObject.SetActive(true);
        One<RectTransform>(SystemItem, "pl_already").gameObject.SetActive(false);
        One<RectTransform>(SystemItem, "btn_receive").gameObject.SetActive(true);
        SetText(SystemItem, "lbl_Text", Data.mailTemplate.claim);
    }
    static void FormatBeyond(LanguageText label, string value)
    {
        // Source ClientUtils.FormatBeyondText trims UTF8 Chinese glyphs and adds
        // Language 300250's {0}... when preferredWidth exceeds sizeDelta.x.
        label.text = value;
        if (label.preferredWidth <= label.rectTransform.sizeDelta.x) return;
        var copy = value;
        while (copy.Length > 1 && label.preferredWidth > label.rectTransform.sizeDelta.x)
        {
            copy = copy.Substring(0, copy.Length - 1);
            label.text = copy;
        }
        var ellipsis = Data.texts.Single(x => x.id == 300250).text;
        label.text = String.Format(ellipsis, copy);
    }
    static void SetSelectedLeftRow()
    {
        var row = LeftRow;
        One<RectTransform>(row, "img_bgActive").gameObject.SetActive(true);
        One<RectTransform>(row, "img_redpot").gameObject.SetActive(false); // Synthetic selected/read EmailInfo fixture.
        One<RectTransform>(row, "img_bag").gameObject.SetActive(true); // rewards != null, takeEnclosure == false.
        var icon = row.transform.Find("img_icon");
        if (!icon || !icon.GetComponent<Image>()) throw new Exception("Original mail list-row img_icon missing");
        icon.gameObject.SetActive(true);
        SpriteInto(icon.GetComponent<Image>(), Data.mailTemplate.listIcon);
        var player = row.GetComponentsInChildren<RectTransform>(true).FirstOrDefault(r => r.name == "UI_Model_PlayerHead");
        if (player) player.gameObject.SetActive(false); // MailDefine.type = 1, no senderInfo.
        var title = One<LanguageText>(row, "lbl_title");
        var desc = One<LanguageText>(row, "lbl_desc");
        FormatBeyond(title, Data.mailTemplate.title);
        FormatBeyond(desc, Data.mailTemplate.subtitle);
        title.color = Color.black; desc.color = Color.black; // UI_Item_MailBtn_SubView.Refresh selected colors.
        var time = One<LanguageText>(row, "lbl_time");
        time.text = Data.mailTemplate.sendTime; time.gameObject.SetActive(true);
    }
    static void SetUnselectedLeftRow()
    {
        // Source UI_Item_MailBtn_SubView.Refresh unselected branch, measured offscreen
        // so the one-row deterministic page fixture remains visually unchanged.
        var row = UnselectedRow;
        One<RectTransform>(row, "img_bgActive").gameObject.SetActive(false);
        One<RectTransform>(row, "img_redpot").gameObject.SetActive(false);
        One<RectTransform>(row, "img_bag").gameObject.SetActive(true);
        var icon = One<Image>(row, "img_icon");
        icon.gameObject.SetActive(true); SpriteInto(icon, Data.mailTemplate.listIcon);
        var player = row.GetComponentsInChildren<RectTransform>(true).FirstOrDefault(r => r.name == "UI_Model_PlayerHead");
        if (player) player.gameObject.SetActive(false);
        var title = One<LanguageText>(row, "lbl_title");
        var desc = One<LanguageText>(row, "lbl_desc");
        FormatBeyond(title, Data.mailTemplate.title);
        FormatBeyond(desc, Data.mailTemplate.subtitle);
        Color titleColor;
        if (!ColorUtility.TryParseHtmlString("#E2D19B", out titleColor)) throw new Exception("Source unselected title color invalid");
        title.color = titleColor; desc.color = Color.white;
        var time = One<LanguageText>(row, "lbl_time");
        time.text = Data.mailTemplate.sendTime; time.gameObject.SetActive(true);
    }
    static void ApplyTexts(GameObject parent)
    {
        foreach (var label in parent.GetComponentsInChildren<LanguageText>(true))
        {
            var row = Data.texts.FirstOrDefault(x => x.id == label.SourceLanguageId);
            if (row != null) label.text = row.text;
        }
    }
    static void ApplyChineseLayout(GameObject parent)
    {
        foreach (var arab in parent.GetComponentsInChildren<ArabLayoutCompment>(true))
        {
            int flags = arab.SourceFlags;
            if ((flags & ~635) != 0) throw new Exception("Unsupported original ArabLayout flags " + flags + " at " + PathOf(arab.transform));
            if (!Applied.Add(arab) || (flags & 1) != 0) continue;
            if ((flags & 2) != 0) { var r = arab.GetComponent<RectTransform>(); var min = r.anchorMin; var max = r.anchorMax; r.anchorMin = new Vector2(1-max.x,min.y); r.anchorMax = new Vector2(1-min.x,max.y); r.pivot = new Vector2(1-r.pivot.x,r.pivot.y); r.anchoredPosition = new Vector2(-r.anchoredPosition.x,r.anchoredPosition.y); }
            if ((flags & 8) != 0) { var t = arab.GetComponent<Text>(); int v = (int)t.alignment; t.alignment = (TextAnchor)(v/3*3+2-v%3); }
            if ((flags & 16) != 0) { var i = arab.GetComponent<Image>(); if (i) { var s = i.transform.localScale; s.x = -s.x; i.transform.localScale = s; } }
            if ((flags & 32) != 0) { var s = arab.GetComponent<Slider>(); if (s && (s.direction == Slider.Direction.LeftToRight || s.direction == Slider.Direction.RightToLeft)) s.direction = s.direction == Slider.Direction.LeftToRight ? Slider.Direction.RightToLeft : Slider.Direction.LeftToRight; }
            if ((flags & 64) != 0) { var r = arab.GetComponent<RectTransform>(); if (r) { var s = r.localScale; s.x = -s.x; r.localScale = s; } }
            if ((flags & 512) != 0) { var g = arab.GetComponent<GridLayoutGroup>(); g.startCorner = (GridLayoutGroup.Corner)((int)g.startCorner ^ 1); int v = (int)g.childAlignment; g.childAlignment = (TextAnchor)(v/3*3+2-v%3); }
        }
    }
    public static void Run()
    {
        Guard();
        if (EditorApplication.isPlaying) throw new Exception("Probe must start from Edit mode");
        Output = Project + "/Artifacts/native_" + DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff");
        Directory.CreateDirectory(Output);
        File.Copy(Project + "/ReferenceInputs.json", Output + "/ReferenceInputs.json");
        Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
        if (Data.probeVersion != 1 || Data.width != 1440 || Data.height != 810 || Data.fixtures.Length != 3 ||
            Data.fixtures[0].rows.Length != 1 || Data.fixtures[1].rows.Length != 7 || Data.fixtures[2].rows.Length != 0)
            throw new Exception("Unexpected native mail fixture contract");
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var rootPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.uiRootPrefab);
        var windowPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.windowPrefab);
        if (!rootPrefab || !windowPrefab) throw new Exception("Original UIRoot or UI_Win_Mail prefab missing");
        Root = (GameObject)PrefabUtility.InstantiatePrefab(rootPrefab);
        Canvas = Root.GetComponent<Canvas>();
        Cam = Root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        var layer = Root.transform.Find("Container/WindowLayer");
        if (!Canvas || !Cam || !layer) throw new Exception("Original UIRoot Canvas/Camera/WindowLayer missing");
        Canvas.worldCamera = Cam;
        Window = (GameObject)PrefabUtility.InstantiatePrefab(windowPrefab);
        var wr = Window.GetComponent<RectTransform>();
        var pos = wr.anchoredPosition; var size = wr.sizeDelta; var scale = wr.localScale;
        wr.SetParent(layer); // UIManager.AddToLayer restores these serialized values.
        wr.anchoredPosition = pos; wr.sizeDelta = size; wr.localScale = scale;
        ApplyTexts(Window);
        SetSelectedMailWindow();
        ApplyChineseLayout(Window);
        EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene(), Project + "/Assets/MailRewardNativeProbe.unity");
        EditorPrefs.SetString(SessionKey, Output);
        EditorApplication.isPlaying = true;
    }
    static void Establish()
    {
        Output = EditorPrefs.GetString(SessionKey);
        Data = JsonUtility.FromJson<Inputs>(File.ReadAllText(Output + "/ReferenceInputs.json"));
        Root = GameObject.Find("UIRoot"); Window = GameObject.Find("UI_Win_Mail");
        if (!Root || !Window) throw new Exception("PlayMode lost original mail prefab hierarchy");
        Canvas = Root.GetComponent<Canvas>();
        Cam = Root.GetComponentsInChildren<Camera>(true).FirstOrDefault();
        if (!Canvas || !Cam) throw new Exception("PlayMode lost original UI camera");
        Target = new RenderTexture(Data.width, Data.height, 24, RenderTextureFormat.ARGB32); Target.Create();
        Cam.targetTexture = Target;
        Evidence = new Report { schema = "rok.mail_reward.native.v1", scope = Data.scope, unityVersion = Application.unityVersion,
            inputSha256 = Hash(Output + "/ReferenceInputs.json"), fixtureSourceSha256 = Data.fixtureSourceSha256,
            playMode = true, width = Data.width, height = Data.height, originalUiRoot = true, originalMailWindow = true,
            originalSystemItem = true, originalMailGift = true, claimNotInvoked = true, serverNotInvoked = true,
            mailTemplate = Data.mailTemplate };
        var mailList = ChildRect(Window.transform, "sv_haveMail");
        MailContent = ChildRect(mailList, "c");
        // ListView.SetContainerLayout(TopToBottom) preserves size while changing
        // its serialized stretch anchor/pivot to center-top.
        var contentSize = MailContent.rect.size;
        MailContent.anchorMin = new Vector2(0.5f, 1); MailContent.anchorMax = new Vector2(0.5f, 1);
        MailContent.pivot = new Vector2(0.5f, 1); MailContent.sizeDelta = contentSize;
        MailContent.anchoredPosition = Vector2.zero;
        var systemPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.systemPrefab);
        if (!systemPrefab) throw new Exception("Original UI_Item_MailTypeSystem prefab missing");
        SystemItem = UnityEngine.Object.Instantiate(systemPrefab, MailContent, false);
        SystemItem.name = "UI_Item_MailTypeSystem";
        var sr = SystemItem.GetComponent<RectTransform>();
        sr.localScale = Vector3.one;
        sr.anchorMin = new Vector2(0.5f, 1); sr.anchorMax = new Vector2(0.5f, 1); sr.pivot = new Vector2(0.5f, 1);
        sr.anchoredPosition = Vector2.zero; // Source ListView first item offset 0 at center-top of c.
        GiftGrid = ChildRect(SystemItem.transform, "pl_gift");
        var grid = GiftGrid.GetComponent<GridLayoutGroup>();
        if (!grid || grid.cellSize != new Vector2(130, 140) || grid.spacing != new Vector2(20, 30) || grid.constraintCount != 5)
            throw new Exception("Source MailTypeSystem gift GridLayoutGroup changed");
        ApplyTexts(SystemItem);
        SetSelectedSystemMail();
        ApplyChineseLayout(SystemItem);
        var leftScroll = ChildRect(Window.transform, "sv_list_view");
        LeftContent = ChildRect(leftScroll, "c");
        var leftSize = LeftContent.rect.size;
        LeftContent.anchorMin = new Vector2(0.5f, 1); LeftContent.anchorMax = new Vector2(0.5f, 1);
        LeftContent.pivot = new Vector2(0.5f, 1); LeftContent.sizeDelta = leftSize;
        LeftContent.anchoredPosition = Vector2.zero;
        var leftPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.listRowPrefab);
        if (!leftPrefab) throw new Exception("Original UI_Item_MailBtn prefab missing");
        LeftRow = UnityEngine.Object.Instantiate(leftPrefab, LeftContent, false);
        LeftRow.name = "UI_Item_MailBtn";
        var lr = LeftRow.GetComponent<RectTransform>();
        var lrSize = lr.rect.size;
        lr.anchorMin = new Vector2(0.5f, 1); lr.anchorMax = new Vector2(0.5f, 1);
        lr.pivot = new Vector2(0.5f, 1); lr.sizeDelta = lrSize;
        lr.anchoredPosition = Vector2.zero; lr.localScale = Vector3.one;
        LeftContent.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical, lrSize.y);
        ApplyTexts(LeftRow);
        ApplyChineseLayout(LeftRow);
        SetSelectedLeftRow();
        UnselectedRow = UnityEngine.Object.Instantiate(leftPrefab, LeftContent, false);
        UnselectedRow.name = "UI_Item_MailBtn_UnselectedProbe";
        var ur = UnselectedRow.GetComponent<RectTransform>();
        ur.anchorMin = new Vector2(0.5f, 1); ur.anchorMax = new Vector2(0.5f, 1);
        ur.pivot = new Vector2(0.5f, 1); ur.sizeDelta = lrSize;
        ur.anchoredPosition = new Vector2(0, -700); ur.localScale = Vector3.one;
        ApplyTexts(UnselectedRow); ApplyChineseLayout(UnselectedRow); SetUnselectedLeftRow();
        Phase = 0;
        EstablishedAt = Time.realtimeSinceStartup;
    }
    static Reward BuildReward(Row row, int index)
    {
        var giftPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.giftPrefab);
        if (!giftPrefab) throw new Exception("Original UI_Model_MailGift prefab missing");
        var gift = UnityEngine.Object.Instantiate(giftPrefab);
        gift.transform.SetParent(GiftGrid); // Same order and scale assignment as MailMediator.InstantiateGift.
        gift.transform.localScale = Vector3.one;
        gift.name = "UI_Model_MailGift";
        ApplyTexts(gift);
        ApplyChineseLayout(gift);
        var bag = ChildRect(gift.transform, "UI_Item_Bag").gameObject;
        var cur = ChildRect(gift.transform, "pl_cur").gameObject;
        SetText(gift, "lbl_name", row.name);
        if (row.kind == "currency")
        {
            bag.SetActive(false); cur.SetActive(true);
            SetText(cur, "lbl_count", row.quantity.ToString("N0"));
            SpriteInto(One<Image>(cur, "img_curicon"), row.icon);
        }
        else
        {
            bag.SetActive(true); cur.SetActive(false);
            var item = ChildRect(bag.transform, "UI_Model_Item").gameObject;
            ChildRect(item.transform, "img_select").gameObject.SetActive(false);
            SetText(item, "lbl_count", row.quantity.ToString("N0"));
            ChildRect(item.transform, "pl_desc_bg").gameObject.SetActive(false);
            var quality = ChildRect(item.transform, "img_quality");
            quality.gameObject.SetActive(row.kind == "item");
            SpriteInto(One<Image>(item, "img_icon"), row.icon);
            if (row.kind == "item") SpriteInto(quality.GetComponent<Image>(), Data.qualitySprite);
        }
        return new Reward { index = index, kind = row.kind, id = row.id, quantity = row.quantity, quality = row.quality,
            iconReference = row.icon, name = row.name, tipEnabled = row.tipEnabled,
            tipText = row.tipEnabled ? row.name : "", clickMeaning = row.tipEnabled ? "HelpTip.CreateTip(name,gift.transform).SetOffset(gift.rect.height/2).Show()" : "No HelpTip binding",
            tipOffsetLocalY = row.tipEnabled ? gift.GetComponent<RectTransform>().rect.height / 2 : 0 };
    }
    static void SetFixture(Fixture fixture)
    {
        for (int i = GiftGrid.childCount - 1; i >= 0; i--) UnityEngine.Object.DestroyImmediate(GiftGrid.GetChild(i).gameObject);
        var rewards = new List<Reward>();
        for (int i = 0; i < fixture.rows.Length; i++) rewards.Add(BuildReward(fixture.rows[i], i));
        GiftGrid.gameObject.SetActive(true); // rewards != null, including empty list, in OnEnclosureList.
        LayoutRebuilder.ForceRebuildLayoutImmediate(GiftGrid);
        LayoutRebuilder.ForceRebuildLayoutImmediate(SystemItem.GetComponent<RectTransform>());
        MailContent.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical, SystemItem.GetComponent<RectTransform>().rect.height);
        Canvas.ForceUpdateCanvases();
    }
    static void PrepareTip(int index)
    {
        bool replaced = ActiveTip != null;
        if (ActiveTip) UnityEngine.Object.DestroyImmediate(ActiveTip); // HelpTip.Show closes the prior UI.s_helpTip.
        SetFixture(Data.fixtures[1]); // Both source-shaped tip examples use the mixed fixture's original gift cells.
        var row = Data.fixtures[1].rows[index];
        if (row.kind != "item" && row.kind != "soldier") throw new Exception("Only item/soldier source gifts bind HelpTip");
        var gift = GiftGrid.GetChild(index).GetComponent<RectTransform>();
        if (!gift) throw new Exception("Source MailGift anchor missing");
        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(Data.tipPrefab);
        var layer = Root.transform.Find("Container/WindowPopLayer");
        if (!prefab || !layer) throw new Exception("Original UI_Pop_TextTip or WindowPopLayer missing");
        ActiveTip = UnityEngine.Object.Instantiate(prefab);
        var tipRect = ActiveTip.GetComponent<RectTransform>();
        var originalPos = tipRect.anchoredPosition; var originalSize = tipRect.sizeDelta; var originalScale = tipRect.localScale;
        tipRect.SetParent(layer); // UIManager.AddToLayer restores original root RectTransform values.
        tipRect.anchoredPosition = originalPos; tipRect.sizeDelta = originalSize; tipRect.localScale = originalScale;
        ActiveTip.name = "UI_Pop_TextTip";
        ApplyTexts(ActiveTip); ApplyChineseLayout(ActiveTip);
        var text = One<LanguageText>(ActiveTip, "lbl_text");
        var background = One<RectTransform>(ActiveTip, "img_bg");
        var plPos = One<RectTransform>(ActiveTip, "pl_pos");
        var down = One<RectTransform>(ActiveTip, "img_arrowSideButtom");
        text.text = row.name;
        float preferred = text.preferredWidth;
        if (preferred >= 550f)
        {
            text.rectTransform.sizeDelta = new Vector2(550f, 0);
            text.rectTransform.sizeDelta = new Vector2(550f, text.preferredHeight);
        }
        else
        {
            text.rectTransform.sizeDelta = new Vector2(preferred, 0);
            text.rectTransform.sizeDelta = new Vector2(preferred, text.preferredHeight);
        }
        background.sizeDelta = text.rectTransform.sizeDelta + new Vector2(40f, 40f); // HelpTipMediator.Frame.
        foreach (var name in new[] { "img_arrowSideTop", "img_arrowSideL", "img_arrowSideR" })
            One<RectTransform>(ActiveTip, name).gameObject.SetActive(false);
        down.gameObject.SetActive(true); // HelpTipData.Style default = arrowDown.
        var center = new Vector3(gift.rect.center.x, gift.rect.center.y, gift.localPosition.z);
        var world = gift.localToWorldMatrix.MultiplyPoint(center);
        var screen = Cam.WorldToScreenPoint(world);
        Vector2 local;
        if (!RectTransformUtility.ScreenPointToLocalPointInRectangle(Root.GetComponent<RectTransform>(), screen, Cam, out local))
            throw new Exception("Source HelpTip anchor cannot be projected into original Canvas");
        plPos.anchoredPosition = local;
        float offset = gift.rect.height / 2f;
        float half = background.sizeDelta.y / 2f;
        float arrow = down.rect.height;
        plPos.anchoredPosition += new Vector2(0, offset + half + arrow); // HelpTipMediator.SetOffset arrowDown.
        Canvas.ForceUpdateCanvases();
        PendingTip = new TipSample { rewardKind = row.kind, rewardText = row.name, rewardIndex = index,
            sourcePrefab = Data.tipPrefab, style = "arrowDown", offsetLocal = offset,
            backgroundHalfHeightLocal = half, arrowHeightLocal = arrow,
            sourceGiftCenterScreen = new Vector2(screen.x, Data.height - screen.y),
            sourceCanvasLocalPosition = local, priorTipClosed = replaced,
            sourcePrefabInstantiated = true };
        TipPrepared = true;
        TipPreparedAt = Time.realtimeSinceStartup;
    }
    static void CaptureTipPhase()
    {
        int index = Phase == 3 ? 6 : 5;
        if (!TipPrepared) { PrepareTip(index); return; }
        if (Time.realtimeSinceStartup - TipPreparedAt < 0.25f) return;
        foreach (var g in ActiveTip.GetComponentsInChildren<Graphic>(true)) g.SetAllDirty();
        Canvas.ForceUpdateCanvases();
        var previous = RenderTexture.active;
        RenderTexture.active = Target; GL.Clear(true, true, Color.clear); RenderTexture.active = previous;
        Cam.Render();
        var image = new Texture2D(Data.width, Data.height, TextureFormat.RGBA32, false);
        RenderTexture.active = Target;
        image.ReadPixels(new Rect(0, 0, Data.width, Data.height), 0, 0); image.Apply();
        var filename = "tip_" + PendingTip.rewardKind + ".png";
        File.WriteAllBytes(Output + "/" + filename, image.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = null;
        PendingTip.png = filename; PendingTip.sha256 = Hash(Output + "/" + filename);
        PendingTip.rewardGift = Capture(GiftGrid.GetChild(index).GetComponent<RectTransform>());
        PendingTip.tipRoot = Capture(ActiveTip.GetComponent<RectTransform>());
        PendingTip.plPos = Capture(One<RectTransform>(ActiveTip, "pl_pos"));
        PendingTip.background = Capture(One<RectTransform>(ActiveTip, "img_bg"));
        PendingTip.arrowDown = Capture(One<RectTransform>(ActiveTip, "img_arrowSideButtom"));
        PendingTip.arrowUp = Capture(One<RectTransform>(ActiveTip, "img_arrowSideTop"));
        PendingTip.arrowLeft = Capture(One<RectTransform>(ActiveTip, "img_arrowSideL"));
        PendingTip.arrowRight = Capture(One<RectTransform>(ActiveTip, "img_arrowSideR"));
        PendingTip.textLabel = Capture(One<RectTransform>(ActiveTip, "lbl_text"));
        TipSamples.Add(PendingTip);
        Evidence.tipSamples = TipSamples.ToArray();
        File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
        Phase++; TipPrepared = false;
        if (Phase == 5)
        {
            Evidence.complete = true;
            File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(0);
        }
    }
    static void Tick()
    {
        if (!EditorApplication.isPlaying || !EditorPrefs.HasKey(SessionKey)) return;
        try
        {
            Guard();
            if (Evidence == null) Establish();
            if (Phase == 3 || Phase == 4) { CaptureTipPhase(); return; }
            if (Phase >= 5) return;
            var windowAnimator = Window.GetComponent<Animator>();
            var windowState = windowAnimator ? windowAnimator.GetCurrentAnimatorStateInfo(0) : new AnimatorStateInfo();
            if (!windowAnimator || !windowAnimator.runtimeAnimatorController || !windowState.IsName("Open") || windowState.normalizedTime < 0.99f)
            {
                if (Time.realtimeSinceStartup - EstablishedAt > 15) throw new Exception("Original mail window Open animation did not reach terminal frame");
                return;
            }
            var fixture = Data.fixtures[Phase];
            SetFixture(fixture);
            foreach (var graphic in Window.GetComponentsInChildren<Graphic>(true)) graphic.SetAllDirty();
            Canvas.ForceUpdateCanvases();
            var previous = RenderTexture.active;
            RenderTexture.active = Target; GL.Clear(true, true, Color.clear); RenderTexture.active = previous;
            Cam.Render(); // Initialize native projection before reading screen corners.
            Evidence.cameraPixelWidth = Cam.pixelWidth; Evidence.cameraPixelHeight = Cam.pixelHeight;
            if (Cam.pixelWidth != Data.width || Cam.pixelHeight != Data.height) throw new Exception("Native camera target dimensions changed");
            var image = new Texture2D(Data.width, Data.height, TextureFormat.RGBA32, false);
            RenderTexture.active = Target;
            image.ReadPixels(new Rect(0, 0, Data.width, Data.height), 0, 0); image.Apply();
            var png = fixture.key + ".png";
            File.WriteAllBytes(Output + "/" + png, image.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(image); RenderTexture.active = null;
            var itemRects = GiftGrid.GetComponentsInChildren<RectTransform>(true).Where(r => r.name == "UI_Model_MailGift" && r.parent == GiftGrid).ToArray();
            if (itemRects.Length != fixture.rows.Length) throw new Exception("Native gift count changed");
            var rewards = new List<Reward>();
            for (int i = 0; i < itemRects.Length; i++)
            {
                var r = itemRects[i]; var data = fixture.rows[i];
                var record = new Reward { index = i, kind = data.kind, id = data.id, quantity = data.quantity, quality = data.quality,
                    iconReference = data.icon, name = data.name, tipEnabled = data.tipEnabled, tipText = data.tipEnabled ? data.name : "",
                    clickMeaning = data.tipEnabled ? "HelpTip.CreateTip(name,gift.transform).SetOffset(gift.rect.height/2).Show()" : "No HelpTip binding",
                    tipOffsetLocalY = data.tipEnabled ? r.rect.height / 2 : 0,
                    gift = Capture(r), cell = Capture(r), nameLabel = Capture(ChildRect(r, "lbl_name")) };
                var bag = ChildRect(r, "UI_Item_Bag"); var cur = ChildRect(r, "pl_cur");
                if (data.kind == "currency")
                {
                    record.icon = Capture(ChildRect(cur, "img_curicon"));
                    record.countLabel = Capture(ChildRect(cur, "lbl_count"));
                    record.clickButton = Capture(ChildRect(cur, "btn_cur"));
                }
                else
                {
                    var item = ChildRect(bag, "UI_Model_Item");
                    record.icon = Capture(ChildRect(item, "img_icon"));
                    record.qualityBackground = Capture(ChildRect(item, "img_quality"));
                    record.countLabel = Capture(ChildRect(item, "lbl_count"));
                    record.clickButton = Capture(ChildRect(item, "btn_animButton"));
                }
                record.quantityText = record.countLabel.text;
                record.tipAnchorScreenRect = record.gift.screenRect;
                rewards.Add(record);
            }
            var animator = Window.GetComponent<Animator>();
            var state = animator ? animator.GetCurrentAnimatorStateInfo(0) : new AnimatorStateInfo();
            var mailList = ChildRect(Window.transform, "sv_haveMail");
            var viewport = ChildRect(mailList, "v");
            var leftList = ChildRect(Window.transform, "sv_list_view");
            var leftViewport = ChildRect(leftList, "v");
            var selectedState = new[] { "pl_mailNone", "pl_mailcontentbg", "sv_haveMail", "UI_Item_MailFightReport" }
                .Select(n => Capture(One<RectTransform>(Window, n))).Concat(
                    new[] { "UI_Item_MailBtnOnButtomDeleteRead", "UI_Item_MailBtnOnButtomRead",
                        "UI_Item_MailBtnOnButtomDel", "UI_Item_MailBtnOnButtomCollect",
                        "UI_Item_MailBtnOnButtomTrans", "UI_Item_MailBtnOnButtomRe" }
                    .Select(n => Capture(One<RectTransform>(Window, n)))).Concat(
                    new[] { "lbl_Content_link", "lbl_Content", "lbl_Sender", "pl_res", "pl_gift", "pl_already", "btn_receive", "lbl_Text" }
                    .Select(n => Capture(One<RectTransform>(SystemItem, n)))).ToArray();
            var tabs = new[] { "UI_Item_MailPageBtnSys", "UI_Item_MailPageBtnReport", "UI_Item_MailPageBtnGuild",
                "UI_Item_MailPageBtnCollect", "UI_Item_MailPageBtnPerson", "UI_Item_MailPageBtnSend" };
            var tabState = tabs.SelectMany(n => { var t = One<RectTransform>(Window, n).gameObject;
                return new[] { Capture(t.GetComponent<RectTransform>()), Capture(One<RectTransform>(t, "img_active")),
                    Capture(One<RectTransform>(t, "lbl_languageText")) }; }).ToArray();
            var sample = new Sample { fixtureKey = fixture.key, fixtureRole = fixture.role, png = png,
                sha256 = Hash(Output + "/" + png), unityFrame = Time.frameCount, rowCount = rewards.Count,
                canvasScaleFactor = Canvas.scaleFactor, windowAnimatorState = state.IsName("Open") ? "Open" : "other",
                windowAnimatorNormalizedTime = state.normalizedTime, uiRoot = Capture(Root.GetComponent<RectTransform>()),
                windowLayer = Capture((RectTransform)Root.transform.Find("Container/WindowLayer")),
                mailWindow = Capture(Window.GetComponent<RectTransform>()),
                leftListScroll = Capture(leftList), leftListViewport = Capture(leftViewport),
                leftListContent = Capture(LeftContent), leftMailRow = Capture(LeftRow.GetComponent<RectTransform>()),
                mailScroll = Capture(mailList), mailViewport = Capture(viewport),
                mailContent = Capture(MailContent), systemItem = Capture(SystemItem.GetComponent<RectTransform>()),
                giftGrid = Capture(GiftGrid), claimButton = Capture(ChildRect(SystemItem.transform, "pl_btn")),
                selectedMailState = selectedState, pageTabState = tabState, rewards = rewards.ToArray(),
                systemGeometry = SystemItem.GetComponentsInChildren<RectTransform>(true).Select(Capture).ToArray(),
                leftMailRowGeometry = LeftRow.GetComponentsInChildren<RectTransform>(true).Select(Capture).ToArray(),
                unselectedMailRowGeometry = UnselectedRow.GetComponentsInChildren<RectTransform>(true).Select(Capture).ToArray(),
                windowGeometry = Window.GetComponentsInChildren<RectTransform>(true).Select(Capture).ToArray() };
            Samples.Add(sample); Evidence.samples = Samples.ToArray();
            File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            Phase++;
        }
        catch (Exception error)
        {
            Debug.LogException(error);
            if (Output != null)
            {
                if (Evidence == null) Evidence = new Report();
                Evidence.failure = error.ToString(); Evidence.samples = Samples.ToArray();
                File.WriteAllText(Output + "/render_evidence.json", JsonUtility.ToJson(Evidence, true));
            }
            EditorPrefs.DeleteKey(SessionKey); EditorApplication.Exit(1);
        }
    }
    static Node Capture(RectTransform r)
    {
        var corners = new Vector3[4]; r.GetWorldCorners(corners);
        var quad = corners.Select(p => { var s = RectTransformUtility.WorldToScreenPoint(Cam, p); return new Vector2(s.x, Data.height - s.y); }).ToArray();
        var x0 = quad.Min(p => p.x); var y0 = quad.Min(p => p.y); var x1 = quad.Max(p => p.x); var y1 = quad.Max(p => p.y);
        var graphic = r.GetComponent<Graphic>(); var image = r.GetComponent<Image>(); var label = r.GetComponent<Text>();
        var button = r.GetComponent<Button>(); var renderer = r.GetComponent<CanvasRenderer>();
        var material = graphic ? graphic.materialForRendering : null;
        var node = new Node { path = PathOf(r), text = label ? label.text : "", graphicType = graphic ? graphic.GetType().FullName : "",
            spriteName = image && image.sprite ? image.sprite.name : "", spriteAssetPath = image && image.sprite ? AssetDatabase.GetAssetPath(image.sprite) : "",
            materialName = material ? material.name : "", shaderName = material && material.shader ? material.shader.name : "",
            buttonType = button ? button.GetType().FullName : "", fontName = label && label.font ? label.font.name : "",
            fontAssetPath = label && label.font ? AssetDatabase.GetAssetPath(label.font) : "",
            fontDynamic = label && label.font && label.font.dynamic,
            fontAssetSize = label && label.font ? label.font.fontSize : 0,
            fontBestFit = label && label.resizeTextForBestFit,
            fontMinSize = label ? label.resizeTextMinSize : 0,
            fontMaxSize = label ? label.resizeTextMaxSize : 0,
            fontStyle = label ? label.fontStyle.ToString() : "", alignment = label ? label.alignment.ToString() : "",
            activeSelf = r.gameObject.activeSelf, activeInHierarchy = r.gameObject.activeInHierarchy,
            graphicEnabled = graphic && graphic.enabled, buttonInteractable = button && button.interactable,
            raycastTarget = graphic && graphic.raycastTarget, canvasRendererCull = renderer && renderer.cull,
            siblingIndex = r.GetSiblingIndex(), fontSize = label ? label.fontSize : 0, color = graphic ? graphic.color : Color.clear,
            anchorMin = r.anchorMin, anchorMax = r.anchorMax, anchoredPosition = r.anchoredPosition,
            sizeDelta = r.sizeDelta, pivot = r.pivot, localScale = r.localScale, lossyScale = r.lossyScale,
            localRect = r.rect, screenCorners = quad, screenRect = new Rect(x0, y0, x1-x0, y1-y0) };
        if (label)
        {
            // The original font/RectTransform and Unity 2019 TextGenerator determine
            // effective BestFit size and glyph quads; no guessed UE font scaling.
            var sourceLabel = label as LanguageText;
            var generator = sourceLabel ? sourceLabel.CaptureGenerator() : label.cachedTextGenerator;
            node.generatorFontSizeUsed = generator.fontSizeUsedForBestFit;
            node.generatorCharacterCountVisible = generator.characterCountVisible;
            var verts = generator.verts;
            // Retain Unity's complete raw TextGenerator buffer. Its trailing
            // sentinel behavior varies with Populate state; trimming four
            // vertices discarded the only glyph quad for one-character text.
            var count = verts.Count;
            node.generatorVertexCount = count;
            node.generatorVertices = verts.Take(count).Select(v => v.position).ToArray();
            if (count > 0)
            {
                var lx0 = node.generatorVertices.Min(v => v.x); var ly0 = node.generatorVertices.Min(v => v.y);
                var lx1 = node.generatorVertices.Max(v => v.x); var ly1 = node.generatorVertices.Max(v => v.y);
                node.generatorVertexBoundsLocal = new Rect(lx0, ly0, lx1-lx0, ly1-ly0);
                var points = node.generatorVertices.Select(v => { var p = RectTransformUtility.WorldToScreenPoint(Cam, r.TransformPoint(v)); return new Vector2(p.x, Data.height-p.y); }).ToArray();
                var px0 = points.Min(v => v.x); var py0 = points.Min(v => v.y);
                var px1 = points.Max(v => v.x); var py1 = points.Max(v => v.y);
                node.generatorVertexBoundsScreen = new Rect(px0, py0, px1-px0, py1-py0);
            }
        }
        return node;
    }
}
