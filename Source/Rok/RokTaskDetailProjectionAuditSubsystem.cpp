#include "RokTaskDetailProjectionAuditSubsystem.h"

#include "RokBuildingPanelWidget.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTaskRewardPreviewData.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokTaskDetailProjectionAudit");
constexpr TCHAR RewardAtlasPath[] = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}
int32 TaggedInt(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return 0;
	for (const FName Tag : Actor->Tags)
	{
		const FString Text = Tag.ToString();
		if (Text.StartsWith(Prefix)) return FCString::Atoi(*Text.Mid(FCString::Strlen(Prefix)));
	}
	return 0;
}
bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	return true;
}
TArray<TSharedPtr<FJsonValue>> RewardPreviewRows(const URokUnityFeatureScreenWidget* Screen)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	TArray<FRokTaskRewardPreviewRow> SourceRows;
	FString Error;
	if (!Screen || !RokTaskRewardPreviewData::LoadGroup(200900, SourceRows, Error)) return Result;
	for (int32 Index = 0; Index < SourceRows.Num(); ++Index)
	{
		const FString Base = FString::Printf(TEXT("TaskRewardPreview_0_%d"), Index);
		const UWidget* Cell = Screen->GetWidgetFromName(*Base);
		const UImage* Icon = Cast<UImage>(Screen->GetWidgetFromName(*(Base + TEXT("Icon"))));
		const UTextBlock* Count = Cast<UTextBlock>(Screen->GetWidgetFromName(*(Base + TEXT("Count"))));
		const FSlateBrush* Brush = Icon ? &Icon->GetBrush() : nullptr;
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("selector"), Base);
		Row->SetNumberField(TEXT("currency_id"), SourceRows[Index].CurrencyId);
		Row->SetNumberField(TEXT("quantity"), static_cast<double>(SourceRows[Index].Quantity));
		Row->SetStringField(TEXT("display_name"), SourceRows[Index].DisplayName);
		Row->SetBoolField(TEXT("exists"), Cell != nullptr);
		Row->SetBoolField(TEXT("visible"), Visible(Cell));
		Row->SetBoolField(TEXT("icon_exists"), Icon != nullptr);
		Row->SetBoolField(TEXT("icon_visible"), Visible(Icon));
		Row->SetStringField(TEXT("icon_resource_path"), Brush && Brush->GetResourceObject()
			? Brush->GetResourceObject()->GetPathName() : FString());
		if (Brush)
		{
			const FBox2f UV = Brush->GetUVRegion();
			TArray<TSharedPtr<FJsonValue>> Min = { MakeShared<FJsonValueNumber>(UV.Min.X), MakeShared<FJsonValueNumber>(UV.Min.Y) };
			TArray<TSharedPtr<FJsonValue>> Max = { MakeShared<FJsonValueNumber>(UV.Max.X), MakeShared<FJsonValueNumber>(UV.Max.Y) };
			Row->SetArrayField(TEXT("icon_uv_min"), Min);
			Row->SetArrayField(TEXT("icon_uv_max"), Max);
		}
		else
		{
			const TArray<TSharedPtr<FJsonValue>> Empty;
			Row->SetArrayField(TEXT("icon_uv_min"), Empty);
			Row->SetArrayField(TEXT("icon_uv_max"), Empty);
		}
		Row->SetBoolField(TEXT("count_exists"), Count != nullptr);
		Row->SetBoolField(TEXT("count_visible"), Visible(Count));
		Row->SetStringField(TEXT("count_text"), Count ? Count->GetText().ToString() : FString());
		Row->SetStringField(TEXT("tooltip"), Cell ? Cell->GetToolTipText().ToString() : FString());
		Result.Add(MakeShared<FJsonValueObject>(Row));
	}
	return Result;
}
bool ValidateRewardPreviewWidgets(const URokUnityFeatureScreenWidget* Screen)
{
	TArray<FRokTaskRewardPreviewRow> SourceRows;
	FString Error;
	if (!Screen || !RokTaskRewardPreviewData::LoadGroup(200900, SourceRows, Error)
		|| SourceRows.Num() != 3 || Screen->GetWidgetFromName(TEXT("TaskRewardGroup_0")) != nullptr)
	{
		return false;
	}
	const int32 ExpectedIds[] = { 100, 101, 104 };
	const int64 ExpectedAmounts[] = { 2000, 2000, 50 };
	const TCHAR* ExpectedCounts[] = { TEXT("2,000"), TEXT("2,000"), TEXT("50") };
	const TCHAR* ExpectedNames[] = { TEXT("食物"), TEXT("木材"), TEXT("钻石") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FString Base = FString::Printf(TEXT("TaskRewardPreview_0_%d"), Index);
		const UWidget* Cell = Screen->GetWidgetFromName(*Base);
		const UImage* Icon = Cast<UImage>(Screen->GetWidgetFromName(*(Base + TEXT("Icon"))));
		const UTextBlock* Count = Cast<UTextBlock>(Screen->GetWidgetFromName(*(Base + TEXT("Count"))));
		if (!Visible(Cell) || !Visible(Icon) || !Visible(Count)
			|| SourceRows[Index].CurrencyId != ExpectedIds[Index]
			|| SourceRows[Index].Quantity != ExpectedAmounts[Index]
			|| SourceRows[Index].DisplayName != ExpectedNames[Index]
			|| Count->GetText().ToString() != ExpectedCounts[Index]
			|| !Cell->GetToolTipText().ToString().Contains(ExpectedNames[Index])
			|| !Cell->GetToolTipText().ToString().Contains(ExpectedCounts[Index])
			|| !Cell->GetToolTipText().ToString().Contains(TEXT("服务器状态待同步"))) return false;
		const FSlateBrush& Brush = Icon->GetBrush();
		if (!Brush.GetResourceObject() || Brush.GetResourceObject()->GetPathName() != RewardAtlasPath) return false;
		const FBox2f UV = Brush.GetUVRegion();
		const FVector2f ExpectedMin(
			static_cast<float>(SourceRows[Index].IconRect.Min.X) / SourceRows[Index].IconAtlasSize.X,
			1.0f - static_cast<float>(SourceRows[Index].IconRect.Max.Y) / SourceRows[Index].IconAtlasSize.Y);
		const FVector2f ExpectedMax(
			static_cast<float>(SourceRows[Index].IconRect.Max.X) / SourceRows[Index].IconAtlasSize.X,
			1.0f - static_cast<float>(SourceRows[Index].IconRect.Min.Y) / SourceRows[Index].IconAtlasSize.Y);
		if (!UV.Min.Equals(ExpectedMin, 0.00001f) || !UV.Max.Equals(ExpectedMax, 0.00001f)) return false;
	}
	return true;
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
}

bool URokTaskDetailProjectionAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(), TEXT("RokTaskDetailProjectionAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokTaskDetailProjectionAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}
TStatId URokTaskDetailProjectionAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokTaskDetailProjectionAuditSubsystem, STATGROUP_Tickables);
}
void URokTaskDetailProjectionAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/TaskDetailProjectionAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokTaskDetailProjectionAuditSubsystem::ScreenshotProcessed);
}
void URokTaskDetailProjectionAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokTaskDetailProjectionAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokTaskDetailProjectionAuditSubsystem::Expect(bool Condition, const TCHAR* Error)
{
	if (!Condition) Finish(Error);
	return Condition;
}

bool URokTaskDetailProjectionAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(), TEXT("Wrong project/world or Slate unavailable"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 HallFlagCount = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(TEXT("-RokAutoSelectBuilding=UnityInitId_5"), ESearchCase::IgnoreCase)) { ++HallFlagCount; continue; }
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) continue;
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& !Token.Equals(AuditFlag, ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok diagnostic/fixture flag"));
	}
	if (!Expect(HallFlagCount == 1, TEXT("Hall auto-selection flag missing or duplicate"))) return false;
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
	Panel = Hud.IsValid() ? Cast<URokBuildingPanelWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))) : nullptr;
	Modal = Hud.IsValid() ? Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget"))) : nullptr;
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	if (!Expect(Controller.IsValid() && Hud.IsValid() && Panel.IsValid() && Modal.IsValid()
		&& Screen.IsValid() && Buildings.IsValid() && Runtime.IsValid() && Server.IsValid()
		&& Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0 && Runtime->IsRuntimeStateLoaded()
		&& !Screen->IsScreenOpen() && !Modal->IsModalOpen()
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), 0.1f),
		TEXT("Expected idle 1440x810 local GameApp HUD"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Account profile is not local preview"))) return false;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (TaggedInt(*It, TEXT("UnityInitId_")) == 5)
		{
			if (Hall.IsValid()) return Expect(false, TEXT("Duplicate UnityInitId_5"));
			Hall = *It;
		}
	const auto Selected = Controller->GetCityPlacementPreviewSnapshot();
	const auto HallState = Buildings->GetBuildingStateForActor(Hall.Get());
	const auto Task = Runtime->GetActiveTask();
	const auto Projection = Runtime->GetActiveTaskLocalProjection(&HallState);
	FRokBuildingUpgradePreview Preview;
	if (!Expect(Hall.IsValid() && Selected.CurrentSelection.Get() == Hall.Get()
		&& HallState.UnityInitId == 5 && HallState.UnityBuildingTypeId == 1 && HallState.Level == 1
		&& !HallState.bUpgradeInProgress && Buildings->GetActiveUpgradeCount() == 0
		&& Task.TaskId == 200900 && Task.Current == 1 && Task.Target == 2 && !Task.bComplete
		&& Projection.bProjected && Projection.Task.Current == 1 && !Projection.bConditionMet
		&& Buildings->GetUpgradePreview(Hall.Get(), Preview) && Preview.bCanStartTimedUpgrade
		&& Preview.BuildSeconds == 2 && Preview.Cost.Food == 3500 && Preview.Cost.Wood == 3500,
		TEXT("Hall L1/Task 200900 local prerequisite differs"))) return false;
	CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(this,
		&URokTaskDetailProjectionAuditSubsystem::OnUpgradeCompleted);
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokTaskDetailProjectionAuditSubsystem.h"),
		TEXT("Source/Rok/RokTaskDetailProjectionAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokTaskDetailProjectionRuntime.py"),
		TEXT("Scripts/VerifyRokTaskRewardPreviewRuntime.py"),
		TEXT("Scripts/BuildRokUnityTaskRewardPreview.py"),
		TEXT("Scripts/VerifyRokUnityTaskRewardPreview.py"),
		TEXT("Saved/RokUnityMigration/task_reward_preview.json"),
		TEXT("Source/Rok/RokTaskRewardPreviewData.h"),
		TEXT("Source/Rok/RokTaskRewardPreviewData.cpp"),
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
		if (!Expect(Hash.Len() == 64, TEXT("Task audit source input missing"))) return false;
		Inputs->SetStringField(Path, Hash);
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
	if (!Expect(bOwn, TEXT("Task audit requires own 1440x810 GameApp window"))) return false;
	if (::IsIconic(Handle)) ::ShowWindow(Handle, SW_RESTORE);
	else if (!::IsWindowVisible(Handle)) ::ShowWindow(Handle, SW_SHOW);
	::SetForegroundWindow(Handle);
	if (!Expect(::GetForegroundWindow() == Handle, TEXT("Task audit game window not foreground"))) return false;
#else
	return Expect(false, TEXT("Task audit requires Windows game window"));
#endif
	return true;
}

void URokTaskDetailProjectionAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep;
	StepSince = Elapsed;
}
void URokTaskDetailProjectionAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted& Completed)
{
	if (Completed.UnityInitId == 5 && Completed.UnityBuildingTypeId == 1
		&& Completed.PreviousLevel == 1 && Completed.CurrentLevel == 2
		&& Completed.PreviousPower == 7 && Completed.CurrentPower == 21)
	{
		++UpgradeCompletionCount;
		UpgradeCompletedWall = FPlatformTime::Seconds();
	}
	else Finish(TEXT("Unexpected upgrade completion event"));
}

bool URokTaskDetailProjectionAuditSubsystem::Click(UWidget* Target, const TCHAR* Label)
{
	if (!Expect(Identity() && Target && Visible(Target) && Target->GetIsEnabled()
		&& FSlateApplication::IsInitialized(), TEXT("Task Slate target unavailable"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("Task target has no cached geometry"))) return false;
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30.0f, -30.0f));
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
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
		TEXT("Task Slate hit did not terminate at own-window UMG SButton"))) return false;
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
	return Expect(Down && Up && After.PrimaryPressEventCount == Before.PrimaryPressEventCount,
		TEXT("Task UI click was unhandled or passed into world press"));
}

bool URokTaskDetailProjectionAuditSubsystem::ValidateState(int32 HallLevel, int32 DisplayCurrent, bool bScreenOpen) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!Hall.IsValid() || !Buildings.IsValid() || !Runtime.IsValid() || !Screen.IsValid() || !Server.IsValid()) return false;
	const auto HallState = Buildings->GetBuildingStateForActor(Hall.Get());
	const auto Task = Runtime->GetActiveTask();
	const auto Projection = Runtime->GetActiveTaskLocalProjection(&HallState);
	const auto Profile = Runtime->GetPlayerProfileState();
	if (HallState.Level != HallLevel || (HallLevel == 2 && HallState.bUpgradeInProgress)
		|| Task.TaskId != 200900 || Task.Current != 1 || Task.Target != 2 || Task.bComplete
		|| Projection.Task.Current != DisplayCurrent || !Projection.bProjected
		|| Projection.bConditionMet != (DisplayCurrent == 2)
		|| Profile.StateKind != TEXT("local_preview") || Profile.bHasRoleId || !Profile.RoleId.IsEmpty()
		|| Profile.bHasCivilization || Profile.CivilizationId != 0 || !Profile.CivilizationName.IsEmpty()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| Screen->IsScreenOpen() != bScreenOpen) return false;
	if (!bScreenOpen) return true;
	const auto* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskTitle_0")));
	const auto* Desc = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskDescription_0")));
	const auto* Action = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
	const auto* ActionLabel = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskAction_0Label")));
	const UWidget* Scroll = Screen->GetWidgetFromName(TEXT("TaskMainScroll"));
	return Visible(Title) && Visible(Desc) && Visible(Action) && Visible(ActionLabel) && Visible(Scroll)
		&& Screen->IsTaskDetailLocalProjectionForAudit()
		&& Screen->IsTaskDetailLocalConditionMetForAudit() == (DisplayCurrent == 2)
		&& Screen->GetTaskDetailDisplayedCurrentForAudit() == DisplayCurrent
		&& Action->GetIsEnabled() && ActionLabel->GetText().ToString() == TEXT("前往")
		&& Title->GetText().ToString().Contains(TEXT("本地"))
		&& Desc->GetText().ToString().Contains(
			FString::Printf(TEXT("(%d/2)"), DisplayCurrent))
		&& !Desc->GetText().ToString().Contains(TEXT("领取奖励"))
		&& Desc->GetToolTipText().ToString().Contains(TEXT("服务器任务状态待同步"))
		&& ValidateRewardPreviewWidgets(Screen.Get());
