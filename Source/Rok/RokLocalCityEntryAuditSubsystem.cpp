#include "RokLocalCityEntryAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
#include "Serialization/JsonReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

namespace
{
constexpr TCHAR AuditFlag[] = TEXT("-RokLocalCityEntryAudit");
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr int32 ExpectedCaptureCount = 3;

bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}

bool ReadJson(const TCHAR* Relative, TSharedPtr<FJsonObject>& Out)
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / Relative))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out)
		&& Out.IsValid();
}

int32 TaggedInt(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return 0;
	for (const FName Tag : Actor->Tags)
	{
		const FString Value = Tag.ToString();
		if (Value.StartsWith(Prefix)) return FCString::Atoi(*Value.Mid(FCString::Strlen(Prefix)));
	}
	return 0;
}

bool HasTaggedPrefix(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return false;
	for (const FName Tag : Actor->Tags)
	{
		if (Tag.ToString().StartsWith(Prefix)) return true;
	}
	return false;
}

FString BuildingIdentity(int32 TypeId, int32 Level, int32 GridX, int32 GridY)
{
	return FString::Printf(TEXT("type=%d;level=%d;grid=%d,%d"), TypeId, Level, GridX, GridY);
}

FString ActorIdentity(const AActor* Actor)
{
	return BuildingIdentity(TaggedInt(Actor, TEXT("UnityBuildingType_")),
		TaggedInt(Actor, TEXT("UnityBuildingLevel_")),
		TaggedInt(Actor, TEXT("UnityGridX_")),
		TaggedInt(Actor, TEXT("UnityGridY_")));
}

bool IsRuntimeInitialBuilding(const AActor* Actor)
{
	return Actor && Actor->Tags.Contains(FName(TEXT("RokUnityConfigInitBuilding")))
		&& Actor->Tags.Contains(FName(TEXT("RokUnityRuntimeBuildingSource")));
}

template<typename T> TArray<uint8> Snapshot(T Value)
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive Archive(Writer, false);
	T::StaticStruct()->SerializeItem(Archive, &Value, nullptr);
	return Bytes;
}

bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0.0f) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
	{
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0.0f) return false;
	}
	for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (const UUserWidget* User = Cast<UUserWidget>(Outer);
			User && (!User->IsVisible() || User->GetRenderOpacity() <= 0.0f)) return false;
	}
	return true;
}

TSharedRef<FJsonObject> PreviewJson(const FRokLocalConstructionPreview& Preview)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("context_verified"), Preview.bContextVerified);
	Json->SetBoolField(TEXT("source_eligible"), Preview.bSourceEligible);
	Json->SetBoolField(TEXT("can_commit"), Preview.bCanCommit);
	Json->SetBoolField(TEXT("position_evaluated"), Preview.bPositionEvaluated);
	Json->SetNumberField(TEXT("visual_civilization"), Preview.VisualCivilization);
	Json->SetNumberField(TEXT("resolved_age"), Preview.ResolvedAge);
	Json->SetNumberField(TEXT("missing_type_id"), Preview.MissingTypeId);
	Json->SetNumberField(TEXT("required_level"), Preview.RequiredLevel);
	Json->SetNumberField(TEXT("current_level"), Preview.CurrentLevel);
	Json->SetNumberField(TEXT("footprint_width"), Preview.Footprint.X);
	Json->SetNumberField(TEXT("footprint_length"), Preview.Footprint.Y);
	Json->SetNumberField(TEXT("cost_food"), Preview.Cost.Food);
	Json->SetNumberField(TEXT("cost_wood"), Preview.Cost.Wood);
	Json->SetNumberField(TEXT("build_seconds"), Preview.BuildSeconds);
	Json->SetStringField(TEXT("source_model_id"), Preview.SourceModelId);
	Json->SetStringField(TEXT("failure_reason"), Preview.FailureReason);
	return Json;
}
}

bool URokLocalCityEntryAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalCityEntryAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokLocalCityEntryAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}

TStatId URokLocalCityEntryAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokLocalCityEntryAuditSubsystem, STATGROUP_Tickables);
}

void URokLocalCityEntryAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bOwnsWorld = TargetWorld(GetWorld());
	if (!bOwnsWorld) return;
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/LocalCityEntryAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokLocalCityEntryAuditSubsystem::ScreenshotProcessed);
}

void URokLocalCityEntryAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Screen.IsValid() && PlacementHandle.IsValid())
		Screen->OnBuildCityPlacementRequested.Remove(PlacementHandle);
	if (bOwnsWorld && !bFinished)
		Finish(TEXT("City world ended before BuildCity card bridge audit completed"));
	Super::Deinitialize();
}

bool URokLocalCityEntryAuditSubsystem::Expect(bool Condition, const TCHAR* Error)
{
	if (!Condition) Finish(Error);
	return Condition;
}

bool URokLocalCityEntryAuditSubsystem::Identity()
{
	return Expect(TargetWorld(GetWorld()), TEXT("Wrong project/map; no BuildCity action permitted"));
}

void URokLocalCityEntryAuditSubsystem::Enter(int32 Next)
{
	Step = Next;
	StepStarted = Elapsed;
}

bool URokLocalCityEntryAuditSubsystem::Begin()
{
	if (!Identity()) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& !Token.Equals(AuditFlag, ESearchCase::IgnoreCase))
		{
			Finish(TEXT("BuildCity entry audit requires an isolated Rok process"));
			return false;
		}
	}
	const auto Unique = [this](UClass* Class) -> UUserWidget*
	{
		TArray<UUserWidget*> All;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, All, Class, false);
		UUserWidget* Result = nullptr;
		int32 Count = 0;
		for (UUserWidget* Widget : All)
		{
			if (Widget->GetWorld() == GetWorld()) { Result = Widget; ++Count; }
		}
		return Count == 1 ? Result : nullptr;
	};
	Hud = Cast<URokMainHudWidget>(Unique(URokMainHudWidget::StaticClass()));
	Screen = Cast<URokUnityFeatureScreenWidget>(Unique(URokUnityFeatureScreenWidget::StaticClass()));
	Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Runtime = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	if (!Expect(Hud.IsValid() && Visible(Hud.Get()) && Screen.IsValid()
		&& !Screen->IsScreenOpen() && Buildings.IsValid() && Runtime.IsValid()
		&& Runtime->IsRuntimeStateLoaded()
		&& FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), 0.1f)
		&& !Screen->OnBuildCityPlacementRequested.IsBound(),
		TEXT("Expected 1440x810 idle HUD, local state and unbound BuildCity bridge"))) return false;
	const FRokUnityRuntimePlayerProfileState Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0,
		TEXT("BuildCity audit requires unknown account civilization and isolated local preview"))) return false;
	Inputs = MakeShared<FJsonObject>();
	const TCHAR* Required[] = {
		TEXT("Source/Rok/RokLocalCityEntryAuditSubsystem.h"),
		TEXT("Source/Rok/RokLocalCityEntryAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokLocalCityEntryRuntime.py"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
		TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
		TEXT("Source/Rok/RokBuildingUpgradeSubsystem.h"),
		TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"),
		TEXT("Source/Rok/RokLocalCitySimulationContext.cpp"),
		TEXT("Source/Rok/RokMainHudWidget.cpp"),
		TEXT("Source/Rok/RokPlayerController.cpp"),
		TEXT("Saved/RokUnityMigration/CityConstructionGridSources/catalog.json"),
		TEXT("Saved/RokUnityMigration/CityConstructionSources/catalog.json"),
		TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"),
		TEXT("Saved/RokUnityMigration/unity_gameapp_city_runtime_scene_manifest.json"),
		TEXT("Saved/RokUnityMigration/BatchReview20261009/current_city_pose_readback.json"),
		TEXT("ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv"),
		TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildCity/BuildCityMediator.cs"),
		TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/CityGlobalMediator.cs")
	};
	for (const TCHAR* Relative : Required)
	{
		TArray<uint8> Bytes;
		if (!Expect(FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectDir() / Relative))
			&& !Bytes.IsEmpty(), TEXT("Required BuildCity source unavailable"))) return false;
		auto File = MakeShared<FJsonObject>();
		File->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
		File->SetNumberField(TEXT("bytes"), Bytes.Num());
		Inputs->SetObjectField(Relative, File);
	}
	ProfileSnapshot = Snapshot(Profile);
	ResourceSnapshot = Snapshot(Buildings->GetCurrentResources());
	UpgradeQueueCount = Buildings->GetActiveUpgradeCount();
	OwnedQueueCount = Buildings->GetOwnedBuildQueueCount();
	LocalQueueCount = Buildings->GetActiveLocalConstructionCount();
	TSharedPtr<FJsonObject> SourceCity;
	TSharedPtr<FJsonObject> RuntimeScene;
	if (!Expect(ReadJson(TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"), SourceCity)
		&& ReadJson(TEXT("Saved/RokUnityMigration/unity_gameapp_city_runtime_scene_manifest.json"), RuntimeScene),
		TEXT("Source InitBuilding state or generated runtime scene manifest is unavailable"))) return false;
	const TArray<TSharedPtr<FJsonValue>>* SourceRows = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* RuntimeRows = nullptr;
	if (!Expect(SourceCity->TryGetArrayField(TEXT("buildings"), SourceRows)
		&& RuntimeScene->TryGetArrayField(TEXT("building_actors"), RuntimeRows)
		&& SourceCity->GetIntegerField(TEXT("building_count")) == 26
		&& SourceRows->Num() == 26
		&& RuntimeScene->GetIntegerField(TEXT("source_initial_building_count")) == 26
		&& RuntimeScene->GetIntegerField(TEXT("source_skipped_boundary_building_count")) == 2
		&& RuntimeScene->GetIntegerField(TEXT("buildings_spawned")) == RuntimeRows->Num()
		&& RuntimeRows->Num() == 24,
		TEXT("InitBuilding 26-row source and 24-actor city representation contract differs"))) return false;
	TMap<int32, FString> SourceIdentities;
	TSet<int32> BoundaryIds;
	for (const TSharedPtr<FJsonValue>& Value : *SourceRows)
	{
		const TSharedPtr<FJsonObject> Row = Value->AsObject();
		if (!Expect(Row.IsValid(), TEXT("Invalid source InitBuilding row"))) return false;
		const int32 Id = Row->GetIntegerField(TEXT("init_id"));
		const TSharedPtr<FJsonObject> Grid = Row->GetObjectField(TEXT("grid"));
		const TSharedPtr<FJsonObject> Visual = Row->GetObjectField(TEXT("visual"));
		if (!Expect(Id > 0 && !SourceIdentities.Contains(Id) && Grid.IsValid() && Visual.IsValid(),
			TEXT("Source InitBuilding identity is missing or duplicated"))) return false;
		SourceIdentities.Add(Id, BuildingIdentity(Row->GetIntegerField(TEXT("type_id")),
			Row->GetIntegerField(TEXT("level")), Grid->GetIntegerField(TEXT("x")),
			Grid->GetIntegerField(TEXT("y"))));
		if (Visual->GetStringField(TEXT("representation")) == TEXT("runtime_boundary_mesh_collection"))
			BoundaryIds.Add(Id);
	}
	TMap<int32, FString> ExpectedActorIdentities;
	for (const TSharedPtr<FJsonValue>& Value : *RuntimeRows)
	{
		const TSharedPtr<FJsonObject> Row = Value->AsObject();
		if (!Expect(Row.IsValid(), TEXT("Invalid generated runtime building row"))) return false;
		const int32 Id = Row->GetIntegerField(TEXT("init_id"));
		const FString Identity = BuildingIdentity(Row->GetIntegerField(TEXT("building_type")),
			Row->GetIntegerField(TEXT("level")), Row->GetIntegerField(TEXT("grid_x")),
			Row->GetIntegerField(TEXT("grid_y")));
		if (!Expect(Id > 0 && !ExpectedActorIdentities.Contains(Id)
			&& SourceIdentities.FindRef(Id) == Identity && !BoundaryIds.Contains(Id),
			TEXT("Runtime actor identity differs from source InitBuilding row"))) return false;
		ExpectedActorIdentities.Add(Id, Identity);
	}
	if (!Expect(BoundaryIds.Num() == 2 && BoundaryIds.Contains(1) && BoundaryIds.Contains(7)
		&& SourceIdentities.Num() == ExpectedActorIdentities.Num() + BoundaryIds.Num(),
		TEXT("Two boundary mesh InitBuilding rows were not excluded by source representation"))) return false;
	const TSharedRef<FJsonObject> BoundActorEvidence = MakeShared<FJsonObject>();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!IsRuntimeInitialBuilding(*It)) continue;
		const int32 Id = TaggedInt(*It, TEXT("UnityInitId_"));
		if (!Expect(Id > 0 && HasTaggedPrefix(*It, TEXT("UnityGridX_"))
			&& HasTaggedPrefix(*It, TEXT("UnityGridY_"))
			&& ExpectedActorIdentities.FindRef(Id) == ActorIdentity(*It)
			&& !BoundInitialActors.Contains(Id),
			TEXT("CityMap runtime building tags do not match unique source InitBuilding identity"))) return false;
		BoundInitialActors.Add(Id, *It);
		BoundInitialIdentities.Add(Id, ActorIdentity(*It));
		const FRokBuildingState State = Buildings->GetBuildingStateForActor(*It);
		if (!Expect(State.BuildingKey == FName(*FString::Printf(TEXT("UnityInitId_%d"), Id))
			&& State.UnityInitId == Id
			&& BuildingIdentity(State.UnityBuildingTypeId, State.Level,
				State.UnityGridX, State.UnityGridY) == ExpectedActorIdentities.FindRef(Id),
			TEXT("Runtime building state is not bound to its source InitBuilding actor"))) return false;
		const TArray<uint8> StateBytes = Snapshot(State);
		InitialBuildingSnapshots.Add(*It, StateBytes);
		const TSharedRef<FJsonObject> ActorEvidence = MakeShared<FJsonObject>();
		ActorEvidence->SetStringField(TEXT("runtime_name"), It->GetName());
		ActorEvidence->SetStringField(TEXT("source_identity"), ActorIdentity(*It));
		ActorEvidence->SetStringField(TEXT("state_sha256"), RokTrainArmyMesh::Sha256(StateBytes));
		BoundActorEvidence->SetObjectField(FString::FromInt(Id), ActorEvidence);
	}
	if (!Expect(Buildings->GetInitialCityBuildingCount() == SourceIdentities.Num()
		&& BoundInitialActors.Num() == ExpectedActorIdentities.Num()
		&& InitialBuildingSnapshots.Num() == ExpectedActorIdentities.Num()
		&& LocalQueueCount == 0,
		TEXT("26 source states, 24 uniquely bound runtime actors, or empty local ledger differs"))) return false;
	Baseline = MakeShared<FJsonObject>();
	Baseline->SetStringField(TEXT("profile_sha256"), RokTrainArmyMesh::Sha256(ProfileSnapshot));
	Baseline->SetStringField(TEXT("resources_sha256"), RokTrainArmyMesh::Sha256(ResourceSnapshot));
	Baseline->SetNumberField(TEXT("source_initial_building_count"), SourceIdentities.Num());
	Baseline->SetNumberField(TEXT("runtime_bound_building_actor_count"), BoundInitialActors.Num());
	TArray<TSharedPtr<FJsonValue>> BoundaryJson;
	for (int32 Id : BoundaryIds) BoundaryJson.Add(MakeShared<FJsonValueNumber>(Id));
	Baseline->SetArrayField(TEXT("boundary_mesh_source_init_ids"), BoundaryJson);
	TArray<TSharedPtr<FJsonValue>> BoundJson;
	for (const auto& Pair : BoundInitialActors) BoundJson.Add(MakeShared<FJsonValueNumber>(Pair.Key));
	Baseline->SetArrayField(TEXT("runtime_bound_init_ids"), BoundJson);
	Baseline->SetObjectField(TEXT("runtime_bound_actor_evidence"), BoundActorEvidence);
	Baseline->SetNumberField(TEXT("active_upgrade_queue_count"), UpgradeQueueCount);
	Baseline->SetNumberField(TEXT("owned_build_queue_count"), OwnedQueueCount);
	Baseline->SetNumberField(TEXT("active_local_construction_count"), LocalQueueCount);
	bBaselineReady = true;
	return true;
}

