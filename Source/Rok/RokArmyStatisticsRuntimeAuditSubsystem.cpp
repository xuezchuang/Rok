#include "RokArmyStatisticsRuntimeAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokUnityModalWidget.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
#include "TextureResource.h"
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokArmyStatisticsRuntimeAudit");
constexpr TCHAR CommonAtlas[] = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");

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
bool ModeReady(const UWidget* Widget)
{
	if (!Visible(Widget) || Widget->GetVisibility() != ESlateVisibility::SelfHitTestInvisible
		|| !FMath::IsNearlyEqual(Widget->GetRenderOpacity(), 1.0, 0.01)) return false;
	const FVector2D Scale = Widget->GetRenderTransform().Scale;
	return FMath::IsNearlyEqual(Scale.X, 1.0, 0.01)
		&& FMath::IsNearlyEqual(Scale.Y, 1.0, 0.01);
}
bool CommonAtlasImageReady(const UWidget* Widget)
{
	const UImage* Image = Cast<UImage>(Widget);
	const UTexture2D* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
	const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
	const FTextureRHIRef TextureRHI = Resource ? Resource->TextureRHI : FTextureRHIRef();
	return Texture && Texture->GetPathName() == CommonAtlas
		&& Texture->GetSizeX() == 2048 && Texture->GetSizeY() == 1024
		&& TextureRHI.IsValid() && TextureRHI->GetDesc().Extent.X == 2048
		&& TextureRHI->GetDesc().Extent.Y == 1024;
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
FString EncodeAuditJson(const TSharedRef<FJsonObject>& Json)
{
	FString Text;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
	return Text;
}
const TCHAR* VisibilityName(ESlateVisibility Value)
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
FString SlateIdentity(const SWidget* Widget)
{
	return Widget ? FString::Printf(TEXT("0x%p"), static_cast<const void*>(Widget)) : FString();
}
void CollectOwnedButtons(UWidget* Root, TArray<UButton*>& OutButtons)
{
	if (!Root) return;
	if (UButton* Button = Cast<UButton>(Root)) OutButtons.AddUnique(Button);
	if (UUserWidget* UserWidget = Cast<UUserWidget>(Root))
	{
		if (UserWidget->WidgetTree && UserWidget->WidgetTree->RootWidget)
			CollectOwnedButtons(UserWidget->WidgetTree->RootWidget, OutButtons);
	}
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			CollectOwnedButtons(Panel->GetChildAt(Index), OutButtons);
	}
}
FString ObjectIdentity(const UObject* Object)
{
	return Object ? FString::Printf(TEXT("0x%p"), static_cast<const void*>(Object)) : FString();
}
TArray<FVector2D> GeometryCorners(const FGeometry& Geometry)
{
	const FVector2D Size = Geometry.GetLocalSize();
	return {Geometry.LocalToAbsolute(FVector2D(0, 0)), Geometry.LocalToAbsolute(FVector2D(Size.X, 0)),
		Geometry.LocalToAbsolute(FVector2D(Size.X, Size.Y)), Geometry.LocalToAbsolute(FVector2D(0, Size.Y))};
}
TSharedRef<FJsonObject> GeometryEvidence(const FVector2D& LocalSize, const TArray<FVector2D>& Corners)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("local_size"), Point(LocalSize));
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FVector2D& Corner : Corners) Values.Add(MakeShared<FJsonValueObject>(Point(Corner)));
	Json->SetArrayField(TEXT("absolute_corners"), Values);
	return Json;
}

int32 FindSlatePathIndex(const FWidgetPath& Path, const SWidget* SlateWidget)
{
	if (!Path.IsValid() || !SlateWidget) return INDEX_NONE;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
		if (&Path.Widgets[Index].Widget.Get() == SlateWidget) return Index;
	return INDEX_NONE;
}

TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("exists"), Widget != nullptr);
	Json->SetBoolField(TEXT("visible"), Visible(Widget));
	Json->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	Json->SetStringField(TEXT("parent"), Widget && Widget->GetParent() ? Widget->GetParent()->GetName() : FString());
	Json->SetStringField(TEXT("visibility"), Widget ? VisibilityName(Widget->GetVisibility()) : TEXT("Absent"));
	Json->SetNumberField(TEXT("opacity"), Widget ? Widget->GetRenderOpacity() : 0.0);
	Json->SetObjectField(TEXT("render_scale"), Point(Widget ? Widget->GetRenderTransform().Scale : FVector2D::ZeroVector));
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Json->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
	Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
	{
		Json->SetStringField(TEXT("text"), Text->GetText().ToString());
		const FLinearColor Color = Text->GetColorAndOpacity().GetSpecifiedColor();
		auto ColorJson = MakeShared<FJsonObject>();
		ColorJson->SetNumberField(TEXT("r"), Color.R);
		ColorJson->SetNumberField(TEXT("g"), Color.G);
		ColorJson->SetNumberField(TEXT("b"), Color.B);
		ColorJson->SetNumberField(TEXT("a"), Color.A);
		Json->SetObjectField(TEXT("text_color"), ColorJson);
	}
	if (const UImage* Image = Cast<UImage>(Widget))
	{
		const UObject* Resource = Image->GetBrush().GetResourceObject();
		const UTexture2D* Texture = Cast<UTexture2D>(Resource);
		Json->SetStringField(TEXT("brush_resource"), Resource ? Resource->GetPathName() : FString());
		Json->SetObjectField(TEXT("texture_size"), Point(Texture
			? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D::ZeroVector));
	}
	return Json;
}
}

bool URokArmyStatisticsRuntimeAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokArmyStatisticsRuntimeAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokArmyStatisticsRuntimeAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokArmyStatisticsRuntimeAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokArmyStatisticsRuntimeAuditSubsystem, STATGROUP_Tickables);
}
void URokArmyStatisticsRuntimeAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/BatchReview20261009WorldSearchAndTileEffects/ArmyStatisticsRuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokArmyStatisticsRuntimeAuditSubsystem::ScreenshotProcessed);
}
void URokArmyStatisticsRuntimeAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokArmyStatisticsRuntimeAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokArmyStatisticsRuntimeAuditSubsystem::Expect(bool Condition, const TCHAR* Error)
{
	if (!Condition) Finish(Error);
	return Condition;
}
bool URokArmyStatisticsRuntimeAuditSubsystem::PrepareOwnWindowFocus()
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
		&& Client.bottom - Client.top == 810, TEXT("Army Statistics requires own 1440x810 GameApp window"))) return false;
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
	return Expect(false, TEXT("Army Statistics requires Windows game window"));
#endif
}

TSharedRef<FJsonObject> URokArmyStatisticsRuntimeAuditSubsystem::BusinessState() const
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
	Json->SetNumberField(TEXT("task_type"), Task.Type);
	Json->SetNumberField(TEXT("task_param1"), Task.Param1);
	Json->SetNumberField(TEXT("task_param2"), Task.Param2);
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
		Entry->SetNumberField(TEXT("power"), Troop.TotalPower);
		Troops.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_troops"), Troops);
	return Json;
}

bool URokArmyStatisticsRuntimeAuditSubsystem::Boundary() const
{
	const APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	return Identity() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& Controller.IsValid() && Pawn && Pawn->GetActorTransform().Equals(BaselineCamera, 0.001f)
		&& Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == BaselinePrimary
		&& !BaselineBusiness.IsEmpty() && EncodeAuditJson(BusinessState()) == BaselineBusiness;
}

