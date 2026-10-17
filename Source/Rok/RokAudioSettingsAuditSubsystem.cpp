#include "RokAudioSettingsAuditSubsystem.h"

#include "RokAudioRouterSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokClientSettingsSubsystem.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR MainFlag[] = TEXT("-RokAudioSettingsAudit");
constexpr TCHAR ResumeFlag[] = TEXT("-RokAudioSettingsAuditResume");
constexpr TCHAR BatchPath[] = TEXT("RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial");

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
TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("x"), Value.X);
	Result->SetNumberField(TEXT("y"), Value.Y);
	return Result;
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
TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("exists"), Widget != nullptr);
	Result->SetBoolField(TEXT("visible"), Visible(Widget));
	Result->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	Result->SetStringField(TEXT("parent"), Widget && Widget->GetParent() ? Widget->GetParent()->GetName() : FString());
	Result->SetObjectField(TEXT("render_scale"), Point(Widget ? Widget->GetRenderTransform().Scale : FVector2D::ZeroVector));
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Result->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
	Result->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Result->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	if (const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
	{
		Result->SetObjectField(TEXT("slot_position"), Point(Slot->GetPosition()));
		Result->SetObjectField(TEXT("slot_size"), Point(Slot->GetSize()));
	}
	if (const UTextBlock* Label = Cast<UTextBlock>(Widget)) Result->SetStringField(TEXT("text"), Label->GetText().ToString());
	if (const USlider* Slider = Cast<USlider>(Widget))
	{
		Result->SetNumberField(TEXT("value"), Slider->GetValue());
		Result->SetNumberField(TEXT("min"), Slider->GetMinValue());
		Result->SetNumberField(TEXT("max"), Slider->GetMaxValue());
		Result->SetNumberField(TEXT("step"), Slider->GetStepSize());
		Result->SetBoolField(TEXT("mouse_uses_step"), Slider->MouseUsesStep);
	}
	return Result;
}
}

bool URokAudioSettingsAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokCityBgmNight"))
		&& (FParse::Param(FCommandLine::Get(), TEXT("RokAudioSettingsAudit"))
			!= FParse::Param(FCommandLine::Get(), TEXT("RokAudioSettingsAuditResume")))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAudioSettingsAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokAudioSettingsAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokAudioSettingsAuditSubsystem, STATGROUP_Tickables);
}
void URokAudioSettingsAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bResume = FParse::Param(FCommandLine::Get(), TEXT("RokAudioSettingsAuditResume"));
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / BatchPath
		/ TEXT("AudioSettingsAudit") / FString::Printf(TEXT("%lld_%u_%s"),
			FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId(),
			bResume ? TEXT("resume") : TEXT("primary")));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokAudioSettingsAuditSubsystem::ScreenshotProcessed);
}
void URokAudioSettingsAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	if (!bFinished && bBegun) Finish(TEXT("Audio settings world teardown interrupted audit"));
	Super::Deinitialize();
}
bool URokAudioSettingsAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokAudioSettingsAuditSubsystem::Expect(bool Condition, const TCHAR* Error)
{
	if (!Condition) Finish(Error);
	return Condition;
}
bool URokAudioSettingsAuditSubsystem::OwnWindowFocus()
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
		&& Client.bottom - Client.top == 810, TEXT("Audio audit requires own 1440x810 window"))) return false;
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
	return Expect(false, TEXT("Audio audit requires Windows game window"));
#endif
}
bool URokAudioSettingsAuditSubsystem::RetryInput(const TCHAR* Kind, const TCHAR* Name,
	const FVector2D& Position, const TArray<TSharedPtr<FJsonValue>>& Types,
	const FString& Terminal, bool bFocused, bool bOwn, bool bForeground,
	bool bHit, bool bHovered)
{
	const FString Key = FString(Kind) + TEXT(":") + Name;
	if (InputRetryKey != Key)
	{
		InputRetryKey = Key;
		InputRetryCount = 0;
	}
	++InputRetryCount;
	auto Attempt = MakeShared<FJsonObject>();
	Attempt->SetStringField(TEXT("kind"), Kind);
	Attempt->SetStringField(TEXT("selector"), Name);
	Attempt->SetNumberField(TEXT("retry"), InputRetryCount);
	Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Attempt->SetObjectField(TEXT("absolute_point"), Point(Position));
	Attempt->SetArrayField(TEXT("hit_path_types"), Types);
	Attempt->SetStringField(TEXT("terminal_type"), Terminal);
	Attempt->SetBoolField(TEXT("focus_prepared"), bFocused);
	Attempt->SetBoolField(TEXT("own_game_window"), bOwn);
	Attempt->SetBoolField(TEXT("foreground"), bForeground);
	Attempt->SetBoolField(TEXT("target_in_hit_path"), bHit);
	Attempt->SetBoolField(TEXT("target_hovered"), bHovered);
	InputAttempts.Add(MakeShared<FJsonValueObject>(Attempt));
	if (InputRetryCount >= 24)
	{
		Finish(FString::Printf(TEXT("Audio audit %s %s hit path not stable after %d pre-input attempts"),
			Kind, Name, InputRetryCount));
	}
	return false;
}
TSharedRef<FJsonObject> URokAudioSettingsAuditSubsystem::BusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Runtime.IsValid() || !Buildings.IsValid() || !Server.IsValid()) return Json;
	const auto Task = Runtime->GetActiveTask();
	const auto Profile = Runtime->GetPlayerProfileState();
	const auto Wallet = Buildings->GetCurrentResources();
	Json->SetNumberField(TEXT("task_id"), Task.TaskId);
	Json->SetNumberField(TEXT("task_current"), Task.Current);
	Json->SetNumberField(TEXT("task_target"), Task.Target);
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
	Json->SetNumberField(TEXT("http_dispatch_attempts"), static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("http_suppressed_requests"), static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	if (Settings.IsValid())
	{
		auto Toggles = MakeShared<FJsonObject>();
		for (const FRokPersonalityToggleDefinition& Definition : Settings->GetPersonalityToggleDefinitions())
			Toggles->SetBoolField(Definition.Name.ToString(), Settings->GetPersonalityToggle(Definition.Name));
		Json->SetObjectField(TEXT("personality_toggles"), Toggles);
	}
	if (GEngine && GEngine->GetGameUserSettings())
	{
		const UGameUserSettings* User = GEngine->GetGameUserSettings();
		Json->SetNumberField(TEXT("engine_scalability_tier"), User->GetOverallScalabilityLevel());
		Json->SetNumberField(TEXT("engine_frame_limit"), User->GetFrameRateLimit());
		Json->SetBoolField(TEXT("engine_vsync"), User->IsVSyncEnabled());
		const FIntPoint Resolution = User->GetScreenResolution();
		Json->SetObjectField(TEXT("engine_resolution"), Point(FVector2D(Resolution.X, Resolution.Y)));
	}
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
		Troops.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_troops"), Troops);
	return Json;
}
bool URokAudioSettingsAuditSubsystem::Boundary() const
{
	return Identity() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& Controller.IsValid()
		&& Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == BaselinePrimary
		&& !BaselineBusiness.IsEmpty() && Encode(BusinessState()) == BaselineBusiness
		&& Settings.IsValid() && Audio.IsValid()
		&& FMath::IsNearlyEqual(static_cast<float>(Settings->GetGraphicsQualityTier()), InitialGraphics)
		&& FMath::IsNearlyEqual(static_cast<float>(Settings->GetFrameRateOption()), InitialFrameRate)
		&& !ScalabilityPath.IsEmpty() && FileSha(ScalabilityPath) == InitialScalabilityHash;
}

