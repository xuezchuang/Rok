#include "RokServerArmySubsystem.h"

#include "RokDataPaths.h"
#include "RokGameNetSubsystem.h"
#include "RokInstantPrice.h"
#include "RokUnityConfigSubsystem.h"
#include "RokServerHeroSubsystem.h"
#include "RokSproto.h"
#include "RokTrainArmyDetail.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64& Out)
{
	return Object.IsValid() && RokSprotoJson::TryGetInt64(Object, Field, Out);
}

TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Value = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(Field, Value) ? *Value : nullptr;
}

bool HasRows(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field)
{
	return Parent.IsValid() && (Parent->HasTypedField<EJson::Array>(Field)
		|| Parent->HasTypedField<EJson::Object>(Field));
}

TArray<TSharedPtr<FJsonObject>> Rows(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field)
{
	TArray<TSharedPtr<FJsonObject>> Result;
	if (!Parent.IsValid()) return Result;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Parent->TryGetArrayField(Field, Array))
	{
		for (const auto& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Row) && Row) Result.Add(*Row);
		}
	}
	else if (const TSharedPtr<FJsonObject> Object = Child(Parent, Field))
	{
		for (const auto& Pair : Object->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if (Pair.Value.IsValid() && Pair.Value->TryGetObject(Row) && Row) Result.Add(*Row);
		}
	}
	return Result;
}

FString TroopName(int32 Id)
{
	static TMap<int32, FString> Names;
	if (const FString* Found = Names.Find(Id)) return *Found;
	FRokTrainArmyDetail Detail;
	FString Error;
	const FString Name = RokTrainArmyDetail::Load(Id, Detail, Error) && !Detail.Name.IsEmpty()
		? Detail.Name : FString::Printf(TEXT("兵种 %d"), Id);
	Names.Add(Id, Name);
	return Name;
}

FRokServerSoldierStack Soldier(const TSharedPtr<FJsonObject>& Object,
	const URokUnityGameplayCatalogSubsystem* Catalog)
{
	FRokServerSoldierStack Result;
	int64 Id = 0, Type = 0, Level = 0, Num = 0, Minor = 0;
	if (!ReadInt(Object, TEXT("id"), Id) || Id <= 0
		|| !ReadInt(Object, TEXT("num"), Num) || Num <= 0) return Result;
	ReadInt(Object, TEXT("type"), Type); ReadInt(Object, TEXT("level"), Level);
	ReadInt(Object, TEXT("minor"), Minor);
	Result.Id = int32(Id); Result.FamilyType = int32(Type); Result.Tier = int32(Level);
	Result.Count = Num; Result.Minor = FMath::Max<int64>(0, Minor);
	FRokUnityCatalogTroop Troop;
	if (Catalog && Catalog->GetTroop(Result.Id, Troop))
	{
		Result.bCatalogKnown = true;
		if (Result.FamilyType <= 0) Result.FamilyType = Troop.ArmsType;
		if (Result.Tier <= 0) Result.Tier = Troop.Level;
		Result.PowerPerUnit = Troop.Power;
		Result.TotalPower = Result.Count * Result.PowerPerUnit;
		Result.Icon = Troop.Icon;
	}
	Result.Name = TroopName(Result.Id);
	return Result;
}

void SortSoldiers(TArray<FRokServerSoldierStack>& RowsIn)
{
	RowsIn.Sort([](const FRokServerSoldierStack& A, const FRokServerSoldierStack& B)
	{
		if (A.FamilyType != B.FamilyType) return A.FamilyType < B.FamilyType;
		if (A.Tier != B.Tier) return A.Tier > B.Tier;
		return A.Id < B.Id;
	});
}

void AggregateSoldiers(TArray<FRokServerSoldierStack>& RowsIn)
{
	TMap<int32, FRokServerSoldierStack> ById;
	for (const FRokServerSoldierStack& Row : RowsIn)
	{
		if (FRokServerSoldierStack* Existing = ById.Find(Row.Id))
		{
			Existing->Count += Row.Count;
			Existing->Minor += Row.Minor;
			Existing->TotalPower += Row.TotalPower;
			Existing->bCatalogKnown &= Row.bCatalogKnown;
		}
		else ById.Add(Row.Id, Row);
	}
	ById.GenerateValueArray(RowsIn);
	SortSoldiers(RowsIn);
}

ERokServerMarchPhase ResolveMarchPhase(int64 Mask)
{
	if (Mask & ((int64(1) << 5) | (int64(1) << 13) | (int64(1) << 15))) return ERokServerMarchPhase::Returning;
	if (Mask & (int64(1) << 10)) return ERokServerMarchPhase::Garrisoned;
	if (Mask & ((int64(1) << 7) | (int64(1) << 24))) return ERokServerMarchPhase::Collecting;
	if (Mask & ((int64(1) << 8) | (int64(1) << 27))) return ERokServerMarchPhase::Battling;
	// The server can set STATIONING up front to request staying after the attack.
	if (Mask & (int64(1) << 1)) return ERokServerMarchPhase::Marching;
	if (Mask & ((int64(1) << 9) | (int64(1) << 14) | (int64(1) << 17))) return ERokServerMarchPhase::Stationed;
	if (Mask & (int64(1) << 22)) return ERokServerMarchPhase::Rallying;
	if (Mask & ((int64(1) << 0) | (int64(1) << 2)
		| (int64(1) << 3) | (int64(1) << 4) | (int64(1) << 19)
		| (int64(1) << 20) | (int64(1) << 23))) return ERokServerMarchPhase::Marching;
	return ERokServerMarchPhase::Unknown;
}

int32 DefaultTroopId(int32 Family, int32 Tier)
{
	return Family > 0 && Tier > 0 ? 10000 + Family * 100 + Tier : 0;
}

int32 ResolvedTroopId(int32 Country, int32 Family, int32 Tier,
	const TMap<int32, TArray<int32>>& FeatureArmsByCountry,
	const URokUnityGameplayCatalogSubsystem* Catalog)
{
	const TArray<int32>* Special = FeatureArmsByCountry.Find(Country);
	if (!Special || !Catalog) return 0; // The civilization can replace common units.
	for (const int32 Id : *Special)
	{
		FRokUnityCatalogTroop Troop;
		if (Catalog->GetTroop(Id, Troop) && Troop.ArmsType == Family && Troop.Level == Tier)
			return Id;
	}
	const int32 Default = DefaultTroopId(Family, Tier);
	FRokUnityCatalogTroop Troop;
	return Catalog->GetTroop(Default, Troop) ? Default : 0;
}

void AppendSoldierSignature(FString& Text, const FRokServerSoldierStack& Row)
{
	Text += FString::Printf(TEXT("|S%d,%d,%d,%lld,%lld,%d,%lld,%d;%s;%s"), Row.Id,
		Row.FamilyType, Row.Tier, Row.Count, Row.Minor, Row.PowerPerUnit,
		Row.TotalPower, Row.bCatalogKnown, *Row.Name, *Row.Icon);
}
}

void URokServerArmySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString ResearchError;
	TrainingResearch.Load(ResearchError);
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const FString Path = RokDataPaths::Resolve(TEXT("ConfigTables/BuildingTownCenter.json"));
	const TArray<TSharedPtr<FJsonValue>>* RowsIn = nullptr;
	if (FFileHelper::LoadFileToString(Json, *Path)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		&& Root.IsValid() && Root->TryGetArrayField(TEXT("rows"), RowsIn) && RowsIn)
	{
		for (const auto& Value : *RowsIn)
		{
			const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
			int64 Level = 0, Slots = 0;
			if (ReadInt(Row, TEXT("level"), Level) && Level > 0
				&& ReadInt(Row, TEXT("troopsDispatchNumber"), Slots) && Slots >= 0)
				DispatchSlotsByTownHallLevel.Add(int32(Level), int32(Slots));
		}
	}
	Json.Reset(); Root.Reset(); RowsIn = nullptr;
	const FString CivilizationPath = RokDataPaths::Resolve(TEXT("ConfigTables/Civilization.json"));
	if (FFileHelper::LoadFileToString(Json, *CivilizationPath)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		&& Root.IsValid() && Root->TryGetArrayField(TEXT("rows"), RowsIn) && RowsIn)
	{
		for (const auto& Value : *RowsIn)
		{
			const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
			int64 Country = 0;
			FString Ids;
			if (!ReadInt(Row, TEXT("ID"), Country) || Country <= 0
				|| !Row->TryGetStringField(TEXT("featureArms"), Ids)) continue;
			TArray<FString> Tokens;
			Ids.ParseIntoArray(Tokens, TEXT("|"), true);
			TArray<int32>& Arms = FeatureArmsByCountry.FindOrAdd(int32(Country));
			for (const FString& Token : Tokens)
			{
				int32 Id = 0;
				if (LexTryParseString(Id, *Token) && Id > 0) Arms.Add(Id);
			}
		}
	}
}

