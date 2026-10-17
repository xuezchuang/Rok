#include "RokCityPlacementRuntimeAuditSubsystem.h"

#include "RokBuildingPanelWidget.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokCityPlacementPreviewActor.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCitySpriteActor.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "GenericPlatform/GenericWindow.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SceneView.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR AuditFlag[] = TEXT("-RokCityPlacementAudit");
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr int32 ExpectedCaptures = 11;

bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}

template<typename T> TArray<uint8> Snapshot(const T& Value)
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive Archive(Writer, false);
	T Copy = Value;
	T::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
	return Bytes;
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

bool HasTagPrefix(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return false;
	for (const FName Tag : Actor->Tags)
		if (Tag.ToString().StartsWith(Prefix)) return true;
	return false;
}

bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	return true;
}

TSharedRef<FJsonObject> GridJson(const FIntPoint& Grid)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Grid.X);
	Json->SetNumberField(TEXT("y"), Grid.Y);
	return Json;
}

TSharedRef<FJsonObject> VectorJson(const FVector& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	Json->SetNumberField(TEXT("z"), Value.Z);
	return Json;
}

TSharedRef<FJsonObject> PointJson(const FVector2D& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}

TSharedRef<FJsonObject> RectJson(const FIntRect& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("min"), GridJson(Value.Min));
	Json->SetObjectField(TEXT("max"), GridJson(Value.Max));
	Json->SetNumberField(TEXT("width"), Value.Width());
	Json->SetNumberField(TEXT("height"), Value.Height());
	return Json;
}

TSharedRef<FJsonObject> MatrixJson(const FMatrix& Value)
{
	auto Json = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Rows;
	bool bFinite = true;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		TArray<TSharedPtr<FJsonValue>> Columns;
		for (int32 Column = 0; Column < 4; ++Column)
		{
			const double Number = Value.M[Row][Column];
			bFinite &= FMath::IsFinite(Number);
			Columns.Add(MakeShared<FJsonValueNumber>(FMath::IsFinite(Number) ? Number : 0.0));
		}
		Rows.Add(MakeShared<FJsonValueArray>(Columns));
	}
	Json->SetBoolField(TEXT("all_finite"), bFinite);
	Json->SetArrayField(TEXT("rows"), Rows);
	return Json;
}

TSharedRef<FJsonObject> DeprojectionJson(const ARokPlayerController* Controller,
	const FVector2D& ScreenPoint)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("input_screen_point"), PointJson(ScreenPoint));
	FVector Origin = FVector::ZeroVector, Direction = FVector::ZeroVector;
	const bool bRay = Controller && Controller->DeprojectScreenPositionToWorld(
		ScreenPoint.X, ScreenPoint.Y, Origin, Direction)
		&& !Origin.ContainsNaN() && !Direction.ContainsNaN();
	if (!bRay)
	{
		Origin = FVector::ZeroVector;
		Direction = FVector::ZeroVector;
	}
	Json->SetBoolField(TEXT("ray_valid"), bRay);
	Json->SetObjectField(TEXT("ray_origin"), VectorJson(Origin));
	Json->SetObjectField(TEXT("ray_direction"), VectorJson(Direction));
	for (const int32 Height : {0, 5})
	{
		const double RawDistance = bRay && !FMath::IsNearlyZero(Direction.Z)
			? (Height - Origin.Z) / Direction.Z : 0.0;
		const bool bHit = bRay && FMath::IsFinite(RawDistance) && RawDistance > 0.0;
		const double Distance = FMath::IsFinite(RawDistance) ? RawDistance : 0.0;
		const FVector Hit = bHit ? Origin + Direction * Distance : FVector::ZeroVector;
		const FString Prefix = FString::Printf(TEXT("ground_z%d_"), Height);
		Json->SetBoolField(Prefix + TEXT("valid"), bHit && !Hit.ContainsNaN());
		Json->SetNumberField(Prefix + TEXT("ray_distance"), Distance);
		Json->SetObjectField(Prefix + TEXT("intersection"), VectorJson(Hit));
	}
	return Json;
}

TSharedRef<FJsonObject> TransformJson(const FTransform& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("location"), VectorJson(Value.GetLocation()));
	Json->SetObjectField(TEXT("rotation_euler"), VectorJson(Value.GetRotation().Euler()));
	Json->SetObjectField(TEXT("scale"), VectorJson(Value.GetScale3D()));
	return Json;
}

TSharedRef<FJsonObject> PreviewComponentJson(const UStaticMeshComponent* Component)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("present"), Component != nullptr);
	if (!Component) return Json;
	Json->SetObjectField(TEXT("world_transform"), TransformJson(Component->GetComponentTransform()));
	Json->SetObjectField(TEXT("relative_transform"), TransformJson(Component->GetRelativeTransform()));
	const FBoxSphereBounds Bounds = Component->CalcBounds(Component->GetComponentTransform());
	Json->SetObjectField(TEXT("world_bounds_origin"), VectorJson(Bounds.Origin));
	Json->SetObjectField(TEXT("world_bounds_box_extent"), VectorJson(Bounds.BoxExtent));
	Json->SetNumberField(TEXT("world_bounds_sphere_radius"), Bounds.SphereRadius);
	FVector LocalMin, LocalMax;
	Component->GetLocalBounds(LocalMin, LocalMax);
	Json->SetObjectField(TEXT("local_bounds_min"), VectorJson(LocalMin));
	Json->SetObjectField(TEXT("local_bounds_max"), VectorJson(LocalMax));
	Json->SetStringField(TEXT("mesh"), Component->GetStaticMesh()
		? Component->GetStaticMesh()->GetPathName() : FString());
	UMaterialInterface* Material = Component->GetMaterial(0);
	Json->SetStringField(TEXT("material"), Material ? Material->GetPathName() : FString());
	Json->SetBoolField(TEXT("registered"), Component->IsRegistered());
	Json->SetBoolField(TEXT("visible"), Component->IsVisible());
	Json->SetNumberField(TEXT("translucency_sort_priority"), Component->TranslucencySortPriority);
	Json->SetNumberField(TEXT("last_render_time_on_screen"), Component->GetLastRenderTimeOnScreen());
	if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material))
	{
		const FLinearColor Tint = Dynamic->K2_GetVectorParameterValue(TEXT("Tint"));
		const FLinearColor Tiling = Dynamic->K2_GetVectorParameterValue(TEXT("Tiling"));
		Json->SetBoolField(TEXT("material_is_dynamic"), true);
		auto TintJson = MakeShared<FJsonObject>();
		TintJson->SetNumberField(TEXT("r"), Tint.R);
		TintJson->SetNumberField(TEXT("g"), Tint.G);
		TintJson->SetNumberField(TEXT("b"), Tint.B);
		TintJson->SetNumberField(TEXT("a"), Tint.A);
		Json->SetObjectField(TEXT("material_tint_rgba"), TintJson);
		auto TilingJson = MakeShared<FJsonObject>();
		TilingJson->SetNumberField(TEXT("r"), Tiling.R);
		TilingJson->SetNumberField(TEXT("g"), Tiling.G);
		TilingJson->SetNumberField(TEXT("b"), Tiling.B);
		TilingJson->SetNumberField(TEXT("a"), Tiling.A);
		Json->SetObjectField(TEXT("material_tiling_rgba"), TilingJson);
	}
	else Json->SetBoolField(TEXT("material_is_dynamic"), false);
	return Json;
}

TSharedRef<FJsonObject> InputRouteJson(const UWorld* World, const ARokPlayerController* Controller,
	const FVector2D& RequestedAbsolute)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("requested_absolute_x"), RequestedAbsolute.X);
	Json->SetNumberField(TEXT("requested_absolute_y"), RequestedAbsolute.Y);
	if (Controller)
	{
		float LocalX = 0.0f, LocalY = 0.0f;
		Json->SetBoolField(TEXT("pc_mouse_position_valid"), Controller->GetMousePosition(LocalX, LocalY));
		Json->SetNumberField(TEXT("pc_local_mouse_x"), LocalX);
		Json->SetNumberField(TEXT("pc_local_mouse_y"), LocalY);
		Json->SetBoolField(TEXT("pc_left_input_down"), Controller->IsInputKeyDown(EKeys::LeftMouseButton));
	}
	if (!FSlateApplication::IsInitialized()) return Json;
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Actual = App.GetCursorPos();
	Json->SetNumberField(TEXT("slate_cursor_absolute_x"), Actual.X);
	Json->SetNumberField(TEXT("slate_cursor_absolute_y"), Actual.Y);
	const TSharedPtr<FSlateUser> User = App.GetUser(0);
	const TSharedPtr<SWidget> Captor = User.IsValid() ? User->GetCursorCaptor() : nullptr;
	Json->SetStringField(TEXT("slate_cursor_captor_type"), Captor.IsValid()
		? Captor->GetTypeAsString() : FString());
	Json->SetBoolField(TEXT("slate_user_mouse_capture"), App.HasUserMouseCapture(0));
	const FWidgetPath CaptorPath = User.IsValid() ? User->GetCursorCaptorPath() : FWidgetPath();
	const FWidgetPath HitPath = App.LocateWindowUnderMouse(RequestedAbsolute,
		App.GetInteractiveTopLevelWindows(), false, 0);
	auto RecordPath = [&Json](const TCHAR* Prefix, const FWidgetPath& Path)
	{
		Json->SetBoolField(FString(Prefix) + TEXT("_valid"), Path.IsValid());
		bool bViewport = false, bUmg = false;
		TArray<TSharedPtr<FJsonValue>> Types;
		for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
		{
			const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
			Types.Add(MakeShared<FJsonValueString>(Type));
			bViewport |= Type.Contains(TEXT("SViewport"));
			bUmg |= Type.Contains(TEXT("SObjectWidget"));
		}
		Json->SetArrayField(FString(Prefix) + TEXT("_widget_types"), Types);
		Json->SetBoolField(FString(Prefix) + TEXT("_contains_viewport"), bViewport);
		Json->SetBoolField(FString(Prefix) + TEXT("_contains_umg"), bUmg);
	};
	RecordPath(TEXT("captor_path"), CaptorPath);
	RecordPath(TEXT("hit_path"), HitPath);
	const UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	Json->SetBoolField(TEXT("viewport_has_mouse_capture"), Viewport
		&& Viewport->Viewport && Viewport->Viewport->HasMouseCapture());
	const TSharedPtr<SWindow> GameWindow = Viewport ? Viewport->GetWindow() : nullptr;
	Json->SetBoolField(TEXT("game_window_present"), GameWindow.IsValid());
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = GameWindow.IsValid()
		? GameWindow->GetNativeWindow() : nullptr;
	const HWND GameHandle = Native.IsValid()
		? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	POINT Cursor = {};
	const bool bCursor = ::GetCursorPos(&Cursor) != 0;
	const HWND HitHandle = bCursor ? ::WindowFromPoint(Cursor) : nullptr;
	const HWND Foreground = ::GetForegroundWindow();
	DWORD GamePid = 0;
	if (GameHandle) ::GetWindowThreadProcessId(GameHandle, &GamePid);
	DWORD HitPid = 0;
	if (HitHandle) ::GetWindowThreadProcessId(HitHandle, &HitPid);
	Json->SetBoolField(TEXT("game_hwnd_owned_by_process"), GamePid == ::GetCurrentProcessId());
	Json->SetBoolField(TEXT("game_hwnd_visible"), GameHandle && ::IsWindowVisible(GameHandle) != 0);
	Json->SetBoolField(TEXT("game_hwnd_minimized"), GameHandle && ::IsIconic(GameHandle) != 0);
	Json->SetNumberField(TEXT("window_from_os_cursor_pid"), HitPid);
	Json->SetBoolField(TEXT("window_from_os_cursor_same_process"), HitPid != 0 && HitPid == GamePid);
	Json->SetStringField(TEXT("game_hwnd"), FString::Printf(TEXT("%p"), GameHandle));
	Json->SetStringField(TEXT("window_from_os_cursor_hwnd"), FString::Printf(TEXT("%p"), HitHandle));
	Json->SetStringField(TEXT("window_from_os_cursor_root_hwnd"), FString::Printf(TEXT("%p"),
		HitHandle ? ::GetAncestor(HitHandle, GA_ROOT) : nullptr));
	Json->SetStringField(TEXT("foreground_hwnd"), FString::Printf(TEXT("%p"), Foreground));
	Json->SetBoolField(TEXT("os_cursor_read"), bCursor);
	Json->SetNumberField(TEXT("os_cursor_x"), Cursor.x);
	Json->SetNumberField(TEXT("os_cursor_y"), Cursor.y);
	Json->SetBoolField(TEXT("os_cursor_hits_own_window"), GameHandle && HitHandle
		&& (HitHandle == GameHandle || ::GetAncestor(HitHandle, GA_ROOT) == GameHandle));
	Json->SetBoolField(TEXT("own_window_foreground"), GameHandle && Foreground == GameHandle);
	RECT WindowRect = {}, ClientRect = {};
	if (GameHandle && ::GetWindowRect(GameHandle, &WindowRect))
	{
		Json->SetNumberField(TEXT("game_window_left"), WindowRect.left);
		Json->SetNumberField(TEXT("game_window_top"), WindowRect.top);
		Json->SetNumberField(TEXT("game_window_right"), WindowRect.right);
		Json->SetNumberField(TEXT("game_window_bottom"), WindowRect.bottom);
	}
	if (GameHandle && ::GetClientRect(GameHandle, &ClientRect))
	{
		POINT Min = {ClientRect.left, ClientRect.top};
		POINT Max = {ClientRect.right, ClientRect.bottom};
		if (::ClientToScreen(GameHandle, &Min) && ::ClientToScreen(GameHandle, &Max))
		{
			Json->SetNumberField(TEXT("game_client_left"), Min.x);
			Json->SetNumberField(TEXT("game_client_top"), Min.y);
			Json->SetNumberField(TEXT("game_client_right"), Max.x);
			Json->SetNumberField(TEXT("game_client_bottom"), Max.y);
		}
	}
