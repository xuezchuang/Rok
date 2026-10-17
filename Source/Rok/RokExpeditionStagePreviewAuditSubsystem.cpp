#include "RokExpeditionStagePreviewAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokExpeditionStagePreviewData.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "UObject/UObjectGlobals.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokExpeditionStagePreviewAudit");
constexpr TCHAR ArtAuditFlag[] = TEXT("-RokExpeditionArtAudit");
constexpr TCHAR BattleAtlas[] = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_battle_atlas.rok_expedition_battle_atlas");
constexpr TCHAR TaskPanelAtlas[] = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.rok_expedition_task_panel");
constexpr TCHAR ResultAtlas[] = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.rok_expedition_result_atlas");

struct FArtWarmSpec
{
	const TCHAR* ObjectPath;
	const TCHAR* UassetPath;
	int32 Width;
	int32 Height;
};
constexpr FArtWarmSpec ArtWarmSpecs[] = {
	{BattleAtlas, TEXT("Content/RokPrototype/UI/ExpeditionSource/rok_expedition_battle_atlas.uasset"), 2048, 256},
	{TaskPanelAtlas, TEXT("Content/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.uasset"), 676, 761},
	{ResultAtlas, TEXT("Content/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.uasset"), 512, 512}};

bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}

bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	return true;
}

bool FeatureModeReady(const UWidget* Widget)
{
	if (!Widget || Widget->GetVisibility() != ESlateVisibility::SelfHitTestInvisible
		|| !Visible(Widget) || !FMath::IsNearlyEqual(Widget->GetRenderOpacity(), 1.0, 0.01))
		return false;
	const FVector2D Scale = Widget->GetRenderTransform().Scale;
	return FMath::IsNearlyEqual(Scale.X, 1.0, 0.01)
		&& FMath::IsNearlyEqual(Scale.Y, 1.0, 0.01);
}

const TCHAR* SlateVisibilityName(ESlateVisibility Value)
{
	switch (Value)
	{
	case ESlateVisibility::Visible: return TEXT("Visible");
	case ESlateVisibility::Collapsed: return TEXT("Collapsed");
	case ESlateVisibility::Hidden: return TEXT("Hidden");
	case ESlateVisibility::HitTestInvisible: return TEXT("HitTestInvisible");
	case ESlateVisibility::SelfHitTestInvisible: return TEXT("SelfHitTestInvisible");
	default: return TEXT("Unknown");
	}
}

TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}

FString FileSha(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}

FString SerializeAuditJsonObject(const TSharedRef<FJsonObject>& Object)
{
	FString Result;
	FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Result));
	return Result;
}

TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("exists"), Widget != nullptr);
	Json->SetBoolField(TEXT("visible"), Visible(Widget));
	Json->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	Json->SetNumberField(TEXT("render_opacity"), Widget ? Widget->GetRenderOpacity() : 0.0);
	Json->SetObjectField(TEXT("render_translation"), Point(Widget
		? Widget->GetRenderTransform().Translation : FVector2D::ZeroVector));
	Json->SetObjectField(TEXT("render_scale"), Point(Widget
		? Widget->GetRenderTransform().Scale : FVector2D::ZeroVector));
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Size)));
	Json->SetObjectField(TEXT("local_size"), Point(Size));
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		Json->SetStringField(TEXT("text"), Text->GetText().ToString());
	return Json;
}

TSharedRef<FJsonObject> ArtWidgetState(const UWidget* Widget)
{
	auto Json = WidgetState(Widget);
	Json->SetStringField(TEXT("slate_visibility"), Widget
		? SlateVisibilityName(Widget->GetVisibility()) : TEXT("Absent"));
	Json->SetStringField(TEXT("parent_widget_name"), Widget && Widget->GetParent()
		? Widget->GetParent()->GetName() : FString());
	if (const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
	{
		Json->SetObjectField(TEXT("canvas_position"), Point(Slot->GetPosition()));
		Json->SetObjectField(TEXT("canvas_size"), Point(Slot->GetSize()));
		Json->SetNumberField(TEXT("canvas_z"), Slot->GetZOrder());
	}
	const UImage* Image = Cast<UImage>(Widget);
	Json->SetBoolField(TEXT("is_image"), Image != nullptr);
	if (!Image) return Json;
	const FSlateBrush& Brush = Image->GetBrush();
	const UObject* Resource = Brush.GetResourceObject();
	const UTexture2D* Texture = Cast<UTexture2D>(Resource);
	const FBox2f UV = Brush.GetUVRegion();
	Json->SetStringField(TEXT("brush_resource_path"), Resource ? Resource->GetPathName() : FString());
	Json->SetObjectField(TEXT("brush_image_size"), Point(FVector2D(Brush.GetImageSize())));
	Json->SetObjectField(TEXT("brush_uv_min"), Point(FVector2D(UV.Min.X, UV.Min.Y)));
	Json->SetObjectField(TEXT("brush_uv_max"), Point(FVector2D(UV.Max.X, UV.Max.Y)));
	Json->SetObjectField(TEXT("texture_size"), Point(Texture
		? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D::ZeroVector));
	return Json;
}

bool SourceImageReady(const UWidget* Widget, const TCHAR* ExpectedPath,
	int32 ExpectedWidth, int32 ExpectedHeight, bool bRequireVisible)
{
	const UImage* Image = Cast<UImage>(Widget);
	const UTexture2D* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
	if (!Texture || Texture->GetPathName() != ExpectedPath
		|| Texture->GetSizeX() != ExpectedWidth || Texture->GetSizeY() != ExpectedHeight
		|| (bRequireVisible && !Visible(Image))) return false;
	const FBox2f UV = Image->GetBrush().GetUVRegion();
	return UV.Min.X >= 0.0f && UV.Min.Y >= 0.0f
		&& UV.Max.X <= 1.0f && UV.Max.Y <= 1.0f
		&& UV.Min.X < UV.Max.X && UV.Min.Y < UV.Max.Y;
}
}

bool URokExpeditionStagePreviewAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const bool bPreviewFlag = FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionStagePreviewAudit"));
	const bool bArtFlag = FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionArtAudit"));
	return TargetWorld(Cast<UWorld>(Outer))
		&& (bPreviewFlag != bArtFlag)
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokExpeditionStagePreviewAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}

TStatId URokExpeditionStagePreviewAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokExpeditionStagePreviewAuditSubsystem, STATGROUP_Tickables);
}

void URokExpeditionStagePreviewAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bArtAudit = FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionArtAudit"));
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ (bArtAudit
			? TEXT("RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/ExpeditionArtAudit")
			: TEXT("RokUnityMigration/BatchReview20261009NightTimeAndExpedition/ExpeditionStagePreviewAudit"))
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokExpeditionStagePreviewAuditSubsystem::ScreenshotProcessed);
}

void URokExpeditionStagePreviewAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}

bool URokExpeditionStagePreviewAuditSubsystem::Identity() const
{
	return TargetWorld(GetWorld());
}

bool URokExpeditionStagePreviewAuditSubsystem::Expect(bool Condition, const TCHAR* Error)
{
	if (!Condition) Finish(Error);
	return Condition;
}

TSharedRef<FJsonObject> URokExpeditionStagePreviewAuditSubsystem::BusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Runtime.IsValid() || !Buildings.IsValid() || !Server.IsValid()) return Json;
	const auto Task = Runtime->GetActiveTask();
	const auto Profile = Runtime->GetPlayerProfileState();
	const auto Wallet = Buildings->GetCurrentResources();
	Json->SetNumberField(TEXT("task_id"), Task.TaskId);
	Json->SetNumberField(TEXT("task_current"), Task.Current);
	Json->SetNumberField(TEXT("task_target"), Task.Target);
	Json->SetBoolField(TEXT("task_complete"), Task.bComplete);
	Json->SetNumberField(TEXT("task_next_id"), Task.NextId);
	Json->SetNumberField(TEXT("task_reward_group"), Task.Reward);
	Json->SetNumberField(TEXT("task_chain_count"), Runtime->GetActiveTaskChain().Num());
	Json->SetNumberField(TEXT("wallet_food"), Wallet.Food);
	Json->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
	Json->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
	Json->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
	Json->SetNumberField(TEXT("active_upgrade_count"), Buildings->GetActiveUpgradeCount());
	Json->SetNumberField(TEXT("active_construction_count"), Buildings->GetActiveLocalConstructionCount());
	Json->SetStringField(TEXT("profile_kind"), Profile.StateKind);
	Json->SetStringField(TEXT("role_id"), Profile.RoleId);
	Json->SetNumberField(TEXT("civilization_id"), Profile.CivilizationId);
	Json->SetNumberField(TEXT("http_dispatch_attempts"),
		static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("http_suppressed_requests"),
		static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	TArray<TSharedPtr<FJsonValue>> Heroes;
	for (const auto& Hero : Runtime->GetStarterHeroes())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Hero.HeroId);
		Entry->SetNumberField(TEXT("level"), Hero.Level);
		Entry->SetNumberField(TEXT("star"), Hero.Star);
		Entry->SetNumberField(TEXT("score"), Hero.Score);
		Heroes.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_heroes"), Heroes);
	TArray<TSharedPtr<FJsonValue>> Troops;
	for (const auto& Troop : Runtime->GetStarterTroopStacks())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Troop.TroopId);
		Entry->SetNumberField(TEXT("level"), Troop.Level);
		Entry->SetNumberField(TEXT("count"), Troop.Count);
		Entry->SetNumberField(TEXT("power"), Troop.TotalPower);
		Troops.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_troops"), Troops);
	return Json;
}

bool URokExpeditionStagePreviewAuditSubsystem::ValidateBusinessBoundary() const
{
	return Identity() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
		&& Controller.IsValid()
		&& Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == BaselinePrimaryPressCount
		&& !BaselineBusinessState.IsEmpty() && SerializeAuditJsonObject(BusinessState()) == BaselineBusinessState;
}

TSharedRef<FJsonObject> URokExpeditionStagePreviewAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("business"), BusinessState());
	Json->SetNumberField(TEXT("controller_primary_press_count"), Controller.IsValid()
		? static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount) : -1.0);
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	if (!Screen.IsValid()) return Json;
#if !UE_BUILD_SHIPPING
	Json->SetBoolField(TEXT("expedition_mode"), Screen->IsExpeditionModeForAudit());
	Json->SetNumberField(TEXT("selected_stage_id"), Screen->GetSelectedExpeditionStageIdForAudit());
	Json->SetBoolField(TEXT("preview_mode"), Screen->IsExpeditionPreviewForAudit());
	if (bArtAudit)
		Json->SetBoolField(TEXT("task_config_visible"), Screen->IsExpeditionTaskConfigVisibleForAudit());
	Json->SetStringField(TEXT("animation_phase"), Screen->GetExpeditionAnimationPhaseForAudit().ToString());
	Json->SetNumberField(TEXT("animation_elapsed"), Screen->GetExpeditionAnimationElapsedForAudit());
	Json->SetNumberField(TEXT("last_boundary_action_id"), Screen->GetExpeditionLastBoundaryActionIdForAudit());
	Json->SetNumberField(TEXT("rendered_stage_count"), Screen->GetExpeditionRenderedStageCountForAudit());
	if (bArtAudit)
	{
		Json->SetBoolField(TEXT("account_state_known"), Screen->IsExpeditionAccountStateKnownForAudit());
		Json->SetNumberField(TEXT("pending_action_id"), Screen->GetPendingActionIdForAudit());
	}
#endif
	const TCHAR* Selectors[] = {
		TEXT("ExpeditionStageButton_1"), TEXT("ExpeditionStageButton_2"),
		TEXT("ExpeditionStageTitle_1"), TEXT("ExpeditionStageTitle_2"),
		TEXT("ExpeditionPreviewButton"), TEXT("ExpeditionBackButton"),
		TEXT("ExpeditionChallenge"), TEXT("ExpeditionPreviewRoot"),
		TEXT("ExpeditionSideRoot"), TEXT("ExpeditionCenterRoot"),
		TEXT("FeatureFrameClose")};
	auto Widgets = MakeShared<FJsonObject>();
	for (const TCHAR* Selector : Selectors)
		Widgets->SetObjectField(Selector, WidgetState(Screen->GetWidgetFromName(Selector)));
	if (bArtAudit)
	{
		Widgets->SetObjectField(TEXT("ExpeditionMapScroll"),
			WidgetState(Screen->GetWidgetFromName(TEXT("ExpeditionMapScroll"))));
		Widgets->SetObjectField(TEXT("ExpeditionTaskConfigOpenButton"),
			WidgetState(Screen->GetWidgetFromName(TEXT("ExpeditionTaskConfigOpenButton"))));
	}
	Json->SetObjectField(TEXT("widgets"), Widgets);
	if (bArtAudit)
	{
		const TCHAR* ArtSelectors[] = {
			TEXT("ExpeditionMapScroll"), TEXT("ExpeditionMapSelectedPanel"),
			TEXT("ExpeditionChallenge"), TEXT("ExpeditionTroopCount"),
			TEXT("ExpeditionStageFlag_1"), TEXT("ExpeditionStageFlag_2"),
			TEXT("ExpeditionTaskPanelImage"), TEXT("ExpeditionCenterRoot"),
			TEXT("ExpeditionCenterImg"),
			TEXT("ExpeditionCenterFrameLeft"), TEXT("ExpeditionCenterFrameRight"),
			TEXT("ExpeditionCenterImgArt"), TEXT("ExpeditionCenterFlagMes"),
			TEXT("ExpeditionSideRoot"), TEXT("ExpeditionPreviewRoot"),
			TEXT("ExpeditionPreviewPanelImage"),
			TEXT("ExpeditionConfigurationNotice"),
			TEXT("ExpeditionStageType_1"), TEXT("ExpeditionStageType_2")};
		auto Art = MakeShared<FJsonObject>();
		Art->SetObjectField(TEXT("ExpeditionScreenRoot"), WidgetState(Screen.Get()));
		Art->SetObjectField(TEXT("FeatureModeCanvas"),
			ArtWidgetState(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))));
		for (const TCHAR* Selector : ArtSelectors)
			Art->SetObjectField(Selector, ArtWidgetState(Screen->GetWidgetFromName(Selector)));
		Json->SetObjectField(TEXT("art_widgets"), Art);
	}
	return Json;
}

