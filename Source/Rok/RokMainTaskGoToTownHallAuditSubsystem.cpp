#include "RokMainTaskGoToTownHallAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
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
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokMainTaskGoToTownHallAudit");

struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Source five and V2 product eight are immutable review inputs. Audit/verifier
// files are also hashed at runtime and compared against the staged release report.
constexpr FInputPin Pinned[] = {
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs"), TEXT("1ac142e1d65155320eb73558a3402d2d8e961e663d545c5fde491cabb7a95a60")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/TaskScriptGlobalMediator.cs"), TEXT("43e328f7897e7248096f9bea5efe1b96e3ce60aa1b62b0cdeb8a4ef2cb38e58d")},
	{TEXT("ref/config-tables/编译客户端配置/CSV/TaskMain.csv"), TEXT("704a6fb0dd5d8601094f7207ea15f2b209d5be9ed2d0a778839d940bcac08b91")},
	{TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json"), TEXT("07fcf9600d8e79afba470ef92c71460dd217e82e22dfaafc236a4fd58b5b0994")},
	{TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"), TEXT("2633514629f9e39744226b2665f6f30e1c2d007e46a7d2ac019a45d72e498804")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("d782e3b7a0f69af84068e2ed3b1ddeb285c141b96e8d714aebee12907971fe5f")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("388221ef002653184e5d155cf5afe319730a5398008d4234ef3f8925a352aec1")},
	{TEXT("Source/Rok/RokMainHudWidget.h"), TEXT("2740efb6b5c8a68222d5429f4d66e37e8e484302a215b6c46bbd8c783f67a2e8")},
	{TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
	{TEXT("Source/Rok/RokBuildingUpgradeSubsystem.h"), TEXT("b2253b3fa8232a873e6f4f07fe4cd0f3127eb4cbd3c720a356794186e93ea7b4")},
	{TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"), TEXT("b20503a0b0dfdfe552502ef307e6d8e6a79c9677e7a185fc0f589cd6c45a8ae5")},
	{TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("50cec5ddbdba9db12730edad021b8f359477d2b596a6504bc554dfda8ac220d3")},
	{TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9c2fca7d169a40b8a5a10ba4ef036b07a6fc76719871f35451c6f49be800171b")},
	{TEXT("Source/Rok/RokPlayerController.h"), TEXT("6eaa6329a03804c3af143c9777e8654d5dd83cfe24f598779b6107c988ffe772")},
	{TEXT("Source/Rok/RokPlayerController.cpp"), TEXT("f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"), TEXT("e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"), TEXT("3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e")},
	{TEXT("Source/Rok/RokServerSubsystem.h"), TEXT("4a07946d525ccb9eaedafcc30e68315e4ca5c52e94a88fc4c9b388833a07a5a9")},
	{TEXT("Source/Rok/RokServerSubsystem.cpp"), TEXT("3d73a28ff42b787c02a5c5a50ceba5bcac47056cc0945608bfea9ff9bd91bde4")},
	{TEXT("Source/Rok/RokUiAnimatedButton.h"), TEXT("4379cd36d5fba08d3b8c0fbf5c1076b8ce8b8cb27d2fec3c7fc2a4a65c62a3f7")},
	{TEXT("Source/Rok/RokUiAnimatedButton.cpp"), TEXT("bf8da24bdce3dfb403b7174eaa496e95550547cc47c57dccaca6e57aa39bedcf")},
	{TEXT("Source/Rok/RokUiWindowAnimation.h"), TEXT("1c39a74fd70ecec8f1e9b2fd135f65b312e86f2be60ebf323596d8a8359f9e3f")},
	{TEXT("Source/Rok/RokUiWindowAnimation.cpp"), TEXT("bae58440eccb1ba826871acee2131b1223094b134cf0cfe04a730c798e2b41e4")},
	{TEXT("Source/Rok/RokTrainArmyMeshData.h"), TEXT("e0c1e4f9f698366526ac740fd60f238076dc2c5ebc0e5ea85299a92fa9072945")},
	{TEXT("Source/Rok/RokTrainArmyMeshData.cpp"), TEXT("e3dfd99ea973b2eba075c117d326ed16ddb44b5188c08740180c4d31c8593a9a")},
	{TEXT("Saved/PendingMainTaskGoToTownHallV2/implementation_report.json"), TEXT("e8624b9a9c9cc04ed2e0b2f1ad79a0291328882a661c1f954a2e80719b1fed12")},
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

bool URokMainTaskGoToTownHallAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false; // Pending-completion count is only exposed to audited Editor builds.
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokMainTaskGoToTownHallAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokMainTaskGoToTownHallAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokMainTaskGoToTownHallAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokMainTaskGoToTownHallAuditSubsystem, STATGROUP_Tickables);
}
void URokMainTaskGoToTownHallAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/MainTaskGoToTownHallRuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokMainTaskGoToTownHallAuditSubsystem::ScreenshotProcessed);
}
void URokMainTaskGoToTownHallAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokMainTaskGoToTownHallAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokMainTaskGoToTownHallAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokMainTaskGoToTownHallAuditSubsystem::PrepareOwnWindowFocus()
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

bool URokMainTaskGoToTownHallAuditSubsystem::Begin()
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
		&& Runtime->IsRuntimeStateLoaded() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& Hud.IsValid() && Screen.IsValid() && Modal.IsValid() && !Screen->IsScreenOpen()
		&& !Modal->IsModalOpen() && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
		TEXT("Expected idle local-only 1440x810 GameApp HUD"))) return false;
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
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	bPointerSaved = true;
	Inputs = MakeShared<FJsonObject>();
	for (const FInputPin& Pin : Pinned)
	{
		const FString Actual = FileSha(FPaths::ProjectDir() / Pin.Path);
		if (!Expect(Actual == Pin.Sha, TEXT("Source or V2 product SHA differs from reviewed input"))) return false;
		Inputs->SetStringField(Pin.Path, Actual);
	}
	for (const TCHAR* Path : {TEXT("Source/Rok/RokMainTaskGoToTownHallAuditSubsystem.h"),
		TEXT("Source/Rok/RokMainTaskGoToTownHallAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokMainTaskGoToTownHallRuntime.py")})
	{
		const FString Actual = FileSha(FPaths::ProjectDir() / Path);
		if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
		Inputs->SetStringField(Path, Actual);
	}
	CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
		this, &URokMainTaskGoToTownHallAuditSubsystem::OnUpgradeCompleted);
	return true;
}