bool URokLocalCityEntryAuditSubsystem::Unchanged()
{
	if (!bBaselineReady) return true;
	if (!Expect(Buildings.IsValid() && Runtime.IsValid()
		&& Snapshot(Runtime->GetPlayerProfileState()) == ProfileSnapshot
		&& Snapshot(Buildings->GetCurrentResources()) == ResourceSnapshot
		&& Buildings->GetActiveUpgradeCount() == UpgradeQueueCount
		&& Buildings->GetOwnedBuildQueueCount() == OwnedQueueCount
		&& Buildings->GetActiveLocalConstructionCount() == LocalQueueCount
		&& Buildings->GetInitialCityBuildingCount() == 26,
		TEXT("BuildCity bridge changed wallet/profile/queue/initial city state"))) return false;
	TSet<int32> Seen;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!IsRuntimeInitialBuilding(*It)) continue;
		const int32 Id = TaggedInt(*It, TEXT("UnityInitId_"));
		const TWeakObjectPtr<AActor>* Bound = BoundInitialActors.Find(Id);
		if (!Expect(Id > 0 && Bound && Bound->Get() == *It && !Seen.Contains(Id)
			&& BoundInitialIdentities.FindRef(Id) == ActorIdentity(*It),
			TEXT("Runtime city actor identity, tags or binding changed"))) return false;
		Seen.Add(Id);
	}
	if (!Expect(Seen.Num() == BoundInitialActors.Num()
		&& InitialBuildingSnapshots.Num() == BoundInitialActors.Num(),
		TEXT("BuildCity bridge changed actual runtime initial actor set"))) return false;
	for (const auto& Pair : InitialBuildingSnapshots)
	{
		if (!Expect(Pair.Key.IsValid()
			&& Snapshot(Buildings->GetBuildingStateForActor(Pair.Key.Get())) == Pair.Value,
			TEXT("BuildCity bridge changed initial building state"))) return false;
	}
	return true;
}