bool URokServerArmySubsystem::GetTrainingTroop(int32 ArmyType, int32 Tier, FRokUnityCatalogTroop& Out) const
{
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto* Catalog = Instance ? Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	int64 Country = 0;
	if (!Catalog || !Net || Net->GetState() != ERokGameNetState::RoleReady
		|| !ReadInt(Net->GetRoleInfo(), TEXT("country"), Country)) return false;
	const int32 TroopId = ResolvedTroopId(static_cast<int32>(Country), ArmyType, Tier, FeatureArmsByCountry, Catalog);
	return TroopId > 0 && Catalog->GetTroop(TroopId, Out);
}

bool URokServerArmySubsystem::IsTrainingTierUnlocked(int32 StudyId) const
{
	if (StudyId == 0) return true;
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	FRokResearchRow Study;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady || !TrainingResearch.GetByRowId(StudyId, Study)) return false;
	for (const auto& Row : Rows(Net->GetRoleInfo(), TEXT("technologies")))
	{
		int64 Type = 0, Level = 0;
		if (ReadInt(Row, TEXT("technologyType"), Type) && Type == Study.StudyType)
			return ReadInt(Row, TEXT("level"), Level) && Level >= Study.Level;
	}
	return false;
}

int32 URokServerArmySubsystem::GetTrainingCapacityBonus() const
{
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto Role = Net && Net->GetState() == ERokGameNetState::RoleReady ? Net->GetRoleInfo() : nullptr;
	int64 Bonus = 0, Uses = 0;
	if (ReadInt(Role, TEXT("itemAddTroopsCapacityCount"), Uses) && Uses > 0
		&& ReadInt(Role, TEXT("itemAddTroopsCapacity"), Bonus))
		return static_cast<int32>(FMath::Clamp<int64>(Bonus, 0, MAX_int32));
	return 0;
}

int64 URokServerArmySubsystem::GetTrainingSeconds(const FRokUnityCatalogTroop& Troop, int32 Count) const
{
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	int64 PerMille = 0;
	// The existing RoleInfo schema normally omits this derived server attribute.
	if (Net && Net->GetState() == ERokGameNetState::RoleReady)
		ReadInt(Net->GetRoleInfo(), TEXT("trainSpeedMulti"), PerMille);
	const int64 Base = int64(Troop.TrainSeconds) * Count;
	return FMath::FloorToInt64(Base * (1.0 - FMath::Clamp<int64>(PerMille, 0, 1000) / 1000.0));
}

bool URokServerArmySubsystem::GetInstantTrainingPrice(const FRokUnityCatalogTroop& Troop, int32 Count, int64& Gems) const
{
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	const auto Role = Net && Net->GetState() == ERokGameNetState::RoleReady ? Net->GetRoleInfo() : nullptr;
	if (!Role || !Config || Count <= 0) return false;
	const auto* Prices = Config->GetTableRows(TEXT("InstantPrice"));
	if (!Prices) return false;

	const TCHAR* Fields[] = {TEXT("food"), TEXT("wood"), TEXT("stone"), TEXT("gold")};
	const int32 Costs[] = {Troop.TrainCost.Food, Troop.TrainCost.Wood, Troop.TrainCost.Stone, Troop.TrainCost.Gold};
	int64 Deficits[5] = {GetTrainingSeconds(Troop, Count)};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Fields); ++Index)
	{
		if (Costs[Index] <= 0) continue;
		int64 Available = 0;
		if (!ReadInt(Role, Fields[Index], Available) || Available < 0) return false;
		Deficits[Index + 1] = FMath::Max<int64>(0, int64(Costs[Index]) * Count - Available);
	}
	double Total = 0.0;
	if (!RokInstantPrice::AddQuote(*Prices, Deficits, Total)) return false;
	Gems = FMath::FloorToInt64(Total);
	return true;
}

bool URokServerArmySubsystem::RequestTraining(int32 BuildingIndex, int32 ArmyType, int32 Tier, int32 Count,
	TFunction<void(const FString&)> OnResult, FString& Error, TOptional<int64> ConfirmedGems)
{
	auto* Instance = GetGameInstance();
	auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net) { Error = TEXT("服务器连接尚未就绪。"); return false; }
	if (!Net->CanRequestTrainingWrite(Error)) return false;
	if (BuildingIndex <= 0 || ArmyType < 1 || ArmyType > 4 || Tier < 1 || Tier > 5)
	{
		Error = TEXT("训练建筑或兵阶无效。");
		return false;
	}

	const auto Snapshot = GetSnapshot();
	const auto* Building = Snapshot.TrainingBuildings.FindByPredicate([=](const FRokServerTrainingBuildingView& Row)
	{
		return Row.BuildingIndex == BuildingIndex && Row.BuildingType == ArmyType + 8;
	});
	if (!Building || Building->Level <= 0 || !Snapshot.bBuildingsKnown || !Snapshot.bTrainingKnown)
	{
		Error = TEXT("请从已同步的训练建筑提交训练。");
		return false;
	}
	const auto* Queue = Snapshot.TrainingQueues.FindByPredicate([ArmyType](const FRokServerTrainingQueue& Row)
	{
		return Row.QueueIndex == ArmyType;
	});
	if (Queue && Queue->State != ERokServerTrainingState::Idle)
	{
		Error = Queue->State == ERokServerTrainingState::AwaitingReceive
			? TEXT("请先领取已完成的部队。") : TEXT("该兵种训练队列正在使用或等待同步。");
		return false;
	}
	const auto Role = Net->GetRoleInfo();
	double ServerSeconds = 0;
	const bool bTimeKnown = Net->TryGetServerTimeSeconds(ServerSeconds);
	for (const auto& Row : Rows(Role, TEXT("buildQueue")))
	{
		int64 Index = 0, Finish = 0;
		if (ReadInt(Row, TEXT("buildingIndex"), Index) && Index == BuildingIndex
			&& ReadInt(Row, TEXT("finishTime"), Finish) && Finish > 0 && (!bTimeKnown || Finish > ServerSeconds))
		{
			Error = TEXT("训练建筑正在升级。");
			return false;
		}
	}
	const auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	FRokUnityCatalogBuildingType BuildingType;
	FRokUnityCatalogTroop Troop;
	if (!Catalog || !Catalog->GetBuildingType(Building->BuildingType, BuildingType)
		|| !GetTrainingTroop(ArmyType, Tier, Troop))
	{
		Error = TEXT("训练配置尚未同步。");
		return false;
	}
	if (!IsTrainingTierUnlocked(Troop.StudyId))
	{
		Error = TEXT("该兵阶尚未解锁。");
		return false;
	}
	const int32 BaseCapacity = BuildingType.GetTrainingCapacity(Building->Level);
	if (BaseCapacity <= 0 || Count <= 0 || Count > int64(BaseCapacity) + GetTrainingCapacityBonus())
	{
		Error = TEXT("训练数量超出当前可选容量。");
		return false;
	}
	const TCHAR* Fields[] = {TEXT("food"), TEXT("wood"), TEXT("stone"), TEXT("gold")};
	if (ConfirmedGems.IsSet())
	{
		int64 Price = 0, Balance = 0;
		if (!GetInstantTrainingPrice(Troop, Count, Price) || !ReadInt(Role, TEXT("denar"), Balance) || !Snapshot.bCityKnown)
		{
			Error = TEXT("宝石费用或部队数据待同步。");
			return false;
		}
		if (Price != *ConfirmedGems)
		{
			Error = TEXT("宝石费用已变化，请重新确认。");
			return false;
		}
		if (Balance < Price)
		{
			Error = TEXT("宝石不足。");
			return false;
		}
	}
	const TCHAR* Names[] = {TEXT("粮食"), TEXT("木材"), TEXT("石料"), TEXT("金币")};
	const int32 Costs[] = {Troop.TrainCost.Food, Troop.TrainCost.Wood, Troop.TrainCost.Stone, Troop.TrainCost.Gold};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Fields); ++Index)
	{
		int64 Available = 0;
		if (!ConfirmedGems.IsSet() && Costs[Index] > 0 && ReadInt(Role, Fields[Index], Available)
			&& Available < int64(Count) * Costs[Index])
		{
			Error = FString::Printf(TEXT("%s不足。"), Names[Index]);
			return false;
		}
	}
	URokGameNetSubsystem::FTrainingWrite Write;
	Write.BuildingIndex = BuildingIndex;
	Write.ArmyType = ArmyType;
	Write.Tier = Tier;
	Write.Count = Count;
	Write.bImmediately = ConfirmedGems.IsSet();
	Write.TroopId = Troop.Id;
	if (const auto* Soldier = Snapshot.CitySoldiers.FindByPredicate([&Troop](const FRokServerSoldierStack& Row)
		{ return Row.Id == Troop.Id; }))
		Write.PreviousTroops = Soldier->Count;
	Write.OnResult = MoveTemp(OnResult);
	return Net->SendTrainingWrite(MoveTemp(Write), Error);
}