bool URokExpeditionStagePreviewAuditSubsystem::Click(UWidget* Target, const TCHAR* Label)
{
	if (bArtAudit && Screen.IsValid() && Screen->IsScreenOpen()
		&& !Expect(FeatureModeReady(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))),
			TEXT("Expedition shared window animation still blocks Slate input"))) return false;
	if (!Expect(Identity() && ValidateBusinessBoundary() && Target && Visible(Target)
		&& Target->GetIsEnabled() && FSlateApplication::IsInitialized(),
		TEXT("Expedition Slate target unavailable or business state changed"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("Expedition target has no cached geometry"))) return false;
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30.0f, -30.0f));
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0,
		FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0,
		FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	TArray<TSharedPtr<FJsonValue>> Types;
	bool bUmg = false;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		bUmg |= Type.Contains(TEXT("SObjectWidget"));
	}
	const FString Terminal = Path.IsValid() && Path.Widgets.Num() > 0
		? Path.Widgets.Last().Widget->GetTypeAsString() : FString();
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Window.IsValid() && Path.IsValid()
		&& &Path.GetWindow().Get() == Window.Get();
	if (!Expect(Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get()) && Target->IsHovered()
		&& bUmg && Terminal.Contains(TEXT("SButton")) && bOwnWindow,
		TEXT("Expedition Slate hit did not terminate at own-window UMG SButton"))) return false;
	const auto Before = Controller->GetCityPlacementPreviewSnapshot();
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const auto After = Controller->GetCityPlacementPreviewSnapshot();
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("label"), Label);
	Action->SetStringField(TEXT("method"), TEXT("Slate ProcessMouseMoveEvent+DownEvent+UpEvent"));
	Action->SetObjectField(TEXT("absolute_center"), Point(Center));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetStringField(TEXT("terminal_type"), Terminal);
	Action->SetBoolField(TEXT("own_game_window"), bOwnWindow);
	Action->SetBoolField(TEXT("target_in_hit_path"), true);
	Action->SetBoolField(TEXT("target_hovered"), true);
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetNumberField(TEXT("controller_primary_before"), static_cast<double>(Before.PrimaryPressEventCount));
	Action->SetNumberField(TEXT("controller_primary_after"), static_cast<double>(After.PrimaryPressEventCount));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Down && Up && After.PrimaryPressEventCount == Before.PrimaryPressEventCount
		&& ValidateBusinessBoundary(), TEXT("Expedition UI click crossed world/business boundary"));
}

bool URokExpeditionStagePreviewAuditSubsystem::ProbeBlockedChallenge()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	UWidget* Challenge = Screen.IsValid()
		? Screen->GetWidgetFromName(TEXT("ExpeditionChallenge")) : nullptr;
	UWidget* Center = Screen.IsValid()
		? Screen->GetWidgetFromName(TEXT("ExpeditionCenterRoot")) : nullptr;
	if (!Expect(bArtAudit && Challenge && Center && Screen->IsExpeditionTaskConfigVisibleForAudit()
		&& Screen->IsExpeditionPreviewForAudit()
		&& Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("PreviewTransition"))
		&& Center->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& !Challenge->GetIsEnabled() && !PendingBlockedSlateAction.IsValid()
		&& FSlateApplication::IsInitialized() && ValidateBusinessBoundary(),
		TEXT("Preview source input disable not established before blocked Slate probe"))) return false;
	const FGeometry Geometry = Challenge->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("Disabled Challenge has no cached source geometry"))) return false;
	const FVector2D CenterPoint = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30.0f, -30.0f));
	App.SetCursorPos(CenterPoint);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, CenterPoint, Away, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = CenterPoint;
	const FWidgetPath Path = App.LocateWindowUnderMouse(CenterPoint,
		App.GetInteractiveTopLevelWindows(), false, 0);
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Window.IsValid() && Path.IsValid()
		&& &Path.GetWindow().Get() == Window.Get();
	TArray<TSharedPtr<FJsonValue>> Types;
	bool bUmg = false;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		bUmg |= Type.Contains(TEXT("SObjectWidget"));
	}
	const bool bTargetHit = Path.IsValid()
		&& Path.ContainsWidget(&Challenge->TakeWidget().Get());
	UWidget* InputBlocker = Screen->GetWidgetFromName(TEXT("FeatureScreenInputBlocker"));
	const bool bBlockerHit = InputBlocker && Path.IsValid()
		&& Path.ContainsWidget(&InputBlocker->TakeWidget().Get());
	if (!Expect(bOwnWindow && bUmg && !bTargetHit && bBlockerHit,
		TEXT("Disabled Challenge did not route to own feature input blocker"))) return false;
	auto Probe = MakeShared<FJsonObject>();
	Probe->SetStringField(TEXT("label"), TEXT("blocked_preview_challenge_stage1"));
	Probe->SetStringField(TEXT("method"), TEXT("Slate ProcessMouseMoveEvent+DownEvent+UpEvent"));
	Probe->SetObjectField(TEXT("absolute_center"), Point(CenterPoint));
	Probe->SetArrayField(TEXT("hit_path_types"), Types);
	Probe->SetStringField(TEXT("terminal_type"), Path.Widgets.Num() > 0
		? Path.Widgets.Last().Widget->GetTypeAsString() : FString());
	Probe->SetBoolField(TEXT("own_game_window"), bOwnWindow);
	Probe->SetBoolField(TEXT("target_in_hit_path"), bTargetHit);
	Probe->SetBoolField(TEXT("feature_input_blocker_in_hit_path"), bBlockerHit);
	Probe->SetObjectField(TEXT("before_state"), State());
	Probe->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, CenterPoint, CenterPoint, Pressed, EKeys::LeftMouseButton, 0,
			FModifierKeysState()));
	Probe->SetObjectField(TEXT("down_state"), State());
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, CenterPoint, CenterPoint,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	Probe->SetObjectField(TEXT("up_state"), State());
	Probe->SetBoolField(TEXT("down_handled"), Down);
	Probe->SetBoolField(TEXT("up_handled"), Up);
	PendingBlockedSlateAction = Probe;
	const auto Stable = [this](const TSharedPtr<FJsonObject>& Json)
	{
		return Json->GetNumberField(TEXT("controller_primary_press_count"))
			== static_cast<double>(BaselinePrimaryPressCount)
			&& Json->GetNumberField(TEXT("pending_action_id")) == INDEX_NONE
			&& Json->GetNumberField(TEXT("last_boundary_action_id")) == INDEX_NONE;
	};
	return Expect(Down && Up && Stable(Probe->GetObjectField(TEXT("before_state")))
		&& Stable(Probe->GetObjectField(TEXT("down_state")))
		&& Stable(Probe->GetObjectField(TEXT("up_state")))
		&& ValidateBusinessBoundary(),
		TEXT("Disabled Challenge dispatched input, queued action or changed business state"));