TSharedRef<FJsonObject> URokMainTaskGoToTownHallAuditSubsystem::BusinessState() const
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

bool URokMainTaskGoToTownHallAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}

TSharedRef<FJsonObject> URokMainTaskGoToTownHallAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Json->SetObjectField(TEXT("business"), BusinessState());
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
#if !UE_BUILD_SHIPPING
	Json->SetNumberField(TEXT("task_page"), Screen.IsValid() ? Screen->GetSelectedTaskPageForAudit() : INDEX_NONE);
	Json->SetBoolField(TEXT("task_local_projection"), Screen.IsValid() && Screen->IsTaskDetailLocalProjectionForAudit());
	Json->SetNumberField(TEXT("task_displayed_current"), Screen.IsValid() ? Screen->GetTaskDetailDisplayedCurrentForAudit() : 0);
#else
	Json->SetNumberField(TEXT("task_page"), INDEX_NONE);
	Json->SetBoolField(TEXT("task_local_projection"), false);
	Json->SetNumberField(TEXT("task_displayed_current"), 0);
#endif
	Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	Json->SetBoolField(TEXT("hall_info_exact"), Modal.IsValid() && Modal->IsBuildingInfoOpenForActor(Hall.Get()));
#if !UE_BUILD_SHIPPING
	Json->SetStringField(TEXT("modal_selected_actor"), Modal.IsValid() && Modal->GetSelectedBuildingActorForAudit()
		? Modal->GetSelectedBuildingActorForAudit()->GetPathName() : FString());
#else
	Json->SetStringField(TEXT("modal_selected_actor"), FString());