bool URokServerArmySubsystem::RequestTrainingAward(int32 BuildingIndex, int32 ArmyType,
	TFunction<void(const FString&)> OnResult, FString& Error)
{
	auto* Instance = GetGameInstance();
	auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net) { Error = TEXT("服务器连接尚未就绪。"); return false; }
	if (!Net->CanRequestTrainingWrite(Error)) return false;
	if (BuildingIndex <= 0 || ArmyType < 1 || ArmyType > 4)
	{
		Error = TEXT("训练建筑无效。");
		return false;
	}

	const auto Snapshot = GetSnapshot();
	const auto* Queue = Snapshot.TrainingQueues.FindByPredicate([=](const FRokServerTrainingQueue& Row)
	{
		return Row.QueueIndex == ArmyType && Row.ArmyType == ArmyType && Row.BuildingIndex == BuildingIndex;
	});
	if (!Snapshot.bTrainingKnown || !Snapshot.bCityKnown || !Queue || !Queue->bBuildingKnown
		|| Queue->State != ERokServerTrainingState::AwaitingReceive || Queue->TargetTroopId <= 0
		|| Queue->Count <= 0 || Queue->Count > MAX_int32)
	{
		Error = TEXT("当前没有可领取的训练队列。");
		return false;
	}
	URokGameNetSubsystem::FTrainingWrite Write;
	Write.bReceive = true;
	Write.BuildingIndex = BuildingIndex;
	Write.ArmyType = ArmyType;
	Write.Tier = Queue->NewTier;
	Write.Count = static_cast<int32>(Queue->Count);
	Write.TroopId = Queue->TargetTroopId;
	if (const auto* Soldier = Snapshot.CitySoldiers.FindByPredicate([Queue](const FRokServerSoldierStack& Row)
		{ return Row.Id == Queue->TargetTroopId; }))
		Write.PreviousTroops = Soldier->Count;
	Write.OnResult = MoveTemp(OnResult);
	return Net->SendTrainingWrite(MoveTemp(Write), Error);
}

FRokServerArmySnapshot URokServerArmySubsystem::GetSnapshot() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestSnapshot.IsSet()) return TestSnapshot.GetValue();
#endif
	FRokServerArmySnapshot Empty;
	const UGameInstance* Instance = GetGameInstance();
	const URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady) return Empty;
	double ServerSeconds = 0.0;
	const bool bHasServerTime = Net->TryGetServerTimeSeconds(ServerSeconds);
	return ProjectSnapshot(Net->GetRoleInfo(), Net->GetCollection(TEXT("Army_ArmyList"), TEXT("armyInfo")),
		Net->GetCollection(TEXT("Transport_TransportList"), TEXT("transportInfo")),
		Net->GetCollection(TEXT("Role_ScoutsInfo"), TEXT("scoutsQueue")),
		Net->GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo")),
		Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>(),
		DispatchSlotsByTownHallLevel, FeatureArmsByCountry, bHasServerTime, ServerSeconds);
}

namespace
{
int32 OwnedGatherArmy(const TSharedPtr<FJsonObject>& Point, int64 RoleId)
{
	int64 Collector = 0, Index = 0, Type = 0;
	if (RoleId <= 0 || !ReadInt(Point, TEXT("objectType"), Type) || Type < 4 || Type > 7
		|| !ReadInt(Point, TEXT("collectRid"), Collector) || Collector != RoleId
		|| !ReadInt(Point, TEXT("armyIndex"), Index) || Index <= 0 || Index > MAX_int32)
		return 0;
	return static_cast<int32>(Index);
}

bool MatchesCollectingArmy(const TSharedPtr<FJsonObject>& Point, const TSharedPtr<FJsonObject>& Army)
{
	int64 Status = 0, ObjectId = 0, TargetId = 0, Hero = 0;
	return ReadInt(Army, TEXT("mainHeroId"), Hero) && Hero > 0
		&& ReadInt(Army, TEXT("status"), Status) && ResolveMarchPhase(Status) == ERokServerMarchPhase::Collecting
		&& ReadInt(Point, TEXT("objectId"), ObjectId) && ObjectId > 0
		&& ReadInt(Child(Army, TEXT("targetArg")), TEXT("targetObjectIndex"), TargetId) && TargetId == ObjectId;
}
}

FRokGatherPointState URokServerArmySubsystem::ProjectGatherPoint(const TSharedPtr<FJsonObject>& Point,
	const TSharedPtr<FJsonObject>& Army, int64 RoleId, double ServerSeconds, int64 BaseGoal)
{
	FRokGatherPointState Result;
	int64 Collector = 0, Stock = 0, Start = 0, Speed = 0;
	if (!ReadInt(Point, TEXT("collectRid"), Collector) || Collector <= 0) return Result;
	Result.bOwned = RoleId > 0 && Collector == RoleId;
	Result.bCanRecall = OwnedGatherArmy(Point, RoleId) > 0 && MatchesCollectingArmy(Point, Army);
	if (!ReadInt(Point, TEXT("resourceAmount"), Stock) || Stock < 0) return Result;
	Result.Remaining = Stock;
	if (!ReadInt(Point, TEXT("collectTime"), Start) || Start <= 0
		|| !ReadInt(Point, TEXT("collectSpeed"), Speed) || Speed <= 0 || ServerSeconds < Start)
		return Result;

	// UI_Pop_WorldObjectInfoMediator.Update: the point publishes rate changes with
	// elapsed seconds at each old rate. The current segment starts after those.
	constexpr double SpeedScale = 10000.0;
	double PastSeconds = 0, PastAmount = 0;
	for (const auto& Segment : Rows(Point, TEXT("collectSpeeds")))
	{
		int64 Duration = 0, Rate = 0;
		if (ReadInt(Segment, TEXT("collectTime"), Duration) && Duration > 0
			&& ReadInt(Segment, TEXT("collectSpeed"), Rate) && Rate > 0)
		{
			PastSeconds += static_cast<double>(Duration);
			PastAmount += static_cast<double>(Duration) * (Rate / SpeedScale);
		}
	}
	if (Result.bCanRecall)
	{
		int64 End = 0;
		const auto Collect = Child(Army, TEXT("collectResource"));
		if (ReadInt(Collect, TEXT("endTime"), End) && End > Start + PastSeconds)
		{
			// ResourceMgr schedules ceil(amount / speed). The resulting goal can
			// exceed the exact load by less than one second of gathering.
			const double ScheduledAmount = PastAmount + (End - Start - PastSeconds) * (Speed / SpeedScale);
			const double LastSecondAmount = ScheduledAmount - Speed / SpeedScale;
			const int64 StockLimitedGoal = FMath::Min(Stock, BaseGoal);
			if (StockLimitedGoal > LastSecondAmount && StockLimitedGoal <= ScheduledAmount)
			{
				Result.Goal = StockLimitedGoal;
			}
			else
			{
				Result.Goal = FMath::FloorToInt64(FMath::Min(static_cast<double>(Stock), ScheduledAmount));
				Result.bGoalApproximate = true;
			}
		}
	}
	const double CurrentAmount = PastAmount + FMath::Max(0.0, FMath::FloorToDouble(ServerSeconds) - Start - PastSeconds)
		* (Speed / SpeedScale);
	Result.Gathered = FMath::FloorToInt64(FMath::Min(CurrentAmount, static_cast<double>(Result.Goal >= 0 ? Result.Goal : Stock)));
	Result.Remaining = Stock - Result.Gathered;
	return Result;
}

