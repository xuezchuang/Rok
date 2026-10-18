#include "RokLocalCitySimulationContext.h"

#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ReadJson(const FString& RelativePath, TSharedPtr<FJsonObject>& Out)
{
	FString Json;
	return FFileHelper::LoadFileToString(Json, *(RokDataPaths::ResolveProjectPath(RelativePath)))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Out) && Out.IsValid();
}

bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& Out)
{
	double Number = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(Key, Number)
		|| !FMath::IsFinite(Number) || Number < MIN_int32 || Number > MAX_int32
		|| FMath::FloorToDouble(Number) != Number) return false;
	Out = static_cast<int32>(Number);
	return true;
}

TSharedPtr<FJsonObject> ReadObject(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Key)
{
	const TSharedPtr<FJsonObject>* Value = nullptr;
	return Parent.IsValid() && Parent->TryGetObjectField(Key, Value) && Value ? *Value : nullptr;
}

bool ReadExactString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const TCHAR* Expected)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(Key, Value) && Value == Expected;
}

bool SameInt(const TSharedPtr<FJsonObject>& Left, const TSharedPtr<FJsonObject>& Right, const TCHAR* Key)
{
	int32 L = 0, R = 0;
	return ReadInt(Left, Key, L) && ReadInt(Right, Key, R) && L == R;
}

bool IsSourcePath(const FString& Path)
{
	return !Path.IsEmpty() && !Path.Contains(TEXT("..")) && !Path.Contains(TEXT("\\"))
		&& (Path.StartsWith(TEXT("Saved/RokUnityMigration/"))
			|| Path.StartsWith(TEXT("Scripts/")) || Path.StartsWith(TEXT("ref/config-tables/")));
}

bool HasTags(const AActor* Actor, int32 InitId, const FIntPoint& Tile)
{
	return Actor && Actor->Tags.Contains(TEXT("RokUnityConfigInitBuilding"))
		&& Actor->Tags.Contains(FName(*FString::Printf(TEXT("UnityInitId_%d"), InitId)))
		&& Actor->Tags.Contains(TEXT("UnityBuildingType_1"))
		&& Actor->Tags.Contains(FName(*FString::Printf(TEXT("UnityGridX_%d"), Tile.X)))
		&& Actor->Tags.Contains(FName(*FString::Printf(TEXT("UnityGridY_%d"), Tile.Y)));
}
}