#endif
	const auto Placement = Controller.IsValid() ? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
	Json->SetNumberField(TEXT("primary_press_count"), static_cast<double>(Placement.PrimaryPressEventCount));
	Json->SetStringField(TEXT("selected_actor"), Placement.CurrentSelection.IsValid()
		? Placement.CurrentSelection->GetPathName() : FString());
	const APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	const FTransform Camera = Pawn ? Pawn->GetActorTransform() : FTransform::Identity;
	const FVector Location = Camera.GetLocation();
	const FRotator Rotation = Camera.Rotator();
	Json->SetNumberField(TEXT("camera_x"), Location.X);
	Json->SetNumberField(TEXT("camera_y"), Location.Y);
	Json->SetNumberField(TEXT("camera_z"), Location.Z);
	Json->SetNumberField(TEXT("camera_pitch"), Rotation.Pitch);
	Json->SetNumberField(TEXT("camera_yaw"), Rotation.Yaw);
	Json->SetNumberField(TEXT("camera_roll"), Rotation.Roll);
	Json->SetNumberField(TEXT("camera_scale_x"), Camera.GetScale3D().X);
	Json->SetNumberField(TEXT("camera_scale_y"), Camera.GetScale3D().Y);
	Json->SetNumberField(TEXT("camera_scale_z"), Camera.GetScale3D().Z);
	auto Widgets = MakeShared<FJsonObject>();
	Widgets->SetObjectField(TEXT("TaskEntryButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("TaskEntryButton")) : nullptr));
	Widgets->SetObjectField(TEXT("FeatureModeCanvas"), WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")) : nullptr));
	Widgets->SetObjectField(TEXT("TaskAction_0"), WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(TEXT("TaskAction_0")) : nullptr));
	Widgets->SetObjectField(TEXT("TaskTitle_0"), WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(TEXT("TaskTitle_0")) : nullptr));
	Widgets->SetObjectField(TEXT("TaskDescription_0"), WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(TEXT("TaskDescription_0")) : nullptr));
	Widgets->SetObjectField(TEXT("Type2CloseButton"), WidgetState(Modal.IsValid() ? Modal->GetWidgetFromName(TEXT("Type2CloseButton")) : nullptr));
	Widgets->SetObjectField(TEXT("BuildingInfoPreviewFrame"), WidgetState(Modal.IsValid() ? Modal->GetWidgetFromName(TEXT("BuildingInfoPreviewFrame")) : nullptr));
	Widgets->SetObjectField(TEXT("BuildingInfoAttributeFrame"), WidgetState(Modal.IsValid() ? Modal->GetWidgetFromName(TEXT("BuildingInfoAttributeFrame")) : nullptr));
	Json->SetObjectField(TEXT("widgets"), Widgets);
	return Json;
}

bool URokMainTaskGoToTownHallAuditSubsystem::Ready(int32 Phase) const
{
	if (!Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Server.IsValid()
		|| !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
		|| Server->GetSuppressedLocalRequestCount() != 0) return false;
	if (Phase == 0 || Phase == 3)
		return !Screen->IsScreenOpen() && !Modal->IsModalOpen()
			&& Visible(Hud->GetWidgetFromName(TEXT("TaskEntryButton")));
	if (Phase == 1)
	{
	#if !UE_BUILD_SHIPPING
		const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
		return Screen->IsScreenOpen() && Screen->GetSelectedTaskPageForAudit() == 1
			&& Screen->IsTaskDetailLocalProjectionForAudit()
			&& Screen->GetTaskDetailDisplayedCurrentForAudit() == 1
			&& !Modal->IsModalOpen() && Visible(Mode)
			&& FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0, .01)
			&& Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01)
			&& Visible(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
	#else
		return false;
	#endif
	}
	if (Phase == 2)
	{
		const UWidget* Mode = Modal->GetWidgetFromName(TEXT("ModalModeCanvas"));
		const bool bModeSettled = !Mode || (Visible(Mode)
			&& FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0, .01)
			&& Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01));
		return !Screen->IsScreenOpen() && Modal->IsBuildingInfoOpenForActor(Hall.Get())
			&& bModeSettled && Visible(Modal->GetWidgetFromName(TEXT("Type2CloseButton")))
			&& Visible(Modal->GetWidgetFromName(TEXT("BuildingInfoPreviewFrame")))
			&& Visible(Modal->GetWidgetFromName(TEXT("BuildingInfoAttributeFrame")));
	}
	return false;
}

void URokMainTaskGoToTownHallAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Go route emitted an upgrade completion event"));
}

bool URokMainTaskGoToTownHallAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
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

void URokMainTaskGoToTownHallAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
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
void URokMainTaskGoToTownHallAuditSubsystem::ScreenshotProcessed()
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
void URokMainTaskGoToTownHallAuditSubsystem::AdvanceCapture()
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
void URokMainTaskGoToTownHallAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}
bool URokMainTaskGoToTownHallAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.main_task_go_town_hall.runtime_audit.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 4
		&& Actions.Num() == 3 && CompletionEvents == 0 && bBaselineSet && Boundary());
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
	Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
	Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), TEXT("Task 200900 cached idle L1 Hall local-preview Go to exact BuildingInfo, no upgrade, reward, task advancement or server request"));
	Json->SetStringField(TEXT("baseline_phase"), TEXT("01_tasks_fully_open"));
	Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
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
void URokMainTaskGoToTownHallAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 4
		&& Actions.Num() == 3 && CompletionEvents == 0 && Boundary();
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Go audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok task 200900 Hall Go audit %s: %s"),
		Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok task Hall Go audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokMainTaskGoToTownHallAuditSubsystem::Tick(float DeltaTime)
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
	case 1: if (Stable(0)) Capture(TEXT("00_city_hud"), 2); break;
	case 2: if (Click(Cast<UButton>(Hud->GetWidgetFromName(TEXT("TaskEntryButton"))), TEXT("open_tasks_from_hud"))) Enter(3); break;
	case 3:
		if (Stable(1))
		{
			FRokBuildingState HallState;
			FRokLocalConstructionTicket Ticket;
			const auto Task = Runtime->GetActiveTask();
			int32 PendingCount = -1;
#if WITH_DEV_AUTOMATION_TESTS
			PendingCount = Buildings->GetPendingCompletedUpgradeCountForTesting();
#endif
			if (!Expect(Buildings->TryPeekExistingBuildingStateForActor(Hall.Get(), HallState)
				&& HallState.BuildingKey == FName(TEXT("UnityInitId_5"))
				&& HallState.Type == ERokBuildingType::TownCenter
				&& HallState.UnityInitId == 5 && HallState.UnityBuildingTypeId == 1
				&& HallState.Level == 1 && !HallState.bUpgradeInProgress
				&& FMath::IsNearlyZero(HallState.UpgradeStartedAt)
				&& FMath::IsNearlyZero(HallState.UpgradeFinishesAt)
				&& Task.TaskId == 200900 && Task.Type == 8 && Task.Param1 == 1 && Task.Param2 == 2
				&& Task.Current == 1 && Task.Target == 2 && !Task.bComplete
				&& Buildings->GetActiveUpgradeCount() == 0
				&& PendingCount == 0
				&& Buildings->GetActiveLocalConstructionCount() == 0
				&& !Buildings->GetLocalConstructionTicket(1, Ticket),
				TEXT("Tasks-open baseline is not the reviewed idle cached L1 Hall"))) return;
			BaselineBusinessJson = BusinessState();
			BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
			BaselineCamera = Controller->GetPawn()->GetActorTransform();
			BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
			bBaselineSet = true;
			Capture(TEXT("01_tasks_fully_open"), 4);
		}
		break;
	case 4: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0"))), TEXT("go_to_cached_hall_info"))) Enter(5); break;
	case 5: if (Boundary() && Stable(2)) Capture(TEXT("02_exact_hall_building_info"), 6); break;
	case 6: if (Boundary() && Click(Cast<UButton>(Modal->GetWidgetFromName(TEXT("Type2CloseButton"))), TEXT("close_hall_info"))) Enter(7); break;
	case 7: if (Boundary() && Stable(3)) Capture(TEXT("03_hud_return"), 8); break;
	case 8: Finish(); break;
	default: Finish(TEXT("Invalid audit state")); break;
	}
}
