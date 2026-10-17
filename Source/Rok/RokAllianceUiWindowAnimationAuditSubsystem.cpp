#include "RokAllianceUiWindowAnimationAuditSubsystem.h"

#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
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
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceUiWindowAnimationAudit");
struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
constexpr FInputPin Pinned[] = {
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("defd87912ce542b0b4bc0b211ff35e0c5cd6be15c1a1c783537d96f8ae2768c4")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("e9abb6226bfcf4b4c9ddec98a7052c27125322f448dcafd609b5ff8416ad738f")},
	{TEXT("Source/Rok/RokUiWindowAnimation.cpp"), TEXT("bae58440eccb1ba826871acee2131b1223094b134cf0cfe04a730c798e2b41e4")},
	{TEXT("Source/Rok/RokUiWindowAnimation.h"), TEXT("1c39a74fd70ecec8f1e9b2fd135f65b312e86f2be60ebf323596d8a8359f9e3f")},
	{TEXT("Saved/PendingAllianceUiAnimationSourceInventoryV1/alliance_ui_animation_source_inventory.json"), TEXT("0941aa82c7077eb79b058c86e82fdf7e51c11e803db143456f842fec5bfd3b1e")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_On.anim"), TEXT("61af1cbceabdab67fa70895702e9739c58cb8081279be1125640fdc85d3ab785")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1_Off.anim"), TEXT("85ab33e1e97b8e1d8a7f526abe73cb2c2f38c74676e6c65935f94df2dc8ecab6")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1.controller"), TEXT("1e3da5855be5ce01199c2eb9993ebd7f401b1eec152838cf4f47aad51ebd34aa")},
	{TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
	{TEXT("Source/Rok/RokMainHudWidget.h"), TEXT("2740efb6b5c8a68222d5429f4d66e37e8e484302a215b6c46bbd8c783f67a2e8")},
	{TEXT("Source/Rok/RokPlayerController.cpp"), TEXT("f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6")},
	{TEXT("Source/Rok/RokPlayerController.h"), TEXT("6eaa6329a03804c3af143c9777e8654d5dd83cfe24f598779b6107c988ffe772")},
	{TEXT("Source/Rok/RokServerSubsystem.cpp"), TEXT("3d73a28ff42b787c02a5c5a50ceba5bcac47056cc0945608bfea9ff9bd91bde4")},
	{TEXT("Source/Rok/RokServerSubsystem.h"), TEXT("4a07946d525ccb9eaedafcc30e68315e4ca5c52e94a88fc4c9b388833a07a5a9")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"), TEXT("3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"), TEXT("e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.uasset"), TEXT("cebb1dcd37be20da472c0982d6b45143315961204ad382a0eb6d4f55b6c92987")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance_flag.uasset"), TEXT("a309524da2970c02219917ef760ef1a7e5904b42296b303a9b155908bfafc0b2")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item2.uasset"), TEXT("6795fe239d77389ef3b0143d1648506df2663c7b0087de5cec0b203537e31a6e")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item3.uasset"), TEXT("1c41e92dd831ecf3617dfa3b29c24700c748644cddc6d7d96925a9cd7a740775")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item4.uasset"), TEXT("dea757e4af8d0a7eda694b8aba390252df099c66d5dd2079c6d34da32241b071")},
	{TEXT("Content/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.uasset"), TEXT("114a8f85a0c657be30db2a02bfc1fa3aa267fdec85482ebbc307198bcba3fb47")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"), TEXT("a3f6245b2c64f552e7b8a253e98f637d6ca0292a4aab87e5fd9183f03c76309f")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset"), TEXT("eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e")},
	{TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.uasset"), TEXT("b739a555477da17858f2ad1f97824b3191017f3cdb3703b6de19337d0622b93b")},
	{TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.uasset"), TEXT("6a484213895179943b40db30e889415dd727d487e6e9ce1fa7de45f4e44aa99b")},
	{TEXT("Content/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.uasset"), TEXT("9ee8869982773da206012e73f04393ca20f7482aac9208acd8c0f641bf6f5b78")},
};
FString FileSha(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X); Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}
int32 PngDimension(const TArray<uint8>& Bytes, int32 Offset)
{
	return (static_cast<int32>(Bytes[Offset]) << 24) | (static_cast<int32>(Bytes[Offset + 1]) << 16)
		| (static_cast<int32>(Bytes[Offset + 2]) << 8) | static_cast<int32>(Bytes[Offset + 3]);
}
}

bool URokAllianceUiWindowAnimationAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokAllianceUiWindowAnimationAudit"))
		&& Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceUiWindowAnimationAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}
void URokAllianceUiWindowAnimationAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/AllianceUiWindowAnimationAuditV1")
		/ FString::Printf(TEXT("%s_pid%d"), *Stamp, FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokAllianceUiWindowAnimationAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceUiWindowAnimationAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
TStatId URokAllianceUiWindowAnimationAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceUiWindowAnimationAuditSubsystem, STATGROUP_Tickables);
}
bool URokAllianceUiWindowAnimationAuditSubsystem::Identity() const
{
	const UWorld* World = GetWorld();
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeDirectoryName(Project); FPaths::NormalizeDirectoryName(Engine);
	return World && World->WorldType == EWorldType::Game && World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		&& FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
		&& FEngineVersion::Current().GetMajor() == 5 && FEngineVersion::Current().GetMinor() == 8;
}
bool URokAllianceUiWindowAnimationAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokAllianceUiWindowAnimationAuditSubsystem::Boundary() const
{
	if (!Identity() || !Controller.IsValid() || !Hud.IsValid() || !Screen.IsValid()
		|| !Server.IsValid() || !Runtime.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	const FPlan* Plan = CurrentPlan();
	const bool bMayCompleteClose = bTracking && Plan && Plan->bClose && Plan->Action != 1;
	if (Plan && Plan->Action != 1 && Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance)
		return false;
	const uint64 Dispatches = Screen->AllianceRootCloseDispatchCountForAudit;
	if (Dispatches != ExpectedDispatches
		&& !(bMayCompleteClose && Dispatches == ExpectedDispatches + 1)) return false;
	const auto ProfileState = Runtime->GetPlayerProfileState();
	return ProfileState.StateKind == TEXT("local_preview") && !ProfileState.bHasRoleId
		&& ProfileState.RoleId.IsEmpty() && !ProfileState.bHasCivilization
		&& ProfileState.CivilizationId == 0 && ProfileState.CivilizationName.IsEmpty();
}
bool URokAllianceUiWindowAnimationAuditSubsystem::FocusOwnWindow()
{
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD OwnerPid = 0; RECT Client = {};
	if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
	if (!Expect(Handle && ::IsWindow(Handle) && OwnerPid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client) && Client.right - Client.left == 1440
		&& Client.bottom - Client.top == 810, TEXT("Wrong own game window size/PID"))) return false;
	if (::GetForegroundWindow() == Handle) return true;
	if (::IsIconic(Handle)) ::ShowWindow(Handle, SW_RESTORE);
	else if (!::IsWindowVisible(Handle)) ::ShowWindow(Handle, SW_SHOW);
	const HWND Foreground = ::GetForegroundWindow();
	const DWORD ForegroundThread = Foreground ? ::GetWindowThreadProcessId(Foreground, nullptr) : 0;
	const DWORD CurrentThread = ::GetCurrentThreadId();
	const bool bAttached = ForegroundThread && ForegroundThread != CurrentThread
		&& ::AttachThreadInput(CurrentThread, ForegroundThread, 1) != 0;
	::BringWindowToTop(Handle); ::SetForegroundWindow(Handle);
	if (bAttached) ::AttachThreadInput(CurrentThread, ForegroundThread, 0);
	return ::GetForegroundWindow() == Handle;
#else
	return Expect(false, TEXT("Windows foreground game window required"));
#endif
}
bool URokAllianceUiWindowAnimationAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(), TEXT("Wrong world or no Slate"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token; int32 AuditCount = 0, LocalCount = 0, RouteCount = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditCount; continue; }
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalCount; continue; }
		if (Token.Equals(TEXT("-RokOpenModal=alliance_unjoined"), ESearchCase::IgnoreCase))
		{ Profile = TEXT("unjoined"); ++RouteCount; continue; }
		if (Token.Equals(TEXT("-RokOpenModal=alliance_joined"), ESearchCase::IgnoreCase))
		{ Profile = TEXT("joined"); ++RouteCount; continue; }
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok flag"));
	}
	if (!Expect(AuditCount == 1 && LocalCount == 1 && RouteCount == 1,
		TEXT("One audit/local/route flag each required"))) return false;
	Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
	Runtime = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	Server = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets) if (Widget && Widget->GetWorld() == GetWorld())
	{
		if (Hud.IsValid()) return Expect(false, TEXT("Ambiguous HUD"));
		Hud = Cast<URokMainHudWidget>(Widget);
	}
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	// The Game HUD may be created after this WorldSubsystem's first Tick.
	if (!Controller.IsValid() || !Controller->GetPawn() || !Runtime.IsValid()
		|| !Runtime->IsRuntimeStateLoaded() || !Server.IsValid()
		|| !Hud.IsValid() || !Screen.IsValid()) return false;
	if (!Expect(Server->IsLocalOnlyRun()
		&& Screen->ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& Screen->bAllianceTestFixtureState == (Profile == TEXT("joined"))
		&& Screen->AlliancePreviewState == (Profile == TEXT("joined") ? 2 : 0)
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
		TEXT("Wrong anonymous local fixture/HUD/viewport"))) return false;
	BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	ExpectedDispatches = Screen->AllianceRootCloseDispatchCountForAudit;
	if (!Expect(Boundary(), TEXT("Network/profile boundary invalid"))) return false;
	Inputs = MakeShared<FJsonObject>();
	for (const FInputPin& Pin : Pinned)
	{
		const FString Actual = FileSha(FPaths::ProjectDir() / Pin.Path);
		if (!Expect(Actual == Pin.Sha, TEXT("Exact animation/source SHA mismatch"))) return false;
		Inputs->SetStringField(Pin.Path, Actual);
	}
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer; bPointerSaved = true;
	return true;
}