#endif
	return Json;
}

}

bool URokCityPlacementRuntimeAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokCityPlacementAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokCityPlacementRuntimeAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}

TStatId URokCityPlacementRuntimeAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityPlacementRuntimeAuditSubsystem, STATGROUP_Tickables);
}

void URokCityPlacementRuntimeAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/CityPlacementRuntimeAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokCityPlacementRuntimeAuditSubsystem::ScreenshotProcessed);
}

void URokCityPlacementRuntimeAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (FSlateApplication::IsInitialized() && OriginalAbsolutePointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalAbsolutePointer);
	Super::Deinitialize();
}

bool URokCityPlacementRuntimeAuditSubsystem::Expect(bool Condition, const TCHAR* Message)
{
	if (!Condition) Finish(Message);
	return Condition;
}

bool URokCityPlacementRuntimeAuditSubsystem::Identity()
{
	return Expect(TargetWorld(GetWorld()), TEXT("Placement audit project or world identity changed"));
}

bool URokCityPlacementRuntimeAuditSubsystem::PrepareOwnGameWindow()
{
	WindowPreparation = MakeShared<FJsonObject>();
	WindowPreparation->SetBoolField(TEXT("attempted"), false);
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> GameWindow = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = GameWindow.IsValid()
		? GameWindow->GetNativeWindow() : nullptr;
	const HWND GameHandle = Native.IsValid()
		? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD OwnerPid = 0;
	if (GameHandle) ::GetWindowThreadProcessId(GameHandle, &OwnerPid);
	const bool bOwnGame = TargetWorld(GetWorld())
		&& FParse::Param(FCommandLine::Get(), TEXT("game"))
		&& GameHandle && ::IsWindow(GameHandle) != 0
		&& OwnerPid == ::GetCurrentProcessId();
	WindowPreparation->SetBoolField(TEXT("verified_own_game_window"), bOwnGame);
	WindowPreparation->SetStringField(TEXT("game_hwnd"), FString::Printf(TEXT("%p"), GameHandle));
	if (!Expect(bOwnGame, TEXT("Placement test window identity or PID was not verified"))) return false;
	RECT Client = {};
	const bool bClient = ::GetClientRect(GameHandle, &Client) != 0
		&& Client.right - Client.left == 1440 && Client.bottom - Client.top == 810;
	WindowPreparation->SetBoolField(TEXT("verified_1440x810_client"), bClient);
	if (!Expect(bClient, TEXT("Placement test window client rect differs from 1440x810"))) return false;
	WindowPreparation->SetBoolField(TEXT("attempted"), true);
	WindowPreparation->SetBoolField(TEXT("visible_before"), ::IsWindowVisible(GameHandle) != 0);
	WindowPreparation->SetBoolField(TEXT("minimized_before"), ::IsIconic(GameHandle) != 0);
	if (::IsIconic(GameHandle)) ::ShowWindow(GameHandle, SW_RESTORE);
	else if (!::IsWindowVisible(GameHandle)) ::ShowWindow(GameHandle, SW_SHOW);
	WindowPreparation->SetBoolField(TEXT("visible_after_show"), ::IsWindowVisible(GameHandle) != 0);
	WindowPreparation->SetBoolField(TEXT("minimized_after_show"), ::IsIconic(GameHandle) != 0);
	WindowPreparation->SetBoolField(TEXT("set_foreground_returned"), ::SetForegroundWindow(GameHandle) != 0);
	WindowPreparation->SetBoolField(TEXT("foreground_is_game_after_call"), ::GetForegroundWindow() == GameHandle);
	return Expect(::IsWindowVisible(GameHandle) != 0 && ::IsIconic(GameHandle) == 0
		&& ::GetForegroundWindow() == GameHandle,
		TEXT("Verified Rok GameApp window did not become visible and foreground"));
#else
	return Expect(false, TEXT("Placement window preparation requires Windows GameApp"));
#endif
}

void URokCityPlacementRuntimeAuditSubsystem::Enter(int32 NextStep)
{
	Step = NextStep;
	StepStarted = Elapsed;
	StableClickTarget.Reset();
	StableClickFrames = 0;
}

void URokCityPlacementRuntimeAuditSubsystem::OnUpgradeCompleted(
	const FRokLocalBuildingUpgradeCompleted& Completed)
{
	if (Completed.UnityInitId != 5 || Completed.UnityBuildingTypeId != 1
		|| Completed.PreviousLevel != 1 || Completed.CurrentLevel != 2
		|| Completed.PreviousPower != 7 || Completed.CurrentPower != 21)
	{
		Finish(TEXT("Unexpected upgrade completion observed during placement audit"));
		return;
	}
	++CompletionCount;
}

bool URokCityPlacementRuntimeAuditSubsystem::Begin()
{
	if (!Identity()) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 SelectionFlags = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(TEXT("-RokAutoSelectBuilding=UnityInitId_5"), ESearchCase::IgnoreCase))
			{ ++SelectionFlags; continue; }
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& !Token.Equals(AuditFlag, ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Placement audit requires only its Hall auto-selection fixture"));
	}
	if (!Expect(SelectionFlags == 1 && FSlateApplication::IsInitialized(),
		TEXT("Placement audit needs one Hall auto-selection flag and Slate"))) return false;
	OriginalAbsolutePointer = FSlateApplication::Get().GetCursorPos();
	LastAbsolutePointer = OriginalAbsolutePointer;
	Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
	Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Runtime = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets,
		URokMainHudWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
		if (Widget && Widget->GetWorld() == GetWorld())
		{
			if (Hud.IsValid()) return Expect(false, TEXT("Multiple HUDs own placement input"));
			Hud = Cast<URokMainHudWidget>(Widget);
		}
	Panel = Hud.IsValid()
		? Cast<URokBuildingPanelWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))) : nullptr;
	Modal = Hud.IsValid()
		? Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget"))) : nullptr;
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	if (!Expect(Controller.IsValid() && Hud.IsValid() && Panel.IsValid() && Modal.IsValid()
		&& Screen.IsValid() && Buildings.IsValid() && Runtime.IsValid()
		&& Runtime->IsRuntimeStateLoaded() && Visible(Hud.Get())
		&& !Screen->IsScreenOpen() && !Modal->IsModalOpen()
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), 0.1f),
		TEXT("Expected one 1440x810 idle GameApp HUD/controller and local state"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId
		&& Profile.RoleId.IsEmpty() && !Profile.bHasCivilization
		&& Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Placement audit cannot operate on an account profile"))) return false;
	if (!PrepareOwnGameWindow()) return false;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (HasTagPrefix(*It, TEXT("UnityInitId_")))
			BaselineUnityInitActors.Add(TWeakObjectPtr<AActor>(*It));
		if (!Expect(!It->Tags.Contains(TEXT("RokLocalConstructedBuilding")),
			TEXT("Unexpected formal constructed actor before placement"))) return false;
		if (!It->Tags.Contains(TEXT("RokUnityConfigInitBuilding"))
			|| !It->Tags.Contains(TEXT("RokUnityRuntimeBuildingSource"))) continue;
		const int32 Id = TaggedInt(*It, TEXT("UnityInitId_"));
		if (!Expect(Id > 0 && !InitialActors.Contains(Id),
			TEXT("Initial city actor identity is missing or duplicated"))) return false;
		InitialActors.Add(Id, *It);
		if (Id == 5) Hall = *It;
	}
	for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
		if (!It->IsActorBeingDestroyed())
			BaselineSourceActors.Add(TWeakObjectPtr<ARokCitySpriteActor>(*It));
	const auto Initial = Controller->GetCityPlacementPreviewSnapshot();
	const FRokBuildingState InitialHallState = Buildings->GetBuildingStateForActor(Hall.Get());
	const FRokResourceStock InitialWallet = Buildings->GetCurrentResources();
	if (!Expect(InitialActors.Num() == 24 && Hall.IsValid()
		&& BaselineUnityInitActors.Num() == 24
		&& BaselineSourceActors.Num() >= 24
		&& Initial.CurrentSelection.Get() == Hall.Get()
		&& !Initial.bActive && !Initial.PreviewActor.IsValid()
		&& InitialHallState.UnityInitId == 5 && InitialHallState.UnityBuildingTypeId == 1
		&& InitialHallState.Level == 1 && InitialHallState.Power == 7
		&& !InitialHallState.bUpgradeInProgress
		&& InitialWallet.Food == 5000 && InitialWallet.Wood == 5000
		&& InitialWallet.Stone == 1500 && InitialWallet.Gold == 600
		&& Buildings->GetActiveUpgradeCount() == 0
		&& Buildings->GetLocalCompletedBuildingPowerDelta() == 0
		&& Profile.LocalBuildingUpgradeScore == 0
		&& Buildings->GetInitialCityBuildingCount() == 26
		&& Buildings->GetActiveLocalConstructionCount() == 0,
		TEXT("Expected 24 source city actors, selected Hall and no local ticket/preview"))) return false;
	FRokBuildingUpgradePreview HallPreview;
	if (!Expect(Buildings->GetUpgradePreview(Hall.Get(), HallPreview)
		&& HallPreview.bCanStartTimedUpgrade && HallPreview.TargetLevel == 2
		&& HallPreview.BuildSeconds == 2 && HallPreview.Cost.Food == 3500
		&& HallPreview.Cost.Wood == 3500 && HallPreview.NextPower == 21,
		TEXT("Hall L1-to-L2 ordinary local upgrade prerequisite differs"))) return false;
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokCityPlacementRuntimeAuditSubsystem.h"),
		TEXT("Source/Rok/RokCityPlacementRuntimeAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokCityPlacementRuntime.py"),
		TEXT("Source/Rok/RokCityPlacementWorldFrame.h"),
		TEXT("Source/Rok/RokCityPlacementWorldFrame.cpp"),
		TEXT("Source/Rok/RokPlayerController.h"),
		TEXT("Source/Rok/RokPlayerController.cpp"),
		TEXT("Source/Rok/RokCityPlacementPreviewActor.h"),
		TEXT("Source/Rok/RokCityPlacementPreviewActor.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"),
		TEXT("Saved/RokUnityMigration/LocalCitySimulationContext/catalog.json"),
		TEXT("Saved/RokUnityMigration/CityConstructionGridSources/catalog.json"),
		TEXT("Saved/RokUnityMigration/unity_initial_city_state.json")
	};
	for (const TCHAR* Relative : Required)
	{
		TArray<uint8> Bytes;
		if (!Expect(FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectDir() / Relative))
			&& !Bytes.IsEmpty(), TEXT("Placement audit input missing"))) return false;
		Inputs->SetStringField(Relative, RokTrainArmyMesh::Sha256(Bytes));
	}
	CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
		this, &URokCityPlacementRuntimeAuditSubsystem::OnUpgradeCompleted);
	return true;
}