FRokGatherPointState URokServerArmySubsystem::GetGatherPointState(const FString& ObjectId) const
{
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady || ObjectId.IsEmpty()) return {};
	const auto Point = Child(Net->GetWorldMapObjects(), *ObjectId);
	const int32 ArmyIndex = OwnedGatherArmy(Point, Net->GetRoleId());
	const auto Army = ArmyIndex > 0
		? Child(Net->GetCollection(TEXT("Army_ArmyList"), TEXT("armyInfo")), *LexToString(ArmyIndex)) : nullptr;
	int64 BaseGoal = -1;
	const auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	// Config.bin foodRaito/woodRaito/stoneRaito/goldRaito are all 1 for
	// these four map types. An empty cargo list makes base Arms.capacity a
	// candidate; the server's finish-time interval checks for load modifiers.
	if (Catalog && Army && HasRows(Army, TEXT("resourceLoads")) && Rows(Army, TEXT("resourceLoads")).IsEmpty())
	{
		const auto Soldiers = Rows(Army, TEXT("soldiers"));
		if (!Soldiers.IsEmpty())
		{
			BaseGoal = 0;
			for (const auto& Row : Soldiers)
			{
				FRokUnityCatalogTroop Troop;
				int64 Id = 0, Num = 0;
				if (!ReadInt(Row, TEXT("id"), Id) || Id <= 0 || Id > MAX_int32
					|| !ReadInt(Row, TEXT("num"), Num) || Num <= 0 || Num > MAX_int32
					|| !Catalog->GetTroop(static_cast<int32>(Id), Troop) || Troop.Capacity <= 0)
				{
					BaseGoal = -1;
					break;
				}
				BaseGoal += Num * Troop.Capacity;
			}
		}
	}
	double ServerSeconds = 0.0;
	Net->TryGetServerTimeSeconds(ServerSeconds);
	return ProjectGatherPoint(Point, Army, Net->GetRoleId(), ServerSeconds, BaseGoal);
}

bool URokServerArmySubsystem::RequestGatherRecall(const FString& ObjectId,
	TFunction<void(const FString&)> OnResult, FString& Error)
{
	auto* Instance = GetGameInstance();
	auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady)
	{
		Error = TEXT("连接服务器后可召回部队。");
		return false;
	}
	const auto Point = Child(Net->GetWorldMapObjects(), *ObjectId);
	const int32 ArmyIndex = OwnedGatherArmy(Point, Net->GetRoleId());
	if (ArmyIndex <= 0 || !MatchesCollectingArmy(Point,
		Child(Net->GetCollection(TEXT("Army_ArmyList"), TEXT("armyInfo")), *LexToString(ArmyIndex))))
	{
		Error = TEXT("采集部队已离开或正在同步。");
		return false;
	}
	return Net->SendGatherRecall(ArmyIndex, MoveTemp(OnResult), Error);
}

namespace
{
// Unity TroopAttackType.Collect and MapObjectInfo object types for the four
// ordinary gathering nodes in UI_Pop_WorldObjectInfo.
constexpr int32 GatherTargetType = 4;
constexpr int32 AttackTargetType = 1;

bool IsBarbarianTarget(const TSharedPtr<FJsonObject>& Target, int64 TargetId)
{
	int64 Id = 0, Type = 0;
	return TargetId > 0 && ReadInt(Target, TEXT("objectId"), Id) && Id == TargetId
		&& ReadInt(Target, TEXT("objectType"), Type) && Type == 2;
}

TSharedPtr<FJsonObject> MakeDispatchTarget(int64 TargetId, const TSharedPtr<FJsonObject>& Target, FString& Error)
{
	const auto Position = Child(Target, TEXT("objectPos"));
	int64 X = 0, Y = 0;
	if (!ReadInt(Position, TEXT("x"), X) || !ReadInt(Position, TEXT("y"), Y))
	{
		Error = TEXT("服务器尚未同步目标坐标。");
		return nullptr;
	}
	const auto Arg = MakeShared<FJsonObject>();
	Arg->SetField(TEXT("targetObjectIndex"), MakeShared<FJsonValueNumberString>(LexToString(TargetId)));
	Arg->SetObjectField(TEXT("pos"), Position);
	return Arg;
}

bool CheckGatherTarget(const TSharedPtr<FJsonObject>& Target, int64 TargetId, FString& Error)
{
	int64 Id = 0, Type = 0, Collector = 0;
	if (TargetId <= 0 || !ReadInt(Target, TEXT("objectId"), Id) || Id != TargetId
		|| !ReadInt(Target, TEXT("objectType"), Type) || Type < 4 || Type > 7)
	{
		Error = TEXT("请重新选择服务器上的资源点。");
		return false;
	}
	if (ReadInt(Target, TEXT("collectRid"), Collector) && Collector > 0)
	{
		Error = TEXT("该资源点已有部队在采集。");
		return false;
	}
	return true;
}
}

bool URokServerArmySubsystem::GetGatherOptions(int64 TargetId, FRokServerArmySnapshot& Army,
	TArray<FRokServerHeroEntry>& Heroes, FString& Error) const
{
	Heroes.Reset();
	const UGameInstance* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady)
	{
		Error = TEXT("连接服务器后可派兵采集。");
		return false;
	}
	Error.Reset();
	if (!CheckGatherTarget(Child(Net->GetWorldMapObjects(), *LexToString(TargetId)),
			TargetId, Error)) return false;

	Army = GetSnapshot();
	if (Army.Marches.ContainsByPredicate([TargetId](const FRokServerMarch& March)
		{ return March.TargetType == GatherTargetType && March.TargetObjectId == TargetId; }))
	{
		Error = TEXT("已有部队前往此资源点。");
		return false;
	}
	return GetCityArmyOptions(Army, Heroes, Error);
}

bool URokServerArmySubsystem::GetCityArmyOptions(FRokServerArmySnapshot& Army,
	TArray<FRokServerHeroEntry>& Heroes, FString& Error) const
{
	Heroes.Reset();
	const auto* HeroSubsystem = GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>();
	const FRokServerHeroSnapshot Roster = HeroSubsystem ? HeroSubsystem->GetSnapshot() : FRokServerHeroSnapshot();
	if (!Army.bCityKnown || !Army.bDispatchUsedKnown || !Roster.bHeroesKnown || !Roster.bRowsComplete)
	{
		Error = TEXT("正在同步城内士兵和统帅，请稍后重试。");
		return false;
	}
	if (Army.bDispatchCapacityKnown && Army.DispatchSlotsUsed >= Army.DispatchSlotsMax)
	{
		Error = TEXT("所有出征队列均已占用。");
		return false;
	}
	if (Army.CityCount <= 0)
	{
		Error = TEXT("城内暂无可派遣的士兵。");
		return false;
	}
	for (const FRokServerHeroEntry& Hero : Roster.Heroes)
	{
		if (!Army.Marches.ContainsByPredicate([Id = Hero.HeroId](const FRokServerMarch& March)
			{ return March.MainHeroId == Id || March.DeputyHeroId == Id; }))
		{
			Heroes.Add(Hero);
		}
	}
	if (Heroes.IsEmpty())
	{
		Error = TEXT("暂无待命的统帅。");
		return false;
	}
	return true;
}

