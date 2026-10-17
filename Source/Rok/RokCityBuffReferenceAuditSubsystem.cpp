#include "RokCityBuffReferenceAuditSubsystem.h"

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
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokCityBuffReferenceAudit");
constexpr TCHAR MainMenuAtlasPath[] = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu");
struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Product pins are refreshed only after root publishes the reviewed V2 files.
constexpr FInputPin Pinned[] = {
	{TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9dc34a7279c15f7cf153c8f063cb06d565be9f9792e50def35bac2195241f05c")},
	{TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef")},
	{TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
	{TEXT("Source/Rok/RokPlayerController.cpp"), TEXT("f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6")},
	{TEXT("Saved/RokUnityMigration/city_buff_reference.json"), TEXT("850733b77e4dfaf330177c2ba035ce538f489d13f28997b27f94d0a66703b1f0")},
	{TEXT("Saved/RokUnityMigration/ConfigTables/Language.json"), TEXT("e228eca8deb7c8daa4a2d910f736c57567db11322d558544309bfbaca412277d")},
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.uasset"), TEXT("eedd8ff20861fd25ab543eaaa833a7177b9990829c09e573d9532d1a08717d3e")},
	{TEXT("ref/config-tables/编译客户端配置/CSV/CityBuff.csv"), TEXT("53432006ce01a2dc55ca05098f6a02fc9fdda5417ea5a50d5f5d307803009e6a")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_mainmenu.tpsheet"), TEXT("04f4c19bff18cfb23414b51b94255ae78e01a3871e26550d7ea3cc4e3a2b13bd")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UICityManager/UI_Pop_BuffListMediator.cs"), TEXT("9ce4ad4aee502f59dc8b8ab2ae2e1db736bf141ed5269f6ac07781c1f3a713bb")}
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
		&& FEngineVersion::Current().GetMajor() == 5 && FEngineVersion::Current().GetMinor() == 8;
}
FString Encode(const TSharedRef<FJsonObject>& Json)
{
	FString Out;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Out));
	return Out;
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
TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}
TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("exists"), Widget != nullptr);
	Json->SetBoolField(TEXT("visible"), Visible(Widget));
	Json->SetStringField(TEXT("name"), Widget ? Widget->GetName() : FString());
	Json->SetStringField(TEXT("text"), Cast<UTextBlock>(Widget) ? Cast<UTextBlock>(Widget)->GetText().ToString() : FString());
	if (const UImage* Image = Cast<UImage>(Widget))
	{
		const FSlateBrush& Brush = Image->GetBrush();
		Json->SetStringField(TEXT("texture"), Brush.GetResourceObject() ? Brush.GetResourceObject()->GetPathName() : FString());
		const FBox2f Uv = Brush.GetUVRegion();
		Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(Uv.Min.X, Uv.Min.Y)));
		Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(Uv.Max.X, Uv.Max.Y)));
	}
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Json->SetObjectField(TEXT("top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	return Json;
}
int32 PngDimension(const TArray<uint8>& Data, int32 Offset)
{
	return (static_cast<int32>(Data[Offset]) << 24) | (static_cast<int32>(Data[Offset + 1]) << 16)
		| (static_cast<int32>(Data[Offset + 2]) << 8) | static_cast<int32>(Data[Offset + 3]);
}
}