bool URokLocalCityEntryAuditSubsystem::CheckCardsAndSource()
{
	if (!Expect(Screen.IsValid() && Screen->IsBuildCityModeForAudit()
		&& Visible(Screen->GetWidgetFromName(TEXT("BuildCityBuildingScroll")))
		&& Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2"))
		&& Screen->GetWidgetFromName(TEXT("BuildCityCard_3"))
		&& Screen->GetWidgetFromName(TEXT("BuildCityCard_13"))
		&& !Screen->GetWidgetFromName(TEXT("BuildCityCardButton_3"))
		&& !Screen->GetWidgetFromName(TEXT("BuildCityCardButton_13")),
		TEXT("Source BuildCity economic cards/button lock state changed"))) return false;
	const UTextBlock* WoodLock = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("BuildCityCardLocked_3")));
	const UTextBlock* AcademyLock = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("BuildCityCardLocked_13")));
	if (!Expect(WoodLock && AcademyLock && WoodLock->GetText().ToString().Contains(TEXT("2"))
		&& AcademyLock->GetText().ToString().Contains(TEXT("4")),
		TEXT("Wood/Academy source Hall 2/4 locks are not displayed"))) return false;
	Previews = MakeShared<FJsonObject>();
	for (const int32 TypeId : {2, 3, 13})
	{
		FRokLocalConstructionPreview Preview;
		if (!Expect(Buildings->PreviewLocalConstructionEntry(TypeId, Preview)
			&& Preview.bContextVerified && !Preview.bPositionEvaluated && !Preview.bCanCommit,
			TEXT("Local context/card preflight is unavailable or claimably committable"))) return false;
		Previews->SetObjectField(FString::FromInt(TypeId), PreviewJson(Preview));
		if (TypeId == 2)
		{
			if (!Expect(Preview.bSourceEligible && Preview.MissingTypeId == 0
				&& Preview.Footprint == FIntPoint(5, 5) && Preview.Cost.Wood == 100
				&& Preview.BuildSeconds == 2 && Preview.VisualCivilization > 0
				&& Preview.ResolvedAge > 0 && !Preview.SourceModelId.IsEmpty(),
				TEXT("Farm2 source eligibility/config or trusted local visual context differs"))) return false;
		}
		else if (!Expect(!Preview.bSourceEligible && Preview.MissingTypeId == 1
			&& Preview.RequiredLevel == (TypeId == 3 ? 2 : 4)
			&& Preview.CurrentLevel == 1,
			TEXT("Wood3/Academy13 source Hall prerequisite differs"))) return false;
	}
	return Unchanged();
}