TSharedPtr<FJsonObject> URokServerArmySubsystem::MakeGatherRequest(int64 TargetId,
	const TSharedPtr<FJsonObject>& Target, const FRokServerArmySnapshot& Army,
	const TArray<FRokServerHeroEntry>& Heroes, int32 MainHeroId, int32 DeputyHeroId,
	const TMap<int32, int64>& Counts, bool bSituStation, FString& Error, bool bAttack)
{
	if (bAttack && !IsBarbarianTarget(Target, TargetId))
	{
		Error = TEXT("该乱军已离开视野或消失。");
		return nullptr;
	}
	if (!bAttack && !CheckGatherTarget(Target, TargetId, Error)) return nullptr;
	const FRokServerHeroEntry* Main = Heroes.FindByPredicate([MainHeroId](const FRokServerHeroEntry& Hero)
		{ return Hero.HeroId == MainHeroId; });
	if (!Main || MainHeroId <= 0 || (DeputyHeroId != 0 && (MainHeroId == DeputyHeroId
		|| !Heroes.ContainsByPredicate([DeputyHeroId](const FRokServerHeroEntry& Hero) { return Hero.HeroId == DeputyHeroId; }))))
	{
		Error = TEXT("所选统帅已不可用，请重新编队。");
		return nullptr;
	}
	if (DeputyHeroId != 0 && (!Main->bStarKnown || Main->Star < 3))
	{
		Error = TEXT("主将需达到三星才能携带副将。");
		return nullptr;
	}
	TArray<TSharedPtr<FJsonValue>> Soldiers;
	for (const auto& Pair : Counts)
	{
		const FRokServerSoldierStack* Stack = Army.CitySoldiers.FindByPredicate([Id = Pair.Key](const FRokServerSoldierStack& Row)
			{ return Row.Id == Id; });
		if (!Stack || Pair.Value <= 0 || Pair.Value > Stack->Count || Pair.Value > MAX_int32
			|| Stack->FamilyType <= 0 || Stack->Tier <= 0)
		{
			Error = TEXT("城内可用士兵已变化，请重新编队。");
			return nullptr;
		}
		const auto Soldier = MakeShared<FJsonObject>();
		Soldier->SetNumberField(TEXT("id"), Stack->Id);
		Soldier->SetNumberField(TEXT("type"), Stack->FamilyType);
		Soldier->SetNumberField(TEXT("level"), Stack->Tier);
		Soldier->SetNumberField(TEXT("num"), Pair.Value);
		Soldiers.Add(MakeShared<FJsonValueObject>(Soldier));
	}
	if (Soldiers.IsEmpty())
	{
		Error = TEXT("请至少选择一名士兵。");
		return nullptr;
	}
	// TroopProxy.SendCreateTroopServer takes the server object ID and raw PosInfo.
	const auto Arg = MakeDispatchTarget(TargetId, Target, Error);
	if (!Arg) return nullptr;
	const auto Request = MakeShared<FJsonObject>();
	Request->SetNumberField(TEXT("mainHeroId"), MainHeroId);
	Request->SetNumberField(TEXT("deputyHeroId"), DeputyHeroId);
	Request->SetArrayField(TEXT("soldiers"), MoveTemp(Soldiers));
	Request->SetNumberField(TEXT("targetType"), bAttack ? AttackTargetType : GatherTargetType);
	Request->SetObjectField(TEXT("targetArg"), Arg);
	Request->SetBoolField(TEXT("isSituStation"), bSituStation);
	return Request;
}

bool URokServerArmySubsystem::RequestGather(int64 TargetId, int32 MainHeroId, int32 DeputyHeroId,
	const TMap<int32, int64>& Counts, TFunction<void(int32, const FString&)> OnResult, FString& Error)
{
	FRokServerArmySnapshot Army;
	TArray<FRokServerHeroEntry> Heroes;
	if (!GetGatherOptions(TargetId, Army, Heroes, Error)) return false;

	auto* Net = GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
	bool bSituStation = false;
	if (const auto Role = Net->GetRoleInfo()) Role->TryGetBoolField(TEXT("situStation"), bSituStation);
	const auto Target = Child(Net->GetWorldMapObjects(), *LexToString(TargetId));
	const auto Body = MakeGatherRequest(TargetId, Target, Army, Heroes, MainHeroId, DeputyHeroId, Counts,
		bSituStation, Error);
	return Body && Net->SendArmyDispatch(Body, TargetId, MainHeroId, MoveTemp(OnResult), Error);
}

bool URokServerArmySubsystem::GetBarbarianAttackInfo(int64 TargetId, FRokBarbarianAttackInfo& Info) const
{
	Info = {};
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	if (!Net || !Config || Net->GetState() != ERokGameNetState::RoleReady) return false;
	const auto Target = Child(Net->GetWorldMapObjects(), *LexToString(TargetId));
	int64 MonsterId = 0;
	int32 Type = 0;
	if (!IsBarbarianTarget(Target, TargetId) || !ReadInt(Target, TEXT("monsterId"), MonsterId)
		|| MonsterId <= 0 || MonsterId > MAX_int32
		|| !Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("type"), Type) || Type != 1
		|| !Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("level"), Info.Level)
		|| !Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("costAP"), Info.ActionCost)) return false;
	Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("l_nameId"), Info.NameId);
	Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("l_descId"), Info.DescriptionId);
	Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("showReward"), Info.RewardGroupId);
	Config->GetStringFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("headIcon"), Info.HeadIcon);
	int64 RefreshTime = 0;
	int32 ShowTime = 0;
	if (ReadInt(Target, TEXT("refreshTime"), RefreshTime) && RefreshTime > 0
		&& Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("showTime"), ShowTime)
		&& ShowTime > 0 && RefreshTime <= MAX_int64 - ShowTime)
	{
		Info.ExpiresAt = RefreshTime + ShowTime;
	}

	const auto Role = Net->GetRoleInfo();
	if (!Role)
	{
		Info.BlockReason = TEXT("正在同步角色状态。");
		return true;
	}
	Role->TryGetBoolField(TEXT("situStation"), Info.bSituStation);
	ReadInt(Role, TEXT("actionForce"), Info.ActionForce);
	int64 Defeated = 0;
	if (ReadInt(Role, TEXT("barbarianLevel"), Defeated))
		Info.AttackLevelMax = static_cast<int32>(FMath::Clamp<int64>(Defeated, 0, MAX_int32 - 1)) + 1;
	if (Info.AttackLevelMax == 0)
		Info.BlockReason = TEXT("正在同步可攻击的乱军等级。");
	else if (Info.Level > Info.AttackLevelMax)
		Info.BlockReason = FString::Printf(TEXT("请先击败等级 %d 的乱军。"), Info.AttackLevelMax);

	return true;
}

bool URokServerArmySubsystem::GetBarbarianArmyOptions(int64 TargetId, FRokServerArmySnapshot& Army,
	TArray<FRokServerHeroEntry>& Heroes, FString& Error) const
{
	FRokBarbarianAttackInfo Info;
	if (!GetBarbarianAttackInfo(TargetId, Info))
	{
		Error = TEXT("该乱军已离开视野或尚未同步。");
		return false;
	}
	Error = Info.BlockReason;
	if (!Error.IsEmpty()) return false;
	if (Info.ActionForce >= 0 && Info.ActionForce < Info.ActionCost)
	{
		Error = TEXT("行动力不足，无法派出新部队。");
		return false;
	}
	Army = GetSnapshot();
	return GetCityArmyOptions(Army, Heroes, Error);
}

bool URokServerArmySubsystem::RequestBarbarianAttack(int64 TargetId, int32 MainHeroId, int32 DeputyHeroId,
	const TMap<int32, int64>& Counts, bool bSituStation, TFunction<void(int32, const FString&)> OnResult, FString& Error)
{
	FRokServerArmySnapshot Army;
	TArray<FRokServerHeroEntry> Heroes;
	if (!GetBarbarianArmyOptions(TargetId, Army, Heroes, Error)) return false;

	auto* Net = GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
	const auto Target = Child(Net->GetWorldMapObjects(), *LexToString(TargetId));
	const auto Body = MakeGatherRequest(TargetId, Target, Army, Heroes, MainHeroId, DeputyHeroId,
		Counts, bSituStation, Error, true);
	return Body && Net->SendArmyDispatch(Body, TargetId, MainHeroId, MoveTemp(OnResult), Error);
}

bool URokServerArmySubsystem::CanRetargetArmy(const FRokServerMarch& Army, int64 TargetId)
{
	// Map.March rejects rally and failed armies. This picker offers active field units.
	return Army.ArmyIndex > 0 && Army.Count > 0 && !Army.bInRally && !(Army.StatusMask & (1 << 6))
		&& !(Army.TargetType == AttackTargetType && Army.TargetObjectId == TargetId)
		&& (Army.Phase == ERokServerMarchPhase::Marching || Army.Phase == ERokServerMarchPhase::Stationed
			|| Army.Phase == ERokServerMarchPhase::Returning || Army.Phase == ERokServerMarchPhase::Collecting);
}