bool URokCityPlacementRuntimeAuditSubsystem::FreezePlacementBaseline()
{
	if (!Identity() || !Expect(Hall.IsValid() && CompletionCount == 1
		&& FPlatformTime::Seconds() - HallClickedWall >= 1.9,
		TEXT("Hall did not naturally finish after its source two-second queue"))) return false;
	const FRokBuildingState HallState = Buildings->GetBuildingStateForActor(Hall.Get());
	const FRokResourceStock Wallet = Buildings->GetCurrentResources();
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(HallState.Level == 2 && HallState.Power == 21
		&& !HallState.bUpgradeInProgress && Wallet.Food == 1500 && Wallet.Wood == 1500
		&& Wallet.Stone == 1500 && Wallet.Gold == 600
		&& Buildings->GetActiveUpgradeCount() == 0
		&& Buildings->GetLocalCompletedBuildingPowerDelta() == 14
		&& Profile.LocalBuildingUpgradeScore == 14,
		TEXT("Authorized Hall upgrade did not settle exact source debit and +14 power"))) return false;
	if (!Expect(!Modal->IsModalOpen() && !Screen->IsScreenOpen()
		&& Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get() == Hall.Get(),
		TEXT("Hall ordinary upgrade did not return to selected city HUD"))) return false;
	ProfileBaseline = Snapshot(Profile);
	ResourceBaseline = Snapshot(Wallet);
	TaskBaseline = Snapshot(Runtime->GetActiveTask());
	for (const auto& Pair : InitialActors)
	{
		if (!Expect(Pair.Value.IsValid(), TEXT("Initial city actor disappeared before placement"))) return false;
		InitialStates.Add(Pair.Key, Snapshot(Buildings->GetBuildingStateForActor(Pair.Value.Get())));
	}
	BaselineUpgradeQueues = Buildings->GetActiveUpgradeCount();
	BaselineLocalQueues = Buildings->GetActiveLocalConstructionCount();
	BaselineOwnedQueues = Buildings->GetOwnedBuildQueueCount();
	if (!Expect(BaselineOwnedQueues == 1,
		TEXT("Source bootstrap must own exactly one permanent worker queue"))) return false;
	FString Error;
	if (!Expect(FRokCityPlacementWorldFrame::ResolveForWorld(GetWorld(), WorldFrame, Error),
		TEXT("Verified city source frame/actor anchors unavailable"))) return false;
	Baseline = MakeShared<FJsonObject>();
	Baseline->SetNumberField(TEXT("authorized_hall_cost_food"), 3500);
	Baseline->SetNumberField(TEXT("authorized_hall_cost_wood"), 3500);
	Baseline->SetNumberField(TEXT("authorized_hall_duration_seconds"), 2);
	Baseline->SetNumberField(TEXT("authorized_hall_power_delta"), 14);
	Baseline->SetNumberField(TEXT("hall_completion_events"), CompletionCount);
	Baseline->SetNumberField(TEXT("wallet_food"), Wallet.Food);
	Baseline->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
	Baseline->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
	Baseline->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
	Baseline->SetStringField(TEXT("profile_sha256"), RokTrainArmyMesh::Sha256(ProfileBaseline));
	Baseline->SetStringField(TEXT("resources_sha256"), RokTrainArmyMesh::Sha256(ResourceBaseline));
	Baseline->SetStringField(TEXT("task_sha256"), RokTrainArmyMesh::Sha256(TaskBaseline));
	Baseline->SetNumberField(TEXT("initial_source_rows"), Buildings->GetInitialCityBuildingCount());
	Baseline->SetNumberField(TEXT("initial_runtime_actors"), InitialActors.Num());
	Baseline->SetNumberField(TEXT("initial_source_actor_instances"), BaselineSourceActors.Num());
	Baseline->SetNumberField(TEXT("initial_unity_init_tagged_actors"), BaselineUnityInitActors.Num());
	Baseline->SetNumberField(TEXT("active_upgrade_queues"), BaselineUpgradeQueues);
	Baseline->SetNumberField(TEXT("active_local_tickets"), BaselineLocalQueues);
	Baseline->SetNumberField(TEXT("owned_queues"), BaselineOwnedQueues);
	bBaselineReady = true;
	return Unchanged();
}

bool URokCityPlacementRuntimeAuditSubsystem::Unchanged()
{
	if (!bBaselineReady) return true;
	if (!Expect(Identity() && Buildings.IsValid() && Runtime.IsValid()
		&& Snapshot(Runtime->GetPlayerProfileState()) == ProfileBaseline
		&& Snapshot(Runtime->GetActiveTask()) == TaskBaseline
		&& Snapshot(Buildings->GetCurrentResources()) == ResourceBaseline
		&& Buildings->GetActiveUpgradeCount() == BaselineUpgradeQueues
		&& Buildings->GetActiveLocalConstructionCount() == BaselineLocalQueues
		&& Buildings->GetOwnedBuildQueueCount() == BaselineOwnedQueues
		&& Buildings->GetInitialCityBuildingCount() == 26
		&& Buildings->GetLocalCompletedBuildingPowerDelta() == 14
		&& CompletionCount == 1,
		TEXT("Temporary placement changed wallet, queue, profile, task or power"))) return false;
	TSet<int32> Seen;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->Tags.Contains(TEXT("RokUnityConfigInitBuilding"))
			|| !It->Tags.Contains(TEXT("RokUnityRuntimeBuildingSource"))) continue;
		const int32 Id = TaggedInt(*It, TEXT("UnityInitId_"));
		if (!Expect(Id > 0 && InitialActors.Contains(Id)
			&& InitialActors.FindRef(Id).Get() == *It && !Seen.Contains(Id),
			TEXT("Temporary placement changed initial actor set or source identity"))) return false;
		Seen.Add(Id);
	}
	if (!Expect(Seen.Num() == 24, TEXT("Temporary placement changed runtime initial actor count")))
		return false;
	int32 SourceCount = 0;
	for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		++SourceCount;
		if (!Expect(BaselineSourceActors.Contains(TWeakObjectPtr<ARokCitySpriteActor>(*It)),
			TEXT("Temporary placement spawned a non-ghost city SourceActor"))) return false;
	}
	if (!Expect(SourceCount == BaselineSourceActors.Num(),
		TEXT("Temporary placement removed a baseline city SourceActor"))) return false;
	int32 UnityInitTaggedCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!Expect(!It->Tags.Contains(TEXT("RokLocalConstructedBuilding")),
			TEXT("Temporary placement spawned a formal constructed actor"))) return false;
		if (!HasTagPrefix(*It, TEXT("UnityInitId_"))) continue;
		++UnityInitTaggedCount;
		if (!Expect(BaselineUnityInitActors.Contains(TWeakObjectPtr<AActor>(*It)),
			TEXT("Temporary placement spawned an actor with a new UnityInitId")))
			return false;
	}
	if (!Expect(UnityInitTaggedCount == BaselineUnityInitActors.Num(),
		TEXT("Temporary placement removed a UnityInitId identity"))) return false;
	for (const auto& Pair : InitialActors)
	{
		if (!Expect(Pair.Value.IsValid() && InitialStates.FindRef(Pair.Key)
			== Snapshot(Buildings->GetBuildingStateForActor(Pair.Value.Get())),
			TEXT("Temporary placement changed a source initial building state"))) return false;
	}
	FRokLocalConstructionTicket Ticket;
	if (!Expect(!Buildings->GetLocalConstructionTicket(1, Ticket),
		TEXT("Temporary placement minted a formal local construction ticket"))) return false;
	return true;
}

bool URokCityPlacementRuntimeAuditSubsystem::CheckPreview(int32 TypeId,
	bool bExpectedActive, bool bExpectedDragging, bool bExpectedValid)
{
	if (!Unchanged() || !Controller.IsValid()) return false;
	const FRokCityPlacementRuntimeSnapshot SnapshotValue =
		Controller->GetCityPlacementPreviewSnapshot();
	int32 PreviewCount = 0;
	AActor* TaggedPreview = nullptr;
	for (TActorIterator<ARokCityPlacementPreviewActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsActorBeingDestroyed()) { ++PreviewCount; TaggedPreview = *It; }
	}
	const UWidget* Controls = Hud.IsValid()
		? Hud->GetWidgetFromName(TEXT("CityPlacementControls")) : nullptr;
	if (!Expect(SnapshotValue.bActive == bExpectedActive
		&& SnapshotValue.bDragging == bExpectedDragging
		&& SnapshotValue.bGridValid == (bExpectedActive && bExpectedValid)
		&& SnapshotValue.bCameraInputSuspended == bExpectedDragging
		&& SnapshotValue.TypeId == (bExpectedActive ? TypeId : 0)
		&& PreviewCount == (bExpectedActive ? 1 : 0)
		&& SnapshotValue.PreviewActor.Get() == TaggedPreview
		&& (!TaggedPreview || (TaggedPreview->Tags.Contains(TEXT("RokPlacementPreviewTemporary"))
			&& TaggedInt(TaggedPreview, TEXT("UnityInitId_")) == 0
			&& !TaggedPreview->Tags.Contains(TEXT("RokLocalConstructedBuilding"))))
		&& SnapshotValue.CurrentSelection.Get() == Hall.Get()
		&& (bExpectedActive ? SnapshotValue.PriorSelection.Get() == Hall.Get() : true)
		&& Controls && Controls->IsVisible() == bExpectedActive,
		TEXT("Placement snapshot, ghost count, camera lock, HUD or selection differs"))) return false;
	auto Check = MakeShared<FJsonObject>();
	Check->SetStringField(TEXT("action"), TEXT("check_preview_snapshot"));
	Check->SetStringField(TEXT("method"), TEXT("Controller public read-only placement snapshot"));
	Check->SetNumberField(TEXT("step"), Step);
	Check->SetNumberField(TEXT("frame"), GFrameCounter);
	Check->SetNumberField(TEXT("expected_type_id"), TypeId);
	Check->SetNumberField(TEXT("ghost_actor_count"), PreviewCount);
	Check->SetBoolField(TEXT("active"), SnapshotValue.bActive);
	Check->SetBoolField(TEXT("dragging"), SnapshotValue.bDragging);
	Check->SetBoolField(TEXT("valid"), SnapshotValue.bGridValid);
	Check->SetObjectField(TEXT("grid"), GridJson(SnapshotValue.Grid));
	Actions.Add(MakeShared<FJsonValueObject>(Check));
	return true;
}

bool URokCityPlacementRuntimeAuditSubsystem::ProjectGrid(const FIntPoint& Grid,
	FVector2D& OutViewport, FVector2D& OutAbsolute) const
{
	if (!Controller.IsValid() || !WorldFrame.IsValid()) return false;
	const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const FViewport* ActualViewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!ActualViewport) return false;
	FVector World;
	FString Error;
	const FRokCityPlacementRuntimeSnapshot State = Controller->GetCityPlacementPreviewSnapshot();
	if (!WorldFrame.GridCenterToWorld({Grid, State.Footprint}, World, Error)
		|| !Controller->ProjectWorldLocationToScreen(World, OutViewport, false)) return false;
	// Project(false) is in the view rectangle's absolute pixel coordinates; the
	// viewport mouse API uses the whole client, including constrained-view bars.
	OutViewport -= FVector2D(ActualViewport->GetInitialPositionXY());
	const FVector2D Pixels = UWidgetLayoutLibrary::GetViewportSize(this);
	const FGeometry Geometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	if (Pixels.X <= 0 || Pixels.Y <= 0 || OutViewport.X < 4 || OutViewport.Y < 4
		|| OutViewport.X >= Pixels.X - 4 || OutViewport.Y >= Pixels.Y - 4) return false;
	OutAbsolute = Geometry.LocalToAbsolute(FVector2D(
		OutViewport.X * Geometry.GetLocalSize().X / Pixels.X,
		OutViewport.Y * Geometry.GetLocalSize().Y / Pixels.Y));
	if (!FSlateApplication::IsInitialized()) return false;
	const FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(
		OutAbsolute, FSlateApplication::Get().GetInteractiveTopLevelWindows(), false, 0);
	bool bViewport = false;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		bViewport |= Type.Contains(TEXT("SViewport"));
		if (Type.Contains(TEXT("SObjectWidget"))) return false;
	}
	return Path.IsValid() && bViewport;
}

