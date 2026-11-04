#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokCityConstructionSources.h"
#include "../RokDataPaths.h"
#include "../RokLocalConstructionVisualFactory.h"

// Commander hall (统帅厅, type 18): Unity BuildCityMediator -> CreateTempBuild with
// build_18_101_1, the same tight Sprite.mesh path as Farm/Wood but no production.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalCommanderHallSourceVisualTest,
	"Rok.Runtime.LocalConstruction.CommanderHall.SourceVisualAndLevelOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalCommanderHallSourceVisualTest::RunTest(const FString&)
{
	const FRokLocalSourceBuildingVisual* Hall = FRokLocalConstructionVisualFactory::FindSourceVisual(18);
	if (!TestNotNull(TEXT("Commander hall has a source visual entry"), Hall)) return false;
	TestEqual(TEXT("Commander hall uses the 101 age-one source model"), FString(Hall->ModelId), FString(TEXT("build_18_101_1")));
	TestFalse(TEXT("Commander hall registers no production clock"), Hall->bProducesResource);
	TestTrue(TEXT("Farm still produces"), FRokLocalConstructionVisualFactory::FindSourceVisual(2)
		&& FRokLocalConstructionVisualFactory::FindSourceVisual(2)->bProducesResource);
	TestTrue(TEXT("Wood still produces"), FRokLocalConstructionVisualFactory::FindSourceVisual(3)
		&& FRokLocalConstructionVisualFactory::FindSourceVisual(3)->bProducesResource);
	TestNull(TEXT("Academy has no migrated source visual"), FRokLocalConstructionVisualFactory::FindSourceVisual(13));
	TestNull(TEXT("Town hall is never a local construction"), FRokLocalConstructionVisualFactory::FindSourceVisual(1));

	FRokCityConstructionSources Sources;
	FString Error;
	if (!TestTrue(TEXT("Source construction models load"), Sources.Load(Error)))
	{
		AddError(Error); return false;
	}
	for (const int32 TypeId : {2, 3, 18})
	{
		FRokCityConstructionModel Model;
		const FRokLocalSourceBuildingVisual* Visual = FRokLocalConstructionVisualFactory::FindSourceVisual(TypeId);
		TestTrue(FString::Printf(TEXT("Type %d visual table matches the source model row"), TypeId),
			Visual && Sources.GetModel(TypeId, 101, 1, Model) && Model.HasVisualSource()
			&& Model.ModelId == Visual->ModelId);
	}

	FString LevelJson;
	TSharedPtr<FJsonObject> LevelRoot;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!TestTrue(TEXT("Source building level rows load"),
		FFileHelper::LoadFileToString(LevelJson, *RokDataPaths::Resolve(TEXT("ConfigTables/BuildingLevelData.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LevelJson), LevelRoot) && LevelRoot.IsValid()
		&& LevelRoot->TryGetArrayField(TEXT("rows"), Rows) && Rows)) return false;
	TSharedPtr<FJsonObject> LevelOneRow;
	for (const TSharedPtr<FJsonValue>& Value : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		double Type = 0.0, Level = 0.0;
		if (Row.IsValid() && Row->TryGetNumberField(TEXT("type"), Type)
			&& Row->TryGetNumberField(TEXT("level"), Level) && Type == 18.0 && Level == 1.0)
		{
			LevelOneRow = Row;
			break;
		}
	}
	if (!TestTrue(TEXT("BuildingLevelData row 1801 exists"), LevelOneRow.IsValid())) return false;
	FRokUnityBuildingLevelConfig LevelOne;
	LevelOne.TypeId = 18;
	LevelOne.Level = 1;
	TestTrue(TEXT("Level-one economy parses"), LevelOne.ParseSourceEconomy(LevelOneRow));
	TestEqual(TEXT("Source food cost"), LevelOne.Cost.Food, 200);
	TestEqual(TEXT("Source wood cost"), LevelOne.Cost.Wood, 200);
	TestEqual(TEXT("No stone cost"), LevelOne.Cost.Stone, 0);
	TestEqual(TEXT("No gold cost"), LevelOne.Cost.Gold, 0);
	TestEqual(TEXT("Source build time is two seconds"), LevelOne.BuildSeconds, 2);
	TestEqual(TEXT("Source level-one power"), LevelOne.Power, 5);
	TestTrue(TEXT("Level-one prerequisites parse"), LevelOne.ParsePrerequisites(LevelOneRow));
	TMap<int32, int32> HallOne;
	HallOne.Add(1, 1);
	FRokUnityBuildingPrerequisite Missing;
	int32 CurrentLevel = 0;
	TestTrue(TEXT("Initial Hall1 city may build the commander hall"),
		LevelOne.CheckPrerequisites(HallOne, Missing, CurrentLevel));

	UStaticMesh* Mesh = Cast<UStaticMesh>(FSoftObjectPath(Hall->MeshPath).TryLoad());
	UMaterialInterface* Material = Cast<UMaterialInterface>(FSoftObjectPath(Hall->MaterialPath).TryLoad());
	if (!TestNotNull(TEXT("Imported tight Sprite.mesh"), Mesh) || !TestNotNull(TEXT("Imported RG material"), Material))
	{
		return false;
	}
	TestEqual(TEXT("Single LOD"), Mesh->GetNumLODs(), 1);
	TestEqual(TEXT("Pinned LOD0 vertices"), Mesh->GetNumVertices(0), Hall->Vertices);
	TestEqual(TEXT("Pinned LOD0 triangles"), Mesh->GetNumTriangles(0), Hall->Triangles);
	TestTrue(TEXT("Mesh slot uses the RG material"), Mesh->GetMaterial(0) == Material);
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	TestTrue(TEXT("Pinned pivot bounds"),
		(Bounds.Origin - Bounds.BoxExtent).Equals(Hall->BoundsLo, 0.05)
		&& (Bounds.Origin + Bounds.BoxExtent).Equals(Hall->BoundsHi, 0.05));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalCommanderHallLedgerTest,
	"Rok.Runtime.LocalConstruction.CommanderHall.LedgerWithoutProduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalCommanderHallLedgerTest::RunTest(const FString&)
{
	FRokCityConstructionGrid Grid;
	FString Error;
	if (!TestTrue(TEXT("Verified source grid loads"), Grid.Load(Error)))
	{
		AddError(Error); return false;
	}
	FRokCityConstructionFootprint Footprint;
	const TArray<FRokCityConstructionFootprint> Empty;
	for (int32 X = -20; X <= 15 && !Footprint.IsValid(); ++X)
	{
		for (int32 Y = -20; Y <= 15; ++Y)
		{
			const FRokCityConstructionFootprint Candidate{FIntPoint(X, Y), FIntPoint(5, 5)};
			if (Grid.CheckFootprint(1, Candidate, Empty, Error))
			{
				Footprint = Candidate;
				break;
			}
		}
	}
	if (!TestTrue(TEXT("An age-one 5x5 source rectangle exists"), Footprint.IsValid())) return false;
	FRokLocalConstructionLedger Ledger;
	FRokLocalConstructionResources Wallet{2500, 2500, 1200, 600};
	FRokLocalConstructionTicket Hall;
	Hall.TypeId = 18; Hall.Civilization = 101; Hall.Age = 1;
	Hall.Footprint = Footprint; Hall.Cost = {200, 200, 0, 0};
	Hall.Power = 5; Hall.StartedAt = 10.0; Hall.FinishesAt = 12.0;
	uint64 Id = 0;
	if (!TestTrue(TEXT("Commander hall reserves the builder queue and debits once"),
		Ledger.TryBegin(Grid, Hall, 0, 1, 0, 1, Wallet, Id, Error)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("Exact food debit"), Wallet.Food, 2300);
	TestEqual(TEXT("Exact wood debit"), Wallet.Wood, 2300);
	TestEqual(TEXT("Stone unchanged"), Wallet.Stone, 1200);
	TArray<FRokLocalConstructionTicket> Completed;
	Ledger.CompleteDue(11.99, Completed);
	TestEqual(TEXT("Two-second build is not completed early"), Completed.Num(), 0);
	Ledger.CompleteDue(12.0, Completed);
	if (!TestEqual(TEXT("Commander hall completes exactly once"), Completed.Num(), 1)) return false;
	uint64 SecondId = 0;
	TestFalse(TEXT("The completed ticket still holds its footprint and count slot"),
		Ledger.TryBegin(Grid, Hall, 0, 1, 0, 1, Wallet, SecondId, Error));
	TestEqual(TEXT("Rejected repeat does not debit"), Wallet.Food, 2300);
	FRokLocalProductionLedger Production;
	FRokLocalProductionSource NoRate{18, 1, 400, 66, 4000};
	TestFalse(TEXT("A completed commander hall cannot start a resource clock"),
		Production.RegisterCompleted(Completed[0], NoRate, 12, Error));
	TestFalse(TEXT("No production clock exists"), Production.HasClock(Id));
	return true;
}
#endif
