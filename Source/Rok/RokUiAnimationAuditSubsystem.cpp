#include "RokUiAnimationAuditSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokWorldMapHudWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokBuildingPanelWidget.h"
#include "RokScoutScreenWidget.h"
#include "RokTrainArmyMeshWidget.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
	int32 ExpectedCaptureCount()
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("RokScoutScreenAudit"))) return 6;
		if (FParse::Param(FCommandLine::Get(), TEXT("RokTrainArmyDetailAudit"))) return 7;
		if (FParse::Param(FCommandLine::Get(), TEXT("RokBuildingLevelDetailAudit"))) return 8;
		if (FParse::Param(FCommandLine::Get(), TEXT("RokArmyShowAudit"))) return 6;
		if (FParse::Param(FCommandLine::Get(), TEXT("RokCommanderRosterAudit"))) return 4;
		return FParse::Param(FCommandLine::Get(), TEXT("RokChatAnimationAudit")) ? 21 : 12;
	}

	bool IsVisibleInHierarchy(const UWidget* Widget)
	{
		if (!Widget || !Widget->IsVisible()) return false;
		for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		{
			if (!Parent->IsVisible()) return false;
		}
		for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		{
			if (const UUserWidget* OuterWidget = Cast<UUserWidget>(Outer); OuterWidget && !OuterWidget->IsVisible())
			{
				return false;
			}
		}
		return true;
	}

	void AddVector(TSharedRef<FJsonObject> Object, const TCHAR* Key, FVector2D Value)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(MakeShared<FJsonValueNumber>(Value.X));
		Values.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Object->SetArrayField(Key, Values);
	}
}

bool URokUiAnimationAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokUiAnimationAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokUiAnimationAuditSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game;
}

void URokUiAnimationAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FString RunName = FString::Printf(TEXT("%s_%s_%u"),
		*FPaths::MakeValidFileName(GetWorld()->GetName()),
		*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
		FPlatformProcess::GetCurrentProcessId());
	OutputDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/UiAnimationAudit") / RunName);
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	ScreenshotProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokUiAnimationAuditSubsystem::OnScreenshotProcessed);
	UE_LOG(LogTemp, Display, TEXT("Rok UI animation audit enabled: world=%s output=%s input=UMG_delegate_not_physical."),
		*GetWorld()->GetPathName(), *OutputDirectory);
	int32 ModeCount = 0;
	for (const TCHAR* Mode : {TEXT("RokScoutScreenAudit"), TEXT("RokTrainArmyDetailAudit"),
		TEXT("RokBuildingLevelDetailAudit"), TEXT("RokArmyShowAudit"), TEXT("RokCommanderRosterAudit"), TEXT("RokChatAnimationAudit")})
		ModeCount += FParse::Param(FCommandLine::Get(), Mode) ? 1 : 0;
	if (ModeCount > 1)
		Fail(TEXT("Enable only one UI audit mode per run; multiple modes would omit requested coverage."));
	else if (FParse::Param(FCommandLine::Get(), TEXT("RokCommanderDataAudit"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("RokCommanderRosterAudit")))
		Fail(TEXT("RokCommanderDataAudit requires RokCommanderRosterAudit."));
	else if (FParse::Param(FCommandLine::Get(), TEXT("RokScoutKeyboardAudit"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("RokScoutScreenAudit")))
		Fail(TEXT("RokScoutKeyboardAudit requires RokScoutScreenAudit."));
}

void URokUiAnimationAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
	Super::Deinitialize();
}

TStatId URokUiAnimationAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokUiAnimationAuditSubsystem, STATGROUP_Tickables);
}

bool URokUiAnimationAuditSubsystem::FindRuntimeWidgets()
{
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokMainHudWidget::StaticClass(), false);
	int32 HudCount = 0;
	for (UUserWidget* Candidate : Found)
	{
		if (Candidate->GetWorld() == GetWorld() && IsVisibleInHierarchy(Candidate)
			&& Candidate->GetCachedGeometry().GetLocalSize().X > 1.0f)
		{
			Hud = Cast<URokMainHudWidget>(Candidate);
			++HudCount;
		}
	}
	Found.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokUnityFeatureScreenWidget::StaticClass(), false);
	int32 FeatureCount = 0;
	for (UUserWidget* Candidate : Found)
	{
		if (Candidate->GetWorld() == GetWorld())
		{
			Feature = Cast<URokUnityFeatureScreenWidget>(Candidate);
			++FeatureCount;
		}
	}
	if (HudCount != 1 || FeatureCount != 1)
	{
		Fail(FString::Printf(TEXT("Expected one visible MainHud and one shared Feature host in this world; got hud=%d feature=%d. World preview must use tactical zoom."), HudCount, FeatureCount));
		return false;
	}
	if (Feature->IsScreenOpen())
	{
		Fail(TEXT("A feature window was already open; audit will not overwrite an existing route."));
		return false;
	}
	Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	Scout = Cast<URokScoutScreenWidget>(Hud->GetWidgetFromName(TEXT("ScoutScreenWidget")));
	if (FParse::Param(FCommandLine::Get(), TEXT("RokScoutScreenAudit")) && (!Scout.IsValid() || !Scout->IsScreenOpen()))
	{
		Fail(TEXT("Scout audit requires explicit scout preview with unknown camp level and queue."));
		return false;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RokBuildingLevelDetailAudit"))
		&& (!Modal.IsValid() || !Modal->IsModalOpen()))
	{
		Fail(TEXT("Building level audit requires the explicit resource building reference preview to be open."));
		return false;
	}
	return true;
}