bool URokCityPlacementRuntimeAuditSubsystem::ProjectHeldGridInterior(const FIntPoint& Grid,
	FVector2D& OutViewport, FVector2D& OutAbsolute,
	TSharedPtr<FJsonObject>& OutEvidence) const
{
	OutEvidence = MakeShared<FJsonObject>();
	OutEvidence->SetStringField(TEXT("action"), TEXT("held_grid_target_preflight"));
	OutEvidence->SetObjectField(TEXT("target_grid"), GridJson(Grid));
	OutEvidence->SetObjectField(TEXT("initial_grid"), GridJson(InitialGrid));
	if (!Controller.IsValid() || !WorldFrame.IsValid()) return false;
	const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const FViewport* ActualViewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!ActualViewport) return false;
	const FRokCityPlacementRuntimeSnapshot State = Controller->GetCityPlacementPreviewSnapshot();
	if (!State.bActive || !State.bDragging || State.Footprint.X <= 0 || State.Footprint.Y <= 0)
		return false;
	OutEvidence->SetObjectField(TEXT("current_grid"), GridJson(State.Grid));
	OutEvidence->SetObjectField(TEXT("initial_pointer_local"), PointJson(State.LastPlacementPressLocalHit));
	if (!PredictedDrag.bActive || PredictedDrag.Footprint.Origin != State.Grid
		|| PredictedDrag.Footprint.Size != State.Footprint) return false;

	// Follow the source's previous pointer and fractional carry, then aim 0.025
	// inside the next 0.1-local-unit quantization bin. A Z=5 sprite-center
	// projection instead lands the ground-plane pointer on a bin boundary.
	const FIntPoint Remaining = Grid - State.Grid;
	const FVector2D InteriorMargin(
		Remaining.X < 0 ? -0.025 : (Remaining.X > 0 ? 0.025 : 0.0),
		Remaining.Y < 0 ? -0.025 : (Remaining.Y > 0 ? 0.025 : 0.0));
	const FVector2D TargetLocal = PredictedDrag.PreviousPointerLocal - PredictedDrag.FractionalMove
		+ FVector2D(double(Remaining.X) * 0.1, double(Remaining.Y) * 0.1) + InteriorMargin;
	const FVector TargetWorld = WorldFrame.GetCityContainerTransform().TransformPosition(
		FVector(TargetLocal.X, TargetLocal.Y, 0.0));
	OutEvidence->SetObjectField(TEXT("source_previous_pointer_local"),
		PointJson(PredictedDrag.PreviousPointerLocal));
	OutEvidence->SetObjectField(TEXT("source_fractional_move_local"),
		PointJson(PredictedDrag.FractionalMove));
	OutEvidence->SetObjectField(TEXT("remaining_grid_delta"), GridJson(Remaining));
	OutEvidence->SetObjectField(TEXT("interior_margin_local"), PointJson(InteriorMargin));
	OutEvidence->SetObjectField(TEXT("target_pointer_local"), PointJson(TargetLocal));
	OutEvidence->SetObjectField(TEXT("target_ground_world"), VectorJson(TargetWorld));
	if (!Controller->ProjectWorldLocationToScreen(TargetWorld, OutViewport, false)) return false;
	OutViewport -= FVector2D(ActualViewport->GetInitialPositionXY());
	const FVector2D RoundedViewport(double(FMath::RoundToInt(OutViewport.X)),
		double(FMath::RoundToInt(OutViewport.Y)));
	OutEvidence->SetObjectField(TEXT("projected_viewport"), PointJson(OutViewport));
	OutEvidence->SetObjectField(TEXT("mouse_pixel_used_by_viewport"), PointJson(RoundedViewport));
	const FVector2D Pixels = UWidgetLayoutLibrary::GetViewportSize(this);
	const FGeometry Geometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	if (Pixels.X <= 0 || Pixels.Y <= 0 || OutViewport.X < 4 || OutViewport.Y < 4
		|| OutViewport.X >= Pixels.X - 4 || OutViewport.Y >= Pixels.Y - 4) return false;
	OutAbsolute = Geometry.LocalToAbsolute(FVector2D(
		OutViewport.X * Geometry.GetLocalSize().X / Pixels.X,
		OutViewport.Y * Geometry.GetLocalSize().Y / Pixels.Y));

	FVector ActualGround;
	FString Error;
	if (!WorldFrame.TryScreenToGround(*Controller.Get(), RoundedViewport, ActualGround, Error))
	{
		OutEvidence->SetStringField(TEXT("inverse_error"), Error);
		return false;
	}
	OutEvidence->SetObjectField(TEXT("rounded_pixel_ground_world"), VectorJson(ActualGround));
	FVector2D ActualLocal;
	if (!FRokCityPlacementPreviewMath::WorldToCityLocal(
		WorldFrame.GetCityContainerTransform(), ActualGround, ActualLocal, Error)) return false;
	OutEvidence->SetObjectField(TEXT("rounded_pixel_pointer_local"), PointJson(ActualLocal));
	FRokCityPlacementDragState Predicted = PredictedDrag;
	bool bChanged = false;
	if (!FRokCityPlacementPreviewMath::MoveDrag(Predicted,
			WorldFrame.GetCityContainerTransform(), ActualGround, false, bChanged, Error))
	{
		OutEvidence->SetStringField(TEXT("source_math_error"), Error);
		return false;
	}
	OutEvidence->SetObjectField(TEXT("source_math_predicted_grid"), GridJson(Predicted.Footprint.Origin));
	const bool bExactTarget = bChanged && Predicted.Footprint.Origin == Grid;
	OutEvidence->SetBoolField(TEXT("preflight_exact_source_grid"), bExactTarget);
	return bExactTarget;
}

bool URokCityPlacementRuntimeAuditSubsystem::ProjectSubgrid(
	FVector2D& OutViewport, FVector2D& OutAbsolute) const
{
	if (!Controller.IsValid() || !WorldFrame.IsValid()) return false;
	const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const FViewport* ActualViewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!ActualViewport) return false;
	FVector2D Center;
	FString Error;
	const auto State = Controller->GetCityPlacementPreviewSnapshot();
	if (!FRokCityPlacementPreviewMath::GridToLocalCenter({State.Grid, State.Footprint},
		Center, Error)) return false;
	const FVector World = WorldFrame.GetCityContainerTransform().TransformPosition(
		FVector(Center.X + 0.04, Center.Y, 0.01));
	if (!Controller->ProjectWorldLocationToScreen(World, OutViewport, false)) return false;
	OutViewport -= FVector2D(ActualViewport->GetInitialPositionXY());
	const FVector2D Pixels = UWidgetLayoutLibrary::GetViewportSize(this);
	const FGeometry Geometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	if (OutViewport.X < 4 || OutViewport.Y < 4
		|| OutViewport.X >= Pixels.X - 4 || OutViewport.Y >= Pixels.Y - 4) return false;
	OutAbsolute = Geometry.LocalToAbsolute(FVector2D(
		OutViewport.X * Geometry.GetLocalSize().X / Pixels.X,
		OutViewport.Y * Geometry.GetLocalSize().Y / Pixels.Y));
	return true;
}

bool URokCityPlacementRuntimeAuditSubsystem::ChooseTargets()
{
	const auto Current = Controller->GetCityPlacementPreviewSnapshot();
	if (!Expect(Current.bActive && Current.TypeId == 2 && Current.Footprint == FIntPoint(5, 5),
		TEXT("Farm ghost is not source five-by-five"))) return false;
	auto Preview = [this, &Current](const FIntPoint& Grid, FString& Reason)
	{
		FRokLocalConstructionRequest Request;
		Request.UnityBuildingTypeId = Current.TypeId;
		Request.Grid = Grid;
		FRokLocalConstructionPreview Result;
		if (!Buildings->PreviewLocalConstruction(Request, Result)
			|| !Result.bContextVerified || !Result.bPositionEvaluated) return false;
		Reason = Result.FailureReason;
		return Result.bSourceEligible;
	};
	bool bValid = false, bOccupied = false, bOutbound = false;
	int32 ValidDistance = MAX_int32;
	for (int32 Y = -20; Y <= 20; ++Y)
		for (int32 X = -20; X <= 20; ++X)
		{
			const FIntPoint Candidate(X, Y);
			FVector2D Viewport, Absolute;
			if (!ProjectGrid(Candidate, Viewport, Absolute)) continue;
			FString Reason;
			const bool bEligible = Preview(Candidate, Reason);
			const int32 Distance = FMath::Abs(X - Current.Grid.X) + FMath::Abs(Y - Current.Grid.Y);
			if (bEligible && Candidate != Current.Grid && Distance < ValidDistance)
			{
				ValidGrid = Candidate;
				ValidDistance = Distance;
				bValid = true;
			}
			else if (!bEligible && !bOccupied && Reason.Contains(TEXT("占用")))
			{
				OccupiedGrid = Candidate;
				bOccupied = true;
			}
			else if (!bEligible && !bOutbound && Reason.Contains(TEXT("范围")))
			{
				OutboundGrid = Candidate;
				bOutbound = true;
			}
		}
	if (!Expect(bValid && bOccupied && bOutbound,
		TEXT("No onscreen valid, occupied and outbound source grid centers for pointer drag"))) return false;
	return true;
}

bool URokCityPlacementRuntimeAuditSubsystem::MovePointer(const FVector2D& Viewport,
	const FVector2D& Absolute, bool bHeld, const TCHAR* Label)
{
	if (!Expect(FSlateApplication::IsInitialized(), TEXT("Slate pointer unavailable"))) return false;
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Absolute);
	if (UGameViewportClient* GameViewport = GetWorld()->GetGameViewport())
		if (GameViewport->Viewport) GameViewport->Viewport->SetMouse(
			FMath::RoundToInt(Viewport.X), FMath::RoundToInt(Viewport.Y));
	TSet<FKey> Buttons;
	if (bHeld) Buttons.Add(EKeys::LeftMouseButton);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Absolute, LastAbsolutePointer,
		Buttons, EKeys::Invalid, 0, FModifierKeysState()));
	LastAbsolutePointer = Absolute;
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		Absolute, App.GetInteractiveTopLevelWindows(), false, 0);
	bool bViewportPath = false, bUiPath = false;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		bViewportPath |= Type.Contains(TEXT("SViewport"));
		bUiPath |= Type.Contains(TEXT("SObjectWidget"));
	}
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), Label);
	Action->SetStringField(TEXT("method"), TEXT("Slate platform cursor + ProcessMouseMoveEvent"));
	Action->SetNumberField(TEXT("viewport_x"), Viewport.X);
	Action->SetNumberField(TEXT("viewport_y"), Viewport.Y);
	Action->SetNumberField(TEXT("absolute_x"), Absolute.X);
	Action->SetNumberField(TEXT("absolute_y"), Absolute.Y);
	Action->SetBoolField(TEXT("left_held"), bHeld);
	Action->SetBoolField(TEXT("viewport_hit_path"), bViewportPath);
	Action->SetBoolField(TEXT("ui_hit_path"), bUiPath);
	if (bHeld)
	{
		FVector Ground;
		FString Error;
		const FVector2D MousePixel(double(FMath::RoundToInt(Viewport.X)),
			double(FMath::RoundToInt(Viewport.Y)));
		bool bGridChanged = false;
		if (!PredictedDrag.bActive
			|| !WorldFrame.TryScreenToGround(*Controller.Get(), MousePixel, Ground, Error)
			|| !FRokCityPlacementPreviewMath::MoveDrag(PredictedDrag,
				WorldFrame.GetCityContainerTransform(), Ground, false, bGridChanged, Error))
		{
			Action->SetStringField(TEXT("source_math_error"), Error);
			Actions.Add(MakeShared<FJsonValueObject>(Action));
			return Expect(false, TEXT("Held pointer could not follow source drag quantization"));
		}
		Action->SetObjectField(TEXT("actual_mouse_ground_world"), VectorJson(Ground));
		Action->SetObjectField(TEXT("source_predicted_grid"), GridJson(PredictedDrag.Footprint.Origin));
		Action->SetObjectField(TEXT("source_fractional_move_local"),
			PointJson(PredictedDrag.FractionalMove));
		Action->SetBoolField(TEXT("source_grid_changed"), bGridChanged);
	}
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Path.IsValid() && bViewportPath && !bUiPath,
		TEXT("World drag pointer did not target real Slate viewport"));
}

bool URokCityPlacementRuntimeAuditSubsystem::WorldButton(bool bDown, const TCHAR* Label)
{
	if (!Expect(FSlateApplication::IsInitialized(), TEXT("Slate button unavailable"))) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		LastAbsolutePointer, App.GetInteractiveTopLevelWindows(), false, 0);
	bool bViewport = false, bUi = false;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		bViewport |= Type.Contains(TEXT("SViewport"));
		bUi |= Type.Contains(TEXT("SObjectWidget"));
	}
	if (!Expect(Path.IsValid() && bViewport && !bUi,
		TEXT("World pointer button is not routed through SViewport"))) return false;
	const auto BeforeSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	if (bDown)
	{
		WorldPressPrimaryCountBefore = BeforeSnapshot.PrimaryPressEventCount;
		WorldPressDragBeginCountBefore = BeforeSnapshot.PlacementDragBeginCount;
	}
	else WorldReleaseCountBefore = BeforeSnapshot.PlacementPointerReleaseCount;
	const TSharedRef<FJsonObject> BeforeRoute = InputRouteJson(GetWorld(), Controller.Get(), LastAbsolutePointer);
	TSet<FKey> Buttons;
	if (bDown) Buttons.Add(EKeys::LeftMouseButton);
	const FPointerEvent Event(0, LastAbsolutePointer, LastAbsolutePointer, Buttons,
		EKeys::LeftMouseButton, 0, FModifierKeysState());
	const bool bHandled = bDown
		? App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(), Event)
		: App.ProcessMouseButtonUpEvent(Event);
	bPointerDown = bDown;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), Label);
	Action->SetStringField(TEXT("method"), bDown
		? TEXT("Slate left down to SViewport") : TEXT("Slate left up to SViewport"));
	Action->SetBoolField(TEXT("handled"), bHandled);
	Action->SetBoolField(TEXT("viewport_hit_path"), bViewport);
	Action->SetBoolField(TEXT("ui_hit_path"), bUi);
	Action->SetObjectField(TEXT("before_route"), BeforeRoute);
	Action->SetObjectField(TEXT("after_route"), InputRouteJson(GetWorld(), Controller.Get(), LastAbsolutePointer));
	const auto AfterSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	Action->SetNumberField(TEXT("primary_press_count_before"), BeforeSnapshot.PrimaryPressEventCount);
	Action->SetNumberField(TEXT("primary_press_count_after"), AfterSnapshot.PrimaryPressEventCount);
	Action->SetNumberField(TEXT("drag_begin_count_before"), BeforeSnapshot.PlacementDragBeginCount);
	Action->SetNumberField(TEXT("drag_begin_count_after"), AfterSnapshot.PlacementDragBeginCount);
	Action->SetNumberField(TEXT("pointer_release_count_before"), BeforeSnapshot.PlacementPointerReleaseCount);
	Action->SetNumberField(TEXT("pointer_release_count_after"), AfterSnapshot.PlacementPointerReleaseCount);
	Action->SetStringField(TEXT("primary_press_outcome_after"), AfterSnapshot.LastPrimaryPressOutcome.ToString());
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(bHandled, TEXT("SViewport pointer button was not handled"));
}

