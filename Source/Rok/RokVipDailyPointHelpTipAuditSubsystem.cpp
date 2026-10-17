#include "RokVipDailyPointHelpTipAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
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
/** Read-only observation of the actual VIP presentation; no Commander portrait exists here. */
struct FRokVipUiSettledSample
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

namespace RokVipUiSettledProbe
{
inline bool HudTexturesReady()
{
#if WITH_EDITOR
    if (FTextureCompilingManager::Get().GetNumRemainingTextures() != 0) return false;
#endif
    for (const TCHAR* Asset : {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip.rok_hud_atlas_vip")})
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Asset);
        const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
        if (!Resource || !Resource->TextureRHI.IsValid()) return false;
    }
    return true;
}
inline FRokVipUiSettledSample Sample(const URokUnityFeatureScreenWidget* Screen, bool bTip)
{
    FRokVipUiSettledSample Result;
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
        && Screen->GetWidgetFromName(TEXT("VipHelp")) != nullptr
        && Screen->GetWidgetFromName(TEXT("FeatureFrameClose")) != nullptr
        && Screen->GetWidgetFromName(TEXT("VipBenefitsScroll")) != nullptr
        && (!bTip || (Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipRoot")) != nullptr
                     && Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipBodyHit")) != nullptr));
#if WITH_EDITOR
    Result.RemainingTextureCompilations = FTextureCompilingManager::Get().GetNumRemainingTextures();
#endif
    // Actual VIP frame, level, chest, and help-tip atlases.
    static const TCHAR* Paths[] = {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip.rok_hud_atlas_vip"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.rok_fullscreen_player"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box.rok_fullscreen_treasure_box")};
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
constexpr TCHAR AuditFlag[] = TEXT("-RokVipDailyPointHelpTipAudit");

struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Exact source, Skill V3 product and runtime boundaries are immutable inputs. Audit/verifier
// files are also hashed at runtime and compared against the staged release report.
constexpr FInputPin Pinned[] = {
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"), TEXT("a3f6245b2c64f552e7b8a253e98f637d6ca0292a4aab87e5fd9183f03c76309f")},
	{TEXT("Saved/PendingHeroTalentReadonlyDetailV1/IndependentReviewV1/review.json"), TEXT("3f9d139bf27a3f01370b884657b381cb23f5506e2b46469dd4dba7377bfa002f")},
	{TEXT("Saved/PendingHeroTalentReadonlyDetailV3/contract.json"), TEXT("cc436ea352e745e5514b8f79e99e499fd5259136ed53bf4b2612d366966bddb1")},
	{TEXT("Saved/PendingHeroTalentReadonlyDetailV3/ready_for_root_review.json"), TEXT("99a826c860ce45686ed7d029f3ece1b96b59dda8915ebebf6d8d192bbce173a1")},
	{TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json"), TEXT("15115118db3366dbdee6f97b705dfaed11da73768694788e2abd7dd1b7a9bd6e")},
	{TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json"), TEXT("07fcf9600d8e79afba470ef92c71460dd217e82e22dfaafc236a4fd58b5b0994")},
	{TEXT("Scripts/BuildRokScoutCatalog.py"), TEXT("f5a01bec3d4216b2214fdfd11f6178f5f0fe00d8e00aa43778625b55908f2c1c")},
	{TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"), TEXT("b20503a0b0dfdfe552502ef307e6d8e6a79c9677e7a185fc0f589cd6c45a8ae5")},
	{TEXT("Source/Rok/RokBuildingUpgradeSubsystem.h"), TEXT("b2253b3fa8232a873e6f4f07fe4cd0f3127eb4cbd3c720a356794186e93ea7b4")},
	{TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
	{TEXT("Source/Rok/RokMainHudWidget.h"), TEXT("2740efb6b5c8a68222d5429f4d66e37e8e484302a215b6c46bbd8c783f67a2e8")},
	{TEXT("Source/Rok/RokPlayerController.cpp"), TEXT("f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6")},
	{TEXT("Source/Rok/RokPlayerController.h"), TEXT("6eaa6329a03804c3af143c9777e8654d5dd83cfe24f598779b6107c988ffe772")},
	{TEXT("Source/Rok/RokServerSubsystem.cpp"), TEXT("3d73a28ff42b787c02a5c5a50ceba5bcac47056cc0945608bfea9ff9bd91bde4")},
	{TEXT("Source/Rok/RokServerSubsystem.h"), TEXT("4a07946d525ccb9eaedafcc30e68315e4ca5c52e94a88fc4c9b388833a07a5a9")},
	{TEXT("Source/Rok/RokTrainArmyMeshData.cpp"), TEXT("e3dfd99ea973b2eba075c117d326ed16ddb44b5188c08740180c4d31c8593a9a")},
	{TEXT("Source/Rok/RokTrainArmyMeshData.h"), TEXT("e0c1e4f9f698366526ac740fd60f238076dc2c5ebc0e5ea85299a92fa9072945")},
	{TEXT("Source/Rok/RokUiAnimatedButton.cpp"), TEXT("bf8da24bdce3dfb403b7174eaa496e95550547cc47c57dccaca6e57aa39bedcf")},
	{TEXT("Source/Rok/RokUiAnimatedButton.h"), TEXT("4379cd36d5fba08d3b8c0fbf5c1076b8ce8b8cb27d2fec3c7fc2a4a65c62a3f7")},
	{TEXT("Source/Rok/RokUiWindowAnimation.cpp"), TEXT("bae58440eccb1ba826871acee2131b1223094b134cf0cfe04a730c798e2b41e4")},
	{TEXT("Source/Rok/RokUiWindowAnimation.h"), TEXT("1c39a74fd70ecec8f1e9b2fd135f65b312e86f2be60ebf323596d8a8359f9e3f")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("39210ad94e5b81c30cbdc6452ccc014c5e18523fa281b5cb4da1b61ef4e67e7b")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("33d05ead38fffc78eb4531083c683974a93af5a9147ebc269fd4225bcf274707")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.cpp"), TEXT("19fe2b78231cf981daa3258ab2aeb28e7eaa3f0054d4f93a8e338bee3e5c2aa6")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.h"), TEXT("78361142356702818e007452f364d8499bcecc5d45b7c88abf0272df6faf2c2b")},
	{TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9c2fca7d169a40b8a5a10ba4ef036b07a6fc76719871f35451c6f49be800171b")},
	{TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("50cec5ddbdba9db12730edad021b8f359477d2b596a6504bc554dfda8ac220d3")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"), TEXT("3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"), TEXT("e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112")},
	{TEXT("Source/Rok/Tests/RokCommanderTalentReadonlyDetailTests.cpp"), TEXT("b289c6715f020422786f0cb5a18b168b2d272376354f8e01fc2abe7471da4e59")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_CaptainTalent.prefab"), TEXT("c2eb5c853307e17a695c98840858b2a84853b4373ef367068f5a0fe81b2170bb")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_TalentSpecialPop.prefab"), TEXT("016729c5d3cafd155149401397fec4bb7aaf6fc96782192d5a8b5ea7b9dfbfbd")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_TalentTop.prefab"), TEXT("34f1f4bfa61946d512a9836d414fc28aae967ff326315d24c902383b233609fc")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png"), TEXT("3e05900aa0e68746af413c92ccd9b11212e6c0b39941e65558f6b7ec7d503bf0")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta"), TEXT("8ec301da2729ea1fc47b1bebcf8436223f92c9193fdeed9b7a6f4644be774c97")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Captain/CaptainMediator.cs"), TEXT("c1b08245f29b8b2a6d95647e8f6d719f312dbf081fc82091bc58c8c82418c2ef")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_CaptainTalent_SubView.cs"), TEXT("c14d9b31ee61cd50a30763fc4750c8bdca85d6cde95c74b40bcd600f443d0787")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_TalentSpecialPop_SubView.cs"), TEXT("21fd26eb89322482079859cdc50ad651425dc986b48ead82ea37a120698c500e")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_TalentTop_SubView.cs"), TEXT("ba3cc59f53cb1a86bd11087f25f167f4ea985644a0e952d2b3f98b55310a31fb")},
	{TEXT("Source/Rok/RokCommanderSkillReadonlyDetail.h"), TEXT("4c337c2932e53fc7ab5b7ffe9030b6e0b85d7a5fbf60fbcca5734c7a3a8ccfce")},
	{TEXT("Source/Rok/Tests/RokCommanderSkillReadonlyDetailTests.cpp"), TEXT("89da04db7bdc971db9184f6e168e436d5777ac567524a85c63558f97c8b0ba6c")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV4/contract.json"), TEXT("ce50964b2550af8bc22db00c50840fba31e176e8c8653e9119455a464e3aebc7")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV4/ready_for_root_review.json"), TEXT("914c17bb8088ba29f7333491d60861eeed1499283e2906f44be77f66b0bd64e8")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV2/IndependentReviewV1/review.json"), TEXT("27175312f900f1511bb20e0a2e528ba3ac9e315e446591288dbbe214a1bc7574")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_CaptainSkill_SubView.cs"), TEXT("2134c1980fe383fc53bda6056b0a93f12c488a97a8fd17d4001f61d64c7ef7b6")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_background.uasset"), TEXT("e2fd1376b752c346dd31117a5582d32aeae0cc82cfdf6d0e7bb8377058646f6a")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_head.uasset"), TEXT("c644358440e254d5f1be45508645606ba42f9d7371c9b6beb15c36d3efc77193")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_skill.uasset"), TEXT("e49fb6820a9de36aa968e684cbbb6b4c8ec367fc41935af1ac17478f1fcdfd23")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_skill2.uasset"), TEXT("4b519495eb3e5c8c2adb6b7db9aa87c69386bacd2bf5dd2eefc0f9a7283d294d")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_ui.uasset"), TEXT("60514fe6be17eafc24fd25f438753686d57107e8917f3a88e85bb090f3ee23bb")},
	{TEXT("Saved/PendingDailyTaskReadonlyCatalogV3/ready_for_root_review.json"), TEXT("95ca2fc33eb008cdbc081352accce559465456414d2444dcdcd3427f54196f5e")},
	{TEXT("Saved/PendingDailyTaskReadonlyCatalogV3/contract.json"), TEXT("1b481051703ca8cb90c0b7b89ce034c82d134cabc98c4542274d1b5a72b7516b")},
	{TEXT("Saved/RokUnityMigration/ConfigTables/TaskDaily.json"), TEXT("974c4ee874ccdcc32d7684aaefe3760ee01aad134f82c40891558cc0152631c7")},
	{TEXT("Saved/RokUnityMigration/ConfigTables/Language.json"), TEXT("e228eca8deb7c8daa4a2d910f736c57567db11322d558544309bfbaca412277d")},
	{TEXT("ref/config-tables/s_TaskDaily.xlsm"), TEXT("b8e16d10cdbf42b22ed6e2b7965450191d1e06dc504640c41cce8e0bf96fe73a")},
	{TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.uasset"), TEXT("b739a555477da17858f2ad1f97824b3191017f3cdb3703b6de19337d0622b93b")},
	{TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_page.uasset"), TEXT("98c5f799d7b19775da0fdacc38966039e33752c3b3e8e5b5def3919f96c1430f")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_task_icon.uasset"), TEXT("b5fa2bc8af9d59c1b86fac2615db33bdb4c25e02026e8a0abe769e4ffe113452")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Win_Quest.prefab"), TEXT("406529e82053553f49555a91c23a1709dd39a5e415d594e5ad279abe25f0582e")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestView.cs"), TEXT("b792d1178b5c6cf51b0d30686c540fe7285c383ec306ffc5ecb42393fc84bc34")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs"), TEXT("1ac142e1d65155320eb73558a3402d2d8e961e663d545c5fde491cabb7a95a60")},
	{TEXT("ref/config-tables/编译客户端配置/CSV/TaskDaily.csv"), TEXT("364db548a68f876918774cb79c802b2edc23f0a3ff943ac8d3ac42ea400037b0")},
	{TEXT("Scripts/BuildRokUnityGameplayCatalog.py"), TEXT("d2528d6262cca7d360cec8e200f3f766fa26af8859419c4b0a22e951d585cc98")},
	{TEXT("Saved/PendingDailyTaskLocalizationSourceProbeV1/report.json"), TEXT("ad8af98684d6970808d3a887d0fe665e83f1c740d646e7b05091e5a238f6596b")},
	{TEXT("Saved/PendingVipDailyPointHelpTipV6/ready_for_root_review.json"), TEXT("88e0b644f2014ac51e693e486a33b56f366b89f7fbbce28ef6f17af4d8d374ea")},
	{TEXT("Saved/PendingVipDailyPointHelpTipV6/contract.json"), TEXT("a888e7c09ed31ba49683df89505b0bd692899ed4af7ddc3b53f4c785a6a6daef")},
	{TEXT("Saved/PendingVipDailyPointHelpTipV2/IndependentReviewV1/review.json"), TEXT("f5957e655c9c6ca4d8863d1a737994e3e5a87457e9ebb54258f28af358dfb985")},
	{TEXT("Saved/PendingVipDailyPointHelpTipV2/source_probe.json"), TEXT("c78e0b38e476f2754127db06d09175582ea69023060631c1268b0493fc4d0afa")},
	{TEXT("Saved/PendingDailyTaskReadonlyRuntimeAuditV2/ready_for_root_review.json"), TEXT("8c70d51e0dd30460abcb51a4628a4623cd98c0f16f4d57a23602cd805749305d")},
	{TEXT("Saved/PendingCommanderSkillReadonlyRuntimeAuditV5/ready_for_root_review.json"), TEXT("b6c0bbab0a41532225cc5c12a9192270cc4ad43e451cc69329a6b745f33d57f7")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip.uasset"), TEXT("630cf1d22da97ceea3275d2068ea8739fa6ff34745360c4c200a1dbb8f7907b6")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.uasset"), TEXT("e972fbf8c225f75e51301915c763cc293f8686c65858a2134cc10967dc6e30b3")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box.uasset"), TEXT("a2c3c4644de339bbeda98fe434e10d76ced8aed471a4d74963436e582a6ffd5f")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIVIP/UI_Win_Vip.prefab"), TEXT("3f10b5367b5fc366ae619095c2c79330564abacbf2a21b6a39f781d9ae079c0f")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/VIP/UI_Win_VipMediator.cs"), TEXT("0e478ff8f58fcd77ac0f7f9ebe88e0299b76dc87edd3fddcf8adb5aaae75d969")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_HelpTips.prefab"), TEXT("82146cb1929c83ed7949c03f3d3b54bb1ecd318bf7d0026cb72ab77caab7a01e")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/HelpTips/HelpTipMediator.cs"), TEXT("46f9e8455ef421c3fc65d2511f7c9435e3cae3cfdf40eb8fa4da3049477e0a71")},
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
#if PLATFORM_WINDOWS
bool RecordOwnedClientAbsoluteRect(const TSharedRef<FJsonObject>& Json, HWND Handle, const FVector2D* Center = nullptr)
{
	DWORD OwnerPid = 0;
	if (Handle && ::IsWindow(Handle)) ::GetWindowThreadProcessId(Handle, &OwnerPid);
	RECT Client = {};
	POINT TopLeft = {}, BottomRight = {};
	bool bValid = Handle && OwnerPid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client)
		&& Client.right - Client.left == 1440 && Client.bottom - Client.top == 810;
	if (bValid)
	{
		TopLeft.x = Client.left; TopLeft.y = Client.top;
		BottomRight.x = Client.right; BottomRight.y = Client.bottom;
		bValid = ::ClientToScreen(Handle, &TopLeft) && ::ClientToScreen(Handle, &BottomRight)
			&& BottomRight.x - TopLeft.x == 1440 && BottomRight.y - TopLeft.y == 810;
	}
	Json->SetBoolField(TEXT("client_absolute_valid"), bValid);
	Json->SetNumberField(TEXT("client_owner_pid"), OwnerPid);
	Json->SetStringField(TEXT("client_window_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Handle)));
	Json->SetObjectField(TEXT("client_absolute_top_left"), Point(FVector2D(TopLeft.x, TopLeft.y)));
	Json->SetObjectField(TEXT("client_absolute_bottom_right"), Point(FVector2D(BottomRight.x, BottomRight.y)));
	return bValid && (!Center || (FMath::IsFinite(Center->X) && FMath::IsFinite(Center->Y)
		&& Center->X >= TopLeft.x && Center->X < BottomRight.x
		&& Center->Y >= TopLeft.y && Center->Y < BottomRight.y));
}
#endif
bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	return true;
}
bool TipCenterValid(const UImage* Center, const UWidget* Background)
{
	if (!Center || !Background || !Visible(Center) || !Visible(Background)) return false;
	const UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"));
	const FTextureResource* Resource = IsValid(Atlas) ? Atlas->GetResource() : nullptr;
	if (!Resource || !Resource->TextureRHI.IsValid() || Center->GetBrush().GetResourceObject() != Atlas) return false;
	const FVector2D Size = Center->GetCachedGeometry().GetLocalSize();
	const FVector2D FrameSize = Background->GetCachedGeometry().GetLocalSize();
	const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Center->Slot);
	if (Size.X < 1.0f || Size.Y < 1.0f || FrameSize.X <= 42.0f || FrameSize.Y <= 47.0f
		|| !Slot || Slot->GetSize().X < 1.0f || Slot->GetSize().Y < 1.0f
		|| Center->GetRenderOpacity() < .995f || Center->GetColorAndOpacity().A < .995f
		|| Center->GetBrush().GetTint(FWidgetStyle()).A < .995f) return false;
	const FBox2f UV = Center->GetBrush().GetUVRegion();
	return UV.bIsValid
		&& FMath::IsNearlyEqual(UV.Min.X, 795.0f / 2048.0f, .00001f)
		&& FMath::IsNearlyEqual(UV.Max.X, 825.0f / 2048.0f, .00001f)
		&& FMath::IsNearlyEqual(UV.Min.Y, 699.0f / 1024.0f, .00001f)
		&& FMath::IsNearlyEqual(UV.Max.Y, 724.0f / 1024.0f, .00001f);
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
	if (const UImage* Image = Cast<UImage>(Widget))
	{
		const FBox2f UV = Image->GetBrush().GetUVRegion();
		Json->SetBoolField(TEXT("uv_valid"), UV.bIsValid);
		Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(UV.Min.X, UV.Min.Y)));
		Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(UV.Max.X, UV.Max.Y)));
		Json->SetNumberField(TEXT("image_alpha"), Image->GetColorAndOpacity().A);
		Json->SetNumberField(TEXT("brush_alpha"), Image->GetBrush().GetTint(FWidgetStyle()).A);
	}
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Json->SetStringField(TEXT("text"), Text->GetText().ToString());
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

bool URokVipDailyPointHelpTipAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false; // Pending-completion count is only exposed to audited Editor builds.
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokVipDailyPointHelpTipAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokVipDailyPointHelpTipAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokVipDailyPointHelpTipAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokVipDailyPointHelpTipAuditSubsystem, STATGROUP_Tickables);
}
void URokVipDailyPointHelpTipAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("PendingVipDailyPointHelpTipRuntimeAuditV6/RuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokVipDailyPointHelpTipAuditSubsystem::ScreenshotProcessed);
}
void URokVipDailyPointHelpTipAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokVipDailyPointHelpTipAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokVipDailyPointHelpTipAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokVipDailyPointHelpTipAuditSubsystem::PrepareOwnWindowFocus()
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

bool URokVipDailyPointHelpTipAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(), TEXT("Wrong world/project or Slate unavailable"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 AuditFlags = 0, LocalFlags = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditFlags; continue; }
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlags; continue; }
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok fixture/diagnostic flag"));
	}
	if (!Expect(AuditFlags == 1 && LocalFlags == 1, TEXT("Audit and local-only flags must occur exactly once"))) return false;
	Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
	Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Runtime = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	Catalog = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
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
		&& Runtime->IsRuntimeStateLoaded() && Catalog.IsValid() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& Hud.IsValid() && Screen.IsValid() && Modal.IsValid() && !Screen->IsScreenOpen()
		&& !Modal->IsModalOpen() && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
		TEXT("Expected idle local-only 1440x810 GameApp HUD and catalog"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Expected anonymous local-preview account boundary"))) return false;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (It->ActorHasTag(TEXT("UnityInitId_5")))
		{
			if (Hall.IsValid()) return Expect(false, TEXT("Ambiguous UnityInitId_5 Hall"));
			Hall = *It;
		}
	if (!Expect(Hall.IsValid() && Hall->ActorHasTag(TEXT("UnityBuildingType_1"))
		&& Hall->GetWorld() == GetWorld(), TEXT("Source Hall identity missing"))) return false;
	FText SourceHelp;
	if (!Expect(Catalog->GetVipDailyPointHelpText(SourceHelp)
		&& !SourceHelp.IsEmpty(), TEXT("Source locale 800032 unavailable"))) return false;
	HelpText = SourceHelp.ToString();
	for (const TCHAR* Asset : {
		TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
		TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip.rok_hud_atlas_vip"),
		TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.rok_fullscreen_player"),
		TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box.rok_fullscreen_treasure_box")})
		if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr,
			TEXT("Required VIP texture asset failed to load"))) return false;
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	bPointerSaved = true;
	Inputs = MakeShared<FJsonObject>();
	for (const FInputPin& Pin : Pinned)
	{
		const FString Actual = FileSha(RokDataPaths::ResolveProjectPath(Pin.Path));
		if (!Expect(Actual == Pin.Sha, TEXT("Pinned source/product SHA differs from reviewed input"))) return false;
		Inputs->SetStringField(Pin.Path, Actual);
	}
	for (const TCHAR* Path : {TEXT("Source/Rok/RokVipDailyPointHelpTipAuditSubsystem.h"),
		TEXT("Source/Rok/RokVipDailyPointHelpTipAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokVipDailyPointHelpTipRuntime.py")})
	{
		const FString Actual = FileSha(RokDataPaths::ResolveProjectPath(Path));
		if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
		Inputs->SetStringField(Path, Actual);
	}
	CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
		this, &URokVipDailyPointHelpTipAuditSubsystem::OnUpgradeCompleted);
	return true;
}