TSharedPtr<FJsonObject> URokServerArmySubsystem::MakeExistingAttackRequest(int64 TargetId,
	const TSharedPtr<FJsonObject>& Target, int32 ArmyIndex, bool bSituStation, FString& Error)
{
	if (!IsBarbarianTarget(Target, TargetId) || ArmyIndex <= 0)
	{
		Error = TEXT("请重新选择乱军和部队。");
		return nullptr;
	}
	const auto Arg = MakeDispatchTarget(TargetId, Target, Error);
	if (!Arg) return nullptr;
	const auto Request = MakeShared<FJsonObject>();
	Request->SetNumberField(TEXT("armyIndex"), ArmyIndex);
	Request->SetNumberField(TEXT("targetType"), AttackTargetType);
	Request->SetObjectField(TEXT("targetArg"), Arg);
	Request->SetBoolField(TEXT("isSituStation"), bSituStation);
	return Request;
}

bool URokServerArmySubsystem::RequestExistingArmyAttack(int64 TargetId, int32 ArmyIndex, bool bSituStation,
	TFunction<void(int32, const FString&)> OnResult, FString& Error)
{
	FRokBarbarianAttackInfo Info;
	if (!GetBarbarianAttackInfo(TargetId, Info))
	{
		Error = TEXT("该乱军已离开视野或尚未同步。");
		return false;
	}
	Error = Info.BlockReason;
	if (!Error.IsEmpty()) return false;
	const auto Snapshot = GetSnapshot();
	const auto* Army = Snapshot.Marches.FindByPredicate([ArmyIndex](const FRokServerMarch& Row)
		{ return Row.ArmyIndex == ArmyIndex; });
	if (!Army || !CanRetargetArmy(*Army, TargetId))
	{
		Error = TEXT("该部队当前无法出征，请重新选择。");
		return false;
	}
	auto* Net = GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
	const auto Body = MakeExistingAttackRequest(TargetId, Child(Net->GetWorldMapObjects(), *LexToString(TargetId)),
		ArmyIndex, bSituStation, Error);
	return Body && Net->SendArmyDispatch(Body, TargetId, Army->MainHeroId, MoveTemp(OnResult), Error, ArmyIndex);
}

ERokServerMarchPhase FRokServerMarch::ClassifyStatus(int64 Status)
{
	return ResolveMarchPhase(Status);
}

bool FRokServerMarch::TryGetAttackMarchPosition(FVector2D& OutPosition, int32* OutNextPoint,
	double ElapsedSinceSnapshot) const
{
	const double TimeLeft = RemainingSeconds - ElapsedSinceSnapshot;
	if (TargetType != AttackTargetType || Phase != ERokServerMarchPhase::Marching || !bTimeKnown
		|| StartTime <= 0 || ArrivalTime <= StartTime || TimeLeft <= 0 || AttackPath.Num() < 2) return false;
	double Length = 0;
	for (int32 Index = 1; Index < AttackPath.Num(); ++Index)
		Length += FVector2D::Distance(AttackPath[Index - 1], AttackPath[Index]);
	if (Length <= UE_SMALL_NUMBER) return false;

	const double Duration = static_cast<double>(ArrivalTime - StartTime);
	double Travelled = Length * FMath::Clamp(1.0 - TimeLeft / Duration, 0.0, 1.0);
	for (int32 Index = 1; Index < AttackPath.Num(); ++Index)
	{
		const FVector2D& From = AttackPath[Index - 1];
		const FVector2D& To = AttackPath[Index];
		const double Segment = FVector2D::Distance(From, To);
		if (Segment > UE_SMALL_NUMBER && Travelled <= Segment)
		{
			OutPosition = FMath::Lerp(From, To, Travelled / Segment);
			if (OutNextPoint) *OutNextPoint = Index;
			return true;
		}
		Travelled -= Segment;
	}
	OutPosition = AttackPath.Last();
	if (OutNextPoint) *OutNextPoint = AttackPath.Num() - 1;
	return true;
}

