#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/FileManager.h"
#include "ProceduralMeshComponent.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokGameNetSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokPlayerController.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Misc/Guid.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "../RokAudioRouterSubsystem.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokCityConstructionSources.h"
#include "../RokCityPlacementFirstFit.h"
#include "../RokCityPlacementPreviewActor.h"
#include "../RokCityPlacementWorldFrame.h"
#include "../RokCitySpriteActor.h"
#include "../RokLocalConstructionVisualFactory.h"
#include "../RokServerCitySubsystem.h"
#include "../RokUnityGameplayCatalogSubsystem.h"

namespace
{
TSharedPtr<FJsonObject> MakeRow(int32 Index, int32 Type, int32 Level, int32 X, int32 Y, int64 FinishTime = -1)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("buildingIndex"), Index);
	Row->SetNumberField(TEXT("type"), Type);
	Row->SetNumberField(TEXT("level"), Level);
	TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
	Pos->SetNumberField(TEXT("x"), X);
	Pos->SetNumberField(TEXT("y"), Y);
	Row->SetObjectField(TEXT("pos"), Pos);
	Row->SetNumberField(TEXT("finishTime"), double(FinishTime));
	return Row;
}

// The parts of Build_BuildingInfo.buildingInfo the city reads, as the Unity editor
// account's role had them on 2026-11-04 (Saved/RokNet/LastSession.json). The merged
// state keys rows by buildingIndex, so key order is not index order.
TSharedPtr<FJsonObject> MakeServerCityFixture()
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
	Info->SetObjectField(TEXT("30"), MakeRow(30, 11, 1, 10, -3));
	Info->SetObjectField(TEXT("3"), MakeRow(3, 15, 1, 6, 7));
	Info->SetObjectField(TEXT("5"), MakeRow(5, 1, 2, -6, 6));
	Info->SetObjectField(TEXT("7"), MakeRow(7, 6, 2, 0, 0));
	Info->SetObjectField(TEXT("27"), MakeRow(27, 2, 1, -2, -7));
	Info->SetObjectField(TEXT("28"), MakeRow(28, 18, 1, -8, -8));
	Info->SetObjectField(TEXT("29"), MakeRow(29, 28, 1, 9, -9));
	return Info;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokServerCityParseTest,
	"Rok.Net.ServerCity.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerCityParseTest::RunTest(const FString& Parameters)
{
	TArray<FRokServerBuilding> Buildings;
	FString Error;
	if (!TestTrue(TEXT("Server city parses"), RokServerCity::ParseBuildings(MakeServerCityFixture(), Buildings, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("Seven rows"), Buildings.Num(), 7)) return false;
	TestEqual(TEXT("Sorted by index: first"), Buildings[0].Index, 3);
	TestEqual(TEXT("Sorted by index: last"), Buildings.Last().Index, 30);
	TestEqual(TEXT("Warehouse pos"), Buildings[0].Pos, FIntPoint(6, 7));
	TestEqual(TEXT("Town hall level"), Buildings[1].Level, 2);
	TestEqual(TEXT("Town hall pos"), Buildings[1].Pos, FIntPoint(-6, 6));
	TestEqual(TEXT("Wall level"), Buildings[2].Level, 2);
	TestEqual(TEXT("Commander hall type"), Buildings[4].Type, 18);
	TestEqual(TEXT("Finish time -1 is idle"), Buildings[4].FinishTime, int64(-1));

	TSharedPtr<FJsonObject> NoPos = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Row = MakeRow(4, 14, 1, -1, -1);
	Row->RemoveField(TEXT("pos"));
	NoPos->SetObjectField(TEXT("4"), Row);
	TestFalse(TEXT("A row without pos is rejected"), RokServerCity::ParseBuildings(NoPos, Buildings, Error));
	TestTrue(TEXT("Rejected parse leaves nothing"), Buildings.IsEmpty());

	TSharedPtr<FJsonObject> Duplicate = MakeShared<FJsonObject>();
	Duplicate->SetObjectField(TEXT("4"), MakeRow(4, 14, 1, -1, -1));
	Duplicate->SetObjectField(TEXT("x"), MakeRow(4, 14, 1, -1, -1));
	TestFalse(TEXT("A repeated buildingIndex is rejected"), RokServerCity::ParseBuildings(Duplicate, Buildings, Error));
	TestFalse(TEXT("A missing collection is rejected"), RokServerCity::ParseBuildings(nullptr, Buildings, Error));
	return true;
}

// Server-built types only need a source visual and a catalog footprint. Scout and
// archery camps are not local-construction types: FindSourceVisual must keep them out
// of the offline BuildCity flow.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokServerCityVisualsTest,
	"Rok.Net.ServerCity.Visuals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerCityVisualsTest::RunTest(const FString& Parameters)
{
	for (const int32 Type : {2, 16, 18, 28, 11})
	{
		const FRokLocalSourceBuildingVisual* Visual = FRokLocalConstructionVisualFactory::FindBuildingVisual(Type);
		if (!TestNotNull(*FString::Printf(TEXT("Type %d has a source visual"), Type), Visual)) continue;
		TestEqual(*FString::Printf(TEXT("Type %d model"), Type), FString(Visual->ModelId),
			FString::Printf(TEXT("build_%d_101_1"), Type));
	}
	TestNotNull(TEXT("Farm is a local construction type"), FRokLocalConstructionVisualFactory::FindSourceVisual(2));
	TestNotNull(TEXT("Commander hall is a local construction type"), FRokLocalConstructionVisualFactory::FindSourceVisual(18));
	TestNull(TEXT("Scout camp is server-only"), FRokLocalConstructionVisualFactory::FindSourceVisual(28));
	TestNull(TEXT("Archery range is server-only"), FRokLocalConstructionVisualFactory::FindSourceVisual(11));
	TestNull(TEXT("Alliance Center uses the server construction flow"), FRokLocalConstructionVisualFactory::FindSourceVisual(16));
	if (const auto* Alliance = FRokLocalConstructionVisualFactory::FindBuildingVisual(16, TEXT("build_16_101_1")))
	{
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* Material = nullptr;
		FString Error;
		TestTrue(TEXT("Initial Alliance Center loads its verified mesh"),
			FRokLocalConstructionVisualFactory::LoadVerifiedSourceMesh(*Alliance, Mesh, Material, Error));
		const FVector Scale = FRokLocalConstructionVisualFactory::GetSourceChildTransform(*Alliance).GetScale3D();
		TestTrue(TEXT("Initial Alliance Center preserves the source width and height scales"),
			Scale.Equals(FVector(-0.0012691392, 0.0012691392, 0.0011784864), 1e-9));
	}
	TestNull(TEXT("Classical city hall requires an exact model ID"),
		FRokLocalConstructionVisualFactory::FindBuildingVisual(1));
	FRokCityConstructionSources Models;
	FString SourceError;
	if (!Models.Load(SourceError)) { AddError(SourceError); return false; }
	// All three BuildCity categories use these exact Civilization 101 / Age 2 rows.
	for (const int32 Type : {1, 2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 28, 29, 30, 31, 51, 52, 53})
	{
		FRokCityConstructionModel Model;
		if (!TestTrue(*FString::Printf(TEXT("Source model for type %d"), Type), Models.GetModel(Type, 101, 2, Model))) continue;
		const auto* Visual = FRokLocalConstructionVisualFactory::FindBuildingVisual(Type, Model.ModelId);
		if (!TestNotNull(*Model.ModelId, Visual)) continue;
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* Material = nullptr;
		FString Error;
		if (!FRokLocalConstructionVisualFactory::LoadVerifiedSourceMesh(*Visual, Mesh, Material, Error)) AddError(Error);
	}

	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName PackageName(*FString::Printf(TEXT("/Temp/RokServerCityVisuals_%s/L_RokServerCityVisuals"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Instance->InitializeStandalone(TEXT("L_RokServerCityVisuals"),
		NewObject<UPackage>(nullptr, PackageName, RF_Transient));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	const auto* HallVisual = FRokLocalConstructionVisualFactory::FindBuildingVisual(1, TEXT("build_1_101_2"));
	UStaticMesh* HallMesh = nullptr;
	UMaterialInterface* HallMaterial = nullptr;
	FString HallError;
	if (HallVisual && FRokLocalConstructionVisualFactory::LoadVerifiedSourceMesh(*HallVisual, HallMesh, HallMaterial, HallError))
	{
		ARokCitySpriteActor* Hall = World->SpawnActor<ARokCitySpriteActor>();
		if (TestNotNull(TEXT("City hall visibility trace fixture"), Hall))
		{
			Hall->ConfigureSourceMesh(HallMesh, HallMaterial, FTransform::Identity, 0);
			FHitResult Hit;
			TestTrue(TEXT("Simple visibility trace picks the classical city hall sprite"),
				Hall->GetSpriteMeshComponent()->LineTraceComponent(Hit, FVector(0, -200, 0), FVector(0, 200, 0),
					FCollisionQueryParams::DefaultQueryParam));
		}
	}
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Gameplay catalog"), Catalog) || !TestTrue(TEXT("Catalog loads"), Catalog->IsCatalogLoaded()))
	{
		return false;
	}
	const TPair<int32, FIntPoint> Footprints[] = {
		{2, FIntPoint(5, 5)}, {16, FIntPoint(6, 6)}, {18, FIntPoint(5, 5)}, {28, FIntPoint(6, 6)}, {11, FIntPoint(7, 7)}};
	for (const TPair<int32, FIntPoint>& Expected : Footprints)
	{
		FRokUnityCatalogBuildingType Row;
		if (TestTrue(*FString::Printf(TEXT("Type %d is in the catalog"), Expected.Key),
			Catalog->GetBuildingType(Expected.Key, Row)))
		{
			TestEqual(*FString::Printf(TEXT("Type %d footprint"), Expected.Key),
				FIntPoint(Row.Width, Row.Length), Expected.Value);
		}
	}
	return true;
}

// Without the source city map the server layout must not half-apply: no frame, no
// spawned actor, and ResolveForWorld keeps its own answer.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokServerCityApplyTest,
	"Rok.Net.ServerCity.Apply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerCityApplyTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName PackageName(*FString::Printf(TEXT("/Temp/RokServerCityFixture_%s/L_RokServerCityFixture"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UPackage* Package = NewObject<UPackage>(nullptr, PackageName, RF_Transient);
	Instance->InitializeStandalone(TEXT("L_RokServerCityFixture"), Package);
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	URokServerCitySubsystem* ServerCity = World->GetSubsystem<URokServerCitySubsystem>();
	URokBuildingUpgradeSubsystem* Upgrades = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Server city subsystem exists in game worlds"), ServerCity)
		|| !TestNotNull(TEXT("Building subsystem"), Upgrades)) return false;

	TArray<FRokServerBuilding> Buildings;
	FString Error;
	if (!TestTrue(TEXT("Fixture parses"), RokServerCity::ParseBuildings(MakeServerCityFixture(), Buildings, Error)))
	{
		return false;
	}
	TestFalse(TEXT("No source city, no server layout"), ServerCity->ApplyBuildings(Buildings, Error));
	TestFalse(TEXT("The failure says why"), Error.IsEmpty());
	TestFalse(TEXT("Nothing counts as applied"), ServerCity->HasAppliedServerCity());
	TestNull(TEXT("Nothing spawned"), ServerCity->FindSpawnedBuilding(28));
	FRokCityPlacementWorldFrame Frame;
	TestFalse(TEXT("No cached frame"), ServerCity->GetAppliedFrame(Frame));

	// ApplyServerBuilding writes what the ring menu and upgrade window read.
	FActorSpawnParameters Params;
	Params.Name = TEXT("ServerCityHallFixture");
	Params.ObjectFlags |= RF_Transient;
	AActor* Hall = World->SpawnActor<AActor>(AActor::StaticClass(), Params);
	if (!TestNotNull(TEXT("Fixture actor"), Hall)) return false;
	Hall->Tags.Add(TEXT("UnityBuildingType_1"));
	Hall->Tags.Add(TEXT("UnityBuildingLevel_1"));
	Hall->Tags.Add(TEXT("UnityGridX_-5"));
	Hall->Tags.Add(TEXT("UnityGridY_5"));
	Upgrades->ApplyServerBuilding(Hall, 1, 2, FIntPoint(-6, 6));
	const FRokBuildingState State = Upgrades->GetBuildingStateForActor(Hall);
	TestEqual(TEXT("Server level"), State.Level, 2);
	TestEqual(TEXT("Server grid X"), State.UnityGridX, -6);
	TestEqual(TEXT("Server grid Y"), State.UnityGridY, 6);
	TestFalse(TEXT("No local upgrade running"), State.bUpgradeInProgress);
	TestTrue(TEXT("Level tag follows"), Hall->ActorHasTag(TEXT("UnityBuildingLevel_2")));
	TestFalse(TEXT("Old level tag is gone"), Hall->ActorHasTag(TEXT("UnityBuildingLevel_1")));
	TestTrue(TEXT("Grid X tag follows"), Hall->ActorHasTag(TEXT("UnityGridX_-6")));
	TestFalse(TEXT("Old grid X tag is gone"), Hall->ActorHasTag(TEXT("UnityGridX_-5")));
	return true;
}

struct FRokCityUpgradeReplay
{
	static bool LoadSchema(URokGameNetSubsystem& Net, FString& Error)
	{
		return Net.LoadSchema(Error) && Net.Schema.FindProtocol(TEXT("Build_BuildingInfo"))
			&& Net.Schema.FindProtocol(TEXT("Role_RoleInfo"));
	}

	static void Submit(URokGameNetSubsystem& Net, int32 Index, int32 TargetLevel, bool bImmediately,
		int64 PreviousQueueTime = 0)
	{
		URokGameNetSubsystem::FCityWrite Write;
		Write.Index = Index;
		Write.TargetLevel = TargetLevel;
		Write.bImmediately = bImmediately;
		if (PreviousQueueTime > 0)
		{
			Write.QueueAction = URokGameNetSubsystem::FCityWrite::EQueueAction::SpeedUp;
			Write.QueueIndex = 1;
			Write.PreviousQueueTime = PreviousQueueTime;
		}
		Net.SprotoSession += 2;
		Net.TrackSentCityWrite(MoveTemp(Write));
	}

	static void AckTwice(URokGameNetSubsystem& Net, int64 Finish = -1)
	{
		FRokSprotoMessage Message;
		Message.bHasSession = true;
		Message.Session = Net.CityWrite->InnerSession;
		Message.Body = MakeShared<FJsonObject>();
		Message.Body->SetNumberField(TEXT("buildingIndex"), Net.CityWrite->Index);
		Message.Body->SetBoolField(TEXT("immediately"), Net.CityWrite->bImmediately);
		if (Finish >= 0) Message.Body->SetNumberField(TEXT("finishTime"), Finish);
		Net.AcknowledgeCityWrite(Message);
		Net.AcknowledgeCityWrite(Message);
	}

	static void PushBuildingTwice(URokGameNetSubsystem& Net, TSharedRef<FJsonObject> Row, int32 Version)
	{
		Row->SetNumberField(TEXT("version"), Version);
		const auto Update = MakeShared<FJsonObject>();
		Update->SetArrayField(TEXT("buildingInfo"), {MakeShared<FJsonValueObject>(Row)});
		const FRokSprotoProtocol& Protocol = *Net.Schema.FindProtocol(TEXT("Build_BuildingInfo"));
		Net.MergePush(Protocol, Update);
		Net.MergePush(Protocol, Update);
	}

	static void PushQueue(URokGameNetSubsystem& Net, int32 Index, int64 Finish)
	{
		const auto Queue = MakeShared<FJsonObject>();
		Queue->SetNumberField(TEXT("queueIndex"), 1);
		Queue->SetNumberField(TEXT("buildingIndex"), Index);
		Queue->SetNumberField(TEXT("finishTime"), Finish);
		const auto Role = MakeShared<FJsonObject>();
		Role->SetArrayField(TEXT("buildQueue"), {MakeShared<FJsonValueObject>(Queue)});
		const auto Update = MakeShared<FJsonObject>();
		Update->SetObjectField(TEXT("roleInfo"), Role);
		Net.MergePush(*Net.Schema.FindProtocol(TEXT("Role_RoleInfo")), Update);
	}

	static bool IsConfirmed(URokGameNetSubsystem& Net)
	{
		Net.CheckCityWrite(FPlatformTime::Seconds());
		return !Net.CityWrite.IsSet();
	}
};

namespace
{
class FBuildCityReplayFlow final : public IAutomationLatentCommand
{
public:
	explicit FBuildCityReplayFlow(FAutomationTestBase& InTest) : Test(InTest) {}
	~FBuildCityReplayFlow() override
	{
		if (RecordingDevice.IsValid()) WriteConstructionRecording();
	}

	bool Update() override
	{
		if (!Hud.IsValid())
		{
			for (TObjectIterator<URokMainHudWidget> It; It; ++It)
			{
				UWorld* World = It->GetWorld();
				if (!World || !It->IsInViewport()) continue;
				const auto* City = World->GetSubsystem<URokServerCitySubsystem>();
				const auto* Net = World->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
				if (!City || !City->HasAppliedServerCity() || !Net || !Net->IsSnapshotReplay()) continue;
				Hud = *It;
				Screen = It->GetFeatureScreenWidgetForPlacement();
				Controller = Cast<ARokPlayerController>(It->GetOwningPlayer());
				if (!Test.TestTrue(TEXT("City HUD and controller are ready"), Screen.IsValid() && Controller.IsValid())) return true;
				if (!Click(Hud.Get(), TEXT("BuildCityButton"))) return true;
				Screen->SampleBuildCityAnimationForCapture(10.0f);
				return Capture(TEXT("Menu"));
			}
			if (FPlatformTime::Seconds() < Deadline) return false;
			Test.AddError(TEXT("Run in the city with the age-two server snapshot replay."));
			return true;
		}
		if (CaptureAt > 0.0)
		{
			const double Now = bCaptureInWorldTime ? Hud->GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
			if (Now < CaptureAt && FPlatformTime::Seconds() < Deadline) return false;
			CaptureAt = 0.0;
			FScreenshotRequest::RequestScreenshot(PendingCapture, true, false);
			return false;
		}
		if (Step == 6 || Step == 9 || Step == 12) return WaitForReplayConstruction();
		if (IFileManager::Get().FileSize(*PendingCapture) <= 0)
		{
			if (FPlatformTime::Seconds() < Deadline) return false;
			if (ReplayRows) RemoveReplayRow();
			RestoreOtherCivilization();
			Test.AddError(TEXT("Missing BuildCity screenshot: ") + PendingCapture);
			return true;
		}
		if (Step == 0)
		{
			const auto* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("BuildCityBuildingScroll")));
			if (!Test.TestNotNull(TEXT("BuildCity scroll"), Scroll)) return true;
			const int32 Types[] = {2, 3, 4, 13};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Types); ++Index)
			{
				const auto* Card = Scroll->GetChildAt(Index);
				Test.TestTrue(TEXT("Source economic cards lead from left to right"),
					Card && Card->GetFName() == FName(*FString::Printf(TEXT("BuildCityCardSize_%d"), Types[Index])));
			}
			++Step;
			return Preview(4, FIntPoint(5, 5), TEXT("QuarryPlacement"));
		}
		if (Step == 7)
		{
			RemoveReplayRow();
			VerifyClassicalMenuModels();
			++Step;
			if (!Click(Hud.Get(), TEXT("BuildCityButton")) || !Click(Screen.Get(), TEXT("BuildCityCategory_1"))) return true;
			return Preview(12, FIntPoint(7, 7), TEXT("SiegeWorkshopPlacement"));
		}
		if (Step == 8)
		{
			if (!Click(Hud.Get(), TEXT("CityPlacementCancelButton"))) return true;
			++Step;
			return StartReplayConstruction(12, SiegeGrid, 10);
		}
		if (Step == 10)
		{
			RemoveReplayRow();
			VerifyCivilizationAges();
			const auto* Net = Hud->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
			auto* City = Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
			const auto Hall = ReplayRows->GetObjectField(TEXT("5"));
			OriginalHallLevel = Hall->GetIntegerField(TEXT("level"));
			CountryBeforeCapture = Net->GetRoleInfo()->TryGetField(TEXT("country"));
			RoleLevelBeforeCapture = Net->GetRoleInfo()->TryGetField(TEXT("level"));
			bOtherCivilizationCapture = true;
			Net->GetRoleInfo()->SetNumberField(TEXT("country"), 501);
			Net->GetRoleInfo()->SetNumberField(TEXT("level"), 10);
			Hall->SetNumberField(TEXT("level"), 10);
			TArray<FRokServerBuilding> Fixture;
			FString Error;
			if (!RokServerCity::ParseBuildings(ReplayRows, Fixture, Error) || !City->ApplyBuildings(Fixture, Error)
				|| !Click(Hud.Get(), TEXT("BuildCityButton")) || !Click(Screen.Get(), TEXT("BuildCityCategory_1")))
			{
				if (!Error.IsEmpty()) Test.AddError(Error);
				RestoreOtherCivilization();
				return true;
			}
			++Step;
			return Preview(12, FIntPoint(7, 7), TEXT("Civilization501Age3SiegePlacement"));
		}
		if (Step == 11)
		{
			if (!Click(Hud.Get(), TEXT("CityPlacementCancelButton")))
			{
				RestoreOtherCivilization();
				return true;
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("RokRecordConstructionAudio")) && !BeginConstructionRecording())
			{
				RestoreOtherCivilization();
				return true;
			}
			++Step;
			return StartReplayConstruction(12, SiegeGrid, 10);
		}
		if (Step == 13)
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("RokRecordConstructionAudio")))
			{
				const auto* City = Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
				if (City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3004"))) return false;
				auto* Net = Hud->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
				FString Error;
				if (!Test.TestTrue(TEXT("Construction replay has the building and role push schema"),
					FRokCityUpgradeReplay::LoadSchema(*Net, Error)))
				{
					if (!Error.IsEmpty()) Test.AddError(Error);
					RestoreOtherCivilization();
					return true;
				}
				UpgradeOriginalQueue = Net->GetRoleInfo()->TryGetField(TEXT("buildQueue"));
				Net->GetRoleInfo()->RemoveField(TEXT("buildQueue"));
				++Step;
				return RunUpgradeActions(0);
			}
			Step = 22;
		}
		if (Step >= 14 && Step <= 21)
		{
			const int32 Action = Step++ - 13;
			return RunUpgradeActions(Action);
		}
		if (Step == 22)
		{
			RemoveReplayRow();
			const auto* Net = Hud->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
			Net->GetRoleInfo()->SetNumberField(TEXT("country"), 201);
			Net->GetRoleInfo()->SetNumberField(TEXT("level"), 16);
			ReplayRows->GetObjectField(TEXT("5"))->SetNumberField(TEXT("level"), 16);
			++Step;
			return Capture(TEXT("Civilization201Age4Boundary"));
		}
		if (Step == 23)
		{
			auto* City = Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
			Test.TestTrue(TEXT("The next civilization and age reach the world"),
				City->FindBuildingBarAnchor(7) && City->FindBuildingBarAnchor(7)->ActorHasTag(TEXT("build_6_201_4"))
				&& City->FindBuildingBarAnchor(1) && City->FindBuildingBarAnchor(1)->ActorHasTag(TEXT("build_8_201_4")));
			Test.TestNull(TEXT("The completion burst expires"), City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3004")));
			if (RecordingDevice.IsValid()) WriteConstructionRecording();
			RestoreOtherCivilization();
			return true;
		}
		if (Step == 3)
		{
			if (!Click(Hud.Get(), TEXT("CityPlacementConfirmButton"))) return true;
			Test.TestTrue(TEXT("Confirmation opens the worker queue when no worker is free"), Screen->IsWorkerQueueOpen());
			Test.TestEqual(TEXT("Confirmation takes the worker route"),
				Controller->GetCityPlacementPreviewSnapshot().LastConfirmOutcome, FName(TEXT("worker_queue_opened")));
			++Step;
			return Capture(TEXT("CavalryWorkerQueue"));
		}
		if (Step == 4)
		{
			if (!Click(Screen.Get(), TEXT("WorkerQueuePrimaryAction"))) return true;
			Test.TestNotNull(TEXT("Worker speed-up opens the building inventory list"), Screen->GetWidgetFromName(TEXT("BuildingSpeedUpItems")));
			++Step;
			return Capture(TEXT("CavalryWorkerSpeedUp"));
		}
		if (Step == 5)
		{
			Screen->RequestCloseScreen();
			const auto State = Controller->GetCityPlacementPreviewSnapshot();
			Test.TestTrue(TEXT("Opening the worker queue preserves the cavalry placement and prevents a write"),
				State.bActive && State.TypeId == 10 && State.Grid == CavalryGrid && !State.bCanCommit);
			Screen->CloseScreen();
			Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>()->ClearBuildQueueSnapshotForTesting();
		}
		if (!Click(Hud.Get(), TEXT("CityPlacementCancelButton"))) return true;
		Test.TestFalse(TEXT("Cancel removes the temporary candidate"), Controller->GetCityPlacementPreviewSnapshot().bActive);
		if (Step == 1)
		{
			++Step;
			if (!Click(Hud.Get(), TEXT("BuildCityButton"))) return true;
			return Preview(13, FIntPoint(6, 6), TEXT("AcademyPlacement"));
		}
		if (Step == 2)
		{
			++Step;
			CheckQuarryLifecycle();
			FRokServerBuildQueueSnapshot Queues;
			Queues.bAvailable = Queues.bTimeSynchronized = true;
			FRokServerBuildQueueView& Primary = Queues.Queues.AddDefaulted_GetRef();
			Primary.QueueIndex = 1;
			Primary.State = ERokServerBuildQueueState::Busy;
			Primary.BuildingIndex = 5;
			Primary.BuildingName = TEXT("市政厅");
			Primary.TargetLevel = 6;
			Primary.bCountdownKnown = Primary.bProgressKnown = true;
			Primary.RemainingSeconds = 248;
			Primary.TotalSeconds = 600;
			Primary.Progress = 1.f - 248.f / 600.f;
			FRokServerBuildQueueView& Secondary = Queues.Queues.AddDefaulted_GetRef();
			Secondary.QueueIndex = 2;
			Secondary.State = ERokServerBuildQueueState::Locked;
			Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>()->SetBuildQueueSnapshotForTesting(Queues);
			if (!Click(Hud.Get(), TEXT("BuildCityButton")) || !Click(Screen.Get(), TEXT("BuildCityCategory_1"))) return true;
			return Preview(10, FIntPoint(7, 7), TEXT("CavalryPlacement"));
		}
		++Step;
		return StartReplayConstruction(10, CavalryGrid, 8);
	}

