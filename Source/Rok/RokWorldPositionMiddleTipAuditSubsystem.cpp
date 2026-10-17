#include "RokWorldPositionMiddleTipAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokClientSettingsSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokStrategyCameraPawn.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapConfigActor.h"
#include "RokWorldMapHudWidget.h"
#include "RokWorldMapStreamingActor.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TextureResource.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR WorldMap[] = TEXT("/Game/Maps/L_RokUnityWorldMapPrototype");
constexpr TCHAR BatchPath[] = TEXT("RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry");
constexpr int32 RequiredCaptures = 11;
constexpr int32 RequiredActions = 6;
constexpr TCHAR CoordinateAtlasPath[] = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu");
constexpr TCHAR MiddleAtlasPath[] = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");

TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("x"), Value.X);
	Result->SetNumberField(TEXT("y"), Value.Y);
	return Result;
}
TSharedRef<FJsonObject> Point3(const FVector& Value)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("x"), Value.X);
	Result->SetNumberField(TEXT("y"), Value.Y);
	Result->SetNumberField(TEXT("z"), Value.Z);
	return Result;
}
TSharedRef<FJsonObject> Color(const FLinearColor& Value)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("r"), Value.R);
	Result->SetNumberField(TEXT("g"), Value.G);
	Result->SetNumberField(TEXT("b"), Value.B);
	Result->SetNumberField(TEXT("a"), Value.A);
	return Result;
}
FString Encode(const TSharedRef<FJsonObject>& Json)
{
	FString Text;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
	return Text;
}
FString FileSha(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
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
	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("exists"), Widget != nullptr);
	Result->SetBoolField(TEXT("visible"), Visible(Widget));
	Result->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Result->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Result->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	Result->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
	Result->SetObjectField(TEXT("desired_size"), Point(Widget ? Widget->GetDesiredSize() : FVector2D::ZeroVector));
	Result->SetStringField(TEXT("parent_name"), Widget && Widget->GetParent()
		? Widget->GetParent()->GetName() : FString());
	Result->SetObjectField(TEXT("render_pivot"), Point(Widget
		? Widget->GetRenderTransformPivot() : FVector2D::ZeroVector));
	if (const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
	{
		auto Canvas = MakeShared<FJsonObject>();
		Canvas->SetObjectField(TEXT("position"), Point(Slot->GetPosition()));
		Canvas->SetObjectField(TEXT("size"), Point(Slot->GetSize()));
		Canvas->SetObjectField(TEXT("alignment"), Point(Slot->GetAlignment()));
		const FAnchors Anchors = Slot->GetAnchors();
		Canvas->SetObjectField(TEXT("anchors_min"), Point(Anchors.Minimum));
		Canvas->SetObjectField(TEXT("anchors_max"), Point(Anchors.Maximum));
		Result->SetObjectField(TEXT("canvas_slot"), Canvas);
	}
	Result->SetNumberField(TEXT("render_opacity"), Widget ? Widget->GetRenderOpacity() : 0.0);
	Result->SetObjectField(TEXT("render_scale"), Point(Widget ? Widget->GetRenderTransform().Scale : FVector2D::ZeroVector));
	Result->SetObjectField(TEXT("render_translation"), Point(Widget ? Widget->GetRenderTransform().Translation : FVector2D::ZeroVector));
	Result->SetNumberField(TEXT("visibility_enum"), Widget ? static_cast<int32>(Widget->GetVisibility()) : -1);
	if (const UEditableTextBox* Input = Cast<UEditableTextBox>(Widget))
		Result->SetStringField(TEXT("text"), Input->GetText().ToString());
	if (const UTextBlock* Label = Cast<UTextBlock>(Widget))
	{
		Result->SetStringField(TEXT("text"), Label->GetText().ToString());
		Result->SetObjectField(TEXT("shadow_offset"), Point(Label->GetShadowOffset()));
		Result->SetObjectField(TEXT("shadow_color"), Color(Label->GetShadowColorAndOpacity()));
		Result->SetObjectField(TEXT("text_color"), Color(Label->GetColorAndOpacity().GetSpecifiedColor()));
		Result->SetNumberField(TEXT("font_size"), Label->GetFont().Size);
	}
	if (const UBorder* Border = Cast<UBorder>(Widget))
		Result->SetObjectField(TEXT("border_brush_color"), Color(Border->GetBrushColor()));
	if (const UImage* Image = Cast<UImage>(Widget))
	{
		const FSlateBrush& Brush = Image->GetBrush();
		const UObject* Resource = Brush.GetResourceObject();
		const UTexture2D* Texture = Cast<UTexture2D>(Resource);
		const FBox2f Uv = Brush.GetUVRegion();
		Result->SetStringField(TEXT("brush_resource"), Resource ? Resource->GetPathName() : FString());
		Result->SetObjectField(TEXT("uv_min"), Point(FVector2D(Uv.Min.X, Uv.Min.Y)));
		Result->SetObjectField(TEXT("uv_max"), Point(FVector2D(Uv.Max.X, Uv.Max.Y)));
		Result->SetObjectField(TEXT("texture_size"), Point(Texture
			? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D::ZeroVector));
		const FTextureResource* TextureResource = Texture ? Texture->GetResource() : nullptr;
		const FTextureRHIRef Rhi = TextureResource ? TextureResource->TextureRHI : FTextureRHIRef();
		Result->SetBoolField(TEXT("rhi_ready"), Rhi.IsValid());
		Result->SetObjectField(TEXT("rhi_extent"), Point(Rhi.IsValid()
			? FVector2D(Rhi->GetDesc().Extent.X, Rhi->GetDesc().Extent.Y) : FVector2D::ZeroVector));
		const FLinearColor Tint = Image->GetColorAndOpacity();
		Result->SetNumberField(TEXT("tint_alpha"), Tint.A);
	}
	return Result;
}
}

bool URokWorldPositionMiddleTipAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokWorldPositionMiddleTipAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
void URokWorldPositionMiddleTipAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / BatchPath
		/ TEXT("WorldPositionMiddleTipAudit") / FString::Printf(TEXT("%lld_%u"),
			FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokWorldPositionMiddleTipAuditSubsystem::ScreenshotProcessed);
}
void URokWorldPositionMiddleTipAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (RawScreenshotHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(RawScreenshotHandle);
		RawScreenshotHandle = FDelegateHandle();
	}
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	if (bBegun && !bFinished) Finish(TEXT("World MiddleTip audit interrupted by game instance teardown"));
	Super::Deinitialize();
}
bool URokWorldPositionMiddleTipAuditSubsystem::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && !bFinished;
}
TStatId URokWorldPositionMiddleTipAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokWorldPositionMiddleTipAuditSubsystem, STATGROUP_Tickables);
}
UWorld* URokWorldPositionMiddleTipAuditSubsystem::CurrentWorld() const
{
	UGameInstance* Instance = GetGameInstance();
	return Instance ? Instance->GetWorld() : nullptr;
}
bool URokWorldPositionMiddleTipAuditSubsystem::IsCity() const
{
	const UWorld* World = CurrentWorld();
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap;
}
bool URokWorldPositionMiddleTipAuditSubsystem::IsWorldMap() const
{
	const UWorld* World = CurrentWorld();
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == WorldMap;
}
bool URokWorldPositionMiddleTipAuditSubsystem::RefreshWidgets()
{
	UWorld* World = CurrentWorld();
	if (!World) return false;
	TArray<UUserWidget*> Found;
	const auto Find = [&](UClass* Class) -> UUserWidget*
	{
		Found.Reset();
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, Class, false);
		UUserWidget* Result = nullptr;
		int32 Count = 0;
		for (UUserWidget* Widget : Found)
			if (Widget && Widget->GetWorld() == World) { Result = Widget; ++Count; }
		return Count == 1 ? Result : nullptr;
	};
	CityHud.Reset();
	TacticalHud.Reset();
	WorldHud.Reset();
	CityHudRootCount = 0;
	WorldHudRootCount = 0;
	TacticalHudCount = 0;
	UnownedMainHudCount = 0;
	Found.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found,
		URokWorldMapHudWidget::StaticClass(), false);
	int32 WorldHudTotal = 0;
	for (UUserWidget* Widget : Found)
		if (Widget && Widget->GetWorld() == World)
		{
			++WorldHudTotal;
			if (Widget->IsInViewport())
			{
				++WorldHudRootCount;
				WorldHud = Cast<URokWorldMapHudWidget>(Widget);
			}
		}
	if (WorldHudTotal != WorldHudRootCount || WorldHudRootCount != 1)
		WorldHud.Reset();
	Screen = Cast<URokUnityFeatureScreenWidget>(Find(URokUnityFeatureScreenWidget::StaticClass()));
	Found.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found,
		URokMainHudWidget::StaticClass(), false);
	for (UUserWidget* Widget : Found)
		if (Widget && Widget->GetWorld() == World)
		{
			if (Widget->IsInViewport())
			{
				++CityHudRootCount;
				CityHud = Cast<URokMainHudWidget>(Widget);
			}
			else if (WorldHud.IsValid() && Widget->GetFName() == FName(TEXT("WorldMapTacticalHud"))
				&& Widget->GetParent() && WorldHud->GetWidgetFromName(TEXT("WorldMapTacticalHud")) == Widget)
			{
				++TacticalHudCount;
				TacticalHud = Cast<URokMainHudWidget>(Widget);
			}
			else ++UnownedMainHudCount;
		}
	return Screen.IsValid() && UnownedMainHudCount == 0
		&& (IsCity() ? (CityHudRootCount == 1 && WorldHudTotal == 0 && TacticalHudCount == 0)
			: (IsWorldMap() && CityHudRootCount == 0 && WorldHudRootCount == 1
				&& TacticalHudCount == 1));
}
bool URokWorldPositionMiddleTipAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokWorldPositionMiddleTipAuditSubsystem::OwnWindowFocus()
{
#if PLATFORM_WINDOWS
	const UWorld* World = CurrentWorld();
	const UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD Pid = 0;
	if (Handle) ::GetWindowThreadProcessId(Handle, &Pid);
	RECT Client = {};
	if (!Expect(Handle && ::IsWindow(Handle) && Pid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client) && Client.right - Client.left == 1440
		&& Client.bottom - Client.top == 810,
		TEXT("World MiddleTip audit requires own 1440x810 window"))) return false;
	if (::GetForegroundWindow() == Handle) return true;
	if (::IsIconic(Handle)) ::ShowWindow(Handle, SW_RESTORE);
	else if (!::IsWindowVisible(Handle)) ::ShowWindow(Handle, SW_SHOW);
	const HWND Foreground = ::GetForegroundWindow();
	const DWORD Thread = Foreground ? ::GetWindowThreadProcessId(Foreground, nullptr) : 0;
	const DWORD CurrentThread = ::GetCurrentThreadId();
	const bool bAttached = Thread && Thread != CurrentThread
		&& ::AttachThreadInput(CurrentThread, Thread, 1) != 0;
	::BringWindowToTop(Handle);
	::SetForegroundWindow(Handle);
	if (bAttached) ::AttachThreadInput(CurrentThread, Thread, 0);
	return ::GetForegroundWindow() == Handle;
#else
	return Expect(false, TEXT("World MiddleTip audit requires Windows game window"));