const URokAllianceUiWindowAnimationAuditSubsystem::FPlan*
URokAllianceUiWindowAnimationAuditSubsystem::CurrentPlan() const
{
	static const FPlan Unjoined[] = {
		{TEXT("join_open"), TEXT("AllianceWelcomeJoin"), 600, TEXT("Join"), false, TEXT("Join"), TEXT("join_open_end")},
		{TEXT("join_back"), TEXT("AllianceJoinBack"), 621, TEXT("Join"), true, TEXT("None"), TEXT("")},
		{TEXT("create_form"), TEXT("AllianceWelcomeCreate"), 601, TEXT("None"), false, TEXT("None"), TEXT("")},
		{TEXT("flag_open"), TEXT("AllianceFlagEdit"), 625, TEXT("Flag"), false, TEXT("Flag"), TEXT("flag_open_end")},
		{TEXT("flag_cancel"), TEXT("AllianceFlagEditorCancel"), 19010, TEXT("Flag"), true, TEXT("None"), TEXT("")},
	};
	static const FPlan Joined[] = {
		{TEXT("gift_open"), TEXT("AllianceFeatureHit_6"), 656, TEXT("Gift"), false, TEXT("Gift"), TEXT("gift_open_end")},
		{TEXT("gift_back"), TEXT("AllianceGiftReferenceBack"), 686, TEXT("Gift"), true, TEXT("Main"), TEXT("")},
		{TEXT("main_reopen_after_gift"), nullptr, 0, TEXT("Main"), false, TEXT("Main"), TEXT("main_open_end")},
		{TEXT("shop_open"), TEXT("AllianceFeatureHit_7"), 657, TEXT("Shop"), false, TEXT("Shop"), TEXT("shop_open_end")},
		{TEXT("shop_back"), TEXT("AllianceShopReferenceBack"), 690, TEXT("Shop"), true, TEXT("Main"), TEXT("")},
		{TEXT("main_reopen_after_shop"), nullptr, 0, TEXT("Main"), false, TEXT("Main"), TEXT("main_reopen_end")},
		{TEXT("main_close"), TEXT("FeatureFrameClose"), 1, TEXT("Main"), true, TEXT("None"), TEXT("")},
	};
	const bool bJoined = Profile == TEXT("joined");
	const int32 Count = bJoined ? UE_ARRAY_COUNT(Joined) : UE_ARRAY_COUNT(Unjoined);
	return PlanIndex < Count ? (bJoined ? &Joined[PlanIndex] : &Unjoined[PlanIndex]) : nullptr;
}