#endif
}

bool URokExpeditionStagePreviewAuditSubsystem::FinishBlockedChallengeProbe()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!PendingBlockedSlateAction.IsValid() ||
		GFrameCounter <= static_cast<uint64>(PendingBlockedSlateAction->GetNumberField(TEXT("frame"))))
		return false;
	auto Next = State();
	PendingBlockedSlateAction->SetObjectField(TEXT("next_frame_state"), Next);
	PendingBlockedSlateAction->SetNumberField(TEXT("next_frame"),
		static_cast<double>(GFrameCounter));
	if (!Expect(Next->GetNumberField(TEXT("controller_primary_press_count"))
			== static_cast<double>(BaselinePrimaryPressCount)
		&& Next->GetNumberField(TEXT("pending_action_id")) == INDEX_NONE
		&& Next->GetNumberField(TEXT("last_boundary_action_id")) == INDEX_NONE
		&& Screen.IsValid() && Screen->IsExpeditionTaskConfigVisibleForAudit()
		&& Screen->IsExpeditionPreviewForAudit() && ValidateBusinessBoundary(),
		TEXT("Disabled Challenge caused delayed action, boundary or world input"))) return false;
	BlockedSlateActions.Add(MakeShared<FJsonValueObject>(PendingBlockedSlateAction));
	PendingBlockedSlateAction.Reset();
	return true;
#endif
}

bool URokExpeditionStagePreviewAuditSubsystem::PrepareOwnGameWindowFocus()
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
		&& ::GetClientRect(Handle, &Client)
		&& Client.right - Client.left == 1440 && Client.bottom - Client.top == 810,
		TEXT("Expedition audit requires own 1440x810 GameApp window"))) return false;
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
	return Expect(false, TEXT("Expedition audit requires Windows game window"));
#endif
}

bool URokExpeditionStagePreviewAuditSubsystem::StartArtTextureWarmup()
{
	if (!bArtAudit || WarmArtTextures.Num() != 0) return false;
	WarmStartFrame = GFrameCounter;
	for (const FArtWarmSpec& Spec : ArtWarmSpecs)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Spec.ObjectPath);
		if (!Expect(Texture && Texture->GetPathName() == Spec.ObjectPath,
			TEXT("Expedition source atlas failed to load by pinned object path"))) return false;
		WarmArtTextures.Emplace(Texture);
	}
	return WarmArtTextures.Num() == UE_ARRAY_COUNT(ArtWarmSpecs);
}

bool URokExpeditionStagePreviewAuditSubsystem::ArtTexturesReady()
{
	if (WarmArtTextures.Num() != UE_ARRAY_COUNT(ArtWarmSpecs)
		|| GFrameCounter <= WarmStartFrame) return false;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ArtWarmSpecs); ++Index)
	{
		const UTexture2D* Texture = WarmArtTextures[Index].Get();
		const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
		const FTextureRHIRef TextureRHI = Resource ? Resource->TextureRHI : FTextureRHIRef();
		const FArtWarmSpec& Spec = ArtWarmSpecs[Index];
		if (!Texture || Texture->GetSizeX() != Spec.Width || Texture->GetSizeY() != Spec.Height
			|| !TextureRHI.IsValid()) return false;
		const FRHITextureDesc& Desc = TextureRHI->GetDesc();
		if (Desc.Extent.X != Spec.Width || Desc.Extent.Y != Spec.Height) return false;
	}
	if (!bWarmArtReady)
	{
		bWarmArtReady = true;
		WarmReadyFrame = GFrameCounter;
		return false; // Let the ready texture resources settle for one natural game frame.
	}
	return GFrameCounter > WarmReadyFrame;
}

TSharedRef<FJsonObject> URokExpeditionStagePreviewAuditSubsystem::ArtTextureWarmState() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("load_started"), WarmStartFrame != 0);
	Json->SetBoolField(TEXT("ready_before_hud_click"), bWarmArtReady);
	Json->SetNumberField(TEXT("start_frame"), static_cast<double>(WarmStartFrame));
	Json->SetNumberField(TEXT("ready_frame"), static_cast<double>(WarmReadyFrame));
	TArray<TSharedPtr<FJsonValue>> Textures;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ArtWarmSpecs); ++Index)
	{
		const FArtWarmSpec& Spec = ArtWarmSpecs[Index];
		const UTexture2D* Texture = WarmArtTextures.IsValidIndex(Index)
			? WarmArtTextures[Index].Get() : nullptr;
		const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
		const FTextureRHIRef TextureRHI = Resource ? Resource->TextureRHI : FTextureRHIRef();
		const FRHITextureDesc* Desc = TextureRHI.IsValid() ? &TextureRHI->GetDesc() : nullptr;
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("object_path"), Spec.ObjectPath);
		Entry->SetStringField(TEXT("uasset_path"), Spec.UassetPath);
		FString InputHash;
		if (Inputs.IsValid()) Inputs->TryGetStringField(Spec.UassetPath, InputHash);
		Entry->SetStringField(TEXT("uasset_sha256"), InputHash);
		Entry->SetObjectField(TEXT("expected_size"), Point(FVector2D(Spec.Width, Spec.Height)));
		Entry->SetObjectField(TEXT("actual_size"), Point(Texture
			? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D::ZeroVector));
		Entry->SetObjectField(TEXT("rhi_extent"), Point(Desc
			? FVector2D(Desc->Extent.X, Desc->Extent.Y) : FVector2D::ZeroVector));
		Entry->SetBoolField(TEXT("rhi_ready"), TextureRHI.IsValid());
		Textures.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("textures"), Textures);
	return Json;
}

bool URokExpeditionStagePreviewAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(),
		TEXT("Wrong project/world or Slate unavailable"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 AuditFlagCount = 0, ArtFlagCount = 0, LocalFlagCount = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditFlagCount; continue; }
		if (Token.Equals(ArtAuditFlag, ESearchCase::IgnoreCase)) { ++ArtFlagCount; continue; }
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlagCount; continue; }
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok diagnostic/fixture flag"));
	}
	if (!Expect(AuditFlagCount == (bArtAudit ? 0 : 1)
		&& ArtFlagCount == (bArtAudit ? 1 : 0) && LocalFlagCount == 1,
		TEXT("Expedition audit/local-only flag missing or duplicate"))) return false;
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
	Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Runtime = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	Server = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
		if (Widget && Widget->GetWorld() == GetWorld())
		{
			if (Hud.IsValid()) return Expect(false, TEXT("More than one game HUD"));
			Hud = Cast<URokMainHudWidget>(Widget);
		}
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	if (!Expect(Controller.IsValid() && Hud.IsValid() && Screen.IsValid()
		&& Buildings.IsValid() && Runtime.IsValid() && Server.IsValid()
		&& Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
		&& Runtime->IsRuntimeStateLoaded() && !Screen->IsScreenOpen()
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), 0.1f),
		TEXT("Expected idle 1440x810 local GameApp HUD"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Account profile is not local preview"))) return false;
	TArray<FRokExpeditionStagePreviewRow> StageRows;
	FString StageError;
	FRokExpeditionStagePreviewRow StageOne, StageTwo;
	if (!Expect(RokExpeditionStagePreviewData::LoadAll(StageRows, StageError)
		&& StageRows.Num() == 80
		&& RokExpeditionStagePreviewData::FindById(StageRows, 1, StageOne)
		&& RokExpeditionStagePreviewData::FindById(StageRows, 2, StageTwo)
		&& StageOne.Id == 1 && StageTwo.Id == 2,
		TEXT("Source stages 1/2 unavailable or invalid"))) return false;
	BaselinePrimaryPressCount = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	BaselineBusinessJson = BusinessState();
	BaselineBusinessState = SerializeAuditJsonObject(BaselineBusinessJson.ToSharedRef());
	if (!Expect(ValidateBusinessBoundary(), TEXT("Expedition baseline business snapshot invalid"))) return false;
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokExpeditionStagePreviewAuditSubsystem.h"),
		TEXT("Source/Rok/RokExpeditionStagePreviewAuditSubsystem.cpp"),
		bArtAudit ? TEXT("Scripts/VerifyRokExpeditionArtRuntimeV2.py")
			: TEXT("Scripts/VerifyRokExpeditionStagePreviewRuntime.py"),
		TEXT("Scripts/BuildRokExpeditionStagePreview.py"),
		TEXT("Scripts/VerifyRokExpeditionStagePreview.py"),
		TEXT("Saved/RokUnityMigration/expedition_stage_preview.json"),
		TEXT("Source/Rok/RokExpeditionStagePreviewData.h"),
		TEXT("Source/Rok/RokExpeditionStagePreviewData.cpp"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"),
		TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"),
		TEXT("Source/Rok/RokServerSubsystem.cpp"),
		TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json")};
	for (const TCHAR* Path : Required)
	{
		const FString Hash = FileSha(FPaths::ProjectDir() / Path);
		if (!Expect(Hash.Len() == 64, TEXT("Expedition audit input missing"))) return false;
		Inputs->SetStringField(Path, Hash);
	}
	if (bArtAudit)
	{
		const TCHAR* ArtRequired[] = {
			TEXT("Scripts/PrepareRokExpeditionVisualAssets.py"),
			TEXT("Scripts/VerifyRokExpeditionVisualAssets.py"),
			TEXT("Scripts/PrepareRokExpeditionResultVisualAssets.py"),
			TEXT("Scripts/VerifyRokExpeditionResultVisualAssets.py"),
			TEXT("Scripts/ImportRokExpeditionResultVisualAsset.py"),
			TEXT("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_ui_implementation.json"),
			TEXT("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_ui_implementation_revision2.json"),
			TEXT("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_ui_implementation_revision3.json"),
			TEXT("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_art_source_oracle_review.json"),
			TEXT("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_visual_ue_import_20261009_015909_339908.json"),
			TEXT("Saved/RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/expedition_result_ue_import_20261009_020448_951277.json"),
			TEXT("Saved/UnityExpeditionAnimationProbeV1/OutputRuns/20261009_013025_916_42504/native_animation_curves.json"),
			TEXT("Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlas_manifest.json"),
			TEXT("Saved/RokUnityMigration/ExpeditionVisualAssets/v2/result_visual_manifest.json"),
			TEXT("Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlases/ui_battle.png"),
			TEXT("Saved/RokUnityMigration/ExpeditionVisualAssets/v1/atlases/img_battle_result_task.png"),
			TEXT("Saved/RokUnityMigration/ExpeditionVisualAssets/v2/ui_result.png"),
			TEXT("Content/RokPrototype/UI/ExpeditionSource/rok_expedition_battle_atlas.uasset"),
			TEXT("Content/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.uasset"),
			TEXT("Content/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.uasset")};
		for (const TCHAR* Path : ArtRequired)
		{
			const FString Hash = FileSha(FPaths::ProjectDir() / Path);
			if (!Expect(Hash.Len() == 64, TEXT("Expedition art audit source missing"))) return false;
			Inputs->SetStringField(Path, Hash);
		}
	}
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD OwnerPid = 0;
	if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
	RECT Client = {};
	const bool bOwn = Handle && ::IsWindow(Handle) && OwnerPid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client)
		&& Client.right - Client.left == 1440 && Client.bottom - Client.top == 810;
	if (!Expect(bOwn, TEXT("Expedition audit requires own 1440x810 GameApp window"))) return false;
	if (bArtAudit)
	{
		// Foreground can change between natural ticks; retry in startup rather than failing this frame.
		if (::GetForegroundWindow() != Handle) return false;
	}
	else
	{
		if (::IsIconic(Handle)) ::ShowWindow(Handle, SW_RESTORE);
		else if (!::IsWindowVisible(Handle)) ::ShowWindow(Handle, SW_SHOW);
		::SetForegroundWindow(Handle);
		if (!Expect(::GetForegroundWindow() == Handle,
			TEXT("Expedition audit game window not foreground"))) return false;
	}
#else
	return Expect(false, TEXT("Expedition audit requires Windows game window"));
#endif
	return true;
}

void URokExpeditionStagePreviewAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep;
	StepSince = Elapsed;
	StableFrames = 0;
}

void URokExpeditionStagePreviewAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(ValidateBusinessBoundary() && !Pending.IsValid()
		&& !FScreenshotRequest::IsScreenshotRequested(),
		TEXT("Expedition screenshot boundary invalid or concurrent"))) return;
	if (bArtAudit && Screen.IsValid() && Screen->IsScreenOpen()
		&& !Expect(FeatureModeReady(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))),
			TEXT("Expedition source capture before shared window animation finished"))) return;