bool URokLocalCityEntryAuditSubsystem::SlateClick(UWidget* Target, const TCHAR* Name)
{
	if (!Identity() || !Unchanged()) return false;
	if (!Expect(Target && Visible(Target) && Target->GetIsEnabled()
		&& FSlateApplication::IsInitialized(), TEXT("BuildCity Slate target unavailable"))) return false;
	const FGeometry Geometry = Target->GetCachedGeometry();
	if (!Expect(Geometry.GetLocalSize().X > 0 && Geometry.GetLocalSize().Y > 0,
		TEXT("BuildCity target has no cached geometry"))) return false;
	const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-32.0f, -32.0f));
	FSlateApplication& App = FSlateApplication::Get();
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, Point, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Point, Away, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	const FWidgetPath Path = App.LocateWindowUnderMouse(
		Point, App.GetInteractiveTopLevelWindows(), false, 0);
	if (!Expect(Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get())
		&& Target->IsHovered(), TEXT("BuildCity virtual pointer missed real Slate button"))) return false;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("button"), Name);
	Action->SetStringField(TEXT("method"), TEXT("Slate MoveAway+Move+Down+Up"));
	Action->SetNumberField(TEXT("step"), Step);
	Action->SetNumberField(TEXT("absolute_x"), Point.X);
	Action->SetNumberField(TEXT("absolute_y"), Point.Y);
	Action->SetBoolField(TEXT("target_in_hit_path"), true);
	Action->SetBoolField(TEXT("target_hovered"), true);
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const FPointerEvent Down(0, Point, Point, Pressed,
		EKeys::LeftMouseButton, 0, FModifierKeysState());
	const FPointerEvent Up(0, Point, Point, TSet<FKey>(),
		EKeys::LeftMouseButton, 0, FModifierKeysState());
	const bool DownHandled = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(), Down);
	const bool UpHandled = App.ProcessMouseButtonUpEvent(Up);
	Action->SetBoolField(TEXT("down_handled"), DownHandled);
	Action->SetBoolField(TEXT("up_handled"), UpHandled);
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	return Expect(DownHandled && UpHandled, TEXT("BuildCity virtual Slate button click not handled"));
}