TSharedRef<FJsonObject> URokArmyStatisticsRuntimeAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("business"), BusinessState());
	Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Json->SetNumberField(TEXT("primary_press_count"), Controller.IsValid()
		? static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount) : -1.0);
	Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	const APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	Json->SetObjectField(TEXT("camera_location"), Point(Pawn ? FVector2D(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y) : FVector2D::ZeroVector));
	Json->SetNumberField(TEXT("camera_z"), Pawn ? Pawn->GetActorLocation().Z : 0);
	Json->SetObjectField(TEXT("camera_rotation_yaw_pitch"), Point(Pawn ? FVector2D(Pawn->GetActorRotation().Yaw, Pawn->GetActorRotation().Pitch) : FVector2D::ZeroVector));
	auto Widgets = MakeShared<FJsonObject>();
	Widgets->SetObjectField(TEXT("RokMainHud"), WidgetState(Hud.Get()));
	Widgets->SetObjectField(TEXT("PlayerProfileHitButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("PlayerProfileHitButton")) : nullptr));
	Widgets->SetObjectField(TEXT("ProfileArmyButton"), WidgetState(Modal.IsValid() ? Modal->GetWidgetFromName(TEXT("ProfileArmyButton")) : nullptr));
	if (Screen.IsValid())
	{
		const TCHAR* Names[] = {TEXT("FeatureModeCanvas"), TEXT("FeatureFrameClose"),
			TEXT("ArmyStatisticsSummaryShade"), TEXT("ArmyStatisticsTotalCount"),
			TEXT("ArmyStatisticsTotalPower"), TEXT("ArmyStatisticsDataSource"),
			TEXT("ArmyStatisticsTroopList"), TEXT("ArmyStatisticsInsideEmpty"),
			TEXT("ArmyStatisticsOutsideEmpty")};
		for (const TCHAR* Name : Names) Widgets->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(Name)));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FString Button = FString::Printf(TEXT("ArmyStatisticsPage_%d"), Index);
			Widgets->SetObjectField(Button, WidgetState(Screen->GetWidgetFromName(*Button)));
			Widgets->SetObjectField(Button + TEXT("Label"), WidgetState(Screen->GetWidgetFromName(*(Button + TEXT("Label")))));
		}
		for (int32 Index = 0; Index < 4; ++Index)
		{
			for (const TCHAR* Suffix : {TEXT("Row"), TEXT("RowName"), TEXT("RowCount"), TEXT("RowPower"), TEXT("RowSource")})
			{
				const FString Name = FString::Printf(TEXT("ArmyStatistics%s_%d"), Suffix, Index);
				Widgets->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(*Name)));
			}
		}
	}
	Json->SetObjectField(TEXT("widgets"), Widgets);
	return Json;
}

bool URokArmyStatisticsRuntimeAuditSubsystem::Ready(int32 Page) const
{
	if (!Boundary() || !Hud.IsValid() || !Modal.IsValid() || !Screen.IsValid()) return false;
	if (Page == -2) return Modal->IsModalOpen() && !Screen->IsScreenOpen()
		&& Visible(Modal->GetWidgetFromName(TEXT("ProfileArmyButton")));
	if (Page == -1) return !Modal->IsModalOpen() && !Screen->IsScreenOpen()
		&& Visible(Hud->GetWidgetFromName(TEXT("PlayerProfileHitButton")));
	const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	if (!Screen->IsScreenOpen() || Modal->IsModalOpen() || !ModeReady(Mode)
		|| !Mode->GetCachedGeometry().GetLocalSize().Equals(FVector2D(1440, 810), 0.1f)) return false;
	for (int32 Index = 0; Index < 3; ++Index)
		if (!Visible(Screen->GetWidgetFromName(*FString::Printf(TEXT("ArmyStatisticsPage_%d"), Index)))) return false;
	const UTextBlock* SourceText = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ArmyStatisticsDataSource")));
	if (!SourceText || !Visible(SourceText)) return false;
	const bool bTotals = Screen->GetWidgetFromName(TEXT("ArmyStatisticsTotalCount"))
		&& Screen->GetWidgetFromName(TEXT("ArmyStatisticsTotalPower"));
	if (Page == 0)
	{
		if (!bTotals || !Visible(Screen->GetWidgetFromName(TEXT("ArmyStatisticsTroopList")))
			|| Screen->GetWidgetFromName(TEXT("ArmyStatisticsInsideEmpty"))
			|| Screen->GetWidgetFromName(TEXT("ArmyStatisticsOutsideEmpty"))
			|| !SourceText->GetText().ToString().Contains(TEXT("非账号部队状态"))) return false;
		for (int32 Index = 0; Index < 4; ++Index)
			if (!Visible(Screen->GetWidgetFromName(*FString::Printf(TEXT("ArmyStatisticsRow_%d"), Index)))) return false;
		return Screen->GetWidgetFromName(TEXT("ArmyStatisticsRow_4")) == nullptr;
	}
	if (Page != 1 && Page != 2) return false;
	const TCHAR* EmptyName = Page == 1 ? TEXT("ArmyStatisticsInsideEmpty") : TEXT("ArmyStatisticsOutsideEmpty");
	const TCHAR* OtherName = Page == 1 ? TEXT("ArmyStatisticsOutsideEmpty") : TEXT("ArmyStatisticsInsideEmpty");
	const UTextBlock* Empty = Cast<UTextBlock>(Screen->GetWidgetFromName(EmptyName));
	return !bTotals && !Screen->GetWidgetFromName(TEXT("ArmyStatisticsTroopList"))
		&& !Screen->GetWidgetFromName(TEXT("ArmyStatisticsRow_0"))
		&& !Screen->GetWidgetFromName(OtherName) && Empty && Visible(Empty)
		&& Empty->GetText().ToString().Contains(Page == 1 ? TEXT("城内兵力位置数据尚未接入") : TEXT("城外兵力位置数据尚未接入"))
		&& SourceText->GetText().ToString().Contains(TEXT("位置归属数据未接入"));
}

bool URokArmyStatisticsRuntimeAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(), TEXT("Wrong project/world or Slate unavailable"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	while (FParse::Token(Cursor, Token, false))
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& !Token.Equals(AuditFlag, ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok diagnostic/fixture flag"));
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
	Widgets.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokUnityModalWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets) if (Widget && Widget->GetWorld() == GetWorld())
	{
		if (Modal.IsValid()) return Expect(false, TEXT("More than one GameApp modal"));
		Modal = Cast<URokUnityModalWidget>(Widget);
	}
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	if (!Expect(Controller.IsValid() && Controller->GetPawn() && Buildings.IsValid() && Runtime.IsValid()
		&& Server.IsValid() && Hud.IsValid() && Modal.IsValid() && Screen.IsValid()
		&& Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), 0.1f),
		TEXT("Army Statistics expects own idle local-only 1440x810 HUD"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Army Statistics account identity is not local preview"))) return false;
	const auto Troops = Runtime->GetStarterTroopStacks();
	const auto Summary = Runtime->GetSummary();
	int32 Count = 0, Power = 0;
	for (const auto& Troop : Troops) { Count += Troop.Count; Power += Troop.TotalPower; }
	if (!Expect(Troops.Num() == 4 && Count == 200 && Power == 200
		&& Summary.TotalTroopCount == Count && Summary.TotalTroopPower == Power,
		TEXT("Army Statistics local bootstrap four-stack source differs"))) return false;
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	BaselineCamera = Controller->GetPawn()->GetActorTransform();
	BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	BaselineBusiness = EncodeAuditJson(BusinessState());
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokArmyStatisticsRuntimeAuditSubsystem.h"),
		TEXT("Source/Rok/RokArmyStatisticsRuntimeAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokArmyStatisticsRuntime.py"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
		TEXT("Source/Rok/RokUiAnimatedButton.h"),
		TEXT("Source/Rok/RokUiAnimatedButton.cpp"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokUnityModalWidget.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"),
		TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"),
		TEXT("Source/Rok/RokServerSubsystem.cpp"),
		TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json"),
		TEXT("Saved/PendingArmyStatisticsPresentationV3/army_statistics_truthful_local_preview_v3_review.json"),
		TEXT("Saved/PendingArmyStatisticsPresentationV3/army_statistics_truthful_local_preview_v3.patch"),
		TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Troop/UI_Win_ArmyShowMediator.cs")};
	for (const TCHAR* Path : Required)
	{
		const FString Hash = FileSha(FPaths::ProjectDir() / Path);
		if (!Expect(Hash.Len() == 64, TEXT("Army Statistics source input missing"))) return false;
		Inputs->SetStringField(Path, Hash);
	}
	if (!Expect(FileSha(FPaths::ProjectDir() / TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"))
		== TEXT("f2f006b9a6b5c889827a2d2249a375cd0debf724b749b4af9ba9b4d4697b345f"),
		TEXT("Army Statistics product is not reviewed V3 target"))) return false;
	return true;
}
bool URokArmyStatisticsRuntimeAuditSubsystem::Click(UWidget* Target, const TCHAR* Label)
{
	auto RecordPreflightFailure = [this, Target, Label](const TCHAR* Reason)
	{
		auto Attempt = MakeShared<FJsonObject>();
		Attempt->SetStringField(TEXT("label"), Label);
		Attempt->SetNumberField(TEXT("step"), Step);
		Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
		Attempt->SetStringField(TEXT("phase"), TEXT("preflight_failure"));
		Attempt->SetStringField(TEXT("failure_reason"), Reason);
		Attempt->SetBoolField(TEXT("target_exists"), Target != nullptr);
		Attempt->SetStringField(TEXT("target_widget_name"), Target ? Target->GetName() : FString());
		Attempt->SetStringField(TEXT("target_widget_type"), Target ? Target->GetClass()->GetName() : FString());
		Attempt->SetStringField(TEXT("target_widget_identity"), ObjectIdentity(Target));
		if (Target)
		{
			const FGeometry Geometry = Target->GetCachedGeometry();
			Attempt->SetObjectField(TEXT("target_geometry"), GeometryEvidence(Geometry.GetLocalSize(), GeometryCorners(Geometry)));
		}
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
	};
	if (!Boundary()) { RecordPreflightFailure(TEXT("business boundary or runtime state invalid")); return Expect(false, TEXT("Army Statistics Slate target preflight failed")); }
	if (!Target) { RecordPreflightFailure(TEXT("target widget missing")); return Expect(false, TEXT("Army Statistics Slate target preflight failed")); }
	if (!Visible(Target) || !Target->GetIsEnabled())
	{
		RecordPreflightFailure(TEXT("target hidden, transparent, or disabled"));
		return Expect(false, TEXT("Army Statistics Slate target preflight failed"));
	}
	if (!FSlateApplication::IsInitialized())
	{
		RecordPreflightFailure(TEXT("Slate application unavailable"));
		return Expect(false, TEXT("Army Statistics Slate target preflight failed"));
	}
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (Geometry.GetLocalSize().X <= 0 || Geometry.GetLocalSize().Y <= 0)
	{
		RecordPreflightFailure(TEXT("target cached geometry has zero size"));
		return Expect(false, TEXT("Army Statistics Slate target preflight failed"));
	}
	UButton* HitOwnedButton = Cast<UButton>(Target);
	if (!HitOwnedButton)
	{
		RecordPreflightFailure(TEXT("target is not the source-pinned UButton target"));
		return Expect(false, TEXT("Army Statistics Slate target preflight failed"));
	}
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
	const TArray<FVector2D> CurrentCorners = GeometryCorners(Geometry);
	if (StableClickTarget.Get() != Target)
	{
		StableClickTarget = Target; StableClickTopLeft = TopLeft; StableClickBottomRight = BottomRight;
		StableClickLocalSize = Geometry.GetLocalSize(); StableClickCorners = CurrentCorners;
		StableClickFrame = GFrameCounter; StableClickFrames = 1; return false;
	}
	bool bSameGeometry = StableClickLocalSize.Equals(Geometry.GetLocalSize(), 0.25f)
		&& StableClickCorners.Num() == CurrentCorners.Num();
	for (int32 Index = 0; bSameGeometry && Index < CurrentCorners.Num(); ++Index)
		bSameGeometry &= StableClickCorners[Index].Equals(CurrentCorners[Index], 0.25f);
	if (!bSameGeometry)
	{
		StableClickTopLeft = TopLeft; StableClickBottomRight = BottomRight;
		StableClickLocalSize = Geometry.GetLocalSize(); StableClickCorners = CurrentCorners;
		StableClickFrame = GFrameCounter; StableClickFrames = 1; return false;
	}
	if (GFrameCounter <= StableClickFrame) return false;
	const uint64 PreviousStableFrame = StableClickFrame;
	const FVector2D PreviousStableSize = StableClickLocalSize;
	const TArray<FVector2D> PreviousStableCorners = StableClickCorners;
	StableClickFrame = GFrameCounter; StableClickLocalSize = Geometry.GetLocalSize(); StableClickCorners = CurrentCorners;
	StableClickTopLeft = TopLeft; StableClickBottomRight = BottomRight;
	++StableClickFrames;
	if (StableClickFrames < 2) return false;

	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30.0f, -30.0f));
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	const TSharedRef<SWidget> TargetSlate = Target->TakeWidget();
	const int32 TargetPathIndex = FindSlatePathIndex(Path, &TargetSlate.Get());
	TArray<TSharedPtr<FJsonValue>> Types;
	TArray<TSharedPtr<FJsonValue>> Nodes;
	bool bUmg = false;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		const SWidget* SlateNode = &Path.Widgets[Index].Widget.Get();
		const FString Type = SlateNode->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		auto Node = MakeShared<FJsonObject>(); Node->SetNumberField(TEXT("index"), Index);
		Node->SetStringField(TEXT("type"), Type); Node->SetStringField(TEXT("identity"), SlateIdentity(SlateNode));
		Node->SetObjectField(TEXT("geometry"), GeometryEvidence(Path.Widgets[Index].Geometry.GetLocalSize(), GeometryCorners(Path.Widgets[Index].Geometry)));
		Nodes.Add(MakeShared<FJsonValueObject>(Node)); bUmg |= Type.Contains(TEXT("SObjectWidget"));
	}
	const FString Terminal = Path.IsValid() && Path.Widgets.Num() ? Path.Widgets.Last().Widget->GetTypeAsString() : FString();
	const int32 OwnedButtonPathIndex = FindSlatePathIndex(Path, &HitOwnedButton->TakeWidget().Get());
	const bool bOwnedButtonInPath = OwnedButtonPathIndex != INDEX_NONE && OwnedButtonPathIndex >= TargetPathIndex
		&& Path.Widgets[OwnedButtonPathIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	const bool bTargetInPath = TargetPathIndex != INDEX_NONE;
	const bool bTargetHovered = Target->IsHovered();
	const uint64 Before = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("label"), Label); Action->SetStringField(TEXT("method"), TEXT("Slate ProcessMouseMoveEvent+DownEvent+UpEvent"));
	Action->SetNumberField(TEXT("step"), Step); Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetObjectField(TEXT("absolute_center"), Point(Center));
	Action->SetObjectField(TEXT("target_geometry"), GeometryEvidence(Geometry.GetLocalSize(), CurrentCorners));
	auto Stability = MakeShared<FJsonObject>();
	Stability->SetNumberField(TEXT("previous_frame"), static_cast<double>(PreviousStableFrame));
	Stability->SetNumberField(TEXT("current_frame"), static_cast<double>(GFrameCounter));
	Stability->SetObjectField(TEXT("previous_geometry"), GeometryEvidence(PreviousStableSize, PreviousStableCorners));
	Stability->SetObjectField(TEXT("current_geometry"), GeometryEvidence(Geometry.GetLocalSize(), CurrentCorners));
	Action->SetObjectField(TEXT("two_frame_stability"), Stability);
	Action->SetStringField(TEXT("target_widget_name"), Target->GetName()); Action->SetStringField(TEXT("target_widget_type"), Target->GetClass()->GetName());
	Action->SetStringField(TEXT("target_widget_class_path"), Target->GetClass()->GetPathName());
	Action->SetBoolField(TEXT("target_widget_is_u_button"), Target->IsA(UButton::StaticClass()));
	Action->SetStringField(TEXT("target_widget_identity"), ObjectIdentity(Target)); Action->SetStringField(TEXT("target_slate_identity"), SlateIdentity(&TargetSlate.Get()));
	Action->SetNumberField(TEXT("target_path_index"), TargetPathIndex); Action->SetArrayField(TEXT("hit_path_types"), Types); Action->SetArrayField(TEXT("hit_path_nodes"), Nodes);
	Action->SetStringField(TEXT("terminal_type"), Terminal); Action->SetBoolField(TEXT("own_game_window"), bOwnWindow); Action->SetBoolField(TEXT("game_window_foreground"), bForeground);
	Action->SetBoolField(TEXT("target_in_hit_path"), bTargetInPath); Action->SetBoolField(TEXT("target_hovered"), bTargetHovered);
	Action->SetBoolField(TEXT("owned_button_in_hit_path"), bOwnedButtonInPath); Action->SetStringField(TEXT("owned_button_widget_name"), HitOwnedButton->GetName());
	Action->SetStringField(TEXT("owned_button_widget_type"), HitOwnedButton->GetClass()->GetName()); Action->SetStringField(TEXT("owned_button_widget_identity"), ObjectIdentity(HitOwnedButton));
	Action->SetStringField(TEXT("owned_button_slate_identity"), SlateIdentity(&HitOwnedButton->TakeWidget().Get())); Action->SetNumberField(TEXT("owned_button_path_index"), OwnedButtonPathIndex);
	TArray<TSharedPtr<FJsonValue>> ParentChain;
	auto ChainNode = MakeShared<FJsonObject>(); ChainNode->SetStringField(TEXT("widget_name"), Target->GetName());
	ChainNode->SetStringField(TEXT("widget_type"), Target->GetClass()->GetName()); ChainNode->SetStringField(TEXT("widget_identity"), ObjectIdentity(Target));
	ChainNode->SetBoolField(TEXT("is_target"), true); ParentChain.Add(MakeShared<FJsonValueObject>(ChainNode));
	Action->SetArrayField(TEXT("owned_button_parent_chain"), ParentChain);
	Action->SetBoolField(TEXT("umg_path_present"), bUmg); Action->SetBoolField(TEXT("down_handled"), false); Action->SetBoolField(TEXT("up_handled"), false);
	Action->SetNumberField(TEXT("primary_before"), static_cast<double>(Before)); Action->SetNumberField(TEXT("primary_after"), static_cast<double>(Before));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	const FString TargetClassPath = Target->GetClass()->GetPathName();
	const bool bPinnedButtonType = Target->IsA(UButton::StaticClass())
		&& (TargetClassPath == TEXT("/Script/UMG.Button") || TargetClassPath == TEXT("/Script/Rok.RokUiAnimatedButton")
			|| TargetClassPath == TEXT("/Script/Rok.RokUnityIndexedButton"));
	if (!Expect(Path.IsValid() && bTargetInPath && bTargetHovered && bUmg && bOwnedButtonInPath
		&& OwnedButtonPathIndex == TargetPathIndex && bPinnedButtonType && bOwnWindow && bForeground,
		TEXT("Army Statistics Slate hit did not resolve to exact target UButton in foreground game window"))) return false;
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(), FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const uint64 After = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Action->SetBoolField(TEXT("down_handled"), Down); Action->SetBoolField(TEXT("up_handled"), Up); Action->SetNumberField(TEXT("primary_after"), static_cast<double>(After));
	StableClickTarget.Reset(); StableClickFrames = 0; StableClickFrame = 0; StableClickCorners.Reset();
	return Expect(Down && Up && Before == After && Boundary(), TEXT("Army Statistics UI click leaked to world or changed business state"));
}

void URokArmyStatisticsRuntimeAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested() && Boundary(),
		TEXT("Army Statistics screenshot request invalid"))) return;
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
void URokArmyStatisticsRuntimeAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	const FString Hash = FileSha(PendingPath);
	if (!Expect(Hash.Len() == 64 && Boundary(), TEXT("Army Statistics PNG/source state absent during screenshot"))) return;
	Pending->SetStringField(TEXT("png_sha256"), Hash);
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(IFileManager::Get().FileSize(*PendingPath)));
	Pending->SetObjectField(TEXT("processed_state"), State());
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokArmyStatisticsRuntimeAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(NextStep);
}
void URokArmyStatisticsRuntimeAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep;
	StepSince = Elapsed;
	StableFrames = 0;
}
bool URokArmyStatisticsRuntimeAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.army_statistics.runtime_audit.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Attempts.IsEmpty() && Captures.Num() == 7 && Actions.Num() == 6);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), TEXT("Real HUD profile modal to Army Statistics; three UE-only local tabs, account deployment unknown; no troop, wallet, queue, camera or HTTP mutation"));
	Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("attempts"), Attempts);
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetObjectField(TEXT("final_state"), State());
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	return FFileHelper::SaveStringToFile(EncodeAuditJson(Json), *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokArmyStatisticsRuntimeAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && Attempts.IsEmpty() && Captures.Num() == 7 && Actions.Num() == 6 && Boundary();
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Army Statistics audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok Army Statistics audit %s: %s"), Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!Complete) UE_LOG(LogTemp, Error, TEXT("Army Statistics audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}


void URokArmyStatisticsRuntimeAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !Identity()) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (Step == 0 && Elapsed > 25.0) { Finish(TEXT("Army Statistics startup/focus timeout")); return; }
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 25.0) Finish(TEXT("Army Statistics screenshot timeout"));
		else AdvanceCapture();
		return;
	}
	if (Step > 0 && Elapsed - StepSince > 35.0)
	{
		Finish(FString::Printf(TEXT("Army Statistics step %d timeout"), Step));
		return;
	}
	auto Stable = [this](int32 Page)
	{
		StableFrames = Ready(Page) ? StableFrames + 1 : 0;
		return StableFrames >= 2;
	};
	switch (Step)
	{
	case 0: if (!bBegun) bBegun = Begin(); if (bBegun && PrepareOwnWindowFocus()) Enter(1); break;
	case 1: if (Stable(-1)) Capture(TEXT("00_city_hud"), 2); break;
	case 2: if (PrepareOwnWindowFocus() && Click(Hud->GetWidgetFromName(TEXT("PlayerProfileHitButton")), TEXT("open_profile_from_hud"))) Enter(3); break;
	case 3: if (Stable(-2)) Capture(TEXT("01_profile"), 4); break;
	case 4: if (Click(Modal->GetWidgetFromName(TEXT("ProfileArmyButton")), TEXT("open_army_statistics_from_profile"))) Enter(5); break;
	case 5: if (Stable(0)) Capture(TEXT("02_local_summary"), 6); break;
	case 6: if (Click(Screen->GetWidgetFromName(TEXT("ArmyStatisticsPage_1")), TEXT("inside_city_unknown"))) Enter(7); break;
	case 7: if (Stable(1)) Capture(TEXT("03_inside_unknown"), 8); break;
	case 8: if (Click(Screen->GetWidgetFromName(TEXT("ArmyStatisticsPage_2")), TEXT("outside_city_unknown"))) Enter(9); break;
	case 9: if (Stable(2)) Capture(TEXT("04_outside_unknown"), 10); break;
	case 10: if (Click(Screen->GetWidgetFromName(TEXT("ArmyStatisticsPage_0")), TEXT("return_local_summary"))) Enter(11); break;
	case 11: if (Stable(0)) Capture(TEXT("05_summary_return"), 12); break;
	case 12: if (Click(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")), TEXT("close_army_statistics"))) Enter(13); break;
	case 13: if (Stable(-1)) Capture(TEXT("06_closed_hud"), 14); break;
	case 14: Finish(); break;
	default: Finish(TEXT("Invalid Army Statistics audit state")); break;
	}
}
