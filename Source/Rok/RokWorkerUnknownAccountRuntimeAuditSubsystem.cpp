#include "RokWorkerUnknownAccountRuntimeAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokBuildingPanelWidget.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
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
#include "Widgets/SWindow.h"
#include "Null/NullPlatformApplicationMisc.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR FeatureSha[] = TEXT("8bcc9d18fe579fa938558fcf7bb845b13e85efeea38e6aa1f5d0460d94217eaa");

bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeDirectoryName(Project);
	FPaths::NormalizeDirectoryName(Engine);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
		&& FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
		&& FEngineVersion::Current().GetMajor() == 5
		&& FEngineVersion::Current().GetMinor() == 8;
}

FString Encode(const TSharedRef<FJsonObject>& Value)
{
	FString Text;
	FJsonSerializer::Serialize(Value, TJsonWriterFactory<>::Create(&Text));
	return Text;
}

FString ShaFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
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
	if (!Widget || !Widget->IsVisible()
		|| Widget->GetRenderOpacity() <= 0.0f) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0.0f) return false;
	return true;
}

FString TextOf(const UWidget* Widget)
{
	const UTextBlock* Text = Cast<UTextBlock>(Widget);
	return Text ? Text->GetText().ToString() : FString();
}

TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("name"), Widget ? Widget->GetName() : FString());
	Json->SetBoolField(TEXT("visible"), Visible(Widget));
	Json->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	Json->SetStringField(TEXT("text"), TextOf(Widget));
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	return Json;
}

FString ResourceKey(const FRokResourceStock& Stock)
{
	return FString::Printf(TEXT("%d,%d,%d,%d"), Stock.Food, Stock.Wood, Stock.Stone, Stock.Gold);
}

UWidget* WorkerFeatureButton(const URokMainHudWidget* Hud)
{
	const URokBuildingPanelWidget* Panel = Hud
		? Cast<URokBuildingPanelWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))) : nullptr;
	return Panel ? Panel->GetWidgetFromName(TEXT("BuildingFeatureButton")) : nullptr;
}

int32 PngDimension(const TArray<uint8>& Bytes, int32 Offset)
{
	return (static_cast<int32>(Bytes[Offset]) << 24) | (static_cast<int32>(Bytes[Offset + 1]) << 16)
		| (static_cast<int32>(Bytes[Offset + 2]) << 8) | static_cast<int32>(Bytes[Offset + 3]);
}

FString ForegroundIdentity()
{
#if PLATFORM_WINDOWS
	return FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
#else
	return TEXT("unsupported_platform");
#endif
}
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokWorkerUnknownAccountAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}

