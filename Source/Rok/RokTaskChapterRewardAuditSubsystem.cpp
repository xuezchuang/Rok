#include "RokTaskChapterRewardAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTaskChapterConfigPreviewData.h"
#include "RokTaskChapterRewardPreviewData.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
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
#include "RenderingThread.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokTaskChapterRewardAudit");
constexpr TCHAR CommonAtlas[] = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
constexpr int32 ChapterCounts[] = {3, 4, 6, 6, 7, 7, 6};
constexpr int32 RequiredCaptureCount = 18;
constexpr int32 RequiredActionCount = 18;

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
		const FVector2D Desired = Text->GetDesiredSize();
		Json->SetObjectField(TEXT("text_slate_desired_size"), Point(Desired));
		Json->SetStringField(TEXT("text_desired_alignment_basis"), TEXT("slate_cached_geometry_scaled"));
		Json->SetObjectField(TEXT("text_desired_absolute_top_left"),
			Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
		Json->SetObjectField(TEXT("text_desired_absolute_bottom_right"),
			Point(Geometry.LocalToAbsolute(Desired)));
		if (const UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Text->Slot))
		{
			Json->SetBoolField(TEXT("fit_slot_right_aligned"),
				ScaleSlot->GetHorizontalAlignment() == HAlign_Right);
			Json->SetBoolField(TEXT("fit_slot_top_aligned"),
				ScaleSlot->GetVerticalAlignment() == VAlign_Top);
		}
	}
	if (const UScaleBox* Fit = Cast<UScaleBox>(Widget))
	{
		Json->SetBoolField(TEXT("scale_to_fit"), Fit->GetStretch() == EStretch::ScaleToFit);
		Json->SetBoolField(TEXT("down_only"), Fit->GetStretchDirection() == EStretchDirection::DownOnly);
	}
	if (const UImage* Image = Cast<UImage>(Widget))
	{
		const FSlateBrush& Brush = Image->GetBrush();
		const UObject* Resource = Brush.GetResourceObject();
		const UTexture2D* Texture = Cast<UTexture2D>(Resource);
		Json->SetStringField(TEXT("brush_resource"), Resource ? Resource->GetPathName() : FString());
		Json->SetObjectField(TEXT("texture_size"), Point(Texture
			? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D::ZeroVector));
		const FBox2f Uv = Brush.GetUVRegion();
		Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(Uv.Min.X, Uv.Min.Y)));
		Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(Uv.Max.X, Uv.Max.Y)));
		const FTextureResource* TextureResource = Texture ? Texture->GetResource() : nullptr;
		const FTextureRHIRef Rhi = TextureResource ? TextureResource->TextureRHI : FTextureRHIRef();
		Json->SetBoolField(TEXT("rhi_ready"), Rhi.IsValid());
		Json->SetObjectField(TEXT("rhi_extent"), Point(Rhi.IsValid()
			? FVector2D(Rhi->GetDesc().Extent.X, Rhi->GetDesc().Extent.Y) : FVector2D::ZeroVector));
	}
	return Json;
}
}

bool URokTaskChapterRewardAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokTaskChapterRewardAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokTaskChapterRewardAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokTaskChapterRewardAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokTaskChapterRewardAuditSubsystem, STATGROUP_Tickables);
}
void URokTaskChapterRewardAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/TaskChapterRewardAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokTaskChapterRewardAuditSubsystem::ScreenshotProcessed);
}
void URokTaskChapterRewardAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokTaskChapterRewardAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokTaskChapterRewardAuditSubsystem::Expect(bool Condition, const TCHAR* Error)
{
	if (!Condition) Finish(Error);
	return Condition;
}
bool URokTaskChapterRewardAuditSubsystem::PrepareOwnWindowFocus()
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
		&& Client.bottom - Client.top == 810, TEXT("Task Chapter Reward requires own 1440x810 GameApp window"))) return false;
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
	return Expect(false, TEXT("Task Chapter Reward requires Windows game window"));
#endif
}