private:
	bool StartReplayConstruction(int32 Type, FIntPoint Grid, int32 Seconds)
	{
		const auto* Net = Hud->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
		ReplayRows = Net->GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo"));
		if (!Test.TestTrue(TEXT("The replay has a building collection"), ReplayRows.IsValid())) return true;
		const auto* City = Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
		ReplayIndex = City->GetAppliedBuildings().Last().Index + 1;
		ReplayType = Type;
		ReplayGrid = Grid;
		bSawPendingConstruction = false;
		ReplayRows->SetObjectField(FString::FromInt(ReplayIndex),
			MakeRow(ReplayIndex, Type, 0, Grid.X, Grid.Y, FDateTime::UtcNow().ToUnixTimestamp() + Seconds));
		Deadline = FPlatformTime::Seconds() + 15.0;
		return false;
	}

	void VerifyClassicalMenuModels()
	{
		UWorld* World = Hud->GetWorld();
		auto* City = World->GetSubsystem<URokServerCitySubsystem>();
		const auto* Upgrades = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
		const auto* Catalog = World->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
		TArray<FRokServerBuilding> Original;
		FString Error;
		if (!RokServerCity::ParseBuildings(ReplayRows, Original, Error)) { Test.AddError(Error); return; }
		ON_SCOPE_EXIT { Test.TestTrue(TEXT("Restore the replay city after the menu sweep"), City->ApplyBuildings(Original, Error)); };
		FRokCityPlacementWorldFrame Frame;
		if (!City->GetAppliedFrame(Frame)) { Test.AddError(TEXT("No verified replay city frame")); return; }
		const int32 Index = Original.Last().Index + 1;
		for (const int32 Type : {5, 12, 17, 19, 20, 21, 29, 30, 31, 51, 52, 53})
		{
			const FString Model = Upgrades->GetCityConstructionModelId(Type, 5, 101);
			FRokUnityCatalogBuildingType Row;
			if (!Test.TestTrue(*FString::Printf(TEXT("Catalog and classical model for type %d"), Type),
				Catalog->GetBuildingType(Type, Row) && !Model.IsEmpty())) continue;
			const FIntPoint Size(Row.Width, Row.Length);
			auto* Ghost = World->SpawnActor<ARokCityPlacementPreviewActor>();
			if (!Test.TestNotNull(TEXT("Placement preview actor"), Ghost)) continue;
			const FIntPoint Grid(4, 13);
			Test.TestTrue(*Model, Ghost->InitializePreview(Type, Model, Error));
			Ghost->SetCandidate(Frame.GetCityContainerTransform(), {Grid, Size});
			Test.TestTrue(TEXT("Preview has the original static mesh"), Ghost->GetBuildingMeshComponent()->GetStaticMesh() != nullptr);
			Ghost->Destroy();

			TArray<FRokServerBuilding> Updated = Original;
			Updated.Add({Index, Type, 1, Grid, -1});
			if (!Test.TestTrue(*FString::Printf(TEXT("Apply completed type %d"), Type), City->ApplyBuildings(Updated, Error))) continue;
			const auto* Body = City->FindSpawnedBuilding(Index);
			Test.TestTrue(*FString::Printf(TEXT("Visible original model at the server grid: %s"), *Model),
				Body && !Body->IsHidden() && Body->GetGameplayTile() == Grid && Body->GetFootprintSize() == Size
				&& Body->ActorHasTag(*FString::Printf(TEXT("RokLocalSourceModel_%s"), *Model)));
		}
		TArray<FRokServerBuilding> Decorations = Original;
		Decorations.RemoveAll([](const FRokServerBuilding& Row) { return Row.Type >= 51 && Row.Type <= 53; });
		if (!City->ApplyBuildings(Decorations, Error)) { Test.AddError(Error); return; }
		for (const int32 Type : {51, 52, 53})
		{
			FRokLocalConstructionPreview Entry;
			Test.TestTrue(TEXT("Zero-second decoration passes live construction prerequisites"),
				Upgrades->PreviewConstructionEntry(Type, Entry) && Entry.bSourceEligible && Entry.BuildSeconds == 0);
			FIntPoint Grid;
			const bool bFound = FRokCityPlacementFirstFit::Find(Entry.GridSize, Entry.Footprint, [&](FIntPoint Position)
			{
				FRokLocalConstructionPreview Preview;
				return Upgrades->PreviewConstruction({Type, 101, 2, Position}, Preview) && Preview.bSourceEligible
					&& !Preview.bNeedsWorkerQueue;
			}, Grid);
			Test.TestTrue(TEXT("Instant decoration finds a valid placement without a worker"), bFound);
		}
	}

	void VerifyCivilizationAges()
	{
		UWorld* World = Hud->GetWorld();
		auto* City = World->GetSubsystem<URokServerCitySubsystem>();
		const auto* Upgrades = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
		const auto* Net = World->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
		const auto* Catalog = World->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
		const auto Role = Net->GetRoleInfo();
		const auto OriginalCountry = Role->TryGetField(TEXT("country"));
		TArray<FRokServerBuilding> Original;
		FString Error;
		if (!RokServerCity::ParseBuildings(ReplayRows, Original, Error)) { Test.AddError(Error); return; }
		ON_SCOPE_EXIT
		{
			if (OriginalCountry) Role->SetField(TEXT("country"), OriginalCountry);
			else Role->RemoveField(TEXT("country"));
			Test.TestTrue(TEXT("Restore the original city and civilization"), City->ApplyBuildings(Original, Error));
		};
		FRokCityPlacementWorldFrame Frame;
		FRokCityConstructionGrid Grid;
		if (!City->GetAppliedFrame(Frame) || !Grid.Load(Error)) { Test.AddError(Error); return; }
		int32 VerifiedRows = 0;
		int32 VerifiedHalls = 0;
		int32 MissingRows = 0;
		int32 VerifiedBoundaries = 0;
		TSet<int32> ShelfLengths;
		for (const int32 Country : {101, 201, 301, 401, 501, 601})
		{
			Role->SetNumberField(TEXT("country"), Country);
			for (const int32 HallLevel : {1, 4, 10, 16, 21})
			{
				const int32 Age = Grid.GetAgeForTownHallLevel(HallLevel);
				TArray<FRokServerBuilding> Fixture = Original;
				Fixture.RemoveAll([](const FRokServerBuilding& Row) { return Row.Type >= 51 && Row.Type <= 53; });
				for (auto& Row : Fixture)
				{
					if (Row.Type == 1) Row.Level = HallLevel;
				}
				if (!City->ApplyBuildings(Fixture, Error)) { Test.AddError(Error); continue; }
				for (const int32 Type : {6, 8})
				{
					const auto* BoundaryRow = Fixture.FindByPredicate([Type](const FRokServerBuilding& Row) { return Row.Type == Type; });
					if (!BoundaryRow) { Test.AddError(TEXT("Boundary row missing from the replay")); continue; }
					const auto* Boundary = City->FindBuildingBarAnchor(BoundaryRow->Index);
					const FString Model = Upgrades->GetCityConstructionModelId(Type, HallLevel, Country);
					const bool bSourcePresent = Country != 301 && Country != 401;
					if (bSourcePresent)
					{
						Test.TestTrue(*FString::Printf(TEXT("Visible boundary %d/%d %s"), Country, Age, *Model),
							Boundary && Boundary->ActorHasTag(FName(*Model)) && !Boundary->IsHidden()
							&& Boundary->GetComponentByClass<UStaticMeshComponent>());
						++VerifiedBoundaries;
					}
					else
					{
						Test.TestFalse(TEXT("Missing boundary has no replacement with a foreign model"),
							Boundary && (Boundary->ActorHasTag(TEXT("RokServerCityWall")) || Boundary->ActorHasTag(TEXT("RokServerGuardTower"))));
					}
				}
				const FString HallModel = Upgrades->GetCityConstructionModelId(1, HallLevel, Country);
				if (FRokLocalConstructionVisualFactory::FindBuildingVisual(1, HallModel))
				{
					const auto* Hall = Cast<ARokCitySpriteActor>(City->FindBuildingBarAnchor(5));
					Test.TestTrue(*FString::Printf(TEXT("Town hall %d/%d %s"), Country, Age, *HallModel),
						Hall && !Hall->IsHidden() && Hall->GetSpriteMeshComponent()->GetStaticMesh()
						&& (Hall->ActorHasTag(*FString::Printf(TEXT("RokLocalSourceModel_%s"), *HallModel))
							|| Hall->ActorHasTag(*FString::Printf(TEXT("UnityModel_%s"), *HallModel))));
					++VerifiedHalls;
					auto UpgradingHall = Fixture;
					if (auto* HallRow = UpgradingHall.FindByPredicate([](const FRokServerBuilding& Row) { return Row.Type == 1; }))
					{
						HallRow->FinishTime = 1794099043;
						Test.TestTrue(TEXT("Town hall construction uses the nine-grid shelf"),
							City->ApplyBuildings(UpgradingHall, Error) && City->FindConstructionScaffold(HallRow->Index)
							&& City->FindConstructionScaffold(HallRow->Index)->ActorHasTag(TEXT("Build_shelf_9")));
						ShelfLengths.Add(9);
					}
					Test.TestTrue(TEXT("Town hall completion removes its shelf"),
						City->ApplyBuildings(Fixture, Error) && !City->FindConstructionScaffold(5));
				}
				FRokLocalConstructionPreview Decoration;
				Test.TestTrue(TEXT("Placement uses the live civilization and hall age"),
					Upgrades->PreviewConstructionEntry(51, Decoration) && Decoration.bSourceEligible
					&& Decoration.ResolvedAge == Age && Decoration.VisualCivilization == Country);
				for (const int32 Type : {2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
					20, 21, 28, 29, 30, 31, 51, 52, 53})
				{
					const FString Model = Upgrades->GetCityConstructionModelId(Type, HallLevel, Country);
					if (Model.IsEmpty())
					{
						Test.AddError(FString::Printf(TEXT("No Unity model key for type %d, civilization %d, age %d"), Type, Country, Age));
						continue;
					}
					if (!FRokLocalConstructionVisualFactory::FindBuildingVisual(Type, Model))
					{
						++MissingRows;
						continue;
					}
					FRokUnityCatalogBuildingType Config;
					if (!Catalog->GetBuildingType(Type, Config)) { Test.AddError(Model); continue; }
					const FIntPoint Size(Config.Width, Config.Length);
					FIntPoint Origin;
					if (!FRokCityPlacementFirstFit::Find(Grid.GetSizeForAge(Age), Size, [&](FIntPoint Pos)
					{
						return Grid.CheckFootprint(Age, {Pos, Size}, {}, Error, nullptr, false);
					}, Origin)) { Test.AddError(Model); continue; }
					auto* Ghost = World->SpawnActor<ARokCityPlacementPreviewActor>();
					if (!Ghost) { Test.AddError(Model); continue; }
					const bool bPreview = Ghost->InitializePreview(Type, Model, Error);
					Ghost->SetCandidate(Frame.GetCityContainerTransform(), {Origin, Size});
					Test.TestTrue(*FString::Printf(TEXT("Placement %d/%d %s"), Country, Age, *Model),
						bPreview && !Ghost->IsHidden() && Ghost->GetBuildingMeshComponent()->GetStaticMesh() != nullptr);
					Ghost->Destroy();
					const int32 Index = Fixture.Last().Index + 1;
					TArray<FRokServerBuilding> Updated = Fixture;
					if (Config.Level1BuildSeconds > 0)
					{
						Updated.Add({Index, Type, 0, Origin, 1794099043});
						Test.TestTrue(*FString::Printf(TEXT("Pending %s"), *Model), City->ApplyBuildings(Updated, Error)
							&& City->FindBuildingBarAnchor(Index) && !City->FindSpawnedBuilding(Index));
						if (Model != TEXT("build_28_501_1"))
						{
							const auto* Scaffold = City->FindConstructionScaffold(Index);
							Test.TestTrue(*FString::Printf(TEXT("Scaffold %s"), *Model), Scaffold && !Scaffold->IsHidden()
								&& Scaffold->ActorHasTag(*FString::Printf(TEXT("Build_shelf_%d"), Size.Y))
								&& Scaffold->GetComponentByClass<UStaticMeshComponent>());
							ShelfLengths.Add(Size.Y);
						}
						Updated.Pop();
					}
					Updated.Add({Index, Type, 1, Origin, -1});
					const bool bApplied = City->ApplyBuildings(Updated, Error);
					const auto* Body = City->FindSpawnedBuilding(Index);
					Test.TestTrue(*FString::Printf(TEXT("Completed %d/%d %s"), Country, Age, *Model),
						bApplied && Body && !City->FindConstructionScaffold(Index)
						&& !Body->IsHidden() && Body->GetGameplayTile() == Origin && Body->GetFootprintSize() == Size
						&& Body->ActorHasTag(*FString::Printf(TEXT("RokLocalSourceModel_%s"), *Model)));
					if (Model == TEXT("build_10_101_3"))
					{
						const auto* Effect = City->FindConstructionEffectForTesting(Index, TEXT("build_3004"));
						Test.TestTrue(TEXT("Completion without an effect child uses the Unity button height"),
							Body && Effect && Effect->GetActorLocation().Equals(Body->GetVisualAnchorWorldPosition()
								+ Frame.GetCityContainerTransform().TransformVector(FVector(0, 0, 0.25))));
					}
					++VerifiedRows;
				}
			}
		}
		Test.AddInfo(FString::Printf(TEXT("Verified %d available menu rows across six civilizations and five ages"), VerifiedRows));
		Test.TestEqual(TEXT("Every source-backed menu row passed through the city"), VerifiedRows, 505);
		Test.TestEqual(TEXT("Every source-backed town hall was checked"), VerifiedHalls, 20);
		Test.TestEqual(TEXT("The menu rows missing Unity bodies have no substitute"), MissingRows, 245);
		Test.TestEqual(TEXT("All source boundary configurations were exercised"), VerifiedBoundaries, 40);
		Test.TestEqual(TEXT("All four original shelf sizes were exercised"), ShelfLengths.Num(), 4);
	}

	bool Click(UUserWidget* Owner, FName Name)
	{
		auto* Button = Cast<UButton>(Owner->GetWidgetFromName(Name));
		if (!Test.TestTrue(Name.ToString(), Button && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		if (Owner == Screen.Get()) Screen->FlushPendingActionForTesting();
		return true;
	}

	bool Preview(int32 Type, const FIntPoint& Footprint, const TCHAR* CaptureName)
	{
		if (!Click(Screen.Get(), *FString::Printf(TEXT("BuildCityCardButton_%d"), Type))) return true;
		const auto State = Controller->GetCityPlacementPreviewSnapshot();
		Test.TestFalse(TEXT("Card closes the drawer"), Screen->IsScreenOpen());
		Test.TestTrue(TEXT("Card places a visible ghost on a valid tile"),
			State.bActive && State.bGridValid && State.PreviewActor.IsValid() && !State.PreviewActor->IsHidden());
		Test.TestEqual(TEXT("Source building type"), State.TypeId, Type);
		Test.TestEqual(TEXT("Source footprint"), State.Footprint, Footprint);
		if (Type == 4) QuarryGrid = State.Grid;
		if (Type == 10) CavalryGrid = State.Grid;
		if (Type == 12) SiegeGrid = State.Grid;
		if (bOtherCivilizationCapture)
		{
			const auto* Expected = FRokLocalConstructionVisualFactory::FindBuildingVisual(12, TEXT("build_12_501_3"));
			const auto* Ghost = Cast<ARokCityPlacementPreviewActor>(State.PreviewActor.Get());
			Test.TestTrue(TEXT("The placement card uses the third-age 501 siege model"),
				Expected && Ghost
				&& Ghost->GetBuildingMeshComponent()->GetStaticMesh() == FSoftObjectPath(Expected->MeshPath).ResolveObject());
		}
		Test.TestFalse(TEXT("Replay cannot submit construction"), State.bCanCommit);
		const auto* Confirm = Hud->GetWidgetFromName(TEXT("CityPlacementConfirmButton"));
		Test.TestTrue(TEXT("Confirmation is enabled only for the busy-worker route in this replay"),
			Confirm && Confirm->GetIsEnabled() == (Type == 10));
		return Capture(CaptureName);
	}

	void CheckQuarryLifecycle()
	{
		auto* City = Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
		const TArray<FRokServerBuilding> Original = City->GetAppliedBuildings();
		TArray<FRokServerBuilding> Updated = Original;
		const int32 Index = Updated.Last().Index + 1;
		Updated.Add({Index, 4, 0, QuarryGrid, 1794099043});
		FString Error;
		if (Test.TestTrue(TEXT("City applies the pending quarry row"), City->ApplyBuildings(Updated, Error)))
		{
			Test.TestNotNull(TEXT("Construction countdown has a world anchor"), City->FindBuildingBarAnchor(Index));
			Test.TestNull(TEXT("Pending quarry has no completed building actor"), City->FindSpawnedBuilding(Index));
			Updated.Last().Level = 1;
			Updated.Last().FinishTime = -1;
			if (Test.TestTrue(TEXT("City applies the completed quarry row"), City->ApplyBuildings(Updated, Error)))
			{
				const auto* Body = City->FindBuildingBarAnchor(Index);
				Test.TestTrue(TEXT("Completed quarry shows the source building body"), Body
					&& Body->ActorHasTag(TEXT("RokLocalSourceModel_build_4_101_2")));
			}
		}
		Test.TestTrue(TEXT("Replay restores the original city"), City->ApplyBuildings(Original, Error));
	}

	bool WaitForReplayConstruction()
	{
		const auto* City = Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
		if (bAwaitingReplayCancellation)
		{
			if (City->FindBuildingBarAnchor(ReplayIndex) && FPlatformTime::Seconds() < Deadline) return false;
			Test.TestFalse(TEXT("Cancelled construction clears its model, shelf and particle actors"),
				City->FindBuildingBarAnchor(ReplayIndex) || City->FindConstructionBody(ReplayIndex)
				|| City->FindConstructionScaffold(ReplayIndex)
				|| City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3003"))
				|| City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3003_shelf"))
				|| City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3004")));
			CheckConstructionSoundCounts(1, 0);
			bAwaitingReplayCancellation = false;
			bCapturedReplayCancellation = true;
			return Capture(TEXT("Civilization501Age3SiegeCancelled"), 0.12);
		}
		if (bCapturedReplayCancellation && !bRestartedReplayConstruction)
		{
			if (IFileManager::Get().FileSize(*PendingCapture) <= 0 && FPlatformTime::Seconds() < Deadline) return false;
			bRestartedReplayConstruction = true;
			bSawPendingConstruction = false;
			return StartReplayConstruction(ReplayType, ReplayGrid, 10);
		}
		if (!bSawPendingConstruction && City->FindBuildingBarAnchor(ReplayIndex))
		{
			Test.TestNull(TEXT("Pending building has no finished actor"), City->FindSpawnedBuilding(ReplayIndex));
			const auto* ConstructionBody = City->FindConstructionBody(ReplayIndex);
			Test.TestTrue(TEXT("Unity keeps the building body visible beneath the scaffold"),
				ConstructionBody && !ConstructionBody->IsHidden() && !ConstructionBody->GetActorEnableCollision()
				&& ConstructionBody->GetGameplayTile() == ReplayGrid);
			const auto* Scaffold = City->FindConstructionScaffold(ReplayIndex);
			Test.TestTrue(TEXT("A visible source shelf is present during construction"),
				Scaffold && Scaffold->ActorHasTag(TEXT("Build_shelf_7")) && !Scaffold->IsHidden());
			Test.TestNotNull(TEXT("Construction starts the world dust"), City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3003")));
			Test.TestNotNull(TEXT("The shelf starts its own dust"), City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3003_shelf")));
			CheckConstructionSoundCounts(bRestartedReplayConstruction ? 2 : 1, 0);
			bSawPendingConstruction = true;
			if (bOtherCivilizationCapture && !bRestartedReplayConstruction)
				return Capture(TEXT("Civilization501Age3SiegeConstruction"), 0.25, true);
		}
		if (bSawPendingConstruction && (!bOtherCivilizationCapture || IFileManager::Get().FileSize(*PendingCapture) > 0))
		{
			if (bOtherCivilizationCapture && !bCapturedReplayCancellation)
			{
				const auto* Dust = City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3003"));
				auto* Mesh = Dust ? Dust->GetComponentByClass<UProceduralMeshComponent>() : nullptr;
				const auto* Section = Mesh ? Mesh->GetProcMeshSection(0) : nullptr;
				Test.TestTrue(TEXT("Open construction renders source dust geometry"),
					Section && Section->ProcIndexBuffer.Num() > 0);
				ReplayRows->SetObjectField(FString::FromInt(ReplayIndex),
					MakeRow(ReplayIndex, ReplayType, -1, ReplayGrid.X, ReplayGrid.Y, 0));
				bAwaitingReplayCancellation = true;
				return false;
			}
			ReplayRows->SetObjectField(FString::FromInt(ReplayIndex),
				MakeRow(ReplayIndex, ReplayType, 1, ReplayGrid.X, ReplayGrid.Y));
		}
		if (const auto* Body = City->FindSpawnedBuilding(ReplayIndex))
		{
			const TCHAR* ModelTag = bOtherCivilizationCapture ? TEXT("RokLocalSourceModel_build_12_501_3")
				: ReplayType == 10 ? TEXT("RokLocalSourceModel_build_10_101_2") : TEXT("RokLocalSourceModel_build_12_101_2");
			Test.TestTrue(TEXT("Completed building is visible at its chosen grid with the source model"),
				bSawPendingConstruction && !City->FindConstructionScaffold(ReplayIndex) && !City->FindConstructionBody(ReplayIndex)
				&& Body == City->FindBuildingBarAnchor(ReplayIndex)
				&& Body->GetGameplayTile() == ReplayGrid && Body->GetFootprintSize() == FIntPoint(7, 7)
				&& Body->ActorHasTag(ModelTag) && !Body->IsHidden()
				&& Body->GetSpriteMeshComponent()->GetStaticMesh());
			++Step;
			Test.TestNotNull(TEXT("The completed building starts its own burst"),
				City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3004")));
			Test.TestNull(TEXT("The completed building clears its construction dust"),
				City->FindConstructionEffectForTesting(ReplayIndex, TEXT("build_3003_shelf")));
			CheckConstructionSoundCounts(2, 1);
			if (bOtherCivilizationCapture) return Capture(TEXT("Civilization501Age3SiegeCompleted"), 0.25, true);
			return Capture(ReplayType == 10 ? TEXT("CavalryCompleted") : TEXT("SiegeWorkshopCompleted"));
		}
		if (FPlatformTime::Seconds() < Deadline) return false;
		RemoveReplayRow();
		RestoreOtherCivilization();
		Test.AddError(TEXT("The city did not apply the building construction states."));
		return true;
	}

	bool BeginConstructionRecording()
	{
		auto* Audio = Hud->GetGameInstance()->GetSubsystem<URokAudioRouterSubsystem>();
		RecordingDevice = Hud->GetWorld()->GetAudioDevice();
		if (!Test.TestTrue(TEXT("Construction recording has an active audio device and unmuted SFX"),
			RecordingDevice.IsValid() && Audio && Audio->GetSfxVolume() > 0.0f))
		{
			RecordingDevice.Reset();
			return false;
		}
		RecordingDevice->StartRecording(nullptr, 30.0f);
		Audio->SetSourceSfxPlaybackForTesting(true);
		RecordingAudioRouter = Audio;
		return true;
	}

	bool RunUpgradeActions(int32 Action)
	{
		auto& Net = *Hud->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
		auto* City = Hud->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
		const auto PushBuilding = [&](int32 Level, int64 Finish, int32 Version)
		{
			FRokCityUpgradeReplay::PushBuildingTwice(Net, MakeRow(ReplayIndex, ReplayType, Level,
				ReplayGrid.X, ReplayGrid.Y, Finish).ToSharedRef(), Version);
			TArray<FRokServerBuilding> Rows;
			FString Error;
			if (RokServerCity::ParseBuildings(ReplayRows, Rows, Error) && City->ApplyBuildings(Rows, Error)) return true;
			Test.AddError(Error);
			return false;
		};
		const auto Effect = [&](FName Name) { return City->FindConstructionEffectForTesting(ReplayIndex, Name); };
		// Let the previous one-shot finish before starting a distinct demonstration.
		if (Action == 5 && Effect(TEXT("build_3004")))
		{
			--Step;
			return false;
		}
		const auto CheckFinished = [&](int32 Level)
		{
			const auto* Body = City->FindSpawnedBuilding(ReplayIndex);
			Test.TestTrue(TEXT("Confirmed upgrade shows the new level and removes its shelf and dust"),
				Body && Body->ActorHasTag(FName(*FString::Printf(TEXT("UnityBuildingLevel_%d"), Level)))
				&& !City->FindConstructionScaffold(ReplayIndex) && !Effect(TEXT("build_3003_shelf"))
				&& !Effect(TEXT("build_3003")));
			Test.TestNotNull(TEXT("Confirmed upgrade produces a completion burst"), Effect(TEXT("build_3004")));
		};
		const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
		switch (Action)
		{
		case 0:
			FRokCityUpgradeReplay::Submit(Net, ReplayIndex, 2, false);
			FRokCityUpgradeReplay::AckTwice(Net);
			Test.TestFalse(TEXT("Timed upgrade ACK waits for the building push"), FRokCityUpgradeReplay::IsConfirmed(Net));
			if (!PushBuilding(1, Now + 60, 1001)) return true;
			Test.TestTrue(TEXT("Server timer confirms the timed upgrade"), FRokCityUpgradeReplay::IsConfirmed(Net));
			Test.TestNotNull(TEXT("Timed upgrade shows the shelf"), City->FindConstructionScaffold(ReplayIndex));
			Test.TestNotNull(TEXT("Timed upgrade plays the shelf dust"), Effect(TEXT("build_3003_shelf")));
			Test.TestNull(TEXT("Upgrade has no separate placement dust"), Effect(TEXT("build_3003")));
			CheckConstructionSoundCounts(3, 1);
			return Capture(TEXT("TimedUpgradeDust"), 0.25, true);
		case 1:
			if (!PushBuilding(2, -1, 1002)) return true;
			CheckFinished(2);
			CheckConstructionSoundCounts(3, 2);
			return Capture(TEXT("TimedUpgradeCompleted"), 0.25, true);
		case 2:
		{
			const auto* Burst = Effect(TEXT("build_3004"));
			if (!PushBuilding(1, Now + 60, 1001) || !PushBuilding(2, -1, 1002)) return true;
			Test.TestTrue(TEXT("Late start and duplicate completion do not restart the timed upgrade"),
				Burst && Effect(TEXT("build_3004")) == Burst && !City->FindConstructionScaffold(ReplayIndex));
			CheckConstructionSoundCounts(3, 2);
			return Capture(TEXT("TimedUpgradeStable"), 2.05, true);
		}
		case 3:
			FRokCityUpgradeReplay::Submit(Net, ReplayIndex, 3, true);
			FRokCityUpgradeReplay::AckTwice(Net);
			Test.TestFalse(TEXT("Instant upgrade ACK waits for the higher building level"), FRokCityUpgradeReplay::IsConfirmed(Net));
			Test.TestFalse(TEXT("Instant upgrade submission has no shelf or burst"),
				City->FindConstructionScaffold(ReplayIndex) || Effect(TEXT("build_3004")));
			CheckConstructionSoundCounts(4, 2);
			return Capture(TEXT("InstantUpgradeSubmitted"), 0.65);
		case 4:
			if (!PushBuilding(3, -1, 1003)) return true;
			Test.TestTrue(TEXT("Higher level confirms the instant upgrade"), FRokCityUpgradeReplay::IsConfirmed(Net));
			CheckFinished(3);
			CheckConstructionSoundCounts(4, 3);
			return Capture(TEXT("InstantUpgradeCompleted"), 0.25, true);
		case 5:
			FRokCityUpgradeReplay::Submit(Net, ReplayIndex, 4, false);
			FRokCityUpgradeReplay::AckTwice(Net);
			UpgradeQueueTime = Now + 120;
			if (!PushBuilding(3, UpgradeQueueTime, 1004)) return true;
			Test.TestTrue(TEXT("Second timed upgrade starts in the server state"), FRokCityUpgradeReplay::IsConfirmed(Net));
			FRokCityUpgradeReplay::PushQueue(Net, ReplayIndex, UpgradeQueueTime);
			CheckConstructionSoundCounts(5, 3);
			return Capture(TEXT("SpeedUpBefore"), 0.25, true);
		case 6:
		{
			const auto* ShelfDust = Effect(TEXT("build_3003_shelf"));
			const auto* PriorBurst = Effect(TEXT("build_3004"));
			FRokCityUpgradeReplay::Submit(Net, ReplayIndex, 4, false, UpgradeQueueTime);
			UpgradeQueueTime -= 60;
			FRokCityUpgradeReplay::AckTwice(Net, UpgradeQueueTime);
			Test.TestFalse(TEXT("Partial speed-up ACK waits for the queue push"), FRokCityUpgradeReplay::IsConfirmed(Net));
			FRokCityUpgradeReplay::PushQueue(Net, ReplayIndex, UpgradeQueueTime);
			Test.TestTrue(TEXT("Partial speed-up confirms the shorter timer"), FRokCityUpgradeReplay::IsConfirmed(Net));
			Test.TestTrue(TEXT("Partial speed-up leaves the shelf in place and starts no particles"),
				City->FindConstructionScaffold(ReplayIndex) && Effect(TEXT("build_3003_shelf")) == ShelfDust
				&& Effect(TEXT("build_3004")) == PriorBurst);
			CheckConstructionSoundCounts(5, 3, 1);
			return Capture(TEXT("SpeedUpShortened"), 2.05, true);
		}
		case 7:
			FRokCityUpgradeReplay::Submit(Net, ReplayIndex, 4, false, UpgradeQueueTime);
			FRokCityUpgradeReplay::AckTwice(Net, 0);
			FRokCityUpgradeReplay::PushQueue(Net, ReplayIndex, -1);
			Test.TestTrue(TEXT("Finishing speed-up confirms the freed queue"), FRokCityUpgradeReplay::IsConfirmed(Net));
			Test.TestNull(TEXT("A freed queue alone does not play construction completion"), Effect(TEXT("build_3004")));
			CheckConstructionSoundCounts(5, 3, 2);
			if (!PushBuilding(4, -1, 1005)) return true;
			CheckFinished(4);
			CheckConstructionSoundCounts(5, 4, 2);
			return Capture(TEXT("SpeedUpCompleted"), 0.25, true);
		case 8:
		{
			const auto* Burst = Effect(TEXT("build_3004"));
			if (!PushBuilding(3, UpgradeQueueTime, 1004) || !PushBuilding(4, -1, 1005)) return true;
			CheckFinished(4);
			Test.TestTrue(TEXT("Repeated and stale building pushes preserve the existing completion burst"),
				Burst && Effect(TEXT("build_3004")) == Burst);
			CheckConstructionSoundCounts(5, 4, 2);
			if (UpgradeOriginalQueue) Net.GetRoleInfo()->SetField(TEXT("buildQueue"), UpgradeOriginalQueue);
			else Net.GetRoleInfo()->RemoveField(TEXT("buildQueue"));
			return Capture(TEXT("SpeedUpStable"), 1.75);
		}
		}
		return true;
	}

	void CheckConstructionSoundCounts(uint32 Starts, uint32 Completions, uint32 SpeedUps = 0)
	{
		if (const auto* Audio = RecordingAudioRouter.Get())
		{
			Test.TestEqual(TEXT("Construction start voices"), Audio->GetSourceSfxCountForTesting(4025), Starts);
			Test.TestEqual(TEXT("Construction completion voices"), Audio->GetSourceSfxCountForTesting(4080), Completions);
			Test.TestEqual(TEXT("Construction speed-up voices"), Audio->GetSourceSfxCountForTesting(4014), SpeedUps);
		}
	}

	void WriteConstructionRecording()
	{
		if (auto* Audio = RecordingAudioRouter.Get()) Audio->SetSourceSfxPlaybackForTesting(false);
		float Channels = 0.0f;
		float Rate = 0.0f;
		const auto& Mix = RecordingDevice->StopRecording(nullptr, Channels, Rate);
		if (Test.TestTrue(TEXT("The audio mixer produced a recorded buffer"), Channels > 0 && Rate > 0 && !Mix.IsEmpty()))
		{
			TArray<int16> PCM;
			PCM.Reserve(Mix.Num());
			bool bAudible = false;
			for (float Sample : Mix)
			{
				const int16 Value = static_cast<int16>(FMath::RoundToInt(FMath::Clamp(Sample, -1.0f, 1.0f) * 32767.0f));
				bAudible |= FMath::Abs(Value) > 10;
				PCM.Add(Value);
			}
			Test.TestTrue(TEXT("The construction mixer recording contains audible samples"), bAudible);
			TArray<uint8> Wave;
			SerializeWaveFile(Wave, reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16),
				static_cast<int32>(Channels), static_cast<int32>(Rate));
			Test.TestTrue(TEXT("The construction mixer recording was saved"), FFileHelper::SaveArrayToFile(Wave,
				*(FPaths::ProjectSavedDir() / TEXT("Automation/BuildCity/ConstructionAndUpgradeAudio.wav"))));
		}
		RecordingDevice.Reset();
	}

	void RemoveReplayRow()
	{
		ReplayRows->RemoveField(FString::FromInt(ReplayIndex));
	}

	void RestoreOtherCivilization()
	{
		if (!bOtherCivilizationCapture) return;
		bOtherCivilizationCapture = false;
		const auto Role = Hud->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>()->GetRoleInfo();
		if (CountryBeforeCapture) Role->SetField(TEXT("country"), CountryBeforeCapture);
		else Role->RemoveField(TEXT("country"));
		if (RoleLevelBeforeCapture) Role->SetField(TEXT("level"), RoleLevelBeforeCapture);
		else Role->RemoveField(TEXT("level"));
		ReplayRows->GetObjectField(TEXT("5"))->SetNumberField(TEXT("level"), OriginalHallLevel);
	}

	bool Capture(const TCHAR* Name, double Delay = 2.0, bool bWorldTime = false)
	{
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/BuildCity"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		PendingCapture = Directory / (FString(Name) + TEXT(".png"));
		bCaptureInWorldTime = bWorldTime;
		const double WallTime = FPlatformTime::Seconds();
		CaptureAt = (bWorldTime ? Hud->GetWorld()->GetTimeSeconds() : WallTime) + Delay;
		Deadline = WallTime + Delay + 15.0;
		IFileManager::Get().Delete(*PendingCapture);
		return false;
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<ARokPlayerController> Controller;
	TWeakObjectPtr<URokAudioRouterSubsystem> RecordingAudioRouter;
	FAudioDeviceHandle RecordingDevice;
	FString PendingCapture;
	FIntPoint QuarryGrid = FIntPoint::ZeroValue;
	FIntPoint CavalryGrid = FIntPoint::ZeroValue;
	FIntPoint SiegeGrid = FIntPoint::ZeroValue;
	FIntPoint ReplayGrid = FIntPoint::ZeroValue;
	int32 ReplayType = 0;
	TSharedPtr<FJsonObject> ReplayRows;
	int32 ReplayIndex = 0;
	bool bSawPendingConstruction = false;
	bool bAwaitingReplayCancellation = false;
	bool bCapturedReplayCancellation = false;
	bool bRestartedReplayConstruction = false;
	bool bOtherCivilizationCapture = false;
	int32 OriginalHallLevel = 0;
	TSharedPtr<FJsonValue> CountryBeforeCapture;
	TSharedPtr<FJsonValue> RoleLevelBeforeCapture;
	TSharedPtr<FJsonValue> UpgradeOriginalQueue;
	int64 UpgradeQueueTime = 0;
	double CaptureAt = 0.0;
	bool bCaptureInWorldTime = false;
	double Deadline = FPlatformTime::Seconds() + 25.0;
	int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildCityReplayFlowTest, "Rok.Ui.BuildCity.RenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokBuildCityReplayFlowTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FBuildCityReplayFlow(*this));
	return true;
}
#endif