void URokLocalCityEntryAuditSubsystem::OnPlacementRequested(int32 TypeId)
{
	++CallbackCount;
	CallbackTypeId = TypeId;
	bCallbackScreenClosed = Screen.IsValid() && !Screen->IsScreenOpen();
}

TSharedRef<FJsonObject> URokLocalCityEntryAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("step"), Step);
	Json->SetNumberField(TEXT("frame"), GFrameCounter);
	Json->SetNumberField(TEXT("world_elapsed_seconds"), Elapsed);
	Json->SetBoolField(TEXT("state_unchanged"), true);
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
	Json->SetBoolField(TEXT("build_city_mode"), Screen.IsValid()
		&& Screen->IsBuildCityModeForAudit());
	Json->SetNumberField(TEXT("selected_type_id"), Screen.IsValid()
		? Screen->GetSelectedBuildCityTypeIdForAudit() : 0);
	Json->SetNumberField(TEXT("callback_count"), CallbackCount);
	Json->SetNumberField(TEXT("callback_type_id"), CallbackTypeId);
	Json->SetBoolField(TEXT("callback_observed_screen_closed"), bCallbackScreenClosed);
	Json->SetBoolField(TEXT("audit_listener_bound"), PlacementHandle.IsValid());
	if (Screen.IsValid())
	{
		const UTextBlock* Feedback = Cast<UTextBlock>(
			Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
		Json->SetStringField(TEXT("feedback"), Feedback ? Feedback->GetText().ToString() : FString());
	}
	return Json;
}