bool URokCityPlacementRuntimeAuditSubsystem::SlateClick(UWidget* Target, const TCHAR* Label)
{
	if (!Identity() || !Expect(Target && Visible(Target) && Target->GetIsEnabled()
		&& FSlateApplication::IsInitialized(), TEXT("Placement Slate UI target unavailable")))
		return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("Placement Slate UI target has no geometry"))) return false;
	const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-32.0f, -32.0f));
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Point);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastAbsolutePointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Point, Away, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	LastAbsolutePointer = Point;
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		Point, App.GetInteractiveTopLevelWindows(), false, 0);
	if (!Expect(Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get())
		&& Target->IsHovered(), TEXT("Virtual pointer missed actual UMG target"))) return false;
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Point, Point, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState()));
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), Label);
	Action->SetStringField(TEXT("method"), TEXT("Slate MoveAway+Move+Down+Up"));
	Action->SetNumberField(TEXT("step"), Step);
	Action->SetBoolField(TEXT("target_in_hit_path"), true);
	Action->SetBoolField(TEXT("target_hovered"), true);
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetNumberField(TEXT("absolute_x"), Point.X);
	Action->SetNumberField(TEXT("absolute_y"), Point.Y);
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Down && Up, TEXT("UMG Slate button click was not handled"));
}

bool URokCityPlacementRuntimeAuditSubsystem::ReadyForClick(
	UWidget* Target, bool bHostAnimating)
{
	if (bHostAnimating || !Target || !Visible(Target) || !Target->GetIsEnabled())
	{
		StableClickTarget.Reset();
		StableClickFrames = 0;
		return false;
	}
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0 || Size.Y <= 0)
	{
		StableClickTarget.Reset();
		StableClickFrames = 0;
		return false;
	}
	const FVector2D Center = Geometry.LocalToAbsolute(Size * 0.5f);
	if (StableClickTarget.Get() != Target
		|| !StableClickCenter.Equals(Center, 0.1f)
		|| !StableClickSize.Equals(Size, 0.1f))
	{
		StableClickTarget = Target;
		StableClickCenter = Center;
		StableClickSize = Size;
		StableClickFrames = 1;
		return false;
	}
	++StableClickFrames;
	if (StableClickFrames == 2)
	{
		auto Settled = MakeShared<FJsonObject>();
		Settled->SetStringField(TEXT("action"), TEXT("target_settled"));
		Settled->SetStringField(TEXT("method"), TEXT("host animation idle + two stable cached-geometry frames"));
		Settled->SetStringField(TEXT("target"), Target->GetName());
		Settled->SetNumberField(TEXT("step"), Step);
		Settled->SetNumberField(TEXT("frame"), GFrameCounter);
		Settled->SetNumberField(TEXT("center_x"), Center.X);
		Settled->SetNumberField(TEXT("center_y"), Center.Y);
		Settled->SetNumberField(TEXT("width"), Size.X);
		Settled->SetNumberField(TEXT("height"), Size.Y);
		Actions.Add(MakeShared<FJsonValueObject>(Settled));
	}
	return StableClickFrames >= 2;
}

bool URokCityPlacementRuntimeAuditSubsystem::SlateUiOriginNoDrag(UWidget* Target)
{
	if (!Expect(Target && Visible(Target) && FSlateApplication::IsInitialized(),
		TEXT("TaskProgress_0 UI-origin target unavailable"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 5 && Geometry.GetLocalSize().Y > 5,
		TEXT("UI-origin target has no visible geometry"))) return false;
	const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.4f);
	const FVector2D Other = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.6f);
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Point);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Point, LastAbsolutePointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		Point, App.GetInteractiveTopLevelWindows(), false, 0);
	const bool bDownTargetPath = Path.IsValid()
		&& Path.ContainsWidget(&Target->TakeWidget().Get())
		&& Path.ContainsWidget(&Hud->TakeWidget().Get());
	if (!Expect(bDownTargetPath,
		TEXT("UI-origin press path missed task row"))) return false;
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool bDownHandled = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	App.SetCursorPos(Other);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Other, Point, Pressed,
		EKeys::Invalid, 0, FModifierKeysState()));
	const FWidgetPath UpPath = App.LocateWindowUnderMouse(
		Other, App.GetInteractiveTopLevelWindows(), false, 0);
	const bool bUpTargetPath = UpPath.IsValid()
		&& UpPath.ContainsWidget(&Target->TakeWidget().Get())
		&& UpPath.ContainsWidget(&Hud->TakeWidget().Get());
	const bool bUpHandled = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Other, Other, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState()));
	LastAbsolutePointer = Other;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), TEXT("ui_origin_press_move_release"));
	Action->SetStringField(TEXT("method"), TEXT("Slate target path + left down + held move + up"));
	Action->SetStringField(TEXT("target"), TEXT("TaskProgress_0"));
	Action->SetNumberField(TEXT("frame"), GFrameCounter);
	Action->SetBoolField(TEXT("down_target_and_hud_in_hit_path"), bDownTargetPath);
	Action->SetBoolField(TEXT("up_target_and_hud_in_hit_path"), bUpTargetPath);
	Action->SetBoolField(TEXT("down_handled"), bDownHandled);
	Action->SetBoolField(TEXT("up_handled"), bUpHandled);
	Action->SetNumberField(TEXT("down_absolute_x"), Point.X);
	Action->SetNumberField(TEXT("down_absolute_y"), Point.Y);
	Action->SetNumberField(TEXT("up_absolute_x"), Other.X);
	Action->SetNumberField(TEXT("up_absolute_y"), Other.Y);
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(bDownTargetPath && bUpTargetPath && bDownHandled && bUpHandled,
		TEXT("TaskProgress UI-origin pointer was not handled through its real Slate path"));
}

bool URokCityPlacementRuntimeAuditSubsystem::PressEscape()
{
	if (!Expect(Controller.IsValid() && GetWorld()->GetGameViewport()
		&& GetWorld()->GetGameViewport()->Viewport,
		TEXT("Escape needs the owned GameViewport"))) return false;
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	const FPlatformUserId ControllerUser = Controller->GetPlatformUserId();
	const FInputDeviceId Device = Mapper.GetPrimaryInputDeviceForUser(ControllerUser);
	const FPlatformUserId DeviceUser = Mapper.GetUserForInputDevice(Device);
	ULocalPlayer* TargetPlayer = GEngine
		? GEngine->GetLocalPlayerFromInputDevice(Viewport, Device) : nullptr;
	const bool bTargetsController = Device.IsValid() && DeviceUser == ControllerUser
		&& TargetPlayer && TargetPlayer->PlayerController == Controller.Get();
	const FRokCityPlacementRuntimeSnapshot Before =
		Controller->GetCityPlacementPreviewSnapshot();
	EscapeCancelCountBefore = Before.PlacementEscapeCancelCount;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), TEXT("escape_cancel"));
	Action->SetStringField(TEXT("method"), TEXT("GameViewportClient::InputKey Escape"));
	Action->SetStringField(TEXT("scope"), TEXT("engine_game_viewport_keyboard_route_not_os_focus"));
	Action->SetNumberField(TEXT("sent_frame"), GFrameCounter);
	Action->SetNumberField(TEXT("device_id"), Device.GetId());
	Action->SetNumberField(TEXT("device_platform_user_id"), DeviceUser.GetInternalId());
	Action->SetNumberField(TEXT("controller_platform_user_id"), ControllerUser.GetInternalId());
	Action->SetBoolField(TEXT("device_valid"), Device.IsValid());
	Action->SetBoolField(TEXT("target_local_player_present"), TargetPlayer != nullptr);
	Action->SetBoolField(TEXT("target_controller_matches"), bTargetsController);
	Action->SetNumberField(TEXT("escape_cancel_count_before"), EscapeCancelCountBefore);
	Action->SetNumberField(TEXT("last_escape_cancel_frame_before"),
		Before.LastPlacementEscapeCancelFrame);
	if (!Expect(Before.bActive && Before.PreviewActor.IsValid(),
		TEXT("Escape audit requires an active real placement preview")))
	{
		Actions.Add(MakeShared<FJsonValueObject>(Action));
		return false;
	}
	if (!bTargetsController)
	{
		Actions.Add(MakeShared<FJsonValueObject>(Action));
		return Expect(false, TEXT("Escape input device does not target the audit Controller"));
	}
	const bool Down = Viewport->InputKey(FInputKeyEventArgs(
		Viewport->Viewport, Device, EKeys::Escape, IE_Pressed, FPlatformTime::Cycles64()));
	const bool Up = Viewport->InputKey(FInputKeyEventArgs(
		Viewport->Viewport, Device, EKeys::Escape, IE_Released, FPlatformTime::Cycles64()));
	Action->SetBoolField(TEXT("down_returned_handled_immediate"), Down);
	Action->SetBoolField(TEXT("up_returned_handled_immediate"), Up);
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	EscapeSentFrame = GFrameCounter;
	return true;
}