TSharedRef<FJsonObject> URokAudioSettingsAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Json->SetObjectField(TEXT("business"), BusinessState());
	Json->SetNumberField(TEXT("primary_press_count"), Controller.IsValid()
		? static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount) : -1.0);
	Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	Json->SetNumberField(TEXT("expected_mode"), ExpectedMode);
	if (Settings.IsValid())
	{
		Json->SetNumberField(TEXT("settings_sfx"), Settings->GetSfxVolume());
		Json->SetNumberField(TEXT("settings_music"), Settings->GetMusicVolume());
		Json->SetNumberField(TEXT("graphics_tier"), static_cast<int32>(Settings->GetGraphicsQualityTier()));
		Json->SetNumberField(TEXT("frame_rate_option"), static_cast<int32>(Settings->GetFrameRateOption()));
	}
	if (Audio.IsValid())
	{
		const FRokAudioRouterVoiceSnapshot Voice = Audio->GetVoiceSnapshot();
		auto Route = MakeShared<FJsonObject>();
		Route->SetNumberField(TEXT("music_setting"), Voice.MusicSetting);
		Route->SetNumberField(TEXT("sfx_setting"), Voice.SfxSetting);
		Route->SetNumberField(TEXT("city_bgm_event_id"), Voice.CityBgmEventId);
		Route->SetStringField(TEXT("bgm_track_name"), Voice.CurrentBgmTrackName);
		Route->SetStringField(TEXT("bgm_sound_path"), Voice.CurrentBgmSoundObjectPath);
		Route->SetBoolField(TEXT("bgm_component_playing"), Voice.bBgmComponentPlaying);
		Route->SetNumberField(TEXT("bgm_component_volume"), Voice.BgmComponentVolumeMultiplier);
		Route->SetNumberField(TEXT("bgm_track_started_world_seconds"), Voice.BgmTrackStartedWorldSeconds);
		Route->SetNumberField(TEXT("bgm_track_elapsed_seconds"), Voice.BgmTrackElapsedSeconds);
		Route->SetNumberField(TEXT("bgm_naturally_finished_count"), Voice.BgmNaturallyFinishedCount);
		Route->SetNumberField(TEXT("bgm_pending_next_delay"), Voice.PendingNextDelaySeconds);
		Route->SetNumberField(TEXT("active_sfx_components"), Voice.ActiveSfxComponents);
		Route->SetNumberField(TEXT("feedback_sequence"), static_cast<double>(Voice.LastSfxFeedbackSequence));
		Route->SetNumberField(TEXT("feedback_pre_change_volume"), Voice.LastSfxFeedbackPreChangeVolume);
		Route->SetNumberField(TEXT("feedback_component_volume"), Voice.LastSfxFeedbackComponentVolumeMultiplier);
		Route->SetNumberField(TEXT("feedback_current_volume"), Voice.LastSfxFeedbackCurrentVolumeMultiplier);
		Route->SetBoolField(TEXT("feedback_currently_playing"), Voice.bLastSfxFeedbackCurrentlyPlaying);
		Route->SetStringField(TEXT("feedback_sound_path"), Voice.LastSfxFeedbackSoundObjectPath);
		Route->SetBoolField(TEXT("feedback_component_created"), Voice.bLastSfxFeedbackComponentCreated);
		Json->SetObjectField(TEXT("route"), Route);
	}
	const auto* Branch = GConfig ? GConfig->FindBranch(FName(*GGameUserSettingsIni), FString()) : nullptr;
	if (Branch && !Branch->IniPath.IsEmpty())
	{
		FConfigFile Disk;
		Disk.Read(Branch->IniPath);
		const FString Section = URokClientSettingsConfig::StaticClass()->GetPathName();
		float DiskSfx = -1.0f, DiskMusic = -1.0f;
		const bool bSfx = Disk.GetFloat(*Section, TEXT("SfxVolume"), DiskSfx);
		const bool bMusic = Disk.GetFloat(*Section, TEXT("MusicVolume"), DiskMusic);
		Json->SetStringField(TEXT("settings_disk_path"), Branch->IniPath);
		Json->SetStringField(TEXT("settings_disk_sha256"), FileSha(Branch->IniPath));
		Json->SetBoolField(TEXT("settings_disk_values_found"), bSfx && bMusic);
		Json->SetNumberField(TEXT("disk_sfx"), DiskSfx);
		Json->SetNumberField(TEXT("disk_music"), DiskMusic);
	}
	if (Screen.IsValid())
	{
		const TCHAR* Names[] = {TEXT("FeatureModeCanvas"), TEXT("SettingsHubGeneralButton"),
			TEXT("GeneralSettingsScroll"), TEXT("GeneralSettingsAudioItem"),
			TEXT("GeneralSettingsSfxTitle"), TEXT("GeneralSettingsSfxDescription"),
			TEXT("GeneralSettingsMusicTitle"), TEXT("GeneralSettingsMusicDescription"),
			TEXT("GeneralSettingsSfxFrame"), TEXT("GeneralSettingsSfxBackground"),
			TEXT("GeneralSettingsSfxFill"), TEXT("GeneralSettingsSfxIconLeft"),
			TEXT("GeneralSettingsSfxIconRight"), TEXT("GeneralSettingsSfxSlider"),
			TEXT("GeneralSettingsMusicFrame"), TEXT("GeneralSettingsMusicBackground"),
			TEXT("GeneralSettingsMusicFill"), TEXT("GeneralSettingsMusicIconLeft"),
			TEXT("GeneralSettingsMusicIconRight"), TEXT("GeneralSettingsMusicSlider"),
			TEXT("FeatureFrameClose")};
		auto Widgets = MakeShared<FJsonObject>();
		for (const TCHAR* Name : Names) Widgets->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(Name)));
		Json->SetObjectField(TEXT("widgets"), Widgets);
		if (const auto* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("GeneralSettingsScroll"))))
		{
			Json->SetNumberField(TEXT("scroll_offset"), Scroll->GetScrollOffset());
			Json->SetNumberField(TEXT("scroll_end"), Scroll->GetScrollOffsetOfEnd());
		}
	}
	return Json;
}

