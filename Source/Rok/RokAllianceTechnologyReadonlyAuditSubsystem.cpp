#include "RokAllianceTechnologyReadonlyAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
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
#include "Components/ScrollBox.h"
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
struct FRokAllianceTechnologyUiSettledSample
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

namespace RokAllianceTechnologyUiSettledProbe
{

inline bool ResearchAtlasesReady()
{
#if WITH_EDITOR
    if (FTextureCompilingManager::Get().GetNumRemainingTextures() != 0) return false;
#endif
    struct FExpected { const TCHAR* Path; FIntPoint Extent; };
    static const FExpected Research[] = {
        {TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1.rok_research_research_icon1"), FIntPoint(512, 2048)},
        {TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon2.rok_research_research_icon2"), FIntPoint(1024, 2048)}};
    for (const FExpected& Expected : Research)
    {
        const UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Expected.Path);
        const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
        if (!Texture || !Resource || !Resource->TextureRHI.IsValid()
            || !Texture->SRGB || Texture->LODGroup != TEXTUREGROUP_UI
            || Texture->MipGenSettings != TMGS_NoMipmaps || !Texture->HasAlphaChannel()
            || Texture->GetNumMips() != 1 || Resource->TextureRHI->GetDesc().NumMips != 1
            || FIntPoint(Texture->GetSizeX(), Texture->GetSizeY()) != Expected.Extent
            || Resource->TextureRHI->GetDesc().Extent != Expected.Extent) return false;
    }
    return true;
}

inline FRokAllianceTechnologyUiSettledSample Sample(const URokUnityFeatureScreenWidget* Screen, bool bReference)
{
    FRokAllianceTechnologyUiSettledSample Result;
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
        && (bReference ? Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceBack")) != nullptr
            : Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceOpen")) != nullptr);
#if WITH_EDITOR
    Result.RemainingTextureCompilations = FTextureCompilingManager::Get().GetNumRemainingTextures();
#endif
    static const TCHAR* Paths[] = {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance"),
        TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1.rok_research_research_icon1"),
        TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon2.rok_research_research_icon2")};
    Result.bTextureRhiReady = true;
    for (const TCHAR* Asset : Paths)
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Asset);
        const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
        if (!Resource || !Resource->TextureRHI.IsValid()) Result.bTextureRhiReady = false;
        else {
            const FIntPoint Extent = Resource->TextureRHI->GetDesc().Extent;
            const bool bResearch = FString(Asset).Contains(TEXT("research_icon"));
            if (bResearch && (!Texture->SRGB || Texture->LODGroup != TEXTUREGROUP_UI
                || Texture->MipGenSettings != TMGS_NoMipmaps || !Texture->HasAlphaChannel()
                || Texture->GetNumMips() != 1 || Resource->TextureRHI->GetDesc().NumMips != 1)) Result.bTextureRhiReady = false;
            if (FString(Asset).Contains(TEXT("research_icon1")) && Extent != FIntPoint(512, 2048)) Result.bTextureRhiReady = false;
            if (FString(Asset).Contains(TEXT("research_icon2")) && Extent != FIntPoint(1024, 2048)) Result.bTextureRhiReady = false;
            ++Result.TextureCount;
        }
    }
    Result.bReady = Result.bStructureValid && Result.UnsettledChildren == 0
        && Result.RemainingTextureCompilations == 0 && Result.bTextureRhiReady
        && Result.TextureCount == UE_ARRAY_COUNT(Paths) && !Result.bPortraitPresent;
    return Result;
}

}

constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceTechnologyReadonlyAudit");

struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Exact source, Alliance Technology product and runtime boundaries are immutable inputs. Audit/verifier
// files are also hashed at runtime and compared against the staged release report.
constexpr FInputPin Pinned[] = {
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
	{TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9c2fca7d169a40b8a5a10ba4ef036b07a6fc76719871f35451c6f49be800171b")},
	{TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("50cec5ddbdba9db12730edad021b8f359477d2b596a6504bc554dfda8ac220d3")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"), TEXT("3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"), TEXT("e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("9dcdab4fd9c49e47a684c495ab58d10dcfad0f8166accf7a8cd38033aaad89de")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("b14c9900310b266ff956968928fa4a26574fde3b86ca8d29190bf5b4aa4dca8f")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.cpp"), TEXT("6c27feacc66b48d75287d55396926afd1691832d7ebd5535cedd36491375d4b7")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.h"), TEXT("392f8d13e4ce1dd338065082ef979d8ab0fb456de6a7c6993a6b197c19799a4c")},
	{TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json"), TEXT("15115118db3366dbdee6f97b705dfaed11da73768694788e2abd7dd1b7a9bd6e")},
	{TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json"), TEXT("07fcf9600d8e79afba470ef92c71460dd217e82e22dfaafc236a4fd58b5b0994")},
	{TEXT("Saved/RokUnityMigration/ConfigTables/AllianceOfficially.json"), TEXT("e4df13a417573b58254320ea27fa16585c8b6a1ec3784ca50fa63dfc31a95421")},
	{TEXT("Saved/RokUnityMigration/ConfigTables/Language.json"), TEXT("e228eca8deb7c8daa4a2d910f736c57567db11322d558544309bfbaca412277d")},
	{TEXT("Saved/PendingAllianceOfficeReadonlyCatalogV2/ready_for_root_review.json"), TEXT("3bfb81e5227a316ce3d538880b5cdeef5bab7ae190acf00ec457ae0745181941")},
	{TEXT("Saved/PendingAllianceOfficeReadonlyCatalogV2/contract.json"), TEXT("b80d317b5bda0999d201bdcca82f6e74fb927619fb29a3b79aa516d5daa6ef6a")},
	{TEXT("Saved/PendingAllianceOfficeReadonlyCatalogV2/source_trace.json"), TEXT("69958d32ed563586c8f0e4542701e676090bbb2e91510b7ecc09424ecea5070a")},
	{TEXT("Saved/PendingActivityCalendarReadonlyCatalogV2/ready_for_root_review.json"), TEXT("1b946f5c241cdc180cada5a07154248d101e71de7252bb2b8db9cb3a38aaf2e9")},
	{TEXT("Saved/PendingActivityCalendarReadonlyCatalogV3/ready_for_root_review.json"), TEXT("fc9626bc6a4db0881e544ce464440017ac995ad076f73e232c73deded486ef1c")},
	{TEXT("Saved/PendingActivityCalendarReadonlyRuntimeAuditV7/ready_for_root_review.json"), TEXT("cb8531e0b89eae7af529649c581df49e49c20e72137e004aff9249c615129047")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV4/ready_for_root_review.json"), TEXT("914c17bb8088ba29f7333491d60861eeed1499283e2906f44be77f66b0bd64e8")},
	{TEXT("Saved/PendingCommanderSkillReadonlyRuntimeAuditV5/ready_for_root_review.json"), TEXT("b6c0bbab0a41532225cc5c12a9192270cc4ad43e451cc69329a6b745f33d57f7")},
	{TEXT("Saved/PendingDailyTaskReadonlyCatalogV3/ready_for_root_review.json"), TEXT("95ca2fc33eb008cdbc081352accce559465456414d2444dcdcd3427f54196f5e")},
	{TEXT("Saved/PendingDailyTaskReadonlyRuntimeAuditV2/ready_for_root_review.json"), TEXT("8c70d51e0dd30460abcb51a4628a4623cd98c0f16f4d57a23602cd805749305d")},
	{TEXT("Saved/PendingVipDailyPointHelpTipV3/ready_for_root_review.json"), TEXT("a124c5b47671f4b50edddbc91c1df4066db85105afb3fee4a6123f5076e9b17a")},
	{TEXT("Saved/PendingVipDailyPointHelpTipRuntimeAuditV2/ready_for_root_review.json"), TEXT("8604d0add1b8733644b69578a0cedb633644afa89647d00e1ee98c73b2f782f3")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"), TEXT("a3f6245b2c64f552e7b8a253e98f637d6ca0292a4aab87e5fd9183f03c76309f")},
	{TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.uasset"), TEXT("b739a555477da17858f2ad1f97824b3191017f3cdb3703b6de19337d0622b93b")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset"), TEXT("eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.uasset"), TEXT("cebb1dcd37be20da472c0982d6b45143315961204ad382a0eb6d4f55b6c92987")},
	{TEXT("ref/config-tables/编译客户端配置/CSV/AllianceOfficially.csv"), TEXT("62f0e3f339aedd8ed255ae84b8473cf3012ed1a42aa52bf733afb99ccabcfb58")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildAppointMediator.cs"), TEXT("5a2b04523af0ca11a0c25fcdc589cd0259fdcea514d7ada3852253585a0990ef")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/AllianceProxy.cs"), TEXT("67c0dea2cb390479f764a902ef4c903f8fd14ca8898c43662d61bc6422d66dbc")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildMain/UI_Win_GuildAppoint.prefab"), TEXT("1b94c664c287627b6d6d89b812ddc8fd0f7e5cd07011d9e15b35abcc2231afe3")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/ui_alliance.tpsheet"), TEXT("a40593228f958e94763568a65b30e4e539d5c141b280727b74cd5d74c2da446b")},
	{TEXT("Saved/PendingAllianceTechnologyReadonlyCatalogV3/ready_for_root_review.json"), TEXT("96eacd486f8a1b791cf0603e46753ec9ed7d64a160d279c55e40bbf00c91d925")},
	{TEXT("Saved/PendingAllianceTechnologyReadonlyCatalogV4/ready_for_root_review.json"), TEXT("11ce1e2b4442c9ab2d3aa5ba19535de98b7b39559694fcd3cff9753c9ae0aa9d")},
	{TEXT("Saved/PendingAllianceTechnologyReadonlyCatalogV1/alliance_technology_readonly_catalog_v1.json"), TEXT("82c8868cc3a83dec1fc0c7e5d126cc567a7d7b1d0781d892496901f94ba557ad")},
	{TEXT("Saved/PendingAllianceTechnologyReadonlyCatalogV1/source_pins.json"), TEXT("0bae44420a070053e115f3ecbf8f780b8abe2bc7688e4e71277ea40151dcc390")},
	{TEXT("Saved/PendingAllianceTechnologyReadonlyCatalogV1/ExtractAllianceTechnologyReadonlyCatalogV1.py"), TEXT("7c07da81916d99c02aca8903f98032a6d8a7ee8c33f0801aed5e71d3e9321562")},
	{TEXT("Saved/PendingAllianceOfficeReadonlyRuntimeAuditV3/Scripts/VerifyRokAllianceOfficeReadonlyRuntime.py"), TEXT("9cc3a9b7e0b12acf4412f36819a4e17c7ed5a04d9df91f316b12254d87370294")},
	{TEXT("Saved/RokUnityMigration/AllianceTechnologyReadonlyCatalogV1.json"), TEXT("82c8868cc3a83dec1fc0c7e5d126cc567a7d7b1d0781d892496901f94ba557ad")},
	{TEXT("Content/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1.uasset"), TEXT("336966bd131d5ec025ae6d25a87876d273f290e7642748697218922c479684f1")},
	{TEXT("Content/RokPrototype/UI/ResearchAtlases/rok_research_research_icon2.uasset"), TEXT("004ca2b459daa6aea56237517e89104c2cbcecd49cf9234b689caabeed905c70")},
	{TEXT("ref/config-tables/编译客户端配置/CSV/AllianceStudy.csv"), TEXT("96ba1ad61d229cb5228d38fab88d778dcff6b2197b367ae2f578fa1353fe3bd8")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_research/research_icon1.png"), TEXT("2e40036c679cc7405df898ff13751f93acbe603a05c374c15a85c958de9ebe3f")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_research/research_icon1.tpsheet"), TEXT("61957ddd9fef313c7fa1bed7671b77aaa9268c04d25691559cffb3d3ed480e15")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_research/research_icon2.png"), TEXT("86b31632f29d8c33c026c7a902626b4a88310370a49cfac87377c220404feb62")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_research/research_icon2.tpsheet"), TEXT("2260849a4c8ce16f9299cbb543c2bb56d781692b6365a6d7b4f9f4bfff39ac98")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildMainMediator.cs"), TEXT("a81406c8d7aa09372fa7b57aeb6f3b5e1138cc9143895df10692990907d10c87")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/Research/UI_Win_GuildResearchMediator.cs"), TEXT("6cd7a4447927bc1443ea5218c9111bc65800e59b804137fca2753ae2a9112d5f")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/AllianceResarchProxy.cs"), TEXT("fe0175f2a9d796fd103f3e173af0e711848d83b495f4f411c33742a4dfb56915")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildResearch/UI_Win_GuildResearch.prefab"), TEXT("4ec82d017a9f67af70a3c0e73720b4fd229f49390ab12d89d1202527e4036231")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildResearch/UI_Item_GuildResearchItem.prefab"), TEXT("69b1b9d74e73fa764fb662404f19c8ef42145066333e0efce734bfee421063bf")},
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
		Json->SetObjectField(TEXT("render_translation"), Point(Widget ? Widget->GetRenderTransform().Translation : FVector2D::ZeroVector));
if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Json->SetStringField(TEXT("text"), Text->GetText().ToString());
	if (const UImage* Image = Cast<UImage>(Widget))
	{
		const FSlateBrush& Brush = Image->GetBrush();
		const FBox2f UV = Brush.GetUVRegion();
		Json->SetStringField(TEXT("brush_resource"), Brush.GetResourceObject() ? Brush.GetResourceObject()->GetPathName() : FString());
		Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(UV.Min.X, UV.Min.Y)));
		Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(UV.Max.X, UV.Max.Y)));
		Json->SetObjectField(TEXT("brush_size"), Point(Brush.ImageSize));
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

bool URokAllianceTechnologyReadonlyAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false; // Pending-completion count is only exposed to audited Editor builds.
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokAllianceTechnologyReadonlyAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceTechnologyReadonlyAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokAllianceTechnologyReadonlyAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceTechnologyReadonlyAuditSubsystem, STATGROUP_Tickables);
}
void URokAllianceTechnologyReadonlyAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("PendingAllianceTechnologyReadonlyRuntimeAuditV5/RuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokAllianceTechnologyReadonlyAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceTechnologyReadonlyAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokAllianceTechnologyReadonlyAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokAllianceTechnologyReadonlyAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokAllianceTechnologyReadonlyAuditSubsystem::PrepareOwnWindowFocus()
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


bool URokAllianceTechnologyReadonlyAuditSubsystem::Begin()
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
    if (!Expect(Catalog->LoadAllianceTechnologyReadonly(), TEXT("Alliance technology static config unavailable"))) return false;
    const auto& Rows = Catalog->GetAllianceTechnologyReadonlyRows();
    TSet<int32> Types;
    int32 Edges = 0;
    for (const auto& Row : Rows)
    {
        if (!Expect(Row.Id > 0 && Row.CategoryType >= 1 && Row.CategoryType <= 3
            && Row.StudyType > 0 && Row.LevelKey > 0 && !Row.Title.IsEmpty()
            && !Row.Description.IsEmpty() && !Row.Title.Contains(TEXT("\uFFFD"))
            && !Row.Description.Contains(TEXT("\uFFFD"))
            && (Row.IconAtlas == TEXT("research_icon1") || Row.IconAtlas == TEXT("research_icon2"))
            && Row.IconRect.Width() > 0 && Row.IconRect.Height() > 0,
            TEXT("Technology source identity/localization/icon invalid"))) return false;
        if (Row.LevelKey == 1) Types.Add(Row.StudyType);
        Edges += Row.Prerequisites.Num();
    }
    for (int32 Type : Types) StudyTypes.Add(Type); StudyTypes.Sort();
    if (!Expect(Rows.Num() == 460 && StudyTypes.Num() == 60 && Edges == 202
        && StudyTypes[0] == 101 && StudyTypes[21] == 122
        && StudyTypes[22] == 201 && StudyTypes[37] == 216
        && StudyTypes[38] == 301 && StudyTypes[59] == 322,
        TEXT("Expected 460 rows, 60 study types and 202 prerequisites"))) return false;
    for (const TCHAR* Asset : {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance"),
        TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1.rok_research_research_icon1"),
        TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon2.rok_research_research_icon2")})
        if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr,
            TEXT("Required Alliance texture failed to load"))) return false;
    OriginalPointer = FSlateApplication::Get().GetCursorPos();
    LastPointer = OriginalPointer;
    bPointerSaved = true;
    Inputs = MakeShared<FJsonObject>();
    for (const FInputPin& Pin : Pinned)
    {
        const FString Actual = FileSha(FPaths::ProjectDir() / Pin.Path);
        if (!Expect(Actual == Pin.Sha, TEXT("Pinned source/product SHA differs from reviewed input"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }
    for (const TCHAR* Path : {TEXT("Source/Rok/RokAllianceTechnologyReadonlyAuditSubsystem.h"),
        TEXT("Source/Rok/RokAllianceTechnologyReadonlyAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokAllianceTechnologyReadonlyRuntime.py")})
    {
        const FString Actual = FileSha(FPaths::ProjectDir() / Path);
        if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
        Inputs->SetStringField(Path, Actual);
    }
    CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
        this, &URokAllianceTechnologyReadonlyAuditSubsystem::OnUpgradeCompleted);
    return true;
}

TSharedRef<FJsonObject> URokAllianceTechnologyReadonlyAuditSubsystem::BusinessState() const
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

bool URokAllianceTechnologyReadonlyAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}

TSharedRef<FJsonObject> URokAllianceTechnologyReadonlyAuditSubsystem::State() const
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
#endif
    Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
    const bool bReference = Screen.IsValid() && Screen->bAllianceTechnologyReferenceOpen;
    const FRokAllianceTechnologyUiSettledSample Settled = RokAllianceTechnologyUiSettledProbe::Sample(Screen.Get(), bReference);
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
    Json->SetObjectField(TEXT("alliance_ui_settled"), Probe);
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
    Widgets->SetObjectField(TEXT("AllianceFeatureButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")) : nullptr));
    for (const TCHAR* Name : {TEXT("FeatureModeCanvas"), TEXT("FeatureFrameClose"), TEXT("FeatureFrameTitle"),
        TEXT("AllianceTechnologyReferenceOpen"), TEXT("AllianceTechnologyReferenceBack"),
        TEXT("AllianceTechnologyReferenceBoundary"), TEXT("AllianceTechnologyDefinitionScroll"),
        TEXT("AllianceTechnologyDetailTitle"), TEXT("AllianceTechnologyDetailIdentity"),
        TEXT("AllianceTechnologyDetailDescription"), TEXT("AllianceTechnologyDetailPrerequisites"),
        TEXT("AllianceTechnologyDetailIcon"), TEXT("AllianceTechnologyIconUnavailable"),
        TEXT("AllianceTechnologyConfiguredPrevious"), TEXT("AllianceTechnologyConfiguredNext")})
        Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
    Json->SetObjectField(TEXT("widgets"), Widgets);
    Json->SetNumberField(TEXT("source_row_count"), Catalog.IsValid() ? Catalog->GetAllianceTechnologyReadonlyRows().Num() : 0);
    Json->SetNumberField(TEXT("study_type_count"), StudyTypes.Num());
    Json->SetNumberField(TEXT("active_mode"), Screen.IsValid() ? static_cast<int32>(Screen->ActiveMode) : -1);
    Json->SetNumberField(TEXT("alliance_preview_state"), Screen.IsValid() ? Screen->AlliancePreviewState : -1);
    Json->SetNumberField(TEXT("selected_alliance_page"), Screen.IsValid() ? Screen->SelectedAlliancePage : -1);
    Json->SetBoolField(TEXT("technology_reference_open"), Screen.IsValid() && Screen->bAllianceTechnologyReferenceOpen);
    Json->SetNumberField(TEXT("selected_category"), Screen.IsValid() ? Screen->SelectedAllianceTechnologyCategory : -1);
    Json->SetNumberField(TEXT("selected_study_type"), Screen.IsValid() ? Screen->SelectedAllianceTechnologyStudyType : -1);
    Json->SetNumberField(TEXT("selected_config_level"), Screen.IsValid() ? Screen->SelectedAllianceTechnologyLevelKey : -1);
    Json->SetNumberField(TEXT("pending_action_id"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2);
    TArray<TSharedPtr<FJsonValue>> Children;
    if (Screen.IsValid()) if (const UCanvasPanel* Canvas = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))))
        for (int32 Index = 0; Index < Canvas->GetChildrenCount(); ++Index)
            Children.Add(MakeShared<FJsonValueObject>(WidgetState(Canvas->GetChildAt(Index))));
    Json->SetArrayField(TEXT("all_direct_children_including_hidden"), Children);
    TArray<TSharedPtr<FJsonValue>> Atlases;
    for (const TCHAR* Path : {
        TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1.rok_research_research_icon1"),
        TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon2.rok_research_research_icon2")})
    {
        const UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
        const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
        auto Atlas = MakeShared<FJsonObject>();
        Atlas->SetStringField(TEXT("path"), Path);
        Atlas->SetBoolField(TEXT("srgb"), Texture && Texture->SRGB);
        Atlas->SetBoolField(TEXT("ui_group"), Texture && Texture->LODGroup == TEXTUREGROUP_UI);
        Atlas->SetBoolField(TEXT("no_mip_generation"), Texture && Texture->MipGenSettings == TMGS_NoMipmaps);
        Atlas->SetBoolField(TEXT("alpha_channel"), Texture && Texture->HasAlphaChannel());
        Atlas->SetNumberField(TEXT("texture_mips"), Texture ? Texture->GetNumMips() : 0);
        Atlas->SetNumberField(TEXT("texture_width"), Texture ? Texture->GetSizeX() : 0);
        Atlas->SetNumberField(TEXT("texture_height"), Texture ? Texture->GetSizeY() : 0);
        Atlas->SetBoolField(TEXT("rhi_valid"), Resource && Resource->TextureRHI.IsValid());
        const FIntPoint Extent = Resource && Resource->TextureRHI.IsValid() ? Resource->TextureRHI->GetDesc().Extent : FIntPoint::ZeroValue;
        Atlas->SetNumberField(TEXT("rhi_width"), Extent.X); Atlas->SetNumberField(TEXT("rhi_height"), Extent.Y);
        Atlas->SetNumberField(TEXT("rhi_mips"), Resource && Resource->TextureRHI.IsValid() ? Resource->TextureRHI->GetDesc().NumMips : 0);
        Atlases.Add(MakeShared<FJsonValueObject>(Atlas));
    }
    Json->SetArrayField(TEXT("research_atlases"), Atlases);
    if (Catalog.IsValid() && Screen.IsValid() && Screen->bAllianceTechnologyReferenceOpen)
    {
        TArray<TSharedPtr<FJsonValue>> AllRows;
        TArray<TSharedPtr<FJsonValue>> Definitions;
        int32 Edges = 0;
        for (const auto& Row : Catalog->GetAllianceTechnologyReadonlyRows())
        {
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("id"), Row.Id);
            Entry->SetNumberField(TEXT("category_type"), Row.CategoryType);
            Entry->SetNumberField(TEXT("study_type"), Row.StudyType);
            Entry->SetNumberField(TEXT("level_key"), Row.LevelKey);
            Entry->SetNumberField(TEXT("title_locale_id"), Row.TitleLocaleId);
            Entry->SetNumberField(TEXT("description_locale_id"), Row.DescriptionLocaleId);
            Entry->SetStringField(TEXT("title"), Row.Title);
            Entry->SetStringField(TEXT("description"), Row.Description);
            Entry->SetStringField(TEXT("icon_atlas"), Row.IconAtlas);
            Entry->SetStringField(TEXT("icon_sprite"), Row.IconSprite);
            auto Rect = MakeShared<FJsonObject>();
            Rect->SetNumberField(TEXT("x"), Row.IconRect.Min.X); Rect->SetNumberField(TEXT("y"), Row.IconRect.Min.Y);
            Rect->SetNumberField(TEXT("w"), Row.IconRect.Width()); Rect->SetNumberField(TEXT("h"), Row.IconRect.Height());
            Entry->SetObjectField(TEXT("icon_rect"), Rect);
            Entry->SetObjectField(TEXT("atlas_size"), Point(FVector2D(Row.AtlasSize.X, Row.AtlasSize.Y)));
            TArray<TSharedPtr<FJsonValue>> Requirements;
            for (const auto& P : Row.Prerequisites)
            {
                auto Requirement = MakeShared<FJsonObject>();
                Requirement->SetNumberField(TEXT("slot"), P.Slot);
                Requirement->SetNumberField(TEXT("study_type"), P.StudyType);
                Requirement->SetNumberField(TEXT("required_level"), P.RequiredLevel);
                Requirement->SetNumberField(TEXT("source_row_id"), P.SourceRowId);
                Requirements.Add(MakeShared<FJsonValueObject>(Requirement));
                ++Edges;
            }
            Entry->SetArrayField(TEXT("prerequisites"), Requirements);
            AllRows.Add(MakeShared<FJsonValueObject>(Entry));
            if (Row.LevelKey == 1 && Row.CategoryType == Screen->SelectedAllianceTechnologyCategory)
            {
                auto Definition = MakeShared<FJsonObject>();
                Definition->SetNumberField(TEXT("study_type"), Row.StudyType);
                if (const URokUnityIndexedButton* Hit = Cast<URokUnityIndexedButton>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceTechnologyDefinitionHit_%d"), Row.StudyType))))
                    Definition->SetNumberField(TEXT("action_id"), Hit->ActionId);
                for (const TCHAR* Kind : {TEXT("Definition"), TEXT("Name"), TEXT("Id"), TEXT("Icon"), TEXT("DefinitionHit")})
                {
                    const FString Name = FCString::Strcmp(Kind, TEXT("DefinitionHit")) == 0
                        ? FString::Printf(TEXT("AllianceTechnologyDefinitionHit_%d"), Row.StudyType)
                        : FString::Printf(TEXT("AllianceTechnology%s_%d"), Kind, Row.StudyType);
                    Definition->SetObjectField(Kind, WidgetState(Screen->GetWidgetFromName(*Name)));
                }
                Definitions.Add(MakeShared<FJsonValueObject>(Definition));
            }
        }
        Json->SetArrayField(TEXT("source_rows"), AllRows);
        Json->SetArrayField(TEXT("definition_widgets"), Definitions);
        Json->SetNumberField(TEXT("prerequisite_edges"), Edges);
        if (const UScrollBox* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionScroll"))))
            Json->SetNumberField(TEXT("definition_scroll_offset"), Scroll->GetScrollOffset());
    }
    return Json;
}

bool URokAllianceTechnologyReadonlyAuditSubsystem::Ready(int32 Phase) const
{
    if (!Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Runtime.IsValid() || !Catalog.IsValid()
        || !Server.IsValid() || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
        || Server->GetSuppressedLocalRequestCount() != 0 || Modal->IsModalOpen()) return false;
    if (Phase == 0 || Phase == 11 || Phase == 14)
        return !Screen->IsScreenOpen() && Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")))
            && RokAllianceTechnologyUiSettledProbe::ResearchAtlasesReady();
    const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Screen->IsScreenOpen() || !Mode || !Visible(Mode)
        || !FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0f, .01f)
        || !Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01f)
        || !Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const bool bReference = Phase != 1 && Phase != 10 && Phase != 12;
    if (!RokAllianceTechnologyUiSettledProbe::Sample(Screen.Get(), bReference).bReady
        || Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance
        || Screen->AlliancePreviewState != 0 || Screen->SelectedAlliancePage != 0
        || Screen->bAllianceTechnologyReferenceOpen != bReference
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE) return false;
    const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureFrameTitle")));
    if (!Title || Title->GetText().ToString() != (bReference ? TEXT("联盟科技配置参考") : TEXT("联盟"))) return false;
    if (!bReference) return Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceOpen")))
        && !Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceBack"));
    if (!Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceBack")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionScroll")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailTitle")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailIdentity")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailDescription")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailPrerequisites")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailIcon")))) return false;
    const int32 Category = Phase == 6 || Phase == 7 ? 2 : Phase == 8 || Phase == 9 ? 3 : 1;
    const int32 Study = Phase == 5 ? 122 : Phase == 6 ? 201 : Phase == 7 ? 216
        : Phase == 8 ? 301 : Phase == 9 ? 322 : 101;
    const int32 Level = Phase == 3 ? 2 : 1;
    if (Screen->SelectedAllianceTechnologyCategory != Category
        || Screen->SelectedAllianceTechnologyStudyType != Study
        || Screen->SelectedAllianceTechnologyLevelKey != Level) return false;
    const auto& Rows = Catalog->GetAllianceTechnologyReadonlyRows();
    if (Rows.Num() != 460 || StudyTypes.Num() != 60) return false;
    int32 CategoryDefinitions = 0, Edges = 0;
    const FRokUnityAllianceTechnologyReadonlyRow* Selected = nullptr;
    for (const auto& Row : Rows)
    {
        Edges += Row.Prerequisites.Num();
        if (Row.CategoryType != Category || Row.LevelKey != 1) continue;
        ++CategoryDefinitions;
        const UTextBlock* Name = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceTechnologyName_%d"), Row.StudyType)));
        const UTextBlock* Id = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceTechnologyId_%d"), Row.StudyType)));
        const UWidget* Icon = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceTechnologyIcon_%d"), Row.StudyType));
        const UWidget* Card = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceTechnologyDefinition_%d"), Row.StudyType));
        const URokUnityIndexedButton* Hit = Cast<URokUnityIndexedButton>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceTechnologyDefinitionHit_%d"), Row.StudyType)));
        if (!Name || Name->GetText().ToString() != Row.Title || !Id
            || Id->GetText().ToString() != FString::Printf(TEXT("studyType %d · 配置等级 1"), Row.StudyType)
            || !Icon || !Card || !Hit || Hit->ActionId != 17000 + Row.StudyType) return false;
    }
    for (const auto& Row : Rows) if (Row.StudyType == Study && Row.LevelKey == Level) Selected = &Row;
    if (CategoryDefinitions != (Category == 2 ? 16 : 22) || Edges != 202 || !Selected) return false;
    const UTextBlock* Detail = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailTitle")));
    const UTextBlock* Identity = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailIdentity")));
    const UTextBlock* Description = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailDescription")));
    const UTextBlock* Requirements = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDetailPrerequisites")));
    FString Prereq = TEXT("配置前置 ID：");
    if (Selected->Prerequisites.IsEmpty()) Prereq += TEXT("无");
    for (int32 Index = 0; Index < Selected->Prerequisites.Num(); ++Index)
    {
        if (Index) Prereq += TEXT("、");
        Prereq += FString::FromInt(Selected->Prerequisites[Index].SourceRowId);
    }
    return Detail && Detail->GetText().ToString() == Selected->Title
        && Description && Description->GetText().ToString() == Selected->Description
        && Identity && Identity->GetText().ToString() == FString::Printf(TEXT("源配置行 ID %d · studyType %d · 配置等级 %d"), Selected->Id, Study, Level)
        && Requirements && Requirements->GetText().ToString() == Prereq;
}

