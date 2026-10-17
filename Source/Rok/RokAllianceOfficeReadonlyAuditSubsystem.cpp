#include "RokAllianceOfficeReadonlyAuditSubsystem.h"

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
struct FRokAllianceOfficeUiSettledSample
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

namespace RokAllianceOfficeUiSettledProbe
{

inline FRokAllianceOfficeUiSettledSample Sample(const URokUnityFeatureScreenWidget* Screen, bool bReference)
{
    FRokAllianceOfficeUiSettledSample Result;
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
        && (bReference ? Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceBack")) != nullptr
            : Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceOpen")) != nullptr);
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
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceOfficeReadonlyAudit");

struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Exact source, Alliance Office product and runtime boundaries are immutable inputs. Audit/verifier
// files are also hashed at runtime and compared against the staged release report.
constexpr FInputPin Pinned[] = {
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.uasset"), TEXT("cebb1dcd37be20da472c0982d6b45143315961204ad382a0eb6d4f55b6c92987")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"), TEXT("a3f6245b2c64f552e7b8a253e98f637d6ca0292a4aab87e5fd9183f03c76309f")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset"), TEXT("eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e")},
	{TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.uasset"), TEXT("b739a555477da17858f2ad1f97824b3191017f3cdb3703b6de19337d0622b93b")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_guild/UIGuildMain/UI_Win_GuildAppoint.prefab"), TEXT("1b94c664c287627b6d6d89b812ddc8fd0f7e5cd07011d9e15b35abcc2231afe3")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/ui_alliance.tpsheet"), TEXT("a40593228f958e94763568a65b30e4e539d5c141b280727b74cd5d74c2da446b")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/AllianceProxy.cs"), TEXT("67c0dea2cb390479f764a902ef4c903f8fd14ca8898c43662d61bc6422d66dbc")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildAppointMediator.cs"), TEXT("5a2b04523af0ca11a0c25fcdc589cd0259fdcea514d7ada3852253585a0990ef")},
	{TEXT("ref/config-tables/编译客户端配置/CSV/AllianceOfficially.csv"), TEXT("62f0e3f339aedd8ed255ae84b8473cf3012ed1a42aa52bf733afb99ccabcfb58")},
	{TEXT("Saved/PendingActivityCalendarReadonlyCatalogV2/ready_for_root_review.json"), TEXT("1b946f5c241cdc180cada5a07154248d101e71de7252bb2b8db9cb3a38aaf2e9")},
	{TEXT("Saved/PendingActivityCalendarReadonlyCatalogV3/ready_for_root_review.json"), TEXT("fc9626bc6a4db0881e544ce464440017ac995ad076f73e232c73deded486ef1c")},
	{TEXT("Saved/PendingActivityCalendarReadonlyRuntimeAuditV7/ready_for_root_review.json"), TEXT("cb8531e0b89eae7af529649c581df49e49c20e72137e004aff9249c615129047")},
	{TEXT("Saved/PendingAllianceOfficeReadonlyCatalogV3/contract.json"), TEXT("bcb74e303702b072543f65963c96d65d895b8fa720a0b92540682a1c9b630863")},
	{TEXT("Saved/PendingAllianceOfficeReadonlyCatalogV3/ready_for_root_review.json"), TEXT("0c37ac2d476b5faa83c5269ae94d91d2cfdb355edd2ad81f2e7e1d497d62c70d")},
	{TEXT("Saved/PendingAllianceOfficeReadonlyCatalogV3/source_trace.json"), TEXT("03d347bf5a6220974f9f2e3027815af137fafd56f7ab39b9b515f342139f5339")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV4/ready_for_root_review.json"), TEXT("914c17bb8088ba29f7333491d60861eeed1499283e2906f44be77f66b0bd64e8")},
	{TEXT("Saved/PendingCommanderSkillReadonlyRuntimeAuditV5/ready_for_root_review.json"), TEXT("b6c0bbab0a41532225cc5c12a9192270cc4ad43e451cc69329a6b745f33d57f7")},
	{TEXT("Saved/PendingDailyTaskReadonlyCatalogV3/ready_for_root_review.json"), TEXT("95ca2fc33eb008cdbc081352accce559465456414d2444dcdcd3427f54196f5e")},
	{TEXT("Saved/PendingDailyTaskReadonlyRuntimeAuditV2/ready_for_root_review.json"), TEXT("8c70d51e0dd30460abcb51a4628a4623cd98c0f16f4d57a23602cd805749305d")},
	{TEXT("Saved/PendingVipDailyPointHelpTipRuntimeAuditV2/ready_for_root_review.json"), TEXT("8604d0add1b8733644b69578a0cedb633644afa89647d00e1ee98c73b2f782f3")},
	{TEXT("Saved/PendingVipDailyPointHelpTipV3/ready_for_root_review.json"), TEXT("a124c5b47671f4b50edddbc91c1df4066db85105afb3fee4a6123f5076e9b17a")},
	{TEXT("Saved/RokUnityMigration/ConfigTables/AllianceOfficially.json"), TEXT("e4df13a417573b58254320ea27fa16585c8b6a1ec3784ca50fa63dfc31a95421")},
	{TEXT("Saved/RokUnityMigration/ConfigTables/Language.json"), TEXT("e228eca8deb7c8daa4a2d910f736c57567db11322d558544309bfbaca412277d")},
	{TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json"), TEXT("15115118db3366dbdee6f97b705dfaed11da73768694788e2abd7dd1b7a9bd6e")},
	{TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json"), TEXT("07fcf9600d8e79afba470ef92c71460dd217e82e22dfaafc236a4fd58b5b0994")},
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
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("962143edafef9ac0a524d91212dd12b82eec277fe1391b512b924693a70366b7")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("2dd522f79dce94b9c734aa86cfbb1e20bc0dd9f96fe28bb5a4966002fa221d43")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.cpp"), TEXT("6c27feacc66b48d75287d55396926afd1691832d7ebd5535cedd36491375d4b7")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.h"), TEXT("392f8d13e4ce1dd338065082ef979d8ab0fb456de6a7c6993a6b197c19799a4c")},
	{TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9c2fca7d169a40b8a5a10ba4ef036b07a6fc76719871f35451c6f49be800171b")},
	{TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("50cec5ddbdba9db12730edad021b8f359477d2b596a6504bc554dfda8ac220d3")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"), TEXT("3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"), TEXT("e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112")},
	{TEXT("Rok.uproject"), TEXT("742d31bf5aae3e77cb6f681a91b02502a7be79c39cee176a8020fb21360283a8")},
	{TEXT("Saved/PendingAllianceOfficeRowLayoutPolishV3/ready_for_root_review.json"), TEXT("d027b1c3e09109b343ad1df5fa879e6fac691357052b87e65c2ed51e40f2038c")},
	{TEXT("Saved/PendingAllianceOfficeRowLayoutPolishV3/implementation_report.json"), TEXT("7f1262b52784c14241d923b500579afed4394944b3e796ca1f1e04fad61525ba")},
	{TEXT("Saved/PendingTaskCompletedRowAnimationV2/ready_for_root_review.json"), TEXT("975fdf206ffc11c47cfa8b2dae38f301eec00b699f75abb61fc5c90ae9ffe622")},
	{TEXT("Saved/PendingTaskCompletedRowAnimationRuntimeAuditV4/ready_for_root_review.json"), TEXT("83deb074a935e7a1c4e0d314c83c0b72c0f7e585a9a769251e0ff9a37f7a8cbd")},
	{TEXT("Saved/PendingAllianceTechnologyReadonlyCatalogV4/ready_for_root_review.json"), TEXT("11ce1e2b4442c9ab2d3aa5ba19535de98b7b39559694fcd3cff9753c9ae0aa9d")},
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
	Json->SetObjectField(TEXT("desired_size"), Point(Widget ? Widget->GetDesiredSize() : FVector2D::ZeroVector));
	Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	if (const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
	{
		auto Canvas = MakeShared<FJsonObject>();
		Canvas->SetObjectField(TEXT("position"), Point(Slot->GetPosition()));
		Canvas->SetObjectField(TEXT("size"), Point(Slot->GetSize()));
		Json->SetObjectField(TEXT("canvas_slot"), Canvas);
	}
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Json->SetStringField(TEXT("text"), Text->GetText().ToString());
	return Json;
}
bool ContainsRect(const UWidget* Outer, const UWidget* Inner, float Tolerance = 1.0f)
{
	if (!Outer || !Inner) return false;
	const FGeometry O = Outer->GetCachedGeometry(), I = Inner->GetCachedGeometry();
	const FVector2D OA = O.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D OB = O.LocalToAbsolute(O.GetLocalSize());
	const FVector2D IA = I.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D IB = I.LocalToAbsolute(I.GetLocalSize());
	return O.GetLocalSize().X > 0 && O.GetLocalSize().Y > 0
		&& I.GetLocalSize().X > 0 && I.GetLocalSize().Y > 0
		&& IA.X >= OA.X - Tolerance && IA.Y >= OA.Y - Tolerance
		&& IB.X <= OB.X + Tolerance && IB.Y <= OB.Y + Tolerance;
}
bool OfficeReferenceLayoutValid(const URokUnityFeatureScreenWidget* Screen, const TArray<int32>& Ids)
{
	if (!Screen || Ids.Num() != 4) return false;
	const UWidget* Back = Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceBack"));
	const UCanvasPanelSlot* BackSlot = Back ? Cast<UCanvasPanelSlot>(Back->Slot) : nullptr;
	if (!BackSlot || BackSlot->GetSize().X <= 0 || BackSlot->GetSize().Y <= 0) return false;
	const FGeometry BG = Back->GetCachedGeometry();
	const double BackTop = BG.LocalToAbsolute(FVector2D::ZeroVector).Y;
	double PreviousBottom = -TNumericLimits<double>::Max();
	for (const int32 Id : Ids)
	{
		const UWidget* Row = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceRow_%d"), Id));
		const UWidget* Hit = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceHit_%d"), Id));
		const UTextBlock* Bonus = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceBonus_%d"), Id)));
		const UCanvasPanelSlot* RowSlot = Row ? Cast<UCanvasPanelSlot>(Row->Slot) : nullptr;
		const UCanvasPanelSlot* HitSlot = Hit ? Cast<UCanvasPanelSlot>(Hit->Slot) : nullptr;
		const UCanvasPanelSlot* BonusSlot = Bonus ? Cast<UCanvasPanelSlot>(Bonus->Slot) : nullptr;
		if (!RowSlot || !HitSlot || !BonusSlot || !Visible(Row) || !Visible(Hit) || !Visible(Bonus)
			|| RowSlot->GetSize().X <= 0 || RowSlot->GetSize().Y <= 0
			|| !RowSlot->GetPosition().Equals(HitSlot->GetPosition(), .25f)
			|| !RowSlot->GetSize().Equals(HitSlot->GetSize(), .25f)
			|| !ContainsRect(Row, Hit) || !ContainsRect(Hit, Row)
			|| !ContainsRect(Row, Bonus) || !ContainsRect(Hit, Bonus)
			|| Bonus->GetDesiredSize().Y <= 0 || Bonus->GetDesiredSize().Y > BonusSlot->GetSize().Y + 1.0f
			|| Bonus->GetDesiredSize().X > BonusSlot->GetSize().X + 1.0f) return false;
		const FGeometry RG = Row->GetCachedGeometry();
		const double Top = RG.LocalToAbsolute(FVector2D::ZeroVector).Y;
		const double Bottom = RG.LocalToAbsolute(RG.GetLocalSize()).Y;
		if (Top < PreviousBottom + 2.0 || Bottom <= Top || Bottom + 8.0 > BackTop) return false;
		PreviousBottom = Bottom;
	}
	return true;
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

bool URokAllianceOfficeReadonlyAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false; // Pending-completion count is only exposed to audited Editor builds.
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokAllianceOfficeReadonlyAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceOfficeReadonlyAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokAllianceOfficeReadonlyAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceOfficeReadonlyAuditSubsystem, STATGROUP_Tickables);
}
void URokAllianceOfficeReadonlyAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("PendingAllianceOfficeRowLayoutPolishRuntimeAuditV4/RuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokAllianceOfficeReadonlyAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceOfficeReadonlyAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokAllianceOfficeReadonlyAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokAllianceOfficeReadonlyAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokAllianceOfficeReadonlyAuditSubsystem::PrepareOwnWindowFocus()
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


bool URokAllianceOfficeReadonlyAuditSubsystem::Begin()
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
    if (!Expect(Catalog->LoadAllianceOfficeReadonly(), TEXT("Alliance office static config unavailable"))) return false;
    const auto& Rows = Catalog->GetAllianceOfficeReadonlyRows();
    for (const auto& Row : Rows) OfficeIds.Add(Row.Id);
    if (!Expect(OfficeIds.Num() == 4 && OfficeIds[0] == 1001 && OfficeIds[1] == 2001
        && OfficeIds[2] == 3001 && OfficeIds[3] == 4001,
        TEXT("Expected four source alliance office IDs"))) return false;
    for (const TCHAR* Asset : {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance")})
        if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr,
            TEXT("Required Alliance texture failed to load"))) return false;
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
    for (const TCHAR* Path : {TEXT("Source/Rok/RokAllianceOfficeReadonlyAuditSubsystem.h"),
        TEXT("Source/Rok/RokAllianceOfficeReadonlyAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokAllianceOfficeReadonlyRuntime.py")})
    {
        const FString Actual = FileSha(RokDataPaths::ResolveProjectPath(Path));
        if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
        Inputs->SetStringField(Path, Actual);
    }
    CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
        this, &URokAllianceOfficeReadonlyAuditSubsystem::OnUpgradeCompleted);
    return true;
}

TSharedRef<FJsonObject> URokAllianceOfficeReadonlyAuditSubsystem::BusinessState() const
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

bool URokAllianceOfficeReadonlyAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}

TSharedRef<FJsonObject> URokAllianceOfficeReadonlyAuditSubsystem::State() const
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
    const bool bReference = Screen.IsValid() && Screen->bAllianceOfficeReferenceOpen;
    const FRokAllianceOfficeUiSettledSample Settled = RokAllianceOfficeUiSettledProbe::Sample(Screen.Get(), bReference);
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
        TEXT("AllianceOfficeReferenceOpen"), TEXT("AllianceOfficeReferenceIntro"),
        TEXT("AllianceOfficeReferenceBack"), TEXT("AllianceOfficeReferenceUnavailable")})
        Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
    Json->SetObjectField(TEXT("widgets"), Widgets);
    Json->SetNumberField(TEXT("office_count"), OfficeIds.Num());
    Json->SetNumberField(TEXT("active_mode"), Screen.IsValid() ? static_cast<int32>(Screen->ActiveMode) : -1);
    Json->SetNumberField(TEXT("selected_office_index"), Screen.IsValid() ? Screen->SelectedAllianceOfficeIndex : -1);
    Json->SetNumberField(TEXT("alliance_preview_state"), Screen.IsValid() ? Screen->AlliancePreviewState : -1);
    Json->SetNumberField(TEXT("selected_alliance_page"), Screen.IsValid() ? Screen->SelectedAlliancePage : -1);
    Json->SetBoolField(TEXT("office_reference_open"), Screen.IsValid() && Screen->bAllianceOfficeReferenceOpen);
    Json->SetNumberField(TEXT("pending_action_id"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2);
    if (Screen.IsValid() && Screen->bAllianceOfficeReferenceOpen && Catalog.IsValid())
    {
        TArray<TSharedPtr<FJsonValue>> AllRows;
        for (const auto& Row : Catalog->GetAllianceOfficeReadonlyRows())
        {
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("id"), Row.Id);
            Entry->SetNumberField(TEXT("name_locale_id"), Row.NameLocaleId);
            Entry->SetNumberField(TEXT("description_locale_id"), Row.DescriptionLocaleId);
            Entry->SetNumberField(TEXT("bonus_locale_id"), Row.AddDescriptionLocaleId);
            Entry->SetStringField(TEXT("source_name"), Row.Name);
            Entry->SetStringField(TEXT("source_description"), Row.Description);
            Entry->SetStringField(TEXT("source_bonus"), Row.AddDescription);
            Entry->SetStringField(TEXT("source_icon"), Row.IconReference);
            Entry->SetStringField(TEXT("icon_sprite"), Row.IconSprite);
            for (const TCHAR* Kind : {TEXT("Row"), TEXT("Name"), TEXT("Description"), TEXT("Bonus"), TEXT("Icon"), TEXT("Hit")})
            {
                const FString Name = FString::Printf(TEXT("AllianceOfficeReference%s_%d"), Kind, Row.Id);
                Entry->SetObjectField(Kind, WidgetState(Screen->GetWidgetFromName(*Name)));
            }
            AllRows.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Json->SetArrayField(TEXT("office_rows"), AllRows);
    }
    return Json;
}