bool URokUiAnimationAuditSubsystem::Dispatch(UUserWidget* Owner, FName ButtonName, FName EventName)
{
	UButton* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(ButtonName)) : nullptr;
	if (!Button || Owner->GetWorld() != GetWorld() || !IsVisibleInHierarchy(Button))
	{
		Fail(FString::Printf(TEXT("Missing, hidden or wrong-world button: %s"), *ButtonName.ToString()));
		return false;
	}
	TSharedRef<FJsonObject> Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("button"), ButtonName.ToString());
	Action->SetStringField(TEXT("event"), EventName.ToString());
	Action->SetNumberField(TEXT("time_seconds"), ElapsedSeconds);
	Action->SetNumberField(TEXT("audit_frame"), static_cast<double>(AuditFrame));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	if (EventName == TEXT("pressed")) Button->OnPressed.Broadcast();
	else if (EventName == TEXT("released")) Button->OnReleased.Broadcast();
	else Button->OnClicked.Broadcast();
	UE_LOG(LogTemp, Display, TEXT("Rok UI animation audit action: %s %s t=%.6f frame=%llu"),
		*ButtonName.ToString(), *EventName.ToString(), ElapsedSeconds, AuditFrame);
	return true;
}

void URokUiAnimationAuditSubsystem::EnterPhase(EPhase NextPhase)
{
	Phase = NextPhase;
	PhaseStartedAt = ElapsedSeconds;
	PhaseCaptureIndex = 0;
	switch (Phase)
	{
	case EPhase::TaskClose:
	case EPhase::TaskOpen: Dispatch(Hud.Get(), TEXT("TaskFoldButton"), TEXT("clicked")); break;
	case EPhase::MoreClose:
	case EPhase::MoreOpen: Dispatch(Hud.Get(), TEXT("MoreFeatureButton"), TEXT("clicked")); break;
	case EPhase::ButtonPress: Dispatch(Hud.Get(), TEXT("ItemsFeatureButton"), TEXT("pressed")); break;
	case EPhase::ButtonRelease: Dispatch(Hud.Get(), TEXT("ItemsFeatureButton"), TEXT("released")); break;
	case EPhase::ItemsOpen: Dispatch(Hud.Get(), TEXT("ItemsFeatureButton"), TEXT("clicked")); break;
	case EPhase::ItemsClose: Dispatch(Feature.Get(), TEXT("FeatureFrameClose"), TEXT("clicked")); break;
	case EPhase::ChatOpen:
	{
		TArray<UUserWidget*> WorldHuds;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), WorldHuds, URokWorldMapHudWidget::StaticClass(), false);
		UUserWidget* WorldHud = nullptr;
		for (UUserWidget* Candidate : WorldHuds)
		{
			if (Candidate && Candidate->GetWorld() == GetWorld() && IsVisibleInHierarchy(Candidate))
			{
				if (WorldHud) { Fail(TEXT("Ambiguous world-map HUD for chat audit")); return; }
				WorldHud = Candidate;
			}
		}
		Dispatch(WorldHud ? WorldHud : Hud.Get(), WorldHud ? TEXT("WorldMapChatButton") : TEXT("ChatButton"), TEXT("clicked"));
		break;
	}
	case EPhase::ContactsHide:
	case EPhase::ContactsShow: Dispatch(Feature.Get(), TEXT("ChatContactsToggleButton"), TEXT("clicked")); break;
	case EPhase::ChatChannel: Dispatch(Feature.Get(), TEXT("ChatChannel_1"), TEXT("clicked")); break;
	case EPhase::ChatEmoji: Dispatch(Feature.Get(), TEXT("ChatEmojiButton"), TEXT("clicked")); break;
	case EPhase::ChatClose: Dispatch(Feature.Get(), TEXT("FeatureFrameClose"), TEXT("clicked")); break;
	case EPhase::CommanderOpen: Dispatch(Hud.Get(), TEXT("CommanderFeatureButton"), TEXT("clicked")); break;
	case EPhase::CommanderSort: Dispatch(Feature.Get(), TEXT("CommanderSortAction"), TEXT("clicked")); break;
	case EPhase::CommanderClose: Dispatch(Feature.Get(), TEXT("CommanderBack"), TEXT("clicked")); break;
	case EPhase::ArmyOpen: Dispatch(Hud.Get(), TEXT("ArmyQueueButton"), TEXT("clicked")); break;
	case EPhase::ArmyRules: Dispatch(Feature.Get(), TEXT("ArmyShowRulesButton"), TEXT("clicked")); break;
	case EPhase::ArmyList: Dispatch(Feature.Get(), TEXT("ArmyShowRulesBackButton"), TEXT("clicked")); break;
	case EPhase::ArmyClose: Dispatch(Feature.Get(), TEXT("FeatureFrameClose"), TEXT("clicked")); break;
	case EPhase::TrainOpen:
		Dispatch(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))), TEXT("BuildingFeatureButton"), TEXT("clicked")); break;
	case EPhase::TrainDetail: Dispatch(Feature.Get(), TEXT("TrainArmyDetailButton"), TEXT("clicked")); break;
	case EPhase::TrainTip: Dispatch(Feature.Get(), TEXT("TrainArmyDetailAttrButton_0"), TEXT("clicked")); break;
	case EPhase::TrainReturn:
		if (Dispatch(Feature.Get(), TEXT("TrainArmyDetailTipClickAway"), TEXT("clicked")))
			Dispatch(Feature.Get(), TEXT("TrainArmyDetailBackButton"), TEXT("clicked"));
		break;
	case EPhase::TrainClose: Dispatch(Feature.Get(), TEXT("FeatureFrameClose"), TEXT("clicked")); break;
	case EPhase::BuildingLevels:
	case EPhase::BuildingReopen: Dispatch(Modal.Get(), TEXT("BuildingLevelInfoButton"), TEXT("clicked")); break;
	case EPhase::BuildingScroll:
		if (auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BuildingLevelDetailsScroll")))) Scroll->ScrollToEnd();
		else Fail(TEXT("Missing actual building level ScrollBox."));
		break;
	case EPhase::BuildingReturn: Dispatch(Modal.Get(), TEXT("BuildingLevelBackButton"), TEXT("clicked")); break;
	case EPhase::BuildingClose: Modal->RequestCloseModal(); break;
	case EPhase::ScoutDescription: Dispatch(Scout.Get(), TEXT("ScoutInfoButton"), TEXT("clicked")); break;
	case EPhase::ScoutReturn: Dispatch(Scout.Get(), TEXT("ScoutBackButton"), TEXT("clicked")); break;
	case EPhase::ScoutClose:
		if (FParse::Param(FCommandLine::Get(), TEXT("RokScoutKeyboardAudit")))
		{
			if (!Scout->HasKeyboardFocus()) { Fail(TEXT("Scout did not acquire keyboard focus after opening.")); break; }
			TSharedRef<FJsonObject> Action = MakeShared<FJsonObject>();
			Action->SetStringField(TEXT("event"), TEXT("Slate_Escape_key_not_physical_keyboard"));
			Action->SetNumberField(TEXT("time_seconds"), ElapsedSeconds);
			Actions.Add(MakeShared<FJsonValueObject>(Action));
			if (!FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0)))
				Fail(TEXT("Focused Scout did not handle Escape through Slate input routing."));
		}
		else Dispatch(Scout.Get(), TEXT("ScoutCloseButton"), TEXT("clicked"));
		break;
	default: break;
	}
}