TSharedRef<FJsonObject> URokCityPlacementRuntimeAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("step"), Step);
	Json->SetNumberField(TEXT("frame"), GFrameCounter);
	Json->SetNumberField(TEXT("elapsed_seconds"), Elapsed);
	Json->SetNumberField(TEXT("hall_completion_events"), CompletionCount);
	Json->SetBoolField(TEXT("placement_baseline_frozen"), bBaselineReady);
	Json->SetBoolField(TEXT("build_city_open"), Screen.IsValid()
		&& Screen->IsScreenOpen() && Screen->IsBuildCityModeForAudit());
	Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	Json->SetBoolField(TEXT("pointer_down"), bPointerDown);
	if (Controller.IsValid())
	{
		const auto SnapshotValue = Controller->GetCityPlacementPreviewSnapshot();
		Json->SetBoolField(TEXT("active"), SnapshotValue.bActive);
		Json->SetBoolField(TEXT("dragging"), SnapshotValue.bDragging);
		Json->SetBoolField(TEXT("grid_valid"), SnapshotValue.bGridValid);
		Json->SetBoolField(TEXT("camera_input_suspended"), SnapshotValue.bCameraInputSuspended);
		Json->SetNumberField(TEXT("escape_cancel_count"), SnapshotValue.PlacementEscapeCancelCount);
		Json->SetNumberField(TEXT("last_escape_cancel_frame"),
			SnapshotValue.LastPlacementEscapeCancelFrame);
		Json->SetNumberField(TEXT("type_id"), SnapshotValue.TypeId);
		Json->SetObjectField(TEXT("grid"), GridJson(SnapshotValue.Grid));
		Json->SetObjectField(TEXT("footprint"), GridJson(SnapshotValue.Footprint));
		Json->SetBoolField(TEXT("ghost_pointer_valid"), SnapshotValue.PreviewActor.IsValid());
		Json->SetBoolField(TEXT("selection_is_initial_hall"),
			Hall.IsValid() && SnapshotValue.CurrentSelection.Get() == Hall.Get());
		Json->SetBoolField(TEXT("prior_selection_is_initial_hall"),
			Hall.IsValid() && SnapshotValue.PriorSelection.Get() == Hall.Get());
		Json->SetBoolField(TEXT("await_pointer_release"), SnapshotValue.bAwaitPointerRelease);
		Json->SetNumberField(TEXT("suppress_primary_click_through_frame"), SnapshotValue.SuppressPrimaryClickThroughFrame);
		Json->SetNumberField(TEXT("primary_press_event_count"), SnapshotValue.PrimaryPressEventCount);
		Json->SetNumberField(TEXT("last_primary_press_frame"), SnapshotValue.LastPrimaryPressFrame);
		Json->SetStringField(TEXT("last_primary_press_outcome"), SnapshotValue.LastPrimaryPressOutcome.ToString());
		Json->SetNumberField(TEXT("placement_drag_begin_count"), SnapshotValue.PlacementDragBeginCount);
		Json->SetNumberField(TEXT("last_placement_drag_begin_frame"), SnapshotValue.LastPlacementDragBeginFrame);
		Json->SetNumberField(TEXT("placement_pointer_release_count"), SnapshotValue.PlacementPointerReleaseCount);
		Json->SetNumberField(TEXT("last_placement_pointer_release_frame"), SnapshotValue.LastPlacementPointerReleaseFrame);
		Json->SetNumberField(TEXT("last_placement_press_frame"), SnapshotValue.LastPlacementPressFrame);
		Json->SetStringField(TEXT("last_placement_press_failure"), SnapshotValue.LastPlacementPressFailure.ToString());
		Json->SetStringField(TEXT("last_placement_press_error"), SnapshotValue.LastPlacementPressError);
		Json->SetObjectField(TEXT("last_placement_press_mouse"), PointJson(SnapshotValue.LastPlacementPressMouse));
		Json->SetObjectField(TEXT("last_placement_press_ground"), VectorJson(SnapshotValue.LastPlacementPressGround));
		Json->SetObjectField(TEXT("last_placement_press_local_hit"), PointJson(SnapshotValue.LastPlacementPressLocalHit));
		Json->SetObjectField(TEXT("last_placement_press_local_center"), PointJson(SnapshotValue.LastPlacementPressLocalCenter));
		if (ARokCityPlacementPreviewActor* Ghost = Cast<ARokCityPlacementPreviewActor>(
			SnapshotValue.PreviewActor.Get()))
		{
			auto Geometry = MakeShared<FJsonObject>();
			Geometry->SetObjectField(TEXT("actor_world_transform"), TransformJson(Ghost->GetActorTransform()));
			Geometry->SetBoolField(TEXT("actor_hidden"), Ghost->IsHidden());
			if (const USceneComponent* Root = Ghost->GetRootComponent())
				Geometry->SetObjectField(TEXT("root_world_transform"), TransformJson(Root->GetComponentTransform()));
			if (const UStaticMeshComponent* Building = Ghost->GetBuildingMeshComponent())
				if (const USceneComponent* Candidate = Building->GetAttachParent())
					Geometry->SetObjectField(TEXT("candidate_world_transform"), TransformJson(Candidate->GetComponentTransform()));
			Geometry->SetObjectField(TEXT("building"), PreviewComponentJson(Ghost->GetBuildingMeshComponent()));
			Geometry->SetObjectField(TEXT("bottom"), PreviewComponentJson(Ghost->GetBottomMeshComponent()));
			FVector ActorOrigin, ActorExtent;
			Ghost->GetActorBounds(false, ActorOrigin, ActorExtent);
			Geometry->SetObjectField(TEXT("actor_bounds_origin"), VectorJson(ActorOrigin));
			Geometry->SetObjectField(TEXT("actor_bounds_box_extent"), VectorJson(ActorExtent));
			Json->SetObjectField(TEXT("preview_geometry"), Geometry);
		}
		if (WorldFrame.IsValid())
		{
			auto Projection = MakeShared<FJsonObject>();
			const FTransform& City = WorldFrame.GetCityContainerTransform();
			Projection->SetObjectField(TEXT("city_container_transform"), TransformJson(City));
			FString Error;
			FVector CenterWorld;
			const bool bCenter = WorldFrame.GridCenterToWorld(
				{SnapshotValue.Grid, SnapshotValue.Footprint}, CenterWorld, Error);
			Projection->SetBoolField(TEXT("grid_center_world_valid"), bCenter);
			Projection->SetStringField(TEXT("grid_center_error"), Error);
			Projection->SetObjectField(TEXT("grid_center_world"), VectorJson(CenterWorld));
			FVector2D CenterScreen;
			const bool bProject = bCenter && Controller->ProjectWorldLocationToScreen(
				CenterWorld, CenterScreen, true);
			UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
			FViewport* ActualViewport = ViewportClient ? ViewportClient->Viewport : nullptr;
			const FIntPoint Initial = ActualViewport
				? ActualViewport->GetInitialPositionXY() : FIntPoint::ZeroValue;
			Projection->SetObjectField(TEXT("viewport_initial_position"), GridJson(Initial));
			Projection->SetObjectField(TEXT("viewport_actual_size"), GridJson(ActualViewport
				? ActualViewport->GetSizeXY() : FIntPoint::ZeroValue));
			const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
			FSceneViewProjectionData ProjectionData;
			const bool bProjectionData = LocalPlayer && ActualViewport
				&& LocalPlayer->GetProjectionData(ActualViewport, ProjectionData);
			Projection->SetBoolField(TEXT("projection_data_valid"), bProjectionData);
			if (bProjectionData)
			{
				Projection->SetObjectField(TEXT("projection_view_rect"), RectJson(ProjectionData.GetViewRect()));
				Projection->SetObjectField(TEXT("projection_constrained_view_rect"),
					RectJson(ProjectionData.GetConstrainedViewRect()));
				Projection->SetObjectField(TEXT("projection_view_origin"), VectorJson(ProjectionData.ViewOrigin));
				Projection->SetObjectField(TEXT("view_rotation_matrix"), MatrixJson(ProjectionData.ViewRotationMatrix));
				Projection->SetObjectField(TEXT("projection_matrix"), MatrixJson(ProjectionData.ProjectionMatrix));
				Projection->SetObjectField(TEXT("view_projection_matrix"),
					MatrixJson(ProjectionData.ComputeViewProjectionMatrix()));
			}
			if (const APlayerCameraManager* Camera = Controller->PlayerCameraManager)
			{
				const FMinimalViewInfo& POV = Camera->GetCameraCacheView();
				Projection->SetObjectField(TEXT("camera_pov_location"), VectorJson(POV.Location));
				Projection->SetObjectField(TEXT("camera_pov_rotation_euler"),
					VectorJson(POV.Rotation.Euler()));
				Projection->SetNumberField(TEXT("camera_pov_fov_degrees"), POV.FOV);
				Projection->SetObjectField(TEXT("camera_manager_location"), VectorJson(Camera->GetCameraLocation()));
				Projection->SetObjectField(TEXT("camera_manager_rotation_euler"),
					VectorJson(Camera->GetCameraRotation().Euler()));
				Projection->SetNumberField(TEXT("camera_manager_fov_degrees"), Camera->GetFOVAngle());
			}
			FVector2D UnconstrainedScreen;
			const bool bUnconstrained = bCenter && Controller->ProjectWorldLocationToScreen(
				CenterWorld, UnconstrainedScreen, false);
			const FVector2D ViewportLocal = UnconstrainedScreen - FVector2D(Initial);
			// Preserve the established field name, now with its promised whole-viewport meaning.
			Projection->SetBoolField(TEXT("grid_center_screen_valid"), bUnconstrained);
			Projection->SetObjectField(TEXT("grid_center_viewport"), PointJson(ViewportLocal));
			Projection->SetBoolField(TEXT("grid_center_project_false_valid"), bUnconstrained);
			Projection->SetObjectField(TEXT("grid_center_project_true_constrained_local"), PointJson(CenterScreen));
			Projection->SetObjectField(TEXT("grid_center_project_false_view_rect_absolute"),
				PointJson(UnconstrainedScreen));
			Projection->SetObjectField(TEXT("grid_center_project_false_minus_viewport_initial"),
				PointJson(ViewportLocal));
			if (bProject)
			{
				Projection->SetObjectField(TEXT("deproject_project_true_exact"),
					DeprojectionJson(Controller.Get(), CenterScreen));
				Projection->SetObjectField(TEXT("deproject_project_true_plus_initial"),
					DeprojectionJson(Controller.Get(), CenterScreen + FVector2D(Initial)));
			}
			if (bUnconstrained)
			{
				Projection->SetObjectField(TEXT("deproject_project_false_exact"),
					DeprojectionJson(Controller.Get(), UnconstrainedScreen));
				Projection->SetObjectField(TEXT("deproject_project_false_minus_initial"),
					DeprojectionJson(Controller.Get(), ViewportLocal));
			}
			if (bUnconstrained)
			{
				const FVector2D Pixels = UWidgetLayoutLibrary::GetViewportSize(this);
				const FGeometry ViewGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
				if (Pixels.X > 0 && Pixels.Y > 0)
					Projection->SetObjectField(TEXT("grid_center_absolute"), PointJson(
						ViewGeometry.LocalToAbsolute(FVector2D(ViewportLocal.X * ViewGeometry.GetLocalSize().X / Pixels.X,
							ViewportLocal.Y * ViewGeometry.GetLocalSize().Y / Pixels.Y))));
			}
			FVector InverseGround;
			const FVector2D AttemptMouse = SnapshotValue.LastPlacementPressFrame > 0
				? SnapshotValue.LastPlacementPressMouse : FVector2D::ZeroVector;
			const bool bGround = SnapshotValue.LastPlacementPressFrame > 0
				&& WorldFrame.TryScreenToGround(*Controller, AttemptMouse, InverseGround, Error);
			Projection->SetBoolField(TEXT("last_press_inverse_ground_valid"), bGround);
			Projection->SetStringField(TEXT("last_press_inverse_ground_error"), Error);
			Projection->SetObjectField(TEXT("last_press_inverse_ground"), VectorJson(InverseGround));
			FVector2D InverseLocal, SourceCenter;
			const bool bLocal = bGround
				&& FRokCityPlacementPreviewMath::WorldToCityLocal(City, InverseGround, InverseLocal, Error);
			Projection->SetBoolField(TEXT("last_press_inverse_local_valid"), bLocal);
			Projection->SetStringField(TEXT("last_press_inverse_local_error"), Error);
			Projection->SetObjectField(TEXT("last_press_inverse_local"), PointJson(InverseLocal));
			const bool bSourceCenter = FRokCityPlacementPreviewMath::GridToLocalCenter(
				{SnapshotValue.Grid, SnapshotValue.Footprint}, SourceCenter, Error);
			Projection->SetBoolField(TEXT("source_grid_local_center_valid"), bSourceCenter);
			Projection->SetObjectField(TEXT("source_grid_local_center"), PointJson(SourceCenter));
			if (bLocal && bSourceCenter)
			{
				Projection->SetObjectField(TEXT("last_press_local_delta"), PointJson(InverseLocal - SourceCenter));
				Projection->SetObjectField(TEXT("allowed_half_footprint"), PointJson(
					FVector2D(SnapshotValue.Footprint.X * 0.05,
						SnapshotValue.Footprint.Y * 0.05)));
			}
			Json->SetObjectField(TEXT("world_frame_projection"), Projection);
		}
	}
	Json->SetObjectField(TEXT("input_route"), InputRouteJson(GetWorld(), Controller.Get(), LastAbsolutePointer));
	int32 Ghosts = 0;
	for (TActorIterator<ARokCityPlacementPreviewActor> It(GetWorld()); It; ++It)
		if (!It->IsActorBeingDestroyed()) ++Ghosts;
	Json->SetNumberField(TEXT("ghost_actor_count"), Ghosts);
	int32 SourceCount = 0, NewSourceCount = 0;
	for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		++SourceCount;
		if (!BaselineSourceActors.Contains(TWeakObjectPtr<ARokCitySpriteActor>(*It)))
			++NewSourceCount;
	}
	int32 InitTaggedCount = 0, NewInitTaggedCount = 0, ConstructedCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(TEXT("RokLocalConstructedBuilding"))) ++ConstructedCount;
		if (!HasTagPrefix(*It, TEXT("UnityInitId_"))) continue;
		++InitTaggedCount;
		if (!BaselineUnityInitActors.Contains(TWeakObjectPtr<AActor>(*It)))
			++NewInitTaggedCount;
	}
	Json->SetNumberField(TEXT("source_actor_count"), SourceCount);
	Json->SetNumberField(TEXT("new_source_actor_count"), NewSourceCount);
	Json->SetNumberField(TEXT("unity_init_tagged_actor_count"), InitTaggedCount);
	Json->SetNumberField(TEXT("new_unity_init_tagged_actor_count"), NewInitTaggedCount);
	Json->SetNumberField(TEXT("formal_constructed_tagged_actor_count"), ConstructedCount);
	if (Buildings.IsValid())
	{
		const auto Wallet = Buildings->GetCurrentResources();
		Json->SetNumberField(TEXT("food"), Wallet.Food);
		Json->SetNumberField(TEXT("wood"), Wallet.Wood);
		Json->SetNumberField(TEXT("stone"), Wallet.Stone);
		Json->SetNumberField(TEXT("gold"), Wallet.Gold);
		Json->SetNumberField(TEXT("upgrade_queues"), Buildings->GetActiveUpgradeCount());
		Json->SetNumberField(TEXT("local_construction_tickets"),
			Buildings->GetActiveLocalConstructionCount());
		Json->SetNumberField(TEXT("owned_queues"), Buildings->GetOwnedBuildQueueCount());
		Json->SetNumberField(TEXT("power_delta"), Buildings->GetLocalCompletedBuildingPowerDelta());
		FRokLocalConstructionTicket FirstTicket;
		Json->SetBoolField(TEXT("first_formal_ticket_exists"),
			Buildings->GetLocalConstructionTicket(1, FirstTicket));
		Json->SetStringField(TEXT("profile_sha256"), Runtime.IsValid()
			? RokTrainArmyMesh::Sha256(Snapshot(Runtime->GetPlayerProfileState())) : FString());
		Json->SetStringField(TEXT("task_sha256"), Runtime.IsValid()
			? RokTrainArmyMesh::Sha256(Snapshot(Runtime->GetActiveTask())) : FString());
	}
	if (Hud.IsValid())
	{
		const UWidget* Controls = Hud->GetWidgetFromName(TEXT("CityPlacementControls"));
		const UTextBlock* Status = Cast<UTextBlock>(
			Hud->GetWidgetFromName(TEXT("CityPlacementStatus")));
		const UTextBlock* Reason = Cast<UTextBlock>(
			Hud->GetWidgetFromName(TEXT("CityPlacementReason")));
		Json->SetBoolField(TEXT("controls_visible"), Controls && Controls->IsVisible());
		Json->SetStringField(TEXT("status"), Status ? Status->GetText().ToString() : FString());
		Json->SetStringField(TEXT("reason"), Reason ? Reason->GetText().ToString() : FString());
	}
	return Json;
}