TSharedRef<FJsonObject> URokTaskChapterRewardAuditSubsystem::BusinessState() const
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
bool URokTaskChapterRewardAuditSubsystem::Boundary() const
{
	return Identity() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& Controller.IsValid()
		&& Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == BaselinePrimary
		&& !BaselineBusiness.IsEmpty() && EncodeAuditJson(BusinessState()) == BaselineBusiness;
}

TSharedRef<FJsonObject> URokTaskChapterRewardAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("business"), BusinessState());
	Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Json->SetNumberField(TEXT("primary_press_count"), Controller.IsValid()
		? static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount) : -1.0);
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	if (!Screen.IsValid()) return Json;
	const UWidget* CurrentModeCanvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	const UWidget* ChapterRoot = Screen->GetWidgetFromName(TEXT("TaskChapterRoot"));
	Json->SetBoolField(TEXT("chapter_root_attached_to_mode_canvas"),
		ChapterRoot && ChapterRoot->GetParent() == CurrentModeCanvas && CurrentModeCanvas);
#if !UE_BUILD_SHIPPING
	Json->SetNumberField(TEXT("task_page"), Screen->GetSelectedTaskPageForAudit());
	Json->SetNumberField(TEXT("chapter_id"), Screen->GetSelectedTaskChapterGroupIdForAudit());
	Json->SetBoolField(TEXT("metadata_known"), Screen->IsSelectedTaskChapterMetadataKnownForAudit());
	Json->SetNumberField(TEXT("source_task_count"), Screen->GetTaskChapterSourceTaskCountForAudit());
	Json->SetNumberField(TEXT("source_group_count"), Screen->GetTaskChapterSourceGroupCountForAudit());
	TArray<TSharedPtr<FJsonValue>> Ids;
	for (int32 Id : Screen->GetRenderedTaskChapterTaskIdsForAudit()) Ids.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("rendered_task_ids"), Ids);
	TArray<TSharedPtr<FJsonValue>> TaskRewardIds;
	for (int32 Id : Screen->GetRenderedTaskChapterTaskRewardPackageIdsForAudit())
		TaskRewardIds.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("rendered_task_reward_package_ids"), TaskRewardIds);
	TArray<TSharedPtr<FJsonValue>> AggregateIds;
	for (int32 Id : Screen->GetRenderedTaskChapterAggregateRewardPackageIdsForAudit())
		AggregateIds.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("rendered_aggregate_reward_package_ids"), AggregateIds);