#if !UE_BUILD_SHIPPING
	if (bArtAudit && Screen.IsValid() && Screen->IsScreenOpen())
	{
		const bool bTask = Screen->IsExpeditionTaskConfigVisibleForAudit();
		const bool bSourceReady = bTask
			? SourceImageReady(Screen->GetWidgetFromName(TEXT("ExpeditionTaskPanelImage")),
				TaskPanelAtlas, 676, 761, false)
				&& SourceImageReady(Screen->GetWidgetFromName(TEXT("ExpeditionCenterFrameLeft")),
					ResultAtlas, 512, 512, false)
				&& SourceImageReady(Screen->GetWidgetFromName(TEXT("ExpeditionCenterFrameRight")),
					ResultAtlas, 512, 512, false)
			: SourceImageReady(Screen->GetWidgetFromName(TEXT("ExpeditionStageFlag_1")),
				BattleAtlas, 2048, 256, false)
				&& SourceImageReady(Screen->GetWidgetFromName(TEXT("ExpeditionStageFlag_2")),
					BattleAtlas, 2048, 256, false);
		if (!Expect(bSourceReady, TEXT("Expedition source art texture/UV absent or fallback"))) return;
	}
#endif
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetObjectField(TEXT("requested_state"), State());
	Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	Pending->SetStringField(TEXT("png"), PendingPath);
	NextStep = After;
	bScreenshotRequested = true;
	bScreenshotDone = false;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}

void URokExpeditionStagePreviewAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || !bScreenshotRequested) return;
	if (!Expect(ValidateBusinessBoundary(), TEXT("Expedition state mutated on screenshot frame"))) return;
	const FString Hash = FileSha(PendingPath);
	if (!Expect(Hash.Len() == 64, TEXT("Expedition screenshot callback PNG missing"))) return;
	Pending->SetStringField(TEXT("png_sha256"), Hash);
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(IFileManager::Get().FileSize(*PendingPath)));
	Pending->SetObjectField(TEXT("processed_state"), State());
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}

void URokExpeditionStagePreviewAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(ValidateBusinessBoundary(), TEXT("Expedition state mutated after screenshot frame"))) return;
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	bScreenshotRequested = false;
	bScreenshotDone = false;
	Save(false);
	Enter(NextStep);
}

bool URokExpeditionStagePreviewAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), bArtAudit
		? TEXT("rok.expedition_art.runtime_audit.v2")
		: TEXT("rok.expedition_stage_preview.runtime_audit.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty()
		&& Captures.Num() == (bArtAudit ? 22 : 14)
		&& Actions.Num() == (bArtAudit ? 14 : 10)
		&& BlockedSlateActions.Num() == (bArtAudit ? 1 : 0));
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), bArtAudit
		? TEXT("Expedition stages 1/2 source art and child animation, read-only real HUD Slate route, no account/battle/claim/server requests")
		: TEXT("Source-only Expedition stages 1/2, real HUD Slate preview/normal UI, no account unlock/battle/claim/server requests"));
	Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	TArray<TSharedPtr<FJsonValue>> SourceIds = {MakeShared<FJsonValueNumber>(1), MakeShared<FJsonValueNumber>(2)};
	Json->SetArrayField(TEXT("source_stage_ids"), SourceIds);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetObjectField(TEXT("initial_business_state"),
		BaselineBusinessJson.IsValid() ? BaselineBusinessJson.ToSharedRef() : MakeShared<FJsonObject>());
	if (bArtAudit) Json->SetObjectField(TEXT("texture_warmup"), ArtTextureWarmState());
	Json->SetNumberField(TEXT("initial_controller_primary_press_count"),
		static_cast<double>(BaselinePrimaryPressCount));
	Json->SetArrayField(TEXT("actions"), Actions);
	if (bArtAudit)
	{
		Json->SetArrayField(TEXT("blocked_slate_actions"), BlockedSlateActions);
		if (PendingBlockedSlateAction.IsValid())
			Json->SetObjectField(TEXT("pending_blocked_slate_action"),
				PendingBlockedSlateAction.ToSharedRef());
	}
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetObjectField(TEXT("final_state"), State());
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	FString Text;
	if (!FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text))) return false;
	return FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokExpeditionStagePreviewAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty()
		&& Captures.Num() == (bArtAudit ? 22 : 14)
		&& Actions.Num() == (bArtAudit ? 14 : 10)
		&& BlockedSlateActions.Num() == (bArtAudit ? 1 : 0)
		&& ValidateBusinessBoundary();
	if (!Complete && Failures.IsEmpty()) Failures.Add(bArtAudit
		? TEXT("Expedition art audit incomplete")
		: TEXT("Expedition stage preview audit incomplete"));
	Save(Complete);
	if (bArtAudit)
	{
		UE_LOG(LogTemp, Display, TEXT("Rok expedition art audit %s: %s"),
			Complete ? TEXT("passed") : TEXT("failed"), *Directory);
		if (!Complete) UE_LOG(LogTemp, Error, TEXT("Expedition art audit: %s"), *Failures.Last());
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Rok expedition stage preview audit %s: %s"),
			Complete ? TEXT("passed") : TEXT("failed"), *Directory);
		if (!Complete) UE_LOG(LogTemp, Error, TEXT("Expedition preview audit: %s"), *Failures.Last());
	}
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}