bool URokCityBuffReferenceAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer) && FParse::Param(FCommandLine::Get(), TEXT("RokCityBuffReferenceAudit"));
#endif
}
bool URokCityBuffReferenceAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokCityBuffReferenceAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityBuffReferenceAuditSubsystem, STATGROUP_Tickables); }
void URokCityBuffReferenceAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokCityBuffReferenceAuditSubsystem::ScreenshotProcessed);
}
void URokCityBuffReferenceAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokCityBuffReferenceAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokCityBuffReferenceAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokCityBuffReferenceAuditSubsystem::PrepareOwnWindowFocus()
{
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD Pid = 0;
	if (Handle) ::GetWindowThreadProcessId(Handle, &Pid);
	RECT Client = {};
	if (!Expect(Handle && ::IsWindow(Handle) && Pid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client) && Client.right - Client.left == 1440
		&& Client.bottom - Client.top == 810, TEXT("Audit requires owned 1440x810 GameApp client"))) return false;
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
bool URokCityBuffReferenceAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(), TEXT("Wrong world/engine or Slate unavailable"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 AuditFlags = 0, LocalFlags = 0, ModalFlags = 0;
	FString ModalName;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditFlags; continue; }
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlags; continue; }
		if (Token.StartsWith(TEXT("-RokOpenModal="), ESearchCase::IgnoreCase))
		{
			++ModalFlags;
			ModalName = Token.Mid(14).ToLower();
			continue;
		}
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok fixture/diagnostic flag"));
	}
	bReference = ModalName == TEXT("buff_reference");
	if (!Expect(AuditFlags == 1 && LocalFlags == 1 && ModalFlags == 1
		&& (ModalName == TEXT("buff") || bReference), TEXT("Require exactly one audit/local-only/buff route"))) return false;
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
	Modal = Hud.IsValid() ? Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget"))) : nullptr;
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	if (!Expect(Controller.IsValid() && Controller->GetPawn() && Hud.IsValid() && Modal.IsValid()
		&& Screen.IsValid() && Buildings.IsValid() && Runtime.IsValid() && Runtime->IsRuntimeStateLoaded()
		&& Server.IsValid() && Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0 && Modal->ActiveMode == ERokUnityModalMode::BuffList
		&& Modal->bReferenceBuffListCaptureState == bReference && !Screen->IsScreenOpen()
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
		TEXT("Controller CLI did not open expected real HUD Buff modal"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId
		&& Profile.RoleId.IsEmpty(), TEXT("Expected anonymous local preview"))) return false;
	Inputs = MakeShared<FJsonObject>();
	for (const FInputPin& Pin : Pinned)
	{
		const FString Actual = FileSha(FPaths::ProjectDir() / Pin.Path);
		if (!Expect(Actual == Pin.Sha, TEXT("Pinned source/product/manifest/atlas changed"))) return false;
		Inputs->SetStringField(Pin.Path, Actual);
	}
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/CityBuffReferenceRuntimeAuditV6")
		/ (bReference ? TEXT("fixture") : TEXT("production")));
	FPaths::NormalizeDirectoryName(Directory);
	IFileManager::Get().MakeDirectory(*Directory, true);
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	bPointerSaved = true;
	BaselineBusinessJson = BusinessState();
	BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
	BaselineCamera = Controller->GetPawn()->GetActorTransform();
	BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	bBaselineSet = true;
	return true;
}
TSharedRef<FJsonObject> URokCityBuffReferenceAuditSubsystem::BusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Buildings.IsValid() || !Runtime.IsValid() || !Server.IsValid()) return Json;
	const auto Wallet = Buildings->GetCurrentResources();
	Json->SetNumberField(TEXT("food"), Wallet.Food);
	Json->SetNumberField(TEXT("wood"), Wallet.Wood);
	Json->SetNumberField(TEXT("stone"), Wallet.Stone);
	Json->SetNumberField(TEXT("gold"), Wallet.Gold);
	const auto Rates = Buildings->GetProductionRates();
	Json->SetNumberField(TEXT("rate_food"), Rates.FoodPerHour);
	Json->SetNumberField(TEXT("rate_wood"), Rates.WoodPerHour);
	Json->SetNumberField(TEXT("rate_stone"), Rates.StonePerHour);
	Json->SetNumberField(TEXT("rate_gold"), Rates.GoldPerHour);
	Json->SetNumberField(TEXT("active_upgrades"), Buildings->GetActiveUpgradeCount());
	Json->SetNumberField(TEXT("active_construction"), Buildings->GetActiveLocalConstructionCount());
	const auto Task = Runtime->GetActiveTask();
	Json->SetNumberField(TEXT("task_id"), Task.TaskId);
	Json->SetNumberField(TEXT("task_current"), Task.Current);
	Json->SetBoolField(TEXT("task_complete"), Task.bComplete);
	const auto Profile = Runtime->GetPlayerProfileState();
	Json->SetStringField(TEXT("profile_kind"), Profile.StateKind);
	Json->SetBoolField(TEXT("profile_has_role_id"), Profile.bHasRoleId);
	Json->SetStringField(TEXT("profile_role_id"), Profile.RoleId);
	Json->SetBoolField(TEXT("profile_has_civilization"), Profile.bHasCivilization);
	Json->SetNumberField(TEXT("profile_civilization_id"), Profile.CivilizationId);
	Json->SetBoolField(TEXT("server_local_only"), Server->IsLocalOnlyRun());
	Json->SetNumberField(TEXT("http_dispatch_attempts"), static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("http_suppressed_requests"), static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	return Json;
}
bool URokCityBuffReferenceAuditSubsystem::Boundary() const
{
	return Identity() && bBaselineSet && Controller.IsValid() && Controller->GetPawn()
		&& Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		&& Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == BaselinePrimary
		&& Encode(BusinessState()) == BaselineBusiness;
}
bool URokCityBuffReferenceAuditSubsystem::ReadyOpen() const
{
	if (!Modal.IsValid() || !Screen.IsValid() || Modal->ActiveMode != ERokUnityModalMode::BuffList
		|| Modal->bReferenceBuffListCaptureState != bReference || Screen->IsScreenOpen()) return false;
	const UWidget* Popup = Modal->GetWidgetFromName(TEXT("BuffListAnimationRoot"));
	const UTextBlock* Banner = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("BuffListReferencePreviewNotice")));
	if (!Visible(Popup) || Popup->GetRenderOpacity() < .995f
		|| !Popup->GetRenderTransform().Translation.IsNearlyZero(.25f)
		|| !Popup->GetRenderTransform().Scale.Equals(FVector2D(1,1), .01f)) return false;