#endif
	auto Widgets = MakeShared<FJsonObject>();
	const TCHAR* Selectors[] = {TEXT("FeatureModeCanvas"), TEXT("TaskPage_0"), TEXT("TaskPage_1"),
		TEXT("TaskChapterRoot"), TEXT("TaskChapterGroupTitle"), TEXT("TaskChapterMetaNotice"),
		TEXT("TaskChapterScroll"), TEXT("TaskChapterAggregateRewardUnavailable"),
		TEXT("FeatureFrameClose")};
	for (const TCHAR* Name : Selectors)
		Widgets->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(Name)));
	for (int32 Chapter = 1; Chapter <= 7; ++Chapter)
	{
		const FString Name = FString::Printf(TEXT("TaskChapterGroupButton_%d"), Chapter);
		Widgets->SetObjectField(Name, WidgetState(Screen->GetWidgetFromName(*Name)));
		const FString Center = Name + TEXT("Background_Center");
		Widgets->SetObjectField(Center, WidgetState(Screen->GetWidgetFromName(*Center)));
	}
	if (const UScrollBox* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll"))))
	{
		Json->SetNumberField(TEXT("scroll_offset"), Scroll->GetScrollOffset());
		Json->SetNumberField(TEXT("scroll_end"), Scroll->GetScrollOffsetOfEnd());
	}
	if (Json->HasField(TEXT("rendered_task_ids")))
	{
		for (const TSharedPtr<FJsonValue>& Value : Json->GetArrayField(TEXT("rendered_task_ids")))
		{
			const int32 Id = static_cast<int32>(Value->AsNumber());
			const FString Base = FString::Printf(TEXT("TaskChapterTaskRow_%d"), Id);
			Widgets->SetObjectField(Base, WidgetState(Screen->GetWidgetFromName(*Base)));
			Widgets->SetObjectField(FString::Printf(TEXT("TaskChapterTaskTitle_%d"), Id),
				WidgetState(Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskChapterTaskTitle_%d"), Id))));
			Widgets->SetObjectField(FString::Printf(TEXT("TaskChapterTaskDescription_%d"), Id),
				WidgetState(Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskChapterTaskDescription_%d"), Id))));
			Widgets->SetObjectField(FString::Printf(TEXT("TaskChapterTaskClaim_%d"), Id),
				WidgetState(Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskChapterTaskClaim_%d"), Id))));
			Widgets->SetObjectField(FString::Printf(TEXT("TaskChapterTaskGo_%d"), Id),
				WidgetState(Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskChapterTaskGo_%d"), Id))));
		}
	}
#if !UE_BUILD_SHIPPING
	const int32 SelectedChapter = Screen->GetSelectedTaskChapterGroupIdForAudit();
	if (Screen->GetSelectedTaskPageForAudit() == 0 && SelectedChapter >= 1 && SelectedChapter <= 7)
	{
		for (const FRokTaskChapterRewardGroup& Group : RewardGroups)
		{
			if (Group.ChapterId != SelectedChapter) continue;
			for (int32 Index = 0; Index < Group.Rows.Num(); ++Index)
			{
				const FString Base = Group.bChapterAggregate
					? FString::Printf(TEXT("TaskChapterAggregateReward_%d_%d"), SelectedChapter, Index)
					: FString::Printf(TEXT("TaskChapterTaskReward_%d_%d"), Group.TaskId, Index);
				Widgets->SetObjectField(Base, WidgetState(Screen->GetWidgetFromName(*Base)));
				Widgets->SetObjectField(Base + TEXT("Icon"), WidgetState(Screen->GetWidgetFromName(*(Base + TEXT("Icon")))));
				if (Group.bChapterAggregate)
					Widgets->SetObjectField(Base + TEXT("QuantityFit"),
						WidgetState(Screen->GetWidgetFromName(*(Base + TEXT("QuantityFit")))));
				Widgets->SetObjectField(Base + TEXT("Quantity"), WidgetState(Screen->GetWidgetFromName(*(Base + TEXT("Quantity")))));
			}
		}
	}
#endif
	Json->SetObjectField(TEXT("widgets"), Widgets);
	return Json;
}

bool URokTaskChapterRewardAuditSubsystem::Ready(int32 Page, int32 Chapter) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!Boundary() || !Screen.IsValid() || !Screen->IsScreenOpen()
		|| Screen->GetSelectedTaskPageForAudit() != Page
		|| !ModeReady(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")))) return false;
	const FGeometry ModeGeometry = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))->GetCachedGeometry();
	if (!ModeGeometry.GetLocalSize().Equals(FVector2D(1440.0, 810.0), 0.1)) return false;
	if (Page == 1) return (!Screen->GetWidgetFromName(TEXT("TaskChapterRoot"))
		|| Screen->GetWidgetFromName(TEXT("TaskChapterRoot"))->GetParent()
			!= Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")))
		&& Screen->GetRenderedTaskChapterTaskIdsForAudit().IsEmpty()
		&& Screen->GetRenderedTaskChapterTaskRewardPackageIdsForAudit().IsEmpty()
		&& Screen->GetRenderedTaskChapterAggregateRewardPackageIdsForAudit().IsEmpty()
		&& !Screen->IsSelectedTaskChapterMetadataKnownForAudit();
	if (Page != 0 || Chapter < 1 || Chapter > 7
		|| Screen->GetSelectedTaskChapterGroupIdForAudit() != Chapter
		|| Screen->IsSelectedTaskChapterMetadataKnownForAudit() != (Chapter < 7)
		|| Screen->GetTaskChapterSourceTaskCountForAudit() != 39
		|| Screen->GetTaskChapterSourceGroupCountForAudit() != 7
		|| !Screen->GetWidgetFromName(TEXT("TaskChapterRoot"))
		|| Screen->GetWidgetFromName(TEXT("TaskChapterRoot"))->GetParent()
			!= Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"))
		|| !Visible(Screen->GetWidgetFromName(TEXT("TaskChapterRoot")))
		|| !Visible(Screen->GetWidgetFromName(TEXT("TaskChapterScroll")))
		|| !CommonAtlasImageReady(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_1Background_Center")))
		|| Screen->GetRenderedTaskChapterTaskIdsForAudit().Num() != ChapterCounts[Chapter - 1]) return false;
	for (int32 Order = 1; Order <= ChapterCounts[Chapter - 1]; ++Order)
	{
		if (ExpectedTaskIdsByChapter[Chapter - 1].Num() != ChapterCounts[Chapter - 1]) return false;
		const int32 Id = ExpectedTaskIdsByChapter[Chapter - 1][Order - 1];
		if (Screen->GetRenderedTaskChapterTaskIdsForAudit()[Order - 1] != Id
			|| !Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskChapterTaskRow_%d"), Id))
			|| !Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskChapterTaskTitle_%d"), Id))
			|| !Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskChapterTaskDescription_%d"), Id))) return false;
	}
	TArray<int32> ExpectedTaskRewardIds;
	TArray<int32> ExpectedAggregateIds;
	for (const FRokTaskChapterRewardGroup& Group : RewardGroups)
	{
		if (Group.ChapterId != Chapter) continue;
		for (const FRokTaskChapterRewardPreviewRow& Row : Group.Rows)
			(Group.bChapterAggregate ? ExpectedAggregateIds : ExpectedTaskRewardIds).Add(Row.PackageRowId);
	}
	if (Screen->GetRenderedTaskChapterTaskRewardPackageIdsForAudit() != ExpectedTaskRewardIds
		|| Screen->GetRenderedTaskChapterAggregateRewardPackageIdsForAudit() != ExpectedAggregateIds) return false;
	return true;
#endif
}

bool URokTaskChapterRewardAuditSubsystem::Begin()
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
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	if (!Expect(Controller.IsValid() && Buildings.IsValid() && Runtime.IsValid() && Server.IsValid()
		&& Hud.IsValid() && Screen.IsValid() && !Screen->IsScreenOpen()
		&& Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
		&& Server->GetSuppressedLocalRequestCount() == 0
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), 0.1),
		TEXT("Task Chapter Reward expects idle local-only 1440x810 HUD"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Task Chapter Reward account identity is not local preview"))) return false;
	TArray<FRokTaskChapterPreviewRow> SourceChapters;
	TArray<FRokTaskChapterTaskPreviewRow> SourceTasks;
	FString SourceError;
	if (!Expect(RokTaskChapterConfigPreviewData::LoadAll(SourceChapters, SourceTasks, SourceError)
		&& SourceChapters.Num() == 7 && SourceTasks.Num() == 39,
		TEXT("Task Chapter Reward source catalog failed to load"))) return false;
	for (int32 Index = 0; Index < 7; ++Index)
	{
		if (!Expect(SourceChapters[Index].Id == Index + 1
			&& SourceChapters[Index].TaskIds.Num() == ChapterCounts[Index]
			&& SourceChapters[Index].bMetadataKnown == (Index < 6),
			TEXT("Task Chapter Reward source grouping/metadata differs"))) return false;
		ExpectedTaskIdsByChapter[Index] = SourceChapters[Index].TaskIds;
	}
	FString RewardError;
	if (!Expect(RokTaskChapterRewardPreviewData::LoadAll(RewardGroups, RewardError)
		&& RewardGroups.Num() == 45,
		TEXT("Task Chapter Reward source catalog failed to load"))) return false;
	int32 RewardRows = 0;
	TSet<FString> AtlasPaths;
	for (const FRokTaskChapterRewardGroup& Group : RewardGroups)
	{
		if (!Expect(!Group.Rows.IsEmpty() && Group.ChapterId >= 1 && Group.ChapterId <= 7
			&& (Group.bChapterAggregate ? Group.ChapterId < 7 && Group.TaskId == 0
				: ExpectedTaskIdsByChapter[Group.ChapterId - 1].Contains(Group.TaskId)),
			TEXT("Task Chapter Reward group boundary invalid"))) return false;
		RewardRows += Group.Rows.Num();
		for (const FRokTaskChapterRewardPreviewRow& Row : Group.Rows) AtlasPaths.Add(Row.AtlasObjectPath);
	}
	if (!Expect(RewardRows == 123 && AtlasPaths.Num() == 5,
		TEXT("Task Chapter Reward row/atlas count differs"))) return false;
	for (const FString& AtlasPath : AtlasPaths)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *AtlasPath);
		if (!Expect(Texture != nullptr, TEXT("Task Chapter Reward cold atlas missing"))) return false;
		Texture->UpdateResource();
	}
	FlushRenderingCommands();
	for (const FString& AtlasPath : AtlasPaths)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *AtlasPath);
		const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
		const FTextureRHIRef Rhi = Resource ? Resource->TextureRHI : FTextureRHIRef();
		if (!Expect(Rhi.IsValid() && Rhi->GetDesc().Extent.X == Texture->GetSizeX()
			&& Rhi->GetDesc().Extent.Y == Texture->GetSizeY(),
			TEXT("Task Chapter Reward cold atlas RHI not ready at full extent"))) return false;
	}
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	BaselineBusiness = EncodeAuditJson(BusinessState());
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokTaskChapterRewardAuditSubsystem.h"),
		TEXT("Source/Rok/RokTaskChapterRewardAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokTaskChapterRewardRuntime.py"),
		TEXT("Source/Rok/RokTaskChapterConfigPreviewData.h"),
		TEXT("Source/Rok/RokTaskChapterConfigPreviewData.cpp"),
		TEXT("Source/Rok/RokTaskChapterRewardPreviewData.h"),
		TEXT("Source/Rok/RokTaskChapterRewardPreviewData.cpp"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"),
		TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"),
		TEXT("Source/Rok/RokServerSubsystem.cpp"),
		TEXT("Saved/RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json"),
		TEXT("Saved/RokUnityMigration/TaskChapterRewardPreview/v1/catalog.json"),
		TEXT("Scripts/BuildRokTaskChapterRewardPreview.py"),
		TEXT("Scripts/VerifyRokTaskChapterRewardPreview.py"),
		TEXT("Saved/RokUnityMigration/TaskChapterRewardPreview/v1/verify.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/task_chapter_reward_data_implementation_v1.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/task_chapter_reward_ui_implementation_revision4.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009QuestChapterArtAndHallCamera/task_chapter_row_art_inventory_v1.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/parallel_migration_batch_20261009T041001199603Z_quest_chapter_hall.json"),
		TEXT("Scripts/BuildRokTaskChapterConfigPreview.py"),
		TEXT("Scripts/VerifyRokTaskChapterConfigPreview.py"),
		TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"),
		TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestView.cs"),
		TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs"),
		TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Win_Quest.prefab")};
	for (const TCHAR* Path : Required)
	{
		const FString Hash = FileSha(FPaths::ProjectDir() / Path);
		if (!Expect(Hash.Len() == 64, TEXT("Task Chapter Reward source input missing"))) return false;
		Inputs->SetStringField(Path, Hash);
	}
	return true;
}

bool URokTaskChapterRewardAuditSubsystem::Click(UWidget* Target, const TCHAR* Label)
{
	if (!Expect(Boundary() && Target && Visible(Target) && Target->GetIsEnabled()
		&& FSlateApplication::IsInitialized(), TEXT("Task Chapter Reward Slate target unavailable"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("Task Chapter Reward target has no cached geometry"))) return false;
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
	const FString Terminal = Path.IsValid() && Path.Widgets.Num() ? Path.Widgets.Last().Widget->GetTypeAsString() : FString();
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	if (!Expect(Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get()) && Target->IsHovered()
		&& bUmg && Terminal.Contains(TEXT("SButton")) && bOwnWindow && bForeground,
		TEXT("Task Chapter Reward Slate hit was not own-window UMG SButton"))) return false;
	const uint64 Before = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const uint64 After = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("label"), Label);
	Action->SetStringField(TEXT("method"), TEXT("Slate ProcessMouseMoveEvent+DownEvent+UpEvent"));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetObjectField(TEXT("absolute_center"), Point(Center));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetStringField(TEXT("terminal_type"), Terminal);
	Action->SetBoolField(TEXT("own_game_window"), bOwnWindow);
	Action->SetBoolField(TEXT("game_window_foreground"), bForeground);
	Action->SetBoolField(TEXT("target_in_hit_path"), true);
	Action->SetBoolField(TEXT("target_hovered"), Target->IsHovered());
	Action->SetBoolField(TEXT("down_handled"), Down);
	Action->SetBoolField(TEXT("up_handled"), Up);
	Action->SetNumberField(TEXT("primary_before"), static_cast<double>(Before));
	Action->SetNumberField(TEXT("primary_after"), static_cast<double>(After));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Down && Up && Before == After && Boundary(),
		TEXT("Task Chapter Reward UI click leaked to world or changed business state"));
}

bool URokTaskChapterRewardAuditSubsystem::WheelChapterScroll(int32 Chapter)
{
	UScrollBox* Scroll = Screen.IsValid()
		? Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll"))) : nullptr;
	if (!Expect(Ready(0, Chapter) && Chapter >= 3 && Chapter <= 7 && Scroll
		&& Scroll->GetScrollOffsetOfEnd() > 0,
		TEXT("Chapter 3-7 must have a scrollable source task list"))) return false;
	const FGeometry Geometry = Scroll->GetCachedGeometry();
	const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	FSlateApplication& App = FSlateApplication::Get();
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	TArray<TSharedPtr<FJsonValue>> Types;
	bool bScroll = false;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const FString Type = Path.Widgets[Index].Widget->GetTypeAsString();
		Types.Add(MakeShared<FJsonValueString>(Type));
		bScroll |= Type.Contains(TEXT("SScrollBox"));
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwn = Path.IsValid() && Window.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = true;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	bForeground = Native.IsValid() && ::GetForegroundWindow() == static_cast<HWND>(Native->GetOSWindowHandle());
#endif
	if (!Expect(bOwn && bScroll && bForeground, TEXT("Chapter scroll wheel must hit foreground own-window SScrollBox"))) return false;
	const double Before = Scroll->GetScrollOffset();
	const uint64 PrimaryBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	const bool Handled = App.ProcessMouseWheelOrGestureEvent(
		FPointerEvent(0, Center, Center, TSet<FKey>(), EKeys::Invalid, -12.0f, FModifierKeysState()), nullptr);
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("label"), *FString::Printf(TEXT("scroll_chapter%d_source_rewards"), Chapter));
	Action->SetNumberField(TEXT("chapter"), Chapter);
	Action->SetStringField(TEXT("method"), TEXT("Slate ProcessMouseWheelOrGestureEvent"));
	Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Action->SetObjectField(TEXT("absolute_center"), Point(Center));
	Action->SetArrayField(TEXT("hit_path_types"), Types);
	Action->SetBoolField(TEXT("own_game_window"), bOwn);
	Action->SetBoolField(TEXT("game_window_foreground"), bForeground);
	Action->SetBoolField(TEXT("scroll_hit_path"), bScroll);
	Action->SetBoolField(TEXT("handled"), Handled);
	Action->SetNumberField(TEXT("offset_before"), Before);
	Action->SetNumberField(TEXT("primary_before"), static_cast<double>(PrimaryBefore));
	Action->SetNumberField(TEXT("primary_after"), static_cast<double>(
		Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(Handled && Boundary(), TEXT("Chapter Slate wheel was unhandled or changed business state"));
}

void URokTaskChapterRewardAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested() && Boundary(),
		TEXT("Task Chapter Reward screenshot request invalid"))) return;
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
void URokTaskChapterRewardAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	const FString Hash = FileSha(PendingPath);
	if (!Expect(Hash.Len() == 64 && Boundary(), TEXT("Task Chapter Reward PNG/source state absent during screenshot"))) return;
	Pending->SetStringField(TEXT("png_sha256"), Hash);
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(IFileManager::Get().FileSize(*PendingPath)));
	Pending->SetObjectField(TEXT("processed_state"), State());
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokTaskChapterRewardAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(NextStep);
}
void URokTaskChapterRewardAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep;
	StepSince = Elapsed;
	StableFrames = 0;
}
bool URokTaskChapterRewardAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.task_chapter_reward.runtime_audit.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty()
		&& Captures.Num() == RequiredCaptureCount && Actions.Num() == RequiredActionCount);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), TEXT("Real HUD/TaskInfo Slate-only chapter source reward configuration preview; account chapter progress/ownership unknown; no claims, GoScript, wallet, queue or HTTP mutation"));
	Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetObjectField(TEXT("final_state"), State());
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	return FFileHelper::SaveStringToFile(EncodeAuditJson(Json), *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokTaskChapterRewardAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (FSlateApplication::IsInitialized() && OriginalPointer != FVector2D::ZeroVector)
		FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && Captures.Num() == RequiredCaptureCount
		&& Actions.Num() == RequiredActionCount && Boundary();
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Task Chapter Reward audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok Task Chapter Reward audit %s: %s"), Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!Complete) UE_LOG(LogTemp, Error, TEXT("Task Chapter Reward audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}

void URokTaskChapterRewardAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !Identity()) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (Step == 0 && Elapsed > 25.0)
	{
		Finish(TEXT("Task Chapter Reward own-window startup/focus timeout"));
		return;
	}
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 25.0) Finish(TEXT("Task Chapter Reward screenshot timeout"));
		else AdvanceCapture();
		return;
	}
	if (Step > 0 && Elapsed - StepSince > 35.0)
	{
		Finish(FString::Printf(TEXT("Task Chapter Reward step %d timeout"), Step));
		return;
	}
	auto Stable = [this](int32 Page, int32 Chapter)
	{
		StableFrames = Ready(Page, Chapter) ? StableFrames + 1 : 0;
		return StableFrames >= 2;
	};
	switch (Step)
	{
	case 0:
		if (!bBegun) bBegun = Begin();
		if (bBegun && PrepareOwnWindowFocus()) Enter(1);
		break;
	case 1: if (PrepareOwnWindowFocus() && Click(Hud->GetWidgetFromName(TEXT("TaskEntryButton")), TEXT("open_main_from_hud"))) Enter(2); break;
	case 2: if (Stable(1, 0)) Capture(TEXT("01_main_default"), 3); break;
	case 3: if (Click(Screen->GetWidgetFromName(TEXT("TaskPage_0")), TEXT("open_chapter_tab"))) Enter(4); break;
	case 4: if (Stable(0, 1)) Capture(TEXT("02_chapter1_default"), 5); break;
	case 5: if (Click(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_2")), TEXT("select_chapter2"))) Enter(6); break;
	case 6: if (Stable(0, 2)) Capture(TEXT("03_chapter2"), 7); break;
	case 7: if (Click(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_3")), TEXT("select_chapter3"))) Enter(8); break;
	case 8: if (Stable(0, 3)) Capture(TEXT("04_chapter3_top"), 9); break;
	case 9: if (WheelChapterScroll(3)) Enter(10); break;
	case 10: if (Stable(0, 3) && Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll")))->GetScrollOffset() > 0)
		Capture(TEXT("05_chapter3_scrolled"), 11); break;
	case 11: if (Click(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_4")), TEXT("select_chapter4"))) Enter(12); break;
	case 12: if (Stable(0, 4)) Capture(TEXT("06_chapter4_top"), 13); break;
	case 13: if (WheelChapterScroll(4)) Enter(14); break;
	case 14: if (Stable(0, 4) && Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll")))->GetScrollOffset() > 0)
		Capture(TEXT("07_chapter4_scrolled"), 15); break;
	case 15: if (Click(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_5")), TEXT("select_chapter5"))) Enter(16); break;
	case 16: if (Stable(0, 5)) Capture(TEXT("08_chapter5_top"), 17); break;
	case 17: if (WheelChapterScroll(5)) Enter(18); break;
	case 18: if (Stable(0, 5) && Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll")))->GetScrollOffset() > 0)
		Capture(TEXT("09_chapter5_scrolled"), 19); break;
	case 19: if (Click(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_6")), TEXT("select_chapter6"))) Enter(20); break;
	case 20: if (Stable(0, 6)) Capture(TEXT("10_chapter6_top"), 21); break;
	case 21: if (WheelChapterScroll(6)) Enter(22); break;
	case 22: if (Stable(0, 6) && Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll")))->GetScrollOffset() > 0)
		Capture(TEXT("11_chapter6_scrolled"), 23); break;
	case 23: if (Click(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_7")), TEXT("select_chapter7_missing_metadata"))) Enter(24); break;
	case 24: if (Stable(0, 7)) Capture(TEXT("12_chapter7_top"), 25); break;
	case 25: if (WheelChapterScroll(7)) Enter(26); break;
	case 26: if (Stable(0, 7) && Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll")))->GetScrollOffset() > 0)
		Capture(TEXT("13_chapter7_scrolled"), 27); break;
	case 27: if (Click(Screen->GetWidgetFromName(TEXT("TaskChapterGroupButton_1")), TEXT("return_chapter1"))) Enter(28); break;
	case 28: if (Stable(0, 1)) Capture(TEXT("14_chapter1_return"), 29); break;
	case 29: if (Click(Screen->GetWidgetFromName(TEXT("TaskPage_1")), TEXT("return_main_tab"))) Enter(30); break;
	case 30: if (Stable(1, 0)) Capture(TEXT("15_main_return"), 31); break;
	case 31: if (Click(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")), TEXT("close_task_screen"))) Enter(32); break;
	case 32:
	{
#if !UE_BUILD_SHIPPING
		const bool bCleared = Screen->GetRenderedTaskChapterTaskIdsForAudit().IsEmpty()
			&& Screen->GetRenderedTaskChapterTaskRewardPackageIdsForAudit().IsEmpty()
			&& Screen->GetRenderedTaskChapterAggregateRewardPackageIdsForAudit().IsEmpty()
			&& !Screen->IsSelectedTaskChapterMetadataKnownForAudit();
#else
		const bool bCleared = false;
#endif
		if (!Screen->IsScreenOpen() && bCleared && Boundary() && ++StableFrames >= 2)
			Capture(TEXT("16_closed"), 33);
		break;
	}
	case 33: if (Click(Hud->GetWidgetFromName(TEXT("TaskEntryButton")), TEXT("reopen_main_from_hud"))) Enter(34); break;
	case 34: if (Stable(1, 0)) Capture(TEXT("17_main_reopened"), 35); break;
	case 35: if (Click(Screen->GetWidgetFromName(TEXT("TaskPage_0")), TEXT("reopen_chapter_tab"))) Enter(36); break;
	case 36: if (Stable(0, 1)) Capture(TEXT("18_chapter1_reopened"), 37); break;
	case 37: Finish(); break;
	default: Finish(TEXT("Invalid Task Chapter Reward audit state")); break;
	}
}