void URokLocalCityEntryAuditSubsystem::Capture(const TCHAR* Name)
{
	if (!Unchanged()) return;
	if (FScreenshotRequest::IsScreenshotRequested())
	{
		Finish(TEXT("Conflicting BuildCity screenshot request"));
		return;
	}
	PendingPath = Directory / (FString(Name) + TEXT(".png"));
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("file"), PendingPath);
	Pending->SetStringField(TEXT("phase_name"), Name);
	Pending->SetObjectField(TEXT("request"), State());
	ShotStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}

void URokLocalCityEntryAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || !Unchanged()) return;
	const int64 Size = IFileManager::Get().FileSize(*PendingPath);
	if (Size <= 0) { Finish(TEXT("BuildCity screenshot callback missing PNG")); return; }
	TArray<uint8> Png;
	if (!FFileHelper::LoadFileToArray(Png, *PendingPath))
	{
		Finish(TEXT("BuildCity screenshot cannot be hashed"));
		return;
	}
	Pending->SetObjectField(TEXT("processed"), State());
	Pending->SetNumberField(TEXT("file_bytes"), Size);
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Png));
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(Step + 1);
}

void URokLocalCityEntryAuditSubsystem::Tick(float DeltaTime)
{
	if (!bOwnsWorld || bFinished) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 3.0) return;
	if (Pending.IsValid())
	{
		if (Elapsed - ShotStarted > 25.0) Finish(TEXT("BuildCity screenshot callback timeout"));
		return;
	}
	if (Step > 0 && Elapsed - StepStarted > 12.0)
	{
		Finish(FString::Printf(TEXT("BuildCity bridge step %d timeout"), Step));
		return;
	}
	if (!Unchanged()) return;
	switch (Step)
	{
	case 0: if (Begin()) Enter(1); break;
	case 1:
		if (SlateClick(Hud->GetWidgetFromName(TEXT("BuildCityButton")), TEXT("BuildCityButton"))) Enter(2);
		break;
	case 2:
		if (Screen.IsValid() && Screen->IsBuildCityModeForAudit() && CheckCardsAndSource())
		{
			UWidget* Farm = Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2"));
			if (UScrollBox* Scroll = Cast<UScrollBox>(
				Screen->GetWidgetFromName(TEXT("BuildCityBuildingScroll"))))
				Scroll->ScrollWidgetIntoView(Farm, false, EDescendantScrollDestination::Center);
			Enter(3);
		}
		break;
	case 3:
		if (Elapsed - StepStarted > 0.5 && Screen->IsBuildCityModeForAudit()
			&& Visible(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2"))))
			Capture(TEXT("01_buildcity_source_cards"));
		break;
	case 4:
		if (!Expect(!Screen->OnBuildCityPlacementRequested.IsBound(),
			TEXT("Production BuildCity listener unexpectedly installed in isolated bridge audit"))) break;
		if (SlateClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2")),
			TEXT("BuildCityCardButton_2_unbound"))) Enter(5);
		break;
	case 5:
		if (Screen->GetSelectedBuildCityTypeIdForAudit() == 2)
		{
			const UTextBlock* Feedback = Cast<UTextBlock>(
				Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
			if (Expect(Screen->IsBuildCityModeForAudit() && CallbackCount == 0
				&& Feedback && Feedback->GetText().ToString().Contains(TEXT("该建筑暂时无法放置"))
				&& Unchanged(),
				TEXT("Unbound Farm card closed BuildCity, changed state or swallowed feedback")))
				Capture(TEXT("02_unbound_card_keeps_screen"));
		}
		break;
	case 6:
		PlacementHandle = Screen->OnBuildCityPlacementRequested.AddUObject(
			this, &URokLocalCityEntryAuditSubsystem::OnPlacementRequested);
		if (Expect(PlacementHandle.IsValid() && Screen->OnBuildCityPlacementRequested.IsBound(),
			TEXT("Audit-only BuildCity placement observer did not bind"))
			&& SlateClick(Screen->GetWidgetFromName(TEXT("BuildCityCardButton_2")),
				TEXT("BuildCityCardButton_2_audit_listener"))) Enter(7);
		break;
	case 7:
		if (CallbackCount > 0)
		{
			if (Expect(CallbackCount == 1 && CallbackTypeId == 2
				&& bCallbackScreenClosed && !Screen->IsScreenOpen() && Unchanged(),
				TEXT("Farm card did not close before exactly one audit-only Type2 broadcast")))
				Capture(TEXT("03_audit_listener_handoff"));
		}
		break;
	case 8: Finish(); break;
	default: Finish(TEXT("Invalid BuildCity bridge audit step")); break;
	}
}

bool URokLocalCityEntryAuditSubsystem::Save(bool Complete)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.local_city_entry.runtime.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty()
		&& Captures.Num() == ExpectedCaptureCount);
	Json->SetStringField(TEXT("audit_flag"), AuditFlag);
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Json->SetStringField(TEXT("entry_kind"), TEXT("real HUD BuildCityButton and Slate Farm2 card click"));
	Json->SetStringField(TEXT("scope"), TEXT("BuildCity source preflight and card-to-city bridge only. Placement listener exists only in this audit, records TypeId and close order, and creates no actor, construction ticket, network request or resource change. Production placement and visual factory are not implemented or verified."));
	Json->SetBoolField(TEXT("listener_audit_only"), true);
	Json->SetBoolField(TEXT("placement_implemented"), false);
	Json->SetNumberField(TEXT("expected_capture_count"), ExpectedCaptureCount);
	Json->SetNumberField(TEXT("callback_count"), CallbackCount);
	Json->SetNumberField(TEXT("callback_type_id"), CallbackTypeId);
	Json->SetBoolField(TEXT("callback_observed_screen_closed"), bCallbackScreenClosed);
	if (Inputs.IsValid()) Json->SetObjectField(TEXT("input_hashes"), Inputs.ToSharedRef());
	if (Baseline.IsValid()) Json->SetObjectField(TEXT("baseline"), Baseline.ToSharedRef());
	if (Previews.IsValid()) Json->SetObjectField(TEXT("source_entry_previews"), Previews.ToSharedRef());
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetArrayField(TEXT("actions"), Actions);
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	if (FailureState.IsValid()) Json->SetObjectField(TEXT("failure_state"), FailureState.ToSharedRef());
	FString Body;
	return FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Body))
		&& FFileHelper::SaveStringToFile(Body, *(Directory / TEXT("manifest.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokLocalCityEntryAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	if (Failures.IsEmpty() && Captures.Num() != ExpectedCaptureCount)
		Failures.Add(TEXT("BuildCity bridge audit did not process all three screenshots"));
	if (!Failures.IsEmpty()) FailureState = State();
	bFinished = true;
	if (Screen.IsValid() && PlacementHandle.IsValid())
	{
		Screen->OnBuildCityPlacementRequested.Remove(PlacementHandle);
		PlacementHandle.Reset();
	}
	const bool Written = Save(Failures.IsEmpty());
	for (const FString& Failure : Failures)
		UE_LOG(LogTemp, Error, TEXT("Rok LocalCityEntryAudit: %s"), *Failure);
	UE_LOG(LogTemp, Display, TEXT("Rok LocalCityEntryAudit passed=%d captures=%d directory=%s"),
		Failures.IsEmpty() && Written, Captures.Num(), *Directory);
	FPlatformMisc::RequestExitWithStatus(false, Failures.IsEmpty() && Written ? 0 : 1);
}