bool URokAllianceTechnologyReadonlyAuditSubsystem::ScrollReady(int32 Phase)
{
    // Selection rebuilds this UScrollBox. Require two distinct laid-out frames of the
    // same replacement widget with an actual positive Slate scroll range.
    if (!Ready(Phase)) { ScrollStableFrames = 0; ScrollStableWidget.Reset(); return false; }
    UScrollBox* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionScroll")));
    if (!Scroll) { ScrollStableFrames = 0; ScrollStableWidget.Reset(); return false; }
    const FVector2D Size = Scroll->GetCachedGeometry().GetLocalSize();
    const float End = Scroll->GetScrollOffsetOfEnd();
    if (Size.X <= 0.0f || Size.Y <= 0.0f || End <= 0.0f || Scroll->GetScrollOffset() >= End)
    {
        ScrollStableFrames = 0; ScrollStableWidget.Reset(); return false;
    }
    if (ScrollStableWidget.Get() == Scroll && GFrameCounter > ScrollStableLastFrame
        && Size.Equals(ScrollStableSize, 0.5f) && FMath::IsNearlyEqual(End, ScrollStableEnd, 0.5f))
    {
        ++ScrollStableFrames;
    }
    else
    {
        ScrollStableFrames = 1;
        ScrollStableFirstFrame = GFrameCounter;
    }
    ScrollStableWidget = Scroll;
    ScrollStableSize = Size;
    ScrollStableEnd = End;
    ScrollStableLastFrame = GFrameCounter;
    return ScrollStableFrames >= 2;
}