TSharedRef<FJsonObject> URokUiAnimationAuditSubsystem::ReadRenderState() const
{
	TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
	State->SetBoolField(TEXT("is_screen_open"), Feature.IsValid() && Feature->IsScreenOpen());
	State->SetBoolField(TEXT("task_reward_visual_fixture"), FParse::Param(FCommandLine::Get(), TEXT("RokTaskRewardPreview")));
	auto AddWidget = [&State](UUserWidget* Owner, const TCHAR* Name)
	{
		UWidget* Widget = Owner ? Owner->GetWidgetFromName(Name) : nullptr;
		if (!Widget)
		{
			State->SetField(Name, MakeShared<FJsonValueNull>());
			return;
		}
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		AddVector(Entry, TEXT("render_scale"), Widget->GetRenderTransform().Scale);
		AddVector(Entry, TEXT("render_translation"), Widget->GetRenderTransform().Translation);
		AddVector(Entry, TEXT("geometry_size"), Widget->GetCachedGeometry().GetLocalSize());
		Entry->SetNumberField(TEXT("render_angle"), Widget->GetRenderTransform().Angle);
		Entry->SetNumberField(TEXT("opacity"), Widget->GetRenderOpacity());
		Entry->SetNumberField(TEXT("visibility"), static_cast<int32>(Widget->GetVisibility()));
		if (const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			AddVector(Entry, TEXT("slot_size"), Slot->GetSize());
			AddVector(Entry, TEXT("slot_position"), Slot->GetPosition());
		}
		State->SetObjectField(Name, Entry);
	};
	AddWidget(Hud.Get(), TEXT("TaskListBackground"));
	AddWidget(Hud.Get(), TEXT("TaskListClip"));
	AddWidget(Hud.Get(), TEXT("TaskFoldButton"));
	AddWidget(Hud.Get(), TEXT("TaskReward_0"));
	AddWidget(Hud.Get(), TEXT("FeatureBackground"));
	AddWidget(Hud.Get(), TEXT("ItemsFeature"));
	AddWidget(Hud.Get(), TEXT("ItemsFeatureButton"));
	AddWidget(Hud.Get(), TEXT("MoreFeatureButton"));
	AddWidget(Feature.Get(), TEXT("FeatureModeCanvas"));
	AddWidget(Feature.Get(), TEXT("ChatContentCanvas"));
	AddWidget(Feature.Get(), TEXT("FeatureScreenDimmer"));
	AddWidget(Feature.Get(), TEXT("FeatureScreenInputBlocker"));
	AddWidget(Feature.Get(), TEXT("ChatAnimationInputShield"));
	AddWidget(Feature.Get(), TEXT("CommanderRosterScroll"));
	AddWidget(Feature.Get(), TEXT("CommanderRosterHeader"));
	AddWidget(Feature.Get(), TEXT("CommanderAnimatedPortrait"));
	AddWidget(Feature.Get(), TEXT("CommanderName"));
	AddWidget(Feature.Get(), TEXT("ArmyShowRulesScroll"));
	AddWidget(Feature.Get(), TEXT("ArmyShowListScroll"));
	AddWidget(Feature.Get(), TEXT("TrainArmyDetailRoot"));
	AddWidget(Feature.Get(), TEXT("TrainArmyStart"));
	AddWidget(Feature.Get(), TEXT("TrainParameterRoot"));
	AddWidget(Feature.Get(), TEXT("TrainParameterLeftArt"));
	AddWidget(Feature.Get(), TEXT("TrainArmyAnimatedModel"));
	AddWidget(Feature.Get(), TEXT("TrainArmyDetailButton"));
	if (auto* Mesh = Feature.IsValid() ? Cast<URokTrainArmyMeshWidget>(Feature->GetWidgetFromName(TEXT("TrainArmyAnimatedModel"))) : nullptr)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("arms_id"), Mesh->GetArmsId());
		Entry->SetNumberField(TEXT("load_generation"), Mesh->GetLoadGeneration());
		Entry->SetNumberField(TEXT("animation_time"), Mesh->GetAnimationTime());
		Entry->SetNumberField(TEXT("duration"), Mesh->GetAnimationDuration());
		Entry->SetNumberField(TEXT("paint_count"), Mesh->GetPaintCount());
		Entry->SetNumberField(TEXT("paint_frame"), Mesh->GetLastPaintFrameIndex());
		Entry->SetNumberField(TEXT("paint_draw_count"), Mesh->GetLastPaintDrawCount());
		Entry->SetBoolField(TEXT("same_instance_as_parameter_page"), TrainMesh.Get() == Mesh);
		State->SetObjectField(TEXT("train_source_mesh"), Entry);
	}
	AddWidget(Feature.Get(), TEXT("FeatureFrameBodyLeft"));
	AddWidget(Feature.Get(), TEXT("FeatureFrameBodyRight"));
	AddWidget(Feature.Get(), TEXT("FeatureFrameHeaderRight"));
	AddWidget(Feature.Get(), TEXT("FeatureFrameHeaderLeft"));
	AddWidget(Feature.Get(), TEXT("FeatureFrameTitle"));
	AddWidget(Feature.Get(), TEXT("TrainArmyDetailTipText"));
	AddWidget(Modal.Get(), TEXT("BuildingDescriptionRoot"));
	AddWidget(Modal.Get(), TEXT("BuildingLevelDetailsRoot"));
	AddWidget(Modal.Get(), TEXT("BuildingLevelDetailsScroll"));
	AddWidget(Scout.Get(), TEXT("ScoutMotionCanvas"));
	AddWidget(Scout.Get(), TEXT("ScoutDescriptionScroll"));
	AddWidget(Scout.Get(), TEXT("ScoutListScroll"));
	State->SetBoolField(TEXT("scout_preview_without_camp_actor_or_queue"), FParse::Param(FCommandLine::Get(), TEXT("RokScoutScreenAudit")));
	State->SetBoolField(TEXT("building_config_preview_without_city_actor"),
		FParse::Param(FCommandLine::Get(), TEXT("RokBuildingLevelDetailAudit")));
	if (auto* Scroll = Modal.IsValid() ? Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BuildingLevelDetailsScroll"))) : nullptr)
	{
		State->SetNumberField(TEXT("building_scroll_offset"), Scroll->GetScrollOffset());
		State->SetNumberField(TEXT("building_scroll_end"), Scroll->GetScrollOffsetOfEnd());
	}
	return State;
}