#endif
}

TSharedRef<FJsonObject> URokTaskDetailProjectionAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Buildings.IsValid() || !Runtime.IsValid() || !Server.IsValid()) return Json;
	const auto HallState = Buildings->GetBuildingStateForActor(Hall.Get());
	const auto Task = Runtime->GetActiveTask();
	const auto Projection = Runtime->GetActiveTaskLocalProjection(&HallState);
	const auto Profile = Runtime->GetPlayerProfileState();
	const auto Wallet = Buildings->GetCurrentResources();
	Json->SetNumberField(TEXT("hall_level"), HallState.Level);
	Json->SetBoolField(TEXT("hall_upgrade_in_progress"), HallState.bUpgradeInProgress);
	Json->SetNumberField(TEXT("active_upgrade_count"), Buildings->GetActiveUpgradeCount());
	Json->SetNumberField(TEXT("upgrade_completion_events"), UpgradeCompletionCount);
	Json->SetNumberField(TEXT("wallet_food"), Wallet.Food);
	Json->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
	Json->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
	Json->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
	Json->SetNumberField(TEXT("bootstrap_task_id"), Task.TaskId);
	Json->SetStringField(TEXT("bootstrap_task_name"), Task.Name);
	Json->SetStringField(TEXT("bootstrap_task_description"), Task.Desc);
	Json->SetNumberField(TEXT("bootstrap_task_type"), Task.Type);
	Json->SetNumberField(TEXT("bootstrap_task_param1"), Task.Param1);
	Json->SetNumberField(TEXT("bootstrap_task_param2"), Task.Param2);
	Json->SetNumberField(TEXT("bootstrap_task_reward_group"), Task.Reward);
	Json->SetNumberField(TEXT("bootstrap_task_current"), Task.Current);
	Json->SetNumberField(TEXT("bootstrap_task_target"), Task.Target);
	Json->SetBoolField(TEXT("bootstrap_task_complete"), Task.bComplete);
	Json->SetNumberField(TEXT("bootstrap_task_next_id"), Task.NextId);
	Json->SetNumberField(TEXT("bootstrap_task_chain_count"), Runtime->GetActiveTaskChain().Num());
	Json->SetBoolField(TEXT("local_projected"), Projection.bProjected);
	Json->SetNumberField(TEXT("local_display_current"), Projection.Task.Current);
	Json->SetBoolField(TEXT("local_condition_met"), Projection.bConditionMet);
	Json->SetStringField(TEXT("profile_state_kind"), Profile.StateKind);
	Json->SetStringField(TEXT("profile_role_id"), Profile.RoleId);
	Json->SetNumberField(TEXT("profile_civilization_id"), Profile.CivilizationId);
	Json->SetNumberField(TEXT("profile_local_building_score"), Profile.LocalBuildingUpgradeScore);
	Json->SetNumberField(TEXT("http_dispatch_attempts"), static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("suppressed_requests"), static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	Json->SetBoolField(TEXT("task_screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	if (Screen.IsValid() && Screen->IsScreenOpen())
	{
		const auto* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskTitle_0")));
		const auto* Desc = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskDescription_0")));
		const auto* Action = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
		const auto* ActionLabel = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskAction_0Label")));
		Json->SetStringField(TEXT("task_title"), Title ? Title->GetText().ToString() : FString());
		Json->SetStringField(TEXT("task_description"), Desc ? Desc->GetText().ToString() : FString());
		Json->SetStringField(TEXT("task_tooltip"), Desc ? Desc->GetToolTipText().ToString() : FString());
		Json->SetStringField(TEXT("task_action_label"), ActionLabel ? ActionLabel->GetText().ToString() : FString());
		Json->SetBoolField(TEXT("task_action_enabled"), Action && Action->GetIsEnabled());
		Json->SetBoolField(TEXT("task_action_visible"), Visible(Action));
		Json->SetBoolField(TEXT("task_scroll_visible"), Visible(Screen->GetWidgetFromName(TEXT("TaskMainScroll"))));
		Json->SetArrayField(TEXT("task_reward_preview_rows"), RewardPreviewRows(Screen.Get()));
		Json->SetBoolField(TEXT("task_reward_group_placeholder_exists"),
			Screen->GetWidgetFromName(TEXT("TaskRewardGroup_0")) != nullptr);
#if !UE_BUILD_SHIPPING
		Json->SetBoolField(TEXT("local_projection_rendered"), Screen->IsTaskDetailLocalProjectionForAudit());
		Json->SetBoolField(TEXT("local_condition_met_rendered"), Screen->IsTaskDetailLocalConditionMetForAudit());
		Json->SetNumberField(TEXT("displayed_current"), Screen->GetTaskDetailDisplayedCurrentForAudit());
#endif
	}
	return Json;
}

void URokTaskDetailProjectionAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested(),
		TEXT("Concurrent screenshot request"))) return;
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
void URokTaskDetailProjectionAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || !bScreenshotRequested) return;
	const FString Phase = Pending->GetStringField(TEXT("phase"));
	const bool bPendingPhase = Phase == TEXT("02_upgrade_pending_task_1of2");
	const int32 ExpectedLevel = Phase.StartsWith(TEXT("01_")) || bPendingPhase ? 1 : 2;
	if (!Expect(ValidateState(ExpectedLevel, ExpectedLevel, true)
		&& Buildings->GetBuildingStateForActor(Hall.Get()).bUpgradeInProgress == bPendingPhase,
		TEXT("Screenshot frame no longer has its declared TaskInfo/Hall state"))) return;
	const FString Hash = FileSha(PendingPath);
	if (!Expect(Hash.Len() == 64, TEXT("Screenshot callback PNG missing"))) return;
	Pending->SetStringField(TEXT("png_sha256"), Hash);
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(IFileManager::Get().FileSize(*PendingPath)));
	Pending->SetObjectField(TEXT("processed_state"), State());
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokTaskDetailProjectionAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	bScreenshotRequested = false;
	bScreenshotDone = false;
	Save(false);
	Enter(NextStep);
}
bool URokTaskDetailProjectionAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.task_detail_projection.runtime_audit.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 4
		&& UpgradeCompletionCount == 1);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), TEXT("ordinary local-only GameApp TaskEntry/TaskInfo and Hall timed upgrade; display-only local condition, no task claim/server task progression"));
	Json->SetStringField(TEXT("reward_preview_scope"), TEXT("TaskMain 200900 source-derived currency display only; no reward claim, wallet mutation, inventory grant or server task progression"));
	Json->SetNumberField(TEXT("upgrade_pressed_wall_seconds"), UpgradePressedWall);
	Json->SetNumberField(TEXT("upgrade_completed_wall_seconds"), UpgradeCompletedWall);
	Json->SetNumberField(TEXT("upgrade_completion_events"), UpgradeCompletionCount);
	Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("actions"), Actions);
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
void URokTaskDetailProjectionAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && Captures.Num() == 4 && UpgradeCompletionCount == 1;
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Task detail audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok task detail projection audit %s: %s"),
		Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!Complete) UE_LOG(LogTemp, Error, TEXT("Task detail audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}

void URokTaskDetailProjectionAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !Identity()) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 30.0) Finish(TEXT("Task screenshot timeout"));
		else AdvanceCapture();
		return;
	}
	if (Step > 0 && Elapsed - StepSince > 35.0)
	{
		Finish(FString::Printf(TEXT("Task audit step %d timeout"), Step));
		return;
	}
	switch (Step)
	{
	case 0: if (Begin()) Enter(1); break;
	case 1:
		if (Click(Hud->GetWidgetFromName(TEXT("TaskEntryButton")), TEXT("open_initial_task_from_hud"))) Enter(2);
		break;
	case 2:
		if (Elapsed - StepSince >= 0.35 && ValidateState(1, 1, true))
			Capture(TEXT("01_initial_hall1_task_1of2"), 3);
		break;
	case 3:
		if (Click(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")), TEXT("close_initial_task"))) Enter(4);
		break;
	case 4:
		if (Elapsed - StepSince >= 0.35 && ValidateState(1, 1, false)
			&& Click(Panel->GetWidgetFromName(TEXT("BuildingUpgradeButton")), TEXT("hall_upgrade_menu"))) Enter(5);
		break;
	case 5:
		if (Modal->IsModalOpen() && Elapsed - StepSince >= 0.35
			&& Click(Modal->GetWidgetFromName(TEXT("TimedUpgradeButton")), TEXT("hall_timed_upgrade_start")))
		{
			UpgradePressedWall = FPlatformTime::Seconds();
			Enter(6);
		}
		break;
	case 6:
		if (Buildings->GetBuildingStateForActor(Hall.Get()).bUpgradeInProgress
			&& Click(Hud->GetWidgetFromName(TEXT("TaskEntryButton")), TEXT("open_task_during_upgrade"))) Enter(7);
		break;
	case 7:
		if (Screen->IsScreenOpen() && Buildings->GetBuildingStateForActor(Hall.Get()).bUpgradeInProgress
			&& ValidateState(1, 1, true) && Elapsed - StepSince >= 0.3)
			Capture(TEXT("02_upgrade_pending_task_1of2"), 8);
		break;
	case 8:
		if (UpgradeCompletionCount == 1 && Buildings->GetBuildingStateForActor(Hall.Get()).Level == 2
			&& ValidateState(2, 2, true))
		{
			if (++PostCompletionStableFrames >= 2) Capture(TEXT("03_natural_completion_task_2of2"), 9);
		}
		else PostCompletionStableFrames = 0;
		break;
	case 9:
		if (Click(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")), TEXT("close_completed_task"))) Enter(10);
		break;
	case 10:
		if (Elapsed - StepSince >= 0.35 && ValidateState(2, 2, false)
			&& Click(Hud->GetWidgetFromName(TEXT("TaskEntryButton")), TEXT("reopen_completed_task_from_hud"))) Enter(11);
		break;
	case 11:
		if (Elapsed - StepSince >= 0.35 && ValidateState(2, 2, true))
			Capture(TEXT("04_reopened_task_2of2"), 12);
		break;
	case 12:
		if (ValidateState(2, 2, true) && FPlatformTime::Seconds() - UpgradePressedWall >= 2.0)
			Finish();
		break;
	default: Finish(TEXT("Invalid task detail audit state")); break;
	}
}
