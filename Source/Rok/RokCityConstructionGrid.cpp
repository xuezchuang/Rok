#include "RokCityConstructionGrid.h"

#include "RokDataPaths.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& Out)
{
	double Value = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value)
		|| Value < MIN_int32 || Value > MAX_int32 || FMath::FloorToDouble(Value) != Value)
	{
		return false;
	}
	Out = static_cast<int32>(Value);
	return true;
}

bool ReadIntegers(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32 Count, TArray<int32>& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || !Values || Values->Num() != Count)
	{
		return false;
	}
	Out.Reset(Count);
	for (const TSharedPtr<FJsonValue>& Entry : *Values)
	{
		if (!Entry.IsValid() || Entry->Type != EJson::Number) return false;
		const double Value = Entry->AsNumber();
		if (!FMath::IsFinite(Value) || Value < MIN_int32 || Value > MAX_int32
			|| FMath::FloorToDouble(Value) != Value) return false;
		Out.Add(static_cast<int32>(Value));
	}
	return true;
}

bool HasNonnegativeCost(const FRokLocalConstructionResources& Cost)
{
	return Cost.Food >= 0 && Cost.Wood >= 0 && Cost.Stone >= 0 && Cost.Gold >= 0;
}
}

int32 FRokCityConstructionGrid::SourceCellIndex(int32 Size, int32 X, int32 Y)
{
	if (Size <= 0 || !(Size & 1)) return INDEX_NONE;
	const int32 Half = (Size - 1) / 2;
	if (X < -Half || X > Half || Y < -Half || Y > Half) return INDEX_NONE;
	const int64 Index = int64(Y + Half) * Size + (X + Half + 1);
	return Index >= 0 && Index < int64(Size) * Size ? static_cast<int32>(Index) : INDEX_NONE;
}