bool URokUiAnimationAuditSubsystem::Capture(const TCHAR* Name, float TargetSeconds, EPhase NextPhase, bool bEndPhase)
{
	if (ElapsedSeconds - PhaseStartedAt < TargetSeconds) return false;
	if (FScreenshotRequest::IsScreenshotRequested())
	{
		Fail(TEXT("Another screenshot request occupies the global slot; refusing to overwrite it."));
		return false;
	}
	const FString FileName = FString::Printf(TEXT("%02d_%s.png"), Captures.Num() + 1, Name);
	PendingScreenshotPath = OutputDirectory / FileName;
	PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("name"), Name);
	PendingCapture->SetStringField(TEXT("path"), PendingScreenshotPath);
	PendingCapture->SetNumberField(TEXT("target_phase_seconds"), TargetSeconds);
	PendingCapture->SetNumberField(TEXT("actual_phase_seconds"), ElapsedSeconds - PhaseStartedAt);
	PendingCapture->SetNumberField(TEXT("runtime_seconds"), ElapsedSeconds);
	PendingCapture->SetNumberField(TEXT("request_audit_frame"), static_cast<double>(AuditFrame));
	PendingCapture->SetNumberField(TEXT("last_tick_delta_seconds"), LastDeltaSeconds);
	PendingCapture->SetObjectField(TEXT("state_at_request"), ReadRenderState());
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture));
	bScreenshotPending = true;
	PendingScreenshotStartedAt = ElapsedSeconds;
	bAdvancePhaseAfterCapture = bEndPhase;
	PhaseAfterCapture = NextPhase;
	++PhaseCaptureIndex;
	FScreenshotRequest::RequestScreenshot(PendingScreenshotPath, true, false, false, FIntRect(), true);
	UE_LOG(LogTemp, Display, TEXT("Rok UI animation audit capture: %s phase_time=%.6f target=%.6f frame=%llu"),
		Name, ElapsedSeconds - PhaseStartedAt, TargetSeconds, AuditFrame);
	return true;
}