void URokCityPlacementRuntimeAuditSubsystem::Capture(const TCHAR* Label, int32 NextStep)
{
	if (!Unchanged()) return;
	if (FScreenshotRequest::IsScreenshotRequested() || Pending.IsValid())
	{
		Finish(TEXT("Conflicting placement screenshot request"));
		return;
	}
	PendingPath = Directory / (FString(Label) + TEXT(".png"));
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase_name"), Label);
	Pending->SetStringField(TEXT("file"), PendingPath);
	StepAfterCapture = NextStep;
	ShotStarted = Elapsed;
	bScreenshotRequested = false;
	bScreenshotProcessed = false;
	RenderReadyFrames = 0;
	PostScreenshotReadyFrames = 0;
	LastRenderReadyFrame = 0;
	LastObservedBuildingRenderTime = 0.0;
	LastObservedBottomRenderTime = 0.0;
}

bool URokCityPlacementRuntimeAuditSubsystem::PrepareReferencedPreviewAssets(
	ARokCityPlacementPreviewActor* Ghost, FString& Error)
{
	Error.Reset();
	if (!Ghost) return true;
	if (PreparedGhost.Get() == Ghost) return true;
	UStaticMeshComponent* Components[] =
		{ Ghost->GetBuildingMeshComponent(), Ghost->GetBottomMeshComponent() };
	TArray<UObject*> ReferencedAssets;
	for (UStaticMeshComponent* Component : Components)
	{
		if (!Component || !Component->GetStaticMesh() || !Component->GetMaterial(0))
		{
			Error = TEXT("Preview missing an actually referenced mesh or material");
			return false;
		}
		ReferencedAssets.AddUnique(Component->GetStaticMesh());
		UMaterialInterface* Material = Component->GetMaterial(0);
		ReferencedAssets.AddUnique(Material);
		Material->EnsureIsComplete();
		TArray<UTexture*> Textures;
		Material->GetUsedTextures(Textures);
		if (Textures.IsEmpty())
		{
			Error = TEXT("Preview material exposed no used source textures");
			return false;
		}
		for (UTexture* Texture : Textures)
		{
			if (!Texture) { Error = TEXT("Preview material has a missing used texture"); return false; }
			ReferencedAssets.AddUnique(Texture);
		}
	}
#if WITH_EDITOR
	if (!IsInGameThread())
	{
		Error = TEXT("Targeted preview asset compilation must run on the game thread");
		return false;
	}
	// This only completes objects bound by the current ghost, including its mesh cards.
	FAssetCompilingManager::Get().FinishCompilationForObjects(ReferencedAssets);
#endif
	PreparedGhost = Ghost;
	GhostReadyObservedWorldSeconds = GetWorld()->GetTimeSeconds();
	RenderReadyFrames = 0;
	LastObservedBuildingRenderTime = 0.0;
	LastObservedBottomRenderTime = 0.0;
	return true;
}

bool URokCityPlacementRuntimeAuditSubsystem::IsRenderReadyForCapture(
	TSharedRef<FJsonObject> Evidence) const
{
	const auto SnapshotValue = Controller.IsValid()
		? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
	auto* Ghost = Cast<ARokCityPlacementPreviewActor>(SnapshotValue.PreviewActor.Get());
	Evidence->SetBoolField(TEXT("ghost_expected"), SnapshotValue.bActive);
	Evidence->SetBoolField(TEXT("ghost_present"), Ghost != nullptr);
	int32 Remaining = 0;
#if WITH_EDITOR
	Remaining = FAssetCompilingManager::Get().GetNumRemainingAssets();
#endif
	Evidence->SetNumberField(TEXT("global_compile_remaining_read_only"), Remaining);
	if (SnapshotValue.bActive && !Ghost) return false;
	if (!Ghost) return Remaining == 0;
	if (PreparedGhost.Get() != Ghost) return false;
	bool bReady = Remaining == 0 && !Ghost->IsHidden();
	UStaticMeshComponent* Components[] =
		{ Ghost->GetBuildingMeshComponent(), Ghost->GetBottomMeshComponent() };
	const TCHAR* Roles[] = { TEXT("building"), TEXT("bottom") };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto Item = MakeShared<FJsonObject>();
		UStaticMeshComponent* Component = Components[Index];
		UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		UMaterialInterface* Material = Component ? Component->GetMaterial(0) : nullptr;
		const bool bMeshReady = Mesh && !Mesh->IsCompiling()
			&& Mesh->GetRenderData() && Mesh->GetNumLODs() > 0;
		const bool bMaterialReady = Material && Material->IsComplete();
		const bool bComponentReady = Component && Component->IsRegistered()
			&& Component->IsVisible() && Component->IsRenderStateCreated();
		const double LastOnScreen = Component
			? Component->GetLastRenderTimeOnScreen() : 0.0;
		const bool bRenderedAfterReady = LastOnScreen > GhostReadyObservedWorldSeconds
			&& GetWorld()->GetTimeSeconds() - LastOnScreen < 0.25;
		Item->SetStringField(TEXT("mesh"), Mesh ? Mesh->GetPathName() : FString());
		Item->SetStringField(TEXT("material"), Material ? Material->GetPathName() : FString());
		Item->SetBoolField(TEXT("mesh_ready"), bMeshReady);
		Item->SetBoolField(TEXT("material_ready"), bMaterialReady);
		Item->SetBoolField(TEXT("component_ready"), bComponentReady);
		Item->SetNumberField(TEXT("last_render_time_on_screen"), LastOnScreen);
		Item->SetBoolField(TEXT("rendered_after_targeted_compile"), bRenderedAfterReady);
		TArray<UTexture*> Textures;
		if (Material) Material->GetUsedTextures(Textures);
		TArray<TSharedPtr<FJsonValue>> TextureEvidence;
		bool bTexturesReady = !Textures.IsEmpty();
		for (UTexture* Texture : Textures)
		{
			auto TextureItem = MakeShared<FJsonObject>();
			const bool bTextureReady = Texture && !Texture->IsCompiling()
				&& Texture->GetResource() != nullptr;
			TextureItem->SetStringField(TEXT("asset"), Texture ? Texture->GetPathName() : FString());
			TextureItem->SetBoolField(TEXT("ready"), bTextureReady);
			TextureEvidence.Add(MakeShared<FJsonValueObject>(TextureItem));
			bTexturesReady &= bTextureReady;
		}
		Item->SetArrayField(TEXT("textures"), TextureEvidence);
		Item->SetBoolField(TEXT("textures_ready"), bTexturesReady);
		Evidence->SetObjectField(Roles[Index], Item);
		bReady &= bMeshReady && bMaterialReady && bComponentReady
			&& bTexturesReady && bRenderedAfterReady;
	}
	return bReady;
}

void URokCityPlacementRuntimeAuditSubsystem::AdvancePendingCapture()
{
	if (!Pending.IsValid() || bFinished) return;
	const auto SnapshotValue = Controller->GetCityPlacementPreviewSnapshot();
	FString Error;
	if (SnapshotValue.bActive && !PrepareReferencedPreviewAssets(
		Cast<ARokCityPlacementPreviewActor>(SnapshotValue.PreviewActor.Get()), Error))
	{
		Pending->SetStringField(TEXT("target_asset_preflight_error"), Error);
		Finish(Error.IsEmpty() ? TEXT("Preview actor unavailable for target asset readiness") : Error);
		return;
	}
	auto Evidence = MakeShared<FJsonObject>();
	Evidence->SetNumberField(TEXT("observation_frame"), GFrameCounter);
	const bool bReady = IsRenderReadyForCapture(Evidence);
	bool bDistinctRenderedFrame = true;
	if (SnapshotValue.bActive && bReady)
	{
		const double BuildingTime = Evidence->GetObjectField(TEXT("building"))
			->GetNumberField(TEXT("last_render_time_on_screen"));
		const double BottomTime = Evidence->GetObjectField(TEXT("bottom"))
			->GetNumberField(TEXT("last_render_time_on_screen"));
		bDistinctRenderedFrame = BuildingTime > LastObservedBuildingRenderTime
			&& BottomTime > LastObservedBottomRenderTime;
		if (bDistinctRenderedFrame)
		{
			LastObservedBuildingRenderTime = BuildingTime;
			LastObservedBottomRenderTime = BottomTime;
		}
	}
	Evidence->SetBoolField(TEXT("distinct_target_render_frame"), bDistinctRenderedFrame);
	Pending->SetObjectField(TEXT("latest_render_readiness"), Evidence);
	if (!bScreenshotRequested)
	{
		if (LastRenderReadyFrame != GFrameCounter)
		{
			RenderReadyFrames = bReady && bDistinctRenderedFrame ? RenderReadyFrames + 1 : 0;
			LastRenderReadyFrame = GFrameCounter;
		}
		if (RenderReadyFrames < 2) return;
		Pending->SetObjectField(TEXT("pre_capture_render_ready"), Evidence);
		Pending->SetNumberField(TEXT("pre_capture_ready_frames"), RenderReadyFrames);
		Pending->SetObjectField(TEXT("request"), State());
		bScreenshotRequested = true;
		FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
		return;
	}
	if (!bScreenshotProcessed || GFrameCounter <= ScreenshotProcessedFrame) return;
	if (LastRenderReadyFrame != GFrameCounter)
	{
		PostScreenshotReadyFrames = bReady && bDistinctRenderedFrame
			? PostScreenshotReadyFrames + 1 : 0;
		LastRenderReadyFrame = GFrameCounter;
	}
	if (PostScreenshotReadyFrames < 2) return;
	Pending->SetObjectField(TEXT("post_capture_render_ready"), Evidence);
	Pending->SetNumberField(TEXT("post_capture_ready_frames"), PostScreenshotReadyFrames);
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(StepAfterCapture);
}

void URokCityPlacementRuntimeAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || !bScreenshotRequested || !Unchanged()) return;
	TArray<uint8> Bytes;
	const int64 Size = IFileManager::Get().FileSize(*PendingPath);
	if (!Expect(Size > 0 && FFileHelper::LoadFileToArray(Bytes, *PendingPath)
		&& !Bytes.IsEmpty(), TEXT("Placement screenshot callback produced no PNG"))) return;
	Pending->SetObjectField(TEXT("processed"), State());
	Pending->SetNumberField(TEXT("file_bytes"), Size);
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	ScreenshotProcessedFrame = GFrameCounter;
	bScreenshotProcessed = true;
	const auto SnapshotValue = Controller->GetCityPlacementPreviewSnapshot();
	if (auto* Ghost = Cast<ARokCityPlacementPreviewActor>(SnapshotValue.PreviewActor.Get()))
	{
		LastObservedBuildingRenderTime = Ghost->GetBuildingMeshComponent()
			? Ghost->GetBuildingMeshComponent()->GetLastRenderTimeOnScreen() : 0.0;
		LastObservedBottomRenderTime = Ghost->GetBottomMeshComponent()
			? Ghost->GetBottomMeshComponent()->GetLastRenderTimeOnScreen() : 0.0;
	}
	else
	{
		LastObservedBuildingRenderTime = 0.0;
		LastObservedBottomRenderTime = 0.0;
	}
}

