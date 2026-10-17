#include "RokAllianceTerritoryReadonlyAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokAllianceTerritoryReadonlyData.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
/** Read-only observation of Alliance's direct animated children; no Commander portrait exists here. */
struct FRokAllianceTerritoryUiSettledSample
{
    int32 DirectChildren = 0;
    int32 UnsettledChildren = 0;
    int32 NotVisibleChildren = 0;
    int32 UnsettledNotVisibleChildren = 0;
    int32 TextureCount = 0;
    int32 RemainingTextureCompilations = -1;
    bool bTextureRhiReady = false;
    bool bPortraitPresent = false;
    bool bPortraitGateApplicable = false;
    bool bStructureValid = false;
    bool bReady = false;
};

namespace RokAllianceTerritoryUiSettledProbe
{

inline FRokAllianceTerritoryUiSettledSample Sample(const URokUnityFeatureScreenWidget* Screen, bool bReference)
{
    FRokAllianceTerritoryUiSettledSample Result;
    if (!Screen || !Screen->IsScreenOpen()) return Result;
    const UCanvasPanel* Canvas = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")));
    if (!Canvas) return Result;
    Result.bPortraitPresent = Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait")) != nullptr;
    for (int32 Index = 0; Index < Canvas->GetChildrenCount(); ++Index)
    {
        const UWidget* Child = Canvas->GetChildAt(Index);
        if (!Child) return Result;
        ++Result.DirectChildren;
        const bool bVisible = Child->IsVisible();
        const bool bSettled = Child->GetRenderOpacity() >= .995f
            && Child->GetRenderTransform().Translation.IsNearlyZero(.25f);
        if (!bVisible) ++Result.NotVisibleChildren;
        if (!bSettled)
        {
            ++Result.UnsettledChildren;
            if (!bVisible) ++Result.UnsettledNotVisibleChildren;
        }
    }
    Result.bStructureValid = Result.DirectChildren > 5
        && Screen->GetWidgetFromName(TEXT("FeatureFrameClose")) != nullptr
        && (bReference ? Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceBack")) != nullptr
            : (Screen->GetWidgetFromName(TEXT("AllianceFeatureHit_1")) != nullptr || Screen->GetWidgetFromName(TEXT("AllianceWelcomeBanner")) != nullptr));
#if WITH_EDITOR
    Result.RemainingTextureCompilations = FTextureCompilingManager::Get().GetNumRemainingTextures();
#endif
    static const TCHAR* Paths[] = {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance")};
    Result.bTextureRhiReady = true;
    for (const TCHAR* Asset : Paths)
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Asset);
        const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
        if (!Resource || !Resource->TextureRHI.IsValid()) Result.bTextureRhiReady = false;
        else ++Result.TextureCount;
    }
    Result.bReady = Result.bStructureValid && Result.UnsettledChildren == 0
        && Result.RemainingTextureCompilations == 0 && Result.bTextureRhiReady
        && Result.TextureCount == UE_ARRAY_COUNT(Paths) && !Result.bPortraitPresent;
    return Result;
}

}

constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceTerritoryReadonlyAudit");

struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Exact Territory ProductV2, source chain and runtime boundaries are immutable inputs. Audit/verifier
// files are also hashed at runtime and compared against the staged release report.
constexpr FInputPin Pinned[] = {
    {TEXT("Saved/PendingAllianceTerritoryReadonlyRuntimeAuditV2/ready_for_root_review.json"), TEXT("189e60caf9f8a389ed4193d9ed7346a3c2c9436be183f12213fc75bbba29ba88")},
    {TEXT("Saved/PendingAllianceTerritoryReadonlyRuntimeAuditV2/release_report.json"), TEXT("a51c80dbb954686bf77d2208dc8fef953984ae611f85343aa026b9e9e1707d0b")},
    {TEXT("Saved/PendingAllianceTerritoryReadonlyLayoutRootIndependentReviewV3/ready_for_root_review.json"), TEXT("ed2464f37edda82e93f947ba378133e0d033f507ca3848160098d9d3a9dea8b0")},
    {TEXT("Saved/PendingAllianceTerritoryReadonlyLayoutV3/alliance_territory_layout_manifest.json"), TEXT("09fa9ceae0e145b798215b09242c2a4d37b44008d13aa1403d3e0c4786f0df03")},
    {TEXT("Saved/PendingAllianceTerritoryReadonlyProductIndependentReviewV2/ready_for_root_review.json"), TEXT("3700284f668317fb664f47f5de2c57b8752437f3e37101051e6c5454c0e519e8")},
    {TEXT("Saved/PendingAllianceTerritoryReadonlyProductV2/ready_for_root_review.json"), TEXT("9eb71e9dd0bf0dc74040cbdfa90cc56445bf2a74b3bfb5416a3619c92a07416c")},
    {TEXT("Saved/PendingAllianceTerritoryReadonlySourceIndependentReviewV1/review.json"), TEXT("f128dcecf23fc4ee374d5b3a89c8ea94491b977e6defe0e9bcd6dca7e985b9db")},
    {TEXT("Saved/PendingAllianceTerritoryReadonlySourceV1/alliance_territory_readonly_manifest.json"), TEXT("32ad80e96861e46350d4a855044307106dc8a7afd9445f7677856b0db4e981b4")},
    {TEXT("Source/Rok/RokAllianceTerritoryReadonlyData.cpp"), TEXT("a217ccb749aa8b6c1c9dac33f32f0667ed54fe3855656674a16cfe40310077a1")},
    {TEXT("Source/Rok/RokAllianceTerritoryReadonlyData.h"), TEXT("4f41df63128530fee13e7e2588c929f25c292a5ff375979faa9b52bd3028ed39")},
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("5414f983c6d8d27a3d6fb58384fbd8510a5bf471a39d0f5b896e3f74cdbfe848")},
    {TEXT("ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity"), TEXT("34c0185bef408f1b0563be0185a35de6f72d99f4d5988dc8d6904e70ccd513c7")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIEffect/UI_10067.prefab"), TEXT("a85b56dfb422f52042dc181206c67f976ebcfeb3c16ebba380a33bd14876e843")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIEffect/UI_10067.prefab.meta"), TEXT("39135ef704cb3337a43914de21416e3ba1f0453532764b620bcb1a6a173b52db")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_Redpoint.prefab"), TEXT("30beaba582c68e683edaeba69beb3b05d379030a2b0a75915faf94ebaa12417c")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_Redpoint.prefab.meta"), TEXT("c73f322a8896fcb2cdf638398c56ea6ca4a3b09eb529a1abbd0e04e4418797de")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Link.prefab"), TEXT("785a11aad10c7c4df3fea2ee83c1a1d4fc4046af90a6de612b71c95b6363b428")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Link.prefab.meta"), TEXT("0a794d4e4a8f6882c3b18bc2033849b688b636b50f4abe4d1c075ea37e19a477")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_ResourcesConsume.prefab"), TEXT("edf43e6767f7acda6b30b4f6e46229724a04d4dd2c4df02415244d94bce2173c")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_ResourcesConsume.prefab.meta"), TEXT("a43d04d6b79e51d7cd55677033fbc89b451e3fa10694f07cb59101b07bac96ee")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type3.prefab"), TEXT("a53bb716d78cdb42ac0e673522c1ba2d5bef2011c33681880eb07a78a6b6792c")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_Type3.prefab.meta"), TEXT("070224e6c3ee7480bb19010b039ea3a446dcadf47ed4d90340164f4136f460f0")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildBuild/UI_Item_GuildTerrtroyBuildingSingle.prefab"), TEXT("a7f84a7972ebaa902798dc3be7c9e85b660a392aeeee07021ba56e748dd9c359")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildBuild/UI_Item_GuildTerrtroyBuildingSingle.prefab.meta"), TEXT("e7d1a8b36109ff6d24ac705fe9d0f9f771d20c4effd6b8a15676f25d39b856bd")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Item_GuildTerrtroyBuilding.prefab"), TEXT("2cbade202686af6bc450eea70bfb2c8c40fd99252ee64564ff555fae9fb40293")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Item_GuildTerrtroyBuilding.prefab.meta"), TEXT("546dde0b9c84bd348ebcb18c8e2e009eb30bfcd94b3dffd86b1714c287dde25c")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Item_GuildTerrtroyRes.prefab"), TEXT("54579f27bd73d7f1a188072ce55d6e2a1c32afe2670d1fe6e706689b9841b060")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Item_GuildTerrtroyRes.prefab.meta"), TEXT("b6dd705f76a560abae49f9ddabe147a37a51f1a9de4dd2f82dc57b99be3776b2")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Item_GuildTerrtroyTitle.prefab"), TEXT("24fa2b5c02a77196d56d00bd61045a4150e246689c2b333a3f0b3dd84983fa79")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Item_GuildTerrtroyTitle.prefab.meta"), TEXT("91c97aac47377293919860336b0143687990ed7a06171856a4faa124e0983867")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Win_GuildTerrtroy.prefab"), TEXT("def155069979bc5db595e36f23a165110183bde1e34396a76f4309556f4e9b7b")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildTerrtroy/UI_Win_GuildTerrtroy.prefab.meta"), TEXT("fc3cab9e3a0557444d0839f9ab6346aadf0eeca1dab16dec0b827b088d500732")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIMainCity/UI_Item_AliGuide.prefab"), TEXT("976f0e46ee062d0f3ce6803a7148f655814069936da3682b7b6fba9dcc44f91d")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIMainCity/UI_Item_AliGuide.prefab.meta"), TEXT("2141b1f34292d7cac102826a8da334678c8687fad04b501478b3203918425f92")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UIButton/UI_Model_StandardButton_MiniBlue.prefab"), TEXT("dab53ed0f607d06674aa31e6d0419527d6e74ea0559addac238514130123ef74")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UIButton/UI_Model_StandardButton_MiniBlue.prefab.meta"), TEXT("4236aa1e44b7e8f40c0a9b6a7a9ecd9700b3315a004f97d759c43fb19cad2be5")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UI_Item_GuildDepotRes.prefab"), TEXT("e84dd36bfdf6ec20ffbf93effa81d72dc3c17bb72308383ee549a7ece12ecf01")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UI_Item_GuildDepotRes.prefab.meta"), TEXT("b79af9e3531a802cc0e219244e62df89c864e8dd3e447dbb61346c62284b2d2b")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UI_Model_SideToggle.prefab"), TEXT("52ab2bf0f7fd9fdbd00dd408e382862067910deffa19ffd61dff847c4bf333f6")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UI_Model_SideToggle.prefab.meta"), TEXT("6891480c656e9f677c1ee907920c161a80f9fb639f924937fafe11512785c59a")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/ui_alliance.png"), TEXT("b12eb5cd882bfd5ef59fe3fd3b0fcf933559865b1819e6f9bf384b55ecce2378")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/ui_alliance.png.meta"), TEXT("4542c703fd4bd051d141620a48cbc38b4213936242d93fc78ba8d79e47bfd25b")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/ui_alliance.tpsheet"), TEXT("a40593228f958e94763568a65b30e4e539d5c141b280727b74cd5d74c2da446b")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/ui_alliance.tpsheet.meta"), TEXT("9e08572912f26f3fa68c5d0b82fbfdeee22fa4f8682c6d0f1b7eafc1a6dbf985")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/sidepage_icon.png"), TEXT("4b966feda7f8f293bf97d4b84d5a9c5713f9f467a5e945b0b744cabb39b457fe")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/sidepage_icon.png.meta"), TEXT("9e21a86d2a41432041c2bdd7c80fee1da8e5cd667a00900117e4f615f18a0511")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png"), TEXT("3e05900aa0e68746af413c92ccd9b11212e6c0b39941e65558f6b7ec7d503bf0")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta"), TEXT("8ec301da2729ea1fc47b1bebcf8436223f92c9193fdeed9b7a6f4644be774c97")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet"), TEXT("74838bff00616d72e51f749808e49220decca4c68fd5485af1c8fdb7699fc0df")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_frame.png"), TEXT("de9321814e96e68bd15d5743e0223c6713e4e5cc91e8ce5c38e276d374bd2d57")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_frame.png.meta"), TEXT("05f4c8463988c2bf60b364dd1ff17fb3ec435344f96741931642e9ad76f1d531")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_mainmenu.png"), TEXT("fd7105be4873adf1fca32218aad73822f8a2e2237aff5c27260a6710b81432b0")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_mainmenu.png.meta"), TEXT("95e3af0cef92e834c15188f570b6724ec198cad4d8dbc0b3db5212df6d2047b8")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/map_icon.png"), TEXT("27d347a3b3d433dbd551b78870dc3412c33956a78b95bb980a129ebec5aef54c")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/map_icon.tpsheet"), TEXT("6bce871bcc08c51f80352b1dc9c4831ca48f43b06e99a234615e1efad2356717")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs"), TEXT("141eb10dcb7f5c75dd5bad094d85f50ea99acbfeec3f7c393e90b7503b969257")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs.meta"), TEXT("c60af07f472ed6548dd91bee533cd88a3982b5bf39ef57769fc8b41d01b64136")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/BtnAnimation.cs"), TEXT("d8d9196e928010704b6ed56b49b28f211fb8fb7a9f8fa5c285a55a1082b6871f")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/BtnAnimation.cs.meta"), TEXT("418a65519e76ddedb9be1f4d1946a3d5d3a763c761f13cf94a1b5e23a95f8a87")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/ListView.cs"), TEXT("684a0319aa8ffe5832254b687d9c9968b921b23313a0e62b24b6b229d7184067")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/ListView.cs.meta"), TEXT("1f833acb89243aadd000bd155d3dedbc40e38e08078bef99c8b05f0ed2d44a6f")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/UIDefaultValue.cs"), TEXT("4a9dfc331e6d342b25b017596e6f8748da1675f4ef87669392684056ac96ce5d")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/UIDefaultValue.cs.meta"), TEXT("2b01b04393fdbc50515f66d3670e295b92895924372e8d95374db39faf230817")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/GameButton.cs"), TEXT("884ffd12e591668bd1969a2ce3c61c949093c5af8d9d21eebf2ab873013ea880")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/GameButton.cs.meta"), TEXT("46ea43a2e3794c346ae556bbeff50950aa14d90fc401eb3a32296a7931551a0b")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/GameToggle.cs"), TEXT("0325a444d38317cb097c7a419e6ff115a88fcfcd9a93188c79c02c94b6cd99d6")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/GameToggle.cs.meta"), TEXT("2fdf60c8bc308ed729f8e205db7de68f05651ac5da32cdf93b842b23c72aeabf")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/LanguageText.cs"), TEXT("805cb768ee6f31bddc5fbb93edf1d01eee4843efdc9b320cce3c61456adb92d4")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/LanguageText.cs.meta"), TEXT("07903870301dfe14dddd7bdaf41a5c1796719b25c332a378bb43e0a0fdcc6549")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/PolygonImage.cs"), TEXT("d1c482a38ea0d1935475a8189e25775c4aef0b84b152f1b90afcb0f8f9c30bc8")},
    {TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/PolygonImage.cs.meta"), TEXT("ea3334437a3b9add6f7d210f4182623fa1cedf88ea8231c197deaa6b6986d48a")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/Config/AllianceBuildingTypeConfig.cs"), TEXT("0228a6a77acd1bbcece28990d78a2548531f79116121bf4a1d930409e1512075")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/AllianceProxy.cs"), TEXT("67c0dea2cb390479f764a902ef4c903f8fd14ca8898c43662d61bc6422d66dbc")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/RS.cs"), TEXT("7fffba2f364a9c4353ceeb86678def21198da51af6049b44e8d8360eb15947db")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/UI.cs"), TEXT("4706185e5962d3dd580e1f521e66af3c6d8e3eaa7e83a0d9992aae3bc433bf25")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Item_GuildTerrtroyBuildingView.cs"), TEXT("52dee62784bc7c4eaec09a29033021c8f2ae79c08db56a6f245e86f377a43e41")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Item_GuildTerrtroyTitleView.cs"), TEXT("26c8b02f70c8b6e377861a6f769773a4300ad03fce0d3e405d55ecc580c5d48b")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildTerrtroyMediator.cs"), TEXT("8060d7d43f7241f2f945a396fbf672c90bbec6ea092d81c539dfc91cd34f3960")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildTerrtroyView.cs"), TEXT("036d30ff93440eea8dff6b47ff26a8af2cc948449efae1ccf1b77e83010ecc15")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/Interface/UIManager/UIDefine.cs"), TEXT("7098de4315790a1bc3e9004d4f6e36e1da458665970f452373eaba88245a30b0")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/GUI/Empty4Raycast.cs"), TEXT("f90a2d1389d40d50604e164560f73480e217f257413fa78f9ca848b1bc905296")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/GUI/Empty4Raycast.cs.meta"), TEXT("1d57518cbc99f4e564873c5a7db95b5582966a73251e27e5d6706c09e10676d7")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab"), TEXT("26faa78145138cdf25cfcc8f0e4fe3d870f376cde004980af0bf279f6b15e8b5")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab.meta"), TEXT("95b120cfb02e78c98539c024dadbcb527daecf84afc04c05151fc2c7fb2911f3")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/UIManager.cs"), TEXT("d7c647d61e9718e6fa52ec368da26a3b9c7d8bdefdaa657c139a877b990d00aa")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/ViewBinder.cs"), TEXT("4c8bdd4a969bf76e12c563c6dba652aae77df8069c64c52459f1dd4078505dcd")},
    {TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/ViewBinder.cs.meta"), TEXT("b42e7b65f56638f1a6277729c9d53f7483de0891411eeb14a5068a4f1a708eb5")},
    {TEXT("ref/config-tables/gen/Configs.data"), TEXT("163c4ab1b8f7adc3b04ce429ea721409f3e978edd568e132e3cff0342b2d596c")},
    {TEXT("ref/config-tables/编译客户端配置/CSV/AllianceBuildingType.csv"), TEXT("89e122de945e9933ecc34e15e7aab698a77f8f49028a1a1d249ba84d0c7d8b6a")},
};

bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeDirectoryName(Engine);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		&& FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
		&& FEngineVersion::Current().GetMajor() == 5
		&& FEngineVersion::Current().GetMinor() == 8;
}
FString FileSha(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
FString Encode(const TSharedRef<FJsonObject>& Json)
{
	FString Text;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
	return Text;
}
TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}
bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	return true;
}
TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("exists"), Widget != nullptr);
	Json->SetBoolField(TEXT("visible"), Visible(Widget));
	Json->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	Json->SetStringField(TEXT("name"), Widget ? Widget->GetName() : FString());
	Json->SetStringField(TEXT("parent"), Widget && Widget->GetParent() ? Widget->GetParent()->GetName() : FString());
	Json->SetNumberField(TEXT("opacity"), Widget ? Widget->GetRenderOpacity() : 0.0);
	Json->SetObjectField(TEXT("scale"), Point(Widget ? Widget->GetRenderTransform().Scale : FVector2D::ZeroVector));
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Json->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
	Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Json->SetStringField(TEXT("text"), Text->GetText().ToString());
    if (const UImage* Icon = Cast<UImage>(Widget))
    {
        const FSlateBrush& Brush = Icon->GetBrush();
        const FBox2f UV = Brush.GetUVRegion();
        Json->SetStringField(TEXT("brush_resource"), Brush.GetResourceObject()
            ? Brush.GetResourceObject()->GetPathName() : FString());
        Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(UV.Min.X, UV.Min.Y)));
        Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(UV.Max.X, UV.Max.Y)));
    }
	return Json;
}
bool HasTag(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return false;
	for (const FName& Tag : Actor->Tags) if (Tag.ToString().StartsWith(Prefix)) return true;
	return false;
}
int32 PngDimension(const TArray<uint8>& Data, int32 Offset)
{
	return (static_cast<int32>(Data[Offset]) << 24) | (static_cast<int32>(Data[Offset + 1]) << 16)
		| (static_cast<int32>(Data[Offset + 2]) << 8) | static_cast<int32>(Data[Offset + 3]);
}
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false; // Pending-completion count is only exposed to audited Editor builds.
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokAllianceTerritoryReadonlyAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceTerritoryReadonlyAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokAllianceTerritoryReadonlyAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceTerritoryReadonlyAuditSubsystem, STATGROUP_Tickables);
}
void URokAllianceTerritoryReadonlyAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("PendingAllianceTerritoryReadonlyRuntimeAuditV3/RuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	FPaths::NormalizeDirectoryName(Directory);
	int64 CandidateTicks = FDateTime::UtcNow().GetTicks();
    while (IFileManager::Get().DirectoryExists(*Directory))
    {
        ++CandidateTicks;
        Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
            / TEXT("PendingAllianceTerritoryReadonlyRuntimeAuditV3/RuntimeAudit")
            / FString::Printf(TEXT("%lld_%u"), CandidateTicks,
                FPlatformProcess::GetCurrentProcessId()));
		FPaths::NormalizeDirectoryName(Directory);
    }
    IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokAllianceTerritoryReadonlyAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceTerritoryReadonlyAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	if (!bFinished)
	{
		Failures.Add(TEXT("World/subsystem deinitialized before Territory audit completed"));
		bFinished = true; // Stop Tick and Finish from writing another receipt during teardown.
		if (!Save(false))
		{
			Failures.Add(TEXT("Territory early-deinitialize failure receipt write failed"));
			UE_LOG(LogTemp, Error, TEXT("Territory early-deinitialize failure receipt write failed: %s"), *Directory);
		}
		Super::Deinitialize();
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	Super::Deinitialize();
}
bool URokAllianceTerritoryReadonlyAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokAllianceTerritoryReadonlyAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokAllianceTerritoryReadonlyAuditSubsystem::PrepareOwnWindowFocus()
{
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD OwnerPid = 0;
	if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
	RECT Client = {};
	if (!Expect(Handle && ::IsWindow(Handle) && OwnerPid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client) && Client.right - Client.left == 1440
		&& Client.bottom - Client.top == 810, TEXT("Audit requires own 1440x810 GameApp window"))) return false;
	if (::GetForegroundWindow() == Handle) return true;
	if (::IsIconic(Handle)) ::ShowWindow(Handle, SW_RESTORE);
	else if (!::IsWindowVisible(Handle)) ::ShowWindow(Handle, SW_SHOW);
	const HWND Foreground = ::GetForegroundWindow();
	const DWORD ForegroundThread = Foreground ? ::GetWindowThreadProcessId(Foreground, nullptr) : 0;
	const DWORD CurrentThread = ::GetCurrentThreadId();
	const bool bAttached = ForegroundThread && ForegroundThread != CurrentThread
		&& ::AttachThreadInput(CurrentThread, ForegroundThread, 1) != 0;
	::BringWindowToTop(Handle);
	::SetForegroundWindow(Handle);
	if (bAttached) ::AttachThreadInput(CurrentThread, ForegroundThread, 0);
	return ::GetForegroundWindow() == Handle;
#else
	return Expect(false, TEXT("Audit requires Windows game window"));
#endif
}