void URokExpeditionStagePreviewAuditSubsystem::Tick(float DeltaTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (bFinished || !Identity()) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (bArtAudit && Step == 0 && Elapsed - StepSince > 20.0)
	{
		Finish(TEXT("Expedition audit own game window foreground timeout"));
		return;
	}
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 30.0) Finish(TEXT("Expedition screenshot timeout"));
		else AdvanceCapture();
		return;
	}
	if (Step > 0 && Elapsed - StepSince > 20.0)
	{
		Finish(bArtAudit && Step == 1
			? TEXT("Expedition source atlas warmup timeout")
			: FString::Printf(TEXT("Expedition audit step %d timeout"), Step));
		return;
	}
	if (Step > 0 && !Expect(ValidateBusinessBoundary(),
		TEXT("Expedition preview changed task, wallet, roster, account or HTTP state"))) return;
	if (bArtAudit)
	{
		TickArt();
		return;
	}
	const auto Normal = [this](int32 Stage)
	{
		return Screen.IsValid() && Screen->IsScreenOpen() && Screen->IsExpeditionModeForAudit()
			&& Screen->GetSelectedExpeditionStageIdForAudit() == Stage
			&& !Screen->IsExpeditionPreviewForAudit()
			&& Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("Idle"));
	};
	const auto Preview = [this](int32 Stage)
	{
		return Screen.IsValid() && Screen->IsScreenOpen() && Screen->IsExpeditionModeForAudit()
			&& Screen->GetSelectedExpeditionStageIdForAudit() == Stage
			&& Screen->IsExpeditionPreviewForAudit()
			&& Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("Idle"));
	};
	switch (Step)
	{
	case 0:
		if (Begin()) Enter(1);
		break;
	case 1:
		if (Click(Hud->GetWidgetFromName(TEXT("CampaignFeatureButton")), TEXT("open_from_hud"))) Enter(2);
		break;
	case 2:
		if (Normal(1) && Screen->GetExpeditionRenderedStageCountForAudit() == 80
			&& Visible(Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_1")))
			&& Visible(Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_2"))))
		{
			if (++StableFrames >= 2) Capture(TEXT("01_normal_stage1_open"), 3);
		}
		else StableFrames = 0;
		break;
	case 3:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionPreviewButton")), TEXT("preview_stage1")))
			Enter(103);
		break;
	case 103:
		if (Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("PreviewTransition"))
			&& Screen->GetSelectedExpeditionStageIdForAudit() == 1
			&& Screen->GetExpeditionAnimationElapsedForAudit() < 0.12f)
			Capture(TEXT("02_preview_stage1_start"), 4);
		break;
	case 4:
		if (Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("PreviewTransition"))
			&& Screen->GetExpeditionAnimationElapsedForAudit() >= 0.12f
			&& Screen->GetExpeditionAnimationElapsedForAudit() <= 0.24f)
			Capture(TEXT("03_preview_stage1_mid"), 5);
		break;
	case 5:
		if (Preview(1))
		{
			if (++StableFrames >= 2) Capture(TEXT("04_preview_stage1_end"), 6);
		}
		else StableFrames = 0;
		break;
	case 6:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionBackButton")), TEXT("back_to_normal_stage1")))
			Enter(106);
		break;
	case 106:
		if (Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("NormalTransition"))
			&& Screen->GetSelectedExpeditionStageIdForAudit() == 1
			&& Screen->GetExpeditionAnimationElapsedForAudit() < 0.12f)
			Capture(TEXT("05_normal_stage1_start"), 7);
		break;
	case 7:
		if (Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("NormalTransition"))
			&& Screen->GetExpeditionAnimationElapsedForAudit() >= 0.25f
			&& Screen->GetExpeditionAnimationElapsedForAudit() <= 0.43f)
			Capture(TEXT("06_normal_stage1_mid"), 8);
		break;
	case 8:
		if (Normal(1))
		{
			if (++StableFrames >= 2) Capture(TEXT("07_normal_stage1_end"), 9);
		}
		else StableFrames = 0;
		break;
	case 9:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionBackButton")), TEXT("repeat_back_normal_boundary")))
			Enter(109);
		break;
	case 109:
		if (Normal(1) && Screen->GetExpeditionLastBoundaryActionIdForAudit() == 1122)
			Capture(TEXT("08_repeat_back_normal_boundary"), 10);
		break;
	case 10:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_2")), TEXT("select_source_stage2")))
			Enter(101);
		break;
	case 101:
		if (Normal(2))
		{
			if (++StableFrames >= 2) Capture(TEXT("09_normal_stage2_selected"), 11);
		}
		else StableFrames = 0;
		break;
	case 11:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionPreviewButton")), TEXT("preview_stage2"))) Enter(12);
		break;
	case 12:
		if (Preview(2))
		{
			if (++StableFrames >= 2) Capture(TEXT("10_preview_stage2_end"), 13);
		}
		else StableFrames = 0;
		break;
	case 13:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionBackButton")), TEXT("back_to_normal_stage2"))) Enter(14);
		break;
	case 14:
		if (Normal(2))
		{
			if (++StableFrames >= 2) Capture(TEXT("11_normal_stage2_end"), 15);
		}
		else StableFrames = 0;
		break;
	case 15:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionChallenge")), TEXT("challenge_source_boundary")))
			Enter(115);
		break;
	case 115:
		if (Normal(2) && Screen->GetExpeditionLastBoundaryActionIdForAudit() == 1120)
			Capture(TEXT("12_challenge_boundary"), 16);
		break;
	case 16:
		if (Click(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")), TEXT("close_expedition"))) Enter(17);
		break;
	case 17:
		if (!Screen->IsScreenOpen() && Elapsed - StepSince >= 0.35)
			Capture(TEXT("13_closed"), 18);
		break;
	case 18:
		if (Click(Hud->GetWidgetFromName(TEXT("CampaignFeatureButton")), TEXT("reopen_from_hud"))) Enter(19);
		break;
	case 19:
		if (Normal(1) && Screen->GetExpeditionRenderedStageCountForAudit() == 80)
		{
			if (++StableFrames >= 2) Capture(TEXT("14_reopened_normal_stage1"), 20);
		}
		else StableFrames = 0;
		break;
	case 20:
		if (Normal(1)) Finish();
		break;
	default:
		Finish(TEXT("Invalid Expedition audit step"));
		break;
	}
#endif
}