void URokUiAnimationAuditSubsystem::OnScreenshotProcessed()
{
	if (!bScreenshotPending || !PendingCapture.IsValid()) return;
	bScreenshotPending = false;
	if ((Phase == EPhase::TrainOpen || Phase == EPhase::TrainReturn || Phase == EPhase::TrainDetail || Phase == EPhase::TrainTip)
		&& !ValidateTrainFixedPresentation(Phase == EPhase::TrainDetail || Phase == EPhase::TrainTip)) return;
	PendingCapture->SetNumberField(TEXT("processed_audit_frame"), static_cast<double>(AuditFrame));
	PendingCapture->SetObjectField(TEXT("state_at_processed"), ReadRenderState());
	const int64 FileSize = IFileManager::Get().FileSize(*PendingScreenshotPath);
	PendingCapture->SetNumberField(TEXT("file_bytes"), static_cast<double>(FileSize));
	PendingCapture->SetBoolField(TEXT("processed"), true);
	if (FileSize <= 0)
	{
		Fail(FString::Printf(TEXT("Screenshot processed but file is missing/empty: %s"), *PendingScreenshotPath));
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("Rok UI animation audit saved: %s bytes=%lld"), *PendingScreenshotPath, FileSize);
	PendingCapture.Reset();
	if (!WriteManifest(false)) Fail(TEXT("Could not save incremental audit manifest."));
}

bool URokUiAnimationAuditSubsystem::ValidateTrainFixedPresentation(bool bDetail)
{
	auto* Animated = Feature->GetWidgetFromName(bDetail ? TEXT("TrainArmyDetailRoot") : TEXT("TrainParameterRoot"));
	if (!Animated) { Fail(TEXT("Train page animation root is missing.")); return false; }
	auto* Mesh = Cast<URokTrainArmyMeshWidget>(Feature->GetWidgetFromName(TEXT("TrainArmyAnimatedModel")));
	if (!Mesh || !Mesh->IsModelLoaded() || Mesh->GetFrameCount() < 2 || Mesh->GetPaintCount() == 0
		|| Mesh->GetLastPaintFrameIndex() < 0 || Mesh->GetLastPaintDrawCount() == 0)
	{ Fail(TEXT("Train source model was not loaded and actually painted.")); return false; }
	if (TrainMesh.IsValid())
	{
		if (TrainMesh.Get() != Mesh || Mesh->GetLoadGeneration() != TrainMeshGeneration || Mesh->GetArmsId() != TrainMeshArmsId)
		{ Fail(TEXT("Train source model instance or load state changed across parameter/detail pages.")); return false; }
	}
	else
	{
		TrainMesh = Mesh; TrainMeshGeneration = Mesh->GetLoadGeneration(); TrainMeshArmsId = Mesh->GetArmsId();
	}
	for (const FName Key : {FName(TEXT("FeatureFrameBodyLeft")), FName(TEXT("FeatureFrameHeaderLeft")),
		FName(TEXT("FeatureFrameBodyRight")), FName(TEXT("FeatureFrameHeaderRight")),
		FName(TEXT("FeatureFrameTitle")), FName(TEXT("TrainParameterLeftArt")), FName(TEXT("TrainArmyAnimatedModel"))})
	{
		const FName Name = Key;
		auto* Fixed = Feature->GetWidgetFromName(Name);
		auto* Slot = Fixed ? Cast<UCanvasPanelSlot>(Fixed->Slot) : nullptr;
		if (!Slot || Fixed->GetParent() != Feature->GetWidgetFromName(TEXT("FeatureModeCanvas"))
			|| Fixed->GetVisibility() == ESlateVisibility::Collapsed || Fixed->GetVisibility() == ESlateVisibility::Hidden
			|| !FMath::IsNearlyEqual(Fixed->GetRenderOpacity(), 1.0f)
			|| !Fixed->GetRenderTransform().Translation.IsNearlyZero())
		{ Fail(FString::Printf(TEXT("Train shared presentation missing or animated: %s"), *Name.ToString())); return false; }
		for (UWidget* Parent = Fixed->GetParent(); Parent; Parent = Parent->GetParent())
			if (Parent == Animated)
			{ Fail(FString::Printf(TEXT("Train shared presentation is inside the page animator: %s"), *Name.ToString())); return false; }
		if (const auto* Position = TrainFixedPositions.Find(Key))
		{
			if (!Position->Equals(Slot->GetPosition(), .01) || !TrainFixedSizes.FindChecked(Key).Equals(Slot->GetSize(), .01))
			{ Fail(FString::Printf(TEXT("Train shared presentation shifts across pages: %s"), *Name.ToString())); return false; }
		}
		else
		{
			TrainFixedPositions.Add(Key, Slot->GetPosition()); TrainFixedSizes.Add(Key, Slot->GetSize());
		}
	}
	return true;
}

void URokUiAnimationAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	ElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	LastDeltaSeconds = DeltaTime;
	++AuditFrame;
	if (ElapsedSeconds > 45.0)
	{
		Fail(TEXT("Audit exceeded the 45-second runtime timeout."));
		return;
	}
	if (bScreenshotPending)
	{
		if (ElapsedSeconds - PendingScreenshotStartedAt > 5.0)
		{
			Fail(TEXT("Screenshot request was not processed within five seconds."));
		}
		return;
	}
	if (bAdvancePhaseAfterCapture)
	{
		bAdvancePhaseAfterCapture = false;
		EnterPhase(PhaseAfterCapture);
		return;
	}
	if (Phase != EPhase::Waiting && (!Hud.IsValid() || !Feature.IsValid()))
	{
		Fail(TEXT("Runtime HUD or shared feature host was destroyed during the audit."));
		return;
	}
	switch (Phase)
	{
	case EPhase::Waiting:
		if (ElapsedSeconds >= 6.0 && FindRuntimeWidgets()) EnterPhase(
			FParse::Param(FCommandLine::Get(), TEXT("RokScoutScreenAudit")) ? EPhase::ScoutInitial :
			FParse::Param(FCommandLine::Get(), TEXT("RokTrainArmyDetailAudit")) ? EPhase::TrainOpen :
			FParse::Param(FCommandLine::Get(), TEXT("RokBuildingLevelDetailAudit")) ? EPhase::BuildingDescription :
			FParse::Param(FCommandLine::Get(), TEXT("RokArmyShowAudit")) ? EPhase::ArmyOpen :
			FParse::Param(FCommandLine::Get(), TEXT("RokCommanderRosterAudit")) ? EPhase::CommanderOpen : EPhase::TaskClose);
		break;
	case EPhase::TaskClose:
		if (PhaseCaptureIndex == 0) Capture(TEXT("task_mid"), 0.16f, Phase, false);
		else Capture(TEXT("task_collapsed"), 0.40f, EPhase::TaskOpen, true);
		break;
	case EPhase::TaskOpen: Capture(TEXT("task_open"), 0.40f, EPhase::MoreClose, true); break;
	case EPhase::MoreClose:
		if (PhaseCaptureIndex == 0) Capture(TEXT("more_mid"), 0.12f, Phase, false);
		else Capture(TEXT("more_collapsed"), 0.30f, EPhase::MoreOpen, true);
		break;
	case EPhase::MoreOpen: Capture(TEXT("more_open"), 0.30f, EPhase::ButtonPress, true); break;
	case EPhase::ButtonPress: Capture(TEXT("button_pressed"), 0.05f, EPhase::ButtonRelease, true); break;
	case EPhase::ButtonRelease: Capture(TEXT("button_released_peak"), 0.083333f, EPhase::ItemsOpen, true); break;
	case EPhase::ItemsOpen:
		if (PhaseCaptureIndex == 0) Capture(TEXT("items_entering"), 0.10f, Phase, false);
		else Capture(TEXT("items_open"), 0.30f, EPhase::ItemsClose, true);
		break;
	case EPhase::ItemsClose:
		if (PhaseCaptureIndex == 0) Capture(TEXT("items_exiting"), 0.075f, Phase, false);
		else Capture(TEXT("items_closed"), 0.20f,
			FParse::Param(FCommandLine::Get(), TEXT("RokChatAnimationAudit")) ? EPhase::ChatOpen : EPhase::Finish, true);
		break;
	case EPhase::ChatOpen:
		if (PhaseCaptureIndex == 0) Capture(TEXT("chat_entering"), 0.16f, Phase, false);
		else Capture(TEXT("chat_open"), 0.40f, EPhase::ContactsHide, true);
		break;
	case EPhase::ContactsHide:
		if (PhaseCaptureIndex == 0) Capture(TEXT("contacts_hiding"), 0.10f, Phase, false);
		else Capture(TEXT("contacts_hidden"), 0.25f, EPhase::ContactsShow, true);
		break;
	case EPhase::ContactsShow: Capture(TEXT("contacts_shown"), 0.25f, EPhase::ChatChannel, true); break;
	case EPhase::ChatChannel: Capture(TEXT("chat_channel"), 0.10f, EPhase::ChatEmoji, true); break;
	case EPhase::ChatEmoji: Capture(TEXT("chat_emoji"), 0.10f, EPhase::ChatClose, true); break;
	case EPhase::ChatClose:
		if (PhaseCaptureIndex == 0) Capture(TEXT("chat_exiting"), 0.16f, Phase, false);
		else Capture(TEXT("chat_closed"), 0.40f, EPhase::Finish, true);
		break;
	case EPhase::CommanderOpen:
		if (PhaseCaptureIndex == 0)
		{
			// Resource loading can delay the route. Capture an actual intermediate
			// pose, not merely a wall-clock timestamp after dispatch.
			UWidget* List = Feature->GetWidgetFromName(FParse::Param(FCommandLine::Get(), TEXT("RokCommanderDataAudit"))
				? TEXT("CommanderName") : TEXT("CommanderRosterScroll"));
			if (List && List->GetRenderOpacity() > 0.05f && List->GetRenderOpacity() < 0.95f)
				Capture(TEXT("commander_roster_entering"), 0.0f, Phase, false);
		}
		else Capture(TEXT("commander_roster_open"), 0.8f, EPhase::CommanderSort, true);
		break;
	case EPhase::CommanderSort: Capture(TEXT("commander_sort_preserved"), 0.1f, EPhase::CommanderClose, true); break;
	case EPhase::CommanderClose: Capture(TEXT("commander_closed"), 0.4f, EPhase::Finish, true); break;
	case EPhase::ArmyOpen: Capture(TEXT("army_open"), 0.5f, EPhase::ArmyRules, true); break;
	case EPhase::ArmyRules:
	case EPhase::ArmyList:
		if (PhaseCaptureIndex == 0)
		{
			const bool bRules = Phase == EPhase::ArmyRules;
			UWidget* Panel = Feature->GetWidgetFromName(bRules ? TEXT("ArmyShowRulesScroll") : TEXT("ArmyShowListScroll"));
			if (Panel && Panel->GetRenderOpacity() > 0.05f && Panel->GetRenderOpacity() < 0.95f)
				Capture(bRules ? TEXT("army_rules_entering") : TEXT("army_list_entering"), 0.0f, Phase, false);
		}
		else if (Phase == EPhase::ArmyRules) Capture(TEXT("army_rules_open"), 0.4f, EPhase::ArmyList, true);
		else Capture(TEXT("army_list_open"), 0.4f, EPhase::ArmyClose, true);
		break;
	case EPhase::ArmyClose: Capture(TEXT("army_closed"), 0.4f, EPhase::Finish, true); break;
	case EPhase::TrainOpen:
		if (ElapsedSeconds - PhaseStartedAt < 0.6) break;
		if (ValidateTrainFixedPresentation(false)) Capture(TEXT("train_parameters"), 0.6f, EPhase::TrainDetail, true);
		break;
	case EPhase::TrainDetail:
	case EPhase::TrainReturn:
	{
		const bool bDetail = Phase == EPhase::TrainDetail;
		// Button actions rebuild on the real widget tick; wait for that page root.
		if (!Feature->GetWidgetFromName(bDetail ? TEXT("TrainArmyDetailRoot") : TEXT("TrainParameterRoot"))) break;
		if (!ValidateTrainFixedPresentation(bDetail)) return;
		if (PhaseCaptureIndex == 0)
		{
			UWidget* Panel = Feature->GetWidgetFromName(bDetail ? TEXT("TrainArmyDetailRoot") : TEXT("TrainParameterRoot"));
			if (!Panel) { Fail(TEXT("Train page animation root is missing.")); return; }
			if (!bDetail)
			{
				for (const TCHAR* Name : {TEXT("TrainParameterLeftArt"), TEXT("TrainArmyAnimatedModel"), TEXT("TrainArmyDetailButton")})
				{
					auto* Fixed = Feature->GetWidgetFromName(Name);
					if (!Fixed || Fixed->GetParent() == Panel || !FMath::IsNearlyEqual(Fixed->GetRenderOpacity(), 1.0f)
						|| !Fixed->GetRenderTransform().Translation.IsNearlyZero())
					{ Fail(FString::Printf(TEXT("Train return animates fixed left/control widget %s"), Name)); return; }
				}
			}
			if (Panel->GetRenderOpacity() > 0.05f && Panel->GetRenderOpacity() < 0.95f)
				Capture(bDetail ? TEXT("train_detail_entering") : TEXT("train_parameters_returning"), 0, Phase, false);
		}
		else Capture(bDetail ? TEXT("train_detail_open") : TEXT("train_parameters_returned"), 0.4f,
			bDetail ? EPhase::TrainTip : EPhase::TrainClose, true);
		break;
	}
	case EPhase::TrainTip: Capture(TEXT("train_attribute_tip"), 0.3f, EPhase::TrainReturn, true); break;
	case EPhase::TrainClose: Capture(TEXT("train_closed"), 0.4f, EPhase::Finish, true); break;
	case EPhase::BuildingDescription: Capture(TEXT("building_description_reference"), 0.4f, EPhase::BuildingLevels, true); break;
	case EPhase::BuildingLevels:
		if (PhaseCaptureIndex == 0)
		{
			UWidget* Panel = Modal->GetWidgetFromName(TEXT("BuildingLevelDetailsRoot"));
			if (Panel && Panel->GetRenderOpacity() > 0.05f && Panel->GetRenderOpacity() < 0.95f)
				Capture(TEXT("building_levels_entering"), 0, Phase, false);
		}
		else Capture(TEXT("building_levels_open"), 0.4f, EPhase::BuildingScroll, true);
		break;
	case EPhase::BuildingScroll:
		if (ElapsedSeconds - PhaseStartedAt >= 0.3f)
		{
			auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BuildingLevelDetailsScroll")));
			if (!Scroll || Scroll->GetScrollOffsetOfEnd() <= 1 ||
				!FMath::IsNearlyEqual(Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd(), 1.0f))
				Fail(TEXT("Building level list did not scroll to a real overflow endpoint."));
			else
			{
				BuildingScrollOffsetBeforeReturn = Scroll->GetScrollOffset();
				Capture(TEXT("building_levels_scrolled"), 0.3f, EPhase::BuildingReturn, true);
			}
		}
		break;
	case EPhase::BuildingReturn:
		if (PhaseCaptureIndex == 0) Capture(TEXT("building_description_returning"), 0.08f, Phase, false);
		else Capture(TEXT("building_description_returned"), 0.4f, EPhase::BuildingReopen, true);
		break;
	case EPhase::BuildingReopen:
		if (ElapsedSeconds - PhaseStartedAt >= 0.4f)
		{
			auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BuildingLevelDetailsScroll")));
			// Source ShowLevelInfo returns early after its first FillContent.
			if (!Scroll || BuildingScrollOffsetBeforeReturn <= 0 ||
				!FMath::IsNearlyEqual(Scroll->GetScrollOffset(), BuildingScrollOffsetBeforeReturn, 1.0f))
				Fail(TEXT("Reopening building levels did not preserve the source scroll position."));
			else Capture(TEXT("building_levels_reopened"), 0.4f, EPhase::BuildingClose, true);
		}
		break;
	case EPhase::BuildingClose: Capture(TEXT("building_reference_closed"), 0.4f, EPhase::Finish, true); break;
	case EPhase::ScoutInitial: Capture(TEXT("scout_queue_unknown"), 0.3f, EPhase::ScoutDescription, true); break;
	case EPhase::ScoutDescription:
	case EPhase::ScoutReturn:
	{
		const bool bDescription = Phase == EPhase::ScoutDescription;
		if (PhaseCaptureIndex == 0)
		{
			UWidget* Panel = Scout->GetWidgetFromName(bDescription ? TEXT("ScoutDescriptionScroll") : TEXT("ScoutListScroll"));
			if (Panel && Panel->GetRenderOpacity() > 0.05f && Panel->GetRenderOpacity() < 0.95f)
				Capture(bDescription ? TEXT("scout_description_entering") : TEXT("scout_list_returning"), 0, Phase, false);
		}
		else Capture(bDescription ? TEXT("scout_description_open") : TEXT("scout_list_returned"), 0.4f,
			bDescription ? EPhase::ScoutReturn : EPhase::ScoutClose, true);
		break;
	}
	case EPhase::ScoutClose: Capture(TEXT("scout_closed"), 0.4f, EPhase::Finish, true); break;
	case EPhase::Finish:
		if (++FinishFrames >= 2) Finish();
		break;
	}
}