#endif
}
TSharedRef<FJsonObject> URokWorldPositionMiddleTipAuditSubsystem::GameBusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Runtime.IsValid() || !Server.IsValid()) return Json;
	const auto Task = Runtime->GetActiveTask();
	const auto Profile = Runtime->GetPlayerProfileState();
	Json->SetNumberField(TEXT("task_id"), Task.TaskId);
	Json->SetNumberField(TEXT("task_current"), Task.Current);
	Json->SetNumberField(TEXT("task_target"), Task.Target);
	Json->SetNumberField(TEXT("task_chain_count"), Runtime->GetActiveTaskChain().Num());
	Json->SetStringField(TEXT("profile_kind"), Profile.StateKind);
	Json->SetStringField(TEXT("role_id"), Profile.RoleId);
	Json->SetBoolField(TEXT("profile_has_role"), Profile.bHasRoleId);
	Json->SetBoolField(TEXT("profile_has_civilization"), Profile.bHasCivilization);
	Json->SetStringField(TEXT("profile_civilization_name"), Profile.CivilizationName);
	Json->SetNumberField(TEXT("civilization_id"), Profile.CivilizationId);
	Json->SetNumberField(TEXT("http_dispatch_attempts"), static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("http_suppressed_requests"), static_cast<double>(Server->GetSuppressedLocalRequestCount()));
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
		if (UGameInstance* Instance = GetGameInstance())
		if (const URokClientSettingsSubsystem* Settings = Instance->GetSubsystem<URokClientSettingsSubsystem>())
		{
			Json->SetNumberField(TEXT("graphics_tier"), static_cast<int32>(Settings->GetGraphicsQualityTier()));
			Json->SetNumberField(TEXT("frame_rate_option"), static_cast<int32>(Settings->GetFrameRateOption()));
			Json->SetNumberField(TEXT("sfx_volume"), Settings->GetSfxVolume());
			Json->SetNumberField(TEXT("music_volume"), Settings->GetMusicVolume());
			auto Toggles = MakeShared<FJsonObject>();
			for (const FRokPersonalityToggleDefinition& Definition : Settings->GetPersonalityToggleDefinitions())
				Toggles->SetBoolField(Definition.Name.ToString(), Settings->GetPersonalityToggle(Definition.Name));
			Json->SetObjectField(TEXT("personality_toggles"), Toggles);
		}
	return Json;
}
TSharedRef<FJsonObject> URokWorldPositionMiddleTipAuditSubsystem::WorldBusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	UWorld* World = CurrentWorld();
	const URokBuildingUpgradeSubsystem* Buildings = World
		? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	Json->SetBoolField(TEXT("available"), Buildings != nullptr);
	if (Buildings)
	{
		const auto Wallet = Buildings->GetCurrentResources();
		Json->SetNumberField(TEXT("wallet_food"), Wallet.Food);
		Json->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
		Json->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
		Json->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
		Json->SetNumberField(TEXT("active_upgrade_count"), Buildings->GetActiveUpgradeCount());
		Json->SetNumberField(TEXT("active_construction_count"), Buildings->GetActiveLocalConstructionCount());
	}
	return Json;
}
bool URokWorldPositionMiddleTipAuditSubsystem::Boundary() const
{
	if (!Server.IsValid() || !Runtime.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0
		|| Server->GetSuppressedLocalRequestCount() != 0
		|| BaselineGameBusiness.IsEmpty()
		|| Encode(GameBusinessState()) != BaselineGameBusiness) return false;
	if (IsCity())
	{
		const UWorld* World = CurrentWorld();
		const ARokPlayerController* Controller = World
			? Cast<ARokPlayerController>(World->GetFirstPlayerController()) : nullptr;
		const ARokStrategyCameraPawn* Camera = Controller
			? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
		return Controller && Camera && !BaselineCityBusiness.IsEmpty()
			&& Encode(WorldBusinessState()) == BaselineCityBusiness
			&& Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == BaselineCityPrimary
			&& Camera->GetActorLocation().Equals(CityCameraLocation, .1f)
			&& Camera->GetActorRotation().Equals(CityCameraRotation, .1f)
			&& FMath::IsNearlyEqual(Camera->GetCurrentCityDxf(), CityCameraDxf, .1f);
	}
	if (IsWorldMap()) return BaselineWorldBusiness.IsEmpty()
		|| Encode(WorldBusinessState()) == BaselineWorldBusiness;
	return false;
}
TSharedRef<FJsonObject> URokWorldPositionMiddleTipAuditSubsystem::State(const TCHAR* GeometryStage) const
{
	auto Json = MakeShared<FJsonObject>();
	const UWorld* World = CurrentWorld();
	const UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	const FGeometry ViewportGeometry = ViewportWidget.IsValid()
		? ViewportWidget->GetCachedGeometry() : FGeometry();
	Json->SetStringField(TEXT("geometry_sample_stage"), GeometryStage);
	Json->SetBoolField(TEXT("viewport_geometry_valid"), ViewportWidget.IsValid()
		&& ViewportGeometry.GetLocalSize().X > 0.0f && ViewportGeometry.GetLocalSize().Y > 0.0f);
	Json->SetObjectField(TEXT("viewport_desktop_origin"), Point(ViewportGeometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("viewport_local_size"), Point(ViewportGeometry.GetLocalSize()));
	Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), World ? World->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetObjectField(TEXT("game_business"), GameBusinessState());
	Json->SetObjectField(TEXT("world_business"), WorldBusinessState());
	Json->SetBoolField(TEXT("city_hud_visible"), CityHud.IsValid() && Visible(CityHud.Get()));
	Json->SetBoolField(TEXT("world_hud_visible"), WorldHud.IsValid() && Visible(WorldHud.Get()));
	Json->SetNumberField(TEXT("city_hud_root_count"), CityHudRootCount);
	Json->SetNumberField(TEXT("world_hud_root_count"), WorldHudRootCount);
	Json->SetNumberField(TEXT("tactical_hud_count"), TacticalHudCount);
	Json->SetNumberField(TEXT("unowned_main_hud_count"), UnownedMainHudCount);
	Json->SetStringField(TEXT("city_hud_root_path"), CityHud.IsValid() ? CityHud->GetPathName() : FString());
	Json->SetStringField(TEXT("world_hud_root_path"), WorldHud.IsValid() ? WorldHud->GetPathName() : FString());
	Json->SetStringField(TEXT("tactical_hud_path"), TacticalHud.IsValid() ? TacticalHud->GetPathName() : FString());
	Json->SetBoolField(TEXT("tactical_hud_visible"), TacticalHud.IsValid() && Visible(TacticalHud.Get()));
	Json->SetBoolField(TEXT("tactical_hud_in_viewport"), TacticalHud.IsValid() && TacticalHud->IsInViewport());
	Json->SetBoolField(TEXT("tactical_owned_by_world_hud"), TacticalHud.IsValid()
		&& WorldHud.IsValid() && WorldHud->GetWidgetFromName(TEXT("WorldMapTacticalHud")) == TacticalHud.Get());
	Json->SetObjectField(TEXT("world_hud_coordinate"), WidgetState(WorldHud.IsValid()
		? WorldHud->GetWidgetFromName(TEXT("WorldMapCoordinateText")) : nullptr));
	const UImage* AtlasImage = WorldHud.IsValid()
		? Cast<UImage>(WorldHud->GetWidgetFromName(TEXT("WorldMapCoordinateBackground_0_0"))) : nullptr;
	const UTexture2D* AtlasTexture = AtlasImage
		? Cast<UTexture2D>(AtlasImage->GetBrush().GetResourceObject()) : nullptr;
	const FTextureResource* AtlasResource = AtlasTexture ? AtlasTexture->GetResource() : nullptr;
	const FTextureRHIRef AtlasRhi = AtlasResource ? AtlasResource->TextureRHI : FTextureRHIRef();
	auto Atlas = MakeShared<FJsonObject>();
	Atlas->SetStringField(TEXT("object_path"), AtlasTexture ? AtlasTexture->GetPathName() : FString());
	Atlas->SetObjectField(TEXT("source_size"), Point(AtlasTexture
		? FVector2D(AtlasTexture->GetSizeX(), AtlasTexture->GetSizeY()) : FVector2D::ZeroVector));
	Atlas->SetObjectField(TEXT("rhi_extent"), Point(AtlasRhi.IsValid()
		? FVector2D(AtlasRhi->GetDesc().Extent.X, AtlasRhi->GetDesc().Extent.Y) : FVector2D::ZeroVector));
	Atlas->SetBoolField(TEXT("rhi_ready"), AtlasRhi.IsValid());
	Json->SetObjectField(TEXT("world_coordinate_atlas"), Atlas);
	Json->SetNumberField(TEXT("world_ready_since_seconds"), WorldMapReadySince);
	Json->SetNumberField(TEXT("audit_elapsed_seconds"), Elapsed);
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	Json->SetBoolField(TEXT("search_controls_visible"), Screen.IsValid()
		&& Visible(Screen->GetWidgetFromName(TEXT("WorldPositionXInput")))
		&& Visible(Screen->GetWidgetFromName(TEXT("WorldPositionYInput")))
		&& Visible(Screen->GetWidgetFromName(TEXT("WorldPositionGo"))));
	Json->SetNumberField(TEXT("expected_step"), Step);
	const FTextureResource* PreparedResource = PreparedMiddleAtlas
		? PreparedMiddleAtlas->GetResource() : nullptr;
	const FTextureRHIRef PreparedRhi = PreparedResource
		? PreparedResource->TextureRHI : FTextureRHIRef();
	auto Prepared = MakeShared<FJsonObject>();
	Prepared->SetStringField(TEXT("object_path"), PreparedMiddleAtlas
		? PreparedMiddleAtlas->GetPathName() : FString());
	Prepared->SetBoolField(TEXT("rhi_ready"), PreparedRhi.IsValid());
	Prepared->SetObjectField(TEXT("rhi_extent"), Point(PreparedRhi.IsValid()
		? FVector2D(PreparedRhi->GetDesc().Extent.X, PreparedRhi->GetDesc().Extent.Y)
		: FVector2D::ZeroVector));
	Json->SetObjectField(TEXT("prepared_middle_atlas"), Prepared);
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
		Json->SetStringField(TEXT("keyboard_focused_type"), Focused.IsValid()
			? Focused->GetTypeAsString() : FString());
	}
	if (Screen.IsValid())
	{
		auto Widgets = MakeShared<FJsonObject>();
		const TCHAR* Names[] = {TEXT("WorldPositionXInput"), TEXT("WorldPositionYInput"),
			TEXT("WorldPositionGo"), TEXT("WorldPositionKingdomValue"),
			TEXT("FeatureBoundaryFeedback"), TEXT("FeatureFrameClose")};
		for (const TCHAR* Name : Names)
			Widgets->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(Name)));
		Json->SetObjectField(TEXT("widgets"), Widgets);
		auto Tip = MakeShared<FJsonObject>();
		const UWidget* Root = Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipRoot"));
		const UWidget* Background = Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipBackground"));
		const UWidget* TextHost = Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipTextHost"));
		const UWidget* Text = Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipText"));
		Tip->SetStringField(TEXT("animated_group_widget_name"), TEXT("WorldPositionMiddleTipRoot"));
		Tip->SetBoolField(TEXT("same_animated_group"), Root && Background && Text
			&& Background->GetParent() == Root
			&& (Text->GetParent() == Root || (TextHost && TextHost->GetParent() == Root
				&& Text->GetParent() == TextHost)));
		Tip->SetObjectField(TEXT("root"), WidgetState(Root));
		Tip->SetObjectField(TEXT("background"), WidgetState(Background));
		Tip->SetObjectField(TEXT("text_host"), WidgetState(TextHost));
		Tip->SetObjectField(TEXT("text"), WidgetState(Text));
		Tip->SetNumberField(TEXT("trigger_frame"), static_cast<double>(TipTriggerFrame));
		Tip->SetNumberField(TEXT("trigger_monotonic_seconds"), TipTriggerMonotonicSeconds);
		Tip->SetNumberField(TEXT("seconds_since_trigger"), TipTriggerElapsedSeconds >= 0.0
			? Elapsed - TipTriggerElapsedSeconds : -1.0);
		Tip->SetNumberField(TEXT("monotonic_seconds_since_trigger"), TipTriggerMonotonicSeconds >= 0.0
			? FPlatformTime::Seconds() - TipTriggerMonotonicSeconds : -1.0);
		Tip->SetNumberField(TEXT("ui_animation_elapsed_seconds"), TipUiElapsed());
		Tip->SetStringField(TEXT("source_curve_clock"), TEXT("WorldPositionMiddleTipElapsedSeconds_NativeTick"));
		Json->SetObjectField(TEXT("middle_tip"), Tip);
	}
	int32 ConfigCount = 0;
	const ARokWorldMapConfigActor* Config = nullptr;
	if (World)
		for (TActorIterator<ARokWorldMapConfigActor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			Config = *It;
			++ConfigCount;
		}
	const FRokWorldMapProjection Projection = Config ? Config->Projection : FRokWorldMapProjection();
	auto Projected = MakeShared<FJsonObject>();
	Projected->SetStringField(TEXT("source"), Config ? TEXT("world_config_actor") : TEXT("local_projection_fallback"));
	Projected->SetNumberField(TEXT("config_actor_count"), ConfigCount);
	Projected->SetStringField(TEXT("config_actor_path"), Config ? Config->GetPathName() : FString());
	Projected->SetBoolField(TEXT("valid"), Projection.IsValid());
	Projected->SetObjectField(TEXT("unity_min"), Point(Projection.UnityBoundsMin));
	Projected->SetObjectField(TEXT("unity_max"), Point(Projection.UnityBoundsMax));
	Projected->SetNumberField(TEXT("display_scale"), Projection.DisplayCoordinateScale);
	Projected->SetObjectField(TEXT("display_min"), Point(Projection.UnityToDisplay(Projection.UnityBoundsMin)));
	Projected->SetObjectField(TEXT("display_max"), Point(Projection.UnityToDisplay(Projection.UnityBoundsMax)));
	Json->SetObjectField(TEXT("projection"), Projected);
	auto Streaming = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> StreamingActorPaths;
	TArray<TSharedPtr<FJsonValue>> RuntimeDataPaths;
	if (World)
		for (TActorIterator<ARokWorldMapStreamingActor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			StreamingActorPaths.Add(MakeShared<FJsonValueString>(It->GetPathName()));
			if (const URokWorldMapRuntimeDataAsset* Data = It->RuntimeData.Get())
				RuntimeDataPaths.Add(MakeShared<FJsonValueString>(Data->GetPathName()));
		}
	Streaming->SetNumberField(TEXT("actor_count"), StreamingActorPaths.Num());
	Streaming->SetArrayField(TEXT("actor_paths"), StreamingActorPaths);
	Streaming->SetArrayField(TEXT("loaded_runtime_data_asset_paths"), RuntimeDataPaths);
	Json->SetObjectField(TEXT("streaming_dependency"), Streaming);
	if (World)
	{
		const APlayerController* Controller = World->GetFirstPlayerController();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		Json->SetStringField(TEXT("pawn_class"), Pawn ? Pawn->GetClass()->GetPathName() : FString());
		if (const ARokPlayerController* CityController = Cast<ARokPlayerController>(Controller))
			Json->SetNumberField(TEXT("city_primary_press_count"), static_cast<double>(
				CityController->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
		if (const ARokStrategyCameraPawn* CityCamera = Cast<ARokStrategyCameraPawn>(Pawn))
		{
			Json->SetObjectField(TEXT("city_camera_location"), Point3(CityCamera->GetActorLocation()));
			Json->SetObjectField(TEXT("city_camera_rotation"), Point3(CityCamera->GetActorRotation().Euler()));
			Json->SetNumberField(TEXT("city_camera_dxf"), CityCamera->GetCurrentCityDxf());
		}
		if (const ARokWorldMapCameraPawn* Camera = Cast<ARokWorldMapCameraPawn>(Pawn))
		{
			Json->SetObjectField(TEXT("view_center_display"), Point(Camera->GetViewCenterDisplay()));
			Json->SetObjectField(TEXT("view_center_unity"), Point(Camera->GetViewCenterUnity()));
			Json->SetNumberField(TEXT("world_camera_dxf"), Camera->GetCurrentDxf());
		}
	}
	return Json;
}
bool URokWorldPositionMiddleTipAuditSubsystem::Begin()
{
	if (!PreparedMiddleAtlas)
		PreparedMiddleAtlas = LoadObject<UTexture2D>(nullptr, MiddleAtlasPath);
	const FTextureResource* PreparedResource = PreparedMiddleAtlas
		? PreparedMiddleAtlas->GetResource() : nullptr;
	const FTextureRHIRef PreparedRhi = PreparedResource
		? PreparedResource->TextureRHI : FTextureRHIRef();
	if (!PreparedMiddleAtlas || PreparedMiddleAtlas->GetPathName() != MiddleAtlasPath
		|| !PreparedRhi.IsValid() || PreparedRhi->GetDesc().Extent.X != 2048
		|| PreparedRhi->GetDesc().Extent.Y != 1024) return false;
	if (!IsCity() || !FSlateApplication::IsInitialized() || !RefreshWidgets()
		|| !CityHud.IsValid() || !Screen.IsValid() || Screen->IsScreenOpen()
		|| !Visible(CityHud.Get()) || !Visible(CityHud->GetWidgetFromName(TEXT("PositionSearchButton"))))
		return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	while (FParse::Token(Cursor, Token, false))
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("-RokWorldPositionMiddleTipAudit"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("World MiddleTip audit cannot share another Rok automation flag"));
	UGameInstance* Instance = GetGameInstance();
	Runtime = Instance ? Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	Server = Instance ? Instance->GetSubsystem<URokServerSubsystem>() : nullptr;
	UWorld* World = CurrentWorld();
	const ARokPlayerController* Controller = World
		? Cast<ARokPlayerController>(World->GetFirstPlayerController()) : nullptr;
	const ARokStrategyCameraPawn* Camera = Controller
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	if (!Expect(Runtime.IsValid() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
		&& Controller && Camera && FVector2D(1440, 810).Equals(
			UWidgetLayoutLibrary::GetViewportSize(this), .1f),
		TEXT("World MiddleTip audit requires idle local-only 1440x810 city HUD"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId
		&& Profile.RoleId.IsEmpty() && !Profile.bHasCivilization
		&& Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("World MiddleTip audit cannot use real account identity"))) return false;
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	BaselineCityPrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	CityCameraLocation = Camera->GetActorLocation();
	CityCameraRotation = Camera->GetActorRotation();
	CityCameraDxf = Camera->GetCurrentCityDxf();
	BaselineGameBusiness = Encode(GameBusinessState());
	BaselineCityBusiness = Encode(WorldBusinessState());
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokWorldPositionMiddleTipAuditSubsystem.h"),
		TEXT("Source/Rok/RokWorldPositionMiddleTipAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokWorldPositionMiddleTipRuntime.py"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokWorldMapHudWidget.cpp"),
		TEXT("Source/Rok/RokStrategyCameraPawn.cpp"),
		TEXT("Source/Rok/RokWorldMapCameraPawn.cpp"),
		TEXT("Source/Rok/RokWorldMapTypes.h"),
		TEXT("Source/Rok/RokWorldMapTypes.cpp"),
		TEXT("Source/Rok/RokWorldMapConfigActor.h"),
		TEXT("Source/Rok/RokWorldMapConfigActor.cpp"),
		TEXT("Source/Rok/RokWorldMapStreamingActor.h"),
		TEXT("Source/Rok/RokWorldMapStreamingActor.cpp"),
		TEXT("Source/Rok/RokWorldMapRuntimeDataAsset.h"),
		TEXT("Source/Rok/RokWorldMapRuntimeDataAsset.cpp"),
		TEXT("Source/Rok/RokServerSubsystem.h"),
		TEXT("Source/Rok/RokServerSubsystem.cpp"),
		TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"),
		TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"),
		TEXT("ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/Tip.cs"),
		TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/AutoPlayAndDestroyTip.cs"),
		TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Common_MidTips.prefab"),
		TEXT("ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UA_Common_TipInMid_On.anim"),
		TEXT("ref/client-unity/Assets/StreamingAssets/Config/Bin/Config.bin"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry/root_middle_tip_integration_v1.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009MiddleTipAndTileParticleGeometry/MiddleTipSourceReferenceV1/source_hierarchy_finding_v1.json"),
		TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"),
		TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset")};
	for (const TCHAR* Path : Required)
	{
		const FString Hash = FileSha(FPaths::ProjectDir() / Path);
		if (!Expect(Hash.Len() == 64, TEXT("World MiddleTip source input missing"))) return false;
		Inputs->SetStringField(Path, Hash);
	}
	if (!OwnWindowFocus() || !Boundary()) return false;
	const IConsoleVariable* ScreenshotDelegate = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenshotDelegate"));
	if (!Expect(ScreenshotDelegate && ScreenshotDelegate->GetInt() == 1
		&& !UGameViewportClient::OnScreenshotCaptured().IsBound(),
		TEXT("World MiddleTip requires exclusive enabled GameViewport screenshot capture delegate"))) return false;
	RawScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(
		this, &URokWorldPositionMiddleTipAuditSubsystem::ScreenshotCaptured);
	return Expect(RawScreenshotHandle.IsValid(), TEXT("World MiddleTip screenshot capture delegate binding failed"));
}
bool URokWorldPositionMiddleTipAuditSubsystem::ReadySearch() const
{
	return Screen.IsValid() && Screen->IsScreenOpen()
		&& Visible(Screen->GetWidgetFromName(TEXT("WorldPositionXInput")))
		&& Visible(Screen->GetWidgetFromName(TEXT("WorldPositionYInput")))
		&& Visible(Screen->GetWidgetFromName(TEXT("WorldPositionGo")));
}
bool URokWorldPositionMiddleTipAuditSubsystem::StableSearch()
{
	StableFrames = ReadySearch() && Boundary() ? StableFrames + 1 : 0;
	return StableFrames >= 2;
}
double URokWorldPositionMiddleTipAuditSubsystem::TipUiElapsed() const
{
#if !UE_BUILD_SHIPPING
	return Screen.IsValid() ? static_cast<double>(Screen->GetWorldPositionMiddleTipElapsedSecondsForAudit()) : -1.0;
#else
	return -1.0;
#endif
}
bool URokWorldPositionMiddleTipAuditSubsystem::TipReady() const
{
	if (!ReadySearch() || !Screen.IsValid()) return false;
	const UWidget* Root = Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipRoot"));
	const UImage* Background = Cast<UImage>(Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipBackground")));
	const UWidget* TextHost = Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipTextHost"));
	const UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipText")));
	const UTexture2D* Texture = Background ? Cast<UTexture2D>(Background->GetBrush().GetResourceObject()) : nullptr;
	const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
	const FTextureRHIRef Rhi = Resource ? Resource->TextureRHI : FTextureRHIRef();
	return Root && Background && Text
		&& Background->GetParent() == Root
		&& (Text->GetParent() == Root || (TextHost && TextHost->GetParent() == Root
			&& Text->GetParent() == TextHost))
		&& Root->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& Visible(Root) && Visible(Background) && Visible(Text)
		&& Text->GetText().ToString() == TEXT("未输入坐标")
		&& Texture && Texture->GetPathName() == MiddleAtlasPath
		&& Texture->GetSizeX() == 2048 && Texture->GetSizeY() == 1024
		&& Rhi.IsValid() && Rhi->GetDesc().Extent.X == 2048
		&& Rhi->GetDesc().Extent.Y == 1024;
}
bool URokWorldPositionMiddleTipAuditSubsystem::TipCollapsed() const
{
	const UWidget* Root = Screen.IsValid()
		? Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipRoot")) : nullptr;
	return ReadySearch() && Root && Root->GetVisibility() == ESlateVisibility::Collapsed
		&& FMath::IsNearlyZero(Root->GetRenderOpacity(), .001f);
}
bool URokWorldPositionMiddleTipAuditSubsystem::AtWorldCenter(float X, float Y) const
{
	const UWorld* World = CurrentWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const ARokWorldMapCameraPawn* Camera = Controller
		? Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()) : nullptr;
	if (!IsWorldMap() || !Camera) return false;
	const FVector2D Display = Camera->GetViewCenterDisplay();
	const FVector2D Unity = Camera->GetViewCenterUnity();
	return Display.Equals(FVector2D(X, Y), .1f)
		&& Unity.Equals(FVector2D(X * 6.0f, Y * 6.0f), .6f);
}
bool URokWorldPositionMiddleTipAuditSubsystem::WorldHudCoordinateMatchesCamera() const
{
	const UWorld* World = CurrentWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const ARokWorldMapCameraPawn* Camera = Controller
		? Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()) : nullptr;
	const UTextBlock* Label = WorldHud.IsValid()
		? Cast<UTextBlock>(WorldHud->GetWidgetFromName(TEXT("WorldMapCoordinateText"))) : nullptr;
	if (!Camera || !Visible(Label)) return false;
	const FVector2D Display = Camera->GetViewCenterDisplay();
	return Label->GetText().ToString() == FString::Printf(TEXT("#1   X:%d Y:%d"),
		FMath::FloorToInt(Display.X), FMath::FloorToInt(Display.Y));
}
bool URokWorldPositionMiddleTipAuditSubsystem::WorldCoordinateAtlasReady() const
{
	const UImage* Image = WorldHud.IsValid()
		? Cast<UImage>(WorldHud->GetWidgetFromName(TEXT("WorldMapCoordinateBackground_0_0"))) : nullptr;
	const UTexture2D* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
	const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
	const FTextureRHIRef TextureRhi = Resource ? Resource->TextureRHI : FTextureRHIRef();
	return Visible(Image) && Texture && Texture->GetPathName() == CoordinateAtlasPath
		&& Texture->GetSizeX() == 1024 && Texture->GetSizeY() == 1024
		&& TextureRhi.IsValid() && TextureRhi->GetDesc().Extent.X == 1024
		&& TextureRhi->GetDesc().Extent.Y == 1024;
}
bool URokWorldPositionMiddleTipAuditSubsystem::RetryInput(const TCHAR* Kind, const TCHAR* Selector,
	const FVector2D& Position, const TArray<TSharedPtr<FJsonValue>>& PathTypes,
	const FString& Terminal, bool bFocus, bool bOwn, bool bForeground,
	bool bTarget, bool bHovered)
{
	const FString Key = FString(Kind) + TEXT(":") + Selector;
	if (RetryKey != Key) { RetryKey = Key; RetryCount = 0; }
	++RetryCount;
	auto Attempt = MakeShared<FJsonObject>();
	Attempt->SetStringField(TEXT("kind"), Kind);
	Attempt->SetStringField(TEXT("selector"), Selector);
	Attempt->SetNumberField(TEXT("attempt"), RetryCount);
	Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Attempt->SetObjectField(TEXT("absolute_point"), Point(Position));
	Attempt->SetArrayField(TEXT("hit_path_types"), PathTypes);
	Attempt->SetStringField(TEXT("terminal_type"), Terminal);
	Attempt->SetBoolField(TEXT("focus_prepared"), bFocus);
	Attempt->SetBoolField(TEXT("own_game_window"), bOwn);
	Attempt->SetBoolField(TEXT("foreground"), bForeground);
	Attempt->SetBoolField(TEXT("target_in_hit_path"), bTarget);
	Attempt->SetBoolField(TEXT("target_hovered"), bHovered);
	InputAttempts.Add(MakeShared<FJsonValueObject>(Attempt));
	if (RetryCount >= 30)
		Finish(FString::Printf(TEXT("World MiddleTip %s %s hit path did not stabilize after 30 pre-input attempts"),
			Kind, Selector));
	return false;
}
bool URokWorldPositionMiddleTipAuditSubsystem::Click(UUserWidget* Owner, const TCHAR* Selector)
{
	UButton* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Selector)) : nullptr;
	if (!Expect(Boundary() && Owner && Owner->GetWorld() == CurrentWorld()
		&& Visible(Button) && Button->GetIsEnabled() && FSlateApplication::IsInitialized(),
		TEXT("World MiddleTip Slate button unavailable"))) return false;
	const FGeometry Geometry = Button->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("World MiddleTip button geometry absent"))) return false;
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
	const bool bFocus = OwnWindowFocus();
	if (bFinished) return false;
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center,
		App.GetInteractiveTopLevelWindows(), false, 0);
	TArray<TSharedPtr<FJsonValue>> Types;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
		Types.Add(MakeShared<FJsonValueString>(Path.Widgets[Index].Widget->GetTypeAsString()));
	const FString Terminal = Path.IsValid() && Path.Widgets.Num()
		? Path.Widgets.Last().Widget->GetTypeAsString() : FString();
	const UGameViewportClient* Viewport = CurrentWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	const bool bTarget = Path.IsValid() && Path.ContainsWidget(&Button->TakeWidget().Get())
		&& Terminal.Contains(TEXT("SButton"));
	if (!(bFocus && bOwn && bForeground && bTarget && Button->IsHovered()))
		return RetryInput(TEXT("button"), Selector, Center, Types, Terminal,
			bFocus, bOwn, bForeground, bTarget, Button->IsHovered());
	RetryKey.Empty(); RetryCount = 0;
	const UEditableTextBox* X = Screen.IsValid()
		? Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("WorldPositionXInput"))) : nullptr;
	const UEditableTextBox* Y = Screen.IsValid()
		? Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("WorldPositionYInput"))) : nullptr;
	const FString BeforeX = X ? X->GetText().ToString() : FString();
	const FString BeforeY = Y ? Y->GetText().ToString() : FString();
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("kind"), TEXT("button"));
	Action->SetStringField(TEXT("selector"), Selector);
	Action->SetStringField(TEXT("method"), TEXT("Slate mouse down/up"));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetNumberField(TEXT("dispatch_monotonic_seconds"), FPlatformTime::Seconds());
	Action->SetObjectField(TEXT("absolute_center"), Point(Center));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetStringField(TEXT("terminal_type"), Terminal);
	Action->SetBoolField(TEXT("own_game_window"), bOwn);
	Action->SetBoolField(TEXT("foreground"), bForeground);
	Action->SetBoolField(TEXT("target_in_hit_path"), bTarget);
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetStringField(TEXT("x_before"), BeforeX);
	Action->SetStringField(TEXT("y_before"), BeforeY);
	Action->SetStringField(TEXT("x_after_dispatch"), IsValid(X) ? X->GetText().ToString() : FString());
	Action->SetStringField(TEXT("y_after_dispatch"), IsValid(Y) ? Y->GetText().ToString() : FString());
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Down && Up && Server.IsValid()
		&& Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0,
		TEXT("World MiddleTip button unhandled or crossed local-only boundary"));
}
bool URokWorldPositionMiddleTipAuditSubsystem::Edit(const TCHAR* Selector, const FString& Text)
{
	UEditableTextBox* Input = Screen.IsValid()
		? Cast<UEditableTextBox>(Screen->GetWidgetFromName(Selector)) : nullptr;
	if (!Expect(ReadySearch() && Boundary() && Input && Visible(Input)
		&& Input->GetIsEnabled() && FSlateApplication::IsInitialized(),
		TEXT("World MiddleTip text input unavailable"))) return false;
	const FGeometry Geometry = Input->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("World MiddleTip text input geometry absent"))) return false;
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
	const bool bFocus = OwnWindowFocus();
	if (bFinished) return false;
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer,
		TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center,
		App.GetInteractiveTopLevelWindows(), false, 0);
	TArray<TSharedPtr<FJsonValue>> Types;
	bool bTextPath = false;
	FString Terminal;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		bTextPath |= Type.Contains(TEXT("SEditableText"));
		Terminal = Type;
	}
	const UGameViewportClient* Viewport = CurrentWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	const bool bTarget = Path.IsValid() && Path.ContainsWidget(&Input->TakeWidget().Get()) && bTextPath;
	if (!(bFocus && bOwn && bForeground && bTarget && Input->IsHovered()))
		return RetryInput(TEXT("edit"), Selector, Center, Types, Terminal,
			bFocus, bOwn, bForeground, bTarget, Input->IsHovered());
	RetryKey.Empty(); RetryCount = 0;
	const FString Before = Input->GetText().ToString();
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const TSharedPtr<SWidget> Focused = App.GetKeyboardFocusedWidget();
	const FString FocusedType = Focused.IsValid() ? Focused->GetTypeAsString() : FString();
	if (!Expect(Down && Up && FocusedType.Contains(TEXT("EditableText")),
		TEXT("World MiddleTip text input did not gain real Slate keyboard focus"))) return false;
	const FModifierKeysState NoMods;
	const FModifierKeysState Control(false, false, true, false, false, false, false, false, false);
	App.ProcessKeyDownEvent(FKeyEvent(EKeys::LeftControl, Control, 0, false, 0, 0));
	const bool SelectAll = App.ProcessKeyDownEvent(FKeyEvent(EKeys::A, Control, 0, false, 0, 0));
	App.ProcessKeyUpEvent(FKeyEvent(EKeys::A, Control, 0, false, 0, 0));
	App.ProcessKeyUpEvent(FKeyEvent(EKeys::LeftControl, NoMods, 0, false, 0, 0));
	const bool Erase = App.ProcessKeyDownEvent(FKeyEvent(EKeys::BackSpace, NoMods, 0, false, 0, 0));
	App.ProcessKeyUpEvent(FKeyEvent(EKeys::BackSpace, NoMods, 0, false, 0, 0));
	const FString AfterErase = Input->GetText().ToString();
	const bool bEmptyNoop = Before.IsEmpty() && !Erase && AfterErase.IsEmpty();
	bool bAllChars = true;
	for (TCHAR Character : Text)
		bAllChars &= App.ProcessKeyCharEvent(FCharacterEvent(Character, NoMods, 0, false));
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("kind"), TEXT("edit"));
	Action->SetStringField(TEXT("selector"), Selector);
	Action->SetStringField(TEXT("method"), TEXT("Slate focus click, Ctrl+A, Backspace, keyboard characters"));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetObjectField(TEXT("absolute_center"), Point(Center));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetStringField(TEXT("keyboard_focused_type"), FocusedType);
	Action->SetBoolField(TEXT("own_game_window"), bOwn);
	Action->SetBoolField(TEXT("foreground"), bForeground);
	Action->SetBoolField(TEXT("target_in_hit_path"), bTarget);
	Action->SetBoolField(TEXT("focus_down_handled"), Down);
	Action->SetBoolField(TEXT("focus_up_handled"), Up);
	Action->SetBoolField(TEXT("select_all_handled"), SelectAll);
	Action->SetBoolField(TEXT("backspace_handled"), Erase);
	Action->SetBoolField(TEXT("backspace_empty_noop"), bEmptyNoop);
	Action->SetBoolField(TEXT("all_characters_handled"), bAllChars);
	Action->SetStringField(TEXT("text_before"), Before);
	Action->SetStringField(TEXT("text_after_erase"), AfterErase);
	Action->SetStringField(TEXT("requested_text"), Text);
	Action->SetStringField(TEXT("text_after"), Input->GetText().ToString());
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(SelectAll && AfterErase.IsEmpty() && (Erase || bEmptyNoop)
		&& bAllChars && Input->GetText().ToString() == Text
		&& Boundary(), TEXT("World MiddleTip real keyboard edit failed or changed business state"));
}
void URokWorldPositionMiddleTipAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep;
	StepSince = Elapsed;
	StableFrames = 0;
	RetryKey.Empty();
	RetryCount = 0;
}
void URokWorldPositionMiddleTipAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(RawScreenshotHandle.IsValid() && !Pending.IsValid()
		&& !FScreenshotRequest::IsScreenshotRequested()
		&& Boundary() && (!IsWorldMap() || (WorldCoordinateAtlasReady()
			&& WorldHudCoordinateMatchesCamera() && WorldMapReadySince >= 0.0
			&& Elapsed - WorldMapReadySince >= 2.0)),
		TEXT("World MiddleTip screenshot request before natural world HUD/atlas readiness"))) return;
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetObjectField(TEXT("requested_state"), State(TEXT("request_before_paint")));
	Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	Pending->SetNumberField(TEXT("requested_seconds_since_tip"), TipTriggerElapsedSeconds >= 0.0
		? Elapsed - TipTriggerElapsedSeconds : -1.0);
	Pending->SetNumberField(TEXT("requested_ui_animation_elapsed_seconds"), TipUiElapsed());
	Pending->SetNumberField(TEXT("requested_monotonic_seconds_since_tip"), TipTriggerMonotonicSeconds >= 0.0
		? FPlatformTime::Seconds() - TipTriggerMonotonicSeconds : -1.0);
	Pending->SetNumberField(TEXT("tip_trigger_frame"), static_cast<double>(TipTriggerFrame));
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	Pending->SetStringField(TEXT("png"), PendingPath);
	NextStep = After;
	bScreenshotDone = false;
	PendingRaw = FRawScreenshot();
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}
void URokWorldPositionMiddleTipAuditSubsystem::ScreenshotCaptured(
	int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	if (bFinished || !Pending.IsValid() || PendingRaw.Pixels.Num() != 0)
	{
		Failures.Add(TEXT("World MiddleTip unsolicited or duplicate screenshot capture delegate"));
		return;
	}
	if (Width != 1440 || Height != 810 || Colors.Num() != Width * Height)
	{
		Failures.Add(TEXT("World MiddleTip capture delegate extent/pixel count differs"));
		return;
	}
	// This is the real Slate paint's FColor array. Copy only; PNG and hashing wait until Finish.
	PendingRaw.Width = Width;
	PendingRaw.Height = Height;
	PendingRaw.Frame = GFrameCounter;
	PendingRaw.CapturedMonotonicSeconds = FPlatformTime::Seconds();
	PendingRaw.Pixels = Colors;
}
void URokWorldPositionMiddleTipAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	if (!Expect(PendingRaw.Width == 1440 && PendingRaw.Height == 810
		&& PendingRaw.Pixels.Num() == 1440 * 810
		&& PendingRaw.Frame >= static_cast<uint64>(Pending->GetNumberField(TEXT("requested_frame")))
		&& PendingRaw.Frame <= GFrameCounter && Boundary(),
		TEXT("World MiddleTip raw Slate paint pixels or source state absent at processed frame"))) return;
	Pending->SetStringField(TEXT("capture_delegate"), TEXT("UGameViewportClient::OnScreenshotCaptured"));
	Pending->SetStringField(TEXT("capture_pixel_format"), TEXT("FColor_BGRA8_top_left_opaque"));
	Pending->SetNumberField(TEXT("capture_width"), PendingRaw.Width);
	Pending->SetNumberField(TEXT("capture_height"), PendingRaw.Height);
	Pending->SetNumberField(TEXT("capture_pixel_count"), PendingRaw.Pixels.Num());
	Pending->SetNumberField(TEXT("capture_frame"), static_cast<double>(PendingRaw.Frame));
	Pending->SetNumberField(TEXT("capture_monotonic_seconds"), PendingRaw.CapturedMonotonicSeconds);
	Pending->SetObjectField(TEXT("processed_state"), State(TEXT("screenshot_processed_after_paint")));
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokWorldPositionMiddleTipAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	Pending->SetObjectField(TEXT("next_frame_state"), State(TEXT("next_frame_before_paint")));
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	RawScreenshots.Add(MoveTemp(PendingRaw));
	PendingRaw = FRawScreenshot();
	Pending.Reset();
	// Intermediate JSON serialization would also delay the next source-clock sample.
	// The final manifest is written after all natural paint samples have been cached.
	Enter(NextStep);
}
bool URokWorldPositionMiddleTipAuditSubsystem::WriteDeferredScreenshots()
{
	if (RawScreenshots.Num() != Captures.Num())
	{
		Failures.Add(TEXT("World MiddleTip raw screenshot/capture count differs before deferred PNG"));
		return false;
	}
	static_assert(sizeof(FColor) == 4, "FColor raw layout must be four bytes");
	for (int32 Index = 0; Index < RawScreenshots.Num(); ++Index)
	{
		const FRawScreenshot& Raw = RawScreenshots[Index];
		const TSharedPtr<FJsonObject> CaptureJson = Captures[Index]->AsObject();
		if (!CaptureJson.IsValid() || Raw.Width != 1440 || Raw.Height != 810
			|| Raw.Pixels.Num() != Raw.Width * Raw.Height
			|| Raw.Frame != static_cast<uint64>(CaptureJson->GetNumberField(TEXT("capture_frame"))))
		{
			Failures.Add(TEXT("World MiddleTip deferred source pixel identity differs"));
			return false;
		}
		for (const FColor& Pixel : Raw.Pixels)
		{
			if (Pixel.A != 255)
			{
				Failures.Add(TEXT("World MiddleTip captured screenshot alpha not opaque"));
				return false;
			}
		}
		const FString PngPath = CaptureJson->GetStringField(TEXT("png"));
		if (IFileManager::Get().FileExists(*PngPath))
		{
			Failures.Add(TEXT("World MiddleTip deferred PNG path already exists"));
			return false;
		}
		const int64 RawSize = static_cast<int64>(Raw.Pixels.Num()) * sizeof(FColor);
		TArray<uint8> RawBytes;
		RawBytes.Append(reinterpret_cast<const uint8*>(Raw.Pixels.GetData()), RawSize);
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Raw.Width, Raw.Height,
			TArrayView64<const FColor>(Raw.Pixels.GetData(), Raw.Pixels.Num()), Png);
		if (Png.Num() <= 4096 || !FFileHelper::SaveArrayToFile(Png, *PngPath))
		{
			Failures.Add(TEXT("World MiddleTip deferred PNG compression/write failed"));
			return false;
		}
		CaptureJson->SetStringField(TEXT("raw_bgra_sha256"), RokTrainArmyMesh::Sha256(RawBytes));
		CaptureJson->SetStringField(TEXT("png_sha256"), FileSha(PngPath));
		CaptureJson->SetNumberField(TEXT("png_bytes"), static_cast<double>(Png.Num()));
		CaptureJson->SetStringField(TEXT("png_encoder"), TEXT("FImageUtils::PNGCompressImageArray_BGRA8"));
		CaptureJson->SetStringField(TEXT("png_write_phase"), TEXT("Finish_after_all_11_paint_captures"));
		CaptureJson->SetNumberField(TEXT("png_write_monotonic_seconds"), FPlatformTime::Seconds());
	}
	bDeferredPngComplete = RawScreenshots.Num() == RequiredCaptures;
	return bDeferredPngComplete;
}
bool URokWorldPositionMiddleTipAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.world_position_middle_tip.runtime_audit.v4"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty()
		&& Captures.Num() == RequiredCaptures && Actions.Num() == RequiredActions);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), TEXT("Local-only real city HUD empty-coordinate MiddleTip timeline and valid city-to-world search through Slate; no account, HTTP or direct camera setter"));
	Json->SetStringField(TEXT("screenshot_capture_delegate"), TEXT("UGameViewportClient::OnScreenshotCaptured"));
	Json->SetStringField(TEXT("png_deferred_until"), TEXT("Finish_after_all_11_paint_captures"));
	Json->SetBoolField(TEXT("deferred_png_complete"), bDeferredPngComplete);
	Json->SetNumberField(TEXT("cached_raw_frames"), RawScreenshots.Num());
	Json->SetNumberField(TEXT("screenshot_delegate_cvar"), 1);
	Json->SetNumberField(TEXT("tip_trigger_frame"), static_cast<double>(TipTriggerFrame));
	Json->SetNumberField(TEXT("tip_trigger_monotonic_seconds"), TipTriggerMonotonicSeconds);
	Json->SetNumberField(TEXT("tip_trigger_audit_elapsed_seconds"), TipTriggerElapsedSeconds);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid()
		? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("input_attempts"), InputAttempts);
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetObjectField(TEXT("final_state"), State(TEXT("final_state")));
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokWorldPositionMiddleTipAuditSubsystem::Finish(const FString& Reason)
{
	if (bFinished) return;
	if (!Reason.IsEmpty()) Failures.Add(Reason);
	bFinished = true;
	if (RawScreenshotHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(RawScreenshotHandle);
		RawScreenshotHandle = FDelegateHandle();
	}
	if (Captures.Num() > 0) WriteDeferredScreenshots();
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && bDeferredPngComplete
		&& IsWorldMap() && Boundary()
		&& Captures.Num() == RequiredCaptures && Actions.Num() == RequiredActions
		&& Screen.IsValid() && !Screen->IsScreenOpen()
		&& AtWorldCenter(100.0f, 50.0f)
		&& WorldHudCoordinateMatchesCamera() && WorldCoordinateAtlasReady();
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("World MiddleTip audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok world position MiddleTip audit %s: %s"),
		Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!Complete) UE_LOG(LogTemp, Error, TEXT("World MiddleTip audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokWorldPositionMiddleTipAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (!bBegun)
	{
		if (Elapsed > 35.0) Finish(TEXT("World MiddleTip audit startup timeout"));
		else if (Begin())
		{
			bBegun = true;
			Enter(1);
			Capture(TEXT("00_city_hud"), 2);
		}
		return;
	}
	RefreshWidgets();
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 25.0) Finish(TEXT("World MiddleTip screenshot timeout"));
		else AdvanceCapture();
		return;
	}
	if (Elapsed - StepSince > (Step == 11 ? 90.0 : 35.0))
	{
		Finish(FString::Printf(TEXT("World MiddleTip step %d timeout"), Step));
		return;
	}
	const auto HasText = [this](const TCHAR* X, const TCHAR* Y)
	{
		const UEditableTextBox* XBox = Screen.IsValid()
			? Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("WorldPositionXInput"))) : nullptr;
		const UEditableTextBox* YBox = Screen.IsValid()
			? Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("WorldPositionYInput"))) : nullptr;
		return XBox && YBox && XBox->GetText().ToString() == X
			&& YBox->GetText().ToString() == Y;
	};
	switch (Step)
	{
	case 2: if (Click(CityHud.Get(), TEXT("PositionSearchButton"))) Enter(3); break;
	case 3: if (StableSearch()) Capture(TEXT("01_city_search_open"), 4); break;
	case 4: if (Edit(TEXT("WorldPositionXInput"), TEXT(""))) Enter(5); break;
	case 5: if (HasText(TEXT(""), TEXT("157"))) Capture(TEXT("02_city_x_empty_pre_go"), 6); break;
	case 6:
		if (Click(Screen.Get(), TEXT("WorldPositionGo")))
		{
			TipTriggerElapsedSeconds = Elapsed;
			TipTriggerMonotonicSeconds = FPlatformTime::Seconds();
			TipTriggerFrame = GFrameCounter;
			Enter(7);
		}
		break;
	case 7: if (TipReady() && HasText(TEXT(""), TEXT("157")) && Boundary()
			&& GFrameCounter > TipTriggerFrame)
			Capture(TEXT("03_tip_first_frame"), 8); break;
	case 8: if (TipReady() && TipUiElapsed() >= .13333334)
			Capture(TEXT("04_tip_0133"), 9); break;
	case 9: if (TipReady() && TipUiElapsed() >= .16666667)
			Capture(TEXT("05_tip_0167"), 10); break;
	case 10: if (TipReady() && TipUiElapsed() >= .5)
			Capture(TEXT("06_tip_0500"), 11); break;
	case 11: if (TipReady() && TipUiElapsed() >= 2.25)
			Capture(TEXT("07_tip_2250"), 12); break;
	case 12: if (TipCollapsed() && TipTriggerMonotonicSeconds >= 0.0
			&& FPlatformTime::Seconds() - TipTriggerMonotonicSeconds >= 3.1
			&& HasText(TEXT(""), TEXT("157")) && Boundary())
			Capture(TEXT("08_tip_3100_collapsed"), 13); break;
	case 13: if (Edit(TEXT("WorldPositionXInput"), TEXT("100"))) Enter(14); break;
	case 14: if (Edit(TEXT("WorldPositionYInput"), TEXT("50"))) Enter(15); break;
	case 15: if (HasText(TEXT("100"), TEXT("50")) && TipCollapsed())
			Capture(TEXT("09_city_valid_inputs"), 16); break;
	case 16: if (Click(Screen.Get(), TEXT("WorldPositionGo"))) Enter(17); break;
	case 17:
		if (IsWorldMap() && RefreshWidgets() && AtWorldCenter(100, 50)
			&& WorldHudCoordinateMatchesCamera() && WorldCoordinateAtlasReady() && Boundary())
		{
			if (WorldMapReadySince < 0.0) WorldMapReadySince = Elapsed;
			if (Elapsed - WorldMapReadySince >= 2.0)
			{
				BaselineWorldBusiness = Encode(WorldBusinessState());
				Capture(TEXT("10_world_after_valid_go"), 18);
			}
		}
		else WorldMapReadySince = -1.0;
		break;
	case 18: Finish(); break;
	default: Finish(TEXT("World MiddleTip step out of range")); break;
	}
}