bool URokAllianceTechnologyReadonlyAuditSubsystem::ScrollTechnology(const TCHAR* Label)
{
    UScrollBox* Scroll = Screen.IsValid() ? Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionScroll"))) : nullptr;
    if (!Expect(Scroll && Visible(Scroll) && ScrollEvents < 3 && Boundary(), TEXT("Technology scroll unavailable"))) return false;
    const FGeometry Geometry = Scroll->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Technology scroll has zero geometry"))) return false;
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    FSlateApplication& App = FSlateApplication::Get();
    App.SetCursorPos(Center);
    App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
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
    bool bForeground = false;
    uint32 OwnerPid = 0;
#if PLATFORM_WINDOWS
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    if (Handle) { DWORD Pid = 0; ::GetWindowThreadProcessId(Handle, &Pid); OwnerPid = Pid; }
    bForeground = Handle && ::GetForegroundWindow() == Handle && OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
    if (!Expect(bScrollInPath && bOwnWindow && bForeground, TEXT("Technology scroll not foreground exact Slate path"))) return false;
    const float Before = Scroll->GetScrollOffset();
    const float EndBefore = Scroll->GetScrollOffsetOfEnd();
    if (!Expect(ScrollStableWidget.Get() == Scroll && ScrollStableFrames >= 2
        && ScrollStableFirstFrame < ScrollStableLastFrame && ScrollStableLastFrame == GFrameCounter
        && Size.Equals(ScrollStableSize, 0.5f) && FMath::IsNearlyEqual(EndBefore, ScrollStableEnd, 0.5f)
        && EndBefore > Before, TEXT("Technology scroll layout/range not settled"))) return false;
    const uint64 PressBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    auto Action = MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("label"), Label);
    Action->SetStringField(TEXT("method"), TEXT("ProcessMouseWheelOrGestureEvent"));
    Action->SetStringField(TEXT("target"), TEXT("AllianceTechnologyDefinitionScroll"));
    Action->SetStringField(TEXT("target_widget_class"), Scroll->GetClass()->GetPathName());
    Action->SetNumberField(TEXT("step"), Step);
    Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Action->SetObjectField(TEXT("absolute_center"), Point(Center));
    Action->SetArrayField(TEXT("hit_path_types"), PathTypes);
    Action->SetBoolField(TEXT("exact_scroll_in_path"), bScrollInPath);
    Action->SetBoolField(TEXT("own_game_window"), bOwnWindow);
    Action->SetBoolField(TEXT("foreground"), bForeground);
    Action->SetNumberField(TEXT("own_window_pid"), OwnerPid);
    Action->SetNumberField(TEXT("wheel_delta"), -2048);
    Action->SetNumberField(TEXT("scroll_before"), Before);
    Action->SetNumberField(TEXT("scroll_end_before"), EndBefore);
    Action->SetObjectField(TEXT("scroll_geometry_size"), Point(Size));
    Action->SetNumberField(TEXT("layout_stable_frames"), ScrollStableFrames);
    Action->SetNumberField(TEXT("layout_first_frame"), static_cast<double>(ScrollStableFirstFrame));
    Action->SetNumberField(TEXT("layout_last_frame"), static_cast<double>(ScrollStableLastFrame));
    Action->SetObjectField(TEXT("business_before"), BusinessState());
    const bool bHandled = App.ProcessMouseWheelOrGestureEvent(
        FPointerEvent(0, Center, Center, TSet<FKey>(), EKeys::Invalid, -2048.0f, FModifierKeysState()), nullptr);
    Action->SetBoolField(TEXT("wheel_handled"), bHandled);
    Action->SetNumberField(TEXT("scroll_after"), Scroll->GetScrollOffset());
    Action->SetObjectField(TEXT("business_after"), BusinessState());
    Action->SetNumberField(TEXT("primary_before"), static_cast<double>(PressBefore));
    Action->SetNumberField(TEXT("primary_after"), static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
    const bool bSafe = bHandled && Scroll->GetScrollOffset() > Before
        && Boundary() && PressBefore == Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    Action->SetStringField(TEXT("failure"), bSafe ? TEXT("") : TEXT("unhandled wheel/no movement/business change"));
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    ++ScrollEvents;
    return Expect(bSafe, TEXT("Actual Technology Slate wheel failed"));
}

void URokAllianceTechnologyReadonlyAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Alliance technology read-only route emitted an upgrade completion event"));
}

bool URokAllianceTechnologyReadonlyAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
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
	const bool bRouteSafe = Down && Up && Before == After && (!bBaselineSet || Boundary());
	Attempt->SetStringField(TEXT("failure"), bRouteSafe ? TEXT("") : TEXT("unhandled, world input, or business mutation"));
	Actions.Add(MakeShared<FJsonValueObject>(Attempt));
	StableTarget.Reset(); ClickStableFrames = 0;
	return Expect(bRouteSafe, TEXT("Slate click failed or changed baseline state"));
}



void URokAllianceTechnologyReadonlyAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
		&& (bBaselineSet ? Boundary() : Ready(0)), TEXT("Screenshot request/boundary invalid"))) return;
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
void URokAllianceTechnologyReadonlyAuditSubsystem::ScreenshotProcessed()
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
void URokAllianceTechnologyReadonlyAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(!bBaselineSet || Boundary(), TEXT("Post-screenshot business state changed"))) return;
	Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(NextStep);
}
void URokAllianceTechnologyReadonlyAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
	ScrollStableWidget.Reset(); ScrollStableFrames = 0; ScrollStableEnd = 0.0f;
	ScrollStableFirstFrame = 0; ScrollStableLastFrame = 0; ScrollStableSize = FVector2D::ZeroVector;
}

bool URokAllianceTechnologyReadonlyAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_technology_readonly.runtime_audit.v2"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 15
        && Actions.Num() == 20 && ScrollEvents == 3 && Attempts.IsEmpty()
        && CompletionEvents == 0 && bBaselineSet && Boundary());
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetStringField(TEXT("scope"), TEXT("Anonymous HUD Alliance technology static reference: 460 source rows, 60 widget identities, six first/last Slate selections, three real wheels, configured level 1/2, close/reopen"));
    Json->SetStringField(TEXT("baseline_phase"), TEXT("00_city_hud"));
    Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
    Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
    Json->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
    Json->SetBoolField(TEXT("full_migration_accepted"), false);
    Json->SetBoolField(TEXT("alliance_membership_accepted"), false);
    Json->SetBoolField(TEXT("research_level_or_progress_accepted"), false);
    Json->SetBoolField(TEXT("account_alliance_state_accepted"), false);
    Json->SetNumberField(TEXT("source_row_count"), 460);
    Json->SetNumberField(TEXT("study_type_count"), StudyTypes.Num());
    Json->SetNumberField(TEXT("prerequisite_edges"), 202);
    Json->SetNumberField(TEXT("scroll_events"), ScrollEvents);
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


void URokAllianceTechnologyReadonlyAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 15
        && Actions.Num() == 20 && Attempts.IsEmpty() && CompletionEvents == 0 && Boundary();
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Alliance technology read-only audit incomplete"));
    Save(Complete);
    UE_LOG(LogTemp, Display, TEXT("Rok alliance technology audit %s: %s"),
        Complete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok alliance technology audit: %s"), *Failures.Last());
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}