bool URokCityPlacementRuntimeAuditSubsystem::Save(bool bComplete)
{
	auto Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("schema"), TEXT("rok.city_placement.runtime_audit.v1"));
	Manifest->SetBoolField(TEXT("complete"), bComplete);
	Manifest->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty()
		&& Captures.Num() == ExpectedCaptures && bBaselineReady && CompletionCount == 1);
	Manifest->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Manifest->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Manifest->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Manifest->SetNumberField(TEXT("capture_count"), Captures.Num());
	if (Pending.IsValid()) Manifest->SetObjectField(TEXT("pending_capture_diagnostics"), Pending.ToSharedRef());
	Manifest->SetNumberField(TEXT("expected_capture_count"), ExpectedCaptures);
	Manifest->SetBoolField(TEXT("production_placement_delegate"), true);
	Manifest->SetBoolField(TEXT("audit_listener_bound"), false);
	FRokLocalConstructionTicket FirstTicket;
	const bool bFirstTicket = Buildings.IsValid()
		&& Buildings->GetLocalConstructionTicket(1, FirstTicket);
	int32 FormalTagged = 0, NewSource = 0, NewInitTagged = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(TEXT("RokLocalConstructedBuilding"))) ++FormalTagged;
		if (HasTagPrefix(*It, TEXT("UnityInitId_"))
			&& !BaselineUnityInitActors.Contains(TWeakObjectPtr<AActor>(*It)))
			++NewInitTagged;
	}
	for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
		if (!It->IsActorBeingDestroyed()
			&& !BaselineSourceActors.Contains(TWeakObjectPtr<ARokCitySpriteActor>(*It)))
			++NewSource;
	Manifest->SetBoolField(TEXT("formal_construction_started"),
		bFirstTicket || FormalTagged > 0 || NewSource > 0 || NewInitTagged > 0
		|| (Buildings.IsValid() && Buildings->GetActiveLocalConstructionCount() > 0));
	Manifest->SetBoolField(TEXT("visual_fidelity_accepted"), false);
	Manifest->SetBoolField(TEXT("world_travel_covered"), false);
	Manifest->SetBoolField(TEXT("other_resolutions_covered"), false);
	Manifest->SetStringField(TEXT("scope"), TEXT("local_preview temporary Farm/Wood placement only; no formal construction, account or server action"));
	Manifest->SetObjectField(TEXT("inputs_sha256"),
		Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Manifest->SetObjectField(TEXT("own_game_window_preparation"),
		WindowPreparation.IsValid() ? WindowPreparation.ToSharedRef() : MakeShared<FJsonObject>());
	Manifest->SetObjectField(TEXT("authorized_hall_prerequisite_and_placement_baseline"),
		Baseline.IsValid() ? Baseline.ToSharedRef() : MakeShared<FJsonObject>());
	Manifest->SetArrayField(TEXT("actions"), Actions);
	Manifest->SetArrayField(TEXT("captures"), Captures);
	Manifest->SetObjectField(TEXT("final_state"), State());
	TArray<TSharedPtr<FJsonValue>> FailureValues;
	for (const FString& Failure : Failures)
		FailureValues.Add(MakeShared<FJsonValueString>(Failure));
	Manifest->SetArrayField(TEXT("failures"), FailureValues);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Manifest, Writer)) return false;
	return FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokCityPlacementRuntimeAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (bPointerDown && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ProcessMouseButtonUpEvent(FPointerEvent(
			0, LastAbsolutePointer, LastAbsolutePointer, TSet<FKey>(),
			EKeys::LeftMouseButton, 0, FModifierKeysState()));
		bPointerDown = false;
	}
	if (FSlateApplication::IsInitialized())
		FSlateApplication::Get().SetCursorPos(OriginalAbsolutePointer);
	const bool bComplete = Error.IsEmpty() && Failures.IsEmpty()
		&& Captures.Num() == ExpectedCaptures && bBaselineReady && CompletionCount == 1;
	if (!bComplete && Failures.IsEmpty()) Failures.Add(TEXT("Placement audit incomplete"));
	Save(bComplete);
	UE_LOG(LogTemp, Display, TEXT("Rok city placement runtime audit %s: %s"),
		bComplete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!bComplete) UE_LOG(LogTemp, Error, TEXT("Rok city placement audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, bComplete ? 0 : 1);
}

void URokCityPlacementRuntimeAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !TargetWorld(GetWorld())) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (Pending.IsValid())
	{
		if (Elapsed - ShotStarted > 25.0) Finish(TEXT("Placement screenshot callback timeout"));
		else AdvancePendingCapture();
		return;
	}
	if (Step > 0 && Elapsed - StepStarted > (Step == 3 ? 10.0 : 12.0))
	{
		Finish(FString::Printf(TEXT("Placement audit step %d timeout"), Step));
		return;
	}
	if (!Identity() || !Unchanged()) return;
	auto GridPoint = [this](const FIntPoint& Grid, bool bHeld, const TCHAR* Label)
	{
		FVector2D Viewport, Absolute;
		if (!bHeld)
			return Expect(ProjectGrid(Grid, Viewport, Absolute),
				TEXT("Source grid center is not in actual world viewport"))
				&& MovePointer(Viewport, Absolute, false, Label);
		TSharedPtr<FJsonObject> Preflight;
		const bool bProjected = ProjectHeldGridInterior(Grid, Viewport, Absolute, Preflight);
		if (Preflight.IsValid())
		{
			Preflight->SetStringField(TEXT("pointer_action"), Label);
			Actions.Add(MakeShared<FJsonValueObject>(Preflight));
		}
		return Expect(bProjected,
			TEXT("Held pointer pixel does not quantize to exact source grid interior"))
			&& MovePointer(Viewport, Absolute, true, Label);
	};
	switch (Step)
	{
	case 0:
		if (Begin()) Enter(1);
		break;
	case 1:
		if (ReadyForClick(Panel->GetWidgetFromName(TEXT("BuildingUpgradeButton")), false)
			&& SlateClick(Panel->GetWidgetFromName(TEXT("BuildingUpgradeButton")),
			TEXT("hall_upgrade_menu"))) Enter(2);
		break;
	case 2:
		if (Modal->IsModalOpen()
			&& ReadyForClick(Modal->GetWidgetFromName(TEXT("TimedUpgradeButton")),
				Modal->GetWindowAnimationForTesting().IsAnimating())
			&& SlateClick(Modal->GetWidgetFromName(TEXT("TimedUpgradeButton")),
				TEXT("hall_timed_upgrade_start")))
		{
			HallClickedWall = FPlatformTime::Seconds();
			Enter(3);
		}
		break;
	case 3:
		if (CompletionCount == 1 && !Buildings->GetBuildingStateForActor(Hall.Get()).bUpgradeInProgress)
			if (FreezePlacementBaseline()) Capture(TEXT("01_hall2_legal_prerequisite"), 4);
		break;
	case 4:
		if (ReadyForClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")), false)
			&& SlateClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")),
			TEXT("build_city_open_farm"))) Enter(5);
		break;
	case 5:
		if (Screen->IsScreenOpen() && Screen->IsBuildCityModeForAudit()
			&& ReadyForClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2")),
				Screen->GetWindowAnimationForTesting().IsAnimating())
			&& SlateClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2")),
				TEXT("farm_source_card"))) Enter(6);
		break;
	case 6:
		if (CheckPreview(2, true, false, true))
		{
			InitialGrid = Controller->GetCityPlacementPreviewSnapshot().Grid;
			if (ChooseTargets()) Capture(TEXT("02_farm_preview"), 7);
		}
		break;
	case 7:
		if (SlateUiOriginNoDrag(Hud->GetWidgetFromName(TEXT("TaskProgress_0"))))
			Enter(70);
		break;
	case 70:
		if (CheckPreview(2, true, false, true)
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == InitialGrid,
				TEXT("UI-origin held move changed the world placement grid one frame later")))
			Capture(TEXT("03_ui_origin_no_drag"), 8);
		break;
	case 8:
		if (GridPoint(InitialGrid, false, TEXT("move_to_farm_ghost"))
			&& WorldButton(true, TEXT("world_press_farm_ghost"))) Enter(9);
		break;
	case 9:
	{
		auto Observation = MakeShared<FJsonObject>();
		Observation->SetStringField(TEXT("action"), TEXT("world_press_next_frame_observation"));
		Observation->SetObjectField(TEXT("state_before_failure_cleanup"), State());
		Actions.Add(MakeShared<FJsonValueObject>(Observation));
		if (Expect(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount
				> WorldPressPrimaryCountBefore,
				TEXT("Slate SViewport down did not reach Controller primary input"))
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().LastPrimaryPressOutcome
				== FName(TEXT("placement_dispatched")),
				TEXT("Controller primary press did not dispatch placement input"))
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().PlacementDragBeginCount
				> WorldPressDragBeginCountBefore,
				TEXT("Controller placement input did not begin a world drag"))
			&& CheckPreview(2, true, true, true)
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == InitialGrid,
				TEXT("World press changed initial Farm grid")))
		{
			const FRokCityPlacementRuntimeSnapshot State =
				Controller->GetCityPlacementPreviewSnapshot();
			FRokCityConstructionFootprint Footprint;
			Footprint.Origin = InitialGrid;
			Footprint.Size = State.Footprint;
			FString MathError;
			PredictedDrag = FRokCityPlacementDragState();
			if (Expect(FRokCityPlacementPreviewMath::BeginDrag(PredictedDrag,
				Footprint, WorldFrame.GetCityContainerTransform(),
				State.LastPlacementPressGround, false, MathError),
				TEXT("Source drag prediction could not begin from real Controller press")))
				Capture(TEXT("04_world_press_camera_lock"), 10);
		}
		break;
	}
	case 10:
	{
		FVector2D Viewport, Absolute;
		if (Expect(ProjectSubgrid(Viewport, Absolute),
			TEXT("Sub-grid pointer not projectable"))
			&& MovePointer(Viewport, Absolute, true, TEXT("subgrid_0p04_move")))
			Enter(11);
		break;
	}
	case 11:
		if (CheckPreview(2, true, true, true)
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == InitialGrid,
				TEXT("Sub-grid move incorrectly crossed a source grid cell")))
			Capture(TEXT("05_subgrid_no_change"), 12);
		break;
	case 12:
		if (GridPoint(ValidGrid, true, TEXT("cross_source_grid_cell"))) Enter(13);
		break;
	case 13:
		if (CheckPreview(2, true, true, true)
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == ValidGrid,
				TEXT("Drag did not move to the source valid grid")))
			Capture(TEXT("06_crosscell_valid"), 14);
		break;
	case 14:
		if (GridPoint(OccupiedGrid, true, TEXT("drag_to_occupied_grid"))) Enter(15);
		break;
	case 15:
		if (CheckPreview(2, true, true, false)
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == OccupiedGrid,
				TEXT("Occupied drag did not reach source grid")))
			Capture(TEXT("07_occupied_invalid"), 16);
		break;
	case 16:
		if (GridPoint(OutboundGrid, true, TEXT("drag_to_outbound_grid"))) Enter(17);
		break;
	case 17:
		if (CheckPreview(2, true, true, false)
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == OutboundGrid,
				TEXT("Outbound drag did not reach source grid")))
			Capture(TEXT("08_outbound_invalid"), 18);
		break;
	case 18:
		if (WorldButton(false, TEXT("release_world_drag"))) Enter(19);
		break;
	case 19:
		if (Expect(Controller->GetCityPlacementPreviewSnapshot().PlacementPointerReleaseCount
				> WorldReleaseCountBefore,
				TEXT("World release did not reach Controller placement input"))
			&& CheckPreview(2, true, false, false))
			Capture(TEXT("09_release_camera_restored"), 20);
		break;
	case 20:
		if (ReadyForClick(Hud->GetWidgetFromName(TEXT("CityPlacementCancelButton")), false)
			&& SlateClick(Hud->GetWidgetFromName(TEXT("CityPlacementCancelButton")),
			TEXT("placement_cancel_button"))) Enter(21);
		break;
	case 21:
		if (CheckPreview(0, false, false, false))
			Capture(TEXT("10_cancel_restores_city"), 22);
		break;
	case 22:
		if (ReadyForClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")), false)
			&& SlateClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")),
			TEXT("build_city_reopen"))) Enter(23);
		break;
	case 23:
		if (Screen->IsScreenOpen() && Screen->IsBuildCityModeForAudit()
			&& ReadyForClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2")),
				Screen->GetWindowAnimationForTesting().IsAnimating())
			&& SlateClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2")),
				TEXT("farm_reopen_card"))) Enter(24);
		break;
	case 24:
		if (CheckPreview(2, true, false, true)
			&& ReadyForClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")), false)
			&& SlateClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")),
				TEXT("switch_to_wood_build_city"))) Enter(25);
		break;
	case 25:
		if (CheckPreview(0, false, false, false)
			&& Screen->IsScreenOpen() && Screen->IsBuildCityModeForAudit()
			&& ReadyForClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_3")),
				Screen->GetWindowAnimationForTesting().IsAnimating())
			&& SlateClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_3")),
				TEXT("wood_source_card"))) Enter(26);
		break;
	case 26:
		if (CheckPreview(3, true, false, true))
			Capture(TEXT("11_wood_switch_one_ghost"), 27);
		break;
	case 27:
		if (PressEscape()) Enter(28);
		break;
	case 28:
		if (GFrameCounter <= EscapeSentFrame) break;
		{
			const FRokCityPlacementRuntimeSnapshot After =
				Controller->GetCityPlacementPreviewSnapshot();
			if (!Expect(After.PlacementEscapeCancelCount == EscapeCancelCountBefore + 1
				&& After.LastPlacementEscapeCancelFrame > EscapeSentFrame
				&& After.LastPlacementEscapeCancelFrame <= GFrameCounter,
				TEXT("Controller Escape handler did not cancel this preview exactly once"))) break;
		}
		if (CheckPreview(0, false, false, false))
		{
			auto Observation = MakeShared<FJsonObject>();
			Observation->SetStringField(TEXT("action"), TEXT("escape_next_input_frame_observation"));
			Observation->SetNumberField(TEXT("sent_frame"), EscapeSentFrame);
			Observation->SetNumberField(TEXT("observed_frame"), GFrameCounter);
			Observation->SetNumberField(TEXT("escape_cancel_count_after"),
				Controller->GetCityPlacementPreviewSnapshot().PlacementEscapeCancelCount);
			Observation->SetNumberField(TEXT("last_escape_cancel_frame"),
				Controller->GetCityPlacementPreviewSnapshot().LastPlacementEscapeCancelFrame);
			Observation->SetObjectField(TEXT("state_before_finish"), State());
			Actions.Add(MakeShared<FJsonValueObject>(Observation));
			Finish();
		}
		break;
	default:
		Finish(TEXT("Placement audit reached an invalid state"));
		break;
	}
}