bool URokAllianceUiWindowAnimationAuditSubsystem::Click(UButton* Target, int32 ExpectedAction, const TCHAR* Label)
{
	if (!Expect(Boundary() && Target && Target->IsVisible() && Target->GetIsEnabled(),
		TEXT("Target/boundary unavailable"))) return false;
	const URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
	if (!Expect(Indexed && Indexed->ActionId == ExpectedAction
		&& ExpectedAction != 623 && ExpectedAction != 620,
		TEXT("Unapproved or wrong action ID"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
	if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Zero target geometry"))) return false;
	if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
		|| !StableBottomRight.Equals(BottomRight, .25f))
	{
		StableTarget = Target; StableTopLeft = TopLeft; StableBottomRight = BottomRight;
		StableFrame = GFrameCounter; StableFrames = 1; return false;
	}
	if (GFrameCounter <= StableFrame) return false;
	StableFrame = GFrameCounter;
	if (++StableFrames < 2) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30, -30));
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
	bool bExact = false, bUmg = false;
	TArray<TSharedPtr<FJsonValue>> Types;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		const SWidget* Widget = &Path.Widgets[Index].Widget.Get();
		const FString Type = Widget->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		bUmg |= Type.Contains(TEXT("SObjectWidget"));
		bExact |= Widget == &ButtonSlate.Get() && Type.Contains(TEXT("SButton"));
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = false; uint32 Pid = 0;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	if (Handle) { DWORD NativePid = 0; ::GetWindowThreadProcessId(Handle, &NativePid); Pid = NativePid; }
	bForeground = Handle && ::GetForegroundWindow() == Handle && Pid == FPlatformProcess::GetCurrentProcessId();
#endif
	if (!Expect(Path.IsValid() && bExact && bUmg && Target->IsHovered() && bOwn && bForeground,
		TEXT("Exact foreground Slate button not hit"))) return false;
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	auto Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("label"), Label); Entry->SetStringField(TEXT("target"), Target->GetName());
	Entry->SetNumberField(TEXT("action_id"), ExpectedAction);
	Entry->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Entry->SetStringField(TEXT("method"), TEXT("ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
	Entry->SetObjectField(TEXT("absolute_center"), Point(Center));
	Entry->SetObjectField(TEXT("absolute_top_left"), Point(TopLeft));
	Entry->SetObjectField(TEXT("absolute_bottom_right"), Point(BottomRight));
	Entry->SetObjectField(TEXT("local_size"), Point(Size));
	Entry->SetArrayField(TEXT("hit_path_types"), Types);
	Entry->SetBoolField(TEXT("exact_button"), bExact);
	Entry->SetBoolField(TEXT("umg_path"), bUmg);
	Entry->SetBoolField(TEXT("own_game_window"), bOwn);
	Entry->SetBoolField(TEXT("foreground"), bForeground);
	Entry->SetNumberField(TEXT("own_window_pid"), Pid);
	Entry->SetNumberField(TEXT("stable_frames"), StableFrames);
	Entry->SetBoolField(TEXT("down_handled"), Down); Entry->SetBoolField(TEXT("up_handled"), Up);
	Entry->SetNumberField(TEXT("pending_action_after"), Screen->PendingActionId);
	Actions.Add(MakeShared<FJsonValueObject>(Entry));
	StableTarget.Reset(); StableFrames = 0;
	return Expect(Down && Up && Screen->PendingActionId == ExpectedAction && Boundary(),
		TEXT("Slate click not deferred or changed local boundary"));
}

bool URokAllianceUiWindowAnimationAuditSubsystem::RepeatBackThroughSlate(const FPlan& Plan)
{
	UButton* Target = Cast<UButton>(Screen->GetWidgetFromName(Plan.Button));
	if (!Expect(Target && Screen->WindowAnimation.IsClosing(), TEXT("Repeat-back target/closing missing"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Center);
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
	bool bExact = false;
	TArray<TSharedPtr<FJsonValue>> Types;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		bExact |= &Path.Widgets[Index].Widget.Get() == &ButtonSlate.Get();
		Types.Add(MakeShared<FJsonValueString>(Path.Widgets[Index].Widget->GetTypeAsString()));
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = false; uint32 Pid = 0;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	if (Handle) { DWORD NativePid = 0; ::GetWindowThreadProcessId(Handle, &NativePid); Pid = NativePid; }
	bForeground = Handle && ::GetForegroundWindow() == Handle && Pid == FPlatformProcess::GetCurrentProcessId();
#endif
	if (!Expect(bOwn && bForeground && Path.IsValid(), TEXT("Repeat Back left own foreground Slate window"))) return false;
	const uint64 Before = Screen->AllianceRootCloseDispatchCountForAudit;
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	auto Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("label"), TEXT("repeat_back_during_close"));
	Entry->SetStringField(TEXT("target"), Plan.Button);
	Entry->SetNumberField(TEXT("action_id"), Plan.Action);
	Entry->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Entry->SetStringField(TEXT("method"), TEXT("ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
	Entry->SetObjectField(TEXT("absolute_center"), Point(Center));
	Entry->SetArrayField(TEXT("hit_path_types"), Types);
	Entry->SetBoolField(TEXT("exact_button_in_hit_path"), bExact);
	Entry->SetBoolField(TEXT("own_game_window"), bOwn);
	Entry->SetBoolField(TEXT("foreground"), bForeground);
	Entry->SetNumberField(TEXT("own_window_pid"), Pid);
	Entry->SetBoolField(TEXT("down_handled"), Down); Entry->SetBoolField(TEXT("up_handled"), Up);
	Entry->SetNumberField(TEXT("pending_root_close_after"), Screen->PendingAllianceRootCloseActionId);
	Entry->SetNumberField(TEXT("dispatch_count_after"), static_cast<double>(Screen->AllianceRootCloseDispatchCountForAudit));
	Actions.Add(MakeShared<FJsonValueObject>(Entry));
	return Expect(Screen->AllianceRootCloseDispatchCountForAudit == Before
		&& Screen->PendingAllianceRootCloseActionId == Plan.Action && Boundary(),
		TEXT("Repeated physical Back escaped close guard"));
}

bool URokAllianceUiWindowAnimationAuditSubsystem::Start(const FPlan& Plan)
{
	if (!Boundary()) return Expect(false, TEXT("Boundary changed before transition"));
	if (Plan.Button && !Click(Cast<UButton>(Screen->GetWidgetFromName(Plan.Button)), Plan.Action, Plan.Label))
		return false;
	ActiveTransition = MakeShared<FJsonObject>(); ActiveSamples.Reset();
	ActiveTransition->SetStringField(TEXT("label"), Plan.Label);
	ActiveTransition->SetStringField(TEXT("root"), Plan.Root);
	ActiveTransition->SetBoolField(TEXT("closing"), Plan.bClose);
	ActiveTransition->SetStringField(TEXT("destination"), Plan.Destination);
	ActiveTransition->SetNumberField(TEXT("action_id"), Plan.Action);
	ActiveTransition->SetNumberField(TEXT("start_frame"), static_cast<double>(GFrameCounter));
	DispatchBefore = Screen->AllianceRootCloseDispatchCountForAudit;
	bTracking = true; bRepeatAttempted = false; PlanSince = Elapsed;
	return true;
}

bool URokAllianceUiWindowAnimationAuditSubsystem::Observe(const FPlan& Plan)
{
	if (!Expect(Boundary(), TEXT("Business/network boundary changed"))) return false;
	const UCanvasPanel* Root = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")));
	if (!Expect(Root != nullptr, TEXT("Real ModeCanvas missing"))) return false;
	auto RootName = [this]() -> const TCHAR*
	{
		switch (Screen->AllianceAnimatedRoot)
		{
		case URokUnityFeatureScreenWidget::EAllianceAnimatedRoot::Main: return TEXT("Main");
		case URokUnityFeatureScreenWidget::EAllianceAnimatedRoot::Join: return TEXT("Join");
		case URokUnityFeatureScreenWidget::EAllianceAnimatedRoot::Flag: return TEXT("Flag");
		case URokUnityFeatureScreenWidget::EAllianceAnimatedRoot::Gift: return TEXT("Gift");
		case URokUnityFeatureScreenWidget::EAllianceAnimatedRoot::Shop: return TEXT("Shop");
		default: return TEXT("None");
		}
	};
	const FString ActualRoot = RootName();
	const bool bSameRoot = ActualRoot == Plan.Root;
	const bool bClosingNow = Screen->WindowAnimation.IsClosing();
	if (bSameRoot && FString(Plan.Root) != TEXT("None")
		&& (Plan.bClose ? bClosingNow : !bClosingNow))
	{
		const FVector2D Scale = Root->GetRenderTransform().Scale;
		const FVector2D Pivot = Root->GetRenderTransformPivot();
		const float Seconds = Screen->WindowAnimation.GetElapsedSeconds();
		auto Sample = MakeShared<FJsonObject>();
		Sample->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
		Sample->SetNumberField(TEXT("audit_wall_seconds"), Elapsed);
		Sample->SetNumberField(TEXT("elapsed_seconds"), Seconds);
		Sample->SetStringField(TEXT("root"), ActualRoot);
		Sample->SetBoolField(TEXT("closing"), bClosingNow);
		Sample->SetObjectField(TEXT("render_scale"), Point(Scale));
		Sample->SetNumberField(TEXT("render_opacity"), Root->GetRenderOpacity());
		Sample->SetObjectField(TEXT("render_pivot"), Point(Pivot));
		Sample->SetBoolField(TEXT("visible"), Root->IsVisible());
		Sample->SetNumberField(TEXT("pending_close_action"), Screen->PendingAllianceRootCloseActionId);
		Sample->SetNumberField(TEXT("dispatch_count"), static_cast<double>(Screen->AllianceRootCloseDispatchCountForAudit));
		const FGeometry Geometry = Root->GetCachedGeometry();
		Sample->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
		Sample->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
		Sample->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
		ActiveSamples.Add(MakeShared<FJsonValueObject>(Sample));
		if (Plan.bClose && Plan.Action != 1 && !bRepeatAttempted
			&& Seconds >= .03f && Seconds <= .10f)
		{
			bRepeatAttempted = true;
			if (!RepeatBackThroughSlate(Plan)) return false;
		}
	}
	bool bComplete = false;
	if (!Plan.bClose && FString(Plan.Root) == TEXT("None"))
		bComplete = Screen->AlliancePreviewState == 3 && ActualRoot == TEXT("None");
	else if (!Plan.bClose)
		bComplete = bSameRoot && Screen->WindowAnimation.GetStyle() == ERokUiWindowAnimationStyle::Type1
			&& !Screen->WindowAnimation.IsAnimating()
			&& Screen->WindowAnimation.GetElapsedSeconds() >= .25f;
	else if (Plan.Action == 1)
		bComplete = Screen->ActiveMode == ERokUnityFeatureScreenMode::None && ActualRoot == TEXT("None")
			&& Screen->AllianceRootCloseDispatchCountForAudit == DispatchBefore;
	else
		bComplete = ActualRoot == Plan.Destination
			&& Screen->AllianceRootCloseDispatchCountForAudit == DispatchBefore + 1
			&& Screen->AllianceRootCloseLastActionForAudit == Plan.Action
			&& Screen->PendingAllianceRootCloseActionId == INDEX_NONE;
	if (!bComplete) return false;
	if (!Expect(!ActiveSamples.IsEmpty() || FString(Plan.Root) == TEXT("None"),
		TEXT("Transition completed without actual Widget frames"))) return false;
	if (!Expect(!Plan.bClose || Plan.Action == 1 || bRepeatAttempted,
		TEXT("No real repeat Back attempt during close"))) return false;
	ActiveTransition->SetArrayField(TEXT("samples"), ActiveSamples);
	ActiveTransition->SetNumberField(TEXT("completed_frame"), static_cast<double>(GFrameCounter));
	ActiveTransition->SetNumberField(TEXT("completed_wall_seconds"), Elapsed);
	ActiveTransition->SetNumberField(TEXT("dispatch_before"), static_cast<double>(DispatchBefore));
	ActiveTransition->SetNumberField(TEXT("dispatch_after"), static_cast<double>(Screen->AllianceRootCloseDispatchCountForAudit));
	ActiveTransition->SetNumberField(TEXT("last_dispatched_action"), Screen->AllianceRootCloseLastActionForAudit);
	ActiveTransition->SetStringField(TEXT("actual_destination"), ActualRoot);
	ActiveTransition->SetBoolField(TEXT("screen_open_after"), Screen->IsScreenOpen());
	ActiveTransition->SetBoolField(TEXT("repeat_attempted"), bRepeatAttempted);
	Transitions.Add(MakeShared<FJsonValueObject>(ActiveTransition));
	ExpectedDispatches = Screen->AllianceRootCloseDispatchCountForAudit;
	ActiveTransition.Reset(); ActiveSamples.Reset(); bTracking = false;
	Save(false);
	return true;
}

bool URokAllianceUiWindowAnimationAuditSubsystem::Capture(const FPlan& Plan)
{
	if (!Plan.Png || !*Plan.Png) return true;
	if (!Expect(!FScreenshotRequest::IsScreenshotRequested() && Boundary(),
		TEXT("Screenshot request overlap/boundary"))) return false;
	const UWidget* Root = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	if (!Expect(Root && Root->GetCachedGeometry().GetLocalSize().X > 0
		&& Root->GetCachedGeometry().GetLocalSize().Y > 0
		&& Root->GetRenderTransformPivot().Equals(FVector2D(.5f,.5f), .001f)
		&& Root->GetRenderOpacity() >= .995f
		&& Root->GetRenderTransform().Scale.Equals(FVector2D(1), .008f),
		TEXT("Open end root not geometrically settled"))) return false;
	PendingPng = Directory / FString::Printf(TEXT("%s.png"), Plan.Png);
	const FGeometry Geometry = Root->GetCachedGeometry();
	PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("phase"), Plan.Png);
	PendingCapture->SetStringField(TEXT("png"), PendingPng);
	PendingCapture->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	PendingCapture->SetObjectField(TEXT("root_local_size"), Point(Geometry.GetLocalSize()));
	PendingCapture->SetObjectField(TEXT("root_absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	PendingCapture->SetObjectField(TEXT("root_absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	PendingCapture->SetObjectField(TEXT("root_pivot"), Point(Root->GetRenderTransformPivot()));
	PendingCapture->SetObjectField(TEXT("root_scale"), Point(Root->GetRenderTransform().Scale));
	PendingCapture->SetNumberField(TEXT("root_opacity"), Root->GetRenderOpacity());
	bScreenshotDone = false;
	FScreenshotRequest::RequestScreenshot(PendingPng, true, false);
	return true;
}
void URokAllianceUiWindowAnimationAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || PendingPng.IsEmpty() || bScreenshotDone) return;
	TArray<uint8> Bytes;
	if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPng) && Bytes.Num() > 24
		&& FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
		&& FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
		&& PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
		&& Boundary(), TEXT("Original PNG callback invalid"))) return;
	if (!Expect(PendingCapture.IsValid(), TEXT("Missing screenshot request metadata"))) return;
	auto Entry = PendingCapture;
	Entry->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
	Entry->SetNumberField(TEXT("bytes"), Bytes.Num());
	Entry->SetNumberField(TEXT("width"), 1440); Entry->SetNumberField(TEXT("height"), 810);
	Entry->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	Captures.Add(MakeShared<FJsonValueObject>(Entry));
	ScreenshotFrame = GFrameCounter; bScreenshotDone = true;
}

bool URokAllianceUiWindowAnimationAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_ui_window_animation.runtime.v1"));
	Json->SetStringField(TEXT("status"), Complete ? TEXT("COMPLETE_NATIVE_CAPTURE") : TEXT("INCOMPLETE"));
	Json->SetStringField(TEXT("profile"), Profile);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("engine"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
	Json->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetNumberField(TEXT("last_frame"), static_cast<double>(GFrameCounter));
	Json->SetBoolField(TEXT("local_only"), Server.IsValid() && Server->IsLocalOnlyRun());
	Json->SetNumberField(TEXT("http_dispatch_attempts"), Server.IsValid() ? Server->GetHttpDispatchAttemptCount() : -1);
	Json->SetNumberField(TEXT("http_suppressed_requests"), Server.IsValid() ? Server->GetSuppressedLocalRequestCount() : -1);
	Json->SetNumberField(TEXT("primary_press_baseline"), static_cast<double>(BaselinePrimary));
	Json->SetNumberField(TEXT("primary_press_final"), Controller.IsValid()
		? static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount) : -1);
	Json->SetObjectField(TEXT("inputs"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("transitions"), Transitions);
	Json->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	FString Text; FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
	return FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("native_capture.json")));
}
void URokAllianceUiWindowAnimationAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	bFinished = true;
	if (!Error.IsEmpty()) Failures.Add(Error);
	const bool Complete = Error.IsEmpty() && Boundary() && CurrentPlan() == nullptr
		&& (Profile == TEXT("joined") ? Transitions.Num() == 7 && Captures.Num() == 4
			: Transitions.Num() == 5 && Captures.Num() == 2);
	Save(Complete);
	if (bPointerSaved && FSlateApplication::IsInitialized())
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokAllianceUiWindowAnimationAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (!Identity()) { Finish(TEXT("Wrong world/project/engine")); return; }
	if (!bBegun)
	{
		if (Elapsed > 15.0) { Finish(TEXT("HUD startup timeout")); return; }
		if (!Begin()) return;
		if (!FocusOwnWindow()) return;
		bBegun = true; PlanSince = Elapsed;
	}
	if (!Boundary()) { Finish(TEXT("Local/network/account boundary changed")); return; }
	if (Elapsed - PlanSince > 15.0) { Finish(TEXT("Transition/screenshot timeout")); return; }
	const FPlan* Plan = CurrentPlan();
	if (!Plan) { Finish(); return; }
	if (!bTracking && PendingPng.IsEmpty())
	{
		Start(*Plan); return;
	}
	if (bTracking)
	{
		if (Observe(*Plan))
		{
			if (Plan->Png && *Plan->Png) Capture(*Plan);
			else { ++PlanIndex; PlanSince = Elapsed; }
		}
		return;
	}
	if (!PendingPng.IsEmpty() && bScreenshotDone && GFrameCounter > ScreenshotFrame)
	{
		PendingPng.Empty(); PendingCapture.Reset(); bScreenshotDone = false;
		++PlanIndex; PlanSince = Elapsed; Save(false);
	}
}