bool FRokLocalCitySimulationContext::Load(FString& OutError)
{
	bLoaded = false;
	TargetLevel.Reset();
	VisualCivilization = InitialAge = InitialHallId = InitialHallLevel = 0;
	InitialHallTile = FIntPoint::ZeroValue;
	SupportedAges.Reset();
	OutError.Reset();
	const FString ContextPath = TEXT("Saved/RokUnityMigration/LocalCitySimulationContext/catalog.json");
	TSharedPtr<FJsonObject> Root;
	if (!ReadJson(ContextPath, Root)
		|| !ReadExactString(Root, TEXT("schema"), TEXT("rok.local_city_simulation_context.v1"))
		|| !ReadExactString(Root, TEXT("mode"), TEXT("local_simulation"))
		|| !Root->TryGetStringField(TEXT("target_level"), TargetLevel)
		|| !TargetLevel.StartsWith(TEXT("/Game/Maps/")))
	{
		OutError = TEXT("Local city simulation context is missing or has an invalid identity");
		return false;
	}
	const TSharedPtr<FJsonObject> Variant = ReadObject(Root, TEXT("visual_variant"));
	const TSharedPtr<FJsonObject> Initial = ReadObject(Root, TEXT("initial_city"));
	const TSharedPtr<FJsonObject> Grid = ReadObject(Root, TEXT("grid"));
	const TSharedPtr<FJsonObject> Construction = ReadObject(Root, TEXT("construction"));
	const TSharedPtr<FJsonObject> Anchor = ReadObject(Root, TEXT("anchor"));
	const TSharedPtr<FJsonObject> Progression = ReadObject(Root, TEXT("age_progression"));
	const TSharedPtr<FJsonObject> Hall = ReadObject(Initial, TEXT("hall"));
	const TSharedPtr<FJsonObject> ExactVariant = ReadObject(Construction, TEXT("exact_visual_variant"));
	const TSharedPtr<FJsonObject> Bootstrap = ReadObject(Initial, TEXT("bootstrap_resources"));
	int32 HallType = 0, BuildingCount = 0, GridAge = 0, GridSize = 0, Normal = 0;
	int32 AnchorX = 0, AnchorY = 0, ExactCivilization = 0, ExactAge = 0;
	FString Tag, InitialPath, GridPath, ConstructionPath;
	if (!ReadInt(Variant, TEXT("civilization"), VisualCivilization) || VisualCivilization <= 0
		|| !ReadInt(Variant, TEXT("age"), InitialAge) || InitialAge <= 0
		|| !ReadExactString(Variant, TEXT("qualification"), TEXT("source_visual_variant_not_account_identity"))
		|| !ReadInt(Initial, TEXT("building_count"), BuildingCount) || BuildingCount != 26
		|| !Initial->TryGetStringField(TEXT("manifest_path"), InitialPath)
		|| !ReadInt(Hall, TEXT("init_id"), InitialHallId) || InitialHallId <= 0
		|| !ReadInt(Hall, TEXT("type"), HallType) || HallType != 1
		|| !ReadInt(Hall, TEXT("level"), InitialHallLevel) || InitialHallLevel <= 0
		|| !ReadInt(Hall, TEXT("grid_x"), InitialHallTile.X)
		|| !ReadInt(Hall, TEXT("grid_y"), InitialHallTile.Y)
		|| !Hall->TryGetStringField(TEXT("actor_identity_tag"), Tag)
		|| Tag != FString::Printf(TEXT("UnityInitId_%d"), InitialHallId)
		|| !Grid->TryGetStringField(TEXT("catalog_path"), GridPath)
		|| !ReadInt(Grid, TEXT("initial_age"), GridAge) || GridAge != InitialAge
		|| !ReadInt(Grid, TEXT("size"), GridSize) || GridSize <= 0
		|| !ReadInt(Grid, TEXT("normal_value"), Normal) || Normal != 5
		|| !Construction->TryGetStringField(TEXT("catalog_path"), ConstructionPath)
		|| !ReadInt(ExactVariant, TEXT("civilization"), ExactCivilization) || ExactCivilization != VisualCivilization
		|| !ReadInt(ExactVariant, TEXT("age"), ExactAge) || ExactAge != InitialAge
		|| !ReadExactString(Anchor, TEXT("status"), TEXT("pending_world_transform_evidence"))
		|| !ReadInt(Anchor, TEXT("grid_x"), AnchorX) || AnchorX != InitialHallTile.X
		|| !ReadInt(Anchor, TEXT("grid_y"), AnchorY) || AnchorY != InitialHallTile.Y
		|| !ReadExactString(Progression, TEXT("map_visual_conversion"), TEXT("pending")))
	{
		OutError = TEXT("Local city source variant, hall, or age contract is inconsistent");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Supported = nullptr;
	if (!Progression->TryGetArrayField(TEXT("supported_simulation_ages"), Supported)
		|| !Supported || Supported->Num() != 1 || !(*Supported)[0].IsValid()
		|| (*Supported)[0]->Type != EJson::Number
		|| (*Supported)[0]->AsNumber() != InitialAge)
	{
		OutError = TEXT("Only the verified initial city visual age may be simulated");
		return false;
	}
	SupportedAges.Add(InitialAge);
	static const TCHAR* RequiredSources[] = {
		TEXT("Saved/RokUnityMigration/unity_gameapp_city_init_building_resolution.json"),
		TEXT("Saved/RokUnityMigration/unity_initial_building_visual_resolution.json"),
		TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"),
		TEXT("Saved/RokUnityMigration/CityConstructionGridSources/catalog.json"),
		TEXT("Saved/RokUnityMigration/CityConstructionSources/catalog.json"),
		TEXT("Scripts/BuildRokLocalCitySimulationContext.py"),
		TEXT("ref/config-tables/编译客户端配置/CSV/InitBuilding.csv"),
		TEXT("ref/config-tables/编译客户端配置/CSV/BuildingModelData.csv"),
		TEXT("ref/config-tables/编译客户端配置/CSV/BuildingTypeConfig.csv"),
		TEXT("ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv"),
		TEXT("ref/config-tables/编译客户端配置/CSV/BuildingResourcesProduce.csv"),
		TEXT("ref/config-tables/编译客户端配置/CSV/CityAgeSize.csv")
	};
	const TSharedPtr<FJsonObject> Sources = ReadObject(Root, TEXT("sources"));
	if (!Sources.IsValid() || Sources->Values.Num() != UE_ARRAY_COUNT(RequiredSources)
		|| InitialPath != RequiredSources[2] || GridPath != RequiredSources[3]
		|| ConstructionPath != RequiredSources[4])
	{
		OutError = TEXT("Local city provenance paths are incomplete");
		return false;
	}
	// Only packaged data (the generated catalogs) is re-hashed; Scripts/ and ref/ do not ship.
	for (const TCHAR* Required : RequiredSources)
	{
		FString Expected;
		TArray<uint8> Bytes;
		if (!Sources->TryGetStringField(Required, Expected) || Expected.Len() != 64
			|| !IsSourcePath(Required)
			|| (RokDataPaths::IsPackagedDataPath(Required)
				&& (!FFileHelper::LoadFileToArray(Bytes, *(RokDataPaths::ResolveProjectPath(Required)))
					|| RokTrainArmyMesh::Sha256(Bytes) != Expected)))
		{
			OutError = FString::Printf(TEXT("Local city source hash changed: %s"), Required);
			return false;
		}
	}
	TSharedPtr<FJsonObject> Legacy, Visual, State, GridCatalog;
	if (!ReadJson(RequiredSources[0], Legacy) || !ReadJson(RequiredSources[1], Visual)
		|| !ReadJson(InitialPath, State) || !ReadJson(GridPath, GridCatalog))
	{
		OutError = TEXT("Local city source reports cannot be read");
		return false;
	}
	FString LegacyTarget, StateTarget;
	int32 LegacyCivilization = 0, LegacyAge = 0, VisualCivilizationValue = 0, VisualAge = 0;
	int32 StateCount = 0, VisualCount = 0;
	if (!Legacy->TryGetStringField(TEXT("target_level"), LegacyTarget) || LegacyTarget != TargetLevel
		|| !State->TryGetStringField(TEXT("target_level"), StateTarget) || StateTarget != TargetLevel
		|| !ReadInt(Legacy, TEXT("civ_type"), LegacyCivilization) || LegacyCivilization != VisualCivilization
		|| !ReadInt(Legacy, TEXT("age"), LegacyAge) || LegacyAge != InitialAge
		|| !ReadInt(Visual, TEXT("civ_type"), VisualCivilizationValue) || VisualCivilizationValue != VisualCivilization
		|| !ReadInt(Visual, TEXT("age"), VisualAge) || VisualAge != InitialAge
		|| !ReadInt(Visual, TEXT("building_count"), VisualCount) || VisualCount != BuildingCount
		|| !ReadInt(State, TEXT("building_count"), StateCount) || StateCount != BuildingCount)
	{
		OutError = TEXT("Local city source reports disagree about map or visual variant");
		return false;
	}
	const TSharedPtr<FJsonObject> StateBootstrap = ReadObject(State, TEXT("bootstrap_resources"));
	for (const TCHAR* Resource : {TEXT("food"), TEXT("wood"), TEXT("stone"), TEXT("gold")})
	{
		if (!SameInt(Bootstrap, StateBootstrap, Resource))
		{
			OutError = TEXT("Local city wallet does not match its bootstrap manifest");
			return false;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
	bool bHallFound = false;
	if (!State->TryGetArrayField(TEXT("buildings"), Buildings) || !Buildings || Buildings->Num() != BuildingCount)
	{
		OutError = TEXT("Local city initial building count is inconsistent");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Buildings)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		int32 Id = 0, Type = 0, Level = 0, X = 0, Y = 0;
		if (!ReadInt(Row, TEXT("init_id"), Id) || Id != InitialHallId) continue;
		const TSharedPtr<FJsonObject> Tile = ReadObject(Row, TEXT("grid"));
		bHallFound = ReadInt(Row, TEXT("type_id"), Type) && Type == HallType
			&& ReadInt(Row, TEXT("level"), Level) && Level == InitialHallLevel
			&& ReadInt(Tile, TEXT("x"), X) && X == InitialHallTile.X
			&& ReadInt(Tile, TEXT("y"), Y) && Y == InitialHallTile.Y;
		break;
	}
	const TArray<TSharedPtr<FJsonValue>>* Ages = nullptr;
	if (!bHallFound || !GridCatalog->TryGetArrayField(TEXT("ages"), Ages) || !Ages || Ages->IsEmpty())
	{
		OutError = TEXT("Local city hall or source age map is missing");
		return false;
	}
	const TSharedPtr<FJsonObject> FirstAge = (*Ages)[0].IsValid() ? (*Ages)[0]->AsObject() : nullptr;
	int32 Age = 0, Size = 0;
	if (!ReadInt(FirstAge, TEXT("age"), Age) || Age != InitialAge
		|| !ReadInt(FirstAge, TEXT("size"), Size) || Size != GridSize)
	{
		OutError = TEXT("Local city initial grid does not match its source map");
		return false;
	}
	bLoaded = true;
	return true;
}

bool FRokLocalCitySimulationContext::ValidateWorld(const UWorld* World, FString& OutError) const
{
	OutError.Reset();
	if (!bLoaded || !World)
	{
		OutError = TEXT("Local city context has not been verified");
		return false;
	}
	const FString PackageName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (PackageName != TargetLevel)
	{
		OutError = FString::Printf(TEXT("Local city map mismatch: %s"), *PackageName);
		return false;
	}
	for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (HasTags(*It, InitialHallId, InitialHallTile)) return true;
	}
	OutError = TEXT("Local city initial hall actor identity was not found");
	return false;
}

bool FRokLocalProductionLedger::RegisterCompleted(const FRokLocalConstructionTicket& Ticket,
	const FRokLocalProductionSource& Source, int64 CompletionSeconds, FString& OutError)
{
	OutError.Reset();
	if (Ticket.Id == 0 || !Ticket.bCompleted || (Ticket.TypeId != 2 && Ticket.TypeId != 3)
		|| Source.TypeId != Ticket.TypeId || Source.Level != 1
		|| Source.ProducePerHour <= 0 || Source.GatherMin <= 0 || Source.GatherMax < Source.GatherMin
		|| !FMath::IsFinite(Ticket.FinishesAt) || Ticket.FinishesAt < 0.0
		|| CompletionSeconds < 0 || double(CompletionSeconds) < Ticket.FinishesAt
		|| Clocks.Contains(Ticket.Id))
	{
		OutError = TEXT("Completed local resource building or source production rate is invalid");
		return false;
	}
	FClock Clock;
	Clock.Source = Source;
	Clock.ChangeTimeSeconds = CompletionSeconds;
	Clocks.Add(Ticket.Id, Clock);
	return true;
}

bool FRokLocalProductionLedger::GetSource(uint64 LocalConstructionId,
	FRokLocalProductionSource& OutSource, FString& OutError) const
{
	const FClock* Clock = Clocks.Find(LocalConstructionId);
	if (!Clock)
	{
		OutError = TEXT("Local resource building has no completed production clock");
		return false;
	}
	OutSource = Clock->Source;
	OutError.Reset();
	return true;
}

bool FRokLocalProductionLedger::TransitionLevel(uint64 LocalConstructionId,
	const FRokLocalProductionSource& ExpectedCurrent, const FRokLocalProductionSource& Next,
	FString& OutError)
{
	FClock* Clock = Clocks.Find(LocalConstructionId);
	if (!Clock || (Clock->Source.TypeId != 2 && Clock->Source.TypeId != 3)
		|| Clock->Source.TypeId != ExpectedCurrent.TypeId
		|| Clock->Source.Level != ExpectedCurrent.Level
		|| Clock->Source.ProducePerHour != ExpectedCurrent.ProducePerHour
		|| Clock->Source.GatherMin != ExpectedCurrent.GatherMin
		|| Clock->Source.GatherMax != ExpectedCurrent.GatherMax
		|| ExpectedCurrent.Level != 1 || Next.Level != 2
		|| Next.TypeId != ExpectedCurrent.TypeId || Next.ProducePerHour <= 0
		|| Next.GatherMin <= 0 || Next.GatherMax < Next.GatherMin)
	{
		OutError = TEXT("Local production level transition does not match its completed source clock");
		return false;
	}
	// A single source-row replacement is the entire ledger commit. The private
	// StoredNum/ChangeTimeSeconds remain byte-for-byte unchanged until collection.
	Clock->Source = Next;
	OutError.Reset();
	return true;
}

bool FRokLocalProductionLedger::Preview(uint64 LocalConstructionId, int64 NowSeconds,
	FRokCityResourceProductionResult& OutResult, FString& OutError) const
{
	OutResult = {};
	const FClock* Clock = Clocks.Find(LocalConstructionId);
	if (!Clock)
	{
		OutError = TEXT("Local resource building has no completed production clock");
		return false;
	}
	FRokCityResourceProductionInput Input;
	Input.bCompleted = true;
	Input.ProducePerHour = Clock->Source.ProducePerHour;
	Input.GatherMin = Clock->Source.GatherMin;
	Input.GatherMax = Clock->Source.GatherMax;
	Input.StoredNum = Clock->StoredNum;
	Input.ChangeTimeSeconds = Clock->ChangeTimeSeconds;
	Input.NowSeconds = NowSeconds;
	return FRokCityResourceProduction::Evaluate(Input, OutResult, OutError);
}

bool FRokLocalProductionLedger::TryCollect(uint64 LocalConstructionId, int64 NowSeconds,
	FRokLocalConstructionResources& InOutResources, int64& OutCollected, FString& OutError)
{
	OutCollected = 0;
	FClock* Clock = Clocks.Find(LocalConstructionId);
	if (!Clock)
	{
		OutError = TEXT("Local resource building has no completed production clock");
		return false;
	}
	FRokCityResourceProductionInput Input;
	Input.bCompleted = true;
	Input.ProducePerHour = Clock->Source.ProducePerHour;
	Input.GatherMin = Clock->Source.GatherMin;
	Input.GatherMax = Clock->Source.GatherMax;
	Input.StoredNum = Clock->StoredNum;
	Input.ChangeTimeSeconds = Clock->ChangeTimeSeconds;
	Input.NowSeconds = NowSeconds;
	FRokCityResourceProductionResult Result;
	FRokCityResourceProductionInput Next;
	if (!FRokCityResourceProduction::Collect(Input, Result, Next, OutError)) return false;
	int32* Balance = Clock->Source.TypeId == 2 ? &InOutResources.Food : &InOutResources.Wood;
	if (*Balance < 0 || Result.Accrued <= 0 || int64(*Balance) + Result.Accrued > MAX_int32)
	{
		OutError = TEXT("Local resource wallet cannot receive the collection amount");
		return false;
	}
	*Balance = static_cast<int32>(int64(*Balance) + Result.Accrued);
	Clock->StoredNum = Next.StoredNum;
	Clock->ChangeTimeSeconds = Next.ChangeTimeSeconds;
	OutCollected = Result.Accrued;
	OutError.Reset();
	return true;
}