bool URokAudioSettingsAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(),
		TEXT("Audio audit project/world/Slate identity invalid"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	while (FParse::Token(Cursor, Token, false))
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& !Token.Equals(MainFlag, ESearchCase::IgnoreCase)
			&& !Token.Equals(ResumeFlag, ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("-RokCityBgmNight"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Audio audit cannot share process with other Rok flags"));
	Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
	Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Runtime = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	Server = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	Settings = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokClientSettingsSubsystem>() : nullptr;
	Audio = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokAudioRouterSubsystem>() : nullptr;
	TArray<UUserWidget*> Found;
	const auto Find = [&](UClass* Class) -> UUserWidget*
	{
		Found.Reset();
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, Class, false);
		UUserWidget* Result = nullptr;
		int32 Count = 0;
		for (UUserWidget* Widget : Found) if (Widget && Widget->GetWorld() == GetWorld()) { Result = Widget; ++Count; }
		return Count == 1 ? Result : nullptr;
	};
	Hud = Cast<URokMainHudWidget>(Find(URokMainHudWidget::StaticClass()));
	Modal = Cast<URokUnityModalWidget>(Find(URokUnityModalWidget::StaticClass()));
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	if (!Expect(Controller.IsValid() && Buildings.IsValid() && Runtime.IsValid()
		&& Server.IsValid() && Settings.IsValid() && Settings->AreSettingsLoaded()
		&& Audio.IsValid() && Audio->AreSourceEventsLoaded()
		&& Hud.IsValid() && Modal.IsValid() && Screen.IsValid()
		&& Visible(Hud.Get()) && !Modal->IsModalOpen() && !Screen->IsScreenOpen()
		&& Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), 0.1),
		TEXT("Audio audit requires idle local-only 1440x810 city HUD"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId
		&& Profile.RoleId.IsEmpty() && !Profile.bHasCivilization
		&& Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Audio audit account identity is not local preview"))) return false;
	const auto* ScalabilityBranch = GConfig ? GConfig->FindBranch(FName(*GScalabilityIni), FString()) : nullptr;
	if (!Expect(ScalabilityBranch && !ScalabilityBranch->IniPath.IsEmpty(),
		TEXT("Audio audit cannot resolve scalability preference path"))) return false;
	ScalabilityPath = FPaths::ConvertRelativePathToFull(ScalabilityBranch->IniPath);
	InitialScalabilityHash = FileSha(ScalabilityPath);
	InitialGraphics = static_cast<float>(Settings->GetGraphicsQualityTier());
	InitialFrameRate = static_cast<float>(Settings->GetFrameRateOption());
	ExpectedSfx = Settings->GetSfxVolume();
	ExpectedMusic = Settings->GetMusicVolume();
	if (bResume && !Expect(FMath::IsNearlyEqual(ExpectedSfx, .37f, .006f)
		&& FMath::IsNearlyEqual(ExpectedMusic, .37f, .006f),
		TEXT("Restarted settings did not load prior 0.37 values"))) return false;
	const FRokAudioRouterVoiceSnapshot Voice = Audio->GetVoiceSnapshot();
	if (!Expect(Voice.CityBgmEventId == 1008 && Voice.bBgmComponentPlaying
		&& Voice.BgmComponentVolumeMultiplier >= 0.0f
		&& FMath::IsNearlyEqual(Voice.BgmComponentVolumeMultiplier, ExpectedMusic, .006f)
		&& FMath::IsNearlyEqual(Voice.SfxSetting, ExpectedSfx, .006f)
		&& !Voice.CurrentBgmSoundObjectPath.IsEmpty(),
		TEXT("Audio audit requires real active night BGM component at persisted volume"))) return false;
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	BaselineBusiness = Encode(BusinessState());
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokAudioSettingsAuditSubsystem.h"),
		TEXT("Source/Rok/RokAudioSettingsAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokAudioSettingsRuntime.py"),
		TEXT("Source/Rok/RokAudioRouterSubsystem.h"),
		TEXT("Source/Rok/RokAudioRouterSubsystem.cpp"),
		TEXT("Source/Rok/RokPlayerController.cpp"),
		TEXT("Source/Rok/RokClientSettingsSubsystem.h"),
		TEXT("Source/Rok/RokClientSettingsSubsystem.cpp"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokWorldMapHudWidget.cpp"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/general_settings_audio_ui_implementation_v1.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/audio_router_implementation_revision2.json"),
		TEXT("Saved/UnityCityAudioProbeV1/OutputRuns/20261009_053128_273_40300/native_city_audio.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/city_audio_probe_native_verify_20261009_053128_273_40300.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/city_bgm_import_prepare_revision2.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/city_bgm_import_20261009T054000384701Z_42692.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/root_city_bgm_asset_import_acceptance_v1.json"),
		TEXT("Saved/RokUnityMigration/CityAudio/catalog.json"),
		TEXT("Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceDay01.uasset"),
		TEXT("Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceDay02.uasset"),
		TEXT("Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceNight01.uasset"),
		TEXT("Content/RokPrototype/UI/Audio/CityBgm/Bgm_PeaceNight02.uasset"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/audio_route_source_contract_v1.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/batch_start.json")};
	for (const TCHAR* Path : Required)
	{
		const FString Hash = FileSha(RokDataPaths::ResolveProjectPath(Path));
		if (!Expect(Hash.Len() == 64, TEXT("Audio audit source input missing"))) return false;
		Inputs->SetStringField(Path, Hash);
	}
	if (!ExportImportedPcm()) return false;
	return Boundary();
}
bool URokAudioSettingsAuditSubsystem::ExportImportedPcm()
{
#if WITH_EDITOR
	constexpr const TCHAR* Names[] = {TEXT("Bgm_PeaceDay01"), TEXT("Bgm_PeaceDay02"),
		TEXT("Bgm_PeaceNight01"), TEXT("Bgm_PeaceNight02")};
	for (const TCHAR* Name : Names)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/RokPrototype/UI/Audio/CityBgm/%s.%s"), Name, Name);
		const USoundWave* Wave = LoadObject<USoundWave>(nullptr, *ObjectPath);
		if (!Expect(Wave && Wave->GetSoundAssetCompressionTypeEnum() == ESoundAssetCompressionType::PCM,
			TEXT("City BGM imported SoundWave absent or not explicit PCM compression"))) return false;
		TArray<uint8> RawPcm;
		uint32 SampleRate = 0;
		uint16 Channels = 0;
		if (!Expect(Wave->GetImportedSoundWaveData(RawPcm, SampleRate, Channels)
			&& SampleRate > 0 && Channels > 0 && RawPcm.Num() > 0
			&& RawPcm.Num() % (sizeof(int16) * Channels) == 0,
			TEXT("City BGM GetImportedSoundWaveData failed or is not interleaved PCM16"))) return false;
		const FString BinPath = Directory / FString::Printf(TEXT("imported_%s.pcm16le"), Name);
		if (!Expect(FFileHelper::SaveArrayToFile(RawPcm, *BinPath),
			TEXT("City BGM imported PCM evidence could not be saved"))) return false;
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Name);
		Entry->SetStringField(TEXT("ue_object_path"), ObjectPath);
		Entry->SetStringField(TEXT("compression_enum"), TEXT("PCM"));
		Entry->SetNumberField(TEXT("sample_rate"), SampleRate);
		Entry->SetNumberField(TEXT("channels"), Channels);
		Entry->SetNumberField(TEXT("pcm_bytes"), RawPcm.Num());
		Entry->SetNumberField(TEXT("sample_frames"), RawPcm.Num() / (sizeof(int16) * Channels));
		Entry->SetStringField(TEXT("pcm_sha256"), FileSha(BinPath));
		Entry->SetStringField(TEXT("pcm_path"), BinPath);
		ImportedPcm.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return ImportedPcm.Num() == 4;
#else
	return Expect(false, TEXT("City BGM imported PCM readback requires WITH_EDITOR"));
#endif
}
bool URokAudioSettingsAuditSubsystem::Ready(int32 Mode) const
{
	if (!Boundary() || !Hud.IsValid() || !Modal.IsValid() || !Screen.IsValid()) return false;
	if (Mode == 0) return !Modal->IsModalOpen() && !Screen->IsScreenOpen();
	if (Mode == 1) return Modal->IsModalOpen() && !Screen->IsScreenOpen()
		&& Visible(Modal->GetWidgetFromName(TEXT("ProfileSettingsButton")))
		&& !Visible(Modal->GetWidgetFromName(TEXT("ModalAnimationInputBlocker")));
	if (Mode == 2) return !Modal->IsModalOpen() && Screen->IsScreenOpen()
		&& Visible(Screen->GetWidgetFromName(TEXT("SettingsHubGeneralButton")))
		&& Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))
		&& FMath::IsNearlyEqual(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))->GetRenderOpacity(), 1.0f, .01f)
		&& Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))->GetRenderTransform().Scale.Equals(FVector2D(1, 1), .01);
	if (Mode == 3)
	{
		const UWidget* Canvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
		const UWidget* Sfx = Screen->GetWidgetFromName(TEXT("GeneralSettingsSfxSlider"));
		const UWidget* Music = Screen->GetWidgetFromName(TEXT("GeneralSettingsMusicSlider"));
		const UScrollBox* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("GeneralSettingsScroll")));
		return !Modal->IsModalOpen() && Screen->IsScreenOpen() && Visible(Canvas)
			&& FMath::IsNearlyEqual(Canvas->GetRenderOpacity(), 1.0f, .01f)
			&& Canvas->GetRenderTransform().Scale.Equals(FVector2D(1, 1), .01)
			&& Visible(Sfx) && Visible(Music)
			&& Cast<USlider>(Sfx) && Cast<USlider>(Music) && Scroll
			&& FMath::IsNearlyEqual(Cast<USlider>(Sfx)->GetValue(), ExpectedSfx, .006f)
			&& FMath::IsNearlyEqual(Cast<USlider>(Music)->GetValue(), ExpectedMusic, .006f)
			&& FMath::IsNearlyEqual(Settings->GetSfxVolume(), ExpectedSfx, .006f)
			&& FMath::IsNearlyEqual(Settings->GetMusicVolume(), ExpectedMusic, .006f)
			&& FMath::IsNearlyEqual(Audio->GetSfxVolume(), ExpectedSfx, .006f)
			&& FMath::IsNearlyEqual(Audio->GetMusicVolume(), ExpectedMusic, .006f);
	}
	return false;
}
bool URokAudioSettingsAuditSubsystem::Click(UUserWidget* Owner, const TCHAR* Name)
{
	UButton* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!Expect(Boundary() && Owner && Owner->GetWorld() == GetWorld()
		&& Visible(Button) && Button->GetIsEnabled() && FSlateApplication::IsInitialized(),
		TEXT("Audio audit Slate button unavailable"))) return false;
	const FGeometry Geometry = Button->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("Audio audit button geometry absent"))) return false;
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
	FSlateApplication& App = FSlateApplication::Get();
	const bool bFocused = OwnWindowFocus();
	if (bFinished) return false;
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	TArray<TSharedPtr<FJsonValue>> Types;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
		Types.Add(MakeShared<FJsonValueString>(Path.Widgets[Index].Widget->GetTypeAsString()));
	const FString Terminal = Path.IsValid() && Path.Widgets.Num()
		? Path.Widgets.Last().Widget->GetTypeAsString() : FString();
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	const bool bHit = Path.IsValid() && Path.ContainsWidget(&Button->TakeWidget().Get())
		&& Terminal.Contains(TEXT("SButton"));
	if (!(bFocused && bHit && Button->IsHovered() && bOwn && bForeground))
		return RetryInput(TEXT("button"), Name, Center, Types, Terminal,
			bFocused, bOwn, bForeground, bHit, Button->IsHovered());
	InputRetryKey.Empty();
	InputRetryCount = 0;
	const uint64 Before = BaselinePrimary;
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("kind"), TEXT("button"));
	Action->SetStringField(TEXT("selector"), Name);
	Action->SetStringField(TEXT("method"), TEXT("Slate mouse down/up"));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetObjectField(TEXT("absolute_center"), Point(Center));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetStringField(TEXT("terminal_type"), Terminal);
	Action->SetBoolField(TEXT("own_game_window"), bOwn);
	Action->SetBoolField(TEXT("foreground"), bForeground);
	Action->SetBoolField(TEXT("target_in_hit_path"), true);
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetNumberField(TEXT("primary_before"), static_cast<double>(Before));
	Action->SetNumberField(TEXT("primary_after"), static_cast<double>(
		Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Down && Up && Boundary(), TEXT("Audio audit button leaked to world or changed business state"));
}
bool URokAudioSettingsAuditSubsystem::WheelToAudio()
{
	UScrollBox* Scroll = Screen.IsValid()
		? Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("GeneralSettingsScroll"))) : nullptr;
	if (!Expect(Ready(3) && Scroll && Scroll->GetScrollOffsetOfEnd() > 0,
		TEXT("Audio audit source scroll unavailable"))) return false;
	const FGeometry Geometry = Scroll->GetCachedGeometry();
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
	FSlateApplication& App = FSlateApplication::Get();
	const bool bFocused = OwnWindowFocus();
	if (bFinished) return false;
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	bool bScroll = false;
	TArray<TSharedPtr<FJsonValue>> Types;
	FString Terminal;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		bScroll |= Type.Contains(TEXT("SScrollBox"));
		Terminal = Type;
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Path.IsValid() && Window.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	if (!(bFocused && bOwn && bScroll && bForeground))
		return RetryInput(TEXT("wheel"), TEXT("GeneralSettingsScroll"), Center, Types,
			Terminal, bFocused, bOwn, bForeground, bScroll, true);
	InputRetryKey.Empty();
	InputRetryCount = 0;
	const double Before = Scroll->GetScrollOffset();
	const bool Handled = App.ProcessMouseWheelOrGestureEvent(
		FPointerEvent(0, Center, Center, TSet<FKey>(), EKeys::Invalid, -3.0f,
			FModifierKeysState()), nullptr);
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("kind"), TEXT("wheel"));
	Action->SetStringField(TEXT("selector"), TEXT("GeneralSettingsScroll"));
	Action->SetStringField(TEXT("method"), TEXT("Slate mouse wheel"));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetBoolField(TEXT("own_game_window"), bOwn);
	Action->SetBoolField(TEXT("foreground"), bForeground);
	Action->SetBoolField(TEXT("scroll_hit_path"), bScroll);
	Action->SetBoolField(TEXT("handled"), Handled);
	Action->SetNumberField(TEXT("offset_before"), Before);
	Action->SetNumberField(TEXT("offset_after_dispatch"), Scroll->GetScrollOffset());
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Handled && Boundary(), TEXT("Audio audit wheel unhandled or changed business state"));
}
bool URokAudioSettingsAuditSubsystem::Slide(const TCHAR* Name, float TargetValue)
{
	USlider* Slider = Screen.IsValid() ? Cast<USlider>(Screen->GetWidgetFromName(Name)) : nullptr;
	UScrollBox* Scroll = Screen.IsValid()
		? Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("GeneralSettingsScroll"))) : nullptr;
	if (!Expect(Ready(3) && Slider && Scroll && Slider->GetIsEnabled()
		&& TargetValue >= 0 && TargetValue <= 1,
		TEXT("Audio audit actual slider unavailable"))) return false;
	const FGeometry Geometry = Slider->GetCachedGeometry();
	const FGeometry ViewGeometry = Scroll->GetCachedGeometry();
	const FVector2D ViewTop = ViewGeometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D ViewBottom = ViewGeometry.LocalToAbsolute(ViewGeometry.GetLocalSize());
	const float LocalX = 11.0f + TargetValue * (Geometry.GetLocalSize().X - 22.0f);
	const FVector2D PointAbsolute = Geometry.LocalToAbsolute(FVector2D(LocalX, Geometry.GetLocalSize().Y * .5f));
	if (!Expect(Geometry.GetLocalSize().X > 800 && Geometry.GetLocalSize().Y > 20
		&& PointAbsolute.Y >= ViewTop.Y + 2 && PointAbsolute.Y <= ViewBottom.Y - 2,
		TEXT("Audio audit slider input point is clipped"))) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const bool bFocused = OwnWindowFocus();
	if (bFinished) return false;
	App.SetCursorPos(PointAbsolute);
	App.ProcessMouseMoveEvent(FPointerEvent(0, PointAbsolute, LastPointer,
		TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = PointAbsolute;
	const FWidgetPath Path = App.LocateWindowUnderMouse(PointAbsolute,
		App.GetInteractiveTopLevelWindows(), false, 0);
	TArray<TSharedPtr<FJsonValue>> Types;
	bool bSlider = false;
	FString Terminal;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		bSlider |= Type.Contains(TEXT("SSlider"));
		Terminal = Type;
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Path.IsValid() && Window.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	const bool bHit = Path.IsValid() && Path.ContainsWidget(&Slider->TakeWidget().Get()) && bSlider;
	if (!(bFocused && bHit && bOwn && bForeground && Slider->IsHovered()))
		return RetryInput(TEXT("slider"), Name, PointAbsolute, Types, Terminal,
			bFocused, bOwn, bForeground, bHit, Slider->IsHovered());
	InputRetryKey.Empty();
	InputRetryCount = 0;
	const FRokAudioRouterVoiceSnapshot BeforeVoice = Audio->GetVoiceSnapshot();
	const float BeforeSfx = Settings->GetSfxVolume();
	const float BeforeMusic = Settings->GetMusicVolume();
	const float BeforeValue = Slider->GetValue();
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, PointAbsolute, PointAbsolute, Pressed,
			EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, PointAbsolute,
		PointAbsolute, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool bSfx = FString(Name).Contains(TEXT("Sfx"));
	const float Actual = Slider->GetValue();
	if (bSfx) ExpectedSfx = Actual; else ExpectedMusic = Actual;
	const FRokAudioRouterVoiceSnapshot AfterVoice = Audio->GetVoiceSnapshot();
	if (!bSfx && FMath::IsNearlyEqual(TargetValue, 0.0f, .001f))
	{
		ZeroTrackPath = AfterVoice.CurrentBgmSoundObjectPath;
		ZeroFinishedCount = AfterVoice.BgmNaturallyFinishedCount;
		ZeroTrackStartedWorldSeconds = AfterVoice.BgmTrackStartedWorldSeconds;
	}
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("kind"), TEXT("slider"));
	Action->SetStringField(TEXT("selector"), Name);
	Action->SetStringField(TEXT("method"), TEXT("Slate SSlider mouse down/up"));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetNumberField(TEXT("target_value"), TargetValue);
	Action->SetNumberField(TEXT("value_before"), BeforeValue);
	Action->SetNumberField(TEXT("value_after"), Actual);
	Action->SetNumberField(TEXT("sfx_before"), BeforeSfx);
	Action->SetNumberField(TEXT("music_before"), BeforeMusic);
	Action->SetObjectField(TEXT("absolute_point"), Point(PointAbsolute));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetBoolField(TEXT("own_game_window"), bOwn);
	Action->SetBoolField(TEXT("foreground"), bForeground);
	Action->SetBoolField(TEXT("slider_hit_path"), bSlider);
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetNumberField(TEXT("feedback_sequence_before"), static_cast<double>(BeforeVoice.LastSfxFeedbackSequence));
	Action->SetNumberField(TEXT("feedback_sequence_after"), static_cast<double>(AfterVoice.LastSfxFeedbackSequence));
	Action->SetNumberField(TEXT("feedback_pre_change_volume"), AfterVoice.LastSfxFeedbackPreChangeVolume);
	Action->SetNumberField(TEXT("feedback_component_volume"), AfterVoice.LastSfxFeedbackComponentVolumeMultiplier);
	Action->SetStringField(TEXT("feedback_sound_path"), AfterVoice.LastSfxFeedbackSoundObjectPath);
	Action->SetBoolField(TEXT("feedback_component_created"), AfterVoice.bLastSfxFeedbackComponentCreated);
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Down && Up && Boundary()
		&& FMath::IsNearlyEqual(Actual, TargetValue, .006f)
		&& AfterVoice.LastSfxFeedbackSequence == BeforeVoice.LastSfxFeedbackSequence + 1
		&& FMath::IsNearlyEqual(AfterVoice.LastSfxFeedbackPreChangeVolume, BeforeSfx, .006f)
		&& AfterVoice.bLastSfxFeedbackComponentCreated
		&& FMath::IsNearlyEqual(AfterVoice.LastSfxFeedbackComponentVolumeMultiplier, BeforeSfx, .006f)
		&& AfterVoice.LastSfxFeedbackSoundObjectPath.Contains(TEXT("Sound_Ui_CommonScroll"))
		&& FMath::IsNearlyEqual(Settings->GetSfxVolume(), ExpectedSfx, .006f)
		&& FMath::IsNearlyEqual(Settings->GetMusicVolume(), ExpectedMusic, .006f)
		&& FMath::IsNearlyEqual(Audio->GetSfxVolume(), ExpectedSfx, .006f)
		&& FMath::IsNearlyEqual(Audio->GetMusicVolume(), ExpectedMusic, .006f),
		TEXT("Audio slider failed real-input, old-SFX feedback or route settings check"));
}

void URokAudioSettingsAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
		&& Boundary(), TEXT("Audio audit screenshot request invalid"))) return;
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetObjectField(TEXT("requested_state"), State());
	Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	Pending->SetStringField(TEXT("png"), PendingPath);
	NextStep = After;
	bScreenshotDone = false;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}
void URokAudioSettingsAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	const FString Hash = FileSha(PendingPath);
	if (!Expect(Hash.Len() == 64 && Boundary(),
		TEXT("Audio audit PNG/source state absent during screenshot"))) return;
	Pending->SetStringField(TEXT("png_sha256"), Hash);
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(IFileManager::Get().FileSize(*PendingPath)));
	Pending->SetObjectField(TEXT("processed_state"), State());
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokAudioSettingsAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(NextStep);
}
void URokAudioSettingsAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep;
	StepSince = Elapsed;
	StableFrames = 0;
	InputRetryKey.Empty();
	InputRetryCount = 0;
}
bool URokAudioSettingsAuditSubsystem::Save(bool Complete) const
{
	const int32 RequiredCaptures = bResume ? 4 : 17;
	const int32 RequiredActions = bResume ? 5 : 16;
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.audio_settings.runtime_audit.v1"));
	Json->SetStringField(TEXT("stage"), bResume ? TEXT("resume") : TEXT("primary"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty()
		&& Captures.Num() == RequiredCaptures && Actions.Num() == RequiredActions);
	Json->SetBoolField(TEXT("cross_process_resume_required_for_final_acceptance"), true);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), TEXT("Local-only real HUD/Slate source audio settings, tracked SFX feedback and active city BGM component; no account, reward, queue, HTTP or graphics mutation"));
	Json->SetStringField(TEXT("scalability_path"), ScalabilityPath);
	Json->SetStringField(TEXT("scalability_sha256_initial"), InitialScalabilityHash);
	Json->SetStringField(TEXT("scalability_sha256_final"), FileSha(ScalabilityPath));
	Json->SetStringField(TEXT("zero_start_track_path"), ZeroTrackPath);
	Json->SetNumberField(TEXT("zero_start_finished_count"), ZeroFinishedCount);
	Json->SetNumberField(TEXT("zero_start_track_started_world_seconds"), ZeroTrackStartedWorldSeconds);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("imported_pcm"), ImportedPcm);
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("input_attempts"), InputAttempts);
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetObjectField(TEXT("final_state"), State());
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokAudioSettingsAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && Boundary()
		&& Captures.Num() == (bResume ? 4 : 17)
		&& Actions.Num() == (bResume ? 5 : 16)
		&& FMath::IsNearlyEqual(Settings->GetSfxVolume(), .37f, .006f)
		&& FMath::IsNearlyEqual(Settings->GetMusicVolume(), .37f, .006f);
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Audio audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok Audio Settings audit %s: %s"),
		Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!Complete) UE_LOG(LogTemp, Error, TEXT("Audio Settings audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokAudioSettingsAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !Identity()) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (Step == 0 && Elapsed > 30.0) { Finish(TEXT("Audio audit startup timeout")); return; }
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 25.0) Finish(TEXT("Audio audit screenshot timeout"));
		else AdvanceCapture();
		return;
	}
	if (Step > 0 && Elapsed - StepSince > 35.0)
	{
		Finish(FString::Printf(TEXT("Audio audit step %d timeout"), Step));
		return;
	}
	const auto Stable = [this](int32 Mode)
	{
		StableFrames = Ready(Mode) ? StableFrames + 1 : 0;
		return StableFrames >= 2;
	};
	if (Step == 0)
	{
		if (!bBegun) bBegun = Begin();
		if (bBegun && OwnWindowFocus()) Enter(1);
		return;
	}
	if (bResume)
	{
		switch (Step)
		{
		case 1: if (Stable(0)) Capture(TEXT("00_resume_hud"), 2); break;
		case 2: if (Click(Hud.Get(), TEXT("PlayerProfileHitButton"))) { ExpectedMode = 1; Enter(3); } break;
		case 3: if (Stable(1) && Click(Modal.Get(), TEXT("ProfileSettingsButton"))) { ExpectedMode = 2; Enter(4); } break;
		case 4: if (Stable(2) && Click(Screen.Get(), TEXT("SettingsHubGeneralButton"))) { ExpectedMode = 3; Enter(5); } break;
		case 5: if (Stable(3)) Capture(TEXT("01_resume_general_top"), 6); break;
		case 6: if (WheelToAudio()) Enter(7); break;
		case 7: if (Stable(3) && Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("GeneralSettingsScroll")))->GetScrollOffset() > 20)
			Capture(TEXT("02_resume_audio_scrolled"), 8); break;
		case 8: if (Click(Screen.Get(), TEXT("FeatureFrameClose"))) { ExpectedMode = 0; Enter(9); } break;
		case 9: if (Stable(0)) Capture(TEXT("03_resume_closed"), 10); break;
		case 10: Finish(); break;
		default: Finish(TEXT("Audio resume state invalid")); break;
		}
		return;
	}
	switch (Step)
	{
	case 1: if (Stable(0)) Capture(TEXT("00_hud"), 2); break;
	case 2: if (Click(Hud.Get(), TEXT("PlayerProfileHitButton"))) { ExpectedMode = 1; Enter(3); } break;
	case 3: if (Stable(1)) Capture(TEXT("01_profile"), 4); break;
	case 4: if (Click(Modal.Get(), TEXT("ProfileSettingsButton"))) { ExpectedMode = 2; Enter(5); } break;
	case 5: if (Stable(2)) Capture(TEXT("02_settings_hub"), 6); break;
	case 6: if (Click(Screen.Get(), TEXT("SettingsHubGeneralButton"))) { ExpectedMode = 3; Enter(7); } break;
	case 7: if (Stable(3)) Capture(TEXT("03_general_top"), 8); break;
	case 8: if (Slide(TEXT("GeneralSettingsSfxSlider"), 0.0f)) Enter(9); break;
	case 9: if (Stable(3)) Capture(TEXT("04_sfx_zero"), 10); break;
	case 10: if (Slide(TEXT("GeneralSettingsSfxSlider"), .37f)) Enter(11); break;
	case 11: if (Stable(3)) Capture(TEXT("05_sfx_fraction"), 12); break;
	case 12: if (Slide(TEXT("GeneralSettingsSfxSlider"), 1.0f)) Enter(13); break;
	case 13: if (Stable(3)) Capture(TEXT("06_sfx_full"), 14); break;
	case 14: if (Slide(TEXT("GeneralSettingsSfxSlider"), .37f)) Enter(15); break;
	case 15: if (Stable(3)) Capture(TEXT("07_sfx_final"), 16); break;
	case 16: if (WheelToAudio()) Enter(17); break;
	case 17: if (Stable(3) && Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("GeneralSettingsScroll")))->GetScrollOffset() > 20)
		Capture(TEXT("08_audio_scrolled"), 18); break;
	case 18: if (Slide(TEXT("GeneralSettingsMusicSlider"), 0.0f)) Enter(19); break;
	case 19: if (Stable(3)) Capture(TEXT("09_music_zero"), 20); break;
	case 20:
	{
		const FRokAudioRouterVoiceSnapshot Voice = Audio->GetVoiceSnapshot();
		if (Stable(3) && ZeroFinishedCount >= 0
			&& Voice.BgmNaturallyFinishedCount > ZeroFinishedCount
			&& Voice.CurrentBgmSoundObjectPath != ZeroTrackPath
			&& Voice.BgmTrackStartedWorldSeconds > ZeroTrackStartedWorldSeconds
			&& Voice.BgmTrackElapsedSeconds > .3
			&& Voice.bBgmComponentPlaying
			&& FMath::IsNearlyEqual(Voice.BgmComponentVolumeMultiplier, 0.0f, .006f))
			Capture(TEXT("10_music_zero_next_track"), 21);
		break;
	}
	case 21: if (Slide(TEXT("GeneralSettingsMusicSlider"), .37f)) Enter(22); break;
	case 22: if (Stable(3)) Capture(TEXT("11_music_fraction"), 23); break;
	case 23: if (Slide(TEXT("GeneralSettingsMusicSlider"), 1.0f)) Enter(24); break;
	case 24: if (Stable(3)) Capture(TEXT("12_music_full"), 25); break;
	case 25: if (Slide(TEXT("GeneralSettingsMusicSlider"), .37f)) Enter(26); break;
	case 26: if (Stable(3)) Capture(TEXT("13_music_final"), 27); break;
	case 27: if (Click(Screen.Get(), TEXT("FeatureFrameClose"))) { ExpectedMode = 0; Enter(28); } break;
	case 28: if (Stable(0)) Capture(TEXT("14_closed"), 29); break;
	case 29: if (Click(Hud.Get(), TEXT("PlayerProfileHitButton"))) { ExpectedMode = 1; Enter(30); } break;
	case 30: if (Stable(1) && Click(Modal.Get(), TEXT("ProfileSettingsButton"))) { ExpectedMode = 2; Enter(31); } break;
	case 31: if (Stable(2)) Capture(TEXT("15_settings_hub_reopened"), 32); break;
	case 32: if (Click(Screen.Get(), TEXT("SettingsHubGeneralButton"))) { ExpectedMode = 3; Enter(33); } break;
	case 33: if (Stable(3)) Capture(TEXT("16_general_reopened"), 34); break;
	case 34: Finish(); break;
	default: Finish(TEXT("Audio primary state invalid")); break;
	}
}