bool FRokCityConstructionGrid::Load(FString& OutError)
{
	Ages.Reset();
	InitialOccupancy.Reset();
	bool bLoaded = false;
	ON_SCOPE_EXIT
	{
		if (!bLoaded) { Ages.Reset(); InitialOccupancy.Reset(); }
	};
	const FString Path = RokDataPaths::Resolve(TEXT("CityConstructionGridSources/catalog.json"));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = TEXT("Source city grid catalog is missing");
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = TEXT("Source city grid catalog is malformed");
		return false;
	}
	FString Schema;
	int32 Normal = 0, Reserved = 0, TypeOffset = 0;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.city_construction_grid_sources.v1")
		|| !ReadInteger(Root, TEXT("normal_value"), Normal) || Normal != 5
		|| !ReadInteger(Root, TEXT("reserved_offset"), Reserved) || Reserved != 7
		|| !ReadInteger(Root, TEXT("building_type_offset"), TypeOffset) || TypeOffset != 100)
	{
		OutError = TEXT("Source city grid schema/constants mismatch");
		return false;
	}
	const TSharedPtr<FJsonObject>* Formula = nullptr;
	int32 XOffset = 0;
	if (!Root->TryGetObjectField(TEXT("grid_index"), Formula) || !Formula || !ReadInteger(*Formula, TEXT("x_offset"), XOffset)
		|| XOffset != 1)
	{
		OutError = TEXT("Source city grid index offset mismatch");
		return false;
	}
	const TSharedPtr<FJsonObject>* Sources = nullptr;
	if (!Root->TryGetObjectField(TEXT("sources"), Sources) || !Sources || (*Sources)->Values.Num() != 6)
	{
		OutError = TEXT("Source city grid provenance is incomplete");
		return false;
	}
	// The recorded ref/ inputs do not ship, so they are checked by the generator and
	// VerifyRokCityConstructionGridSources.py, not re-hashed here.
	for (const auto& Source : (*Sources)->Values)
	{
		const FString SourcePath(*Source.Key);
		FString Expected;
		if (!SourcePath.StartsWith(TEXT("ref/")) || SourcePath.Contains(TEXT("..")) || !Source.Value.IsValid()
			|| !Source.Value->TryGetString(Expected) || Expected.Len() != 64)
		{
			OutError = FString::Printf(TEXT("Source city grid provenance changed: %s"), *SourcePath);
			return false;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* AgeRows = nullptr;
	if (!Root->TryGetArrayField(TEXT("ages"), AgeRows) || !AgeRows || AgeRows->Num() != 5)
	{
		OutError = TEXT("Source city grid requires five age maps");
		return false;
	}
	static constexpr int32 ExpectedSizes[] = {41, 45, 49, 53, 57};
	static constexpr int32 ExpectedLevels[] = {1, 4, 10, 16, 21};
	for (int32 Row = 0; Row < AgeRows->Num(); ++Row)
	{
		const TSharedPtr<FJsonObject> Object = (*AgeRows)[Row].IsValid() ? (*AgeRows)[Row]->AsObject() : nullptr;
		FRokCityConstructionAge Age;
		int32 BuildableCount = 0;
		if (!ReadInteger(Object, TEXT("age"), Age.Age) || Age.Age != Row + 1
			|| !ReadInteger(Object, TEXT("town_hall_level"), Age.TownHallLevel) || Age.TownHallLevel != ExpectedLevels[Row]
			|| !ReadInteger(Object, TEXT("size"), Age.Size) || Age.Size != ExpectedSizes[Row]
			|| !ReadInteger(Object, TEXT("buildable_cell_count"), BuildableCount))
		{
			OutError = TEXT("Source city grid age identity mismatch");
			return false;
		}
		TArray<int32> Raw, Empty;
		if (!ReadIntegers(Object, TEXT("raw_cells"), Age.Size * Age.Size, Raw)
			|| !ReadIntegers(Object, TEXT("empty_cells"), Age.Size * Age.Size, Empty))
		{
			OutError = TEXT("Source city grid cell array mismatch");
			return false;
		}
		int32 CountedBuildable = 0;
		for (int32 Index = 0; Index < Raw.Num(); ++Index)
		{
			int32 Normalized = Raw[Index] > 100 ? Raw[Index] - 100 + 7 : Raw[Index];
			Normalized = Normalized > 7 ? 5 : Normalized;
			if (Empty[Index] != Normalized || Empty[Index] < 0 || Empty[Index] > 255)
			{
				OutError = TEXT("Source city grid normalization mismatch");
				return false;
			}
			Age.EmptyCells.Add(static_cast<uint8>(Empty[Index]));
			CountedBuildable += Empty[Index] == 5 ? 1 : 0;
		}
		if (CountedBuildable != BuildableCount)
		{
			OutError = TEXT("Source city grid buildable count mismatch");
			return false;
		}
		Ages.Add(Age.Age, MoveTemp(Age));
	}
	const TArray<TSharedPtr<FJsonValue>>* InitialRows = nullptr;
	if (!Root->TryGetArrayField(TEXT("initial_occupancy"), InitialRows) || !InitialRows || InitialRows->Num() != 26)
	{
		OutError = TEXT("Source city grid initial occupancy count mismatch");
		return false;
	}
	TSet<int32> SeenIds;
	for (const TSharedPtr<FJsonValue>& Value : *InitialRows)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FRokCityConstructionInitialOccupant Occupant;
		if (!ReadInteger(Object, TEXT("init_id"), Occupant.InitId) || Occupant.InitId <= 0 || SeenIds.Contains(Occupant.InitId)
			|| !ReadInteger(Object, TEXT("type"), Occupant.TypeId) || Occupant.TypeId <= 0
			|| !ReadInteger(Object, TEXT("level"), Occupant.Level) || Occupant.Level <= 0
			|| !ReadInteger(Object, TEXT("pos_x"), Occupant.Footprint.Origin.X)
			|| !ReadInteger(Object, TEXT("pos_y"), Occupant.Footprint.Origin.Y)
			|| !ReadInteger(Object, TEXT("width"), Occupant.Footprint.Size.X) || Occupant.Footprint.Size.X < 0
			|| !ReadInteger(Object, TEXT("length"), Occupant.Footprint.Size.Y) || Occupant.Footprint.Size.Y < 0)
		{
			OutError = TEXT("Source city grid initial building row mismatch");
			return false;
		}
		if ((Occupant.Footprint.Size.X == 0) != (Occupant.Footprint.Size.Y == 0)
			|| Occupant.Footprint.Size.X > 41 || Occupant.Footprint.Size.Y > 41
			|| (Occupant.Footprint.IsValid()
				&& (Occupant.Footprint.Origin.X < -20 || Occupant.Footprint.Origin.Y < -20
					|| int64(Occupant.Footprint.Origin.X) + Occupant.Footprint.Size.X > 21
					|| int64(Occupant.Footprint.Origin.Y) + Occupant.Footprint.Size.Y > 21)))
		{
			OutError = TEXT("Source city grid initial building extent is invalid");
			return false;
		}
		SeenIds.Add(Occupant.InitId);
		TArray<int32> ActualIndices;
		if (Occupant.Footprint.Size.X > 0 && Occupant.Footprint.Size.Y > 0)
		{
			for (int32 X = Occupant.Footprint.Origin.X; X < Occupant.Footprint.Origin.X + Occupant.Footprint.Size.X; ++X)
			{
				for (int32 Y = Occupant.Footprint.Origin.Y; Y < Occupant.Footprint.Origin.Y + Occupant.Footprint.Size.Y; ++Y)
				{
					const int32 Index = SourceCellIndex(41, X, Y);
					if (Index == INDEX_NONE || Ages[1].EmptyCells[Index] != 5)
					{
						OutError = TEXT("Source city grid initial building lies outside legal source cells");
						return false;
					}
					ActualIndices.Add(Index);
				}
			}
		}
		TArray<int32> SourceIndices;
		if (!ReadIntegers(Object, TEXT("cell_indices"), ActualIndices.Num(), SourceIndices)
			|| SourceIndices != ActualIndices)
		{
			OutError = TEXT("Source city grid initial cell indices mismatch");
			return false;
		}
		InitialOccupancy.Add(Occupant);
	}
	const TArray<TSharedPtr<FJsonValue>>* Overlaps = nullptr;
	if (!Root->TryGetArrayField(TEXT("overlaps"), Overlaps) || !Overlaps || !Overlaps->IsEmpty())
	{
		OutError = TEXT("Source city grid has unresolved initial occupancy overlaps");
		return false;
	}
	for (const auto& AgePair : Ages)
	{
		TSet<int32> Occupied;
		for (const FRokCityConstructionInitialOccupant& Initial : InitialOccupancy)
		{
			if (!Initial.Footprint.IsValid()) continue;
			for (int32 X = Initial.Footprint.Origin.X; X < Initial.Footprint.Origin.X + Initial.Footprint.Size.X; ++X)
			{
				for (int32 Y = Initial.Footprint.Origin.Y; Y < Initial.Footprint.Origin.Y + Initial.Footprint.Size.Y; ++Y)
				{
					const int32 Index = SourceCellIndex(AgePair.Value.Size, X, Y);
					if (Index == INDEX_NONE || AgePair.Value.EmptyCells[Index] != 5 || Occupied.Contains(Index))
					{
						OutError = TEXT("Source city grid initial occupancy is invalid for an age");
						return false;
					}
					Occupied.Add(Index);
				}
			}
		}
	}
	bLoaded = true;
	OutError.Reset();
	return true;
}

