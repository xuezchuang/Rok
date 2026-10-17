#include "RokAllianceShopReadonlyAuditSubsystem.h"

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
#include "Framework/Application/SlateUser.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
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
#include "Null/NullPlatformApplicationMisc.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
/** Read-only observation of Alliance's direct animated children; no Commander portrait exists here. */
struct FRokAllianceShopUiSettledSample
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

namespace RokAllianceShopUiSettledProbe
{

inline FRokAllianceShopUiSettledSample Sample(const URokUnityFeatureScreenWidget* Screen, bool bReference)
{
    FRokAllianceShopUiSettledSample Result;
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
        && (bReference ? Screen->GetWidgetFromName(TEXT("AllianceShopReferenceBack")) != nullptr
            : (Screen->GetWidgetFromName(TEXT("AllianceFeatureHit_7")) != nullptr || Screen->GetWidgetFromName(TEXT("AllianceWelcomeBanner")) != nullptr));
#if WITH_EDITOR
    Result.RemainingTextureCompilations = FTextureCompilingManager::Get().GetNumRemainingTextures();
#endif
    static const TCHAR* Paths[] = {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.rok_modal_atlas_item0"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.rok_modal_atlas_item1"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item2.rok_fullscreen_item2"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item3.rok_fullscreen_item3"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5")};
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
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceShopReadonlyAudit");
constexpr TCHAR OriginalRoot[] = TEXT("D:/ueDemo/Rok");

struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Exact Shop product, source chain and runtime boundaries are immutable inputs. Audit/verifier
// files are also hashed at runtime and compared against the staged release report.
constexpr FInputPin Pinned[] = {
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("a421b7c542f3f40bca8883960e0bea17c64b2fb956da46a1f70ad151f2923019")},
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("e5e395a0801e77b13455c74b01976024d070d3f11b458be2ceec6d8da08111e3")},
    {TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.cpp"), TEXT("398d8cd9ed8d35e4c94b89e5550f2bfe8306e49cd820a2010484bdb3e723f9c0")},
    {TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.h"), TEXT("829801026703048bb94ef2f4a9a267a8aaf313824c610ccb1b702d5433ca6997")},
    {TEXT("Saved/RokUnityMigration/AllianceShopReadonlyCatalogV1.json"), TEXT("094444ea65e2200dc423be9274d09baa7a4efcadbd3930e9125300fc11192c77")},
    {TEXT("ref/config-tables/编译客户端配置/CSV/AllianceShopItem.csv"), TEXT("ca6ac31adf5ddb6b0717f9c5d707709d22463690c0de5f87c3ee3969c6b4309d")},
    {TEXT("ref/config-tables/编译客户端配置/CSV/Item.csv"), TEXT("def848dd38b6acfba66b1e3dccb9dbe198ead1fa3702bdfc8d6bda954a6b079b")},
    {TEXT("Saved/RokUnityMigration/ConfigTables/Language.json"), TEXT("e228eca8deb7c8daa4a2d910f736c57567db11322d558544309bfbaca412277d")},
    {TEXT("ref/config-tables/s_Language.xlsm"), TEXT("c33d40148f60a915f04d8e201f303d8b0d4e07eab4ef28da10fe8b0d27aaed19")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item1.tpsheet"), TEXT("57996a51dd8db6f6baac1406cba0f5ecaf0a456c0f68a987429d5d322c42d678")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item2.tpsheet"), TEXT("6c35e5c8b40d704caf0b11e31ee8bbd439a1bae3dc339c776caa0b3b1578f5b8")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item3.tpsheet"), TEXT("d9f12287537c28df120bca1276ca03046456ff4f1432dccc0b6898669afa41bf")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item5.tpsheet"), TEXT("4ad7984172c25e3d1feadbd5571d2c9ccfe5684d66c0e968c3a1333583689870")},
    {TEXT("Saved/PendingAllianceShopReadonlyCatalogV1/ready_for_root_review.json"), TEXT("a86049b47029d37567de7b9eb56c9b4ed74ba81864c396abbcab15a3074ba104")},
    {TEXT("Saved/PendingAllianceShopReadonlyCatalogIndependentReviewV1/review.json"), TEXT("97b720586d73a52270c6e1be09731b88a3486d453cbb62dfbe2ca4f4b3e46190")},
    {TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.uasset"), TEXT("6a484213895179943b40db30e889415dd727d487e6e9ce1fa7de45f4e44aa99b")},
    {TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.uasset"), TEXT("9ee8869982773da206012e73f04393ca20f7482aac9208acd8c0f641bf6f5b78")},
    {TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item2.uasset"), TEXT("6795fe239d77389ef3b0143d1648506df2663c7b0087de5cec0b203537e31a6e")},
    {TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item3.uasset"), TEXT("1c41e92dd831ecf3617dfa3b29c24700c748644cddc6d7d96925a9cd7a740775")},
    {TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.uasset"), TEXT("114a8f85a0c657be30db2a02bfc1fa3aa267fdec85482ebbc307198bcba3fb47")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildMainMediator.cs"), TEXT("a81406c8d7aa09372fa7b57aeb6f3b5e1138cc9143895df10692990907d10c87")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/Store/UI_Win_GuildStoreMediator.cs"), TEXT("af6ed06ce0affe5385ff013c54363cc77e9a55a4774d77d2ff32f02ade28fc83")},
    {TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"), TEXT("b20503a0b0dfdfe552502ef307e6d8e6a79c9677e7a185fc0f589cd6c45a8ae5")},
    {TEXT("Source/Rok/RokBuildingUpgradeSubsystem.h"), TEXT("b2253b3fa8232a873e6f4f07fe4cd0f3127eb4cbd3c720a356794186e93ea7b4")},
    {TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("5b896308b0327f5e04c8f6434625a15703145f399893cd1306404b408724ae83")},
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
    {TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9dc34a7279c15f7cf153c8f063cb06d565be9f9792e50def35bac2195241f05c")},
    {TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef")},
    {TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"), TEXT("3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e")},
    {TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"), TEXT("e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112")},
    {TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"), TEXT("a3f6245b2c64f552e7b8a253e98f637d6ca0292a4aab87e5fd9183f03c76309f")},
    {TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.uasset"), TEXT("b739a555477da17858f2ad1f97824b3191017f3cdb3703b6de19337d0622b93b")},
    {TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset"), TEXT("eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e")},
    {TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.uasset"), TEXT("cebb1dcd37be20da472c0982d6b45143315961204ad382a0eb6d4f55b6c92987")},
};

FString PinnedPath(const TCHAR* Relative)
{
    const FString Name(Relative);
    const bool bOriginalReference = Name.StartsWith(TEXT("ref/"))
        || Name.StartsWith(TEXT("Saved/PendingAllianceShopReadonlyCatalog"))
        || Name == TEXT("Saved/RokUnityMigration/ConfigTables/Language.json");
    const FString Base = bOriginalReference ? FString(OriginalRoot) : FPaths::ProjectDir();
    return FPaths::ConvertRelativePathToFull(Base, Name);
}

bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeDirectoryName(Engine);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
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
FString ForegroundIdentity()
{
#if PLATFORM_WINDOWS
	return FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
#else
	return TEXT("unsupported_platform");
#endif
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

bool URokAllianceShopReadonlyAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false; // Pending-completion count is only exposed to audited Editor builds.
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokAllianceShopReadonlyAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
		&& FParse::Param(FCommandLine::Get(), TEXT("game"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceShopReadonlyAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokAllianceShopReadonlyAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceShopReadonlyAuditSubsystem, STATGROUP_Tickables);
}
void URokAllianceShopReadonlyAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("PendingAllianceShopReadonlyRuntimeAuditV2/RuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	int64 CandidateTicks = FDateTime::UtcNow().GetTicks();
    while (IFileManager::Get().DirectoryExists(*Directory))
    {
        ++CandidateTicks;
        Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
            / TEXT("PendingAllianceShopReadonlyRuntimeAuditV2/RuntimeAudit")
            / FString::Printf(TEXT("%lld_%u"), CandidateTicks,
                FPlatformProcess::GetCurrentProcessId()));
    }
    IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokAllianceShopReadonlyAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceShopReadonlyAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	VirtualUser.Reset();
	Super::Deinitialize();
}
bool URokAllianceShopReadonlyAuditSubsystem::Identity() const
{
	return TargetWorld(GetWorld()) && FApp::IsGame() && FSlateApplication::IsInitialized()
		&& FSlateApplication::Get().IsRenderingOffScreen()
		&& FNullPlatformApplicationMisc::IsUsingNullApplication();
}
bool URokAllianceShopReadonlyAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokAllianceShopReadonlyAuditSubsystem::Begin()
{
    if (!Expect(Identity() && FSlateApplication::IsInitialized() && IFileManager::Get().DirectoryExists(*Directory), TEXT("Wrong world/project, output, or Slate unavailable"))) return false;
    const TCHAR* Cursor = FCommandLine::Get();
    FString Token; int32 AuditFlags = 0, LocalFlags = 0, RouteFlags = 0;
    int32 OffscreenFlags = 0, GameFlags = 0;
    while (FParse::Token(Cursor, Token, false))
    {
        if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditFlags; continue; }
        if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlags; continue; }
        if (Token.Equals(TEXT("-RokOpenModal=alliance_joined"), ESearchCase::IgnoreCase)) { ++RouteFlags; continue; }
        if (Token.Equals(TEXT("-RenderOffScreen"), ESearchCase::IgnoreCase)) { ++OffscreenFlags; continue; }
        if (Token.Equals(TEXT("-game"), ESearchCase::IgnoreCase)) { ++GameFlags; continue; }
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)) return Expect(false, TEXT("Unexpected Rok fixture/action flag"));
    }
    if (!Expect(AuditFlags == 1 && LocalFlags == 1 && RouteFlags == 1
        && OffscreenFlags == 1 && GameFlags == 1,
        TEXT("Exactly one offscreen game audit, local-only and joined preview route required"))) return false;
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
        && Hud.IsValid() && Screen.IsValid() && Modal.IsValid() && !Modal->IsModalOpen()
        && Screen->ActiveMode == ERokUnityFeatureScreenMode::Alliance
        && Screen->AlliancePreviewState == 2 && Screen->SelectedAlliancePage == 0
        && Screen->bAllianceTestFixtureState && !Screen->bAllianceShopReferenceOpen
        && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
        TEXT("Expected local-only CLI joined fixture, anonymous GameApp HUD"))) return false;
    const auto Profile = Runtime->GetPlayerProfileState();
    if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
        && !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
        TEXT("Expected anonymous local-preview account boundary"))) return false;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("UnityInitId_5")))
    {
        if (Hall.IsValid()) return Expect(false, TEXT("Ambiguous UnityInitId_5 Hall")); Hall = *It;
    }
    if (!Expect(Hall.IsValid() && Hall->ActorHasTag(TEXT("UnityBuildingType_1")) && Hall->GetWorld() == GetWorld(),
        TEXT("Source Hall identity missing"))) return false;
    if (!Expect(Catalog->LoadAllianceShopReadonly(), TEXT("Shop static config unavailable"))) return false;
    const auto& ShopRows = Catalog->GetAllianceShopReadonlyRows();
    const auto& ShopGroups = Catalog->GetAllianceShopReadonlyGroups();
    if (!Expect(ShopRows.Num() == 21 && ShopGroups.Num() == 4,
        TEXT("Shop catalog row/group count differs"))) return false;
    const int32 ExpectedCounts[] = {4, 4, 4, 9};
    for (int32 Group = 1; Group <= 4; ++Group)
    {
        int32 Count = 0;
        for (const auto& Row : ShopRows) if (Row.Group == Group) ++Count;
        if (!Expect(ShopGroups[Group - 1].Group == Group && Count == ExpectedCounts[Group - 1],
            TEXT("Shop group membership differs"))) return false;
    }

    for (const TCHAR* Asset : {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.rok_modal_atlas_item0"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.rok_modal_atlas_item1"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item2.rok_fullscreen_item2"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item3.rok_fullscreen_item3"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5")})
        if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr, TEXT("Alliance texture failed to load"))) return false;
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    if (!Expect(Window.IsValid() && Window->IsVisible() && Native.IsValid()
        && Native->GetOSWindowHandle() == nullptr && Native->IsRenderingOffScreen()
        && Window->GetViewportSize().Equals(FVector2D(1440, 810), .1f),
        TEXT("Expected own 1440x810 Null offscreen GameViewport SWindow"))) return false;
    VirtualUser = FSlateApplication::Get().FindOrCreateVirtualUser(1);
    if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0,
        TEXT("Dedicated virtual Slate user unavailable"))) return false;
    ForegroundAtBegin = ForegroundIdentity();
    UserZeroFocusAtBegin = FSlateApplication::Get().GetUserFocusedWidget(0).Get();
    LastPointer = FVector2D::ZeroVector;
#if PLATFORM_WINDOWS
    POINT HardwareCursor = {};
    bHardwareCursorAtBegin = ::GetCursorPos(&HardwareCursor) != 0;
    HardwareCursorXAtBegin = HardwareCursor.x;
    HardwareCursorYAtBegin = HardwareCursor.y;
    if (!Expect(bHardwareCursorAtBegin, TEXT("Unable to observe hardware cursor"))) return false;
#endif
    Inputs = MakeShared<FJsonObject>();
    for (const FInputPin& Pin : Pinned)
    {
        const FString Actual = FileSha(PinnedPath(Pin.Path));
        if (!Expect(Actual == Pin.Sha, TEXT("Pinned source/product SHA differs from reviewed input"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }
    for (const TCHAR* Path : {TEXT("Source/Rok/RokAllianceShopReadonlyAuditSubsystem.h"),
        TEXT("Source/Rok/RokAllianceShopReadonlyAuditSubsystem.cpp")})
    {
        const FString Actual = FileSha(FPaths::ProjectDir() / Path);
        if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
        Inputs->SetStringField(Path, Actual);
    }
    CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(this, &URokAllianceShopReadonlyAuditSubsystem::OnUpgradeCompleted);
    return true;
}

TSharedRef<FJsonObject> URokAllianceShopReadonlyAuditSubsystem::BusinessState() const
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

bool URokAllianceShopReadonlyAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	if (!FSlateApplication::IsInitialized() || !VirtualUser.IsValid()
		|| VirtualUser->GetUserIndex() <= 0
		|| ForegroundIdentity() != ForegroundAtBegin
		|| FSlateApplication::Get().GetUserFocusedWidget(0).Get() != UserZeroFocusAtBegin) return false;
#if PLATFORM_WINDOWS
	POINT Cursor = {};
	if (!bHardwareCursorAtBegin || !::GetCursorPos(&Cursor)
		|| Cursor.x != HardwareCursorXAtBegin || Cursor.y != HardwareCursorYAtBegin) return false;
#endif
	return Encode(BusinessState()) == BaselineBusiness;
}


TSharedRef<FJsonObject> URokAllianceShopReadonlyAuditSubsystem::State() const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetObjectField(TEXT("business"), BusinessState());
    const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    Json->SetBoolField(TEXT("null_offscreen_window"), Native.IsValid()
        && Native->GetOSWindowHandle() == nullptr && Native->IsRenderingOffScreen()
        && Window.IsValid() && Window->IsVisible()
        && FNullPlatformApplicationMisc::IsUsingNullApplication());
    Json->SetNumberField(TEXT("virtual_user_index"), VirtualUser.IsValid() ? VirtualUser->GetUserIndex() : -1);
    Json->SetStringField(TEXT("foreground_identity"), ForegroundIdentity());
    Json->SetBoolField(TEXT("foreground_unchanged"), ForegroundIdentity() == ForegroundAtBegin);
    Json->SetBoolField(TEXT("user0_focus_unchanged"), FSlateApplication::IsInitialized()
        && FSlateApplication::Get().GetUserFocusedWidget(0).Get() == UserZeroFocusAtBegin);
#if PLATFORM_WINDOWS
    POINT Cursor = {};
    Json->SetBoolField(TEXT("hardware_cursor_unchanged"), bHardwareCursorAtBegin
        && ::GetCursorPos(&Cursor) && Cursor.x == HardwareCursorXAtBegin
        && Cursor.y == HardwareCursorYAtBegin);
#endif
    Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
    const bool bReference = Screen.IsValid() && Screen->IsScreenOpen() && Screen->bAllianceShopReferenceOpen;
    const FRokAllianceShopUiSettledSample Settled = RokAllianceShopUiSettledProbe::Sample(Screen.Get(), bReference);
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
    Widgets->SetObjectField(TEXT("AllianceFeatureButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")) : nullptr));
    for (const TCHAR* Name : {TEXT("FeatureModeCanvas"), TEXT("FeatureFrameClose"), TEXT("FeatureFrameTitle"),
        TEXT("AllianceFeatureHit_7"), TEXT("AllianceJoinedFixtureTag"), TEXT("AllianceShopReferenceBack"),
        TEXT("AllianceShopReferenceBoundary"), TEXT("AllianceShopReferenceUnavailable"),
        TEXT("AllianceShopDetailName"), TEXT("AllianceShopDetailIdentity"),
        TEXT("AllianceShopDetailBuyConfigPrice"), TEXT("AllianceShopDetailStockConfigPrice"),
        TEXT("AllianceShopDetailServerBoundary"), TEXT("AllianceShopDetailQuality"),
        TEXT("AllianceShopDetailIcon")})
        Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
    Json->SetObjectField(TEXT("widgets"), Widgets);
    Json->SetNumberField(TEXT("active_mode"), Screen.IsValid() ? static_cast<int32>(Screen->ActiveMode) : -1);
    Json->SetNumberField(TEXT("alliance_preview_state"), Screen.IsValid() ? Screen->AlliancePreviewState : -1);
    Json->SetNumberField(TEXT("selected_alliance_page"), Screen.IsValid() ? Screen->SelectedAlliancePage : -1);
    Json->SetBoolField(TEXT("alliance_fixture"), Screen.IsValid() && Screen->bAllianceTestFixtureState);
    Json->SetBoolField(TEXT("shop_reference_open"), bReference);
    Json->SetNumberField(TEXT("selected_shop_group"), Screen.IsValid() ? Screen->SelectedAllianceShopGroup : -1);
    Json->SetNumberField(TEXT("selected_shop_item_type"), Screen.IsValid() ? Screen->SelectedAllianceShopItemType : -1);
    Json->SetNumberField(TEXT("pending_action_id"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2);
    Json->SetNumberField(TEXT("shop_row_count"), Catalog.IsValid() ? Catalog->GetAllianceShopReadonlyRows().Num() : -1);
    Json->SetNumberField(TEXT("shop_group_count"), Catalog.IsValid() ? Catalog->GetAllianceShopReadonlyGroups().Num() : -1);
    if (bReference && Catalog.IsValid())
    {
        TArray<TSharedPtr<FJsonValue>> Groups;
        for (const auto& Group : Catalog->GetAllianceShopReadonlyGroups())
        {
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("group"), Group.Group);
            Entry->SetNumberField(TEXT("locale_id"), Group.NameLocaleId);
            Entry->SetStringField(TEXT("name"), Group.Name);
            Entry->SetObjectField(TEXT("button"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceShopGroup_%d"), Group.Group))));
            Groups.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Json->SetArrayField(TEXT("shop_groups"), Groups);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 Index = 0; Index < Catalog->GetAllianceShopReadonlyRows().Num(); ++Index)
        {
            const auto& Row = Catalog->GetAllianceShopReadonlyRows()[Index];
            if (Row.Group != Screen->SelectedAllianceShopGroup) continue;
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("global_index"), Index);
            Entry->SetNumberField(TEXT("item_type"), Row.ItemType);
            Entry->SetNumberField(TEXT("group"), Row.Group);
            Entry->SetNumberField(TEXT("stock_price"), Row.StockPrice);
            Entry->SetNumberField(TEXT("selling_price"), Row.SellingPrice);
            Entry->SetNumberField(TEXT("name_locale_id"), Row.NameLocaleId);
            Entry->SetStringField(TEXT("name"), Row.Name);
            Entry->SetNumberField(TEXT("quality"), Row.Quality);
            Entry->SetStringField(TEXT("icon_reference"), Row.IconReference);
            Entry->SetStringField(TEXT("icon_atlas"), Row.IconAtlas);
            Entry->SetStringField(TEXT("icon_sprite"), Row.IconSprite);
            Entry->SetNumberField(TEXT("icon_x"), Row.IconRect.Min.X);
            Entry->SetNumberField(TEXT("icon_y"), Row.IconRect.Min.Y);
            Entry->SetNumberField(TEXT("icon_width"), Row.IconRect.Width());
            Entry->SetNumberField(TEXT("icon_height"), Row.IconRect.Height());
            Entry->SetNumberField(TEXT("atlas_width"), Row.AtlasSize.X);
            Entry->SetNumberField(TEXT("atlas_height"), Row.AtlasSize.Y);
            Entry->SetNumberField(TEXT("icon_u_min"), static_cast<double>(Row.IconRect.Min.X) / Row.AtlasSize.X);
            Entry->SetNumberField(TEXT("icon_v_min"), static_cast<double>(Row.AtlasSize.Y - Row.IconRect.Max.Y) / Row.AtlasSize.Y);
            Entry->SetNumberField(TEXT("icon_u_max"), static_cast<double>(Row.IconRect.Max.X) / Row.AtlasSize.X);
            Entry->SetNumberField(TEXT("icon_v_max"), static_cast<double>(Row.AtlasSize.Y - Row.IconRect.Min.Y) / Row.AtlasSize.Y);
            Entry->SetObjectField(TEXT("row"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceShopRow_%d"), Row.ItemType))));
            Entry->SetObjectField(TEXT("label"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceShopName_%d"), Row.ItemType))));
            Entry->SetObjectField(TEXT("icon"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceShopIcon_%d"), Row.ItemType))));
            Entry->SetObjectField(TEXT("select"), WidgetState(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceShopSelect_%d"), Row.ItemType))));
            Rows.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Json->SetArrayField(TEXT("shop_rows"), Rows);
    }
    return Json;
}

const TCHAR* ShopIconAsset(const FString& Atlas)
{
    if (Atlas == TEXT("item1")) return TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.rok_modal_atlas_item1");
    if (Atlas == TEXT("item2")) return TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item2.rok_fullscreen_item2");
    if (Atlas == TEXT("item3")) return TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item3.rok_fullscreen_item3");
    if (Atlas == TEXT("item5")) return TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5");
    return nullptr;
}

bool URokAllianceShopReadonlyAuditSubsystem::RowsMatchCurrent() const
{
    if (!Catalog.IsValid() || !Screen.IsValid()) return false;
    const UCanvasPanel* Canvas = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")));
    if (!Canvas || !Visible(Canvas)) return false;
    const FGeometry CanvasGeo = Canvas->GetCachedGeometry();
    const FVector2D CanvasTop = CanvasGeo.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D CanvasBottom = CanvasGeo.LocalToAbsolute(CanvasGeo.GetLocalSize());
    if (!CanvasGeo.GetLocalSize().Equals(FVector2D(1440.0f, 810.0f), .25f)
        || !CanvasTop.Equals(FVector2D::ZeroVector, .75f)
        || !CanvasBottom.Equals(FVector2D(1440.0f, 810.0f), .75f)) return false;
    const int32 Group = Screen->SelectedAllianceShopGroup;
    const auto* Selected = Catalog->FindAllianceShopReadonly(Screen->SelectedAllianceShopItemType);
    if (!Selected || Selected->Group != Group) return false;
    const int32 Counts[] = {4, 4, 4, 9};
    if (Group < 1 || Group > 4 || Catalog->GetAllianceShopReadonlyRows().Num() != 21
        || Catalog->GetAllianceShopReadonlyGroups().Num() != 4) return false;
    int32 VisibleRows = 0; float PreviousBottom = 0;
    for (int32 Index = 0; Index < Catalog->GetAllianceShopReadonlyRows().Num(); ++Index)
    {
        const auto& Row = Catalog->GetAllianceShopReadonlyRows()[Index];
        if (Row.Group != Group) continue;
        const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceShopName_%d"), Row.ItemType)));
        const UWidget* Card = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceShopRow_%d"), Row.ItemType));
        const UImage* Icon = Cast<UImage>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceShopIcon_%d"), Row.ItemType)));
        const UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceShopSelect_%d"), Row.ItemType)));
        const auto* Indexed = Cast<URokUnityIndexedButton>(Button);
        const FString Expected = FString::Printf(TEXT("%s · ID %d · 品质 %d"), *Row.Name, Row.ItemType, Row.Quality);
        const TCHAR* AtlasPath = ShopIconAsset(Row.IconAtlas);
        const UTexture2D* Atlas = AtlasPath ? LoadObject<UTexture2D>(nullptr, AtlasPath) : nullptr;
        if (!Label || Label->GetText().ToString() != Expected || !Visible(Label) || !Visible(Card)
            || !Visible(Icon) || !Visible(Button) || !Indexed || Indexed->ActionId != 18100 + Index) return false;
        if (!Atlas || Icon->GetBrush().GetResourceObject() != Atlas) return false;
        const FBox2f Uv = Icon->GetBrush().GetUVRegion();
        if (!FMath::IsNearlyEqual(Uv.Min.X, static_cast<float>(Row.IconRect.Min.X) / Row.AtlasSize.X, 1e-4f)
            || !FMath::IsNearlyEqual(Uv.Min.Y, static_cast<float>(Row.AtlasSize.Y - Row.IconRect.Max.Y) / Row.AtlasSize.Y, 1e-4f)
            || !FMath::IsNearlyEqual(Uv.Max.X, static_cast<float>(Row.IconRect.Max.X) / Row.AtlasSize.X, 1e-4f)
            || !FMath::IsNearlyEqual(Uv.Max.Y, static_cast<float>(Row.AtlasSize.Y - Row.IconRect.Min.Y) / Row.AtlasSize.Y, 1e-4f)) return false;
        const FGeometry Geo = Button->GetCachedGeometry();
        const FGeometry CardGeo = Card->GetCachedGeometry();
        const FGeometry LabelGeo = Label->GetCachedGeometry();
        const FGeometry IconGeo = Icon->GetCachedGeometry();
        const FVector2D Top = Geo.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D Bottom = Geo.LocalToAbsolute(Geo.GetLocalSize());
        // CapturePoint converts the Unity 1440x900 Y into the 1440x810 canvas by subtracting 45.
        const float RowY = 270.0f + VisibleRows * 52.0f - 45.0f;
        const FVector2D ExpectedTop = CanvasGeo.LocalToAbsolute(FVector2D(202.0f, RowY));
        const FVector2D ExpectedBottom = CanvasGeo.LocalToAbsolute(FVector2D(820.0f, RowY + 48.0f));
        const FVector2D ExpectedLabelTop = CanvasGeo.LocalToAbsolute(FVector2D(261.0f, RowY + 9.0f));
        const FVector2D ExpectedLabelBottom = CanvasGeo.LocalToAbsolute(FVector2D(801.0f, RowY + 39.0f));
        const FVector2D ExpectedIconTop = CanvasGeo.LocalToAbsolute(FVector2D(209.0f, RowY + 4.0f));
        const FVector2D ExpectedIconBottom = CanvasGeo.LocalToAbsolute(FVector2D(249.0f, RowY + 44.0f));
        if (!Geo.GetLocalSize().Equals(FVector2D(618.0f, 48.0f), .25f)
            || !CardGeo.GetLocalSize().Equals(FVector2D(618.0f, 48.0f), .25f)
            || !LabelGeo.GetLocalSize().Equals(FVector2D(540.0f, 30.0f), .25f)
            || !IconGeo.GetLocalSize().Equals(FVector2D(40.0f, 40.0f), .25f)
            || !Top.Equals(ExpectedTop, .75f) || !Bottom.Equals(ExpectedBottom, .75f)
            || !CardGeo.LocalToAbsolute(FVector2D::ZeroVector).Equals(ExpectedTop, .75f)
            || !CardGeo.LocalToAbsolute(CardGeo.GetLocalSize()).Equals(ExpectedBottom, .75f)
            || !LabelGeo.LocalToAbsolute(FVector2D::ZeroVector).Equals(ExpectedLabelTop, .75f)
            || !LabelGeo.LocalToAbsolute(LabelGeo.GetLocalSize()).Equals(ExpectedLabelBottom, .75f)
            || !IconGeo.LocalToAbsolute(FVector2D::ZeroVector).Equals(ExpectedIconTop, .75f)
            || !IconGeo.LocalToAbsolute(IconGeo.GetLocalSize()).Equals(ExpectedIconBottom, .75f)
            || !(Top.X >= 190 && Bottom.X <= 840 && Top.Y >= CanvasTop.Y
                 && Bottom.Y <= 810 && Top.Y >= PreviousBottom - .25f)) return false;
        PreviousBottom = Bottom.Y; ++VisibleRows;
    }
    if (VisibleRows != Counts[Group - 1]) return false;
    for (const auto& GroupRow : Catalog->GetAllianceShopReadonlyGroups())
    {
        const UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceShopGroup_%d"), GroupRow.Group)));
        const auto* Indexed = Cast<URokUnityIndexedButton>(Button);
        if (!Visible(Button) || !Indexed || Indexed->ActionId != 690 + GroupRow.Group) return false;
    }
    auto TextIs = [this](const TCHAR* Name, const FString& Text) {
        const UTextBlock* Widget = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
        return Widget && Visible(Widget) && Widget->GetText().ToString() == Text;
    };
    const UImage* DetailIcon = Cast<UImage>(Screen->GetWidgetFromName(TEXT("AllianceShopDetailIcon")));
    const UImage* QualityIcon = Cast<UImage>(Screen->GetWidgetFromName(TEXT("AllianceShopDetailQuality")));
    const TCHAR* DetailAtlas = ShopIconAsset(Selected->IconAtlas);
    const FBox2f DetailUv = DetailIcon ? FBox2f(DetailIcon->GetBrush().GetUVRegion()) : FBox2f();
    return Visible(DetailIcon) && Visible(QualityIcon) && DetailAtlas
        && DetailIcon->GetBrush().GetResourceObject() == LoadObject<UTexture2D>(nullptr, DetailAtlas)
        && FMath::IsNearlyEqual(DetailUv.Min.X, static_cast<float>(Selected->IconRect.Min.X) / Selected->AtlasSize.X, 1e-4f)
        && FMath::IsNearlyEqual(DetailUv.Min.Y, static_cast<float>(Selected->AtlasSize.Y - Selected->IconRect.Max.Y) / Selected->AtlasSize.Y, 1e-4f)
        && FMath::IsNearlyEqual(DetailUv.Max.X, static_cast<float>(Selected->IconRect.Max.X) / Selected->AtlasSize.X, 1e-4f)
        && FMath::IsNearlyEqual(DetailUv.Max.Y, static_cast<float>(Selected->AtlasSize.Y - Selected->IconRect.Min.Y) / Selected->AtlasSize.Y, 1e-4f)
        && QualityIcon->GetBrush().GetResourceObject() == LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.rok_modal_atlas_item0"))
        && TextIs(TEXT("AllianceShopDetailName"), Selected->Name)
        && TextIs(TEXT("AllianceShopDetailIdentity"), FString::Printf(TEXT("物品 ID %d · 配置品质 %d"), Selected->ItemType, Selected->Quality))
        && TextIs(TEXT("AllianceShopDetailBuyConfigPrice"), FString::Printf(TEXT("个人积分购买配置价：%d"), Selected->SellingPrice))
        && TextIs(TEXT("AllianceShopDetailStockConfigPrice"), FString::Printf(TEXT("联盟积分补货配置价：%d"), Selected->StockPrice))
        && TextIs(TEXT("AllianceShopDetailServerBoundary"), TEXT("此处不能查询实际库存、积分余额、可购买数量或购买结果。"));
}

bool URokAllianceShopReadonlyAuditSubsystem::Ready(int32 Phase) const
{
    if (!Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Runtime.IsValid() || !Catalog.IsValid()
        || !Server.IsValid() || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
        || Server->GetSuppressedLocalRequestCount() != 0 || Modal->IsModalOpen()) return false;
    if (Phase == 7 || Phase == 9) return !Screen->IsScreenOpen()
        && Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")));
    const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Screen->IsScreenOpen() || !Mode || !Visible(Mode)
        || !FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0, .01)
        || !Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01)
        || !Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const bool bShop = (Phase >= 1 && Phase <= 4) || Phase == 6;
    if (!RokAllianceShopUiSettledProbe::Sample(Screen.Get(), bShop).bReady) return false;
    if (Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance || Screen->SelectedAlliancePage != 0
        || Screen->bAllianceShopReferenceOpen != bShop || Screen->GetPendingActionIdForAudit() != INDEX_NONE) return false;
    const int32 ExpectedState = Phase == 8 ? 0 : 2;
    if (Screen->AlliancePreviewState != ExpectedState
        || Screen->bAllianceTestFixtureState != (ExpectedState == 2)) return false;
    const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureFrameTitle")));
    if (!Title || Title->GetText().ToString() != (bShop ? TEXT("联盟商店配置参考") : TEXT("联盟"))) return false;
    if (!bShop) return ExpectedState == 2
        ? Visible(Screen->GetWidgetFromName(TEXT("AllianceFeatureHit_7")))
        : !Screen->GetWidgetFromName(TEXT("AllianceFeatureHit_7"));
    const UTextBlock* BoundaryText = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceShopReferenceBoundary")));
    if (!BoundaryText || BoundaryText->GetText().ToString()
        != TEXT("仅显示商品配置定义和配置价；实际库存、积分余额、权限与购买状态均需服务器数据。")
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceShopReferenceBack")))
        || Screen->GetWidgetFromName(TEXT("AllianceShopReferenceUnavailable"))) return false;
    if (Screen->SelectedAllianceShopGroup != (Phase == 6 ? 1 : Phase)) return false;
    if (Phase == 6 && Screen->SelectedAllianceShopItemType
        != Catalog->GetAllianceShopReadonlyRows()[0].ItemType) return false;
    return RowsMatchCurrent();
}

bool URokAllianceShopReadonlyAuditSubsystem::RecordEffect(int32 ExpectedGroup, int32 ExpectedItem, bool bOpen)
{
    if (Effects.Num() == Actions.Num()) return true;
    if (GFrameCounter <= EffectFrame || !Screen.IsValid()
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE) return false;
    const bool bClosed = !Screen->IsScreenOpen();
    if (!bClosed && !RokAllianceShopUiSettledProbe::Sample(Screen.Get(), bOpen).bReady) return false;
    if (bOpen && !RowsMatchCurrent()) return false;
    const bool bOk = Screen->SelectedAllianceShopGroup == ExpectedGroup
        && Screen->SelectedAllianceShopItemType == ExpectedItem
        && (bClosed || Screen->bAllianceShopReferenceOpen == bOpen) && Boundary();
    if (!Expect(bOk, TEXT("Next-tick Shop action effect or immutable business failed"))) return false;
    auto Receipt = MakeShared<FJsonObject>();
    Receipt->SetNumberField(TEXT("action_index"), Actions.Num() - 1);
    Receipt->SetNumberField(TEXT("action_frame"), static_cast<double>(EffectFrame));
    Receipt->SetNumberField(TEXT("effect_frame"), static_cast<double>(GFrameCounter));
    Receipt->SetObjectField(TEXT("state"), State());
    Effects.Add(MakeShared<FJsonValueObject>(Receipt));
    return true;
}

void URokAllianceShopReadonlyAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Alliance Shop read-only route emitted an upgrade completion event"));
}

bool URokAllianceShopReadonlyAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
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
	if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0,
		TEXT("Dedicated virtual Slate user was lost"))) return false;
	const int32 UserIndex = VirtualUser->GetUserIndex();
	const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30, -30));
	const FString ForegroundBefore = ForegroundIdentity();
	const TSharedPtr<SWidget> UserZeroFocusBefore = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
	POINT HardwareCursorBefore = {};
	const bool bCursorReadBefore = ::GetCursorPos(&HardwareCursorBefore) != 0;
#endif
	App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Away, LastPointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()), true);
	App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Center, Away, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()), true);
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, UserIndex);
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
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const bool bOwnedOffscreenWindow = Native.IsValid() && Native->GetOSWindowHandle() == nullptr
		&& Native->IsRenderingOffScreen() && Window->IsVisible()
		&& FNullPlatformApplicationMisc::IsUsingNullApplication();
	const bool bExactButton = ButtonIndex != INDEX_NONE
		&& Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
	Attempt->SetStringField(TEXT("method"), TEXT("dedicated_virtual_user_synthetic_slate_move_down_up"));
	Attempt->SetNumberField(TEXT("virtual_user_index"), UserIndex);
	Attempt->SetNumberField(TEXT("virtual_user_slot"), VirtualUser->GetVirtualUserIndex());
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
	Attempt->SetBoolField(TEXT("null_offscreen_window"), bOwnedOffscreenWindow);
	Attempt->SetStringField(TEXT("foreground_before"), ForegroundBefore);
	Attempt->SetStringField(TEXT("user0_focus_before"), FString::Printf(TEXT("0x%p"),
		static_cast<const void*>(UserZeroFocusBefore.Get())));
	Attempt->SetNumberField(TEXT("stable_frames"), ClickStableFrames);
	const uint64 Before = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetNumberField(TEXT("primary_before"), static_cast<double>(Before));
	Attempt->SetObjectField(TEXT("business_before"), BusinessState());
	Attempt->SetBoolField(TEXT("screen_open_before"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_before"), Modal.IsValid() && Modal->IsModalOpen());
	if (!Path.IsValid() || !bExactButton || !bUmgPath || !Target->IsHovered()
		|| !bOwnWindow || !bOwnedOffscreenWindow)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("Own Null offscreen Slate path did not prove exact UButton"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Exact target button was not hit in own offscreen window"));
	}
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(UserIndex, 0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(UserIndex, 0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const FString ForegroundAfter = ForegroundIdentity();
	const TSharedPtr<SWidget> UserZeroFocusAfter = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
	POINT HardwareCursorAfter = {};
	const bool bCursorReadAfter = ::GetCursorPos(&HardwareCursorAfter) != 0;
	const bool bCursorUnchanged = bCursorReadBefore && bCursorReadAfter
		&& HardwareCursorBefore.x == HardwareCursorAfter.x
		&& HardwareCursorBefore.y == HardwareCursorAfter.y;
#else
	const bool bCursorUnchanged = false;
#endif
	const uint64 After = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetBoolField(TEXT("down_handled"), Down);
	Attempt->SetBoolField(TEXT("up_handled"), Up);
	Attempt->SetBoolField(TEXT("dispatch_return_proves_click"), false);
	Attempt->SetBoolField(TEXT("foreground_unchanged"), ForegroundBefore == ForegroundAfter);
	Attempt->SetBoolField(TEXT("user0_focus_unchanged"), UserZeroFocusBefore == UserZeroFocusAfter);
	Attempt->SetBoolField(TEXT("hardware_cursor_unchanged"), bCursorUnchanged);
	Attempt->SetStringField(TEXT("foreground_after"), ForegroundAfter);
	Attempt->SetBoolField(TEXT("win32_input_or_focus_api_called"), false);
	Attempt->SetNumberField(TEXT("primary_after"), static_cast<double>(After));
	Attempt->SetObjectField(TEXT("business_after"), BusinessState());
	Attempt->SetBoolField(TEXT("screen_open_after"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_after"), Modal.IsValid() && Modal->IsModalOpen());
	const int32 PendingAfter = Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : INDEX_NONE;
	Attempt->SetNumberField(TEXT("pending_action_id_after"), PendingAfter);
	const bool bDeferred = Indexed ? PendingAfter == Indexed->ActionId : PendingAfter == INDEX_NONE;
	const bool bRouteSafe = Up && bDeferred && Before == After
		&& ForegroundBefore == ForegroundAfter && UserZeroFocusBefore == UserZeroFocusAfter
		&& bCursorUnchanged && (!bBaselineSet || Boundary());
	Attempt->SetStringField(TEXT("failure"), bRouteSafe ? TEXT("") : TEXT("unhandled, world input, or business mutation"));
	Actions.Add(MakeShared<FJsonValueObject>(Attempt));
	StableTarget.Reset(); ClickStableFrames = 0;
	return Expect(bRouteSafe, TEXT("Slate click failed or changed baseline state"));
}




void URokAllianceShopReadonlyAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
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

void URokAllianceShopReadonlyAuditSubsystem::ScreenshotProcessed()
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
void URokAllianceShopReadonlyAuditSubsystem::AdvanceCapture()
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
void URokAllianceShopReadonlyAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}


bool URokAllianceShopReadonlyAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_shop_readonly.synthetic_offscreen_runtime_audit.v1"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 10
        && Actions.Num() == 30 && Effects.Num() == 30 && Attempts.IsEmpty() && CompletionEvents == 0 && bBaselineSet && Boundary());
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetStringField(TEXT("scope"), TEXT("CLI local joined fixture and real Slate Shop group/item navigation; later HUD click proves unjoined boundary"));
    Json->SetStringField(TEXT("baseline_phase"), TEXT("00_cli_joined_fixture"));
    Json->SetStringField(TEXT("entry_route"), TEXT("-RokOpenModal=alliance_joined"));
    Json->SetBoolField(TEXT("hud_click_enters_joined_claim"), false);
    Json->SetBoolField(TEXT("alliance_membership_accepted"), false);
    Json->SetBoolField(TEXT("shop_purchase_or_stock_accepted"), false);
    Json->SetBoolField(TEXT("account_alliance_state_accepted"), false);
    Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
    Json->SetBoolField(TEXT("real_os_pointer_or_focus_used"), false);
    Json->SetBoolField(TEXT("visual_parity_accepted"), false);
    Json->SetNumberField(TEXT("shop_row_count"), Catalog.IsValid() ? Catalog->GetAllianceShopReadonlyRows().Num() : -1);
    Json->SetNumberField(TEXT("shop_group_count"), Catalog.IsValid() ? Catalog->GetAllianceShopReadonlyGroups().Num() : -1);
    Json->SetNumberField(TEXT("completion_events"), CompletionEvents);
    Json->SetNumberField(TEXT("typing_events"), 0);
    Json->SetNumberField(TEXT("scroll_events"), 0);
    Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("actions"), Actions);
    Json->SetArrayField(TEXT("action_effects"), Effects);
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


void URokAllianceShopReadonlyAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 10
        && Actions.Num() == 30 && Effects.Num() == 30 && Attempts.IsEmpty() && CompletionEvents == 0 && Boundary();
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Alliance Shop read-only audit incomplete"));
    Save(Complete);
    UE_LOG(LogTemp, Display, TEXT("Rok alliance Shop audit %s: %s"), Complete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok alliance Shop audit: %s"), *Failures.Last());
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}

void URokAllianceShopReadonlyAuditSubsystem::Tick(float DeltaTime)
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
    auto Stable = [this](int32 Phase) { StableFrames = Ready(Phase) ? StableFrames + 1 : 0; return StableFrames >= 2; };
    auto Press = [this](const FString& Name, const TCHAR* Label) {
        if (!Boundary() || !Click(Cast<UButton>(Name == TEXT("AllianceFeatureButton")
            ? Hud->GetWidgetFromName(*Name) : Screen->GetWidgetFromName(*Name)), Label)) return false;
        EffectFrame = GFrameCounter; return true;
    };
    const auto& Rows = Catalog.IsValid() ? Catalog->GetAllianceShopReadonlyRows()
        : TArray<FRokUnityAllianceShopReadonlyRow>();
    auto FirstInGroup = [&Rows](int32 Group) {
        for (const auto& Row : Rows) if (Row.Group == Group) return Row.ItemType;
        return 0;
    };
    switch (Step)
    {
    case 0: if (!bBegun) bBegun = Begin(); if (bBegun) Enter(1); break;
    case 1:
        if (Stable(0))
        {
            FRokLocalConstructionTicket Ticket; int32 PendingCount = -1;
#if WITH_DEV_AUTOMATION_TESTS
            PendingCount = Buildings->GetPendingCompletedUpgradeCountForTesting();
#endif
            if (!Expect(Buildings->GetActiveUpgradeCount() == 0 && PendingCount == 0
                && Buildings->GetActiveLocalConstructionCount() == 0 && !Buildings->GetLocalConstructionTicket(1, Ticket),
                TEXT("Shop audit requires idle local business queues"))) return;
            BaselineBusinessJson = BusinessState(); BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
            BaselineCamera = Controller->GetPawn()->GetActorTransform();
            BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            bBaselineSet = true; Capture(TEXT("00_cli_joined_fixture"), 2);
        } break;
    case 2: if (Press(TEXT("AllianceFeatureHit_7"), TEXT("open_shop_657"))) Enter(3); break;
    case 3:
        if (RecordEffect(1, FirstInGroup(1), true) && Stable(1))
        { GroupCursor = 1; RowCursor = 0; Enter(4); }
        break;
    case 4:
        while (Rows.IsValidIndex(RowCursor) && Rows[RowCursor].Group < GroupCursor) ++RowCursor;
        if (Rows.IsValidIndex(RowCursor) && Rows[RowCursor].Group == GroupCursor)
        {
            if (Press(FString::Printf(TEXT("AllianceShopSelect_%d"), Rows[RowCursor].ItemType), TEXT("select_shop_config_row"))) Enter(41);
        }
        else if (Stable(GroupCursor))
        {
            const FString Phase = FString::Printf(TEXT("0%d_shop_group_%d"), GroupCursor, GroupCursor);
            Capture(*Phase, 5);
        }
        break;
    case 41:
        if (Rows.IsValidIndex(RowCursor) && RecordEffect(GroupCursor, Rows[RowCursor].ItemType, true))
        { ++RowCursor; Enter(4); }
        break;
    case 5:
        if (GroupCursor < 4)
        {
            const int32 Next = GroupCursor + 1;
            if (Press(FString::Printf(TEXT("AllianceShopGroup_%d"), Next), TEXT("switch_shop_group"))) Enter(51);
        }
        else Enter(6);
        break;
    case 51:
        if (RecordEffect(GroupCursor + 1, FirstInGroup(GroupCursor + 1), true))
        { ++GroupCursor; Enter(4); }
        break;
    case 6: if (Press(TEXT("AllianceShopReferenceBack"), TEXT("back_joined"))) Enter(7); break;
    case 7: if (RecordEffect(4, Rows.Last().ItemType, false) && Stable(5)) Capture(TEXT("05_joined_return"), 8); break;
    case 8: if (Press(TEXT("AllianceFeatureHit_7"), TEXT("reopen_shop_657"))) Enter(9); break;
    case 9: if (RecordEffect(1, FirstInGroup(1), true) && Stable(6)) Capture(TEXT("06_shop_reopened_group_1"), 10); break;
    case 10: if (Press(TEXT("FeatureFrameClose"), TEXT("close_to_hud"))) Enter(11); break;
    case 11: if (RecordEffect(1, FirstInGroup(1), false) && Stable(7)) Capture(TEXT("07_hud_closed"), 12); break;
    case 12: if (Press(TEXT("AllianceFeatureButton"), TEXT("hud_alliance_unjoined"))) Enter(13); break;
    case 13: if (RecordEffect(1, 0, false) && Stable(8)) Capture(TEXT("08_hud_unjoined_boundary"), 14); break;
    case 14: if (Press(TEXT("FeatureFrameClose"), TEXT("final_close_hud"))) Enter(15); break;
    case 15: if (RecordEffect(1, 0, false) && Stable(9)) Capture(TEXT("09_final_hud"), 16); break;
    case 16: Finish(); break;
    default: Finish(TEXT("Invalid Shop audit state")); break;
    }
}