void URokExpeditionStagePreviewAuditSubsystem::TickArt()
{
#if UE_BUILD_SHIPPING
	return;
#else
	const auto Map = [this](int32 Stage)
	{
		return Screen.IsValid() && Screen->IsScreenOpen()
			&& FeatureModeReady(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")))
			&& Screen->IsExpeditionModeForAudit()
			&& !Screen->IsExpeditionTaskConfigVisibleForAudit()
			&& Screen->GetSelectedExpeditionStageIdForAudit() == Stage
			&& Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("Idle"))
			&& Visible(Screen->GetWidgetFromName(TEXT("ExpeditionMapScroll")))
			&& Visible(Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_1")))
			&& Visible(Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_2")))
			&& !Screen->GetWidgetFromName(TEXT("ExpeditionTaskPanelImage"));
	};
	const auto Task = [this](int32 Stage, bool bPreview)
	{
		return Screen.IsValid() && Screen->IsScreenOpen()
			&& FeatureModeReady(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")))
			&& Screen->IsExpeditionModeForAudit()
			&& Screen->IsExpeditionTaskConfigVisibleForAudit()
			&& Screen->GetSelectedExpeditionStageIdForAudit() == Stage
			&& Screen->IsExpeditionPreviewForAudit() == bPreview
			&& Screen->GetExpeditionAnimationPhaseForAudit() == FName(TEXT("Idle"))
			&& !Screen->GetWidgetFromName(TEXT("ExpeditionMapScroll"))
			&& !Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_1"))
			&& !Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_2"));
	};
	const auto Transition = [this](int32 Stage, bool bPreview, const TCHAR* Phase,
		float Minimum, float Maximum)
	{
		const float Time = Screen.IsValid()
			? Screen->GetExpeditionAnimationElapsedForAudit() : -1.0f;
		return Screen.IsValid() && Screen->IsExpeditionTaskConfigVisibleForAudit()
			&& FeatureModeReady(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")))
			&& Screen->GetSelectedExpeditionStageIdForAudit() == Stage
			&& Screen->IsExpeditionPreviewForAudit() == bPreview
			&& Screen->GetExpeditionAnimationPhaseForAudit() == FName(Phase)
			&& Time >= Minimum && Time <= Maximum;
	};
	switch (Step)
	{
	case 0:
		if (PrepareOwnGameWindowFocus() && Begin() && StartArtTextureWarmup()) Enter(1);
		break;
	case 1:
		if (ArtTexturesReady()
			&& Click(Hud->GetWidgetFromName(TEXT("CampaignFeatureButton")), TEXT("open_from_hud"))) Enter(2);
		break;
	case 2:
		if (Map(1) && Screen->GetExpeditionRenderedStageCountForAudit() == 80)
		{
			if (++StableFrames >= 2) Capture(TEXT("01_map_stage1_open"), 3);
		}
		else StableFrames = 0;
		break;
	case 3:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_1")), TEXT("select_source_stage1"))) Enter(4);
		break;
	case 4:
		if (Map(1))
		{
			if (++StableFrames >= 2) Capture(TEXT("02_map_stage1_selected"), 5);
		}
		else StableFrames = 0;
		break;
	case 5:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionTaskConfigOpenButton")), TEXT("open_task_config_stage1"))) Enter(105);
		break;
	case 105:
		if (Transition(1, false, TEXT("NormalTransition"), 0.0f, 0.12f))
			Capture(TEXT("03_task_stage1_start"), 6);
		break;
	case 6:
		if (Transition(1, false, TEXT("NormalTransition"), 0.25f, 0.43f))
			Capture(TEXT("04_task_stage1_mid"), 7);
		break;
	case 7:
		if (Task(1, false))
		{
			if (++StableFrames >= 2) Capture(TEXT("05_task_stage1_end"), 8);
		}
		else StableFrames = 0;
		break;
	case 8:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionPreviewButton")), TEXT("preview_stage1"))) Enter(108);
		break;
	case 108:
		if (Transition(1, true, TEXT("PreviewTransition"), 0.0f, 0.12f))
			Capture(TEXT("06_preview_stage1_start"), 9);
		break;
	case 9:
		if (Transition(1, true, TEXT("PreviewTransition"), 0.12f, 0.24f))
			Capture(TEXT("07_preview_stage1_mid"), 10);
		break;
	case 10:
		if (ProbeBlockedChallenge()) Enter(110);
		break;
	case 110:
		if (FinishBlockedChallengeProbe()) Enter(210);
		break;
	case 210:
		if (Task(1, true))
		{
			if (++StableFrames >= 2) Capture(TEXT("08_preview_stage1_end"), 11);
		}
		else StableFrames = 0;
		break;
	case 11:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionBackButton")), TEXT("back_to_normal_stage1"))) Enter(111);
		break;
	case 111:
		if (Transition(1, false, TEXT("NormalTransition"), 0.0f, 0.12f))
			Capture(TEXT("09_normal_stage1_start"), 12);
		break;
	case 12:
		if (Transition(1, false, TEXT("NormalTransition"), 0.25f, 0.43f))
			Capture(TEXT("10_normal_stage1_mid"), 13);
		break;
	case 13:
		if (Task(1, false))
		{
			if (++StableFrames >= 2) Capture(TEXT("11_normal_stage1_end"), 14);
		}
		else StableFrames = 0;
		break;
	case 14:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionBackButton")), TEXT("back_to_map_stage1"))) Enter(114);
		break;
	case 114:
		if (Map(1))
		{
			if (++StableFrames >= 2) Capture(TEXT("12_map_stage1_return"), 15);
		}
		else StableFrames = 0;
		break;
	case 15:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionStageButton_2")), TEXT("select_source_stage2"))) Enter(115);
		break;
	case 115:
		if (Map(2))
		{
			if (++StableFrames >= 2) Capture(TEXT("13_map_stage2_selected"), 16);
		}
		else StableFrames = 0;
		break;
	case 16:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionTaskConfigOpenButton")), TEXT("open_task_config_stage2"))) Enter(116);
		break;
	case 116:
		if (Transition(2, false, TEXT("NormalTransition"), 0.0f, 0.12f))
			Capture(TEXT("14_task_stage2_start"), 17);
		break;
	case 17:
		if (Transition(2, false, TEXT("NormalTransition"), 0.25f, 0.43f))
			Capture(TEXT("15_task_stage2_mid"), 18);
		break;
	case 18:
		if (Task(2, false))
		{
			if (++StableFrames >= 2) Capture(TEXT("16_task_stage2_end"), 19);
		}
		else StableFrames = 0;
		break;
	case 19:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionPreviewButton")), TEXT("preview_stage2"))) Enter(119);
		break;
	case 119:
		if (Task(2, true))
		{
			if (++StableFrames >= 2) Capture(TEXT("17_preview_stage2_end"), 20);
		}
		else StableFrames = 0;
		break;
	case 20:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionBackButton")), TEXT("back_to_normal_stage2"))) Enter(120);
		break;
	case 120:
		if (Task(2, false))
		{
			if (++StableFrames >= 2) Capture(TEXT("18_normal_stage2_end"), 21);
		}
		else StableFrames = 0;
		break;
	case 21:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionChallenge")), TEXT("challenge_source_boundary"))) Enter(121);
		break;
	case 121:
		if (Task(2, false) && Screen->GetExpeditionLastBoundaryActionIdForAudit() == 1120)
			Capture(TEXT("19_challenge_boundary"), 22);
		break;
	case 22:
		if (Click(Screen->GetWidgetFromName(TEXT("ExpeditionBackButton")), TEXT("back_to_map_stage2"))) Enter(122);
		break;
	case 122:
		if (Map(2))
		{
			if (++StableFrames >= 2) Capture(TEXT("20_map_stage2_return"), 23);
		}
		else StableFrames = 0;
		break;
	case 23:
		if (Click(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")), TEXT("close_expedition"))) Enter(24);
		break;
	case 24:
		if (!Screen->IsScreenOpen() && Elapsed - StepSince >= 0.35)
			Capture(TEXT("21_closed"), 25);
		break;
	case 25:
		if (Click(Hud->GetWidgetFromName(TEXT("CampaignFeatureButton")), TEXT("reopen_from_hud"))) Enter(26);
		break;
	case 26:
		if (Map(1) && Screen->GetExpeditionRenderedStageCountForAudit() == 80)
		{
			if (++StableFrames >= 2) Capture(TEXT("22_reopened_map_stage1"), 27);
		}
		else StableFrames = 0;
		break;
	case 27:
		if (Map(1)) Finish();
		break;
	default:
		Finish(TEXT("Invalid Expedition art audit step"));
		break;
	}
#endif
}