bool URokUiAnimationAuditSubsystem::WriteManifest(bool bComplete)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("world"), GetWorld()->GetPathName());
	Root->SetStringField(TEXT("project_dir"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("input_method"), TEXT("real_UMG_delegates_not_physical_mouse"));
	Root->SetStringField(TEXT("timing_note"), TEXT("Real runtime ticks only. Requested time is a lower bound; frame quantization is recorded. State is sampled before and after the UI screenshot frame; no testing Advance methods are used."));
	Root->SetStringField(TEXT("output_dir"), OutputDirectory);
	Root->SetBoolField(TEXT("complete"), bComplete);
	const int32 ExpectedCaptures = ExpectedCaptureCount();
	Root->SetBoolField(TEXT("success"), bComplete && Failures.IsEmpty() && Captures.Num() == ExpectedCaptures);
	Root->SetNumberField(TEXT("runtime_seconds"), ElapsedSeconds);
	Root->SetArrayField(TEXT("captures"), Captures);
	Root->SetArrayField(TEXT("actions"), Actions);
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Failure : Failures) Errors.Add(MakeShared<FJsonValueString>(Failure));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer)) return false;
	const bool bRunSaved = FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bLatestSaved = FFileHelper::SaveStringToFile(Json,
		*(FPaths::GetPath(OutputDirectory) / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return bRunSaved && bLatestSaved;
}

void URokUiAnimationAuditSubsystem::Fail(const FString& Reason)
{
	if (bFinished) return;
	Failures.Add(Reason);
	bFinished = true;
	UE_LOG(LogTemp, Error, TEXT("Rok UI animation audit FAILED: %s"), *Reason);
	if (!WriteManifest(false)) UE_LOG(LogTemp, Error, TEXT("Rok UI animation audit could not write failure manifest."));
	FGenericPlatformMisc::RequestExit(false);
}

void URokUiAnimationAuditSubsystem::Finish()
{
	const int32 ExpectedCaptures = ExpectedCaptureCount();
	if (Captures.Num() != ExpectedCaptures || (Feature.IsValid() && Feature->IsScreenOpen())
		|| (FParse::Param(FCommandLine::Get(), TEXT("RokScoutScreenAudit")) && Scout.IsValid() && Scout->IsScreenOpen())
		|| (FParse::Param(FCommandLine::Get(), TEXT("RokBuildingLevelDetailAudit")) && Modal.IsValid() && Modal->IsModalOpen()))
	{
		Fail(TEXT("Final state must contain all requested screenshots and a closed feature window."));
		return;
	}
	if (!WriteManifest(true))
	{
		Fail(TEXT("Could not save completed audit manifest."));
		return;
	}
	bFinished = true;
	UE_LOG(LogTemp, Display, TEXT("Rok UI animation audit COMPLETE: screenshots=%d manifest=%s input=UMG_delegate_not_physical."), Captures.Num(),
		*(OutputDirectory / TEXT("manifest.json")));
	FGenericPlatformMisc::RequestExit(false);
}