#if WITH_EDITOR
	if (FTextureCompilingManager::Get().GetNumRemainingTextures() != 0) return false;
#endif
	static const TCHAR* Atlases[] = {
		TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
		TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"), MainMenuAtlasPath};
	for (const TCHAR* Path : Atlases)
	{
		const UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
		const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
		if (!Resource || !Resource->TextureRHI.IsValid()) return false;
	}
	if (!bReference)
	{
		const UTextBlock* BoundaryTitle = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("BuffListServiceBoundaryTitle")));
		return Visible(BoundaryTitle) && BoundaryTitle->GetText().ToString() == TEXT("城市增益状态尚未接入")
			&& !Modal->GetWidgetFromName(TEXT("BuffListDefinitionRow_0"))
			&& !Modal->GetWidgetFromName(TEXT("BuffListDefinitionRow_1")) && !Banner;
	}
	const UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, MainMenuAtlasPath);
	if (!Visible(Banner) || Banner->GetText().ToString() != TEXT("城市增益静态配置预览 · 非实时/未激活")) return false;
	const TCHAR* Titles[] = {TEXT("食物采集速度+10%"), TEXT("食物产量+25%")};
	const TCHAR* Fields[] = {TEXT("配置时长 21600 秒（非剩余时间）"), TEXT("配置时长 28800 秒（非剩余时间）")};
	const FIntRect IconRects[] = {FIntRect(38,142,73,177), FIntRect(114,476,149,511)};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const UTextBlock* Title = Cast<UTextBlock>(Modal->GetWidgetFromName(*FString::Printf(TEXT("BuffListDefinitionTitle_%d"), Index)));
		const UTextBlock* Field = Cast<UTextBlock>(Modal->GetWidgetFromName(*FString::Printf(TEXT("BuffListDefinitionFields_%d"), Index)));
		const UImage* Icon = Cast<UImage>(Modal->GetWidgetFromName(*FString::Printf(TEXT("BuffListDefinitionIcon_%d"), Index)));
		const FBox2f Uv = Icon ? static_cast<FBox2f>(Icon->GetBrush().GetUVRegion()) : FBox2f();
		const FVector2f ExpectedMin(static_cast<float>(IconRects[Index].Min.X) / 1024.0f,
			static_cast<float>(1024 - IconRects[Index].Max.Y) / 1024.0f);
		const FVector2f ExpectedMax(static_cast<float>(IconRects[Index].Max.X) / 1024.0f,
			static_cast<float>(1024 - IconRects[Index].Min.Y) / 1024.0f);
		if (!Visible(Title) || Title->GetText().ToString() != Titles[Index]
			|| !Visible(Field) || Field->GetText().ToString() != Fields[Index]
			|| !Visible(Icon) || Icon->GetBrush().GetResourceObject() != Atlas
			|| !Uv.Min.Equals(ExpectedMin, 0.0001f) || !Uv.Max.Equals(ExpectedMax, 0.0001f)) return false;
	}
	return !Modal->GetWidgetFromName(TEXT("BuffListDefinitionRow_2"));
}
bool URokCityBuffReferenceAuditSubsystem::ReadyManager() const
{
	const UTextBlock* Title = Screen.IsValid() ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureFrameTitle"))) : nullptr;
	auto HasPositiveCachedGeometry = [](const UWidget* Widget)
	{
		if (!Visible(Widget)) return false;
		const FGeometry Geometry = Widget->GetCachedGeometry();
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2D A = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D B = Geometry.LocalToAbsolute(Size);
		return Size.X > 0 && Size.Y > 0 && B.X > A.X && B.Y > A.Y;
	};
	return bReference && Modal.IsValid() && !Modal->IsModalOpen() && Screen.IsValid()
		&& Screen->IsScreenOpen() && HasPositiveCachedGeometry(Screen->GetWidgetFromName(TEXT("CityManagerListScroll")))
		&& HasPositiveCachedGeometry(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))
		&& (!Title || Title->GetText().ToString() == TEXT("城市管理"));
}
bool URokCityBuffReferenceAuditSubsystem::ReadyClosed() const
{
	return Modal.IsValid() && !Modal->IsModalOpen() && !Visible(Modal.Get())
		&& Screen.IsValid() && !Screen->IsScreenOpen() && Hud.IsValid() && Visible(Hud.Get());
}
TSharedRef<FJsonObject> URokCityBuffReferenceAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Json->SetBoolField(TEXT("reference"), bReference);
	Json->SetStringField(TEXT("authority"), bReference ? TEXT("local_fixture") : TEXT("server"));
	Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	Json->SetBoolField(TEXT("modal_reference_flag"), Modal.IsValid() && Modal->bReferenceBuffListCaptureState);
	Json->SetBoolField(TEXT("city_manager_open"), Screen.IsValid() && Screen->IsScreenOpen());
	Json->SetBoolField(TEXT("business_boundary"), Boundary());
	Json->SetObjectField(TEXT("business"), BusinessState());
	auto Widgets = MakeShared<FJsonObject>();
	if (Modal.IsValid()) for (const TCHAR* Name : {TEXT("BuffListPopupTitle"), TEXT("BuffListReferencePreviewNotice"),
		TEXT("BuffListServiceBoundaryTitle"), TEXT("BuffListDefinitionRow_0"), TEXT("BuffListDefinitionRow_1"),
		TEXT("BuffListDefinitionTitle_0"), TEXT("BuffListDefinitionTitle_1"),
		TEXT("BuffListDefinitionFields_0"), TEXT("BuffListDefinitionFields_1"),
		TEXT("BuffListDefinitionIcon_0"), TEXT("BuffListDefinitionIcon_1"),
		TEXT("BuffListMoreButton"), TEXT("ModalDismissMask")})
		Widgets->SetObjectField(Name, WidgetState(Modal->GetWidgetFromName(Name)));
	if (Screen.IsValid()) for (const TCHAR* Name : {TEXT("CityManagerListScroll"), TEXT("FeatureFrameClose")})
		Widgets->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(Name)));
	Json->SetObjectField(TEXT("widgets"), Widgets);
	return Json;
}
bool URokCityBuffReferenceAuditSubsystem::Click(UButton* Target, const TCHAR* Label, bool bOutside)
{
	if (!Target || !Visible(Target) || !Target->GetIsEnabled()) return Expect(false, TEXT("Target UButton missing/hidden/disabled"));
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
	if (Size.X <= 0 || Size.Y <= 0) return Expect(false, TEXT("Target has zero cached geometry"));
	if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
		|| !StableBottomRight.Equals(BottomRight, .25f))
	{
		StableTarget = Target; StableTopLeft = TopLeft; StableBottomRight = BottomRight;
		StableFrame = GFrameCounter; ClickStableFrames = 1; return false;
	}
	if (GFrameCounter <= StableFrame) return false;
	StableFrame = GFrameCounter;
	if (++ClickStableFrames < 2) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Center = bOutside ? Geometry.LocalToAbsolute(FVector2D(70,70)) : Geometry.LocalToAbsolute(Size * .5f);
	if (bOutside)
	{
		const UWidget* Popup = Modal->GetWidgetFromName(TEXT("BuffListAnimationRoot"));
		const FGeometry PopupGeometry = Popup ? Popup->GetCachedGeometry() : FGeometry();
		const FVector2D PopupA = PopupGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D PopupB = PopupGeometry.LocalToAbsolute(PopupGeometry.GetLocalSize());
		if (!Expect(Popup && !(Center.X >= PopupA.X && Center.X <= PopupB.X
			&& Center.Y >= PopupA.Y && Center.Y <= PopupB.Y), TEXT("Dismiss click is inside popup"))) return false;
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> OwnWindow = Viewport ? Viewport->GetWindow() : nullptr;
	if (!Expect(OwnWindow.IsValid() && PrepareOwnWindowFocus(), TEXT("Audit lost owned foreground window"))) return false;
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30,-30));
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	const TSharedRef<SWidget> TargetSlate = Target->TakeWidget();
	int32 TargetIndex = INDEX_NONE;
	bool bUmgPath = false;
	TArray<TSharedPtr<FJsonValue>> PathTypes;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		const SWidget* Node = &Path.Widgets[Index].Widget.Get();
		const FString Type = Node->GetTypeAsString();
		PathTypes.Add(MakeShared<FJsonValueString>(Type));
		bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
		if (Node == &TargetSlate.Get()) TargetIndex = Index;
	}
	bool bForeground = false;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = OwnWindow->GetNativeWindow();
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD Pid = 0;
	if (Handle) ::GetWindowThreadProcessId(Handle, &Pid);
	bForeground = Handle && ::GetForegroundWindow() == Handle && Pid == ::GetCurrentProcessId();