bool URokAllianceOfficeReadonlyAuditSubsystem::Ready(int32 Phase) const
{
    if (!Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Runtime.IsValid() || !Catalog.IsValid()
        || !Server.IsValid() || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
        || Server->GetSuppressedLocalRequestCount() != 0 || Modal->IsModalOpen()) return false;
    if (Phase == 0 || Phase == 7 || Phase == 11)
        return !Screen->IsScreenOpen() && Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")));
    const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Screen->IsScreenOpen() || !Mode || !Visible(Mode)
        || !FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0, .01)
        || !Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01)
        || !Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const bool bReference = (Phase >= 2 && Phase <= 5) || Phase == 9 || Phase == 10;
    if (!RokAllianceOfficeUiSettledProbe::Sample(Screen.Get(), bReference).bReady) return false;
    if (Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance
        || Screen->AlliancePreviewState != 0 || Screen->SelectedAlliancePage != 0
        || Screen->bAllianceOfficeReferenceOpen != bReference
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE) return false;
    const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureFrameTitle")));
    if (!Title || Title->GetText().ToString() != (bReference ? TEXT("官职配置参考") : TEXT("联盟"))) return false;
    if (!bReference) return Visible(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceOpen")))
        && !Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceBack"));
    const UTextBlock* Intro = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceIntro")));
    if (!Intro || Intro->GetText().ToString() != TEXT("仅展示配置定义，不代表当前任命、人数、权限或已生效加成；选择不会改变账号状态。")
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceBack")))
        || Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceUnavailable"))
        || OfficeIds.Num() != 4 || OfficeIds[0] != 1001 || OfficeIds[1] != 2001
        || OfficeIds[2] != 3001 || OfficeIds[3] != 4001) return false;
    const auto& Rows = Catalog->GetAllianceOfficeReadonlyRows();
    if (Rows.Num() != 4) return false;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const auto& Row = Rows[Index];
        if (Row.Id != OfficeIds[Index] || Row.Name.IsEmpty() || Row.Description.IsEmpty()
            || Row.AddDescription.IsEmpty() || Row.Name.Contains(TEXT("\uFFFD"))
            || Row.Description.Contains(TEXT("\uFFFD")) || Row.AddDescription.Contains(TEXT("\uFFFD"))) return false;
        const UTextBlock* Name = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceName_%d"), Row.Id)));
        const UTextBlock* Description = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceDescription_%d"), Row.Id)));
        const UTextBlock* Bonus = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceBonus_%d"), Row.Id)));
        const UWidget* Icon = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceIcon_%d"), Row.Id));
        const UWidget* Hit = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceHit_%d"), Row.Id));
        const UWidget* Card = Screen->GetWidgetFromName(*FString::Printf(TEXT("AllianceOfficeReferenceRow_%d"), Row.Id));
        if (!Name || Name->GetText().ToString() != FString::Printf(TEXT("#%d · %s"), Row.Id, *Row.Name)
            || !Description || Description->GetText().ToString() != Row.Description
            || !Bonus || Bonus->GetText().ToString() != FString::Printf(TEXT("配置加成说明：%s"), *Row.AddDescription)
            || !Icon || !Visible(Icon) || !Hit || !Visible(Hit) || !Card || !Visible(Card)) return false;
    }
    const int32 ExpectedIndex = Phase == 2 ? 0 : Phase == 3 ? 1 : Phase == 4 ? 2 : Phase == 5 ? 3 : Phase == 9 ? 0 : 0;
    return Screen->SelectedAllianceOfficeIndex == ExpectedIndex
        && OfficeReferenceLayoutValid(Screen.Get(), OfficeIds);
}

void URokAllianceOfficeReadonlyAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Alliance office read-only route emitted an upgrade completion event"));
}

bool URokAllianceOfficeReadonlyAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
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



void URokAllianceOfficeReadonlyAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
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
void URokAllianceOfficeReadonlyAuditSubsystem::ScreenshotProcessed()
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
void URokAllianceOfficeReadonlyAuditSubsystem::AdvanceCapture()
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
void URokAllianceOfficeReadonlyAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}

bool URokAllianceOfficeReadonlyAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_office_row_layout_polish.runtime_audit.v4"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 12
        && Actions.Num() == 11 && Attempts.IsEmpty() && CompletionEvents == 0 && bBaselineSet && Boundary());
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetStringField(TEXT("scope"), TEXT("Actual HUD Alliance welcome to four-definition reference and measured row layout, close/reopen; anonymous config only"));
    Json->SetStringField(TEXT("baseline_phase"), TEXT("00_city_hud"));
    Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
    Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
    Json->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
    Json->SetBoolField(TEXT("alliance_membership_accepted"), false);
    Json->SetBoolField(TEXT("office_appointment_or_bonus_accepted"), false);
    Json->SetBoolField(TEXT("account_alliance_state_accepted"), false);
    Json->SetNumberField(TEXT("office_count"), OfficeIds.Num());
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


void URokAllianceOfficeReadonlyAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 12
        && Actions.Num() == 11 && Attempts.IsEmpty() && CompletionEvents == 0 && Boundary();
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Alliance office read-only audit incomplete"));
    Save(Complete);
    UE_LOG(LogTemp, Display, TEXT("Rok alliance office audit %s: %s"),
        Complete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok alliance office audit: %s"), *Failures.Last());
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}


void URokAllianceOfficeReadonlyAuditSubsystem::Tick(float DeltaTime)
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
                TEXT("Alliance audit requires idle local business queues"))) return;
            BaselineBusinessJson = BusinessState();
            BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
            BaselineCamera = Controller->GetPawn()->GetActorTransform();
            BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            bBaselineSet = true;
            Capture(TEXT("00_city_hud"), 2);
        }
        break;
    case 2: if (Boundary() && Click(Cast<UButton>(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton"))), TEXT("open_alliance_from_hud"))) Enter(3); break;
    case 3: if (Boundary() && Stable(1)) Capture(TEXT("01_alliance_welcome"), 4); break;
    case 4: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceOpen"))), TEXT("open_office_reference"))) Enter(5); break;
    case 5: if (Boundary() && Stable(2)) Capture(TEXT("02_office_reference"), 6); break;
    case 6: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceHit_2001"))), TEXT("select_office_2001"))) Enter(7); break;
    case 7: if (Boundary() && Stable(3)) Capture(TEXT("03_office_2001"), 8); break;
    case 8: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceHit_3001"))), TEXT("select_office_3001"))) Enter(9); break;
    case 9: if (Boundary() && Stable(4)) Capture(TEXT("04_office_3001"), 10); break;
    case 10: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceHit_4001"))), TEXT("select_office_4001"))) Enter(11); break;
    case 11: if (Boundary() && Stable(5)) Capture(TEXT("05_office_4001"), 12); break;
    case 12: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceBack"))), TEXT("back_to_alliance_welcome"))) Enter(13); break;
    case 13: if (Boundary() && Stable(6)) Capture(TEXT("06_welcome_return"), 14); break;
    case 14: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))), TEXT("close_alliance_to_hud"))) Enter(15); break;
    case 15: if (Boundary() && Stable(7)) Capture(TEXT("07_hud_return"), 16); break;
    case 16: if (Boundary() && Click(Cast<UButton>(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton"))), TEXT("reopen_alliance_from_hud"))) Enter(17); break;
    case 17: if (Boundary() && Stable(8)) Capture(TEXT("08_alliance_reopened"), 18); break;
    case 18: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceOpen"))), TEXT("reopen_office_reference"))) Enter(19); break;
    case 19: if (Boundary() && Stable(9)) Capture(TEXT("09_office_reopened"), 20); break;
    case 20: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceHit_1001"))), TEXT("select_office_1001_after_reopen"))) Enter(21); break;
    case 21: if (Boundary() && Stable(10)) Capture(TEXT("10_office_1001_after_reopen"), 22); break;
    case 22: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))), TEXT("final_close_alliance_to_hud"))) Enter(23); break;
    case 23: if (Boundary() && Stable(11)) Capture(TEXT("11_final_hud"), 24); break;
    case 24: Finish(); break;
    default: Finish(TEXT("Invalid audit state")); break;
    }
}