TStatId URokWorkerUnknownAccountRuntimeAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokWorkerUnknownAccountRuntimeAuditSubsystem, STATGROUP_Tickables);
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("PendingWorkerUnknownAccountRuntimeAuditV2/RuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	if (IFileManager::Get().DirectoryExists(*Directory) || !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		Failures.Add(TEXT("Unique audit output directory unavailable"));
	}
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokWorkerUnknownAccountRuntimeAuditSubsystem::ScreenshotProcessed);
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::Deinitialize()
{
	bDeinitializing = true;
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (!bFinished)
	{
		Finish(TEXT("Audit world ended before completion"));
	}
	VirtualUser.Reset();
	Super::Deinitialize();
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::Expect(bool Value, const TCHAR* Reason)
{
	if (!Value) Finish(Reason);
	return Value;
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::OwnOffscreenWindow() const
{
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	return Window.IsValid() && Window->IsVisible()
		&& Window->GetViewportSize().Equals(FVector2D(1440, 810), .1f)
		&& Native.IsValid() && Native->GetOSWindowHandle() == nullptr
		&& Native->IsRenderingOffScreen()
		&& FSlateApplication::IsInitialized() && FSlateApplication::Get().IsRenderingOffScreen()
		&& FNullPlatformApplicationMisc::IsUsingNullApplication();
#else
	return false;
#endif
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::Begin()
{
	if (!Expect(Failures.IsEmpty() && TargetWorld(GetWorld()) && FSlateApplication::IsInitialized()
		&& FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
		&& FParse::Param(FCommandLine::Get(), TEXT("game")) && FApp::IsGame()
		&& FSlateApplication::Get().IsRenderingOffScreen()
		&& FNullPlatformApplicationMisc::IsUsingNullApplication(),
		TEXT("Project, engine, world, Null offscreen game or Slate mismatch"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 AuditFlags = 0, LocalFlags = 0, SelectionFlags = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(TEXT("-RokWorkerUnknownAccountAudit"), ESearchCase::IgnoreCase)) { ++AuditFlags; continue; }
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlags; continue; }
		if (Token.Equals(TEXT("-RokAutoSelectBuilding=UnityBuildingType_7"), ESearchCase::IgnoreCase)) { ++SelectionFlags; continue; }
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok fixture or diagnostic flag"));
	}
	if (!Expect(AuditFlags == 1 && LocalFlags == 1 && SelectionFlags == 1,
		TEXT("Exactly one audit/local-only/Worker auto-selection flag required"))) return false;
	Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
	Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Runtime = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	Server = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget && Widget->GetWorld() == GetWorld())
		{
			if (Hud.IsValid()) return Expect(false, TEXT("Multiple actual city HUDs"));
			Hud = Cast<URokMainHudWidget>(Widget);
		}
	}
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("UnityBuildingType_7")))
		{
			if (Worker.IsValid()) return Expect(false, TEXT("Ambiguous Worker Hut selection"));
			Worker = *It;
		}
	}
	if (!Expect(Controller.IsValid() && Controller->GetPawn() && Hud.IsValid() && Screen.IsValid()
		&& Buildings.IsValid() && Runtime.IsValid() && Server.IsValid() && Worker.IsValid()
		&& !Screen->IsScreenOpen() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f)
		&& Visible(WorkerFeatureButton(Hud.Get())),
		TEXT("Expected real selected Worker city HUD, anonymous local-only server, and 1440x810 viewport"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Expected anonymous local preview profile"))) return false;
	if (!Expect(ShaFile(FPaths::ProjectDir() / TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp")) == FeatureSha,
		TEXT("Worker product CPP is not exact reviewed unknown-account overlay"))) return false;
	FSlateApplication& App = FSlateApplication::Get();
	if (!Expect(App.GetUser(0).IsValid() && OwnOffscreenWindow(),
		TEXT("Owned visible Null offscreen city SWindow or user zero missing"))) return false;
	UserZeroFocus = App.GetUserFocusedWidget(0);
	ForegroundAtBegin = ForegroundIdentity();
#if PLATFORM_WINDOWS
	POINT HardwareCursor = {};
	bHardwareCursorRead = ::GetCursorPos(&HardwareCursor) != 0;
	if (bHardwareCursorRead) { HardwareCursorX = HardwareCursor.x; HardwareCursorY = HardwareCursor.y; }
#endif
	if (!Expect(bHardwareCursorRead, TEXT("Cannot read external hardware cursor before virtual input"))) return false;
	// The handle owns registration. Its actual user index is assigned by Slate.
	VirtualUser = App.FindOrCreateVirtualUser(1);
	if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0
		&& App.GetUser(VirtualUser->GetUserIndex()).IsValid(),
		TEXT("Dedicated Slate virtual user handle unavailable"))) return false;
	BaselineUpgradeActor = Buildings->GetFirstActiveUpgradeActor();
	BaselineActiveUpgradeCount = Buildings->GetActiveUpgradeCount();
	BaselineActiveConstructionCount = Buildings->GetActiveLocalConstructionCount();
	const FRokBuildingState State = Buildings->GetBuildingStateForActor(Worker.Get());
	BaselineWorkerLevel = State.Level;
	bBaselineWorkerUpgrading = State.bUpgradeInProgress;
	bBaselineQueueBusy = BaselineUpgradeActor.IsValid()
		&& Buildings->GetBuildingStateForActor(BaselineUpgradeActor.Get()).bUpgradeInProgress;
	BaselineResources = ResourceKey(Buildings->GetCurrentResources());
	BaselinePrimaryPresses = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	BaselineCamera = Controller->GetPawn()->GetActorTransform();
	return Expect(Boundary(), TEXT("Worker account, queue, cursor, focus, camera or local HTTP baseline changed"));
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::Boundary() const
{
	if (!Controller.IsValid() || !Controller->GetPawn() || !Server.IsValid() || !Buildings.IsValid()
		|| !Worker.IsValid() || !FSlateApplication::IsInitialized() || !OwnOffscreenWindow()
		|| !VirtualUser.IsValid() || VirtualUser->GetUserIndex() <= 0) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const TSharedPtr<FSlateUser> Zero = App.GetUser(0);
	const FRokBuildingState WorkerState = Buildings->GetBuildingStateForActor(Worker.Get());
	bool bCursorSame = false;
#if PLATFORM_WINDOWS
	POINT HardwareCursor = {};
	bCursorSame = bHardwareCursorRead && ::GetCursorPos(&HardwareCursor) != 0
		&& HardwareCursor.x == HardwareCursorX && HardwareCursor.y == HardwareCursorY;
#endif
	return Zero.IsValid() && Zero->GetFocusedWidget() == UserZeroFocus.Pin()
		&& bCursorSame && ForegroundIdentity() == ForegroundAtBegin
		&& App.GetUser(VirtualUser->GetUserIndex()).IsValid()
		&& Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
		&& Buildings->GetActiveUpgradeCount() == BaselineActiveUpgradeCount
		&& Buildings->GetActiveLocalConstructionCount() == BaselineActiveConstructionCount
		&& Buildings->GetFirstActiveUpgradeActor() == BaselineUpgradeActor.Get()
		&& WorkerState.Level == BaselineWorkerLevel
		&& WorkerState.bUpgradeInProgress == bBaselineWorkerUpgrading
		&& ResourceKey(Buildings->GetCurrentResources()) == BaselineResources
		&& Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == BaselinePrimaryPresses
		&& Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f);
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::Ready(int32 Phase) const
{
	if (!Boundary() || !Hud.IsValid() || !Screen.IsValid()) return false;
	if (Phase == 0 || Phase == 3)
		return !Screen->IsScreenOpen() && Visible(WorkerFeatureButton(Hud.Get()));
#if !UE_BUILD_SHIPPING
	if (!Screen->IsScreenOpen() || Screen->GetFeatureContextActorForAudit() != Worker.Get()) return false;
#else
	return false;
#endif
	const FString Title = TextOf(Screen->GetWidgetFromName(TEXT("WorkerQueuePrimaryTitle")));
	const FString RuntimeState = TextOf(Screen->GetWidgetFromName(TEXT("WorkerQueueRuntimeState")));
	const bool bPrimary = bBaselineQueueBusy
		? Title.Contains(TEXT("正在升级中")) && RuntimeState == TEXT("城主，建筑工人们正热火朝天地辛勤工作着。")
		: Title == TEXT("您的建造队列正处于空闲状态")
			&& RuntimeState == TEXT("城主，您的建筑工人随时可以开工！");
	const bool bUnknown = TextOf(Screen->GetWidgetFromName(TEXT("WorkerQueueSecondaryTitle")))
		== TEXT("第二建造队列  ·  账号状态未同步")
		&& TextOf(Screen->GetWidgetFromName(TEXT("WorkerQueueSecondaryNote")))
		== TEXT("队列状态由账号确认；此本地预览未同步是否已开通。")
		&& TextOf(Screen->GetWidgetFromName(TEXT("WorkerQueueContractBoundary")))
		== TEXT("第二队列由账号服务器管理；本地不会提前解锁、扣费或延长有效期。")
		&& Visible(Screen->GetWidgetFromName(TEXT("WorkerQueueSecondQueueUnlockOrExtend")))
		&& Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")));
	if (!bPrimary || !bUnknown) return false;
	if (Phase == 1) return true;
	return Phase == 2 && Visible(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")))
		&& TextOf(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")))
		== TEXT("第二队列需由服务器判断当前应解锁还是续期，并确认支付与有效期；当前未创建订单、延长有效期或伪造队列状态。");
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::Click(UButton* Target, const TCHAR* Label, int32 ExpectedAction)
{
	if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0
		&& Target && Visible(Target) && Target->GetIsEnabled() && Boundary(),
		TEXT("Worker Slate target missing, hidden, disabled, or boundary changed"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
	if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Worker Slate target has zero geometry"))) return false;
	if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
		|| !StableBottomRight.Equals(BottomRight, .25f))
	{
		StableTarget = Target;
		StableTopLeft = TopLeft;
		StableBottomRight = BottomRight;
		StableFrame = GFrameCounter;
		StableClickFrames = 1;
		return false;
	}
	if (GFrameCounter <= StableFrame) return false;
	StableFrame = GFrameCounter;
	if (++StableClickFrames < 3) return false;
	URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
	if (!Expect(ExpectedAction == 0 ? Indexed == nullptr : Indexed != nullptr,
		TEXT("Worker button indexed type mismatch"))) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const int32 UserIndex = VirtualUser->GetUserIndex();
	const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30.0f, -30.0f));
	const FString ForegroundBefore = ForegroundIdentity();
	const TSharedPtr<SWidget> UserZeroFocusBefore = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
	POINT CursorBefore = {};
	const bool bCursorBefore = ::GetCursorPos(&CursorBefore) != 0;
#else
	const bool bCursorBefore = false;
#endif
	if (!Expect(bCursorBefore, TEXT("Cannot read hardware cursor before virtual click"))) return false;
	App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Away, Away, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()), true);
	App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Center, Away, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()), true);
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, UserIndex);
	const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
	int32 ButtonIndex = INDEX_NONE;
	bool bUmg = false;
	TArray<TSharedPtr<FJsonValue>> PathTypes;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		const SWidget* Node = &Path.Widgets[Index].Widget.Get();
		const FString Type = Node->GetTypeAsString();
		PathTypes.Add(MakeShared<FJsonValueString>(Type));
		bUmg |= Type.Contains(TEXT("SObjectWidget"));
		if (Node == &ButtonSlate.Get()) ButtonIndex = Index;
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Path.IsValid() && Window.IsValid() && &Path.GetWindow().Get() == Window.Get();
	const bool bExact = ButtonIndex != INDEX_NONE && Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
	if (!Expect(bOwnWindow && OwnOffscreenWindow() && bExact && bUmg && Target->IsHovered() && Boundary(),
		TEXT("Worker virtual-user hit path did not prove exact owned Null offscreen UMG button"))) return false;
	auto Attempt = MakeShared<FJsonObject>();
	Attempt->SetStringField(TEXT("label"), Label);
	Attempt->SetStringField(TEXT("target"), Target->GetName());
	Attempt->SetNumberField(TEXT("action_id"), ExpectedAction);
	Attempt->SetNumberField(TEXT("virtual_user_index"), UserIndex);
	Attempt->SetNumberField(TEXT("pointer_index"), 0);
	Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Attempt->SetNumberField(TEXT("stable_distinct_frames"), StableClickFrames);
	Attempt->SetObjectField(TEXT("top_left"), Point(TopLeft));
	Attempt->SetObjectField(TEXT("bottom_right"), Point(BottomRight));
	Attempt->SetObjectField(TEXT("center"), Point(Center));
	Attempt->SetArrayField(TEXT("hit_path_types"), PathTypes);
	Attempt->SetNumberField(TEXT("exact_button_path_index"), ButtonIndex);
	Attempt->SetBoolField(TEXT("umg_path"), bUmg);
	Attempt->SetBoolField(TEXT("owned_offscreen_window"), bOwnWindow);
	Attempt->SetBoolField(TEXT("os_hit_audit_accepted"), false);
	Attempt->SetStringField(TEXT("foreground_before"), ForegroundBefore);
	Attempt->SetStringField(TEXT("user0_focus_before"), FString::Printf(TEXT("0x%p"),
		static_cast<void*>(UserZeroFocusBefore.Get())));