int32 FRokCityConstructionGrid::GetAgeForTownHallLevel(int32 TownHallLevel) const
{
	int32 Result = 0;
	for (const auto& Pair : Ages)
	{
		if (TownHallLevel >= Pair.Value.TownHallLevel) Result = FMath::Max(Result, Pair.Key);
	}
	return Result;
}

bool FRokCityConstructionGrid::CheckFootprint(int32 Age, const FRokCityConstructionFootprint& Footprint,
	TConstArrayView<FRokCityConstructionFootprint> LocalFootprints, FString& OutError,
	TArray<int32>* OutCellIndices) const
{
	if (OutCellIndices) OutCellIndices->Reset();
	const FRokCityConstructionAge* GridAge = Ages.Find(Age);
	if (!GridAge || !Footprint.IsValid())
	{
		OutError = TEXT("城市年龄或建筑占地配置无效。");
		return false;
	}
	const int64 Half = (GridAge->Size - 1) / 2;
	const int64 EndX = int64(Footprint.Origin.X) + Footprint.Size.X;
	const int64 EndY = int64(Footprint.Origin.Y) + Footprint.Size.Y;
	if (Footprint.Size.X > GridAge->Size || Footprint.Size.Y > GridAge->Size
		|| Footprint.Origin.X < -Half || Footprint.Origin.Y < -Half
		|| EndX > Half + 1 || EndY > Half + 1)
	{
		OutError = TEXT("位置超出城市源格图范围。");
		return false;
	}
	for (const FRokCityConstructionInitialOccupant& Initial : InitialOccupancy)
	{
		if (Initial.Footprint.IsValid() && Footprint.Overlaps(Initial.Footprint))
		{
			OutError = TEXT("位置已被初始建筑占用。");
			return false;
		}
	}
	for (const FRokCityConstructionFootprint& Local : LocalFootprints)
	{
		if (Local.IsValid() && Footprint.Overlaps(Local))
		{
			OutError = TEXT("位置已被本地建筑或在建预约占用。");
			return false;
		}
	}
	TArray<int32> CheckedIndices;
	for (int32 X = Footprint.Origin.X; X < Footprint.Origin.X + Footprint.Size.X; ++X)
	{
		for (int32 Y = Footprint.Origin.Y; Y < Footprint.Origin.Y + Footprint.Size.Y; ++Y)
		{
			const int32 Index = SourceCellIndex(GridAge->Size, X, Y);
			if (Index == INDEX_NONE)
			{
				OutError = TEXT("位置超出城市源格图范围。");
				return false;
			}
			if (GridAge->EmptyCells[Index] != 5)
			{
				OutError = TEXT("位置包含不可建造的源格子。");
				return false;
			}
			CheckedIndices.Add(Index);
		}
	}
	if (OutCellIndices) *OutCellIndices = MoveTemp(CheckedIndices);
	OutError.Reset();
	return true;
}