bool URokAllianceTerritoryReadonlyAuditSubsystem::Begin()
{
    if (!Expect(Identity() && FSlateApplication::IsInitialized()
        && IFileManager::Get().DirectoryExists(*Directory), TEXT("Wrong city/project/output or Slate unavailable"))) return false;
    const TCHAR* Cursor = FCommandLine::Get();
    FString Token; int32 AuditFlags = 0, LocalFlags = 0, RouteFlags = 0;
    while (FParse::Token(Cursor, Token, false))
    {
        if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditFlags; continue; }
        if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlags; continue; }
        if (Token.Equals(TEXT("-RokOpenModal=alliance_joined"), ESearchCase::IgnoreCase)) { ++RouteFlags; continue; }
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
            return Expect(false, TEXT("Unexpected Rok fixture/action flag"));
    }
    if (!Expect(AuditFlags == 1 && LocalFlags == 1 && RouteFlags == 1,
        TEXT("Exactly one territory audit, local-only and CLI joined fixture required"))) return false;
    Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
    Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    Runtime = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
    Server = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
    for (UUserWidget* Widget : Widgets) if (Widget && Widget->GetWorld() == GetWorld())
    {
        if (Hud.IsValid()) return Expect(false, TEXT("More than one GameApp HUD"));
        Hud = Cast<URokMainHudWidget>(Widget);
    }
    Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
    Modal = Hud.IsValid() ? Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget"))) : nullptr;
    if (!Expect(Controller.IsValid() && Controller->GetPawn() && Buildings.IsValid() && Runtime.IsValid()
        && Runtime->IsRuntimeStateLoaded() && Server.IsValid() && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
        && Hud.IsValid() && Screen.IsValid() && Modal.IsValid() && !Modal->IsModalOpen()
        && Screen->ActiveMode == ERokUnityFeatureScreenMode::Alliance
        && Screen->AlliancePreviewState == 2 && Screen->SelectedAlliancePage == 0
        && Screen->bAllianceTestFixtureState && !Screen->bAllianceTerritoryReferenceOpen
        && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
        TEXT("Expected anonymous local-only CLI joined GameApp HUD"))) return false;
    const auto Profile = Runtime->GetPlayerProfileState();
    if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
        && !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
        TEXT("Expected anonymous local-preview account boundary"))) return false;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("UnityInitId_5")))
    {
        if (Hall.IsValid()) return Expect(false, TEXT("Ambiguous UnityInitId_5 Hall"));
        Hall = *It;
    }
    if (!Expect(Hall.IsValid() && Hall->ActorHasTag(TEXT("UnityBuildingType_1"))
        && Hall->GetWorld() == GetWorld(), TEXT("Source Hall identity missing"))) return false;
    const auto& Rows = RokAllianceTerritoryReadonly::GetRows();
    const auto& Groups = RokAllianceTerritoryReadonly::GetGroups();
    if (!Expect(Rows.Num() == 12 && Groups.Num() == 4, TEXT("Territory static definition count differs"))) return false;
    const int32 Counts[] = {3, 4, 1, 4};
    for (int32 Group = 0; Group < 4; ++Group)
    {
        int32 Count = 0;
        for (const auto& Row : Rows) if (Row.GroupIndex == Group) ++Count;
        if (!Expect(Groups[Group].Index == Group && Count == Counts[Group], TEXT("Territory group membership differs"))) return false;
    }
    for (int32 Type = 1; Type <= 12; ++Type)
    {
        const auto* Row = RokAllianceTerritoryReadonly::FindRow(Type);
        if (!Expect(Row && Row->UnityType == Type && Row->NameLocalizationId > 0
            && Row->DescriptionLocalizationId > 0
            && !RokAllianceTerritoryReadonly::GetChineseSourceText(Row->NameLocalizationId).IsEmpty()
            && !RokAllianceTerritoryReadonly::GetChineseSourceText(Row->DescriptionLocalizationId).IsEmpty()
            && (Row->DetailLocalizationId == 0
                || !RokAllianceTerritoryReadonly::GetChineseSourceText(Row->DetailLocalizationId).IsEmpty())
            && Row->bSourceIconMissing == (Type == 7 || Type == 11),
            TEXT("Territory source ID/localization/icon state differs"))) return false;
    }
    for (const TCHAR* Asset : {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance")})
        if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr,
            TEXT("Alliance shell texture failed to load"))) return false;
    OriginalPointer = FSlateApplication::Get().GetCursorPos(); LastPointer = OriginalPointer; bPointerSaved = true;
    Inputs = MakeShared<FJsonObject>();
    for (const FInputPin& Pin : Pinned)
    {
        const FString Actual = FileSha(FPaths::ProjectDir() / Pin.Path);
        if (!Expect(Actual == Pin.Sha, TEXT("Pinned territory product/source SHA differs"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }
    for (const TCHAR* Path : {TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
        TEXT("Source/Rok/RokAllianceTerritoryReadonlyAuditSubsystem.h"),
        TEXT("Source/Rok/RokAllianceTerritoryReadonlyAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokAllianceTerritoryReadonlyRuntime.py")})
    {
        const FString Actual = FileSha(FPaths::ProjectDir() / Path);
        if (!Expect(Actual.Len() == 64, TEXT("Integrated territory audit/verifier input missing"))) return false;
        Inputs->SetStringField(Path, Actual);
    }
    CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(this,
        &URokAllianceTerritoryReadonlyAuditSubsystem::OnUpgradeCompleted);
    return true;
}

TSharedRef<FJsonObject> URokAllianceTerritoryReadonlyAuditSubsystem::BusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Runtime.IsValid() || !Buildings.IsValid() || !Server.IsValid() || !Hall.IsValid()) return Json;
	FRokBuildingState HallState;
	const bool bCached = Buildings->TryPeekExistingBuildingStateForActor(Hall.Get(), HallState);
	Json->SetBoolField(TEXT("hall_cached_exact_binding"), bCached);
	Json->SetStringField(TEXT("hall_actor_path"), Hall->GetPathName());
	Json->SetStringField(TEXT("hall_actor_class"), Hall->GetClass()->GetPathName());
	TArray<FString> Tags;
	for (const FName& Tag : Hall->Tags) Tags.Add(Tag.ToString());
	Tags.Sort();
	TArray<TSharedPtr<FJsonValue>> TagValues;
	for (const FString& Tag : Tags) TagValues.Add(MakeShared<FJsonValueString>(Tag));
	Json->SetArrayField(TEXT("hall_tags"), TagValues);
	Json->SetStringField(TEXT("hall_key"), HallState.BuildingKey.ToString());
	Json->SetStringField(TEXT("hall_display_name"), HallState.DisplayName.ToString());
	Json->SetNumberField(TEXT("hall_type"), static_cast<int32>(HallState.Type));
	Json->SetNumberField(TEXT("hall_init_id"), HallState.UnityInitId);
	Json->SetNumberField(TEXT("hall_source_type"), HallState.UnityBuildingTypeId);
	Json->SetNumberField(TEXT("hall_grid_x"), HallState.UnityGridX);
	Json->SetNumberField(TEXT("hall_grid_y"), HallState.UnityGridY);
	Json->SetNumberField(TEXT("hall_level"), HallState.Level);
	Json->SetNumberField(TEXT("hall_power"), HallState.Power);
	Json->SetNumberField(TEXT("hall_produce_per_hour"), HallState.ProducePerHour);
	Json->SetNumberField(TEXT("hall_gather_min"), HallState.GatherMin);
	Json->SetNumberField(TEXT("hall_gather_max"), HallState.GatherMax);
	Json->SetBoolField(TEXT("hall_upgrade_in_progress"), HallState.bUpgradeInProgress);
	Json->SetNumberField(TEXT("hall_upgrade_started_at"), HallState.UpgradeStartedAt);
	Json->SetNumberField(TEXT("hall_upgrade_finishes_at"), HallState.UpgradeFinishesAt);
	Json->SetStringField(TEXT("hall_icon"), HallState.Icon.ToString());
	const auto Wallet = Buildings->GetCurrentResources();
	Json->SetNumberField(TEXT("wallet_food"), Wallet.Food);
	Json->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
	Json->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
	Json->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
	const auto Rates = Buildings->GetProductionRates();
	Json->SetNumberField(TEXT("rate_food"), Rates.FoodPerHour);
	Json->SetNumberField(TEXT("rate_wood"), Rates.WoodPerHour);
	Json->SetNumberField(TEXT("rate_stone"), Rates.StonePerHour);
	Json->SetNumberField(TEXT("rate_gold"), Rates.GoldPerHour);
	Json->SetNumberField(TEXT("owned_queues"), Buildings->GetOwnedBuildQueueCount());
	Json->SetNumberField(TEXT("active_upgrades"), Buildings->GetActiveUpgradeCount());
	Json->SetNumberField(TEXT("active_construction"), Buildings->GetActiveLocalConstructionCount());
#if WITH_DEV_AUTOMATION_TESTS
	Json->SetNumberField(TEXT("pending_completed_upgrades"), Buildings->GetPendingCompletedUpgradeCountForTesting());
#else
	Json->SetNumberField(TEXT("pending_completed_upgrades"), -1);
#endif
	Json->SetNumberField(TEXT("completed_upgrade_events"), CompletionEvents);
	// IDs are consecutive from one and tickets are never removed. Absence of ID 1
	// plus zero active constructions proves this fresh-city fixture has no ticket.
	FRokLocalConstructionTicket Ticket;
	Json->SetBoolField(TEXT("construction_ticket_1_exists"), Buildings->GetLocalConstructionTicket(1, Ticket));
	TArray<FString> CityActors;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (HasTag(*It, TEXT("UnityInitId_")) || It->ActorHasTag(TEXT("RokLocalConstructedBuilding")))
			CityActors.Add(It->GetPathName());
	CityActors.Sort();
	TArray<TSharedPtr<FJsonValue>> CityValues;
	for (const FString& Path : CityActors) CityValues.Add(MakeShared<FJsonValueString>(Path));
	Json->SetArrayField(TEXT("city_actor_paths"), CityValues);
	const auto Task = Runtime->GetActiveTask();
	Json->SetNumberField(TEXT("task_id"), Task.TaskId);
	Json->SetNumberField(TEXT("task_type"), Task.Type);
	Json->SetNumberField(TEXT("task_param1"), Task.Param1);
	Json->SetNumberField(TEXT("task_param2"), Task.Param2);
	Json->SetNumberField(TEXT("task_current"), Task.Current);
	Json->SetNumberField(TEXT("task_target"), Task.Target);
	Json->SetBoolField(TEXT("task_complete"), Task.bComplete);
	Json->SetNumberField(TEXT("task_next_id"), Task.NextId);
	Json->SetNumberField(TEXT("task_reward_group"), Task.Reward);
	Json->SetStringField(TEXT("task_progress_rule"), Task.ProgressRule);
	Json->SetStringField(TEXT("task_name"), Task.Name);
	Json->SetStringField(TEXT("task_description"), Task.Desc);
	Json->SetNumberField(TEXT("task_require"), Task.Require);
	Json->SetStringField(TEXT("task_icon"), Task.Icon);
	TArray<TSharedPtr<FJsonValue>> TaskChain;
	for (const auto& Row : Runtime->GetActiveTaskChain())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Row.TaskId);
		Entry->SetStringField(TEXT("name"), Row.Name);
		Entry->SetStringField(TEXT("description"), Row.Desc);
		Entry->SetNumberField(TEXT("type"), Row.Type);
		Entry->SetNumberField(TEXT("param1"), Row.Param1);
		Entry->SetNumberField(TEXT("param2"), Row.Param2);
		Entry->SetNumberField(TEXT("require"), Row.Require);
		Entry->SetNumberField(TEXT("reward"), Row.Reward);
		Entry->SetNumberField(TEXT("next_id"), Row.NextId);
		Entry->SetStringField(TEXT("progress_rule"), Row.ProgressRule);
		Entry->SetNumberField(TEXT("current"), Row.Current);
		Entry->SetNumberField(TEXT("target"), Row.Target);
		Entry->SetBoolField(TEXT("complete"), Row.bComplete);
		Entry->SetStringField(TEXT("icon"), Row.Icon);
		TaskChain.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("task_chain"), TaskChain);
	const auto Profile = Runtime->GetPlayerProfileState();
	Json->SetStringField(TEXT("profile_kind"), Profile.StateKind);
	Json->SetBoolField(TEXT("profile_has_role_id"), Profile.bHasRoleId);
	Json->SetStringField(TEXT("profile_role_id"), Profile.RoleId);
	Json->SetBoolField(TEXT("profile_has_display_name"), Profile.bHasDisplayName);
	Json->SetStringField(TEXT("profile_display_name"), Profile.DisplayName);
	Json->SetBoolField(TEXT("profile_has_civilization"), Profile.bHasCivilization);
	Json->SetNumberField(TEXT("profile_civilization_id"), Profile.CivilizationId);
	Json->SetStringField(TEXT("profile_civilization_name"), Profile.CivilizationName);
	Json->SetNumberField(TEXT("profile_local_hero_score"), Profile.LocalHeroScore);
	Json->SetNumberField(TEXT("profile_local_troop_power"), Profile.LocalTroopPower);
	Json->SetNumberField(TEXT("profile_local_preview_score"), Profile.LocalPreviewScore);
	Json->SetNumberField(TEXT("profile_local_building_upgrade_score"), Profile.LocalBuildingUpgradeScore);
	TArray<TSharedPtr<FJsonValue>> Heroes;
	for (const auto& Hero : Runtime->GetStarterHeroes())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Hero.HeroId);
		Entry->SetNumberField(TEXT("level"), Hero.Level);
		Entry->SetNumberField(TEXT("star"), Hero.Star);
		Entry->SetNumberField(TEXT("max_star"), Hero.MaxStar);
		Entry->SetNumberField(TEXT("rare"), Hero.Rare);
		Entry->SetNumberField(TEXT("score"), Hero.Score);
		Entry->SetNumberField(TEXT("skill_count"), Hero.SkillCount);
		Entry->SetNumberField(TEXT("talent_count"), Hero.TalentCount);
		Entry->SetStringField(TEXT("icon"), Hero.Icon);
		Entry->SetStringField(TEXT("model"), Hero.Model);
		Heroes.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_heroes"), Heroes);
	TArray<TSharedPtr<FJsonValue>> Troops;
	for (const auto& Troop : Runtime->GetStarterTroopStacks())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Troop.TroopId);
		Entry->SetNumberField(TEXT("arms_type"), Troop.ArmsType);
		Entry->SetNumberField(TEXT("level"), Troop.Level);
		Entry->SetNumberField(TEXT("count"), Troop.Count);
		Entry->SetNumberField(TEXT("power_per_unit"), Troop.PowerPerUnit);
		Entry->SetNumberField(TEXT("power"), Troop.TotalPower);
		Entry->SetNumberField(TEXT("train_seconds"), Troop.TrainSeconds);
		Entry->SetStringField(TEXT("icon"), Troop.Icon);
		Entry->SetStringField(TEXT("model"), Troop.Model);
		Troops.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_troops"), Troops);
	TArray<TSharedPtr<FJsonValue>> Trainable;
	for (const int32 Id : Runtime->GetTrainableTroopIds()) Trainable.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("trainable_troop_ids"), Trainable);
	Json->SetBoolField(TEXT("server_local_only"), Server->IsLocalOnlyRun());
	Json->SetNumberField(TEXT("http_dispatch_attempts"), static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("http_suppressed_requests"), static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	return Json;
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}