#if PLATFORM_WINDOWS
	Attempt->SetNumberField(TEXT("hardware_cursor_x_before"), CursorBefore.x);
	Attempt->SetNumberField(TEXT("hardware_cursor_y_before"), CursorBefore.y);
#endif
	const uint64 BeforePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	int32 CallbackCount = 0;
	int32 ActualActionId = INDEX_NONE;
	FDelegateHandle ObservationHandle;
	if (Indexed)
	{
		ObservationHandle = Indexed->OnIndexedClicked.AddLambda([&CallbackCount, &ActualActionId](int32 ActionId)
		{
			++CallbackCount;
			ActualActionId = ActionId;
		});
	}
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(UserIndex, 0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(UserIndex, 0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	if (Indexed) Indexed->OnIndexedClicked.Remove(ObservationHandle);
	const bool bActionObserved = Indexed ? CallbackCount == 1 && ActualActionId == ExpectedAction : CallbackCount == 0;
	const FString ForegroundAfter = ForegroundIdentity();
	const TSharedPtr<SWidget> UserZeroFocusAfter = App.GetUserFocusedWidget(0);
	bool bCursorUnchanged = false;
#if PLATFORM_WINDOWS
	POINT CursorAfter = {};
	bCursorUnchanged = ::GetCursorPos(&CursorAfter) != 0
		&& CursorBefore.x == CursorAfter.x && CursorBefore.y == CursorAfter.y;
#endif
	Attempt->SetBoolField(TEXT("down_handled"), Down);
	Attempt->SetBoolField(TEXT("up_handled"), Up);
	Attempt->SetNumberField(TEXT("actual_action_id"), Indexed ? ActualActionId : 0);
	Attempt->SetNumberField(TEXT("indexed_callback_count"), CallbackCount);
	Attempt->SetBoolField(TEXT("indexed_callback_observed"), Indexed && CallbackCount == 1);
	Attempt->SetBoolField(TEXT("action_identity_matched"), bActionObserved);
	Attempt->SetStringField(TEXT("foreground_after"), ForegroundAfter);
	Attempt->SetStringField(TEXT("user0_focus_after"), FString::Printf(TEXT("0x%p"),
		static_cast<void*>(UserZeroFocusAfter.Get())));
#if PLATFORM_WINDOWS
	Attempt->SetNumberField(TEXT("hardware_cursor_x_after"), CursorAfter.x);
	Attempt->SetNumberField(TEXT("hardware_cursor_y_after"), CursorAfter.y);
#endif
	Attempt->SetBoolField(TEXT("foreground_unchanged"), ForegroundBefore == ForegroundAfter);
	Attempt->SetBoolField(TEXT("user0_focus_unchanged"), UserZeroFocusBefore == UserZeroFocusAfter);
	Attempt->SetBoolField(TEXT("hardware_cursor_unchanged"), bCursorUnchanged);
	Attempt->SetNumberField(TEXT("primary_presses_before"), static_cast<double>(BeforePrimary));
	Attempt->SetNumberField(TEXT("primary_presses_after"),
		static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
	Actions.Add(MakeShared<FJsonValueObject>(Attempt));
	StableTarget.Reset();
	StableClickFrames = 0;
	return Expect(Down && Up && bActionObserved && ForegroundBefore == ForegroundAfter
		&& UserZeroFocusBefore == UserZeroFocusAfter && bCursorUnchanged && Boundary(),
		TEXT("Worker real UMG click was unhandled or changed protected offscreen state"));
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::Capture(const TCHAR* Phase, int32 NextStep)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
		&& Boundary(), TEXT("Worker screenshot cannot start from unsettled/protected state"))) return;
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetNumberField(TEXT("request_frame"), static_cast<double>(GFrameCounter));
	Pending->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	Pending->SetObjectField(TEXT("worker_button"), WidgetState(WorkerFeatureButton(Hud.Get())));
	for (const TCHAR* Name : {TEXT("WorkerQueuePrimaryTitle"), TEXT("WorkerQueueRuntimeState"),
		TEXT("WorkerQueueSecondaryTitle"), TEXT("WorkerQueueSecondaryNote"),
		TEXT("WorkerQueueSecondQueueUnlockOrExtend"), TEXT("WorkerQueueContractBoundary"),
		TEXT("FeatureBoundaryFeedback"), TEXT("FeatureFrameClose")})
	{
		Pending->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(Name)));
	}
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	if (!Expect(!IFileManager::Get().FileExists(*PendingPath), TEXT("Worker screenshot path already exists"))) return;
	Pending->SetStringField(TEXT("png"), PendingPath);
	AfterCaptureStep = NextStep;
	bPendingProcessed = false;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bPendingProcessed) return;
	TArray<uint8> Bytes;
	if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
		&& FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
		&& FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
		&& PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
		&& Boundary(), TEXT("Processed Worker screenshot missing, invalid PNG, or changed boundary"))) return;
	if (!Expect(GFrameCounter >= static_cast<uint64>(Pending->GetNumberField(TEXT("request_frame"))),
		TEXT("Worker screenshot callback predates request frame"))) return;
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(Bytes.Num()));
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	Pending->SetNumberField(TEXT("width"), 1440);
	Pending->SetNumberField(TEXT("height"), 810);
	Pending->SetStringField(TEXT("processed_primary_title"),
		TextOf(Screen->GetWidgetFromName(TEXT("WorkerQueuePrimaryTitle"))));
	Pending->SetStringField(TEXT("processed_secondary_title"),
		TextOf(Screen->GetWidgetFromName(TEXT("WorkerQueueSecondaryTitle"))));
	Pending->SetStringField(TEXT("processed_boundary_feedback"),
		TextOf(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback"))));
	ScreenshotFrame = GFrameCounter;
	bPendingProcessed = true;
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::AdvanceCapture()
{
	if (!bPendingProcessed || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(Boundary(), TEXT("Worker state changed after processed screenshot"))) return;
	Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	if (!Expect(Save(false), TEXT("Worker progress receipt write failed"))) return;
	Enter(AfterCaptureStep);
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep;
	StepSince = Elapsed;
	StableReadyFrames = 0;
	ReadyFrame = 0;
	StableClickFrames = 0;
	StableTarget.Reset();
}

bool URokWorkerUnknownAccountRuntimeAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.worker_unknown_account.local_runtime_audit.v2"));
	Json->SetStringField(TEXT("scope"), TEXT("local_worker_unknown_account_hud_navigation_only"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Actions.Num() == 3
		&& Captures.Num() == 4 && Boundary());
	Json->SetBoolField(TEXT("global_account_request_suppression_verified"), false);
	Json->SetBoolField(TEXT("server_account_state_verified"), false);
	Json->SetBoolField(TEXT("visual_parity_accepted"), false);
	Json->SetBoolField(TEXT("os_hit_audit_accepted"), false);
	Json->SetBoolField(TEXT("owned_null_offscreen_window"), OwnOffscreenWindow());
	Json->SetStringField(TEXT("external_foreground_at_begin"), ForegroundAtBegin);
	Json->SetStringField(TEXT("external_foreground_at_end"), ForegroundIdentity());
	Json->SetBoolField(TEXT("external_foreground_unchanged"), ForegroundIdentity() == ForegroundAtBegin);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetNumberField(TEXT("virtual_user_index"), VirtualUser.IsValid() ? VirtualUser->GetUserIndex() : INDEX_NONE);
	Json->SetBoolField(TEXT("source_worker_actor_unique"), Worker.IsValid());
	Json->SetStringField(TEXT("source_worker_actor"), Worker.IsValid() ? Worker->GetPathName() : FString());
	Json->SetBoolField(TEXT("primary_queue_busy_from_upgrade_subsystem"), bBaselineQueueBusy);
	Json->SetNumberField(TEXT("active_upgrade_count_before"), BaselineActiveUpgradeCount);
	Json->SetNumberField(TEXT("active_construction_count_before"), BaselineActiveConstructionCount);
	Json->SetStringField(TEXT("resource_stock_before"), BaselineResources);
	Json->SetNumberField(TEXT("local_http_dispatch_attempt_count"), Server.IsValid() ? Server->GetHttpDispatchAttemptCount() : -1);
	Json->SetNumberField(TEXT("local_suppressed_request_count"), Server.IsValid() ? Server->GetSuppressedLocalRequestCount() : -1);
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	const bool Complete = Failures.IsEmpty() && Actions.Num() == 3 && Captures.Num() == 4 && Boundary();
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Worker local navigation audit incomplete"));
	bFinished = true;
	const bool bSaved = Save(Complete);
	if (!bSaved)
	{
		Failures.Add(TEXT("Final Worker manifest could not be saved"));
		UE_LOG(LogTemp, Error, TEXT("Worker unknown-account audit final receipt write failed: %s"), *Directory);
	}
	VirtualUser.Reset();
	if (Complete && bSaved)
	{
		UE_LOG(LogTemp, Display, TEXT("Worker unknown-account local navigation audit passed: %s"), *Directory);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Worker unknown-account local navigation audit failed: %s"), *Directory);
	}
	if (!bDeinitializing) FPlatformMisc::RequestExitWithStatus(false, Complete && bSaved ? 0 : 1);
	else if (!Complete || !bSaved) FPlatformMisc::RequestExitWithStatus(false, 1);
}

void URokWorkerUnknownAccountRuntimeAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 30.0) Finish(TEXT("Worker screenshot processed callback timeout"));
		else AdvanceCapture();
		return;
	}
	if (Elapsed - StepSince > 35.0) { Finish(TEXT("Worker audit step timeout")); return; }
	auto Stable = [this](int32 Phase)
	{
		if (GFrameCounter <= ReadyFrame) return false;
		ReadyFrame = GFrameCounter;
		StableReadyFrames = Ready(Phase) ? StableReadyFrames + 1 : 0;
		return StableReadyFrames >= 3;
	};
	switch (Step)
	{
	case 0: if (!bStarted) bStarted = Begin(); if (bStarted) Enter(1); break;
	case 1: if (Stable(0)) Capture(TEXT("00_selected_worker_hud"), 2); break;
	case 2: if (Click(Cast<UButton>(WorkerFeatureButton(Hud.Get())), TEXT("hud_worker_feature"), 0)) Enter(3); break;
	case 3: if (Stable(1)) Capture(TEXT("01_worker_unknown_account"), 4); break;
	case 4: if (Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("WorkerQueueSecondQueueUnlockOrExtend"))), TEXT("action1420_account_explanation"), 1420)) Enter(5); break;
	case 5: if (Stable(2)) Capture(TEXT("02_action1420_boundary_feedback"), 6); break;
	case 6: if (Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))), TEXT("close_worker_screen"), 1)) Enter(7); break;
	case 7: if (Stable(3)) Capture(TEXT("03_closed_worker_hud"), 8); break;
	case 8: Finish(); break;
	default: Finish(TEXT("Invalid Worker audit state")); break;
	}
}