TSharedRef<FJsonObject> URokVipDailyPointHelpTipAuditSubsystem::BusinessState() const
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
	Json->SetBoolField(TEXT("profile_has_vip_level"), Profile.bHasVipLevel);
	Json->SetNumberField(TEXT("profile_vip_level"), Profile.VipLevel);
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

bool URokVipDailyPointHelpTipAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	const auto Placement = Controller->GetCityPlacementPreviewSnapshot();
	const FString Selection = Placement.CurrentSelection.IsValid()
		? Placement.CurrentSelection->GetPathName() : FString();
	if (Selection != BaselineSelection) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}

TSharedRef<FJsonObject> URokVipDailyPointHelpTipAuditSubsystem::State() const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetObjectField(TEXT("business"), BusinessState());
    const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
#if PLATFORM_WINDOWS
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    const HWND Foreground = ::GetForegroundWindow();
    DWORD OwnerPid = 0, ForegroundPid = 0;
    RECT Client = {};
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
    RecordOwnedClientAbsoluteRect(Json, Handle);
#endif
    Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("hud_textures_ready"), RokVipUiSettledProbe::HudTexturesReady());
    Json->SetBoolField(TEXT("vip_test_fixture_state"), Screen.IsValid() && Screen->bVipTestFixtureState);
    Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
    const bool bTip = Screen.IsValid() && (Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot != nullptr && Screen->VipDailyPointHelpTipRoot->GetParent() == Screen->ModeCanvas);
    const FRokVipUiSettledSample Settled = RokVipUiSettledProbe::Sample(Screen.Get(), bTip);
    auto Probe = MakeShared<FJsonObject>();
    Probe->SetBoolField(TEXT("ready"), Settled.bReady);
    Probe->SetBoolField(TEXT("structure_valid"), Settled.bStructureValid);
    Probe->SetNumberField(TEXT("direct_children"), Settled.DirectChildren);
    Probe->SetNumberField(TEXT("unsettled_children"), Settled.UnsettledChildren);
    Probe->SetNumberField(TEXT("not_visible_children"), Settled.NotVisibleChildren);
    Probe->SetNumberField(TEXT("unsettled_not_visible_children"), Settled.UnsettledNotVisibleChildren);
    Probe->SetNumberField(TEXT("remaining_texture_compilations"), Settled.RemainingTextureCompilations);
    Probe->SetNumberField(TEXT("texture_count"), Settled.TextureCount);
    Probe->SetBoolField(TEXT("texture_rhi_ready"), Settled.bTextureRhiReady);
    Probe->SetBoolField(TEXT("portrait_gate_applicable"), Settled.bPortraitGateApplicable);
    Probe->SetBoolField(TEXT("portrait_present"), Settled.bPortraitPresent);
    Json->SetObjectField(TEXT("vip_ui_settled"), Probe);
    const auto Placement = Controller.IsValid() ? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
    Json->SetNumberField(TEXT("primary_press_count"), static_cast<double>(Placement.PrimaryPressEventCount));
    Json->SetStringField(TEXT("selected_actor"), Placement.CurrentSelection.IsValid()
        ? Placement.CurrentSelection->GetPathName() : FString());
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
    Widgets->SetObjectField(TEXT("VipHitButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("VipHitButton")) : nullptr));
    for (const TCHAR* Name : {TEXT("FeatureModeCanvas"), TEXT("FeatureFrameClose"), TEXT("VipHelp"),
        TEXT("VipBenefitsScroll"), TEXT("VipDailyPointChest"), TEXT("VipAddPoints"),
        TEXT("VipClaimFreeChest"), TEXT("VipBuySpecialChest"),
        TEXT("VipDailyPointHelpTipRoot"), TEXT("VipDailyPointHelpTipBodyHit"),
        TEXT("VipDailyPointHelpTipBackground"), TEXT("VipDailyPointHelpTipBackground_Center"),
        TEXT("VipDailyPointHelpTipText"),
        TEXT("VipDailyPointHelpTipArrow")})
        Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
    Json->SetObjectField(TEXT("widgets"), Widgets);
    const UScrollBox* Scroll = Screen.IsValid() ? Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("VipBenefitsScroll"))) : nullptr;
    Json->SetNumberField(TEXT("benefits_scroll_offset"), Scroll ? Scroll->GetScrollOffset() : -1);
    Json->SetBoolField(TEXT("help_tip_visible"), bTip);
    Json->SetNumberField(TEXT("pending_action_id"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : INDEX_NONE);
    Json->SetStringField(TEXT("source_help_text"), HelpText);
    const UTextBlock* TipText = Screen.IsValid() ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipText"))) : nullptr;
    const UImage* CenterPatch = Screen.IsValid() ? Cast<UImage>(Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipBackground_Center"))) : nullptr;
    if (bTip && TipText)
    {
        const FSlateFontInfo Font = TipText->GetFont();
        const FLinearColor Color = TipText->GetColorAndOpacity().GetSpecifiedColor();
        Json->SetStringField(TEXT("tip_font_face"), Font.TypefaceFontName.ToString());
        Json->SetNumberField(TEXT("tip_font_size"), Font.Size);
        Json->SetNumberField(TEXT("tip_text_color_r"), Color.R);
        Json->SetNumberField(TEXT("tip_text_color_g"), Color.G);
        Json->SetNumberField(TEXT("tip_text_color_b"), Color.B);
    }
    const UWidget* TipBackground = Screen.IsValid() ? Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipBackground")) : nullptr;
    Json->SetBoolField(TEXT("tip_background_center_common_atlas"), bTip && CenterPatch
        && CenterPatch->GetBrush().GetResourceObject() == LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common")));
    Json->SetBoolField(TEXT("tip_background_center_geometry_uv_rhi_alpha_valid"), bTip && TipCenterValid(CenterPatch, TipBackground));
    return Json;
}

bool URokVipDailyPointHelpTipAuditSubsystem::Ready(int32 Phase) const
{
    if (!Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Runtime.IsValid() || !Catalog.IsValid()
        || !Server.IsValid() || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
        || Server->GetSuppressedLocalRequestCount() != 0 || Modal->IsModalOpen()
        || Screen->bVipTestFixtureState) return false;
    if (Phase == 0 || Phase == 14)
        return !Screen->IsScreenOpen() && Visible(Hud->GetWidgetFromName(TEXT("VipHitButton")))
            && RokVipUiSettledProbe::HudTexturesReady();
    const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Screen->IsScreenOpen() || !Mode || !Visible(Mode)
        || !FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0, .01)
        || !Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01)
        || !Visible(Screen->GetWidgetFromName(TEXT("VipHelp")))
        || !Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const bool bTip = Phase == 2 || Phase == 3 || Phase == 4 || Phase == 6 || Phase == 8
        || Phase == 10 || Phase == 12;
    if (!RokVipUiSettledProbe::Sample(Screen.Get(), bTip).bReady) return false;
    const bool bActualTip = (Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot != nullptr && Screen->VipDailyPointHelpTipRoot->GetParent() == Screen->ModeCanvas);
    if (bActualTip != bTip || Screen->GetPendingActionIdForAudit() != INDEX_NONE) return false;
    if (!bTip) return !Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot == nullptr;
    const UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipText")));
    const UWidget* Background = Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipBackground"));
    const UWidget* Arrow = Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipArrow"));
    const UWidget* Body = Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipBodyHit"));
    const UImage* CenterPatch = Cast<UImage>(Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipBackground_Center")));
    const FLinearColor Ink = Text ? Text->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::White;
    return Text && Visible(Text) && Text->GetText().ToString() == HelpText
        && Background && Visible(Background) && Arrow && Visible(Arrow)
        && Body && Visible(Body) && Body->GetCachedGeometry().GetLocalSize().X > 0
        && Body->GetCachedGeometry().GetLocalSize().Y > 0
        && Text->GetFont().TypefaceFontName == TEXT("Regular")
        && FMath::IsNearlyEqual(Text->GetFont().Size, 13.5f, .1f)
        && Ink.R < .2f && Ink.G < .2f && Ink.B < .2f
        && TipCenterValid(CenterPatch, Background);
}

void URokVipDailyPointHelpTipAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Daily task read-only route emitted an upgrade completion event"));
}

bool URokVipDailyPointHelpTipAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
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
#if PLATFORM_WINDOWS
	const bool bClientRect = RecordOwnedClientAbsoluteRect(Attempt, OwnHandle, &Center);
#else
	const bool bClientRect = false;
#endif
	Attempt->SetNumberField(TEXT("stable_frames"), ClickStableFrames);
	const uint64 Before = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetNumberField(TEXT("primary_before"), static_cast<double>(Before));
	Attempt->SetObjectField(TEXT("business_before"), BusinessState());
	Attempt->SetBoolField(TEXT("screen_open_before"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_before"), Modal.IsValid() && Modal->IsModalOpen());
	if (!Path.IsValid() || !bExactButton || !bUmgPath || !Target->IsHovered() || !bOwnWindow || !bForeground || !bClientRect)
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
	const bool bRouteSafe = Down && Up && Before == After && (!bBaselineSet || Boundary());
	Attempt->SetStringField(TEXT("failure"), bRouteSafe ? TEXT("") : TEXT("unhandled, world input, or business mutation"));
	Actions.Add(MakeShared<FJsonValueObject>(Attempt));
	StableTarget.Reset(); ClickStableFrames = 0;
	return Expect(bRouteSafe, TEXT("Slate click failed or changed baseline state"));
}

bool URokVipDailyPointHelpTipAuditSubsystem::OutsideDismiss(UButton* Target, const TCHAR* Label)
{
    if (!Expect(Target && Visible(Target) && Boundary()
        && (Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot != nullptr && Screen->VipDailyPointHelpTipRoot->GetParent() == Screen->ModeCanvas),
        TEXT("Outside target/tip unavailable"))) return false;
    const URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
    if (!Expect(Indexed && ((Indexed->ActionId == 742 && Target->GetIsEnabled())
        || (!Target->GetIsEnabled() && (Indexed->ActionId == 740 || Indexed->ActionId == 741
        || Indexed->ActionId == 743 || Indexed->ActionId == 744))),
        TEXT("Outside target action identity differs"))) return false;
    const FGeometry Geometry = Target->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Outside target has no cached geometry"))) return false;
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    FSlateApplication& App = FSlateApplication::Get();
    App.SetCursorPos(Center);
    App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()));
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
    const TSharedRef<SWidget> ScreenSlate = Screen->TakeWidget();
    const TSharedRef<SWidget> TargetSlate = Target->TakeWidget();
    bool bScreenInPath = false, bDisabledTargetInPath = false;
    TArray<TSharedPtr<FJsonValue>> PathTypes;
    TArray<TSharedPtr<FJsonValue>> PathNodes;
    for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
    {
        const SWidget* Node = &Path.Widgets[Index].Widget.Get();
        auto Item = MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("index"), Index);
        Item->SetStringField(TEXT("type"), Node->GetTypeAsString());
        Item->SetStringField(TEXT("slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Node)));
        PathTypes.Add(MakeShared<FJsonValueString>(Node->GetTypeAsString()));
        PathNodes.Add(MakeShared<FJsonValueObject>(Item));
        bScreenInPath |= Node == &ScreenSlate.Get();
        bDisabledTargetInPath |= Node == &TargetSlate.Get();
    }
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
    uint32 OwnerPid = 0;
    bool bForeground = false;
    FString OwnWindowId, ForegroundWindowId;
#if PLATFORM_WINDOWS
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    if (Handle) { DWORD Pid = 0; ::GetWindowThreadProcessId(Handle, &Pid); OwnerPid = Pid; }
    OwnWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(Handle));
    ForegroundWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
    bForeground = Handle && ::GetForegroundWindow() == Handle
        && OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
    if (!Expect(bScreenInPath && bOwnWindow && bForeground
        && Screen->GetPendingActionIdForAudit() == INDEX_NONE,
        TEXT("Outside pointer not routed through foreground VIP screen"))) return false;
    auto Action = MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("label"), Label);
    Action->SetStringField(TEXT("method"), TEXT("ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
    Action->SetStringField(TEXT("target"), Target->GetName());
    Action->SetStringField(TEXT("target_widget_class"), Target->GetClass()->GetPathName());
    Action->SetNumberField(TEXT("action_id"), Indexed->ActionId);
    Action->SetBoolField(TEXT("target_enabled"), Target->GetIsEnabled());
    Action->SetBoolField(TEXT("disabled_target_in_hit_path"), bDisabledTargetInPath);
    Action->SetBoolField(TEXT("screen_in_hit_path"), bScreenInPath);
    Action->SetArrayField(TEXT("hit_path_types"), PathTypes);
    Action->SetArrayField(TEXT("hit_path_nodes"), PathNodes);
    Action->SetObjectField(TEXT("absolute_center"), Point(Center));
    Action->SetObjectField(TEXT("target_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
    Action->SetObjectField(TEXT("target_bottom_right"), Point(Geometry.LocalToAbsolute(Size)));
    Action->SetBoolField(TEXT("own_game_window"), bOwnWindow);
    Action->SetBoolField(TEXT("foreground"), bForeground);
    Action->SetStringField(TEXT("own_window_identity"), OwnWindowId);
    Action->SetStringField(TEXT("foreground_window_identity"), ForegroundWindowId);
    Action->SetNumberField(TEXT("own_window_pid"), OwnerPid);
#if PLATFORM_WINDOWS
    if (!Expect(RecordOwnedClientAbsoluteRect(Action, Handle, &Center),
        TEXT("Outside pointer center is outside owned 1440x810 client screen rect"))) return false;
#else
    return Expect(false, TEXT("Outside pointer requires Windows client screen rect"));
#endif
    Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Action->SetNumberField(TEXT("pending_action_before"), Screen->GetPendingActionIdForAudit());
    Action->SetNumberField(TEXT("primary_before"), static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
    Action->SetObjectField(TEXT("business_before"), BusinessState());
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
        FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
        TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool bTipGone = (!Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot == nullptr);
    Action->SetBoolField(TEXT("down_handled"), Down);
    Action->SetBoolField(TEXT("up_handled"), Up);
    Action->SetBoolField(TEXT("tip_closed_after"), bTipGone);
    Action->SetNumberField(TEXT("pending_action_after"), Screen->GetPendingActionIdForAudit());
    Action->SetNumberField(TEXT("primary_after"), static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
    Action->SetObjectField(TEXT("business_after"), BusinessState());
    Action->SetBoolField(TEXT("modal_open_after"), Modal->IsModalOpen());
    const bool bSafe = Down && bTipGone && Screen->GetPendingActionIdForAudit() == INDEX_NONE
        && !Modal->IsModalOpen() && Boundary();
    Action->SetStringField(TEXT("failure"), bSafe ? TEXT("") : TEXT("outside preview did not consume/dismiss or changed state"));
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    if (bSafe && Indexed->ActionId != 742) ++OutsideDismissals;
    return Expect(bSafe, TEXT("Real outside pointer failed VIP preview/readonly gate"));
}

bool URokVipDailyPointHelpTipAuditSubsystem::NonLeftInput(bool bWheel)
{
    UWidget* Target = Screen.IsValid() ? Screen->GetWidgetFromName(
        bWheel ? TEXT("VipBenefitsScroll") : TEXT("VipDailyPointHelpTipBodyHit")) : nullptr;
    if (!Expect(Target && Visible(Target) && Boundary()
        && (Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot != nullptr && Screen->VipDailyPointHelpTipRoot->GetParent() == Screen->ModeCanvas),
        TEXT("VIP non-left input target/tip unavailable"))) return false;
    const FGeometry Geometry = Target->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("VIP non-left target has no geometry"))) return false;
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    FSlateApplication& App = FSlateApplication::Get();
    App.SetCursorPos(Center);
    App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()));
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
    const TSharedRef<SWidget> ScreenSlate = Screen->TakeWidget();
    const TSharedRef<SWidget> TargetSlate = Target->TakeWidget();
    bool bScreenInPath = false, bTargetInPath = false;
    TArray<TSharedPtr<FJsonValue>> PathTypes;
    for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
    {
        const SWidget* Node = &Path.Widgets[Index].Widget.Get();
        PathTypes.Add(MakeShared<FJsonValueString>(Node->GetTypeAsString()));
        bScreenInPath |= Node == &ScreenSlate.Get();
        bTargetInPath |= Node == &TargetSlate.Get();
    }
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
    uint32 OwnerPid = 0;
    bool bForeground = false;
#if PLATFORM_WINDOWS
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    if (Handle) { DWORD Pid = 0; ::GetWindowThreadProcessId(Handle, &Pid); OwnerPid = Pid; }
    bForeground = Handle && ::GetForegroundWindow() == Handle
        && OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
    if (!Expect(bScreenInPath && bTargetInPath && bOwnWindow && bForeground,
        TEXT("VIP non-left hit path/window invalid"))) return false;
    const UScrollBox* Scroll = bWheel ? Cast<UScrollBox>(Target) : nullptr;
    auto Action = MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("label"), bWheel ? TEXT("wheel_vip_benefits_tip_stays") : TEXT("right_inside_tip_stays"));
    Action->SetStringField(TEXT("method"), bWheel ? TEXT("ProcessMouseWheelOrGestureEvent")
        : TEXT("ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
    Action->SetStringField(TEXT("target"), Target->GetName());
    Action->SetStringField(TEXT("target_widget_class"), Target->GetClass()->GetPathName());
    Action->SetArrayField(TEXT("hit_path_types"), PathTypes);
    Action->SetBoolField(TEXT("screen_in_hit_path"), bScreenInPath);
    Action->SetBoolField(TEXT("target_in_hit_path"), bTargetInPath);
    Action->SetBoolField(TEXT("own_game_window"), bOwnWindow);
    Action->SetBoolField(TEXT("foreground"), bForeground);
    Action->SetNumberField(TEXT("own_window_pid"), OwnerPid);
    Action->SetObjectField(TEXT("absolute_center"), Point(Center));
#if PLATFORM_WINDOWS
    if (!Expect(RecordOwnedClientAbsoluteRect(Action, Handle, &Center),
        TEXT("VIP right/wheel center is outside owned 1440x810 client screen rect"))) return false;
#else
    return Expect(false, TEXT("VIP right/wheel requires Windows client screen rect"));
#endif
    Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Action->SetNumberField(TEXT("scroll_before"), Scroll ? Scroll->GetScrollOffset() : -1);
    Action->SetObjectField(TEXT("business_before"), BusinessState());
    Action->SetNumberField(TEXT("primary_before"), static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
    bool Down = false, Up = false, Wheel = false;
    if (bWheel)
        Wheel = App.ProcessMouseWheelOrGestureEvent(FPointerEvent(0, Center, Center,
            TSet<FKey>(), EKeys::Invalid, -1.0f, FModifierKeysState()), nullptr);
    else
    {
        TSet<FKey> Pressed; Pressed.Add(EKeys::RightMouseButton);
        Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
            FPointerEvent(0, Center, Center, Pressed, EKeys::RightMouseButton, 0, FModifierKeysState()));
        Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
            TSet<FKey>(), EKeys::RightMouseButton, 0, FModifierKeysState()));
    }
    Action->SetBoolField(TEXT("down_handled"), Down);
    Action->SetBoolField(TEXT("up_handled"), Up);
    Action->SetBoolField(TEXT("wheel_handled"), Wheel);
    Action->SetNumberField(TEXT("scroll_after"), Scroll ? Scroll->GetScrollOffset() : -1);
    Action->SetBoolField(TEXT("tip_still_open"), (Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot != nullptr && Screen->VipDailyPointHelpTipRoot->GetParent() == Screen->ModeCanvas));
    Action->SetNumberField(TEXT("pending_action_after"), Screen->GetPendingActionIdForAudit());
    Action->SetNumberField(TEXT("primary_after"), static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
    Action->SetObjectField(TEXT("business_after"), BusinessState());
    const bool bSafe = (!bWheel || Wheel)
        && (Screen->bVipDailyPointHelpVisible && Screen->VipDailyPointHelpTipRoot != nullptr && Screen->VipDailyPointHelpTipRoot->GetParent() == Screen->ModeCanvas)
        && Screen->GetPendingActionIdForAudit() == INDEX_NONE && Boundary();
    Action->SetStringField(TEXT("failure"), bSafe ? TEXT("") : TEXT("non-left input dismissed tip or changed business"));
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    return Expect(bSafe, TEXT("VIP right/wheel input changed tip or business"));
}

void URokVipDailyPointHelpTipAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	PendingPhase = Captures.Num();
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
		&& PendingPhase >= 0 && PendingPhase < 15 && Ready(PendingPhase)
		&& (!bBaselineSet || Boundary()), TEXT("Screenshot request/settled/boundary invalid"))) return;
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("requested_state"), State());
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	Pending->SetStringField(TEXT("png"), PendingPath);
	NextStep = After;
	bScreenshotDone = false;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}
void URokVipDailyPointHelpTipAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	TArray<uint8> Bytes;
	if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
		&& FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
		&& FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
		&& PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
		&& Ready(PendingPhase) && (!bBaselineSet || Boundary()), TEXT("Processed screenshot PNG/extent/settled/business state invalid"))) return;
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(Bytes.Num()));
	Pending->SetNumberField(TEXT("png_width"), 1440);
	Pending->SetNumberField(TEXT("png_height"), 810);
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("processed_state"), State());
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokVipDailyPointHelpTipAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(Ready(PendingPhase) && (!bBaselineSet || Boundary()), TEXT("Post-screenshot settled/business state changed"))) return;
	Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(NextStep);
}
void URokVipDailyPointHelpTipAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}
bool URokVipDailyPointHelpTipAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.vip_daily_point_help_tip.runtime_audit.v3"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 15
        && Actions.Num() == 15 && OutsideDismissals == 4
        && Attempts.IsEmpty() && CompletionEvents == 0 && bBaselineSet && Boundary());
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetStringField(TEXT("scope"), TEXT("Actual anonymous HUD VIP daily-point help 800032 real Slate; disabled 740/741/743/744 outside dismiss only, no purchase or claim"));
    Json->SetStringField(TEXT("baseline_phase"), TEXT("00_city_hud"));
    Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
    Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
    Json->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
    Json->SetBoolField(TEXT("full_migration_accepted"), false);
    Json->SetBoolField(TEXT("account_task_state_accepted"), false);
    Json->SetBoolField(TEXT("claim_state_accepted"), false);
    Json->SetNumberField(TEXT("locale_id"), 800032);
    Json->SetStringField(TEXT("source_help_text"), HelpText);
    Json->SetNumberField(TEXT("outside_dismissals"), OutsideDismissals);
    Json->SetBoolField(TEXT("logged_in_enabled_purchase_claim_covered"), false);
    Json->SetNumberField(TEXT("completion_events"), CompletionEvents);
    Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("actions"), Actions);
    Json->SetArrayField(TEXT("attempts"), Attempts);
    Json->SetArrayField(TEXT("captures"), Captures);
    Json->SetObjectField(TEXT("baseline_business"), BaselineBusinessJson.IsValid()
        ? BaselineBusinessJson.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetObjectField(TEXT("final_state"), State());
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Json->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokVipDailyPointHelpTipAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 15
        && Actions.Num() == 15 && OutsideDismissals == 4
        && Attempts.IsEmpty() && CompletionEvents == 0 && Boundary();
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("VIP help-tip read-only audit incomplete"));
    Save(Complete);
    UE_LOG(LogTemp, Display, TEXT("Rok VIP help-tip audit %s: %s"),
        Complete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok VIP help-tip audit: %s"), *Failures.Last());
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokVipDailyPointHelpTipAuditSubsystem::Tick(float DeltaTime)
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
    if (Elapsed - StepSince > 35.0) { Finish(TEXT("Audit step timeout")); return; }
    auto Stable = [this](int32 Phase)
    {
        StableFrames = Ready(Phase) ? StableFrames + 1 : 0;
        return StableFrames >= 2;
    };
    switch (Step)
    {
    case 0: if (!bBegun) bBegun = Begin(); if (bBegun && PrepareOwnWindowFocus()) Enter(1); break;
    case 1:
        if (Stable(0))
        {
            FRokLocalConstructionTicket Ticket;
            int32 PendingCount = -1;
#if WITH_DEV_AUTOMATION_TESTS
            PendingCount = Buildings->GetPendingCompletedUpgradeCountForTesting();
#endif
            if (!Expect(Buildings->GetActiveUpgradeCount() == 0 && PendingCount == 0
                && Buildings->GetActiveLocalConstructionCount() == 0
                && !Buildings->GetLocalConstructionTicket(1, Ticket),
                TEXT("Daily audit requires idle local business queues"))) return;
            BaselineBusinessJson = BusinessState();
            BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
            BaselineCamera = Controller->GetPawn()->GetActorTransform();
            BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
			const auto Placement = Controller->GetCityPlacementPreviewSnapshot();
			BaselineSelection = Placement.CurrentSelection.IsValid()
				? Placement.CurrentSelection->GetPathName() : FString();
            bBaselineSet = true;
            Capture(TEXT("00_city_hud"), 2);
        }
        break;
    case 2: if (Boundary() && Click(Cast<UButton>(Hud->GetWidgetFromName(TEXT("VipHitButton"))), TEXT("open_vip_from_hud"))) Enter(3); break;
    case 3: if (Boundary() && Stable(1)) Capture(TEXT("01_vip"), 4); break;
    case 4: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipHelp"))), TEXT("open_help_742"))) Enter(5); break;
    case 5: if (Boundary() && Stable(2)) Capture(TEXT("02_help_open"), 6); break;
    case 6: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipDailyPointHelpTipBodyHit"))), TEXT("inside_body_745_stays"))) Enter(7); break;
    case 7: if (Boundary() && Stable(3)) Capture(TEXT("03_inside_stays"), 8); break;
    case 8: if (Boundary() && NonLeftInput(false)) Enter(9); break;
    case 9: if (Boundary() && NonLeftInput(true)) Enter(10); break;
    case 10: if (Boundary() && Stable(4)) Capture(TEXT("04_right_wheel_stays"), 11); break;
    case 11: if (OutsideDismiss(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipDailyPointChest"))), TEXT("outside_740_consumed"))) Enter(12); break;
    case 12: if (Boundary() && Stable(5)) Capture(TEXT("05_outside_740_closed"), 13); break;
    case 13: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipHelp"))), TEXT("reopen_after_740"))) Enter(14); break;
    case 14: if (Boundary() && Stable(6)) Capture(TEXT("06_reopened_740"), 15); break;
    case 15: if (OutsideDismiss(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipAddPoints"))), TEXT("outside_741_consumed"))) Enter(16); break;
    case 16: if (Boundary() && Stable(7)) Capture(TEXT("07_outside_741_closed"), 17); break;
    case 17: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipHelp"))), TEXT("reopen_after_741"))) Enter(18); break;
    case 18: if (Boundary() && Stable(8)) Capture(TEXT("08_reopened_741"), 19); break;
    case 19: if (OutsideDismiss(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipClaimFreeChest"))), TEXT("outside_743_consumed"))) Enter(20); break;
    case 20: if (Boundary() && Stable(9)) Capture(TEXT("09_outside_743_closed"), 21); break;
    case 21: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipHelp"))), TEXT("reopen_after_743"))) Enter(22); break;
    case 22: if (Boundary() && Stable(10)) Capture(TEXT("10_reopened_743"), 23); break;
    case 23: if (OutsideDismiss(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipBuySpecialChest"))), TEXT("outside_744_consumed"))) Enter(24); break;
    case 24: if (Boundary() && Stable(11)) Capture(TEXT("11_outside_744_closed"), 25); break;
    case 25: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipHelp"))), TEXT("reopen_after_744"))) Enter(26); break;
    case 26: if (Boundary() && Stable(12)) Capture(TEXT("12_reopened_744"), 27); break;
    case 27: if (OutsideDismiss(Cast<UButton>(Screen->GetWidgetFromName(TEXT("VipHelp"))), TEXT("close_help_742"))) Enter(28); break;
    case 28: if (Boundary() && Stable(13)) Capture(TEXT("13_help_closed"), 29); break;
    case 29: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))), TEXT("close_vip_to_hud"))) Enter(30); break;
    case 30: if (Boundary() && Stable(14)) Capture(TEXT("14_hud_return"), 31); break;
    case 31: Finish(); break;
    default: Finish(TEXT("Invalid audit state")); break;
    }
}
