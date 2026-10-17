#include "RokLocalConstructionRuntimeAuditSubsystem.h"

#include "RokBuildingPanelWidget.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokCityPlacementPreviewActor.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCitySpriteActor.h"
#include "RokBuildingSelectionIndicator.h"
#include "RokCityGammaLayerCensusSubsystem.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokStrategyCameraPawn.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
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
#include "Engine/Texture2D.h"
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
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR AuditFlag[] = TEXT("-RokLocalConstructionAudit");
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

TSharedRef<FJsonObject> PlacementWidgetGeometry(const UWidget* Widget)
{
	auto Item = MakeShared<FJsonObject>();
	Item->SetBoolField(TEXT("present"), Widget != nullptr);
	Item->SetBoolField(TEXT("visible_with_ancestors"), Visible(Widget));
	Item->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	if (!Widget) return Item;
	const FGeometry Geometry = Widget->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
	Item->SetObjectField(TEXT("local_size"), PointJson(Size));
	Item->SetObjectField(TEXT("absolute_top_left"), PointJson(TopLeft));
	Item->SetObjectField(TEXT("absolute_bottom_right"), PointJson(BottomRight));
	Item->SetObjectField(TEXT("absolute_center"), PointJson((TopLeft + BottomRight) * 0.5));
	Item->SetObjectField(TEXT("absolute_size"), PointJson(BottomRight - TopLeft));
	return Item;
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
	FVector LocalMin = FVector::ZeroVector, LocalMax = FVector::ZeroVector;
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

bool URokLocalConstructionRuntimeAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalConstructionAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokLocalConstructionRuntimeAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}

TStatId URokLocalConstructionRuntimeAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokLocalConstructionRuntimeAuditSubsystem, STATGROUP_Tickables);
}

void URokLocalConstructionRuntimeAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/LocalConstructionRuntimeAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokLocalConstructionRuntimeAuditSubsystem::ScreenshotProcessed);
}

void URokLocalConstructionRuntimeAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (FSlateApplication::IsInitialized() && OriginalAbsolutePointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalAbsolutePointer);
	Super::Deinitialize();
}

bool URokLocalConstructionRuntimeAuditSubsystem::Expect(bool Condition, const TCHAR* Message)
{
	if (!Condition) Finish(Message);
	return Condition;
}