TSharedRef<FJsonObject> URokAllianceTerritoryReadonlyAuditSubsystem::State() const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetObjectField(TEXT("business"), BusinessState());
    const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
#if PLATFORM_WINDOWS
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    const HWND Foreground = ::GetForegroundWindow(); DWORD OwnerPid = 0, ForegroundPid = 0; RECT Client = {};
    if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
    if (Foreground) ::GetWindowThreadProcessId(Foreground, &ForegroundPid);
    const bool bClient = Handle && ::GetClientRect(Handle, &Client);
    Json->SetBoolField(TEXT("window_foreground"), Handle && Foreground == Handle);
    Json->SetNumberField(TEXT("window_owner_pid"), OwnerPid);
    Json->SetNumberField(TEXT("foreground_owner_pid"), ForegroundPid);
    Json->SetNumberField(TEXT("client_width"), bClient ? Client.right - Client.left : 0);
    Json->SetNumberField(TEXT("client_height"), bClient ? Client.bottom - Client.top : 0);
    Json->SetStringField(TEXT("own_window_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Handle)));
    Json->SetStringField(TEXT("foreground_window_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Foreground)));
#endif
    Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
    const bool bReference = Screen.IsValid() && Screen->IsScreenOpen() && Screen->bAllianceTerritoryReferenceOpen;
    const FRokAllianceTerritoryUiSettledSample Settled = RokAllianceTerritoryUiSettledProbe::Sample(Screen.Get(), bReference);
    auto Probe = MakeShared<FJsonObject>();
    Probe->SetBoolField(TEXT("ready"), Settled.bReady);
    Probe->SetBoolField(TEXT("structure_valid"), Settled.bStructureValid);
    Probe->SetNumberField(TEXT("direct_children"), Settled.DirectChildren);
    Probe->SetNumberField(TEXT("unsettled_children"), Settled.UnsettledChildren);
    Probe->SetNumberField(TEXT("remaining_texture_compilations"), Settled.RemainingTextureCompilations);
    Probe->SetNumberField(TEXT("texture_count"), Settled.TextureCount);
    Probe->SetBoolField(TEXT("texture_rhi_ready"), Settled.bTextureRhiReady);
    Json->SetObjectField(TEXT("alliance_ui_settled"), Probe);
    const auto Placement = Controller.IsValid() ? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
    Json->SetNumberField(TEXT("primary_press_count"), static_cast<double>(Placement.PrimaryPressEventCount));
    Json->SetStringField(TEXT("selected_actor"), Placement.CurrentSelection.IsValid() ? Placement.CurrentSelection->GetPathName() : FString());
    const APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
    const FTransform Camera = Pawn ? Pawn->GetActorTransform() : FTransform::Identity;
    const FVector L = Camera.GetLocation(); const FRotator R = Camera.Rotator();
    Json->SetNumberField(TEXT("camera_x"), L.X); Json->SetNumberField(TEXT("camera_y"), L.Y);
    Json->SetNumberField(TEXT("camera_z"), L.Z); Json->SetNumberField(TEXT("camera_pitch"), R.Pitch);
    Json->SetNumberField(TEXT("camera_yaw"), R.Yaw); Json->SetNumberField(TEXT("camera_roll"), R.Roll);
    Json->SetNumberField(TEXT("camera_scale_x"), Camera.GetScale3D().X);
    Json->SetNumberField(TEXT("camera_scale_y"), Camera.GetScale3D().Y);
    Json->SetNumberField(TEXT("camera_scale_z"), Camera.GetScale3D().Z);
    auto Widgets = MakeShared<FJsonObject>();
    Widgets->SetObjectField(TEXT("AllianceFeatureButton"), WidgetState(Hud.IsValid()
        ? Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")) : nullptr));
    for (const TCHAR* Name : {TEXT("FeatureModeCanvas"), TEXT("FeatureFrameClose"), TEXT("FeatureFrameTitle"),
        TEXT("AllianceFeatureHit_1"), TEXT("AllianceTerritoryReferenceBack"),
        TEXT("AllianceTerritoryReferenceBoundary"), TEXT("AllianceTerritoryReferenceScroll"),
        TEXT("AllianceTerritoryReturnToDefinitions"), TEXT("AllianceTerritoryDefinitionDetailScroll"),
        TEXT("AllianceTerritoryDefinitionName"), TEXT("AllianceTerritoryDefinitionDescription"),
        TEXT("AllianceTerritoryDefinitionRules"), TEXT("AllianceTerritoryDefinitionIconState")})
        Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
    Json->SetObjectField(TEXT("widgets"), Widgets);
    Json->SetNumberField(TEXT("active_mode"), Screen.IsValid() ? static_cast<int32>(Screen->ActiveMode) : -1);
    Json->SetNumberField(TEXT("alliance_preview_state"), Screen.IsValid() ? Screen->AlliancePreviewState : -1);
    Json->SetNumberField(TEXT("selected_alliance_page"), Screen.IsValid() ? Screen->SelectedAlliancePage : -1);
    Json->SetBoolField(TEXT("alliance_fixture"), Screen.IsValid() && Screen->bAllianceTestFixtureState);
    Json->SetBoolField(TEXT("territory_reference_open"), bReference);
    Json->SetNumberField(TEXT("expanded_group"), Screen.IsValid() ? Screen->ExpandedAllianceTerritoryGroup : -2);
    Json->SetNumberField(TEXT("selected_type"), Screen.IsValid() ? Screen->SelectedAllianceTerritoryType : -1);
    Json->SetNumberField(TEXT("pending_action_id"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2);
    Json->SetNumberField(TEXT("definition_count"), RokAllianceTerritoryReadonly::GetRows().Num());
    Json->SetNumberField(TEXT("group_count"), RokAllianceTerritoryReadonly::GetGroups().Num());
    if (bReference && Screen.IsValid())
    {
        TArray<TSharedPtr<FJsonValue>> Groups;
        for (const auto& Group : RokAllianceTerritoryReadonly::GetGroups())
        {
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("group"), Group.Index);
            Entry->SetNumberField(TEXT("title_locale_id"), Group.TitleLocalizationId);
            Entry->SetNumberField(TEXT("header_locale_id"), Group.HeaderLocalizationId);
            Entry->SetStringField(TEXT("title"), RokAllianceTerritoryReadonly::GetChineseSourceText(Group.TitleLocalizationId));
            Entry->SetStringField(TEXT("header"), RokAllianceTerritoryReadonly::GetChineseSourceText(Group.HeaderLocalizationId));
            Entry->SetObjectField(TEXT("button"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), Group.Index))));
            Entry->SetObjectField(TEXT("title_widget"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceTerritoryGroupTitle_%d"), Group.Index))));
            Entry->SetObjectField(TEXT("header_widget"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceTerritoryGroupSummary_%d"), Group.Index))));
            Groups.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Json->SetArrayField(TEXT("groups"), Groups);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& Row : RokAllianceTerritoryReadonly::GetRows())
        {
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("unity_type"), Row.UnityType);
            Entry->SetNumberField(TEXT("group"), Row.GroupIndex);
            Entry->SetNumberField(TEXT("name_locale_id"), Row.NameLocalizationId);
            Entry->SetNumberField(TEXT("description_locale_id"), Row.DescriptionLocalizationId);
            Entry->SetNumberField(TEXT("detail_locale_id"), Row.DetailLocalizationId);
            Entry->SetStringField(TEXT("name"), RokAllianceTerritoryReadonly::GetChineseSourceText(Row.NameLocalizationId));
            Entry->SetStringField(TEXT("description"), RokAllianceTerritoryReadonly::GetChineseSourceText(Row.DescriptionLocalizationId));
            Entry->SetStringField(TEXT("detail"), Row.DetailLocalizationId > 0
                ? RokAllianceTerritoryReadonly::GetChineseSourceText(Row.DetailLocalizationId) : FString());
            Entry->SetStringField(TEXT("icon_reference"), Row.SourceIconReference);
            Entry->SetBoolField(TEXT("source_icon_missing"), Row.bSourceIconMissing);
            Entry->SetObjectField(TEXT("select"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceTerritorySelect_%d"), Row.UnityType))));
            Entry->SetObjectField(TEXT("label"), WidgetState(Screen->GetWidgetFromName(
                *(Row.GroupIndex == 3 ? FString::Printf(TEXT("AllianceTerritoryResourceName_%d"), Row.UnityType)
                    : FString::Printf(TEXT("AllianceTerritoryName_%d"), Row.UnityType)))));
            Entry->SetObjectField(TEXT("icon_state"), WidgetState(Screen->GetWidgetFromName(
                *(Row.GroupIndex == 3 ? FString::Printf(TEXT("AllianceTerritoryResourceIconState_%d"), Row.UnityType)
                    : FString::Printf(TEXT("AllianceTerritoryIconState_%d"), Row.UnityType)))));
            Rows.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Json->SetArrayField(TEXT("rows"), Rows);
    }
    return Json;
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::RowsMatchCurrent() const
{
    if (!Screen.IsValid() || !Screen->IsScreenOpen() || !Screen->bAllianceTerritoryReferenceOpen) return false;
    const UTextBlock* BoundaryText = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceBoundary")));
    if (!BoundaryText || !Visible(BoundaryText) || BoundaryText->GetText().ToString()
        != TEXT("仅显示12种静态配置；联盟实际领土、拥有数量、建筑状态、坐标、产出与权限未同步。")) return false;
    const auto& Rows = RokAllianceTerritoryReadonly::GetRows();
    const auto& Groups = RokAllianceTerritoryReadonly::GetGroups();
    if (Rows.Num() != 12 || Groups.Num() != 4) return false;
    const int32 Group = Screen->ExpandedAllianceTerritoryGroup;
    const int32 Type = Screen->SelectedAllianceTerritoryType;
    if (Type > 0)
    {
        const auto* Selected = RokAllianceTerritoryReadonly::FindRow(Type);
        if (!Selected || Selected->GroupIndex != Group) return false;
        const auto TextIs = [this](const TCHAR* Name, const FString& Text)
        {
            const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
            return Label && Visible(Label) && Label->GetText().ToString() == Text;
        };
        const FString IconText = Selected->bSourceIconMissing ? TEXT("源图标缺失")
            : (Selected->SourceIconReference[0] == 0 ? TEXT("源未配置图标") : TEXT("图标未接入"));
        const UButton* Back = Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTerritoryReturnToDefinitions")));
        const auto* Indexed = Cast<URokUnityIndexedButton>(Back);
        if (!Visible(Back) || !Indexed || Indexed->ActionId != 19630
            || !TextIs(TEXT("AllianceTerritoryDefinitionName"), RokAllianceTerritoryReadonly::GetChineseSourceText(Selected->NameLocalizationId))
            || !TextIs(TEXT("AllianceTerritoryDefinitionDescription"), RokAllianceTerritoryReadonly::GetChineseSourceText(Selected->DescriptionLocalizationId))
            || !TextIs(TEXT("AllianceTerritoryDefinitionIconState"), IconText)) return false;
        const UWidget* Detail = Screen->GetWidgetFromName(TEXT("AllianceTerritoryDefinitionRules"));
        return Selected->DetailLocalizationId > 0
            ? TextIs(TEXT("AllianceTerritoryDefinitionRules"), RokAllianceTerritoryReadonly::GetChineseSourceText(Selected->DetailLocalizationId))
            : Detail == nullptr;
    }
    if (Group < INDEX_NONE || Group > 3 || !Visible(Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceScroll")))) return false;
    for (const auto& Header : Groups)
    {
        const UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), Header.Index)));
        const auto* Indexed = Cast<URokUnityIndexedButton>(Button);
        const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceTerritoryGroupTitle_%d"), Header.Index)));
        const UTextBlock* Summary = Cast<UTextBlock>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceTerritoryGroupSummary_%d"), Header.Index)));
        if (!Visible(Button) || !Indexed || Indexed->ActionId != 19620 + Header.Index
            || !Visible(Title) || !Visible(Summary)
            || Title->GetText().ToString() != RokAllianceTerritoryReadonly::GetChineseSourceText(Header.TitleLocalizationId)
            || Summary->GetText().ToString() != RokAllianceTerritoryReadonly::GetChineseSourceText(Header.HeaderLocalizationId)) return false;
    }
    int32 VisibleCount = 0;
    for (const auto& Row : Rows)
    {
        const UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceTerritorySelect_%d"), Row.UnityType)));
        if (Row.GroupIndex != Group) { if (Button) return false; continue; }
        const auto* Indexed = Cast<URokUnityIndexedButton>(Button);
        const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(
            *(Row.GroupIndex == 3 ? FString::Printf(TEXT("AllianceTerritoryResourceName_%d"), Row.UnityType)
                : FString::Printf(TEXT("AllianceTerritoryName_%d"), Row.UnityType))));
        const UTextBlock* Icon = Cast<UTextBlock>(Screen->GetWidgetFromName(
            *(Row.GroupIndex == 3 ? FString::Printf(TEXT("AllianceTerritoryResourceIconState_%d"), Row.UnityType)
                : FString::Printf(TEXT("AllianceTerritoryIconState_%d"), Row.UnityType))));
        const FString ExpectedIcon = Row.bSourceIconMissing ? TEXT("源图标缺失")
            : (Row.SourceIconReference[0] == 0 ? TEXT("源未配置图标") : TEXT("图标未接入"));
        if (!Visible(Button) || !Indexed || Indexed->ActionId != 19600 + Row.UnityType
            || !Label || !Visible(Label) || Label->GetText().ToString()
                != RokAllianceTerritoryReadonly::GetChineseSourceText(Row.NameLocalizationId)
            || !Icon || !Visible(Icon) || Icon->GetText().ToString() != ExpectedIcon) return false;
        const FGeometry Geometry = Button->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0 || Geometry.GetLocalSize().Y <= 0) return false;
        ++VisibleCount;
    }
    const int32 Expected[] = {3, 4, 1, 4};
    return Group == INDEX_NONE ? VisibleCount == 0 : VisibleCount == Expected[Group];
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::Ready(int32 Phase) const
{
    if (!Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Runtime.IsValid()
        || !Server.IsValid() || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
        || Server->GetSuppressedLocalRequestCount() != 0 || Modal->IsModalOpen()) return false;
    if (Phase == 6) return !Screen->IsScreenOpen()
        && Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")));
    const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Screen->IsScreenOpen() || !Mode || !Visible(Mode)
        || !FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0, .01)
        || !Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01)
        || !Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const bool bReference = Phase == 1 || Phase == 2 || Phase == 3 || Phase == 5;
    if (!RokAllianceTerritoryUiSettledProbe::Sample(Screen.Get(), bReference).bReady) return false;
    if (Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance || Screen->SelectedAlliancePage != 0
        || Screen->bAllianceTerritoryReferenceOpen != bReference
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE
        || Screen->AlliancePreviewState != 2 || !Screen->bAllianceTestFixtureState) return false;
    const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureFrameTitle")));
    if (!Title || Title->GetText().ToString() != (bReference ? TEXT("联盟领土静态配置参考") : TEXT("联盟"))) return false;
    if (!bReference) return Visible(Screen->GetWidgetFromName(TEXT("AllianceFeatureHit_1")));
    if (Phase == 1 || Phase == 5)
        return Screen->ExpandedAllianceTerritoryGroup == INDEX_NONE
            && Screen->SelectedAllianceTerritoryType == 0 && RowsMatchCurrent();
    if (Phase == 2) return Screen->ExpandedAllianceTerritoryGroup >= 0
        && Screen->ExpandedAllianceTerritoryGroup <= 3
        && Screen->SelectedAllianceTerritoryType == 0 && RowsMatchCurrent();
    return Phase == 3 && Screen->ExpandedAllianceTerritoryGroup >= 0
        && Screen->SelectedAllianceTerritoryType >= 1
        && Screen->SelectedAllianceTerritoryType <= 12 && RowsMatchCurrent();
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::RecordEffect(int32 ExpectedGroup, int32 ExpectedItem, bool bOpen)
{
    if (Effects.Num() == Actions.Num()) return true;
    if (GFrameCounter <= EffectFrame || !Screen.IsValid()
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE) return false;
    if (bOpen && !RokAllianceTerritoryUiSettledProbe::Sample(Screen.Get(), true).bReady) return false;
    const bool bClosed = !Screen->IsScreenOpen();
    const bool bState = bClosed ? !bOpen
        : (Screen->bAllianceTerritoryReferenceOpen == bOpen
            && Screen->ExpandedAllianceTerritoryGroup == ExpectedGroup
            && Screen->SelectedAllianceTerritoryType == ExpectedItem
            && (!bOpen || RowsMatchCurrent()));
    if (!Expect(bState && Boundary(), TEXT("Next-tick Territory action effect or immutable business failed"))) return false;
    auto Receipt = MakeShared<FJsonObject>();
    Receipt->SetNumberField(TEXT("action_index"), Actions.Num() - 1);
    Receipt->SetNumberField(TEXT("action_frame"), static_cast<double>(EffectFrame));
    Receipt->SetNumberField(TEXT("effect_frame"), static_cast<double>(GFrameCounter));
    Receipt->SetObjectField(TEXT("state"), State());
    Effects.Add(MakeShared<FJsonValueObject>(Receipt));
    return true;
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::TargetInSlatePath(UButton* Target) const
{
    if (!Target || !Visible(Target) || !Target->GetIsEnabled()) return false;
    const FGeometry Geometry = Target->GetCachedGeometry();
    if (Geometry.GetLocalSize().X <= 0 || Geometry.GetLocalSize().Y <= 0) return false;
    const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
    FSlateApplication& App = FSlateApplication::Get();
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
    if (!Path.IsValid()) return false;
    const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
    for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        if (&Path.Widgets[Index].Widget.Get() == &ButtonSlate.Get()) return true;
    return false;
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::WheelListTowardsTarget(UButton* Target)
{
    UScrollBox* Scroll = Screen.IsValid() ? Cast<UScrollBox>(Screen->GetWidgetFromName(
        TEXT("AllianceTerritoryReferenceScroll"))) : nullptr;
    if (!Expect(Scroll && Visible(Scroll) && Target && WheelEvents < 24 && Boundary(),
        TEXT("Territory target scroll unavailable"))) return false;
    const FGeometry Geometry = Scroll->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Territory scroll has zero geometry"))) return false;
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    const FGeometry TargetGeometry = Target->GetCachedGeometry();
    const FVector2D TargetCenter = TargetGeometry.LocalToAbsolute(TargetGeometry.GetLocalSize() * .5f);
    const float WheelDelta = TargetCenter.Y < Geometry.LocalToAbsolute(FVector2D::ZeroVector).Y ? 3.0f : -3.0f;
    FSlateApplication& App = FSlateApplication::Get();
    App.SetCursorPos(Center);
    App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer,
        TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
    const TSharedRef<SWidget> ScrollSlate = Scroll->TakeWidget();
    bool bScrollInPath = false;
    TArray<TSharedPtr<FJsonValue>> PathTypes;
    for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
    {
        const SWidget* Node = &Path.Widgets[Index].Widget.Get();
        PathTypes.Add(MakeShared<FJsonValueString>(Node->GetTypeAsString()));
        bScrollInPath |= Node == &ScrollSlate.Get();
    }
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
    bool bForeground = false; uint32 OwnerPid = 0;
#if PLATFORM_WINDOWS
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    if (Handle) { DWORD Pid = 0; ::GetWindowThreadProcessId(Handle, &Pid); OwnerPid = Pid; }
    bForeground = Handle && ::GetForegroundWindow() == Handle
        && OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
    if (!Expect(bScrollInPath && bOwnWindow && bForeground,
        TEXT("Territory scroll not exact foreground Slate path"))) return false;
    const float Before = Scroll->GetScrollOffset();
    const uint64 PressBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    auto Receipt = MakeShared<FJsonObject>();
    Receipt->SetStringField(TEXT("method"), TEXT("ProcessMouseWheelOrGestureEvent"));
    Receipt->SetStringField(TEXT("target"), Target->GetName());
    Receipt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Receipt->SetNumberField(TEXT("group"), GroupCursor);
    Receipt->SetNumberField(TEXT("row_cursor"), RowCursor);
    Receipt->SetObjectField(TEXT("absolute_center"), Point(Center));
    Receipt->SetObjectField(TEXT("scroll_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
    Receipt->SetObjectField(TEXT("scroll_bottom_right"), Point(Geometry.LocalToAbsolute(Size)));
    Receipt->SetObjectField(TEXT("scroll_local_size"), Point(Size));
    Receipt->SetArrayField(TEXT("hit_path_types"), PathTypes);
    Receipt->SetBoolField(TEXT("exact_scroll_in_path"), bScrollInPath);
    Receipt->SetBoolField(TEXT("own_game_window"), bOwnWindow);
    Receipt->SetBoolField(TEXT("foreground"), bForeground);
    Receipt->SetNumberField(TEXT("own_window_pid"), OwnerPid);
    Receipt->SetNumberField(TEXT("wheel_delta"), WheelDelta);
    Receipt->SetNumberField(TEXT("scroll_before"), Before);
    Receipt->SetObjectField(TEXT("business_before"), BusinessState());
    const bool bHandled = App.ProcessMouseWheelOrGestureEvent(
        FPointerEvent(0, Center, Center, TSet<FKey>(), EKeys::Invalid, WheelDelta,
            FModifierKeysState()), nullptr);
    Receipt->SetBoolField(TEXT("wheel_handled"), bHandled);
    Receipt->SetNumberField(TEXT("scroll_after"), Scroll->GetScrollOffset());
    Receipt->SetObjectField(TEXT("business_after"), BusinessState());
    Receipt->SetNumberField(TEXT("primary_before"), static_cast<double>(PressBefore));
    Receipt->SetNumberField(TEXT("primary_after"), static_cast<double>(
        Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
    const bool bSafe = bHandled && (Scroll->GetScrollOffset() - Before) * (-WheelDelta) > 0
        && Boundary()
        && PressBefore == Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    Receipt->SetStringField(TEXT("failure"), bSafe ? TEXT("") : TEXT("unhandled wheel/no movement/business changed"));
    Scrolls.Add(MakeShared<FJsonValueObject>(Receipt));
    ++WheelEvents;
    return Expect(bSafe, TEXT("Actual Territory Slate wheel failed"));
}

void URokAllianceTerritoryReadonlyAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Alliance Territory static-reference route emitted an upgrade completion event"));
}

bool URokAllianceTerritoryReadonlyAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
{
	auto Attempt = MakeShared<FJsonObject>();
	Attempt->SetStringField(TEXT("label"), Label);
	Attempt->SetNumberField(TEXT("step"), Step);
	Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Attempt->SetStringField(TEXT("target"), Target ? Target->GetName() : FString());
	Attempt->SetBoolField(TEXT("target_exists"), Target != nullptr);
	if (!Target || !Visible(Target) || !Target->GetIsEnabled())
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("target missing, disabled, or hidden"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Slate target unavailable"));
	}
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
	if (Size.X <= 0 || Size.Y <= 0)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("target has zero cached geometry"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Slate target has zero geometry"));
	}
	if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
		|| !StableBottomRight.Equals(BottomRight, .25f))
	{
		StableTarget = Target;
		StableTopLeft = TopLeft;
		StableBottomRight = BottomRight;
		StableFrame = GFrameCounter;
		ClickStableFrames = 1;
		return false;
	}
	if (GFrameCounter <= StableFrame) return false;
	StableFrame = GFrameCounter;
	if (++ClickStableFrames < 2) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30, -30));
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
	int32 ButtonIndex = INDEX_NONE;
	TArray<TSharedPtr<FJsonValue>> PathTypes;
	TArray<TSharedPtr<FJsonValue>> PathNodes;
	bool bUmgPath = false;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		const SWidget* Node = &Path.Widgets[Index].Widget.Get();
		const FString Type = Node->GetTypeAsString();
		PathTypes.Add(MakeShared<FJsonValueString>(Type));
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("index"), Index);
		Entry->SetStringField(TEXT("type"), Type);
		Entry->SetStringField(TEXT("slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Node)));
		PathNodes.Add(MakeShared<FJsonValueObject>(Entry));
		bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
		if (Node == &ButtonSlate.Get()) ButtonIndex = Index;
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = false;
	uint32 OwnerPid = 0;
	FString OwnWindowId;
	FString ForegroundWindowId;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND OwnHandle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	if (OwnHandle) { DWORD NativePid = 0; ::GetWindowThreadProcessId(OwnHandle, &NativePid); OwnerPid = NativePid; }
	OwnWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(OwnHandle));
	ForegroundWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
	bForeground = OwnHandle && ::GetForegroundWindow() == OwnHandle
		&& OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
	const bool bExactButton = ButtonIndex != INDEX_NONE
		&& Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
	Attempt->SetStringField(TEXT("method"), TEXT("ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
	Attempt->SetObjectField(TEXT("absolute_center"), Point(Center));
	Attempt->SetObjectField(TEXT("absolute_top_left"), Point(TopLeft));
	Attempt->SetObjectField(TEXT("absolute_bottom_right"), Point(BottomRight));
	Attempt->SetObjectField(TEXT("local_size"), Point(Size));
	Attempt->SetArrayField(TEXT("hit_path_types"), PathTypes);
	Attempt->SetArrayField(TEXT("hit_path_nodes"), PathNodes);
	Attempt->SetStringField(TEXT("terminal_type"), Path.IsValid() && Path.Widgets.Num()
		? Path.Widgets.Last().Widget->GetTypeAsString() : FString());
	Attempt->SetNumberField(TEXT("exact_button_path_index"), ButtonIndex);
	Attempt->SetBoolField(TEXT("exact_button_in_path"), bExactButton);
	Attempt->SetBoolField(TEXT("umg_path_present"), bUmgPath);
	Attempt->SetStringField(TEXT("target_widget_class"), Target->GetClass()->GetPathName());
	const URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
	Attempt->SetNumberField(TEXT("action_id"), Indexed ? Indexed->ActionId : 0);
	Attempt->SetStringField(TEXT("target_widget_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Target)));
	Attempt->SetStringField(TEXT("target_slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(&ButtonSlate.Get())));
	Attempt->SetBoolField(TEXT("target_hovered"), Target->IsHovered());
	Attempt->SetBoolField(TEXT("own_game_window"), bOwnWindow);
	Attempt->SetBoolField(TEXT("foreground"), bForeground);
	Attempt->SetStringField(TEXT("own_window_identity"), OwnWindowId);
	Attempt->SetStringField(TEXT("foreground_window_identity"), ForegroundWindowId);
	Attempt->SetNumberField(TEXT("own_window_pid"), OwnerPid);
	Attempt->SetNumberField(TEXT("stable_frames"), ClickStableFrames);
	const uint64 Before = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetNumberField(TEXT("primary_before"), static_cast<double>(Before));
	Attempt->SetObjectField(TEXT("business_before"), BusinessState());
	Attempt->SetBoolField(TEXT("screen_open_before"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_before"), Modal.IsValid() && Modal->IsModalOpen());
	if (!Path.IsValid() || !bExactButton || !bUmgPath || !Target->IsHovered() || !bOwnWindow || !bForeground)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("Slate path/hover/window did not prove exact UButton"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Exact target button was not hit in foreground game window"));
	}
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const uint64 After = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetBoolField(TEXT("down_handled"), Down);
	Attempt->SetBoolField(TEXT("up_handled"), Up);
	Attempt->SetNumberField(TEXT("primary_after"), static_cast<double>(After));
	Attempt->SetObjectField(TEXT("business_after"), BusinessState());
	Attempt->SetBoolField(TEXT("screen_open_after"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_after"), Modal.IsValid() && Modal->IsModalOpen());
	const int32 PendingAfter = Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : INDEX_NONE;
	Attempt->SetNumberField(TEXT("pending_action_id_after"), PendingAfter);
	const bool bDeferred = Indexed ? PendingAfter == Indexed->ActionId : PendingAfter == INDEX_NONE;
	const bool bRouteSafe = Down && Up && bDeferred && Before == After && (!bBaselineSet || Boundary());
	Attempt->SetStringField(TEXT("failure"), bRouteSafe ? TEXT("") : TEXT("unhandled, world input, or business mutation"));
	Actions.Add(MakeShared<FJsonValueObject>(Attempt));
	StableTarget.Reset(); ClickStableFrames = 0;
	return Expect(bRouteSafe, TEXT("Slate click failed or changed baseline state"));
}




void URokAllianceTerritoryReadonlyAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
    if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
        && bBaselineSet && Boundary(), TEXT("Screenshot request/boundary invalid"))) return;
    Pending = MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("phase"), Phase);
    Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
    Pending->SetObjectField(TEXT("requested_state"), State());
    PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
    Pending->SetStringField(TEXT("png"), PendingPath);
    NextStep = After; bScreenshotDone = false;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}

void URokAllianceTerritoryReadonlyAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	TArray<uint8> Bytes;
	if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
		&& FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
		&& FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
		&& PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
		&& (!bBaselineSet || Boundary()), TEXT("Processed screenshot PNG/extent/business state invalid"))) return;
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(Bytes.Num()));
	Pending->SetNumberField(TEXT("png_width"), 1440);
	Pending->SetNumberField(TEXT("png_height"), 810);
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("processed_state"), State());
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokAllianceTerritoryReadonlyAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(!bBaselineSet || Boundary(), TEXT("Post-screenshot business state changed"))) return;
	Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	if (!Save(false))
	{
		Finish(TEXT("Territory in-progress manifest write failed"));
		return;
	}
	Enter(NextStep);
}
void URokAllianceTerritoryReadonlyAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}


bool URokAllianceTerritoryReadonlyAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_territory_readonly.runtime_audit.v3"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 21
        && Actions.Num() == 32 && Effects.Num() == 32
        && Scrolls.Num() == WheelEvents && WheelEvents > 0 && WheelEvents <= 24
        && Attempts.IsEmpty() && CompletionEvents == 0 && bBaselineSet && Boundary());
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetStringField(TEXT("output_directory"), Directory);
    Json->SetStringField(TEXT("scope"), TEXT("anonymous CLI joined fixture; real Slate local static territory definition navigation only"));
    Json->SetStringField(TEXT("entry_route"), TEXT("-RokOpenModal=alliance_joined"));
    Json->SetStringField(TEXT("audit_opt_in"), AuditFlag);
    Json->SetBoolField(TEXT("owned_territory_accepted"), false);
    Json->SetBoolField(TEXT("territory_layout_visual_parity_accepted"), false);
    Json->SetBoolField(TEXT("account_alliance_state_accepted"), false);
    Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
    Json->SetNumberField(TEXT("definition_count"), RokAllianceTerritoryReadonly::GetRows().Num());
    Json->SetNumberField(TEXT("group_count"), RokAllianceTerritoryReadonly::GetGroups().Num());
    Json->SetNumberField(TEXT("completion_events"), CompletionEvents);
    Json->SetNumberField(TEXT("typing_events"), 0);
    Json->SetNumberField(TEXT("scroll_events"), WheelEvents);
    Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("actions"), Actions);
    Json->SetArrayField(TEXT("action_effects"), Effects);
    Json->SetArrayField(TEXT("scrolls"), Scrolls);
    Json->SetArrayField(TEXT("attempts"), Attempts);
    Json->SetArrayField(TEXT("captures"), Captures);
    Json->SetObjectField(TEXT("baseline_business"), BaselineBusinessJson.IsValid()
        ? BaselineBusinessJson.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetObjectField(TEXT("final_state"), bBaselineSet ? State() : MakeShared<FJsonObject>());
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Json->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokAllianceTerritoryReadonlyAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 21
        && Actions.Num() == 32 && Effects.Num() == 32
        && Scrolls.Num() == WheelEvents && WheelEvents > 0 && WheelEvents <= 24
        && Attempts.IsEmpty() && CompletionEvents == 0 && Boundary();
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Alliance Territory static audit incomplete"));
    if (!Save(Complete))
    {
        Complete = false;
        Failures.Add(TEXT("Territory final audit manifest write failed"));
        if (!Save(false))
        {
            Failures.Add(TEXT("Territory fallback failure manifest write failed"));
            UE_LOG(LogTemp, Error, TEXT("Territory failure manifest write failed: %s"), *Directory);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Rok alliance Territory audit %s: %s"),
        Complete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!Complete)
    {
        UE_LOG(LogTemp, Error, TEXT("Rok alliance Territory audit: %s"), *Failures.Last());
    }
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}

void URokAllianceTerritoryReadonlyAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !Identity()) return;
    Elapsed += FMath::Max(0.0f, DeltaTime);
    if (Elapsed < 2.0) return;
    if (Pending.IsValid())
    {
        if (Elapsed - StepSince > 30.0) Finish(TEXT("Screenshot callback timeout"));
        else AdvanceCapture();
        return;
    }
    if (Elapsed - StepSince > 40.0) { Finish(TEXT("Territory audit step timeout")); return; }
    auto Stable = [this](int32 Phase) { StableFrames = Ready(Phase) ? StableFrames + 1 : 0; return StableFrames >= 2; };
    auto Press = [this](const FString& Name, const TCHAR* Label)
    {
        if (!Boundary() || !Click(Cast<UButton>(Name == TEXT("AllianceFeatureButton")
            ? Hud->GetWidgetFromName(*Name) : Screen->GetWidgetFromName(*Name)), Label)) return false;
        EffectFrame = GFrameCounter; return true;
    };
    const auto& Rows = RokAllianceTerritoryReadonly::GetRows();
    auto TypeInGroup = [&Rows](int32 Group, int32 Index)
    {
        for (const auto& Row : Rows) if (Row.GroupIndex == Group && Index-- == 0) return Row.UnityType;
        return 0;
    };
    switch (Step)
    {
    case 0: if (!bBegun) bBegun = Begin(); if (bBegun && PrepareOwnWindowFocus()) Enter(1); break;
    case 1:
        if (Stable(0))
        {
            FRokLocalConstructionTicket Ticket; int32 PendingCount = -1;
#if WITH_DEV_AUTOMATION_TESTS
            PendingCount = Buildings->GetPendingCompletedUpgradeCountForTesting();
#endif
            if (!Expect(Buildings->GetActiveUpgradeCount() == 0 && PendingCount == 0
                && Buildings->GetActiveLocalConstructionCount() == 0
                && !Buildings->GetLocalConstructionTicket(1, Ticket),
                TEXT("Territory audit requires idle local business queues"))) return;
            BaselineBusinessJson = BusinessState(); BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
            BaselineCamera = Controller->GetPawn()->GetActorTransform();
            BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            bBaselineSet = true; Capture(TEXT("00_cli_joined_fixture"), 2);
        } break;
    case 2: if (Press(TEXT("AllianceFeatureHit_1"), TEXT("open_territory_651"))) Enter(3); break;
    case 3:
        if (RecordEffect(INDEX_NONE, 0, true) && Stable(1))
        { GroupCursor = 0; RowCursor = 0; Capture(TEXT("01_reference_open"), 4); }
        break;
    case 4:
        if (GroupCursor < 4)
        {
            UButton* Target = Cast<UButton>(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), GroupCursor)));
            if (!TargetInSlatePath(Target))
            {
                if (WheelListTowardsTarget(Target)) Enter(4);
                break;
            }
            if (Press(FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), GroupCursor),
                TEXT("expand_territory_group"))) Enter(5);
        }
        else if (Press(TEXT("AllianceTerritoryReferenceBack"), TEXT("back_joined_695"))) Enter(11);
        break;
    case 5:
        if (RecordEffect(GroupCursor, 0, true) && Stable(2))
        {
            const FString Phase = FString::Printf(TEXT("%02d_group_%d"), 2 + GroupCursor, GroupCursor);
            Capture(*Phase, 6);
        }
        break;
    case 6:
    {
        const int32 Type = TypeInGroup(GroupCursor, RowCursor);
        if (Type > 0)
        {
            UButton* Target = Cast<UButton>(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceTerritorySelect_%d"), Type)));
            if (!TargetInSlatePath(Target))
            {
                if (WheelListTowardsTarget(Target)) Enter(6);
                break;
            }
            if (Press(FString::Printf(TEXT("AllianceTerritorySelect_%d"), Type),
                TEXT("select_static_territory_definition"))) Enter(7);
        }
        else { ++GroupCursor; RowCursor = 0; Enter(4); }
        break;
    }
    case 7:
    {
        const int32 Type = TypeInGroup(GroupCursor, RowCursor);
        if (Type <= 0) { Finish(TEXT("Territory type cursor invalid")); break; }
        if (RecordEffect(GroupCursor, Type, true) && Stable(3))
        {
            const FString Phase = FString::Printf(TEXT("detail_type_%02d"), Type);
            Capture(*Phase, 8);
        }
        break;
    }
    case 8: if (Press(TEXT("AllianceTerritoryReturnToDefinitions"), TEXT("return_to_definitions_19630"))) Enter(9); break;
    case 9:
        if (RecordEffect(GroupCursor, 0, true) && Stable(2))
        { ++RowCursor; Enter(6); }
        break;
    case 11: if (RecordEffect(INDEX_NONE, 0, false) && Stable(4)) Capture(TEXT("18_joined_return"), 12); break;
    case 12: if (Press(TEXT("AllianceFeatureHit_1"), TEXT("reopen_territory_651"))) Enter(13); break;
    case 13: if (RecordEffect(INDEX_NONE, 0, true) && Stable(5)) Capture(TEXT("19_reference_reopened"), 14); break;
    case 14: if (Press(TEXT("FeatureFrameClose"), TEXT("close_to_hud"))) Enter(15); break;
    case 15: if (RecordEffect(INDEX_NONE, 0, false) && Stable(6)) Capture(TEXT("20_hud_closed"), 16); break;
    case 16: Finish(); break;
    default: Finish(TEXT("Invalid Territory audit state")); break;
    }
}