FRokServerArmySnapshot URokServerArmySubsystem::ProjectSnapshot(
	const TSharedPtr<FJsonObject>& Role, const TSharedPtr<FJsonObject>& ArmyInfo,
	const TSharedPtr<FJsonObject>& TransportInfo, const TSharedPtr<FJsonObject>& ScoutsQueue,
	const TSharedPtr<FJsonObject>& Buildings,
	const URokUnityGameplayCatalogSubsystem* Catalog,
	const TMap<int32, int32>& DispatchByTownHallLevel,
	const TMap<int32, TArray<int32>>& FeatureArmsByCountry,
	bool bHasServerTime, double ServerTimeSeconds)
{
	FRokServerArmySnapshot Result;
	Result.bAvailable = true;
	Result.bLoaded = Role.IsValid() && Catalog && Catalog->IsCatalogLoaded();
	Result.bCityKnown = HasRows(Role, TEXT("soldiers"));
	Result.bTrainingKnown = HasRows(Role, TEXT("armyQueue"));
	Result.bArmiesKnown = ArmyInfo.IsValid();
	Result.bBuildingsKnown = Buildings.IsValid();
	if (!Result.bLoaded)
	{
		Result.Signature = FString::Printf(TEXT("role-ready:partial:%d:%d:%d:%d"),
			Result.bCityKnown, Result.bTrainingKnown, Result.bArmiesKnown, Result.bBuildingsKnown);
		return Result;
	}
	if (Buildings.IsValid())
	{
		for (const auto& Pair : Buildings->Values)
		{
			const TSharedPtr<FJsonObject>* Building = nullptr;
			int64 Index = 0, Type = 0, Level = 0, FinishTime = -1;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Building) || !Building) continue;
			if (!ReadInt(*Building, TEXT("buildingIndex"), Index)) LexTryParseString(Index, *Pair.Key);
			if (Index <= 0 || !ReadInt(*Building, TEXT("type"), Type)
				|| Type < 9 || Type > 12 // EnumCityBuildingType: four troop training facilities.
				|| !ReadInt(*Building, TEXT("level"), Level) || Level <= 0) continue;
			ReadInt(*Building, TEXT("finishTime"), FinishTime);
			if (FinishTime == 0) continue;
			FRokServerTrainingBuildingView& View = Result.TrainingBuildings.AddDefaulted_GetRef();
			View.BuildingIndex = int32(Index); View.BuildingType = int32(Type); View.Level = int32(Level);
			FRokUnityCatalogBuildingType TypeRow;
			if (Catalog->GetBuildingType(View.BuildingType, TypeRow)) View.Name = TypeRow.DisplayNameZhCn;
			Result.KnownTrainingBuildingIndices.Add(View.BuildingIndex);
		}
		Result.TrainingBuildings.Sort([](const auto& A, const auto& B) { return A.BuildingIndex < B.BuildingIndex; });
		Result.KnownTrainingBuildingIndices.Sort();
	}
	Result.bCityPowerKnown = Result.bCityKnown;
	bool bCityRowsComplete = Result.bCityKnown;
	for (const TSharedPtr<FJsonObject>& SoldierRow : Rows(Role, TEXT("soldiers")))
	{
		int64 Num = 0;
		if (ReadInt(SoldierRow, TEXT("num"), Num) && Num <= 0) continue;
		FRokServerSoldierStack Stack = Soldier(SoldierRow, Catalog);
		if (Stack.Id <= 0) { Result.bCityPowerKnown = false; bCityRowsComplete = false; continue; }
		Result.CityCount += Stack.Count;
		Result.CityPower += Stack.TotalPower;
		if (!Stack.bCatalogKnown) Result.bCityPowerKnown = false;
		Result.CitySoldiers.Add(MoveTemp(Stack));
	}
	SortSoldiers(Result.CitySoldiers);

	Result.bFieldPowerKnown = Result.bArmiesKnown;
	Result.bUpgradingPowerKnown = Result.bTrainingKnown;
	TArray<FRokServerSoldierStack> ExtraSoldiers;
	int64 Country = 0;
	const bool bCountryKnown = ReadInt(Role, TEXT("country"), Country);
	bool bArmyRowsComplete = Result.bArmiesKnown;
	if (ArmyInfo.IsValid())
	{
		for (const auto& Pair : ArmyInfo->Values)
		{
			const TSharedPtr<FJsonObject>* RowPtr = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(RowPtr) || !RowPtr)
			{ bArmyRowsComplete = false; Result.bFieldPowerKnown = false; continue; }
			const TSharedPtr<FJsonObject>& Row = *RowPtr;
			int64 Hero = 0, Index = 0;
			if (ReadInt(Row, TEXT("mainHeroId"), Hero) && Hero == 0) continue; // TroopProxy deletion marker.
			if (!ReadInt(Row, TEXT("armyIndex"), Index)) LexTryParseString(Index, *Pair.Key);
			if (Index <= 0 || !ReadInt(Row, TEXT("mainHeroId"), Hero) || Hero <= 0)
			{ bArmyRowsComplete = false; Result.bFieldPowerKnown = false; continue; }
			FRokServerMarch March;
			March.ArmyIndex = int32(Index); March.MainHeroId = int32(Hero);
			int64 Value = 0;
			if (ReadInt(Row, TEXT("deputyHeroId"), Value)) March.DeputyHeroId = int32(Value);
			if (ReadInt(Row, TEXT("status"), Value)) March.StatusMask = Value;
			March.Phase = ResolveMarchPhase(March.StatusMask);
			Row->TryGetBoolField(TEXT("isInRally"), March.bInRally);
			if (ReadInt(Row, TEXT("targetType"), Value)) March.TargetType = int32(Value);
			const TSharedPtr<FJsonObject> Target = Child(Row, TEXT("targetArg"));
			ReadInt(Target, TEXT("targetObjectIndex"), March.TargetObjectId);
			ReadInt(Row, TEXT("objectIndex"), March.MapObjectId);
			if (Target.IsValid())
			{
				Target->TryGetStringField(TEXT("targetName"), March.TargetName);
				const TSharedPtr<FJsonObject> Pos = Child(Target, TEXT("pos"));
				int64 X = 0, Y = 0;
				if (ReadInt(Pos, TEXT("x"), X) && ReadInt(Pos, TEXT("y"), Y))
				{ March.TargetX = int32(X); March.TargetY = int32(Y); March.bTargetKnown = true; }
			}
			ReadInt(Row, TEXT("startTime"), March.StartTime);
			ReadInt(Row, TEXT("arrivalTime"), March.ArrivalTime);
			March.bTimeKnown = bHasServerTime && March.ArrivalTime > 0;
			if (March.bTimeKnown) March.RemainingSeconds = FMath::Max(0.0, double(March.ArrivalTime) - ServerTimeSeconds);
			if (March.TargetType == AttackTargetType && March.Phase == ERokServerMarchPhase::Marching)
			{
				for (const auto& Point : Rows(Row, TEXT("path")))
				{
					int64 X = 0, Y = 0;
					if (ReadInt(Point, TEXT("x"), X) && ReadInt(Point, TEXT("y"), Y))
						March.AttackPath.Add(FVector2D(X, Y) / 100.0);
				}
			}
			March.bSoldiersKnown = HasRows(Row, TEXT("soldiers"));
			March.bPowerKnown = March.bSoldiersKnown;
			if (!March.bSoldiersKnown) { Result.bFieldPowerKnown = false; bArmyRowsComplete = false; }
			for (const TSharedPtr<FJsonObject>& SoldierRow : Rows(Row, TEXT("soldiers")))
			{
				int64 Num = 0;
				if (ReadInt(SoldierRow, TEXT("num"), Num) && Num <= 0) continue;
				FRokServerSoldierStack Stack = Soldier(SoldierRow, Catalog);
				if (Stack.Id <= 0) { March.bPowerKnown = false; Result.bFieldPowerKnown = false; bArmyRowsComplete = false; continue; }
				March.Count += Stack.Count;
				March.Power += Stack.TotalPower;
				if (!Stack.bCatalogKnown) { March.bPowerKnown = false; Result.bFieldPowerKnown = false; }
				Result.FieldCount += Stack.Count;
				Result.FieldPower += Stack.TotalPower;
				Result.FieldSoldiers.Add(Stack);
				March.Soldiers.Add(MoveTemp(Stack));
			}
			for (const TSharedPtr<FJsonObject>& MinorRow : Rows(Row, TEXT("minorSoldiers")))
			{
				int64 Num = 0;
				if (ReadInt(MinorRow, TEXT("num"), Num) && Num <= 0) continue;
				FRokServerSoldierStack Stack = Soldier(MinorRow, Catalog);
				if (Stack.Id <= 0) { bArmyRowsComplete = false; continue; }
				Stack.TotalPower = 0; // SoldierProxy.GetTroopsTotalPower excludes minorSoldiers.
				March.MinorCount += Stack.Count;
				Result.FieldMinorCount += Stack.Count;
				ExtraSoldiers.Add(MoveTemp(Stack));
			}
			SortSoldiers(March.Soldiers);
			Result.Marches.Add(MoveTemp(March));
		}
		Result.Marches.Sort([](const FRokServerMarch& A, const FRokServerMarch& B) { return A.ArmyIndex < B.ArmyIndex; });
	}
	Result.bDispatchUsedKnown = bArmyRowsComplete && TransportInfo.IsValid();
	if (TransportInfo.IsValid())
	{
		for (const auto& Pair : TransportInfo->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			int64 TargetIndex = 0;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Row) || !Row
				|| !ReadInt(*Row, TEXT("targetObjectIndex"), TargetIndex))
			{ Result.bDispatchUsedKnown = false; continue; }
			if (TargetIndex != -1) ++Result.TransportCount;
		}
	}
	if (Result.bDispatchUsedKnown) Result.DispatchSlotsUsed = Result.Marches.Num() + Result.TransportCount;
	Result.bHudQueueKnown = Result.bDispatchUsedKnown && ScoutsQueue.IsValid();
	if (ScoutsQueue.IsValid())
	{
		for (const auto& Pair : ScoutsQueue->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			int64 Status = 0;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Row) || !Row
				|| !ReadInt(*Row, TEXT("scoutsStatus"), Status))
			{ Result.bHudQueueKnown = false; continue; }
			// ScoutProxy.GetAllActiveScounts excludes only ScoutState.None (1 << 14).
			if (Status != (int64(1) << 14)) ++Result.ActiveScoutCount;
		}
	}
	if (Result.bHudQueueKnown)
		Result.HudQueueRowCount = Result.DispatchSlotsUsed + Result.ActiveScoutCount;

	bool bTrainingRowsComplete = Result.bTrainingKnown;
	if (Result.bTrainingKnown)
	{
		for (const TSharedPtr<FJsonObject>& Row : Rows(Role, TEXT("armyQueue")))
		{
			int64 Index = 0;
			if (!ReadInt(Row, TEXT("queueIndex"), Index) || Index <= 0)
			{ bTrainingRowsComplete = false; continue; }
			FRokServerTrainingQueue Queue;
			Queue.QueueIndex = int32(Index);
			int64 Value = 0;
			if (ReadInt(Row, TEXT("buildingIndex"), Value)) Queue.BuildingIndex = int32(Value);
			if (ReadInt(Row, TEXT("armyType"), Value)) Queue.ArmyType = int32(Value);
			if (ReadInt(Row, TEXT("newArmyLevel"), Value)) Queue.NewTier = int32(Value);
			if (ReadInt(Row, TEXT("oldArmyLevel"), Value)) Queue.OldTier = int32(Value);
			if (ReadInt(Row, TEXT("armyNum"), Value)) Queue.Count = Value;
			ReadInt(Row, TEXT("beginTime"), Queue.BeginTime);
			ReadInt(Row, TEXT("finishTime"), Queue.FinishTime);
			ReadInt(Row, TEXT("firstFinishTime"), Queue.FirstFinishTime);
			if (Queue.OldTier > 0 && Queue.Count > 0 && !Row->HasField(TEXT("finishTime")))
				bTrainingRowsComplete = false;
			for (const FRokServerTrainingBuildingView& Building : Result.TrainingBuildings)
			{
				if (Building.BuildingIndex != Queue.BuildingIndex) continue;
				Queue.bBuildingKnown = true; Queue.BuildingType = Building.BuildingType;
				Queue.BuildingLevel = Building.Level; Queue.BuildingName = Building.Name;
				break;
			}
			Queue.TargetTroopId = bCountryKnown
				? ResolvedTroopId(int32(Country), Queue.ArmyType, Queue.NewTier, FeatureArmsByCountry, Catalog)
				: 0;
			if (Queue.TargetTroopId > 0) Queue.TroopName = TroopName(Queue.TargetTroopId);
			if (Queue.OldTier > 0 && Queue.Count > 0 && Queue.FinishTime > 0)
			{
				Result.UpgradingCount += Queue.Count;
				const int32 OldId = bCountryKnown
					? ResolvedTroopId(int32(Country), Queue.ArmyType, Queue.OldTier, FeatureArmsByCountry, Catalog)
					: 0;
				FRokServerSoldierStack OldStack;
				OldStack.Id = OldId; OldStack.FamilyType = Queue.ArmyType;
				OldStack.Tier = Queue.OldTier; OldStack.Count = Queue.Count;
				OldStack.Name = OldId > 0 ? TroopName(OldId)
					: FString::Printf(TEXT("兵种类型 %d T%d"), Queue.ArmyType, Queue.OldTier);
				FRokUnityCatalogTroop OldTroop;
				if (OldId > 0 && Catalog->GetTroop(OldId, OldTroop))
				{
					OldStack.bCatalogKnown = true;
					OldStack.PowerPerUnit = OldTroop.Power;
					OldStack.TotalPower = OldStack.Count * OldStack.PowerPerUnit;
					OldStack.Icon = OldTroop.Icon;
					Result.UpgradingPower += OldStack.TotalPower;
				}
				else Result.bUpgradingPowerKnown = false;
				ExtraSoldiers.Add(MoveTemp(OldStack));
			}
			int64 Finish = 0;
			// BuildingLogic.initQueue creates a never-used family slot with only queueIndex.
			if ((ReadInt(Row, TEXT("finishTime"), Finish) && Finish == -1) || Row->Values.Num() == 1)
				Queue.State = ERokServerTrainingState::Idle;
			else if (Finish > 0 && Queue.Count > 0 && Queue.ArmyType > 0)
			{
				Queue.State = Queue.OldTier > 0 ? ERokServerTrainingState::Upgrading : ERokServerTrainingState::Training;
				Queue.bTimeKnown = bHasServerTime;
				if (Queue.bTimeKnown)
				{
					Queue.RemainingSeconds = FMath::Max(0.0, double(Finish) - ServerTimeSeconds);
					if (Queue.RemainingSeconds <= 0.0) Queue.State = ERokServerTrainingState::AwaitingReceive;
				}
				if (Queue.BeginTime > 0 && Finish >= Queue.BeginTime)
				{
					Queue.DurationSeconds = double(Finish - Queue.BeginTime);
					if (Queue.bTimeKnown && Queue.DurationSeconds > 0)
						Queue.Progress01 = FMath::Clamp((ServerTimeSeconds - double(Queue.BeginTime)) / Queue.DurationSeconds, 0.0, 1.0);
				}
			}
			Result.TrainingQueues.Add(MoveTemp(Queue));
		}
		Result.TrainingQueues.Sort([](const FRokServerTrainingQueue& A, const FRokServerTrainingQueue& B)
		{ return A.QueueIndex < B.QueueIndex; });
	}

	int32 TownHallLevel = 0;
	if (Buildings.IsValid())
	{
		for (const auto& Pair : Buildings->Values)
		{
			const TSharedPtr<FJsonObject>* Building = nullptr;
			int64 Type = 0, Level = 0, FinishTime = -1;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Building) || !Building
				|| !ReadInt(*Building, TEXT("type"), Type) || Type != 1
				|| !ReadInt(*Building, TEXT("level"), Level) || Level <= 0) continue;
			ReadInt(*Building, TEXT("finishTime"), FinishTime);
			if (FinishTime != 0) TownHallLevel = FMath::Max(TownHallLevel, int32(Level));
		}
	}
	if (const int32* Max = DispatchByTownHallLevel.Find(TownHallLevel))
	{ Result.bDispatchCapacityKnown = true; Result.DispatchSlotsMax = *Max; }

	AggregateSoldiers(Result.FieldSoldiers);
	TMap<int32, FRokServerSoldierStack> AllById;
	for (const FRokServerSoldierStack& Stack : Result.CitySoldiers) AllById.Add(Stack.Id, Stack);
	for (const FRokServerSoldierStack& Stack : Result.FieldSoldiers)
	{
		if (FRokServerSoldierStack* Existing = AllById.Find(Stack.Id))
		{
			Existing->Count += Stack.Count; Existing->Minor += Stack.Minor;
			Existing->TotalPower += Stack.TotalPower;
			Existing->bCatalogKnown &= Stack.bCatalogKnown;
		}
		else AllById.Add(Stack.Id, Stack);
	}
	for (const FRokServerSoldierStack& Stack : ExtraSoldiers)
	{
		const int32 Key = Stack.Id > 0 ? Stack.Id : -(Stack.FamilyType * 100 + Stack.Tier);
		if (FRokServerSoldierStack* Existing = AllById.Find(Key))
		{
			Existing->Count += Stack.Count; Existing->Minor += Stack.Minor;
			Existing->TotalPower += Stack.TotalPower;
			Existing->bCatalogKnown &= Stack.bCatalogKnown;
		}
		else AllById.Add(Key, Stack);
	}
	AllById.GenerateValueArray(Result.AllSoldiers);
	SortSoldiers(Result.AllSoldiers);
	Result.TotalCount = Result.CityCount + Result.FieldCount + Result.FieldMinorCount + Result.UpgradingCount;
	Result.bTotalCountKnown = bCityRowsComplete && bArmyRowsComplete && bTrainingRowsComplete;
	Result.TotalPower = Result.CityPower + Result.FieldPower + Result.UpgradingPower;
	Result.bTotalPowerKnown = Result.bCityPowerKnown && Result.bFieldPowerKnown
		&& Result.bUpgradingPowerKnown && bTrainingRowsComplete;

	FString SignatureText = FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%d,%d,%d"),
		Result.bAvailable, Result.bLoaded, Result.bCityKnown, Result.bArmiesKnown,
		Result.bTrainingKnown, Result.bDispatchCapacityKnown, Result.bDispatchUsedKnown,
		Result.TransportCount, Result.DispatchSlotsUsed, Result.DispatchSlotsMax,
		Result.bHudQueueKnown, Result.ActiveScoutCount, Result.HudQueueRowCount,
		Result.CityCount, Result.FieldCount, Result.TotalCount,
		Result.CityPower, Result.FieldPower, Result.TotalPower,
		Result.bCityPowerKnown, Result.bFieldPowerKnown, Result.bTotalPowerKnown);
	SignatureText += FString::Printf(TEXT("|X%lld,%lld,%lld,%d,%d,%d"), Result.FieldMinorCount,
		Result.UpgradingCount, Result.UpgradingPower, Result.bTotalCountKnown,
		Result.bUpgradingPowerKnown, Result.bBuildingsKnown);
	for (const auto& Row : Result.TrainingBuildings)
		SignatureText += FString::Printf(TEXT("|B%d,%d,%d;%s"), Row.BuildingIndex,
			Row.BuildingType, Row.Level, *Row.Name);
	for (const auto& Row : Result.CitySoldiers) AppendSoldierSignature(SignatureText, Row);
	for (const auto& Row : Result.FieldSoldiers) AppendSoldierSignature(SignatureText, Row);
	for (const auto& Row : Result.AllSoldiers) AppendSoldierSignature(SignatureText, Row);
	for (const auto& Row : Result.TrainingQueues)
	{
		SignatureText += FString::Printf(TEXT("|Q%d,%d,%d,%d,%d,%d,%d,%d,%lld,%lld,%lld,%lld,%d,%d,%d;%s;%s"),
			Row.QueueIndex, Row.BuildingIndex, Row.BuildingType, Row.BuildingLevel,
			Row.ArmyType, Row.NewTier, Row.OldTier, Row.TargetTroopId,
			Row.Count, Row.BeginTime, Row.FinishTime, Row.FirstFinishTime,
			Row.bBuildingKnown, Row.bTimeKnown, int32(Row.State), *Row.BuildingName, *Row.TroopName);
	}
	for (const auto& Row : Result.Marches)
	{
		SignatureText += FString::Printf(TEXT("|M%d,%d,%d,%lld,%d,%d,%d,%d,%d,%d,%lld,%lld,%d,%lld,%lld,%lld,%d;%s"),
			Row.ArmyIndex, Row.MainHeroId, Row.DeputyHeroId, Row.StatusMask, int32(Row.Phase),
			Row.TargetType, Row.TargetX, Row.TargetY, Row.bTargetKnown, Row.bSoldiersKnown,
			Row.StartTime, Row.ArrivalTime, Row.bTimeKnown, Row.Count, Row.MinorCount, Row.Power, Row.bPowerKnown,
			*Row.TargetName);
		for (const auto& SoldierRow : Row.Soldiers) AppendSoldierSignature(SignatureText, SoldierRow);
	}
	Result.Signature = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*SignatureText));
	return Result;
}