bool FRokLocalConstructionLedger::TryBegin(const FRokCityConstructionGrid& Grid,
	FRokLocalConstructionTicket Proposed, int32 ActiveUpgrades, int32 OwnedQueues,
	int32 ExistingBuildingCount, int32 BuildingCountLimit,
	FRokLocalConstructionResources& InOutResources, uint64& OutId, FString& OutError)
{
	OutId = 0;
	if (!Grid.IsLoaded() || Proposed.Id != 0 || Proposed.bCompleted || Proposed.TypeId <= 0
		|| Proposed.Civilization <= 0 || Proposed.Age <= 0 || !Proposed.Footprint.IsValid()
		|| !FMath::IsFinite(Proposed.StartedAt) || !FMath::IsFinite(Proposed.FinishesAt)
		|| Proposed.StartedAt < 0.0 || Proposed.FinishesAt <= Proposed.StartedAt
		|| Proposed.Power < 0 || !HasNonnegativeCost(Proposed.Cost))
	{
		OutError = TEXT("本地建造命令不完整。");
		return false;
	}
	if (ActiveUpgrades < 0 || OwnedQueues <= 0 || int64(ActiveUpgrades) + GetActiveCount() >= OwnedQueues)
	{
		OutError = TEXT("建造队列已满。");
		return false;
	}
	int32 LocalBuildingCount = 0;
	for (const FRokLocalConstructionTicket& Ticket : Tickets) LocalBuildingCount += Ticket.TypeId == Proposed.TypeId ? 1 : 0;
	if (ExistingBuildingCount < 0 || BuildingCountLimit <= 0
		|| int64(ExistingBuildingCount) + LocalBuildingCount >= BuildingCountLimit)
	{
		OutError = TEXT("当前市政厅等级的建筑数量上限已达到。");
		return false;
	}
	if (!InOutResources.CanAfford(Proposed.Cost))
	{
		OutError = TEXT("建造资源不足。");
		return false;
	}
	TArray<FRokCityConstructionFootprint> LocalFootprints;
	for (const FRokLocalConstructionTicket& Existing : Tickets) LocalFootprints.Add(Existing.Footprint);
	if (!Grid.CheckFootprint(Proposed.Age, Proposed.Footprint, LocalFootprints, OutError)) return false;
	if (NextId == MAX_uint64)
	{
		OutError = TEXT("本地建造编号已耗尽。");
		return false;
	}
	Proposed.Id = NextId++;
	InOutResources.Food -= Proposed.Cost.Food;
	InOutResources.Wood -= Proposed.Cost.Wood;
	InOutResources.Stone -= Proposed.Cost.Stone;
	InOutResources.Gold -= Proposed.Cost.Gold;
	Tickets.Add(Proposed);
	OutId = Proposed.Id;
	OutError.Reset();
	return true;
}

void FRokLocalConstructionLedger::CompleteDue(double Now, TArray<FRokLocalConstructionTicket>& OutCompleted)
{
	OutCompleted.Reset();
	if (!FMath::IsFinite(Now)) return;
	for (FRokLocalConstructionTicket& Ticket : Tickets)
	{
		if (!Ticket.bCompleted && Now >= Ticket.FinishesAt)
		{
			Ticket.bCompleted = true;
			OutCompleted.Add(Ticket);
		}
	}
}

int32 FRokLocalConstructionLedger::GetActiveCount() const
{
	int32 Count = 0;
	for (const FRokLocalConstructionTicket& Ticket : Tickets) Count += Ticket.bCompleted ? 0 : 1;
	return Count;
}

bool FRokLocalConstructionLedger::Find(uint64 Id, FRokLocalConstructionTicket& OutTicket) const
{
	for (const FRokLocalConstructionTicket& Ticket : Tickets)
	{
		if (Ticket.Id == Id) { OutTicket = Ticket; return true; }
	}
	return false;
}