bool URokLocalConstructionRuntimeAuditSubsystem::Identity()
{
	return Expect(TargetWorld(GetWorld()), TEXT("Placement audit project or world identity changed"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::PrepareOwnGameWindow()
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

void URokLocalConstructionRuntimeAuditSubsystem::Enter(int32 NextStep)
{
	Step = NextStep;
	StepStarted = Elapsed;
	StableClickTarget.Reset();
	StableClickFrames = 0;
}

void URokLocalConstructionRuntimeAuditSubsystem::OnUpgradeCompleted(
	const FRokLocalBuildingUpgradeCompleted& Completed)
{
	if (Completed.UnityInitId == 5 && Completed.UnityBuildingTypeId == 1
		&& Completed.PreviousLevel == 1 && Completed.CurrentLevel == 2
		&& Completed.PreviousPower == 7 && Completed.CurrentPower == 21)
	{
		++CompletionCount;
		return;
	}
	if (Completed.UnityInitId == 0
		&& (Completed.UnityBuildingTypeId == 2 || Completed.UnityBuildingTypeId == 3)
		&& Completed.PreviousLevel == 0 && Completed.CurrentLevel == 1
		&& Completed.PreviousPower == 0 && Completed.CurrentPower == 5)
	{
		++LocalCompletionCount;
		if (Completed.UnityBuildingTypeId == 2)
		{
			if (!CheckProduction(FarmId, TEXT("farm_first_completion_production"),
				FarmCompletionProduction)) return;
		}
		else if (!CheckProduction(WoodId, TEXT("wood_first_completion_production"),
			WoodCompletionProduction)) return;
		return;
	}
	Finish(TEXT("Unexpected construction or upgrade completion event"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::Begin()
{
	if (!Identity()) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 SelectionFlags = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(TEXT("-RokAutoSelectBuilding=UnityInitId_5"), ESearchCase::IgnoreCase))
			{ ++SelectionFlags; continue; }
		if (Token.Equals(TEXT("-RokCityGammaLayerCensus"), ESearchCase::IgnoreCase)) continue;
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) continue;
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
	Server = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
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
		&& Server.IsValid() && FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
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
		TEXT("Source/Rok/RokLocalConstructionRuntimeAuditSubsystem.h"),
		TEXT("Source/Rok/RokLocalConstructionRuntimeAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokLocalConstructionRuntime.py"),
		TEXT("Scripts/VerifyRokPlacementMenuRuntime.py"),
		TEXT("Source/Rok/RokCityPlacementWorldFrame.h"),
		TEXT("Source/Rok/RokCityPlacementWorldFrame.cpp"),
		TEXT("Source/Rok/RokPlayerController.h"),
		TEXT("Source/Rok/RokPlayerController.cpp"),
		TEXT("Source/Rok/RokStrategyCameraPawn.h"),
		TEXT("Source/Rok/RokStrategyCameraPawn.cpp"),
		TEXT("Source/Rok/RokCityPlacementPreviewActor.h"),
		TEXT("Source/Rok/RokCityPlacementPreviewActor.cpp"),
		TEXT("Source/Rok/RokCitySpriteActor.h"),
		TEXT("Source/Rok/RokCitySpriteActor.cpp"),
		TEXT("Source/Rok/RokBuildingSelectionIndicator.h"),
		TEXT("Source/Rok/RokBuildingSelectionIndicator.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.h"),
		TEXT("Source/Rok/RokBuildingPanelWidget.h"),
		TEXT("Source/Rok/RokBuildingPanelWidget.cpp"),
		TEXT("Source/Rok/RokUiWindowAnimation.h"),
		TEXT("Source/Rok/RokUiWindowAnimation.cpp"),
		TEXT("Content/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.uasset"),
		TEXT("Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/atlas_manifest.json"),
		TEXT("Saved/RokUnityMigration/PlacementUiAtlas/20261008_221417_249217_45548/ue_import_report.json"),
		TEXT("Content/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.uasset"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_static.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision2.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_hud_ui_revision3.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_close_input_source.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_animation_source_inventory.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_previous_selection_source.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_menu_gray_render_diagnosis.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v2.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_source_v3.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/PlacementGrayMaterial/20261008T230559679259Z_46968_ebba69ea/create.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_gray_material_root_create.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009ConstructionVisual/placement_ui_asset_inventory.json"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"),
		TEXT("Source/Rok/RokLocalConstructionVisualFactory.h"),
		TEXT("Source/Rok/RokLocalConstructionVisualFactory.cpp"),
		TEXT("Source/Rok/RokCityConstructionGrid.h"),
		TEXT("Source/Rok/RokCityConstructionGrid.cpp"),
		TEXT("Source/Rok/RokCityResourceProduction.h"),
		TEXT("Source/Rok/RokCityResourceProduction.cpp"),
		TEXT("Source/Rok/RokCityGammaLayerCensusSubsystem.h"),
		TEXT("Source/Rok/RokCityGammaLayerCensusSubsystem.cpp"),
		TEXT("Source/Rok/RokServerSubsystem.h"),
		TEXT("Source/Rok/RokServerSubsystem.cpp"),
		TEXT("Saved/RokUnityMigration/LocalCitySimulationContext/catalog.json"),
		TEXT("Saved/RokUnityMigration/CityConstructionGridSources/catalog.json"),
		TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"),
		TEXT("Saved/RokUnityMigration/ConfigTables/BuildingResourcesProduce.json")
	};
	for (const TCHAR* Relative : Required)
	{
		TArray<uint8> Bytes;
		if (!Expect(FFileHelper::LoadFileToArray(Bytes, *(RokDataPaths::ResolveProjectPath(Relative)))
			&& !Bytes.IsEmpty(), TEXT("Placement audit input missing"))) return false;
		Inputs->SetStringField(Relative, RokTrainArmyMesh::Sha256(Bytes));
	}
	CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
		this, &URokLocalConstructionRuntimeAuditSubsystem::OnUpgradeCompleted);
	return true;
}

bool URokLocalConstructionRuntimeAuditSubsystem::FreezePlacementBaseline()
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
	Baseline->SetBoolField(TEXT("server_local_only_run"), Server->IsLocalOnlyRun());
	Baseline->SetNumberField(TEXT("server_http_dispatch_attempts"),
		static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Baseline->SetNumberField(TEXT("server_suppressed_local_requests"),
		static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	bBaselineReady = true;
	return Unchanged();
}

bool URokLocalConstructionRuntimeAuditSubsystem::Unchanged()
{
    if (!bBaselineReady) return true;
    if (!Expect(Identity() && Buildings.IsValid() && Runtime.IsValid() && Server.IsValid()
        && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0
        && Server->GetSuppressedLocalRequestCount() == 0
        && Buildings->GetActiveUpgradeCount() == BaselineUpgradeQueues
        && Buildings->GetOwnedBuildQueueCount() == BaselineOwnedQueues
        && Buildings->GetInitialCityBuildingCount() == 26
        && CompletionCount == 1 && Snapshot(Runtime->GetActiveTask()) == TaskBaseline,
        TEXT("Initial city, task or permanent worker queue changed during construction"))) return false;
    auto Profile = Runtime->GetPlayerProfileState();
    const int32 ExpectedDelta = 14 + 5 * LocalCompletionCount;
    if (!Expect(Profile.LocalBuildingUpgradeScore == ExpectedDelta
        && Buildings->GetLocalCompletedBuildingPowerDelta() == ExpectedDelta,
        TEXT("Local construction power did not increase exactly once per completion"))) return false;
    Profile.LocalPreviewScore -= (ExpectedDelta - 14);
    Profile.LocalBuildingUpgradeScore = 14;
    if (!Expect(Snapshot(Profile) == ProfileBaseline,
        TEXT("Account/profile authority changed beyond derived local building power"))) return false;
    TSet<int32> Seen;
    int32 NewSourceCount = 0, SourceCount = 0, FormalCount = 0, NewInitCount = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->IsActorBeingDestroyed()) continue;
        if (HasTagPrefix(*It, TEXT("UnityInitId_")))
        {
            const int32 Id = TaggedInt(*It, TEXT("UnityInitId_"));
            if (!InitialActors.Contains(Id) || InitialActors.FindRef(Id).Get() != *It || Seen.Contains(Id))
                ++NewInitCount;
            else Seen.Add(Id);
        }
        if (It->Tags.Contains(TEXT("RokLocalConstructedBuilding"))) ++FormalCount;
    }
    for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
        if (!It->IsActorBeingDestroyed())
        {
            ++SourceCount;
            if (!BaselineSourceActors.Contains(TWeakObjectPtr<ARokCitySpriteActor>(*It)))
                ++NewSourceCount;
        }
    for (const TWeakObjectPtr<ARokCitySpriteActor>& Actor : BaselineSourceActors)
        if (!Expect(Actor.IsValid(), TEXT("Baseline source actor was removed"))) return false;
    if (!Expect(Seen.Num() == 24 && NewInitCount == 0
        && SourceCount == BaselineSourceActors.Num() + NewSourceCount
        && NewSourceCount <= 2 && FormalCount <= NewSourceCount,
        TEXT("Construction altered initial actor identities or created extra source actors"))) return false;
    for (const auto& Pair : InitialActors)
    {
        if (!Expect(Pair.Value.IsValid() && InitialStates.FindRef(Pair.Key)
            == Snapshot(Buildings->GetBuildingStateForActor(Pair.Value.Get())),
            TEXT("Construction changed an initial building state"))) return false;
    }
    return true;
}

bool URokLocalConstructionRuntimeAuditSubsystem::CheckPreview(int32 TypeId,
    bool bExpectedActive, bool bExpectedDragging, bool bExpectedValid)
{
    if (!Unchanged() || !Controller.IsValid()) return false;
    const auto Value = Controller->GetCityPlacementPreviewSnapshot();
    int32 Ghosts = 0;
    AActor* Ghost = nullptr;
    for (TActorIterator<ARokCityPlacementPreviewActor> It(GetWorld()); It; ++It)
        if (!It->IsActorBeingDestroyed()) { ++Ghosts; Ghost = *It; }
    const UWidget* Controls = Hud.IsValid()
        ? Hud->GetWidgetFromName(TEXT("CityPlacementControls")) : nullptr;
    const UWidget* Confirm = Hud.IsValid()
        ? Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")) : nullptr;
    if (!Expect(Value.bActive == bExpectedActive
        && Value.bDragging == bExpectedDragging
        && Value.bGridValid == (bExpectedActive && bExpectedValid)
        && Value.bCameraInputSuspended == bExpectedDragging
        && Value.TypeId == (bExpectedActive ? TypeId : 0)
        && Ghosts == (bExpectedActive ? 1 : 0)
        && Value.PreviewActor.Get() == Ghost
        && (!Ghost || (Ghost->Tags.Contains(TEXT("RokPlacementPreviewTemporary"))
            && !HasTagPrefix(Ghost, TEXT("UnityInitId_"))
            && !Ghost->Tags.Contains(TEXT("RokLocalConstructedBuilding"))))
        && Controls && Controls->IsVisible() == bExpectedActive
        && Confirm && Confirm->GetIsEnabled() == (bExpectedActive && bExpectedValid && Value.bCanCommit),
        TEXT("Construction preview, ghost, camera or confirm button state differs"))) return false;
    return true;
}

bool URokLocalConstructionRuntimeAuditSubsystem::ProjectGrid(const FIntPoint& Grid,
	FVector2D& OutViewport, FVector2D& OutAbsolute) const
{
	if (!Controller.IsValid() || !WorldFrame.IsValid()) return false;
	const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const FViewport* ActualViewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!ActualViewport) return false;
	FVector World = FVector::ZeroVector;
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

bool URokLocalConstructionRuntimeAuditSubsystem::ProjectHeldGridInterior(const FIntPoint& Grid,
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

	FVector ActualGround = FVector::ZeroVector;
	FString Error;
	if (!WorldFrame.TryScreenToGround(*Controller.Get(), RoundedViewport, ActualGround, Error))
	{
		OutEvidence->SetStringField(TEXT("inverse_error"), Error);
		return false;
	}
	OutEvidence->SetObjectField(TEXT("rounded_pixel_ground_world"), VectorJson(ActualGround));
	FVector2D ActualLocal = FVector2D::ZeroVector;
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

bool URokLocalConstructionRuntimeAuditSubsystem::ProjectSubgrid(
	FVector2D& OutViewport, FVector2D& OutAbsolute) const
{
	if (!Controller.IsValid() || !WorldFrame.IsValid()) return false;
	const UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const FViewport* ActualViewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!ActualViewport) return false;
	FVector2D Center = FVector2D::ZeroVector;
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

bool URokLocalConstructionRuntimeAuditSubsystem::ChooseTargets()
{
    const auto Current = Controller->GetCityPlacementPreviewSnapshot();
    if (!Expect(Current.bActive && Current.TypeId == 2 && Current.Footprint == FIntPoint(5, 5),
        TEXT("Farm ghost is not source five-by-five"))) return false;
    bool bOccupied = false;
    int32 BestDistance = MAX_int32;
    for (int32 Y = -20; Y <= 20; ++Y)
        for (int32 X = -20; X <= 20; ++X)
        {
            const FIntPoint Candidate(X, Y);
			FVector2D Viewport = FVector2D::ZeroVector, Absolute = FVector2D::ZeroVector;
            if (!ProjectGrid(Candidate, Viewport, Absolute)) continue;
            FRokLocalConstructionRequest Request;
            Request.UnityBuildingTypeId = 2;
            Request.Grid = Candidate;
            FRokLocalConstructionPreview Preview;
            if (!Buildings->PreviewLocalConstruction(Request, Preview)
                || !Preview.bContextVerified || !Preview.bPositionEvaluated) continue;
            if (!Preview.bCanCommit && Preview.FailureReason.Contains(TEXT("占用"))
                && !bOccupied)
            {
                OccupiedGrid = Candidate;
                bOccupied = true;
            }
            const int32 Distance = FMath::Abs(X - Current.Grid.X) + FMath::Abs(Y - Current.Grid.Y);
            if (Preview.bCanCommit && Candidate != Current.Grid && Distance < BestDistance)
            {
                ValidGrid = Candidate;
                BestDistance = Distance;
            }
        }
    return Expect(bOccupied && BestDistance < MAX_int32,
        TEXT("No onscreen source-occupied and source-valid Farm target grids"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::MovePointer(const FVector2D& Viewport,
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
		FVector Ground = FVector::ZeroVector;
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

bool URokLocalConstructionRuntimeAuditSubsystem::WorldButton(bool bDown, const TCHAR* Label)
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

TSharedRef<FJsonObject> URokLocalConstructionRuntimeAuditSubsystem::PlacementMenuEvidence() const
{
	auto Evidence = MakeShared<FJsonObject>();
	const URokMainHudWidget* MainHud = Hud.Get();
	const UWidget* Controls = MainHud ? MainHud->GetWidgetFromName(TEXT("CityPlacementControls")) : nullptr;
	const UWidget* Confirm = MainHud ? MainHud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")) : nullptr;
	const UWidget* Cancel = MainHud ? MainHud->GetWidgetFromName(TEXT("CityPlacementCancelButton")) : nullptr;
	const UImage* ConfirmImage = MainHud ? Cast<UImage>(MainHud->GetWidgetFromName(TEXT("CityPlacementConfirmImage"))) : nullptr;
	const UImage* CancelImage = MainHud ? Cast<UImage>(MainHud->GetWidgetFromName(TEXT("CityPlacementCancelImage"))) : nullptr;
	Evidence->SetObjectField(TEXT("controls"), PlacementWidgetGeometry(Controls));
	Evidence->SetObjectField(TEXT("confirm_button"), PlacementWidgetGeometry(Confirm));
	Evidence->SetObjectField(TEXT("cancel_button"), PlacementWidgetGeometry(Cancel));
	Evidence->SetObjectField(TEXT("confirm_image"), PlacementWidgetGeometry(ConfirmImage));
	Evidence->SetObjectField(TEXT("cancel_image"), PlacementWidgetGeometry(CancelImage));
	bool bClientRect = false;
	if (ConfirmImage && Controller.IsValid())
	{
		const FGeometry IconGeometry = ConfirmImage->GetCachedGeometry();
		const FVector2D IconCenter = IconGeometry.LocalToAbsolute(
			IconGeometry.GetLocalSize() * 0.5f);
		const TSharedRef<FJsonObject> Route = InputRouteJson(GetWorld(), Controller.Get(), IconCenter);
		double ClientLeft = 0.0, ClientTop = 0.0, ClientRight = 0.0, ClientBottom = 0.0;
		bClientRect = Route->TryGetNumberField(TEXT("game_client_left"), ClientLeft)
			&& Route->TryGetNumberField(TEXT("game_client_top"), ClientTop)
			&& Route->TryGetNumberField(TEXT("game_client_right"), ClientRight)
			&& Route->TryGetNumberField(TEXT("game_client_bottom"), ClientBottom);
		Evidence->SetBoolField(TEXT("screenshot_client_rect_valid"), bClientRect);
		Evidence->SetObjectField(TEXT("screenshot_client_origin"),
			PointJson(FVector2D(ClientLeft, ClientTop)));
		Evidence->SetObjectField(TEXT("screenshot_client_size"),
			PointJson(FVector2D(ClientRight - ClientLeft, ClientBottom - ClientTop)));
	}
	const auto SnapshotValue = Controller.IsValid()
		? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
	const ARokStrategyCameraPawn* Camera = Controller.IsValid()
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	const double Dxf = Camera ? Camera->GetCurrentCityDxf() : -1.0;
	Evidence->SetNumberField(TEXT("camera_city_dxf"), Dxf);
	Evidence->SetNumberField(TEXT("source_disabled_color_transition_seconds"), 0.1);
	const UCanvasPanelSlot* Slot = Controls ? Cast<UCanvasPanelSlot>(Controls->Slot) : nullptr;
	const FVector2D SlotPosition = Slot ? Slot->GetPosition() : FVector2D::ZeroVector;
	const FVector2D RenderScale = Controls ? Controls->GetRenderTransform().Scale : FVector2D::ZeroVector;
	Evidence->SetObjectField(TEXT("canvas_slot_position"), PointJson(SlotPosition));
	Evidence->SetObjectField(TEXT("render_scale"), PointJson(RenderScale));
	const FRokUiWindowAnimationSample OpenEndSample = FRokUiWindowAnimation::Evaluate(
		ERokUiWindowAnimationStyle::BuildingMenu, false,
		FRokUiWindowAnimation::GetDuration(ERokUiWindowAnimationStyle::BuildingMenu, false));
	const float RootOpacity = Controls ? Controls->GetRenderOpacity() : -1.0f;
	const bool bSourceOpenEndOpacity = Controls
		&& FMath::IsNearlyEqual(RootOpacity, OpenEndSample.Opacity, 0.001f);
	Evidence->SetNumberField(TEXT("root_render_opacity"), RootOpacity);
	Evidence->SetNumberField(TEXT("source_open_end_opacity"), OpenEndSample.Opacity);
	Evidence->SetNumberField(TEXT("source_open_duration_seconds"),
		FRokUiWindowAnimation::GetDuration(ERokUiWindowAnimationStyle::BuildingMenu, false));
	Evidence->SetBoolField(TEXT("source_open_end_opacity_reached"), bSourceOpenEndOpacity);
	bool bSourceOpenClipFinished = false;
	float OpenElapsed = -1.0f;
#if !UE_BUILD_SHIPPING
	if (MainHud)
	{
		OpenElapsed = MainHud->GetPlacementMenuAnimationElapsedSecondsForAudit();
		bSourceOpenClipFinished = !MainHud->IsPlacementMenuAnimatingForAudit()
			&& !MainHud->IsPlacementMenuClosingForAudit()
			&& OpenElapsed >= FRokUiWindowAnimation::GetDuration(
				ERokUiWindowAnimationStyle::BuildingMenu, false) - 0.001f;
	}
#endif
	Evidence->SetNumberField(TEXT("source_open_elapsed_seconds"), OpenElapsed);
	Evidence->SetBoolField(TEXT("source_open_clip_finished"), bSourceOpenClipFinished);
	const FVector2D RootSize = MainHud ? MainHud->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const double ExpectedScale = Dxf > 0 && RootSize.Y > 1 ? (172.0 / Dxf) * (RootSize.Y / 810.0) : 0.0;
	Evidence->SetNumberField(TEXT("expected_source_render_scale"), ExpectedScale);
	const ARokCityPlacementPreviewActor* Ghost = Cast<ARokCityPlacementPreviewActor>(SnapshotValue.PreviewActor.Get());
	const UStaticMeshComponent* Bottom = Ghost ? Ghost->GetBottomMeshComponent() : nullptr;
	const FVector ButtonWorld = Bottom && Ghost
		? Bottom->GetComponentLocation() + Ghost->GetActorTransform().TransformVector(FVector(0, 0, 0.24))
		: FVector::ZeroVector;
	FVector2D Projected = FVector2D::ZeroVector;
	const bool bProjected = Controller.IsValid() && Bottom
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller.Get(), ButtonWorld, Projected, false);
	Evidence->SetBoolField(TEXT("world_button_anchor_projected"), bProjected);
	Evidence->SetObjectField(TEXT("world_button_anchor"), VectorJson(ButtonWorld));
	Evidence->SetObjectField(TEXT("world_button_projected_widget_position"), PointJson(Projected));
	Evidence->SetNumberField(TEXT("projection_slot_error"), bProjected ? FVector2D::Distance(Projected, SlotPosition) : -1.0);
	bool bBrushReady = true;
	const UTexture2D* Atlas = nullptr;
	for (const bool bConfirm : {true, false})
	{
		const UImage* Icon = bConfirm ? ConfirmImage : CancelImage;
		const FSlateBrush* Brush = Icon ? &Icon->GetBrush() : nullptr;
		UObject* Resource = Brush ? Brush->GetResourceObject() : nullptr;
		UMaterialInstanceDynamic* Gray = bConfirm && !SnapshotValue.bCanCommit
			? Cast<UMaterialInstanceDynamic>(Resource) : nullptr;
		const UTexture2D* Texture = bConfirm && !SnapshotValue.bCanCommit
			? (Gray ? Cast<UTexture2D>(Gray->K2_GetTextureParameterValue(TEXT("SourceAtlas"))) : nullptr)
			: Cast<UTexture2D>(Resource);
		if (!bConfirm) Atlas = Texture;
		const FBox2f Uv = Brush ? static_cast<FBox2f>(Brush->GetUVRegion()) : FBox2f();
		const FVector2D ExpectedMin = bConfirm
			? FVector2D(721.0 / 1024.0, 361.0 / 1024.0)
			: FVector2D(1.0 / 1024.0, 501.0 / 1024.0);
		const FVector2D ExpectedMax = bConfirm
			? FVector2D(809.0 / 1024.0, 449.0 / 1024.0)
			: FVector2D(89.0 / 1024.0, 589.0 / 1024.0);
		const bool bUv = Brush && Uv.bIsValid
			&& FVector2D(Uv.Min.X, Uv.Min.Y).Equals(ExpectedMin, 0.00001f)
			&& FVector2D(Uv.Max.X, Uv.Max.Y).Equals(ExpectedMax, 0.00001f);
		const bool bAsset = Texture && Texture->GetPathName()
			== TEXT("/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.rok_placement_ui_hud")
			&& Texture->GetSizeX() == 1024 && Texture->GetSizeY() == 1024;
		const bool bGray = !bConfirm || SnapshotValue.bCanCommit
			|| (Gray && Gray->GetBaseMaterial()
				&& Gray->GetBaseMaterial()->GetPathName()
					== TEXT("/Game/RokPrototype/UI/PlacementAtlases/M_RokPlacementGrayUI.M_RokPlacementGrayUI"));
		const FLinearColor ExpectedTint = bConfirm && !SnapshotValue.bCanCommit
			? FLinearColor(200.0f / 255.0f, 200.0f / 255.0f,
				200.0f / 255.0f, 128.0f / 255.0f)
			: FLinearColor::White;
		const FLinearColor MaterialTint = Gray
			? Gray->K2_GetVectorParameterValue(TEXT("SourceTint")) : FLinearColor::White;
		const bool bSourceEndTint = MaterialTint.Equals(ExpectedTint, 0.002f);
		const FLinearColor ActualColor = Icon ? Icon->GetColorAndOpacity() : FLinearColor::Transparent;
		const bool bSourceEndColor = ActualColor.Equals(FLinearColor::White, 0.002f);
		const FString Prefix = bConfirm ? TEXT("confirm") : TEXT("cancel");
		const FVector2D RootLocal = Controls && Icon
			? Controls->GetCachedGeometry().AbsoluteToLocal(
				Icon->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector))
			: FVector2D::ZeroVector;
		Evidence->SetObjectField(Prefix + TEXT("_root_local_top_left"), PointJson(RootLocal));
		Evidence->SetStringField(Prefix + TEXT("_brush_resource"), Resource ? Resource->GetPathName() : FString());
		Evidence->SetStringField(Prefix + TEXT("_gray_base_material"), Gray && Gray->GetBaseMaterial()
			? Gray->GetBaseMaterial()->GetPathName() : FString());
		Evidence->SetStringField(Prefix + TEXT("_source_atlas"), Texture ? Texture->GetPathName() : FString());
		Evidence->SetObjectField(Prefix + TEXT("_uv_min"), PointJson(Brush && Uv.bIsValid
			? FVector2D(Uv.Min.X, Uv.Min.Y) : FVector2D::ZeroVector));
		Evidence->SetObjectField(Prefix + TEXT("_uv_max"), PointJson(Brush && Uv.bIsValid
			? FVector2D(Uv.Max.X, Uv.Max.Y) : FVector2D::ZeroVector));
		Evidence->SetBoolField(Prefix + TEXT("_uv_matches_source"), bUv);
		Evidence->SetBoolField(Prefix + TEXT("_atlas_ready"), bAsset);
		Evidence->SetBoolField(Prefix + TEXT("_gray_material_ready"), bGray);
		auto ColorJson = MakeShared<FJsonObject>();
		ColorJson->SetNumberField(TEXT("r"), ActualColor.R);
		ColorJson->SetNumberField(TEXT("g"), ActualColor.G);
		ColorJson->SetNumberField(TEXT("b"), ActualColor.B);
		ColorJson->SetNumberField(TEXT("a"), ActualColor.A);
		Evidence->SetObjectField(Prefix + TEXT("_linear_color"), ColorJson);
		auto TintJson = MakeShared<FJsonObject>();
		TintJson->SetNumberField(TEXT("r"), MaterialTint.R);
		TintJson->SetNumberField(TEXT("g"), MaterialTint.G);
		TintJson->SetNumberField(TEXT("b"), MaterialTint.B);
		TintJson->SetNumberField(TEXT("a"), MaterialTint.A);
		Evidence->SetObjectField(Prefix + TEXT("_source_tint_gamma"), TintJson);
		Evidence->SetBoolField(Prefix + TEXT("_source_end_color"), bSourceEndColor);
		Evidence->SetBoolField(Prefix + TEXT("_source_end_tint"), bSourceEndTint);
		bBrushReady &= bUv && bAsset && bGray && bSourceEndColor && bSourceEndTint && Brush
			&& RootLocal.Equals(bConfirm ? FVector2D(279.2, 167.5) : FVector2D(179.2, 167.5), 1.0f)
			&& Brush->ImageSize.Equals(FVector2D(87.0, 87.0), 0.01f);
	}
	const bool bGeometryReady = Controls && Confirm && Cancel && ConfirmImage && CancelImage
		&& bClientRect
		&& bSourceOpenEndOpacity && bSourceOpenClipFinished
		&& Visible(Controls) && Visible(Confirm) && Visible(Cancel)
		&& Visible(ConfirmImage) && Visible(CancelImage)
		&& Controls->GetIsEnabled() && Cancel->GetIsEnabled()
		&& Confirm->GetIsEnabled() == SnapshotValue.bCanCommit
		&& ConfirmImage->GetParent() == Controls && Confirm->GetParent() == Controls
		&& CancelImage->GetParent() == Controls && Cancel->GetParent() == Controls
		&& Controls->GetCachedGeometry().GetLocalSize().Equals(FVector2D(472.7, 220.0), 0.1f)
		&& Confirm->GetCachedGeometry().GetLocalSize().Equals(FVector2D(87, 87), 0.1f)
		&& Cancel->GetCachedGeometry().GetLocalSize().Equals(FVector2D(87, 87), 0.1f)
		&& ConfirmImage->GetCachedGeometry().GetLocalSize().Equals(FVector2D(87, 87), 0.1f)
		&& CancelImage->GetCachedGeometry().GetLocalSize().Equals(FVector2D(87, 87), 0.1f)
		&& FVector2D::Distance(Confirm->GetCachedGeometry().LocalToAbsolute(FVector2D(43.5)),
			ConfirmImage->GetCachedGeometry().LocalToAbsolute(FVector2D(43.5))) < 1.0
		&& FVector2D::Distance(Cancel->GetCachedGeometry().LocalToAbsolute(FVector2D(43.5)),
			CancelImage->GetCachedGeometry().LocalToAbsolute(FVector2D(43.5))) < 1.0;
	const bool bProjectionReady = SnapshotValue.bActive && bProjected && Camera
		&& Dxf > 0 && Dxf <= 270 && FMath::IsFinite(ExpectedScale)
		&& FMath::IsNearlyEqual(RenderScale.X, ExpectedScale, 0.01)
		&& FMath::IsNearlyEqual(RenderScale.Y, ExpectedScale, 0.01)
		&& FVector2D::Distance(Projected, SlotPosition) < 2.0;
	Evidence->SetBoolField(TEXT("geometry_ready"), bGeometryReady);
	Evidence->SetBoolField(TEXT("brush_ready"), bBrushReady && Atlas != nullptr);
	Evidence->SetBoolField(TEXT("projection_ready"), bProjectionReady);
	Evidence->SetBoolField(TEXT("ready"), bGeometryReady && bProjectionReady && bBrushReady && Atlas != nullptr);
	return Evidence;
}

bool URokLocalConstructionRuntimeAuditSubsystem::ObservePlacementMenuReadiness(TSharedRef<FJsonObject> Evidence)
{
	const FString Phase = Pending.IsValid() ? Pending->GetStringField(TEXT("phase_name")) : FString();
	const bool bExpected = Phase == TEXT("02_farm_valid_preview")
		|| Phase == TEXT("02b_farm_invalid_gray_preview")
		|| Phase == TEXT("06_wood_valid_preview");
	Evidence->SetBoolField(TEXT("placement_menu_expected"), bExpected);
	if (!bExpected) return true;
	if (!bPlacementMenuPointerCleared)
	{
		const UWidget* ControlsWidget = Hud.IsValid()
			? Hud->GetWidgetFromName(TEXT("CityPlacementControls")) : nullptr;
		const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
		const FVector2D ViewportSize = Viewport.GetLocalSize();
		if (!Expect(ControlsWidget && ViewportSize.X > 100 && ViewportSize.Y > 100
			&& FSlateApplication::IsInitialized()
			&& !Controller->GetCityPlacementPreviewSnapshot().bDragging,
			TEXT("Placement screenshot cannot clear hover during a drag"))) return false;
		const FGeometry ControlsGeometry = ControlsWidget->GetCachedGeometry();
		const FVector2D Previous = LastAbsolutePointer;
		FVector2D Target = FVector2D::ZeroVector;
		for (const FVector2D& Corner : {FVector2D(16.0f, 16.0f),
			FVector2D(ViewportSize.X - 16.0f, 16.0f),
			FVector2D(16.0f, ViewportSize.Y - 16.0f),
			FVector2D(ViewportSize.X - 16.0f, ViewportSize.Y - 16.0f)})
		{
			const FVector2D Candidate = Viewport.LocalToAbsolute(Corner);
			const FVector2D Local = ControlsGeometry.AbsoluteToLocal(Candidate);
			const FVector2D Size = ControlsGeometry.GetLocalSize();
			if (!Candidate.Equals(Previous, 1.0f)
				&& (Local.X < -8.0f || Local.Y < -8.0f
					|| Local.X > Size.X + 8.0f || Local.Y > Size.Y + 8.0f))
			{
				Target = Candidate;
				break;
			}
		}
		if (!Expect(Target != FVector2D::ZeroVector,
			TEXT("No non-popup client pixel available to clear placement hover"))) return false;
		const auto Before = Controller->GetCityPlacementPreviewSnapshot();
		FSlateApplication& App = FSlateApplication::Get();
		App.SetCursorPos(Target);
		App.ProcessMouseMoveEvent(FPointerEvent(0, Target, Previous,
			TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
		LastAbsolutePointer = Target;
		const auto After = Controller->GetCityPlacementPreviewSnapshot();
		const TSharedRef<FJsonObject> Route = InputRouteJson(GetWorld(), Controller.Get(), Target);
		double ClientLeft = 0.0, ClientTop = 0.0, ClientRight = 0.0, ClientBottom = 0.0;
		const bool bInsideClient = Route->TryGetNumberField(TEXT("game_client_left"), ClientLeft)
			&& Route->TryGetNumberField(TEXT("game_client_top"), ClientTop)
			&& Route->TryGetNumberField(TEXT("game_client_right"), ClientRight)
			&& Route->TryGetNumberField(TEXT("game_client_bottom"), ClientBottom)
			&& Target.X >= ClientLeft && Target.X < ClientRight
			&& Target.Y >= ClientTop && Target.Y < ClientBottom;
		if (!Expect(Route->GetBoolField(TEXT("os_cursor_hits_own_window")) && bInsideClient
			&& Before.PrimaryPressEventCount == After.PrimaryPressEventCount
			&& Before.PlacementDragBeginCount == After.PlacementDragBeginCount
			&& Before.Grid == After.Grid && Before.CurrentSelection == After.CurrentSelection
			&& !After.bDragging,
			TEXT("Placement hover clear escaped game client or changed drag/selection"))) return false;
		auto Action = MakeShared<FJsonObject>();
		Action->SetStringField(TEXT("action"), TEXT("placement_menu_hover_clear"));
		Action->SetStringField(TEXT("phase_name"), Phase);
		Action->SetStringField(TEXT("method"), TEXT("real Slate mouse move to non-popup client pixel; no button press"));
		Action->SetObjectField(TEXT("from_absolute"), PointJson(Previous));
		Action->SetObjectField(TEXT("to_absolute"), PointJson(Target));
		const FVector2D PopupLocal = ControlsGeometry.AbsoluteToLocal(Target);
		const FVector2D PopupSize = ControlsGeometry.GetLocalSize();
		Action->SetBoolField(TEXT("outside_popup_geometry"), PopupLocal.X < -8.0f
			|| PopupLocal.Y < -8.0f || PopupLocal.X > PopupSize.X + 8.0f
			|| PopupLocal.Y > PopupSize.Y + 8.0f);
		Action->SetObjectField(TEXT("route"), Route);
		Action->SetBoolField(TEXT("inside_game_client"), bInsideClient);
		Action->SetNumberField(TEXT("primary_press_before"), Before.PrimaryPressEventCount);
		Action->SetNumberField(TEXT("primary_press_after"), After.PrimaryPressEventCount);
		Action->SetNumberField(TEXT("drag_begin_before"), Before.PlacementDragBeginCount);
		Action->SetNumberField(TEXT("drag_begin_after"), After.PlacementDragBeginCount);
		Action->SetObjectField(TEXT("grid_before"), PointJson(FVector2D(Before.Grid.X, Before.Grid.Y)));
		Action->SetObjectField(TEXT("grid_after"), PointJson(FVector2D(After.Grid.X, After.Grid.Y)));
		Action->SetBoolField(TEXT("selection_unchanged"), Before.CurrentSelection == After.CurrentSelection);
		Action->SetBoolField(TEXT("dragging_after"), After.bDragging);
		Action->SetNumberField(TEXT("sent_frame"), GFrameCounter);
		Actions.Add(MakeShared<FJsonValueObject>(Action));
		bPlacementMenuPointerCleared = true;
		PlacementMenuPointerClearWall = FPlatformTime::Seconds();
	}
	const TSharedRef<FJsonObject> Menu = PlacementMenuEvidence();
	Menu->SetBoolField(TEXT("hover_clear_performed"), bPlacementMenuPointerCleared);
	Menu->SetNumberField(TEXT("hover_clear_settle_seconds"),
		FPlatformTime::Seconds() - PlacementMenuPointerClearWall);
	const auto SnapshotValue = Controller->GetCityPlacementPreviewSnapshot();
	const UWidget* OldMenu = Panel.IsValid()
		? Panel->GetWidgetFromName(TEXT("BuildingMenuRoot")) : nullptr;
	const bool bOldPanelCollapsed = Panel.IsValid()
		&& Panel->GetVisibility() == ESlateVisibility::Collapsed;
	Menu->SetStringField(TEXT("previous_selection_actor"), SnapshotValue.CurrentSelection.IsValid()
		? SnapshotValue.CurrentSelection->GetName() : FString());
	Menu->SetNumberField(TEXT("previous_selected_panel_visibility"),
		Panel.IsValid() ? static_cast<int32>(Panel->GetVisibility()) : -1);
	Menu->SetNumberField(TEXT("previous_selected_panel_root_opacity"),
		OldMenu ? OldMenu->GetRenderOpacity() : -1.0f);
	Menu->SetBoolField(TEXT("previous_selected_panel_collapsed"), bOldPanelCollapsed);
	Menu->SetBoolField(TEXT("ready"), Menu->GetBoolField(TEXT("ready"))
		&& FPlatformTime::Seconds() - PlacementMenuPointerClearWall >= 0.11
		&& !SnapshotValue.CurrentSelection.IsValid() && bOldPanelCollapsed);
	const TSharedPtr<FJsonObject> Controls = Menu->GetObjectField(TEXT("controls"));
	if (!Menu->GetBoolField(TEXT("ready")))
	{
		PlacementMenuStableFrames = 0;
		Menu->SetNumberField(TEXT("stable_geometry_frames"), 0);
		Menu->SetNumberField(TEXT("observed_frame"), GFrameCounter);
		Evidence->SetObjectField(TEXT("placement_menu"), Menu);
		return false;
	}
	const FVector2D Center(Controls->GetObjectField(TEXT("absolute_center"))->GetNumberField(TEXT("x")),
		Controls->GetObjectField(TEXT("absolute_center"))->GetNumberField(TEXT("y")));
	const FVector2D Size(Controls->GetObjectField(TEXT("absolute_size"))->GetNumberField(TEXT("x")),
		Controls->GetObjectField(TEXT("absolute_size"))->GetNumberField(TEXT("y")));
	if (Menu->GetBoolField(TEXT("ready")) && PlacementMenuLastCenter.Equals(Center, 0.1f)
		&& PlacementMenuLastSize.Equals(Size, 0.1f)) ++PlacementMenuStableFrames;
	else PlacementMenuStableFrames = Menu->GetBoolField(TEXT("ready")) ? 1 : 0;
	PlacementMenuLastCenter = Center;
	PlacementMenuLastSize = Size;
	Menu->SetNumberField(TEXT("stable_geometry_frames"), PlacementMenuStableFrames);
	Menu->SetNumberField(TEXT("observed_frame"), GFrameCounter);
	Evidence->SetObjectField(TEXT("placement_menu"), Menu);
	return Menu->GetBoolField(TEXT("ready")) && PlacementMenuStableFrames >= 2;
}

bool URokLocalConstructionRuntimeAuditSubsystem::SlateClick(UWidget* Target, const TCHAR* Label)
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
	const bool bConstructionConfirm = FCString::Strcmp(Label, TEXT("farm_confirm_ordinary_hud")) == 0
		|| FCString::Strcmp(Label, TEXT("wood_confirm_ordinary_hud")) == 0;
	const auto BeforeSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	const UWidget* SelectedMenuBefore = bConstructionConfirm && Panel.IsValid()
		? Panel->GetWidgetFromName(TEXT("BuildingMenuRoot")) : nullptr;
	const int32 PanelVisibilityBefore = bConstructionConfirm && Panel.IsValid()
		? static_cast<int32>(Panel->GetVisibility()) : -1;
	const float PanelOpacityBefore = SelectedMenuBefore
		? SelectedMenuBefore->GetRenderOpacity() : -1.0f;
	const FRokResourceStock WalletBefore = Buildings->GetCurrentResources();
	const int32 TicketsBefore = Buildings->GetActiveLocalConstructionCount();
	const TSharedRef<FJsonObject> BeforeRoute = bConstructionConfirm
		? InputRouteJson(GetWorld(), Controller.Get(), Point) : MakeShared<FJsonObject>();
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const auto DownSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	const TSharedRef<FJsonObject> DownRoute = bConstructionConfirm
		? InputRouteJson(GetWorld(), Controller.Get(), Point) : MakeShared<FJsonObject>();
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Point, Point, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const auto UpSnapshot = Controller->GetCityPlacementPreviewSnapshot();
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
	bool bBuildingMenuVisible = false;
	if (bConstructionConfirm)
	{
		const FRokResourceStock WalletAfter = Buildings->GetCurrentResources();
		const UWidget* BuildingMenu = Panel.IsValid()
			? Panel->GetWidgetFromName(TEXT("BuildingMenuRoot")) : nullptr;
		bBuildingMenuVisible = Panel.IsValid() && Visible(Panel.Get())
			&& BuildingMenu && Visible(BuildingMenu);
		const int32 PanelVisibilityAfter = Panel.IsValid()
			? static_cast<int32>(Panel->GetVisibility()) : -1;
		const bool bOldPanelClosing = bBuildingMenuVisible
			&& Panel->GetVisibility() == ESlateVisibility::HitTestInvisible
			&& BeforeSnapshot.CurrentSelection.IsValid()
			&& !UpSnapshot.CurrentSelection.IsValid();
		Action->SetNumberField(TEXT("selected_panel_visibility_before"), PanelVisibilityBefore);
		Action->SetNumberField(TEXT("selected_panel_visibility_after_up"), PanelVisibilityAfter);
		Action->SetNumberField(TEXT("selected_panel_root_opacity_before"), PanelOpacityBefore);
		Action->SetNumberField(TEXT("selected_panel_root_opacity_after_up"),
			BuildingMenu ? BuildingMenu->GetRenderOpacity() : -1.0f);
		Action->SetBoolField(TEXT("old_selected_panel_closing_after_up"), bOldPanelClosing);
		Action->SetStringField(TEXT("gesture_contract"), TEXT("one real Slate UI press/release; separate repeat tap is another gesture"));
		Action->SetNumberField(TEXT("sent_frame"), GFrameCounter);
		Action->SetNumberField(TEXT("confirm_sent_wall_seconds"), FPlatformTime::Seconds());
		Action->SetObjectField(TEXT("before_route"), BeforeRoute);
		Action->SetObjectField(TEXT("down_route"), DownRoute);
		Action->SetObjectField(TEXT("up_route"), InputRouteJson(GetWorld(), Controller.Get(), Point));
		Action->SetNumberField(TEXT("primary_press_before"), BeforeSnapshot.PrimaryPressEventCount);
		Action->SetNumberField(TEXT("primary_press_after_down"), DownSnapshot.PrimaryPressEventCount);
		Action->SetNumberField(TEXT("primary_press_after_up"), UpSnapshot.PrimaryPressEventCount);
		Action->SetStringField(TEXT("primary_press_outcome_before"), BeforeSnapshot.LastPrimaryPressOutcome.ToString());
		Action->SetStringField(TEXT("primary_press_outcome_after_down"), DownSnapshot.LastPrimaryPressOutcome.ToString());
		Action->SetStringField(TEXT("primary_press_outcome_after_up"), UpSnapshot.LastPrimaryPressOutcome.ToString());
		Action->SetStringField(TEXT("selection_before"), BeforeSnapshot.CurrentSelection.IsValid()
			? BeforeSnapshot.CurrentSelection->GetName() : FString());
		Action->SetStringField(TEXT("selection_after_down"), DownSnapshot.CurrentSelection.IsValid()
			? DownSnapshot.CurrentSelection->GetName() : FString());
		Action->SetStringField(TEXT("selection_after_up"), UpSnapshot.CurrentSelection.IsValid()
			? UpSnapshot.CurrentSelection->GetName() : FString());
		Action->SetBoolField(TEXT("building_menu_visible_after_up"), bBuildingMenuVisible);
		Action->SetNumberField(TEXT("confirm_count_before"), BeforeSnapshot.ConfirmClickCount);
		Action->SetNumberField(TEXT("confirm_count_after_up"), UpSnapshot.ConfirmClickCount);
		Action->SetNumberField(TEXT("active_tickets_before"), TicketsBefore);
		Action->SetNumberField(TEXT("active_tickets_after_up"), Buildings->GetActiveLocalConstructionCount());
		Action->SetNumberField(TEXT("wallet_food_before"), WalletBefore.Food);
		Action->SetNumberField(TEXT("wallet_wood_before"), WalletBefore.Wood);
		Action->SetNumberField(TEXT("wallet_food_after_up"), WalletAfter.Food);
		Action->SetNumberField(TEXT("wallet_wood_after_up"), WalletAfter.Wood);
		const UWidget* PlacementControls = Hud.IsValid()
			? Hud->GetWidgetFromName(TEXT("CityPlacementControls")) : nullptr;
		const UCanvasPanelSlot* PlacementSlot = PlacementControls
			? Cast<UCanvasPanelSlot>(PlacementControls->Slot) : nullptr;
		Action->SetObjectField(TEXT("placement_menu_last_anchor_after_up"), PointJson(
			PlacementSlot ? PlacementSlot->GetPosition() : FVector2D::ZeroVector));
		Action->SetObjectField(TEXT("placement_menu_render_scale_after_up"), PointJson(
			PlacementControls ? PlacementControls->GetRenderTransform().Scale : FVector2D::ZeroVector));
		Action->SetNumberField(TEXT("placement_menu_root_opacity_after_up"),
			PlacementControls ? PlacementControls->GetRenderOpacity() : -1.0f);
		FirstConfirmAction = Action;
	}
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Down && Up && (!bConstructionConfirm
		|| (BeforeRoute->GetBoolField(TEXT("hit_path_contains_umg"))
			&& DownSnapshot.PrimaryPressEventCount == BeforeSnapshot.PrimaryPressEventCount
			&& UpSnapshot.PrimaryPressEventCount == BeforeSnapshot.PrimaryPressEventCount
			&& DownSnapshot.CurrentSelection == BeforeSnapshot.CurrentSelection
			&& !UpSnapshot.CurrentSelection.IsValid()
			&& UpSnapshot.ConfirmClickCount == BeforeSnapshot.ConfirmClickCount + 1
			&& Buildings->GetActiveLocalConstructionCount() == TicketsBefore + 1
			&& (!bBuildingMenuVisible || Action->GetBoolField(TEXT("old_selected_panel_closing_after_up"))))),
		TEXT("UMG confirm gesture was unhandled or selected the world during the same press/release"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::ReadyForClick(
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

bool URokLocalConstructionRuntimeAuditSubsystem::SlateUiOriginNoDrag(UWidget* Target)
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

bool URokLocalConstructionRuntimeAuditSubsystem::PressEscape()
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

TSharedRef<FJsonObject> URokLocalConstructionRuntimeAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("step"), Step);
	Json->SetNumberField(TEXT("frame"), GFrameCounter);
	Json->SetNumberField(TEXT("elapsed_seconds"), Elapsed);
	Json->SetNumberField(TEXT("world_time_seconds"), GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0);
	const ARokStrategyCameraPawn* StrategyCamera = Controller.IsValid()
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	Json->SetNumberField(TEXT("camera_city_dxf"), StrategyCamera ? StrategyCamera->GetCurrentCityDxf() : -1.0);
	Json->SetNumberField(TEXT("farm_zoom_wheel_event_count"), ZoomWheelEventCount);
	Json->SetNumberField(TEXT("farm_zoom_completed_dxf"), ZoomCompletedDxf);
	Json->SetBoolField(TEXT("server_local_only_run"), Server.IsValid()
		&& Server->IsLocalOnlyRun());
	Json->SetNumberField(TEXT("server_http_dispatch_attempts"), Server.IsValid()
		? static_cast<double>(Server->GetHttpDispatchAttemptCount()) : -1.0);
	Json->SetNumberField(TEXT("server_suppressed_local_requests"), Server.IsValid()
		? static_cast<double>(Server->GetSuppressedLocalRequestCount()) : -1.0);
	Json->SetNumberField(TEXT("hall_completion_events"), CompletionCount);
	Json->SetNumberField(TEXT("local_completion_events"), LocalCompletionCount);
	if (FarmCompletionProduction.IsValid())
		Json->SetObjectField(TEXT("farm_first_completion_production"),
			FarmCompletionProduction.ToSharedRef());
	if (WoodCompletionProduction.IsValid())
		Json->SetObjectField(TEXT("wood_first_completion_production"),
			WoodCompletionProduction.ToSharedRef());
	Json->SetNumberField(TEXT("farm_ticket_id"), static_cast<double>(FarmId));
	Json->SetNumberField(TEXT("wood_ticket_id"), static_cast<double>(WoodId));
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
		Json->SetBoolField(TEXT("can_commit"), SnapshotValue.bCanCommit);
		Json->SetNumberField(TEXT("confirm_click_count"), static_cast<double>(SnapshotValue.ConfirmClickCount));
		Json->SetNumberField(TEXT("last_confirm_click_frame"), static_cast<double>(SnapshotValue.LastConfirmClickFrame));
		Json->SetStringField(TEXT("last_confirm_outcome"), SnapshotValue.LastConfirmOutcome.ToString());
		Json->SetStringField(TEXT("last_confirm_failure"), SnapshotValue.LastConfirmFailure);
		Json->SetNumberField(TEXT("last_confirmed_local_id"),
			static_cast<double>(SnapshotValue.LastConfirmedLocalConstructionId));
		Json->SetBoolField(TEXT("last_confirmed_actor_valid"), SnapshotValue.LastConfirmedActor.IsValid());
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
		Json->SetBoolField(TEXT("selection_is_farm"), FarmId > 0 && Buildings.IsValid()
			&& SnapshotValue.CurrentSelection.Get() == Buildings->GetLocalConstructionActor(FarmId));
		Json->SetBoolField(TEXT("selection_is_wood"), WoodId > 0 && Buildings.IsValid()
			&& SnapshotValue.CurrentSelection.Get() == Buildings->GetLocalConstructionActor(WoodId));
		Json->SetBoolField(TEXT("selection_is_none"), !SnapshotValue.CurrentSelection.IsValid());
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
			FVector ActorOrigin = FVector::ZeroVector, ActorExtent = FVector::ZeroVector;
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
			FVector CenterWorld = FVector::ZeroVector;
			const bool bCenter = WorldFrame.GridCenterToWorld(
				{SnapshotValue.Grid, SnapshotValue.Footprint}, CenterWorld, Error);
			Projection->SetBoolField(TEXT("grid_center_world_valid"), bCenter);
			Projection->SetStringField(TEXT("grid_center_error"), Error);
			Projection->SetObjectField(TEXT("grid_center_world"), VectorJson(CenterWorld));
			FVector2D CenterScreen = FVector2D::ZeroVector;
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
			FVector2D UnconstrainedScreen = FVector2D::ZeroVector;
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
			FVector InverseGround = FVector::ZeroVector;
			const FVector2D AttemptMouse = SnapshotValue.LastPlacementPressFrame > 0
				? SnapshotValue.LastPlacementPressMouse : FVector2D::ZeroVector;
			const bool bGround = SnapshotValue.LastPlacementPressFrame > 0
				&& WorldFrame.TryScreenToGround(*Controller, AttemptMouse, InverseGround, Error);
			Projection->SetBoolField(TEXT("last_press_inverse_ground_valid"), bGround);
			Projection->SetStringField(TEXT("last_press_inverse_ground_error"), Error);
			Projection->SetObjectField(TEXT("last_press_inverse_ground"), VectorJson(InverseGround));
			FVector2D InverseLocal = FVector2D::ZeroVector, SourceCenter = FVector2D::ZeroVector;
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
		Json->SetNumberField(TEXT("profile_local_power"), Runtime.IsValid()
			? Runtime->GetPlayerProfileState().LocalBuildingUpgradeScore : -1);
		for (const uint64 Id : {FarmId, WoodId})
		{
			if (Id == 0) continue;
			FRokLocalConstructionTicket Ticket;
			const bool bTicket = Buildings->GetLocalConstructionTicket(Id, Ticket);
			const FString Key = Id == FarmId ? TEXT("farm_ticket") : TEXT("wood_ticket");
			auto Item = MakeShared<FJsonObject>();
			Item->SetBoolField(TEXT("exists"), bTicket);
			if (bTicket)
			{
				Item->SetNumberField(TEXT("id"), static_cast<double>(Ticket.Id));
				Item->SetNumberField(TEXT("type_id"), Ticket.TypeId);
				Item->SetObjectField(TEXT("grid"), GridJson(Ticket.Footprint.Origin));
				Item->SetObjectField(TEXT("footprint"), GridJson(Ticket.Footprint.Size));
				Item->SetNumberField(TEXT("cost_food"), Ticket.Cost.Food);
				Item->SetNumberField(TEXT("cost_wood"), Ticket.Cost.Wood);
				Item->SetNumberField(TEXT("power"), Ticket.Power);
				Item->SetNumberField(TEXT("started_at_world_seconds"), Ticket.StartedAt);
				Item->SetNumberField(TEXT("finishes_at_world_seconds"), Ticket.FinishesAt);
				Item->SetBoolField(TEXT("completed"), Ticket.bCompleted);
			}
			Item->SetBoolField(TEXT("pending"), Buildings->IsLocalConstructionPending(Id));
			ARokCitySpriteActor* Actor = Buildings->GetLocalConstructionActor(Id);
			Item->SetBoolField(TEXT("actor_valid"), IsValid(Actor));
			Item->SetBoolField(TEXT("actor_hidden"), Actor ? Actor->IsHidden() : true);
			if (Actor)
			{
				Item->SetObjectField(TEXT("actor_transform"), TransformJson(Actor->GetActorTransform()));
				Item->SetObjectField(TEXT("actor_grid"), GridJson(Actor->GetGameplayTile()));
				Item->SetObjectField(TEXT("actor_footprint"), GridJson(Actor->GetFootprintSize()));
				Item->SetBoolField(TEXT("formal_tag"), Actor->Tags.Contains(TEXT("RokLocalConstructedBuilding")));
				Item->SetBoolField(TEXT("pending_tag"), Actor->Tags.Contains(TEXT("RokLocalConstructionPending")));
				Item->SetBoolField(TEXT("unity_init_tag"), HasTagPrefix(Actor, TEXT("UnityInitId_")));
			}
			FString Failure;
			Item->SetBoolField(TEXT("completion_failure"),
				Buildings->GetLocalConstructionCompletionFailure(Id, Failure));
			Item->SetStringField(TEXT("completion_failure_reason"), Failure);
			Json->SetObjectField(Key, Item);
		}
		FRokLocalConstructionTicket FirstTicket;
		Json->SetBoolField(TEXT("first_formal_ticket_exists"),
			Buildings->GetLocalConstructionTicket(1, FirstTicket));
		Json->SetStringField(TEXT("profile_sha256"), Runtime.IsValid()
			? RokTrainArmyMesh::Sha256(Snapshot(Runtime->GetPlayerProfileState())) : FString());
		if (Runtime.IsValid())
		{
			auto Profile = Runtime->GetPlayerProfileState();
			const int32 ExtraLocalPower = Buildings->GetLocalCompletedBuildingPowerDelta() - 14;
			Profile.LocalPreviewScore -= ExtraLocalPower;
			Profile.LocalBuildingUpgradeScore = 14;
			Json->SetStringField(TEXT("profile_authority_normalized_sha256"),
				RokTrainArmyMesh::Sha256(Snapshot(Profile)));
		}
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
		if (Controller.IsValid() && Controller->GetCityPlacementPreviewSnapshot().bActive)
			Json->SetObjectField(TEXT("placement_menu"), PlacementMenuEvidence());
	}
	return Json;
}

bool URokLocalConstructionRuntimeAuditSubsystem::CheckProduction(uint64 Id,
	const TCHAR* Label, TSharedPtr<FJsonObject>& OutSample)
{
	FRokLocalConstructionTicket Ticket;
	FRokCityResourceProductionResult Result;
	FString Error;
	const double WorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	const bool bTicket = Id > 0 && Buildings.IsValid()
		&& Buildings->GetLocalConstructionTicket(Id, Ticket);
	const bool bQuery = bTicket && Ticket.bCompleted
		&& Buildings->QueryLocalProduction(Id, Result, Error);
	const int64 ClockStart = bTicket
		? static_cast<int64>(FMath::CeilToDouble(Ticket.FinishesAt)) : 0;
	const int64 WholeElapsed = bTicket
		? FMath::Max<int64>(0, static_cast<int64>(FMath::FloorToDouble(WorldSeconds)) - ClockStart) : 0;
	const double Expected = FMath::Min(4000.0, double(WholeElapsed) * 400.0 / 3600.0);
	auto Sample = MakeShared<FJsonObject>();
	Sample->SetStringField(TEXT("action"), Label);
	Sample->SetStringField(TEXT("method"), TEXT("read-only QueryLocalProduction at actual world time"));
	Sample->SetNumberField(TEXT("id"), static_cast<double>(Id));
	Sample->SetBoolField(TEXT("ticket_exists"), bTicket);
	Sample->SetBoolField(TEXT("query_success"), bQuery);
	Sample->SetStringField(TEXT("query_error"), Error);
	Sample->SetNumberField(TEXT("world_seconds"), WorldSeconds);
	Sample->SetNumberField(TEXT("ticket_finishes_at"), bTicket ? Ticket.FinishesAt : 0.0);
	Sample->SetNumberField(TEXT("clock_start_ceil_seconds"), ClockStart);
	Sample->SetNumberField(TEXT("source_whole_elapsed_seconds"), WholeElapsed);
	Sample->SetNumberField(TEXT("source_rate_per_hour"), 400);
	Sample->SetNumberField(TEXT("source_gather_min"), 66);
	Sample->SetNumberField(TEXT("expected_accrued_amount"), Expected);
	Sample->SetNumberField(TEXT("accrued_amount"), Result.AccruedAmount);
	Sample->SetNumberField(TEXT("accrued_floor"), Result.Accrued);
	Sample->SetBoolField(TEXT("can_collect"), Result.bCanCollect);
	Sample->SetBoolField(TEXT("at_capacity"), Result.bAtCapacity);
	Sample->SetNumberField(TEXT("observed_frame"), GFrameCounter);
	const bool bFirstCompletion = FString(Label).Contains(TEXT("first_completion"));
	Sample->SetBoolField(TEXT("first_completion_event_sample"), bFirstCompletion);
	OutSample = Sample;
	Actions.Add(MakeShared<FJsonValueObject>(Sample));
	return Expect(bQuery && Error.IsEmpty() && FMath::IsFinite(WorldSeconds)
		&& WorldSeconds >= Ticket.FinishesAt
		&& FMath::IsNearlyEqual(Result.AccruedAmount, Expected, 0.00001)
		&& Result.Accrued == static_cast<int64>(FMath::FloorToDouble(Expected))
		&& Result.bCanCollect == (Expected >= 66.0)
		&& Result.bAtCapacity == (Expected >= 4000.0)
		&& (!bFirstCompletion || (!Result.bCanCollect && Expected < 66.0)),
		TEXT("Read-only source production query failed at actual completion clock"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::CheckSelectionFeedback(
	ARokCitySpriteActor* Actor, const TCHAR* Label, bool bExpectArrows)
{
	int32 IndicatorCount = 0;
	int32 VisibleArrowCount = 0;
	bool bIndicatorVisible = false;
	TArray<TSharedPtr<FJsonValue>> ArrowNames;
	TArray<TSharedPtr<FJsonValue>> ArrowComponents;
	FString IndicatorName;
	FVector IndicatorLocation = FVector::ZeroVector;
	for (TActorIterator<ARokBuildingSelectionIndicator> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		++IndicatorCount;
		bIndicatorVisible = !It->IsHidden();
		IndicatorName = It->GetName();
		IndicatorLocation = It->GetActorLocation();
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents<UStaticMeshComponent>(Components);
		for (const UStaticMeshComponent* Component : Components)
		{
			auto ComponentState = MakeShared<FJsonObject>();
			ComponentState->SetStringField(TEXT("name"), Component ? Component->GetName() : FString());
			ComponentState->SetBoolField(TEXT("registered"), Component && Component->IsRegistered());
			ComponentState->SetBoolField(TEXT("component_visible"), Component && Component->IsVisible());
			ComponentState->SetBoolField(TEXT("actor_hidden"), It->IsHidden());
			ComponentState->SetStringField(TEXT("mesh"), Component && Component->GetStaticMesh()
				? Component->GetStaticMesh()->GetPathName() : FString());
			ComponentState->SetStringField(TEXT("material"), Component && Component->GetMaterial(0)
				? Component->GetMaterial(0)->GetPathName() : FString());
			ComponentState->SetNumberField(TEXT("last_render_time_on_screen"), Component
				? Component->GetLastRenderTimeOnScreen() : 0.0);
			ArrowComponents.Add(MakeShared<FJsonValueObject>(ComponentState));
			if (Component && Component->IsRegistered() && Component->IsVisible()
				&& Component->GetStaticMesh() && Component->GetMaterial(0)
				&& !It->IsHidden())
			{
				++VisibleArrowCount;
				ArrowNames.Add(MakeShared<FJsonValueString>(Component->GetName()));
			}
		}
	}
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), Label);
	Action->SetStringField(TEXT("method"), TEXT("read-only selected source actor and indicator components"));
	Action->SetNumberField(TEXT("observed_frame"), GFrameCounter);
	Action->SetNumberField(TEXT("last_primary_press_frame"), Controller.IsValid()
		? static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().LastPrimaryPressFrame) : -1.0);
	Action->SetStringField(TEXT("source_actor"), Actor ? Actor->GetPathName() : FString());
	Action->SetNumberField(TEXT("source_building_type"), Actor ? Actor->GetUnityBuildingType() : -1);
	Action->SetNumberField(TEXT("source_unity_lod_scale"), Actor ? Actor->GetUnityLodScale() : -1.0);
	Action->SetObjectField(TEXT("source_actor_scale"), VectorJson(Actor
		? Actor->GetActorScale3D() : FVector::ZeroVector));
	Action->SetObjectField(TEXT("source_mesh_relative_scale"), VectorJson(Actor
		&& Actor->GetSpriteMeshComponent()
		? Actor->GetSpriteMeshComponent()->GetRelativeScale3D() : FVector::ZeroVector));
	Action->SetBoolField(TEXT("source_actor_hidden"), Actor ? Actor->IsHidden() : true);
	const ARokStrategyCameraPawn* Camera = Controller.IsValid()
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	Action->SetNumberField(TEXT("camera_city_dxf"), Camera ? Camera->GetCurrentCityDxf() : -1.0);
	Action->SetNumberField(TEXT("expected_visible_arrow_count"), bExpectArrows ? 4 : 0);
	Action->SetBoolField(TEXT("actor_can_show_move_arrows"), Actor && Actor->CanShowMoveArrows());
	Action->SetNumberField(TEXT("indicator_actor_count"), IndicatorCount);
	Action->SetStringField(TEXT("indicator_actor_name"), IndicatorName);
	Action->SetObjectField(TEXT("indicator_world_location"), VectorJson(IndicatorLocation));
	Action->SetBoolField(TEXT("indicator_visible"), bIndicatorVisible);
	Action->SetNumberField(TEXT("visible_arrow_component_count"), VisibleArrowCount);
	Action->SetArrayField(TEXT("arrow_components"), ArrowNames);
	Action->SetArrayField(TEXT("arrow_component_state"), ArrowComponents);
	if (!bExpectArrows) Action->SetObjectField(TEXT("full_source_state"), State());
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	const bool bExpected = bExpectArrows
		? Actor && Actor->CanShowMoveArrows() && IndicatorCount == 1
			&& bIndicatorVisible && VisibleArrowCount == 4
		: Actor && !Actor->CanShowMoveArrows() && IndicatorCount == 1
			&& !bIndicatorVisible && VisibleArrowCount == 0
			&& Camera && Camera->GetCurrentCityDxf() > 250.0f;
	return Expect(bExpected, bExpectArrows
		? TEXT("Selected building did not show four source move arrows after true viewport zoom")
		: TEXT("Source zoomed-out selected building unexpectedly showed move arrows"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::SlateZoomInAtWorldPointer()
{
	ARokStrategyCameraPawn* Camera = Controller.IsValid()
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	if (!Expect(Camera && FSlateApplication::IsInitialized()
		&& !Controller->GetCityPlacementPreviewSnapshot().bCameraInputSuspended,
		TEXT("Source camera cannot receive ordinary viewport wheel input"))) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Point = LastAbsolutePointer;
	App.SetCursorPos(Point);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Point, Point, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		Point, App.GetInteractiveTopLevelWindows(), false, 0);
	bool bViewport = false, bUmG = false;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		bViewport |= Type.Contains(TEXT("SViewport"));
		bUmG |= Type.Contains(TEXT("SObjectWidget"));
	}
	const TSharedPtr<FSlateUser> User = App.GetUser(0);
	const bool bCaptured = User.IsValid() && User->GetCursorCaptor().IsValid();
	if (!Expect(Path.IsValid() && bViewport && !bUmG && !bCaptured,
		TEXT("Camera wheel did not target the uncaptured GameApp SViewport"))) return false;
	ZoomBeforeDxf = Camera->GetCurrentCityDxf();
	PendingZoomAction = MakeShared<FJsonObject>();
	PendingZoomAction->SetStringField(TEXT("action"), TEXT("farm_source_viewport_wheel_up"));
	PendingZoomAction->SetStringField(TEXT("method"), TEXT("SlateApplication::OnMouseWheel positive delta to actual SViewport"));
	PendingZoomAction->SetNumberField(TEXT("sent_frame"), GFrameCounter);
	PendingZoomAction->SetNumberField(TEXT("wheel_delta"), 1.0);
	PendingZoomAction->SetNumberField(TEXT("absolute_x"), Point.X);
	PendingZoomAction->SetNumberField(TEXT("absolute_y"), Point.Y);
	PendingZoomAction->SetNumberField(TEXT("dxf_before"), ZoomBeforeDxf);
	PendingZoomAction->SetBoolField(TEXT("viewport_hit_path"), bViewport);
	PendingZoomAction->SetBoolField(TEXT("umg_hit_path"), bUmG);
	PendingZoomAction->SetBoolField(TEXT("slate_mouse_captured"), bCaptured);
	const bool bHandled = App.OnMouseWheel(1.0f, Point);
	PendingZoomAction->SetBoolField(TEXT("slate_wheel_handled"), bHandled);
	Actions.Add(MakeShared<FJsonValueObject>(PendingZoomAction));
	ZoomWheelSentFrame = GFrameCounter;
	++ZoomWheelEventCount;
	return Expect(bHandled && ZoomWheelEventCount <= 12,
		TEXT("Source viewport wheel was not handled within bounded zoom attempts"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::CheckConstruction(uint64 Id,
	int32 TypeId, bool bCompleted, int32 Food, int32 Wood, int32 ActiveCount)
{
	FRokLocalConstructionTicket Ticket;
	FString CompletionFailure;
	ARokCitySpriteActor* Actor = Buildings->GetLocalConstructionActor(Id);
	const FRokResourceStock Wallet = Buildings->GetCurrentResources();
	const bool bPending = Buildings->IsLocalConstructionPending(Id);
	const bool bFailure = Buildings->GetLocalConstructionCompletionFailure(Id, CompletionFailure);
	const FIntPoint ExpectedGrid = TypeId == 2 ? FarmGrid : WoodGrid;
	if (!Expect(Id > 0 && Buildings->GetLocalConstructionTicket(Id, Ticket)
		&& Ticket.Id == Id && Ticket.TypeId == TypeId && Ticket.Civilization == 101
		&& Ticket.Age == 1 && Ticket.Footprint.Origin == ExpectedGrid
		&& Ticket.Footprint.Size == FIntPoint(5, 5)
		&& Ticket.Power == 5 && Ticket.bCompleted == bCompleted
		&& FMath::IsNearlyEqual(Ticket.FinishesAt - Ticket.StartedAt, 2.0, 0.001)
		&& Ticket.Cost.Food == (TypeId == 3 ? 100 : 0)
		&& Ticket.Cost.Wood == (TypeId == 2 ? 100 : 0)
		&& Wallet.Food == Food && Wallet.Wood == Wood
		&& Wallet.Stone == 1500 && Wallet.Gold == 600
		&& Buildings->GetActiveLocalConstructionCount() == ActiveCount
		&& !bFailure && CompletionFailure.IsEmpty()
		&& Actor && Actor->GetWorld() == GetWorld()
		&& Actor->GetGameplayTile() == ExpectedGrid
		&& Actor->GetFootprintSize() == FIntPoint(5, 5)
		&& !Actor->IsHidden()
		&& !HasTagPrefix(Actor, TEXT("UnityInitId_"))
		&& Actor->Tags.Contains(FName(*FString::Printf(TEXT("UnityBuildingType_%d"), TypeId)))
		&& Actor->Tags.Contains(FName(*FString::Printf(TEXT("UnityGridX_%d"), ExpectedGrid.X)))
		&& Actor->Tags.Contains(FName(*FString::Printf(TEXT("UnityGridY_%d"), ExpectedGrid.Y)))
		&& Actor->Tags.Contains(FName(*FString::Printf(TEXT("RokLocalBuildingId_%llu"),
			static_cast<unsigned long long>(Id))))
		&& Actor->Tags.Contains(TEXT("RokLocalConstructionPending")) != bCompleted
		&& Actor->Tags.Contains(TEXT("RokLocalConstructedBuilding")) == bCompleted
		&& (!bCompleted || Actor->Tags.Contains(FName(*FString::Printf(
			TEXT("RokLocalBuildingId_%llu"), static_cast<unsigned long long>(Id)))))
		&& bPending != bCompleted,
		TEXT("Construction ticket, source fee/time, actor or pending transition differs"))) return false;
	if (!Expect(CheckConstructionBoundary(WoodId > 0 ? 2 : 1, ActiveCount),
		TEXT("Construction source actor set changed unexpectedly"))) return false;
	if (bCompleted)
	{
		TSharedPtr<FJsonObject> Query;
		if (!CheckProduction(Id, TypeId == 2
			? TEXT("farm_completed_production_requery")
			: TEXT("wood_completed_production_requery"), Query)) return false;
	}
	return true;
}

bool URokLocalConstructionRuntimeAuditSubsystem::CheckConstructionBoundary(
	int32 ExpectedNewActors, int32 ExpectedActiveCount)
{
	int32 NewSource = 0, Formal = 0, Ghosts = 0;
	for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		if (!BaselineSourceActors.Contains(TWeakObjectPtr<ARokCitySpriteActor>(*It)))
		{
			++NewSource;
			if (It->Tags.Contains(TEXT("RokLocalConstructedBuilding"))) ++Formal;
		}
	}
	for (TActorIterator<ARokCityPlacementPreviewActor> It(GetWorld()); It; ++It)
		if (!It->IsActorBeingDestroyed()) ++Ghosts;
	return Expect(NewSource == ExpectedNewActors
		&& Formal == LocalCompletionCount
		&& Ghosts == 0
		&& Buildings->GetActiveLocalConstructionCount() == ExpectedActiveCount,
		TEXT("Formal actor, ghost or active-ticket count differs"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::ProjectConstructedActor(
	ARokCitySpriteActor* Actor, FVector2D& OutViewport, FVector2D& OutAbsolute) const
{
	const UGameViewportClient* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const FViewport* Viewport = Client ? Client->Viewport : nullptr;
	FVector CenterWorld = FVector::ZeroVector;
	FString Error;
	if (!Actor || !Viewport || !Controller.IsValid() || !WorldFrame.IsValid()
		|| !WorldFrame.GridCenterToWorld(
			{Actor->GetGameplayTile(), Actor->GetFootprintSize()}, CenterWorld, Error)
		|| !Controller->ProjectWorldLocationToScreen(CenterWorld, OutViewport, false)) return false;
	OutViewport -= FVector2D(Viewport->GetInitialPositionXY());
	const FVector2D Pixels = UWidgetLayoutLibrary::GetViewportSize(this);
	const FGeometry Geometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	if (Pixels.X <= 0 || Pixels.Y <= 0 || OutViewport.X < 4 || OutViewport.Y < 4
		|| OutViewport.X >= Pixels.X - 4 || OutViewport.Y >= Pixels.Y - 4) return false;
	OutAbsolute = Geometry.LocalToAbsolute(FVector2D(
		OutViewport.X * Geometry.GetLocalSize().X / Pixels.X,
		OutViewport.Y * Geometry.GetLocalSize().Y / Pixels.Y));
	return true;
}

bool URokLocalConstructionRuntimeAuditSubsystem::DisabledConfirmClick()
{
	UWidget* Target = Hud.IsValid()
		? Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")) : nullptr;
	if (!Expect(Target && Visible(Target) && !Target->GetIsEnabled(),
		TEXT("Invalid source grid did not disable confirmation"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("Disabled confirm has no real Slate geometry"))) return false;
	const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
	FSlateApplication& App = FSlateApplication::Get();
	ConfirmCountBefore = Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount;
	const FRokResourceStock WalletBefore = Buildings->GetCurrentResources();
	App.SetCursorPos(Point);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Point, LastAbsolutePointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	LastAbsolutePointer = Point;
	const auto BeforeSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	const ARokStrategyCameraPawn* Camera = Cast<ARokStrategyCameraPawn>(Controller->GetPawn());
	const double CameraDxfBefore = Camera ? Camera->GetCurrentCityDxf() : -1.0;
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		Point, App.GetInteractiveTopLevelWindows(), false, 0);
	if (!Expect(Path.IsValid() && Path.ContainsWidget(&Hud->TakeWidget().Get()),
		TEXT("Invalid confirm click did not hit the real HUD"))) return false;
	const TSharedRef<FJsonObject> BeforeRoute = InputRouteJson(GetWorld(), Controller.Get(), Point);
	if (!Expect(BeforeRoute->GetBoolField(TEXT("os_cursor_hits_own_window")),
		TEXT("Disabled confirm pointer escaped the own game window"))) return false;
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Point, Point, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const auto DownSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Point, Point, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const auto UpSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), TEXT("invalid_disabled_confirm_pointer"));
	Action->SetStringField(TEXT("method"), TEXT("Slate Move+Down+Up on disabled HUD confirm"));
	Action->SetObjectField(TEXT("before_route"), BeforeRoute);
	Action->SetNumberField(TEXT("primary_press_before"), BeforeSnapshot.PrimaryPressEventCount);
	Action->SetNumberField(TEXT("primary_press_after_down"), DownSnapshot.PrimaryPressEventCount);
	Action->SetNumberField(TEXT("primary_press_after_up"), UpSnapshot.PrimaryPressEventCount);
	Action->SetStringField(TEXT("primary_press_outcome_before"), BeforeSnapshot.LastPrimaryPressOutcome.ToString());
	Action->SetStringField(TEXT("primary_press_outcome_after_down"), DownSnapshot.LastPrimaryPressOutcome.ToString());
	Action->SetStringField(TEXT("primary_press_outcome_after_up"), UpSnapshot.LastPrimaryPressOutcome.ToString());
	Action->SetNumberField(TEXT("drag_begin_before"), BeforeSnapshot.PlacementDragBeginCount);
	Action->SetNumberField(TEXT("drag_begin_after_down"), DownSnapshot.PlacementDragBeginCount);
	Action->SetNumberField(TEXT("drag_begin_after_up"), UpSnapshot.PlacementDragBeginCount);
	Action->SetStringField(TEXT("selection_before"), BeforeSnapshot.CurrentSelection.IsValid()
		? BeforeSnapshot.CurrentSelection->GetName() : FString());
	Action->SetStringField(TEXT("selection_after_down"), DownSnapshot.CurrentSelection.IsValid()
		? DownSnapshot.CurrentSelection->GetName() : FString());
	Action->SetStringField(TEXT("selection_after_up"), UpSnapshot.CurrentSelection.IsValid()
		? UpSnapshot.CurrentSelection->GetName() : FString());
	Action->SetObjectField(TEXT("grid_before"), PointJson(FVector2D(BeforeSnapshot.Grid.X, BeforeSnapshot.Grid.Y)));
	Action->SetObjectField(TEXT("grid_after_down"), PointJson(FVector2D(DownSnapshot.Grid.X, DownSnapshot.Grid.Y)));
	Action->SetObjectField(TEXT("grid_after_up"), PointJson(FVector2D(UpSnapshot.Grid.X, UpSnapshot.Grid.Y)));
	Action->SetBoolField(TEXT("dragging_before"), BeforeSnapshot.bDragging);
	Action->SetBoolField(TEXT("dragging_after_down"), DownSnapshot.bDragging);
	Action->SetBoolField(TEXT("dragging_after_up"), UpSnapshot.bDragging);
	Action->SetNumberField(TEXT("camera_dxf_before"), CameraDxfBefore);
	Action->SetNumberField(TEXT("camera_dxf_after_up"), Camera ? Camera->GetCurrentCityDxf() : -1.0);
	Action->SetNumberField(TEXT("confirm_count_before"), ConfirmCountBefore);
	Action->SetNumberField(TEXT("confirm_count_after_immediate"),
		static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount));
	Action->SetNumberField(TEXT("wallet_food_before"), WalletBefore.Food);
	Action->SetNumberField(TEXT("wallet_wood_before"), WalletBefore.Wood);
	Action->SetNumberField(TEXT("active_tickets_before"),
		Buildings->GetActiveLocalConstructionCount());
	const FRokResourceStock WalletAfter = Buildings->GetCurrentResources();
	Action->SetNumberField(TEXT("wallet_food_after_immediate"), WalletAfter.Food);
	Action->SetNumberField(TEXT("wallet_wood_after_immediate"), WalletAfter.Wood);
	Action->SetNumberField(TEXT("active_tickets_after_immediate"),
		Buildings->GetActiveLocalConstructionCount());
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetNumberField(TEXT("sent_frame"), GFrameCounter);
	InvalidDisabledConfirmAction = Action;
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	const auto ValidPrimaryStep = [](const FRokCityPlacementRuntimeSnapshot& Previous,
		const FRokCityPlacementRuntimeSnapshot& Current)
	{
		return (Current.PrimaryPressEventCount == Previous.PrimaryPressEventCount
			&& Current.LastPrimaryPressOutcome == Previous.LastPrimaryPressOutcome)
			|| (Current.PrimaryPressEventCount == Previous.PrimaryPressEventCount + 1
				&& Current.LastPrimaryPressOutcome == FName(TEXT("ui_owned")));
	};
	return Expect(Down && Up && BeforeSnapshot.bActive && !BeforeSnapshot.bDragging
		&& ValidPrimaryStep(BeforeSnapshot, DownSnapshot)
		&& ValidPrimaryStep(DownSnapshot, UpSnapshot)
		&& UpSnapshot.PrimaryPressEventCount <= BeforeSnapshot.PrimaryPressEventCount + 1
		&& BeforeSnapshot.PlacementDragBeginCount == DownSnapshot.PlacementDragBeginCount
		&& DownSnapshot.PlacementDragBeginCount == UpSnapshot.PlacementDragBeginCount
		&& BeforeSnapshot.CurrentSelection == DownSnapshot.CurrentSelection
		&& DownSnapshot.CurrentSelection == UpSnapshot.CurrentSelection
		&& BeforeSnapshot.Grid == DownSnapshot.Grid && DownSnapshot.Grid == UpSnapshot.Grid
		&& !DownSnapshot.bDragging && !UpSnapshot.bDragging
		&& Camera && FMath::IsNearlyEqual(CameraDxfBefore, Camera->GetCurrentCityDxf(), 0.001)
		&& Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount
		== ConfirmCountBefore && WalletAfter.Food == WalletBefore.Food
		&& WalletAfter.Wood == WalletBefore.Wood
		&& Buildings->GetActiveLocalConstructionCount() == 0,
		TEXT("Disabled confirmation dispatched placement, selected world or changed transaction"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::RepeatConfirmPointer(const TCHAR* Label)
{
	if (!Expect(FSlateApplication::IsInitialized()
		&& LastConfirmAbsolutePointer != FVector2D::ZeroVector,
		TEXT("Repeated confirm pointer has no original HUD position"))) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const uint64 CountBefore = Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount;
	const FRokResourceStock WalletBefore = Buildings->GetCurrentResources();
	const int32 TicketsBefore = Buildings->GetActiveLocalConstructionCount();
	App.SetCursorPos(LastConfirmAbsolutePointer);
	App.ProcessMouseMoveEvent(FPointerEvent(0, LastConfirmAbsolutePointer,
		LastAbsolutePointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastAbsolutePointer = LastConfirmAbsolutePointer;
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		LastConfirmAbsolutePointer, App.GetInteractiveTopLevelWindows(), false, 0);
	UWidget* Confirm = Hud.IsValid()
		? Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")) : nullptr;
	UWidget* Controls = Hud.IsValid()
		? Hud->GetWidgetFromName(TEXT("CityPlacementControls")) : nullptr;
	const bool bConfirmStillHittable = Confirm && Path.IsValid()
		&& Path.ContainsWidget(&Confirm->TakeWidget().Get()) && Confirm->GetIsEnabled();
	bool bClosing = false;
	float CloseElapsed = -1.0f;
#if !UE_BUILD_SHIPPING
	if (Hud.IsValid())
	{
		bClosing = Hud->IsPlacementMenuClosingForAudit();
		CloseElapsed = Hud->GetPlacementMenuAnimationElapsedSecondsForAudit();
	}
#endif
	const UCanvasPanelSlot* ControlsSlot = Controls ? Cast<UCanvasPanelSlot>(Controls->Slot) : nullptr;
	const FVector2D LastAnchor = ControlsSlot ? ControlsSlot->GetPosition() : FVector2D::ZeroVector;
	const FVector2D LastScale = Controls ? Controls->GetRenderTransform().Scale : FVector2D::ZeroVector;
	const float CloseOpacity = Controls ? Controls->GetRenderOpacity() : -1.0f;
	const bool bOldSelectedPanelClosedBeforeRepeat = Panel.IsValid()
		&& Panel->GetVisibility() == ESlateVisibility::Collapsed;
	if (!bClosing && !Expect(bOldSelectedPanelClosedBeforeRepeat
		&& FirstConfirmAction.IsValid()
		&& ObserveSelectedPanelCloseEnd(
			FirstConfirmAction->GetNumberField(TEXT("confirm_sent_wall_seconds"))),
		TEXT("Independent post-Close tap began before old selected menu finished"))) return false;
	if (bClosing)
	{
		const TSharedPtr<FJsonObject> ConfirmAnchor = FirstConfirmAction.IsValid()
			? FirstConfirmAction->GetObjectField(TEXT("placement_menu_last_anchor_after_up")) : nullptr;
		const TSharedPtr<FJsonObject> ConfirmScale = FirstConfirmAction.IsValid()
			? FirstConfirmAction->GetObjectField(TEXT("placement_menu_render_scale_after_up")) : nullptr;
		if (!Expect(ConfirmAnchor.IsValid() && ConfirmScale.IsValid()
			&& LastAnchor.Equals(FVector2D(ConfirmAnchor->GetNumberField(TEXT("x")),
				ConfirmAnchor->GetNumberField(TEXT("y"))), 0.1f)
			&& LastScale.Equals(FVector2D(ConfirmScale->GetNumberField(TEXT("x")),
				ConfirmScale->GetNumberField(TEXT("y"))), 0.01f),
			TEXT("Closing placement menu changed its last world anchor or scale"))) return false;
	}
	if (!Expect(Path.IsValid() && (bClosing ? bConfirmStillHittable : !bConfirmStillHittable),
		TEXT("Close-phase repeat lost enabled confirm hit or post-Close confirm remained hittable"))) return false;
	const TSharedRef<FJsonObject> BeforeRoute = InputRouteJson(GetWorld(), Controller.Get(), LastConfirmAbsolutePointer);
	const auto BeforeSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, LastConfirmAbsolutePointer, LastConfirmAbsolutePointer,
			Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const TSharedRef<FJsonObject> DownRoute = InputRouteJson(GetWorld(), Controller.Get(), LastConfirmAbsolutePointer);
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0,
		LastConfirmAbsolutePointer, LastConfirmAbsolutePointer, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState()));
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("action"), Label);
	Action->SetStringField(TEXT("method"), TEXT("second Slate tap at original confirm pixel during source Close or after collapse"));
	Action->SetStringField(TEXT("gesture_contract"), TEXT("separate later tap; closing confirm must be inert; after collapse world selection is allowed"));
	Action->SetObjectField(TEXT("before_route"), BeforeRoute);
	Action->SetObjectField(TEXT("down_route"), DownRoute);
	Action->SetObjectField(TEXT("up_route"), InputRouteJson(GetWorld(), Controller.Get(), LastConfirmAbsolutePointer));
	Action->SetStringField(TEXT("selection_before"), BeforeSnapshot.CurrentSelection.IsValid()
		? BeforeSnapshot.CurrentSelection->GetName() : FString());
	const auto AfterSnapshot = Controller->GetCityPlacementPreviewSnapshot();
	Action->SetStringField(TEXT("selection_after_up"), AfterSnapshot.CurrentSelection.IsValid()
		? AfterSnapshot.CurrentSelection->GetName() : FString());
	Action->SetNumberField(TEXT("placement_menu_root_opacity_after_up"),
		Controls ? Controls->GetRenderOpacity() : -1.0f);
	Action->SetObjectField(TEXT("placement_menu_last_anchor_after_up"), PointJson(
		ControlsSlot ? ControlsSlot->GetPosition() : FVector2D::ZeroVector));
	Action->SetNumberField(TEXT("primary_press_before"), BeforeSnapshot.PrimaryPressEventCount);
	Action->SetNumberField(TEXT("primary_press_after_up"), AfterSnapshot.PrimaryPressEventCount);
	Action->SetBoolField(TEXT("confirm_enabled_in_hit_path"), bConfirmStillHittable);
	Action->SetBoolField(TEXT("placement_menu_closing_before"), bClosing);
	Action->SetBoolField(TEXT("old_selected_panel_closed_before_repeat"), bOldSelectedPanelClosedBeforeRepeat);
	Action->SetNumberField(TEXT("placement_menu_close_elapsed_before"), CloseElapsed);
	Action->SetNumberField(TEXT("placement_menu_root_opacity_before"), CloseOpacity);
	Action->SetObjectField(TEXT("placement_menu_last_anchor_before"), PointJson(LastAnchor));
	Action->SetObjectField(TEXT("placement_menu_render_scale_before"), PointJson(LastScale));
	Action->SetNumberField(TEXT("confirm_count_before"), static_cast<double>(CountBefore));
	Action->SetNumberField(TEXT("confirm_count_after_immediate"),
		static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount));
	Action->SetNumberField(TEXT("wallet_food_before"), WalletBefore.Food);
	Action->SetNumberField(TEXT("wallet_wood_before"), WalletBefore.Wood);
	const FRokResourceStock WalletAfter = Buildings->GetCurrentResources();
	Action->SetNumberField(TEXT("wallet_food_after_immediate"), WalletAfter.Food);
	Action->SetNumberField(TEXT("wallet_wood_after_immediate"), WalletAfter.Wood);
	Action->SetNumberField(TEXT("active_tickets_before"), TicketsBefore);
	Action->SetNumberField(TEXT("active_tickets_after_immediate"), Buildings->GetActiveLocalConstructionCount());
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetNumberField(TEXT("sent_frame"), GFrameCounter);
	RepeatConfirmAction = Action;
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount
		== CountBefore && WalletAfter.Food == WalletBefore.Food
		&& WalletAfter.Wood == WalletBefore.Wood
		&& Buildings->GetActiveLocalConstructionCount() == TicketsBefore
		&& (!bClosing
			|| (AfterSnapshot.PrimaryPressEventCount == BeforeSnapshot.PrimaryPressEventCount
				&& AfterSnapshot.CurrentSelection == BeforeSnapshot.CurrentSelection)),
		TEXT("Separate repeated tap changed confirmation, wallet or ticket"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::ObserveFirstConfirmNextFrame(const TCHAR* ExpectedAction)
{
	if (!Expect(FirstConfirmAction.IsValid()
		&& FirstConfirmAction->GetStringField(TEXT("action")) == ExpectedAction
		&& GFrameCounter > FirstConfirmAction->GetNumberField(TEXT("sent_frame")),
		TEXT("First confirm has no following game-frame readback"))) return false;
	const auto SnapshotValue = Controller->GetCityPlacementPreviewSnapshot();
	const FRokResourceStock Wallet = Buildings->GetCurrentResources();
	FirstConfirmAction->SetNumberField(TEXT("observed_after_frame"), GFrameCounter);
	FirstConfirmAction->SetNumberField(TEXT("primary_press_after_next_game_frame"), SnapshotValue.PrimaryPressEventCount);
	FirstConfirmAction->SetStringField(TEXT("selection_after_next_game_frame"),
		SnapshotValue.CurrentSelection.IsValid() ? SnapshotValue.CurrentSelection->GetName() : FString());
	FirstConfirmAction->SetNumberField(TEXT("confirm_count_after_next_game_frame"), SnapshotValue.ConfirmClickCount);
	FirstConfirmAction->SetNumberField(TEXT("wallet_food_after_next_game_frame"), Wallet.Food);
	FirstConfirmAction->SetNumberField(TEXT("wallet_wood_after_next_game_frame"), Wallet.Wood);
	FirstConfirmAction->SetNumberField(TEXT("active_tickets_after_next_game_frame"),
		Buildings->GetActiveLocalConstructionCount());
	const UWidget* SelectedMenu = Panel.IsValid()
		? Panel->GetWidgetFromName(TEXT("BuildingMenuRoot")) : nullptr;
	const bool bOldMenuStillVisible = Panel.IsValid() && Visible(Panel.Get())
		&& SelectedMenu && Visible(SelectedMenu);
	FirstConfirmAction->SetBoolField(TEXT("selected_panel_visible_after_next_game_frame"), bOldMenuStillVisible);
	FirstConfirmAction->SetNumberField(TEXT("selected_panel_visibility_after_next_game_frame"),
		Panel.IsValid() ? static_cast<int32>(Panel->GetVisibility()) : -1);
	FirstConfirmAction->SetNumberField(TEXT("selected_panel_root_opacity_after_next_game_frame"),
		SelectedMenu ? SelectedMenu->GetRenderOpacity() : -1.0f);
	return Expect(SnapshotValue.PrimaryPressEventCount
			== FirstConfirmAction->GetNumberField(TEXT("primary_press_before"))
		&& !SnapshotValue.CurrentSelection.IsValid()
		&& SnapshotValue.ConfirmClickCount
			== FirstConfirmAction->GetNumberField(TEXT("confirm_count_after_up"))
		&& Wallet.Food == FirstConfirmAction->GetNumberField(TEXT("wallet_food_after_up"))
		&& Wallet.Wood == FirstConfirmAction->GetNumberField(TEXT("wallet_wood_after_up"))
		&& Buildings->GetActiveLocalConstructionCount()
			== FirstConfirmAction->GetNumberField(TEXT("active_tickets_after_up"))
		&& (!bOldMenuStillVisible || (Panel.IsValid()
			&& Panel->GetVisibility() == ESlateVisibility::HitTestInvisible
			&& FirstConfirmAction->GetBoolField(TEXT("old_selected_panel_closing_after_up")))),
		TEXT("First confirm gesture leaked a deferred world press or changed wallet/ticket next frame"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::ObserveRepeatConfirmNextFrame(const TCHAR* ExpectedAction)
{
	if (!Expect(RepeatConfirmAction.IsValid()
		&& RepeatConfirmAction->GetStringField(TEXT("action")) == ExpectedAction
		&& GFrameCounter > RepeatConfirmAction->GetNumberField(TEXT("sent_frame")),
		TEXT("Repeated confirm tap has no following game-frame readback"))) return false;
	const auto SnapshotValue = Controller->GetCityPlacementPreviewSnapshot();
	const FRokResourceStock Wallet = Buildings->GetCurrentResources();
	RepeatConfirmAction->SetNumberField(TEXT("observed_after_frame"), GFrameCounter);
	RepeatConfirmAction->SetNumberField(TEXT("primary_press_after_next_game_frame"), SnapshotValue.PrimaryPressEventCount);
	RepeatConfirmAction->SetStringField(TEXT("selection_after_next_game_frame"),
		SnapshotValue.CurrentSelection.IsValid() ? SnapshotValue.CurrentSelection->GetName() : FString());
	RepeatConfirmAction->SetNumberField(TEXT("confirm_count_after_next_game_frame"), SnapshotValue.ConfirmClickCount);
	RepeatConfirmAction->SetNumberField(TEXT("wallet_food_after_next_game_frame"), Wallet.Food);
	RepeatConfirmAction->SetNumberField(TEXT("wallet_wood_after_next_game_frame"), Wallet.Wood);
	RepeatConfirmAction->SetNumberField(TEXT("active_tickets_after_next_game_frame"),
		Buildings->GetActiveLocalConstructionCount());
	const bool bClosingHit = RepeatConfirmAction->GetBoolField(TEXT("placement_menu_closing_before"));
	return Expect(SnapshotValue.ConfirmClickCount
			== RepeatConfirmAction->GetNumberField(TEXT("confirm_count_before"))
		&& Wallet.Food == RepeatConfirmAction->GetNumberField(TEXT("wallet_food_before"))
		&& Wallet.Wood == RepeatConfirmAction->GetNumberField(TEXT("wallet_wood_before"))
		&& Buildings->GetActiveLocalConstructionCount()
			== RepeatConfirmAction->GetNumberField(TEXT("active_tickets_before"))
		&& (!bClosingHit || (SnapshotValue.PrimaryPressEventCount
			== RepeatConfirmAction->GetNumberField(TEXT("primary_press_before"))
			&& !SnapshotValue.CurrentSelection.IsValid())),
		TEXT("Repeated Close-phase tap reached construction or leaked a deferred world press"));
}

bool URokLocalConstructionRuntimeAuditSubsystem::ObserveSelectedPanelCloseEnd(double ConfirmClickedWall)
{
	if (FirstConfirmAction.IsValid()
		&& FirstConfirmAction->HasField(TEXT("selected_panel_close_end_frame"))) return true;
	if (!Expect(FirstConfirmAction.IsValid() && Panel.IsValid()
		&& !Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.IsValid(),
		TEXT("Selected building menu reopened or a new world selection appeared after confirm"))) return false;
	const ESlateVisibility Visibility = Panel->GetVisibility();
	if (Visibility == ESlateVisibility::HitTestInvisible)
	{
		if (FPlatformTime::Seconds() - ConfirmClickedWall > 1.0)
			Expect(false, TEXT("Old selected building menu did not finish source 0.25-second Close"));
		return false;
	}
	if (!Expect(Visibility == ESlateVisibility::Collapsed,
		TEXT("New selected building menu appeared before old Close finished"))) return false;
	const UWidget* Menu = Panel->GetWidgetFromName(TEXT("BuildingMenuRoot"));
	const double ElapsedWall = FPlatformTime::Seconds() - ConfirmClickedWall;
	FirstConfirmAction->SetNumberField(TEXT("selected_panel_close_end_frame"), GFrameCounter);
	FirstConfirmAction->SetNumberField(TEXT("selected_panel_close_end_wall_elapsed_seconds"), ElapsedWall);
	FirstConfirmAction->SetNumberField(TEXT("selected_panel_visibility_after_close"),
		static_cast<int32>(Visibility));
	FirstConfirmAction->SetNumberField(TEXT("selected_panel_root_opacity_after_close"),
		Menu ? Menu->GetRenderOpacity() : -1.0f);
	FirstConfirmAction->SetBoolField(TEXT("selected_panel_visible_after_close"),
		Visible(Panel.Get()) && Menu && Visible(Menu));
	return Expect(!FirstConfirmAction->GetBoolField(TEXT("selected_panel_visible_after_close"))
		&& ElapsedWall <= 1.0
		&& (!FirstConfirmAction->GetBoolField(TEXT("old_selected_panel_closing_after_up"))
			|| ElapsedWall >= 0.20),
		TEXT("Selected building menu remained visible or closed before source animation"));
}

void URokLocalConstructionRuntimeAuditSubsystem::Capture(const TCHAR* Label, int32 NextStep)
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
	bSelectionArrowAssetsPrepared = false;
	SelectionPanelLastObservedFrame = 0;
	SelectionPanelFirstStableFrame = 0;
	SelectionPanelStableFrames = 0;
	SelectionMenuCenter = FVector2D::ZeroVector;
	SelectionMenuSize = FVector2D::ZeroVector;
	SelectionTitleCenter = FVector2D::ZeroVector;
	SelectionTitleSize = FVector2D::ZeroVector;
	PlacementMenuLastCenter = FVector2D::ZeroVector;
	PlacementMenuLastSize = FVector2D::ZeroVector;
	PlacementMenuStableFrames = 0;
	bPlacementMenuPointerCleared = false;
	PlacementMenuPointerClearWall = 0.0;
	RenderReadyFrames = 0;
	PostScreenshotReadyFrames = 0;
	LastRenderReadyFrame = 0;
	LastObservedBuildingRenderTime = 0.0;
	LastObservedBottomRenderTime = 0.0;
	LastObservedConstructionRenderTime = 0.0;
	SelectionArrowReadyObservedWorldSeconds = GetWorld()->GetTimeSeconds();
	LastObservedSelectionArrowRenderTime = 0.0;
}

ARokCitySpriteActor* URokLocalConstructionRuntimeAuditSubsystem::GetCaptureConstructionActor() const
{
	if (!Pending.IsValid() || !Buildings.IsValid()) return nullptr;
	const FString Phase = Pending->GetStringField(TEXT("phase_name"));
	if (Phase.StartsWith(TEXT("03_")) || Phase.StartsWith(TEXT("04_"))
		|| Phase.StartsWith(TEXT("05_")) || Phase.StartsWith(TEXT("06_")))
		return FarmId ? Buildings->GetLocalConstructionActor(FarmId) : nullptr;
	if (Phase.StartsWith(TEXT("07_")) || Phase.StartsWith(TEXT("08_"))
		|| Phase.StartsWith(TEXT("09_")))
		return WoodId ? Buildings->GetLocalConstructionActor(WoodId) : nullptr;
	return nullptr;
}

bool URokLocalConstructionRuntimeAuditSubsystem::PrepareReferencedConstructionAssets(
	ARokCitySpriteActor* Actor, FString& Error)
{
	Error.Reset();
	if (!Actor) return true;
	if (PreparedConstructionActor.Get() == Actor) return true;
	UStaticMeshComponent* Component = Actor->GetSpriteMeshComponent();
	UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
	UMaterialInterface* Material = Component ? Component->GetMaterial(0) : nullptr;
	if (!Mesh || !Material)
	{
		Error = TEXT("Constructed source actor is missing its referenced mesh or material");
		return false;
	}
	TArray<UObject*> ReferencedAssets;
	ReferencedAssets.Add(Mesh);
	ReferencedAssets.Add(Material);
	Material->EnsureIsComplete();
	TArray<UTexture*> Textures;
	Material->GetUsedTextures(Textures);
	if (Textures.IsEmpty())
	{
		Error = TEXT("Constructed source material has no referenced textures");
		return false;
	}
	for (UTexture* Texture : Textures)
	{
		if (!Texture)
		{
			Error = TEXT("Constructed source material has a missing texture");
			return false;
		}
		ReferencedAssets.AddUnique(Texture);
	}
#if WITH_EDITOR
	if (!IsInGameThread())
	{
		Error = TEXT("Targeted constructed asset compilation requires the game thread");
		return false;
	}
	FAssetCompilingManager::Get().FinishCompilationForObjects(ReferencedAssets);
#endif
	PreparedConstructionActor = Actor;
	ConstructionReadyObservedWorldSeconds = GetWorld()->GetTimeSeconds();
	LastObservedConstructionRenderTime = 0.0;
	RenderReadyFrames = 0;
	return true;
}

bool URokLocalConstructionRuntimeAuditSubsystem::PrepareReferencedSelectionArrowAssets(
	FString& Error)
{
	Error.Reset();
	if (!Pending.IsValid() || bSelectionArrowAssetsPrepared) return true;
	const FString Phase = Pending->GetStringField(TEXT("phase_name"));
	if (Phase != TEXT("05_farm_world_selected")
		&& Phase != TEXT("09_wood_world_selected")) return true;
	if (!IsInGameThread())
	{
		Error = TEXT("Selection arrow asset preparation requires the game thread");
		return false;
	}
	struct FArrowReference
	{
		FString Name;
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* Material = nullptr;
		TArray<UTexture*> Textures;
	};
	TArray<FArrowReference> References;
	TArray<UObject*> ReferencedAssets;
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("scope"), TEXT("four selected indicator component meshes, materials and their used textures only"));
	Report->SetNumberField(TEXT("started_frame"), GFrameCounter);
	Report->SetNumberField(TEXT("started_world_seconds"), GetWorld()->GetTimeSeconds());
	const double StartedWallSeconds = FPlatformTime::Seconds();
	int32 IndicatorCount = 0;
	for (TActorIterator<ARokBuildingSelectionIndicator> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		++IndicatorCount;
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents<UStaticMeshComponent>(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			FArrowReference& Reference = References.AddDefaulted_GetRef();
			Reference.Name = Component ? Component->GetName() : FString();
			Reference.Mesh = Component ? Component->GetStaticMesh() : nullptr;
			Reference.Material = Component ? Component->GetMaterial(0) : nullptr;
			if (!Reference.Mesh || !Reference.Material)
			{
				Error = TEXT("Selected source arrow lacks its actual mesh or material");
				break;
			}
			ReferencedAssets.AddUnique(Reference.Mesh);
			ReferencedAssets.AddUnique(Reference.Material);
			Reference.Material->GetUsedTextures(Reference.Textures);
			for (UTexture* Texture : Reference.Textures)
			{
				if (!Texture)
				{
					Error = TEXT("Selected source arrow material has a missing used texture");
					break;
				}
				ReferencedAssets.AddUnique(Texture);
			}
			if (!Error.IsEmpty()) break;
		}
		if (!Error.IsEmpty()) break;
	}
	Report->SetNumberField(TEXT("indicator_actor_count"), IndicatorCount);
	Report->SetNumberField(TEXT("arrow_component_count"), References.Num());
	Report->SetNumberField(TEXT("unique_referenced_asset_count"), ReferencedAssets.Num());
	auto Snapshot = [&References]()
	{
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FArrowReference& Reference : References)
		{
			auto Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("name"), Reference.Name);
			Item->SetStringField(TEXT("mesh"), Reference.Mesh
				? Reference.Mesh->GetPathName() : FString());
			Item->SetBoolField(TEXT("mesh_compiling"), Reference.Mesh
				&& Reference.Mesh->IsCompiling());
			Item->SetBoolField(TEXT("mesh_render_data_ready"), Reference.Mesh
				&& Reference.Mesh->GetRenderData() != nullptr);
			Item->SetStringField(TEXT("material"), Reference.Material
				? Reference.Material->GetPathName() : FString());
			Item->SetBoolField(TEXT("material_complete"), Reference.Material
				&& Reference.Material->IsComplete());
			TArray<TSharedPtr<FJsonValue>> TextureItems;
			for (UTexture* Texture : Reference.Textures)
			{
				auto TextureItem = MakeShared<FJsonObject>();
				TextureItem->SetStringField(TEXT("asset"), Texture
					? Texture->GetPathName() : FString());
				TextureItem->SetBoolField(TEXT("compiling"), Texture
					&& Texture->IsCompiling());
				TextureItem->SetBoolField(TEXT("resource_ready"), Texture
					&& Texture->GetResource() != nullptr);
				TextureItems.Add(MakeShared<FJsonValueObject>(TextureItem));
			}
			Item->SetArrayField(TEXT("used_textures"), TextureItems);
			Items.Add(MakeShared<FJsonValueObject>(Item));
		}
		return Items;
	};
	Report->SetArrayField(TEXT("before"), Snapshot());
	if (Error.IsEmpty() && (IndicatorCount != 1 || References.Num() != 4))
		Error = TEXT("Expected exactly one selected indicator with four source arrow components");
	if (!Error.IsEmpty())
	{
		Report->SetStringField(TEXT("error"), Error);
		Pending->SetObjectField(TEXT("selection_arrow_asset_preparation"), Report);
		return false;
	}
	for (const FArrowReference& Reference : References)
		Reference.Material->EnsureIsComplete();
#if WITH_EDITOR
	FAssetCompilingManager::Get().FinishCompilationForObjects(ReferencedAssets);
#endif
	Report->SetArrayField(TEXT("after"), Snapshot());
	Report->SetBoolField(TEXT("targeted_compile_finished"), true);
	Report->SetNumberField(TEXT("finished_frame"), GFrameCounter);
	Report->SetNumberField(TEXT("finished_world_seconds"), GetWorld()->GetTimeSeconds());
	Report->SetNumberField(TEXT("preparation_wall_seconds"),
		FPlatformTime::Seconds() - StartedWallSeconds);
	Pending->SetObjectField(TEXT("selection_arrow_asset_preparation"), Report);
	SelectionArrowReadyObservedWorldSeconds = GetWorld()->GetTimeSeconds();
	bSelectionArrowAssetsPrepared = true;
	return true;
}

bool URokLocalConstructionRuntimeAuditSubsystem::PrepareReferencedPreviewAssets(
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

bool URokLocalConstructionRuntimeAuditSubsystem::IsRenderReadyForCapture(
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
	ARokCitySpriteActor* ConstructionActor = GetCaptureConstructionActor();
	const FString Phase = Pending.IsValid() ? Pending->GetStringField(TEXT("phase_name")) : FString();
	const bool bConstructionExpected = Phase.StartsWith(TEXT("03_"))
		|| Phase.StartsWith(TEXT("04_")) || Phase.StartsWith(TEXT("05_"))
		|| Phase.StartsWith(TEXT("06_")) || Phase.StartsWith(TEXT("07_"))
		|| Phase.StartsWith(TEXT("08_")) || Phase.StartsWith(TEXT("09_"));
	Evidence->SetBoolField(TEXT("construction_actor_expected"), bConstructionExpected);
	Evidence->SetBoolField(TEXT("construction_actor_present"), ConstructionActor != nullptr);
	bool bReady = Remaining == 0 && (!bConstructionExpected || ConstructionActor != nullptr);
	if (ConstructionActor)
	{
		auto Item = MakeShared<FJsonObject>();
		UStaticMeshComponent* Component = ConstructionActor->GetSpriteMeshComponent();
		UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		UMaterialInterface* Material = Component ? Component->GetMaterial(0) : nullptr;
		const bool bMeshReady = Mesh && !Mesh->IsCompiling()
			&& Mesh->GetRenderData() && Mesh->GetNumLODs() > 0;
		const bool bMaterialReady = Material && Material->IsComplete();
		const bool bComponentReady = Component && Component->IsRegistered()
			&& Component->IsVisible() && Component->IsRenderStateCreated()
			&& !ConstructionActor->IsHidden();
		const double LastOnScreen = Component
			? Component->GetLastRenderTimeOnScreen() : 0.0;
		const bool bRenderedAfterReady = LastOnScreen > ConstructionReadyObservedWorldSeconds
			&& GetWorld()->GetTimeSeconds() - LastOnScreen < 0.25;
		Item->SetStringField(TEXT("actor"), ConstructionActor->GetPathName());
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
		Evidence->SetObjectField(TEXT("construction_actor"), Item);
		bReady &= PreparedConstructionActor.Get() == ConstructionActor
			&& bMeshReady && bMaterialReady && bComponentReady
			&& bTexturesReady && bRenderedAfterReady;
	}
	const bool bSelectedCapture = Phase == TEXT("05_farm_world_selected")
		|| Phase == TEXT("09_wood_world_selected");
	const bool bFarSelectedCapture = Phase == TEXT("05_farm_zoomed_out_selected");
	Evidence->SetBoolField(TEXT("selection_arrows_expected"), bSelectedCapture);
	Evidence->SetBoolField(TEXT("selection_no_arrows_expected"), bFarSelectedCapture);
	if (bFarSelectedCapture)
	{
		int32 IndicatorCount = 0;
		bool bIndicatorHidden = false;
		for (TActorIterator<ARokBuildingSelectionIndicator> It(GetWorld()); It; ++It)
		{
			if (It->IsActorBeingDestroyed()) continue;
			++IndicatorCount;
			bIndicatorHidden = It->IsHidden();
		}
		const ARokStrategyCameraPawn* Camera = Controller.IsValid()
			? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
		Evidence->SetNumberField(TEXT("selection_indicator_count"), IndicatorCount);
		Evidence->SetBoolField(TEXT("selection_indicator_hidden"), bIndicatorHidden);
		Evidence->SetNumberField(TEXT("selection_camera_city_dxf"),
			Camera ? Camera->GetCurrentCityDxf() : -1.0);
		bReady &= IndicatorCount == 1 && bIndicatorHidden
			&& Camera && Camera->GetCurrentCityDxf() > 250.0f
			&& ConstructionActor && !ConstructionActor->CanShowMoveArrows()
			&& Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get()
				== ConstructionActor;
	}
	if (bSelectedCapture)
	{
		int32 IndicatorCount = 0;
		int32 RenderedArrowCount = 0;
		double EarliestArrowRenderTime = TNumericLimits<double>::Max();
		TArray<TSharedPtr<FJsonValue>> ArrowEvidence;
		for (TActorIterator<ARokBuildingSelectionIndicator> It(GetWorld()); It; ++It)
		{
			if (It->IsActorBeingDestroyed()) continue;
			++IndicatorCount;
			TArray<UStaticMeshComponent*> Components;
			It->GetComponents<UStaticMeshComponent>(Components);
			for (const UStaticMeshComponent* Component : Components)
			{
				auto Item = MakeShared<FJsonObject>();
				UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
				UMaterialInterface* Material = Component ? Component->GetMaterial(0) : nullptr;
				const double LastOnScreen = Component
					? Component->GetLastRenderTimeOnScreen() : 0.0;
				const bool bRegistered = Component && Component->IsRegistered();
				const bool bVisible = Component && Component->IsVisible();
				const bool bRenderStateCreated = Component && Component->IsRenderStateCreated();
				const bool bMeshReady = Mesh && !Mesh->IsCompiling() && Mesh->GetRenderData();
				const bool bMaterialReady = Material && Material->IsComplete();
				const double WorldSeconds = GetWorld()->GetTimeSeconds();
				const bool bRenderedAfterSelection = LastOnScreen > SelectionArrowReadyObservedWorldSeconds;
				const bool bRecentlyRendered = WorldSeconds - LastOnScreen < 0.25;
				const bool bArrowReady = !It->IsHidden() && bRegistered && bVisible
					&& bRenderStateCreated && bMeshReady && bMaterialReady
					&& bRenderedAfterSelection && bRecentlyRendered;
				Item->SetStringField(TEXT("name"), Component ? Component->GetName() : FString());
				Item->SetBoolField(TEXT("indicator_hidden"), It->IsHidden());
				Item->SetBoolField(TEXT("registered"), bRegistered);
				Item->SetBoolField(TEXT("component_visible"), bVisible);
				Item->SetBoolField(TEXT("render_state_created"), bRenderStateCreated);
				Item->SetBoolField(TEXT("mesh_ready"), bMeshReady);
				Item->SetBoolField(TEXT("material_ready"), bMaterialReady);
				Item->SetBoolField(TEXT("rendered_after_selection"), bRenderedAfterSelection);
				Item->SetBoolField(TEXT("recently_rendered"), bRecentlyRendered);
				Item->SetNumberField(TEXT("selection_ready_observed_world_seconds"),
					SelectionArrowReadyObservedWorldSeconds);
				Item->SetNumberField(TEXT("observation_world_seconds"), WorldSeconds);
				Item->SetNumberField(TEXT("seconds_since_last_on_screen_render"),
					WorldSeconds - LastOnScreen);
				Item->SetBoolField(TEXT("ready_and_rendered_after_selection"), bArrowReady);
				Item->SetNumberField(TEXT("last_render_time_on_screen"), LastOnScreen);
				ArrowEvidence.Add(MakeShared<FJsonValueObject>(Item));
				if (bArrowReady)
				{
					++RenderedArrowCount;
					EarliestArrowRenderTime = FMath::Min(EarliestArrowRenderTime, LastOnScreen);
				}
			}
		}
		Evidence->SetNumberField(TEXT("selection_indicator_count"), IndicatorCount);
		Evidence->SetNumberField(TEXT("selection_rendered_arrow_count"), RenderedArrowCount);
		Evidence->SetNumberField(TEXT("selection_earliest_arrow_render_time"),
			RenderedArrowCount == 4 ? EarliestArrowRenderTime : 0.0);
		Evidence->SetArrayField(TEXT("selection_arrow_components"), ArrowEvidence);
		bReady &= IndicatorCount == 1 && RenderedArrowCount == 4;
	}
	if (!Ghost) return bReady;
	if (PreparedGhost.Get() != Ghost) return false;
	bReady &= !Ghost->IsHidden();
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

bool URokLocalConstructionRuntimeAuditSubsystem::ObserveSelectedPanelReadiness(
	TSharedRef<FJsonObject> Evidence)
{
	const FString Phase = Pending.IsValid()
		? Pending->GetStringField(TEXT("phase_name")) : FString();
	const bool bExpected = Phase == TEXT("05_farm_zoomed_out_selected")
		|| Phase == TEXT("05_farm_world_selected")
		|| Phase == TEXT("09_wood_world_selected");
	Evidence->SetBoolField(TEXT("selected_panel_expected"), bExpected);
	if (!bExpected) return true;
	auto PanelEvidence = MakeShared<FJsonObject>();
	PanelEvidence->SetNumberField(TEXT("observed_frame"), GFrameCounter);
	PanelEvidence->SetStringField(TEXT("menu_widget"), TEXT("BuildingMenuRoot"));
	PanelEvidence->SetStringField(TEXT("title_widget"), TEXT("BuildingTitleRoot"));
	UWidget* Menu = Panel.IsValid()
		? Panel->GetWidgetFromName(TEXT("BuildingMenuRoot")) : nullptr;
	UWidget* Title = Panel.IsValid()
		? Panel->GetWidgetFromName(TEXT("BuildingTitleRoot")) : nullptr;
	const bool bWidgetsVisible = Menu && Title && Visible(Panel.Get())
		&& Visible(Menu) && Visible(Title);
	PanelEvidence->SetBoolField(TEXT("widgets_visible"), bWidgetsVisible);
	const float MenuOpacity = Menu ? Menu->GetRenderOpacity() : -1.0f;
	const FVector2D TitleScale = Title
		? Title->GetRenderTransform().Scale : FVector2D::ZeroVector;
	const FRokUiWindowAnimationSample EndSample = FRokUiWindowAnimation::Evaluate(
		ERokUiWindowAnimationStyle::BuildingMenu, false,
		FRokUiWindowAnimation::GetDuration(ERokUiWindowAnimationStyle::BuildingMenu, false));
	const bool bFinalSample = bWidgetsVisible
		&& FMath::IsNearlyEqual(MenuOpacity, EndSample.Opacity, 0.001f)
		&& FMath::IsNearlyEqual(TitleScale.X, EndSample.Scale, 0.001f)
		&& FMath::IsNearlyEqual(TitleScale.Y, EndSample.Scale, 0.001f);
	PanelEvidence->SetNumberField(TEXT("menu_opacity"), MenuOpacity);
	PanelEvidence->SetNumberField(TEXT("title_scale_x"), TitleScale.X);
	PanelEvidence->SetNumberField(TEXT("title_scale_y"), TitleScale.Y);
	PanelEvidence->SetNumberField(TEXT("source_final_opacity"), EndSample.Opacity);
	PanelEvidence->SetNumberField(TEXT("source_final_title_scale"), EndSample.Scale);
	PanelEvidence->SetBoolField(TEXT("source_final_animation_sample"), bFinalSample);
	FVector2D MenuCenter = FVector2D::ZeroVector;
	FVector2D MenuSize = FVector2D::ZeroVector;
	FVector2D TitleCenter = FVector2D::ZeroVector;
	FVector2D TitleSize = FVector2D::ZeroVector;
	if (Menu && Title)
	{
		const FGeometry MenuGeometry = Menu->GetCachedGeometry();
		const FGeometry TitleGeometry = Title->GetCachedGeometry();
		const FVector2D MenuTopLeft = MenuGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D MenuBottomRight = MenuGeometry.LocalToAbsolute(MenuGeometry.GetLocalSize());
		const FVector2D TitleTopLeft = TitleGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D TitleBottomRight = TitleGeometry.LocalToAbsolute(TitleGeometry.GetLocalSize());
		MenuSize = MenuBottomRight - MenuTopLeft;
		TitleSize = TitleBottomRight - TitleTopLeft;
		MenuCenter = (MenuTopLeft + MenuBottomRight) * 0.5f;
		TitleCenter = (TitleTopLeft + TitleBottomRight) * 0.5f;
	}
	const bool bValidGeometry = MenuSize.X > 0 && MenuSize.Y > 0
		&& TitleSize.X > 0 && TitleSize.Y > 0
		&& FMath::IsFinite(MenuCenter.X) && FMath::IsFinite(MenuCenter.Y)
		&& FMath::IsFinite(TitleCenter.X) && FMath::IsFinite(TitleCenter.Y);
	const bool bSameGeometry = SelectionPanelStableFrames > 0
		&& SelectionMenuCenter.Equals(MenuCenter, 0.1f)
		&& SelectionMenuSize.Equals(MenuSize, 0.1f)
		&& SelectionTitleCenter.Equals(TitleCenter, 0.1f)
		&& SelectionTitleSize.Equals(TitleSize, 0.1f);
	if (!bFinalSample || !bValidGeometry)
	{
		SelectionPanelStableFrames = 0;
		SelectionPanelFirstStableFrame = 0;
	}
	else if (SelectionPanelLastObservedFrame != GFrameCounter)
	{
		SelectionPanelStableFrames = bSameGeometry
			? SelectionPanelStableFrames + 1 : 1;
		if (SelectionPanelStableFrames == 1)
			SelectionPanelFirstStableFrame = GFrameCounter;
	}
	SelectionPanelLastObservedFrame = GFrameCounter;
	SelectionMenuCenter = MenuCenter;
	SelectionMenuSize = MenuSize;
	SelectionTitleCenter = TitleCenter;
	SelectionTitleSize = TitleSize;
	PanelEvidence->SetBoolField(TEXT("cached_geometry_valid"), bValidGeometry);
	PanelEvidence->SetNumberField(TEXT("menu_center_x"), MenuCenter.X);
	PanelEvidence->SetNumberField(TEXT("menu_center_y"), MenuCenter.Y);
	PanelEvidence->SetNumberField(TEXT("menu_width"), MenuSize.X);
	PanelEvidence->SetNumberField(TEXT("menu_height"), MenuSize.Y);
	PanelEvidence->SetNumberField(TEXT("title_center_x"), TitleCenter.X);
	PanelEvidence->SetNumberField(TEXT("title_center_y"), TitleCenter.Y);
	PanelEvidence->SetNumberField(TEXT("title_width"), TitleSize.X);
	PanelEvidence->SetNumberField(TEXT("title_height"), TitleSize.Y);
	PanelEvidence->SetNumberField(TEXT("first_stable_frame"), SelectionPanelFirstStableFrame);
	PanelEvidence->SetNumberField(TEXT("stable_geometry_frames"), SelectionPanelStableFrames);
	PanelEvidence->SetBoolField(TEXT("ready"), SelectionPanelStableFrames >= 2);
	Evidence->SetObjectField(TEXT("selected_panel"), PanelEvidence);
	return SelectionPanelStableFrames >= 2;
}

void URokLocalConstructionRuntimeAuditSubsystem::AdvancePendingCapture()
{
	if (!Pending.IsValid() || bFinished) return;
	if ((StepAfterCapture == 16 && !Buildings->IsLocalConstructionPending(FarmId))
		|| (StepAfterCapture == 24 && !Buildings->IsLocalConstructionPending(WoodId)))
	{
		Finish(TEXT("Natural two-second construction finished before pending screenshot completed"));
		return;
	}
	const auto SnapshotValue = Controller->GetCityPlacementPreviewSnapshot();
	FString Error;
	if (SnapshotValue.bActive && !PrepareReferencedPreviewAssets(
		Cast<ARokCityPlacementPreviewActor>(SnapshotValue.PreviewActor.Get()), Error))
	{
		Pending->SetStringField(TEXT("target_asset_preflight_error"), Error);
		Finish(Error.IsEmpty() ? TEXT("Preview actor unavailable for target asset readiness") : Error);
		return;
	}
	ARokCitySpriteActor* ConstructionActor = GetCaptureConstructionActor();
	if (ConstructionActor && !PrepareReferencedConstructionAssets(ConstructionActor, Error))
	{
		Pending->SetStringField(TEXT("construction_asset_preflight_error"), Error);
		Finish(Error.IsEmpty() ? TEXT("Constructed actor assets were not ready") : Error);
		return;
	}
	if (!PrepareReferencedSelectionArrowAssets(Error))
	{
		Pending->SetStringField(TEXT("selection_arrow_asset_preflight_error"), Error);
		Finish(Error.IsEmpty() ? TEXT("Selected arrow assets were not ready") : Error);
		return;
	}
	auto Evidence = MakeShared<FJsonObject>();
	Evidence->SetNumberField(TEXT("observation_frame"), GFrameCounter);
	const bool bAssetsReady = IsRenderReadyForCapture(Evidence);
	const bool bSelectedPanelReady = ObserveSelectedPanelReadiness(Evidence);
	const bool bPlacementMenuReady = ObservePlacementMenuReadiness(Evidence);
	const bool bReady = bAssetsReady && bSelectedPanelReady && bPlacementMenuReady;
	bool bDistinctRenderedFrame = true;
	if (bReady && ConstructionActor)
	{
		const double ActorTime = Evidence->GetObjectField(TEXT("construction_actor"))
			->GetNumberField(TEXT("last_render_time_on_screen"));
		bDistinctRenderedFrame &= ActorTime > LastObservedConstructionRenderTime;
		if (ActorTime > LastObservedConstructionRenderTime)
			LastObservedConstructionRenderTime = ActorTime;
	}
	if (bReady && Evidence->GetBoolField(TEXT("selection_arrows_expected")))
	{
		const double ArrowTime = Evidence->GetNumberField(
			TEXT("selection_earliest_arrow_render_time"));
		bDistinctRenderedFrame &= ArrowTime > LastObservedSelectionArrowRenderTime;
		if (ArrowTime > LastObservedSelectionArrowRenderTime)
			LastObservedSelectionArrowRenderTime = ArrowTime;
	}
	if (SnapshotValue.bActive && bReady)
	{
		const double BuildingTime = Evidence->GetObjectField(TEXT("building"))
			->GetNumberField(TEXT("last_render_time_on_screen"));
		const double BottomTime = Evidence->GetObjectField(TEXT("bottom"))
			->GetNumberField(TEXT("last_render_time_on_screen"));
		const bool bGhostDistinct = BuildingTime > LastObservedBuildingRenderTime
			&& BottomTime > LastObservedBottomRenderTime;
		bDistinctRenderedFrame &= bGhostDistinct;
		if (bGhostDistinct)
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

void URokLocalConstructionRuntimeAuditSubsystem::ScreenshotProcessed()
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
	if (ARokCitySpriteActor* Actor = GetCaptureConstructionActor())
	{
		const UStaticMeshComponent* Component = Actor->GetSpriteMeshComponent();
		LastObservedConstructionRenderTime = Component
			? Component->GetLastRenderTimeOnScreen() : 0.0;
	}
	const FString Phase = Pending->GetStringField(TEXT("phase_name"));
	if (Phase == TEXT("05_farm_world_selected")
		|| Phase == TEXT("09_wood_world_selected"))
	{
		double Earliest = TNumericLimits<double>::Max();
		int32 Count = 0;
		for (TActorIterator<ARokBuildingSelectionIndicator> It(GetWorld()); It; ++It)
		{
			if (It->IsActorBeingDestroyed()) continue;
			TArray<UStaticMeshComponent*> Components;
			It->GetComponents<UStaticMeshComponent>(Components);
			for (const UStaticMeshComponent* Component : Components)
				if (Component)
				{
					Earliest = FMath::Min(Earliest, Component->GetLastRenderTimeOnScreen());
					++Count;
				}
		}
		LastObservedSelectionArrowRenderTime = Count == 4 ? Earliest : 0.0;
	}
}

bool URokLocalConstructionRuntimeAuditSubsystem::Save(bool bComplete)
{
	auto Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("schema"), TEXT("rok.local_construction.runtime_audit.v1"));
	Manifest->SetBoolField(TEXT("complete"), bComplete);
	Manifest->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty()
		&& Captures.Num() == ExpectedCaptures && bBaselineReady
		&& CompletionCount == 1 && LocalCompletionCount == 2 && FarmId > 0 && WoodId > FarmId);
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
	Manifest->SetStringField(TEXT("scope"), TEXT("ordinary HUD local_preview Hall2 then Farm/Wood confirmation and natural two-second completion; observed ServerSubsystem HTTP dispatch=0 and suppressed requests=0 under RokLocalOnly; no scaffolding effect, gamma visual parity, world travel or other resolution"));
	Manifest->SetNumberField(TEXT("farm_ticket_id"), static_cast<double>(FarmId));
	Manifest->SetNumberField(TEXT("wood_ticket_id"), static_cast<double>(WoodId));
	Manifest->SetNumberField(TEXT("local_completion_events"), LocalCompletionCount);
	Manifest->SetNumberField(TEXT("farm_submit_wall_seconds"), FarmClickedWall);
	Manifest->SetNumberField(TEXT("farm_completion_observed_wall_seconds"), FarmCompletionWall);
	Manifest->SetNumberField(TEXT("wood_submit_wall_seconds"), WoodClickedWall);
	Manifest->SetNumberField(TEXT("wood_completion_observed_wall_seconds"), WoodCompletionWall);
	Manifest->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	Manifest->SetBoolField(TEXT("construction_started_only_by_production_hud_delegate"), true);
	Manifest->SetStringField(TEXT("delegate_assurance"),
		TEXT("source_pinned_code_and_observed_controller_outcome"));
	Manifest->SetBoolField(TEXT("server_local_only_run"), Server.IsValid()
		&& Server->IsLocalOnlyRun());
	Manifest->SetNumberField(TEXT("server_http_dispatch_attempts"), Server.IsValid()
		? static_cast<double>(Server->GetHttpDispatchAttemptCount()) : -1.0);
	Manifest->SetNumberField(TEXT("server_suppressed_local_requests"), Server.IsValid()
		? static_cast<double>(Server->GetSuppressedLocalRequestCount()) : -1.0);
	Manifest->SetBoolField(TEXT("gamma_census_invoked"), bGammaCensusInvoked);
	Manifest->SetBoolField(TEXT("gamma_census_returned"), bGammaCensusReturned);
	Manifest->SetStringField(TEXT("gamma_census_manifest_path"), GammaCensusPath);
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

void URokLocalConstructionRuntimeAuditSubsystem::Finish(const FString& Error)
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
		&& Captures.Num() == ExpectedCaptures && bBaselineReady
		&& CompletionCount == 1 && LocalCompletionCount == 2 && FarmId > 0 && WoodId > FarmId;
	if (!bComplete && Failures.IsEmpty()) Failures.Add(TEXT("Placement audit incomplete"));
	Save(bComplete);
	UE_LOG(LogTemp, Display, TEXT("Rok local construction runtime audit %s: %s"),
		bComplete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!bComplete) UE_LOG(LogTemp, Error, TEXT("Rok local construction audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, bComplete ? 0 : 1);
}


void URokLocalConstructionRuntimeAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !TargetWorld(GetWorld())) return;
    Elapsed += FMath::Max(0.0f, DeltaTime);
    if (Elapsed < 2.0) return;
    if (Pending.IsValid())
    {
        if (Elapsed - ShotStarted > 30.0) Finish(TEXT("Construction screenshot callback timeout"));
        else AdvancePendingCapture();
        return;
    }
    if (Step > 0 && Elapsed - StepStarted > 35.0)
    {
        Finish(FString::Printf(TEXT("Construction audit step %d timed out"), Step));
        return;
    }
    if (!Identity() || !Unchanged()) return;
    auto GridPoint = [this](const FIntPoint& Grid, bool bHeld, const TCHAR* Label)
    {
        FVector2D Viewport = FVector2D::ZeroVector, Absolute = FVector2D::ZeroVector;
        if (!bHeld)
            return Expect(ProjectGrid(Grid, Viewport, Absolute),
                TEXT("Construction grid center is not in actual world viewport"))
                && MovePointer(Viewport, Absolute, false, Label);
        TSharedPtr<FJsonObject> Preflight;
        const bool bTarget = ProjectHeldGridInterior(Grid, Viewport, Absolute, Preflight);
        if (Preflight.IsValid())
        {
            Preflight->SetStringField(TEXT("pointer_action"), Label);
            Actions.Add(MakeShared<FJsonValueObject>(Preflight));
        }
        return Expect(bTarget, TEXT("Held pointer did not quantize to exact source grid"))
            && MovePointer(Viewport, Absolute, true, Label);
    };
    auto StartPredictedDrag = [this]()
    {
        const auto Value = Controller->GetCityPlacementPreviewSnapshot();
        FRokCityConstructionFootprint Footprint;
        Footprint.Origin = Value.Grid;
        Footprint.Size = Value.Footprint;
        FString Error;
        PredictedDrag = FRokCityPlacementDragState();
        return Expect(Value.PlacementDragBeginCount > WorldPressDragBeginCountBefore
            && Value.LastPrimaryPressOutcome == FName(TEXT("placement_dispatched"))
            && FRokCityPlacementPreviewMath::BeginDrag(PredictedDrag, Footprint,
                WorldFrame.GetCityContainerTransform(), Value.LastPlacementPressGround, false, Error),
            TEXT("Real Controller input did not begin source-grid drag"));
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
            if (FreezePlacementBaseline()) Capture(TEXT("01_hall2_prerequisite"), 4);
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
            FarmGrid = InitialGrid;
            if (ChooseTargets()) Capture(TEXT("02_farm_valid_preview"), 7);
        }
        break;
    case 7:
        if (GridPoint(FarmGrid, false, TEXT("farm_ghost_world_pointer"))
            && WorldButton(true, TEXT("farm_world_down"))) Enter(8);
        break;
    case 8:
        if (StartPredictedDrag() && CheckPreview(2, true, true, true)
            && GridPoint(OccupiedGrid, true, TEXT("farm_drag_to_source_occupied"))) Enter(9);
        break;
    case 9:
        if (CheckPreview(2, true, true, false)
            && Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == OccupiedGrid,
                TEXT("Occupied drag reached the wrong source grid"))
            && WorldButton(false, TEXT("farm_release_invalid_drag"))) Enter(10);
        break;
    case 10:
        if (CheckPreview(2, true, false, false))
            Capture(TEXT("02b_farm_invalid_gray_preview"), 101);
        break;
    case 101:
        if (CheckPreview(2, true, false, false)
            && DisabledConfirmClick()) Enter(11);
        break;
    case 11:
        if (!Expect(InvalidDisabledConfirmAction.IsValid()
            && GFrameCounter > InvalidDisabledConfirmAction->GetNumberField(TEXT("sent_frame"))
            && CheckPreview(2, true, false, false),
            TEXT("Disabled invalid confirm lacked a following game-frame preview readback"))) break;
        if (!InvalidDisabledConfirmAction->HasField(TEXT("observed_after_frame")))
        {
            const FRokResourceStock WalletAfterFrame = Buildings->GetCurrentResources();
            const auto AfterFrame = Controller->GetCityPlacementPreviewSnapshot();
            const ARokStrategyCameraPawn* Camera = Cast<ARokStrategyCameraPawn>(Controller->GetPawn());
            InvalidDisabledConfirmAction->SetNumberField(TEXT("observed_after_frame"), GFrameCounter);
            InvalidDisabledConfirmAction->SetNumberField(TEXT("confirm_count_after_next_game_frame"),
                static_cast<double>(AfterFrame.ConfirmClickCount));
            InvalidDisabledConfirmAction->SetNumberField(TEXT("primary_press_after_next_game_frame"),
                AfterFrame.PrimaryPressEventCount);
            InvalidDisabledConfirmAction->SetStringField(TEXT("primary_press_outcome_after_next_game_frame"),
                AfterFrame.LastPrimaryPressOutcome.ToString());
            InvalidDisabledConfirmAction->SetNumberField(TEXT("drag_begin_after_next_game_frame"),
                AfterFrame.PlacementDragBeginCount);
            InvalidDisabledConfirmAction->SetStringField(TEXT("selection_after_next_game_frame"),
                AfterFrame.CurrentSelection.IsValid() ? AfterFrame.CurrentSelection->GetName() : FString());
            InvalidDisabledConfirmAction->SetObjectField(TEXT("grid_after_next_game_frame"),
                PointJson(FVector2D(AfterFrame.Grid.X, AfterFrame.Grid.Y)));
            InvalidDisabledConfirmAction->SetBoolField(TEXT("dragging_after_next_game_frame"), AfterFrame.bDragging);
            InvalidDisabledConfirmAction->SetNumberField(TEXT("camera_dxf_after_next_game_frame"),
                Camera ? Camera->GetCurrentCityDxf() : -1.0);
            InvalidDisabledConfirmAction->SetNumberField(TEXT("wallet_food_after_next_game_frame"),
                WalletAfterFrame.Food);
            InvalidDisabledConfirmAction->SetNumberField(TEXT("wallet_wood_after_next_game_frame"),
                WalletAfterFrame.Wood);
            InvalidDisabledConfirmAction->SetNumberField(TEXT("active_tickets_after_next_game_frame"),
                Buildings->GetActiveLocalConstructionCount());
            InvalidDisabledConfirmAction->SetBoolField(TEXT("invalid_preview_active_after_next_game_frame"),
                AfterFrame.bActive);
        }
        if (!Expect(InvalidDisabledConfirmAction->GetNumberField(TEXT("confirm_count_after_next_game_frame"))
                == InvalidDisabledConfirmAction->GetNumberField(TEXT("confirm_count_before"))
            && InvalidDisabledConfirmAction->GetNumberField(TEXT("primary_press_after_next_game_frame"))
                >= InvalidDisabledConfirmAction->GetNumberField(TEXT("primary_press_after_up"))
            && InvalidDisabledConfirmAction->GetNumberField(TEXT("primary_press_after_next_game_frame"))
                <= InvalidDisabledConfirmAction->GetNumberField(TEXT("primary_press_before")) + 1
            && (InvalidDisabledConfirmAction->GetNumberField(TEXT("primary_press_after_next_game_frame"))
                    == InvalidDisabledConfirmAction->GetNumberField(TEXT("primary_press_after_up"))
                ? InvalidDisabledConfirmAction->GetStringField(TEXT("primary_press_outcome_after_next_game_frame"))
                    == InvalidDisabledConfirmAction->GetStringField(TEXT("primary_press_outcome_after_up"))
                : InvalidDisabledConfirmAction->GetStringField(TEXT("primary_press_outcome_after_next_game_frame"))
                    == TEXT("ui_owned"))
            && InvalidDisabledConfirmAction->GetNumberField(TEXT("drag_begin_after_next_game_frame"))
                == InvalidDisabledConfirmAction->GetNumberField(TEXT("drag_begin_before"))
            && InvalidDisabledConfirmAction->GetStringField(TEXT("selection_after_next_game_frame"))
                == InvalidDisabledConfirmAction->GetStringField(TEXT("selection_before"))
            && InvalidDisabledConfirmAction->GetObjectField(TEXT("grid_after_next_game_frame"))->GetNumberField(TEXT("x"))
                == InvalidDisabledConfirmAction->GetObjectField(TEXT("grid_before"))->GetNumberField(TEXT("x"))
            && InvalidDisabledConfirmAction->GetObjectField(TEXT("grid_after_next_game_frame"))->GetNumberField(TEXT("y"))
                == InvalidDisabledConfirmAction->GetObjectField(TEXT("grid_before"))->GetNumberField(TEXT("y"))
            && !InvalidDisabledConfirmAction->GetBoolField(TEXT("dragging_after_next_game_frame"))
            && FMath::IsNearlyEqual(
                InvalidDisabledConfirmAction->GetNumberField(TEXT("camera_dxf_after_next_game_frame")),
                InvalidDisabledConfirmAction->GetNumberField(TEXT("camera_dxf_before")), 0.001)
            && InvalidDisabledConfirmAction->GetNumberField(TEXT("wallet_food_after_next_game_frame"))
                == InvalidDisabledConfirmAction->GetNumberField(TEXT("wallet_food_before"))
            && InvalidDisabledConfirmAction->GetNumberField(TEXT("wallet_wood_after_next_game_frame"))
                == InvalidDisabledConfirmAction->GetNumberField(TEXT("wallet_wood_before"))
            && InvalidDisabledConfirmAction->GetNumberField(TEXT("active_tickets_after_next_game_frame"))
                == InvalidDisabledConfirmAction->GetNumberField(TEXT("active_tickets_before"))
            && Buildings->GetActiveLocalConstructionCount() == 0,
            TEXT("Disabled invalid confirm reached Controller, wallet or local ticket next frame"))) break;
        if (GridPoint(OccupiedGrid, false, TEXT("invalid_ghost_world_pointer"))
            && WorldButton(true, TEXT("farm_invalid_ghost_world_down"))) Enter(12);
        break;
    case 12:
        if (StartPredictedDrag() && CheckPreview(2, true, true, false)
            && GridPoint(ValidGrid, true, TEXT("farm_drag_to_source_valid"))) Enter(13);
        break;
    case 13:
        if (CheckPreview(2, true, true, true)
            && Expect(Controller->GetCityPlacementPreviewSnapshot().Grid == ValidGrid,
                TEXT("Return drag did not reach source valid grid"))
            && WorldButton(false, TEXT("farm_release_valid_drag"))) Enter(14);
        break;
    case 14:
        if (CheckPreview(2, true, false, true)
            && ReadyForClick(Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")), false))
        {
            FarmGrid = ValidGrid;
            ConfirmCountBefore = Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount;
            UWidget* Confirm = Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton"));
            LastConfirmAbsolutePointer = Confirm->GetCachedGeometry().LocalToAbsolute(
                Confirm->GetCachedGeometry().GetLocalSize() * 0.5);
            if (SlateClick(Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")),
                TEXT("farm_confirm_ordinary_hud")))
            {
                FarmClickedWall = FPlatformTime::Seconds();
                Enter(15);
            }
        }
        break;
    case 15:
    {
		if (!ObserveFirstConfirmNextFrame(TEXT("farm_confirm_ordinary_hud"))) break;
        const auto Value = Controller->GetCityPlacementPreviewSnapshot();
        FarmId = Value.LastConfirmedLocalConstructionId;
        if (Expect(Value.ConfirmClickCount == ConfirmCountBefore + 1
            && Value.LastConfirmOutcome == FName(TEXT("committed"))
            && FarmId > 0 && Value.LastConfirmedActor.IsValid()
            && Value.LastConfirmedActor.Get() == Buildings->GetLocalConstructionActor(FarmId)
            && !Value.bActive && !Value.PreviewActor.IsValid()
            && !Value.bCameraInputSuspended && !Value.CurrentSelection.IsValid(),
            TEXT("Farm ordinary HUD confirm did not commit exactly once and clear ghost"))
            && CheckConstruction(FarmId, 2, false, 1500, 1400, 1)
            && RepeatConfirmPointer(TEXT("farm_repeat_confirm_pointer")))
            Enter(151);
        break;
    }
    case 151:
		if (ObserveRepeatConfirmNextFrame(TEXT("farm_repeat_confirm_pointer"))
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount
                == ConfirmCountBefore + 1,
            TEXT("Second Farm tap triggered another confirmation"))
            && CheckConstruction(FarmId, 2, false, 1500, 1400, 1)) Enter(152);
        break;
    case 152:
		if (ObserveSelectedPanelCloseEnd(FarmClickedWall))
        {
            FVector2D Viewport = FVector2D::ZeroVector, Absolute = FVector2D::ZeroVector;
            if (Expect(ProjectConstructedActor(Buildings->GetLocalConstructionActor(FarmId),
                    Viewport, Absolute), TEXT("Pending Farm has no world pointer projection"))
                && MovePointer(Viewport, Absolute, false, TEXT("farm_pending_world_pointer")))
            {
                SelectionPressBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
                if (WorldButton(true, TEXT("farm_pending_world_down"))
                    && WorldButton(false, TEXT("farm_pending_world_up"))) Enter(150);
            }
        }
        break;
    case 150:
        if (Expect(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount
                == SelectionPressBefore + 1
            && Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get()
                != Buildings->GetLocalConstructionActor(FarmId),
            TEXT("Pending Farm became selectable before source timer completed")))
            Capture(TEXT("03_farm_pending_once"), 16);
        break;
    case 16:
        if (LocalCompletionCount >= 1 && !Buildings->IsLocalConstructionPending(FarmId))
            if (Expect(LocalCompletionCount == 1 && FPlatformTime::Seconds() - FarmClickedWall >= 1.9,
                TEXT("Farm did not finish once after natural source two seconds"))
                && CheckConstruction(FarmId, 2, true, 1500, 1400, 0))
            {
                FarmCompletionWall = FPlatformTime::Seconds();
                Capture(TEXT("04_farm_completed_registered"), 17);
            }
        break;
    case 17:
    {
        FVector2D Viewport = FVector2D::ZeroVector, Absolute = FVector2D::ZeroVector;
        if (Expect(ProjectConstructedActor(Buildings->GetLocalConstructionActor(FarmId),
                Viewport, Absolute), TEXT("Completed Farm has no real world click projection"))
            && MovePointer(Viewport, Absolute, false, TEXT("farm_completed_world_pointer")))
        {
            SelectionPressBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            if (WorldButton(true, TEXT("farm_completed_world_down"))
                && WorldButton(false, TEXT("farm_completed_world_up"))) Enter(18);
        }
        break;
    }
    case 18:
        if (Expect(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount
                == SelectionPressBefore + 1
            && Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get()
                == Buildings->GetLocalConstructionActor(FarmId),
            TEXT("Completed Farm was not selected through ordinary world input"))
            && CheckSelectionFeedback(Buildings->GetLocalConstructionActor(FarmId),
                TEXT("farm_zoomed_out_selection_no_arrows"), false))
            Capture(TEXT("05_farm_zoomed_out_selected"), 181);
        break;
    case 181:
        if (SlateZoomInAtWorldPointer()) Enter(182);
        break;
    case 182:
    {
        if (GFrameCounter <= ZoomWheelSentFrame + 1) break;
        const ARokStrategyCameraPawn* Camera = Cast<ARokStrategyCameraPawn>(Controller->GetPawn());
        const double DxfAfter = Camera ? Camera->GetCurrentCityDxf() : -1.0;
        if (PendingZoomAction.IsValid())
        {
            PendingZoomAction->SetNumberField(TEXT("observed_frame"), GFrameCounter);
            PendingZoomAction->SetNumberField(TEXT("dxf_after_game_frame"), DxfAfter);
        }
        if (!Expect(Camera && FMath::IsFinite(DxfAfter)
            && DxfAfter < ZoomBeforeDxf - 0.01,
            TEXT("Slate viewport wheel did not advance the real CameraPawn zoom"))) break;
        if (DxfAfter <= 245.0)
        {
            ZoomCompletedDxf = DxfAfter;
            Enter(183);
        }
        else Enter(181);
        break;
    }
    case 183:
    {
        const ARokStrategyCameraPawn* Camera = Cast<ARokStrategyCameraPawn>(Controller->GetPawn());
        ARokCitySpriteActor* Farm = Buildings->GetLocalConstructionActor(FarmId);
        if (!Expect(Camera && Farm && Camera->GetCurrentCityDxf() <= 245.0f,
            TEXT("True viewport zoom did not remain below source arrow threshold"))) break;
        if (Farm->GetUnityLodScale() > 1.0f + KINDA_SMALL_NUMBER)
        {
            if (Elapsed - StepStarted < 1.0) break;
            Expect(false, TEXT("Farm source LOD did not settle after actual viewport zoom"));
            break;
        }
        if (!Expect(Farm->CanShowMoveArrows(),
            TEXT("Zoomed source Farm still cannot show move arrows"))) break;
        auto ZoomResult = MakeShared<FJsonObject>();
        ZoomResult->SetStringField(TEXT("action"), TEXT("farm_zoom_source_lod_settled"));
        ZoomResult->SetNumberField(TEXT("observed_frame"), GFrameCounter);
        ZoomResult->SetNumberField(TEXT("camera_city_dxf"), Camera->GetCurrentCityDxf());
        ZoomResult->SetNumberField(TEXT("farm_unity_lod_scale"), Farm->GetUnityLodScale());
        ZoomResult->SetNumberField(TEXT("wheel_event_count"), ZoomWheelEventCount);
        Actions.Add(MakeShared<FJsonValueObject>(ZoomResult));
        FVector2D Viewport = FVector2D::ZeroVector, Absolute = FVector2D::ZeroVector;
        if (Expect(ProjectConstructedActor(Farm, Viewport, Absolute),
                TEXT("Zoomed Farm has no recomputed world click projection"))
            && MovePointer(Viewport, Absolute, false, TEXT("farm_zoomed_world_pointer")))
        {
            SelectionPressBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            if (WorldButton(true, TEXT("farm_zoomed_world_down"))
                && WorldButton(false, TEXT("farm_zoomed_world_up"))) Enter(184);
        }
        break;
    }
    case 184:
        if (Expect(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount
                == SelectionPressBefore + 1
            && Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get()
                == Buildings->GetLocalConstructionActor(FarmId),
            TEXT("Zoomed Farm was not selected through recomputed ordinary world input"))
            && CheckSelectionFeedback(Buildings->GetLocalConstructionActor(FarmId),
                TEXT("farm_zoomed_in_selection_arrows"), true))
            Capture(TEXT("05_farm_world_selected"), 19);
        break;
    case 19:
        if (ReadyForClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")), false)
            && SlateClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")),
                TEXT("build_city_open_wood"))) Enter(20);
        break;
    case 20:
        if (Screen->IsScreenOpen() && Screen->IsBuildCityModeForAudit()
            && ReadyForClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_3")),
                Screen->GetWindowAnimationForTesting().IsAnimating())
            && SlateClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_3")),
                TEXT("wood_source_card"))) Enter(21);
        break;
    case 21:
        if (CheckPreview(3, true, false, true))
        {
            WoodGrid = Controller->GetCityPlacementPreviewSnapshot().Grid;
            Capture(TEXT("06_wood_valid_preview"), 22);
        }
        break;
    case 22:
        if (ReadyForClick(Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")), false))
        {
            ConfirmCountBefore = Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount;
            UWidget* Confirm = Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton"));
            LastConfirmAbsolutePointer = Confirm->GetCachedGeometry().LocalToAbsolute(
                Confirm->GetCachedGeometry().GetLocalSize() * 0.5);
            if (SlateClick(Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton")),
                TEXT("wood_confirm_ordinary_hud")))
            {
                WoodClickedWall = FPlatformTime::Seconds();
                Enter(23);
            }
        }
        break;
    case 23:
    {
		if (!ObserveFirstConfirmNextFrame(TEXT("wood_confirm_ordinary_hud"))) break;
        const auto Value = Controller->GetCityPlacementPreviewSnapshot();
        WoodId = Value.LastConfirmedLocalConstructionId;
        if (Expect(Value.ConfirmClickCount == ConfirmCountBefore + 1
            && Value.LastConfirmOutcome == FName(TEXT("committed"))
            && WoodId > FarmId && Value.LastConfirmedActor.IsValid()
            && Value.LastConfirmedActor.Get() == Buildings->GetLocalConstructionActor(WoodId)
            && !Value.bActive && !Value.PreviewActor.IsValid()
            && !Value.bCameraInputSuspended && !Value.CurrentSelection.IsValid(),
            TEXT("Wood ordinary HUD confirm did not commit exactly once and clear ghost"))
            && CheckConstruction(WoodId, 3, false, 1400, 1400, 1)
            && CheckConstruction(FarmId, 2, true, 1400, 1400, 1)
            && RepeatConfirmPointer(TEXT("wood_repeat_confirm_pointer")))
            Enter(231);
        break;
    }
    case 231:
		if (ObserveRepeatConfirmNextFrame(TEXT("wood_repeat_confirm_pointer"))
			&& Expect(Controller->GetCityPlacementPreviewSnapshot().ConfirmClickCount
                == ConfirmCountBefore + 1,
            TEXT("Second Wood tap triggered another confirmation"))
            && CheckConstruction(WoodId, 3, false, 1400, 1400, 1)
            && CheckConstruction(FarmId, 2, true, 1400, 1400, 1)) Enter(232);
        break;
    case 232:
		if (ObserveSelectedPanelCloseEnd(WoodClickedWall))
        {
            FVector2D Viewport = FVector2D::ZeroVector, Absolute = FVector2D::ZeroVector;
            if (Expect(ProjectConstructedActor(Buildings->GetLocalConstructionActor(WoodId),
                    Viewport, Absolute), TEXT("Pending Wood has no world pointer projection"))
                && MovePointer(Viewport, Absolute, false, TEXT("wood_pending_world_pointer")))
            {
                SelectionPressBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
                if (WorldButton(true, TEXT("wood_pending_world_down"))
                    && WorldButton(false, TEXT("wood_pending_world_up"))) Enter(230);
            }
        }
        break;
    case 230:
        if (Expect(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount
                == SelectionPressBefore + 1
            && Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get()
                != Buildings->GetLocalConstructionActor(WoodId),
            TEXT("Pending Wood became selectable before source timer completed")))
            Capture(TEXT("07_wood_pending_once"), 24);
        break;
    case 24:
        if (LocalCompletionCount >= 2 && !Buildings->IsLocalConstructionPending(WoodId))
            if (Expect(LocalCompletionCount == 2 && FPlatformTime::Seconds() - WoodClickedWall >= 1.9,
                TEXT("Wood did not finish once after natural source two seconds"))
                && CheckConstruction(WoodId, 3, true, 1400, 1400, 0)
                && CheckConstruction(FarmId, 2, true, 1400, 1400, 0))
            {
                WoodCompletionWall = FPlatformTime::Seconds();
                Capture(TEXT("08_wood_completed_registered"), 25);
            }
        break;
    case 25:
    {
        FVector2D Viewport = FVector2D::ZeroVector, Absolute = FVector2D::ZeroVector;
        if (Expect(ProjectConstructedActor(Buildings->GetLocalConstructionActor(WoodId),
                Viewport, Absolute), TEXT("Completed Wood has no real world click projection"))
            && MovePointer(Viewport, Absolute, false, TEXT("wood_completed_world_pointer")))
        {
            SelectionPressBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            if (WorldButton(true, TEXT("wood_completed_world_down"))
                && WorldButton(false, TEXT("wood_completed_world_up"))) Enter(26);
        }
        break;
    }
    case 26:
        if (Expect(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount
                == SelectionPressBefore + 1
            && Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get()
                == Buildings->GetLocalConstructionActor(WoodId),
            TEXT("Completed Wood was not selected through ordinary world input"))
            && CheckSelectionFeedback(Buildings->GetLocalConstructionActor(WoodId),
                TEXT("wood_selection_arrows"), true))
            Capture(TEXT("09_wood_world_selected"), 27);
        break;
    case 27:
        if (URokCityGammaLayerCensusSubsystem* Census = GetWorld()
            ->GetSubsystem<URokCityGammaLayerCensusSubsystem>())
        {
            bGammaCensusInvoked = true;
            bGammaCensusReturned = Census->CaptureNow(TEXT("farm_wood_completed"), GammaCensusPath);
        }
        Finish();
        break;
    default:
        Finish(TEXT("Construction audit reached an invalid state"));
        break;
    }
}