void URokAllianceTechnologyReadonlyAuditSubsystem::Tick(float DeltaTime)
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
                TEXT("Technology audit requires idle local business queues"))) return;
            BaselineBusinessJson = BusinessState();
            BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
            BaselineCamera = Controller->GetPawn()->GetActorTransform();
            BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            bBaselineSet = true;
            Capture(TEXT("00_city_hud"), 2);
        } break;
    case 2: if (Boundary() && Click(Cast<UButton>(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton"))), TEXT("open_alliance_from_hud"))) Enter(3); break;
    case 3: if (Boundary() && Stable(1)) Capture(TEXT("01_alliance_welcome"), 4); break;
    case 4: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceOpen"))), TEXT("open_technology_reference"))) Enter(5); break;
    case 5: if (Boundary() && Stable(2)) Capture(TEXT("02_develop_first_default"), 6); break;
    case 6: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionHit_101"))), TEXT("select_develop_first_101"))) Enter(7); break;
    case 7: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyConfiguredNext"))), TEXT("configured_level_next_2"))) Enter(8); break;
    case 8: if (Boundary() && Stable(3)) Capture(TEXT("03_configured_level_2"), 9); break;
    case 9: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyConfiguredPrevious"))), TEXT("configured_level_previous_1"))) Enter(10); break;
    case 10: if (Boundary() && Stable(4)) Capture(TEXT("04_configured_level_1"), 11); break;
    case 11: if (Boundary() && ScrollReady(4) && ScrollTechnology(TEXT("scroll_develop_to_last"))) Enter(12); break;
    case 12: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionHit_122"))), TEXT("select_develop_last_122"))) Enter(13); break;
    case 13: if (Boundary() && Stable(5)) Capture(TEXT("05_develop_last"), 14); break;
    case 14: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyCategory_2"))), TEXT("select_territory_category"))) Enter(15); break;
    case 15: if (Boundary() && Stable(6)) Capture(TEXT("06_territory_first_default"), 16); break;
    case 16: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionHit_201"))), TEXT("select_territory_first_201"))) Enter(17); break;
    case 17: if (Boundary() && ScrollReady(6) && ScrollTechnology(TEXT("scroll_territory_to_last"))) Enter(18); break;
    case 18: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionHit_216"))), TEXT("select_territory_last_216"))) Enter(19); break;
    case 19: if (Boundary() && Stable(7)) Capture(TEXT("07_territory_last"), 20); break;
    case 20: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyCategory_3"))), TEXT("select_war_category"))) Enter(21); break;
    case 21: if (Boundary() && Stable(8)) Capture(TEXT("08_war_first_default"), 22); break;
    case 22: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionHit_301"))), TEXT("select_war_first_301"))) Enter(23); break;
    case 23: if (Boundary() && ScrollReady(8) && ScrollTechnology(TEXT("scroll_war_to_last"))) Enter(24); break;
    case 24: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyDefinitionHit_322"))), TEXT("select_war_last_322"))) Enter(25); break;
    case 25: if (Boundary() && Stable(9)) Capture(TEXT("09_war_last"), 26); break;
    case 26: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceBack"))), TEXT("back_to_alliance_welcome"))) Enter(27); break;
    case 27: if (Boundary() && Stable(10)) Capture(TEXT("10_welcome_return"), 28); break;
    case 28: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))), TEXT("close_alliance_to_hud"))) Enter(29); break;
    case 29: if (Boundary() && Stable(11)) Capture(TEXT("11_hud_return"), 30); break;
    case 30: if (Boundary() && Click(Cast<UButton>(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton"))), TEXT("reopen_alliance_from_hud"))) Enter(31); break;
    case 31: if (Boundary() && Stable(12)) Capture(TEXT("12_alliance_reopened"), 32); break;
    case 32: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceOpen"))), TEXT("reopen_technology_reference"))) Enter(33); break;
    case 33: if (Boundary() && Stable(13)) Capture(TEXT("13_technology_reopened"), 34); break;
    case 34: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))), TEXT("final_close_alliance_to_hud"))) Enter(35); break;
    case 35: if (Boundary() && Stable(14)) Capture(TEXT("14_final_hud"), 36); break;
    case 36: Finish(); break;


    }
}