#endif
	const bool bExact = Path.IsValid() && &Path.GetWindow().Get() == OwnWindow.Get()
		&& TargetIndex != INDEX_NONE && Path.Widgets[TargetIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"))
		&& bUmgPath && Target->IsHovered() && bForeground;
	if (!Expect(bExact, TEXT("Slate hit path did not prove exact owned UButton"))) return false;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("label"), Label);
	Action->SetStringField(TEXT("target"), Target->GetName());
	Action->SetStringField(TEXT("method"), TEXT("ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
	Action->SetObjectField(TEXT("absolute_point"), Point(Center));
	Action->SetObjectField(TEXT("absolute_top_left"), Point(TopLeft));
	Action->SetObjectField(TEXT("absolute_bottom_right"), Point(BottomRight));
	Action->SetArrayField(TEXT("slate_path_types"), PathTypes);
	Action->SetBoolField(TEXT("exact_target"), bExact);
	Action->SetBoolField(TEXT("owned_foreground"), bForeground);
	Action->SetBoolField(TEXT("outside_popup"), bOutside);
	Action->SetObjectField(TEXT("before"), State());
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	const TSharedRef<FJsonObject> ImmediateAfter = State();
	Action->SetNumberField(TEXT("immediate_frame"), static_cast<double>(GFrameCounter));
	Action->SetObjectField(TEXT("immediate_after"), ImmediateAfter);
	Action->SetNumberField(TEXT("after_frame"), static_cast<double>(GFrameCounter));
	Action->SetBoolField(TEXT("after_layout_settled"), false);
	Action->SetObjectField(TEXT("after"), ImmediateAfter);
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	StableTarget.Reset(); ClickStableFrames = 0;
	return Expect(Down && Up && Boundary(), TEXT("Slate input unhandled or business/camera/HTTP changed"));
}
void URokCityBuffReferenceAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("requested_state"), State());
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	Pending->SetStringField(TEXT("png"), PendingPath);
	NextStep = After; bScreenshotDone = false;
	IFileManager::Get().Delete(*PendingPath, false, true);
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}
void URokCityBuffReferenceAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	TArray<uint8> Bytes;
	if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
		&& FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
		&& FMemory::Memcmp(Bytes.GetData()+12, "IHDR", 4) == 0
		&& PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
		&& Boundary(), TEXT("Processed screenshot PNG/extent/boundary invalid"))) return;
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(Bytes.Num()));
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("processed_state"), State());
	ScreenshotFrame = GFrameCounter; bScreenshotDone = true;
}
void URokCityBuffReferenceAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(Boundary(), TEXT("Post-screenshot boundary changed"))) return;
	Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset(); Save(false); Enter(NextStep);
}
void URokCityBuffReferenceAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}
bool URokCityBuffReferenceAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.city_buff_reference.runtime_audit.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Boundary()
		&& Captures.Num() == (bReference ? 3 : 2) && Actions.Num() == (bReference ? 2 : 1));
	Json->SetStringField(TEXT("route"), bReference ? TEXT("buff_reference") : TEXT("buff"));
	Json->SetStringField(TEXT("authority"), bReference ? TEXT("local_fixture") : TEXT("server"));
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
	Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
	Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	Json->SetBoolField(TEXT("active_buff_state_accepted"), false);
	Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetObjectField(TEXT("baseline_business"), BaselineBusinessJson.IsValid() ? BaselineBusinessJson.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetObjectField(TEXT("final_state"), State());
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokCityBuffReferenceAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && bBaselineSet && Boundary()
		&& Captures.Num() == (bReference ? 3 : 2) && Actions.Num() == (bReference ? 2 : 1);
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("City buff audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok city buff audit %s: %s"), Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokCityBuffReferenceAuditSubsystem::Tick(float DeltaTime)
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
	auto Stable = [this](bool bReady) { StableFrames = bReady ? StableFrames + 1 : 0; return StableFrames >= 2; };
	auto SettleLastAction = [this](const TCHAR* ExpectedLabel)
	{
		if (!Expect(!Actions.IsEmpty() && Actions.Last().IsValid()
			&& Actions.Last()->AsObject().IsValid()
			&& Actions.Last()->AsObject()->GetStringField(TEXT("label")) == ExpectedLabel,
			TEXT("Settled action receipt missing or misordered"))) return false;
		const TSharedPtr<FJsonObject> Action = Actions.Last()->AsObject();
		if (!Expect(GFrameCounter > static_cast<uint64>(Action->GetNumberField(TEXT("immediate_frame")))
			&& Boundary(), TEXT("Settled action was not a later unchanged frame"))) return false;
		Action->SetObjectField(TEXT("after"), State());
		Action->SetNumberField(TEXT("after_frame"), static_cast<double>(GFrameCounter));
		Action->SetBoolField(TEXT("after_layout_settled"), true);
		return true;
	};
	switch (Step)
	{
	case 0: if (!bBegun) bBegun = Begin(); if (bBegun && PrepareOwnWindowFocus()) Enter(1); break;
	case 1: if (Boundary() && Stable(ReadyOpen())) Capture(TEXT("00_buff_popup"), 2); break;
	case 2:
		if (Boundary() && Click(Cast<UButton>(Modal->GetWidgetFromName(
			bReference ? TEXT("BuffListMoreButton") : TEXT("ModalDismissMask"))),
			bReference ? TEXT("more_to_city_manager") : TEXT("outside_close"), !bReference)) Enter(3);
		break;
	case 3:
		if (Boundary() && Stable(bReference ? ReadyManager() : ReadyClosed()))
			if (SettleLastAction(bReference ? TEXT("more_to_city_manager") : TEXT("outside_close")))
				Capture(bReference ? TEXT("01_city_manager") : TEXT("01_hud_return"), bReference ? 4 : 6);
		break;
	case 4:
		if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))),
			TEXT("close_city_manager_to_hud"))) Enter(5);
		break;
	case 5:
		if (Boundary() && Stable(ReadyClosed()) && SettleLastAction(TEXT("close_city_manager_to_hud")))
			Capture(TEXT("02_hud_return"), 6);
		break;
	case 6: Finish(); break;
	default: Finish(TEXT("Invalid audit step")); break;
	}
}
