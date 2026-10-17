#include "RokBuildingUpgradeSubsystem.h"

#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Stats/Stats.h"
#include "RokUnityConfigSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokCitySpriteActor.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokLocalConstructionVisualFactory.h"

namespace
{
FString StripGeneratedSuffixes(FString Label)
{
	static const TCHAR* Suffixes[] = {
		TEXT("_Roof"),
		TEXT("_UpperBlock"),
		TEXT("_RoofBlock"),
		TEXT("_UpperKeep"),
		TEXT("_CrownRoof"),
		TEXT("_FlagBlock"),
		TEXT("_MillPost"),
		TEXT("_MillBlade"),
		TEXT("_Shaft"),
		TEXT("_Cap")
	};

	bool bChanged = true;
	while (bChanged)
	{
		bChanged = false;
		for (const TCHAR* Suffix : Suffixes)
		{
			if (Label.RemoveFromEnd(Suffix))
			{
				bChanged = true;
			}
		}
	}
	return Label;
}

TSoftObjectPtr<UTexture2D> MakeIcon(const TCHAR* Path)
{
	return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path));
}

int32 JsonInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return 0;
	}

	double Number = 0.0;
	if (Object->TryGetNumberField(FieldName, Number))
	{
		return FMath::RoundToInt(Number);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return FCString::Atoi(*StringValue);
	}

	return 0;
}

FString JsonString(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return FString();
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return StringValue;
	}

	double Number = 0.0;
	if (Object->TryGetNumberField(FieldName, Number))
	{
		return FString::SanitizeFloat(Number);
	}

	bool BoolValue = false;
	if (Object->TryGetBoolField(FieldName, BoolValue))
	{
		return BoolValue ? TEXT("true") : TEXT("false");
	}

	return FString();
}

bool JsonBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return false;
	}

	bool BoolValue = false;
	if (Object->TryGetBoolField(FieldName, BoolValue))
	{
		return BoolValue;
	}

	return JsonInt(Object, FieldName) != 0;
}

TSharedPtr<FJsonObject> JsonObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* ChildObject = nullptr;
	if (Object->TryGetObjectField(FieldName, ChildObject) && ChildObject)
	{
		return *ChildObject;
	}
	return nullptr;
}
}

bool FRokUnityBuildingLevelConfig::ParseSourceEconomy(const TSharedPtr<FJsonObject>& Row)
{
	bSourceEconomyValid = false;
	if (!Row.IsValid()) return false;
	const auto ReadRequired = [&Row](const TCHAR* Field, int32 Minimum, int32& Out)
	{
		const TSharedPtr<FJsonValue> Value = Row->TryGetField(Field);
		if (!Value.IsValid()) return false;
		if (Value->Type == EJson::Number)
		{
			const double Number = Value->AsNumber();
			if (!FMath::IsFinite(Number) || Number < Minimum || Number > MAX_int32
				|| Number != FMath::FloorToDouble(Number)) return false;
			Out = static_cast<int32>(Number);
			return true;
		}
		if (Value->Type != EJson::String) return false;
		const FString Text = Value->AsString();
		if (Text.IsEmpty()) return false;
		int64 Number = 0;
		for (const TCHAR Digit : Text)
		{
			if (Digit < TEXT('0') || Digit > TEXT('9')) return false;
			const int64 ValueDigit = Digit - TEXT('0');
			if (Number > (MAX_int32 - ValueDigit) / 10) return false;
			Number = Number * 10 + ValueDigit;
		}
		if (Number < Minimum) return false;
		Out = static_cast<int32>(Number);
		return true;
	};
	int32 ParsedType = 0, ParsedLevel = 0, Food = 0, Wood = 0, Stone = 0, Gold = 0;
	int32 Seconds = 0, ParsedPower = 0;
	if (!ReadRequired(TEXT("type"), 1, ParsedType)
		|| !ReadRequired(TEXT("level"), 1, ParsedLevel)
		|| !ReadRequired(TEXT("food"), 0, Food)
		|| !ReadRequired(TEXT("wood"), 0, Wood)
		|| !ReadRequired(TEXT("stone"), 0, Stone)
		|| !ReadRequired(TEXT("coin"), 0, Gold)
		|| !ReadRequired(TEXT("buildingTime"), 1, Seconds)
		|| !ReadRequired(TEXT("power"), 0, ParsedPower)
		|| ParsedType != TypeId || ParsedLevel != Level) return false;
	Cost.Food = Food;
	Cost.Wood = Wood;
	Cost.Stone = Stone;
	Cost.Gold = Gold;
	BuildSeconds = Seconds;
	Power = ParsedPower;
	bSourceEconomyValid = true;
	return true;
}

bool FRokUnityBuildingLevelConfig::ParsePrerequisites(const TSharedPtr<FJsonObject>& Row)
{
	Prerequisites.Reset();
	bPrerequisitesValid = false;
	if (!Row.IsValid()) return false;
	const auto ReadNonnegativeInt = [&Row](const FString& Field, int32& Out)
	{
		const TSharedPtr<FJsonValue> Value = Row->TryGetField(Field);
		if (!Value.IsValid()) return false;
		if (Value->Type == EJson::Number)
		{
			const double Number = Value->AsNumber();
			if (!FMath::IsFinite(Number) || Number < 0 || Number > MAX_int32 || Number != FMath::FloorToDouble(Number)) return false;
			Out = int32(Number); return true;
		}
		if (Value->Type != EJson::String) return false;
		const FString Text = Value->AsString();
		if (Text.IsEmpty()) return false;
		int64 Number = 0;
		for (TCHAR Character : Text)
		{
			if (Character < TEXT('0') || Character > TEXT('9')) return false;
			Number = Number * 10 + Character - TEXT('0');
			if (Number > MAX_int32) return false;
		}
		Out = int32(Number); return true;
	};
	for (int32 Slot = 1; Slot <= 3; ++Slot)
	{
		FRokUnityBuildingPrerequisite Requirement;
		if (!ReadNonnegativeInt(FString::Printf(TEXT("reqType%d"), Slot), Requirement.TypeId)
			|| !ReadNonnegativeInt(FString::Printf(TEXT("reqLevel%d"), Slot), Requirement.Level)
			|| (Requirement.TypeId > 0 && Requirement.Level == 0))
		{
			Prerequisites.Reset(); return false;
		}
		Prerequisites.Add(Requirement);
	}
	bPrerequisitesValid = true;
	return true;
}

bool FRokUnityBuildingLevelConfig::CheckPrerequisites(const TMap<int32, int32>& CompletedLevels,
	FRokUnityBuildingPrerequisite& OutMissing, int32& OutCurrentLevel) const
{
	OutMissing = FRokUnityBuildingPrerequisite(); OutCurrentLevel = 0;
	if (!bPrerequisitesValid || Prerequisites.Num() != 3) return false;
	for (const auto& Requirement : Prerequisites)
	{
		if (Requirement.TypeId == 0) continue;
		const int32 Current = CompletedLevels.FindRef(Requirement.TypeId);
		if (Current < Requirement.Level)
		{
			OutMissing = Requirement; OutCurrentLevel = Current; return false;
		}
	}
	return true;
}

void URokBuildingUpgradeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadUnityBuildingLevelConfig();
	LoadUnityBuildingProduceConfig();
	LoadUnityInitialCityState();
	FString ConstructionError;
	if (!ConstructionGrid.Load(ConstructionError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok source construction grid unavailable: %s"), *ConstructionError);
	}
	bConstructionSourcesLoaded = ConstructionSources.Load(ConstructionError);
	if (!bConstructionSourcesLoaded)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok source construction models unavailable: %s"), *ConstructionError);
	}
	if (!LocalCityContext.Load(ConstructionError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok local city simulation context unavailable: %s"), *ConstructionError);
	}
}

void URokBuildingUpgradeSubsystem::Tick(float DeltaTime)
{
	for (TPair<FName, FRokBuildingState>& Pair : BuildingStates)
	{
		RefreshUpgradeCompletion(Pair.Value);
	}
	FString ConstructionAuthorityError;
	const bool bMaySettleConstruction = LocalConstructionLedger.GetTickets().IsEmpty()
		|| CanUseLocalCitySimulation(ConstructionAuthorityError);
	if (bMaySettleConstruction)
	{
		TArray<FRokLocalConstructionTicket> CompletedConstruction;
		LocalConstructionLedger.CompleteDue(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, CompletedConstruction);
		for (const FRokLocalConstructionTicket& Ticket : LocalConstructionLedger.GetTickets())
		{
			if (!Ticket.bCompleted || (Ticket.TypeId != 2 && Ticket.TypeId != 3)
				|| (LocalConstructionActors.Contains(Ticket.Id)
					&& !PendingLocalConstructionActors.Contains(Ticket.Id))) continue;
			ARokCitySpriteActor* Actor = nullptr;
			if (const TWeakObjectPtr<ARokCitySpriteActor>* Pending = PendingLocalConstructionActors.Find(Ticket.Id))
			{
				Actor = Pending->Get();
			}
			FString Failure;
			if (!IsValid(Actor))
			{
				Failure = TEXT("已扣款的本地建筑缺少预备源视觉实例，完成登记待修复。");
			}
			else if (!LocalConstructionActors.Contains(Ticket.Id)
				&& !RegisterCompletedLocalConstructionActor(Ticket.Id, Actor, Failure))
			{
				// Keep the pending actor, ticket, and error for explicit recovery.
			}
			else
			{
				FRokLocalConstructionVisualFactory::MarkCompletedVisual(*Actor, Ticket);
				PendingLocalConstructionActors.Remove(Ticket.Id);
				LocalConstructionCompletionFailures.Remove(Ticket.Id);
				continue;
			}
			if (LocalConstructionCompletionFailures.FindRef(Ticket.Id) != Failure)
			{
				UE_LOG(LogTemp, Error, TEXT("Local construction completion held: id=%llu type=%d: %s"),
					static_cast<unsigned long long>(Ticket.Id), Ticket.TypeId, *Failure);
			}
			LocalConstructionCompletionFailures.Add(Ticket.Id, Failure);
		}
	}
	else
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
		for (const FRokLocalConstructionTicket& Ticket : LocalConstructionLedger.GetTickets())
		{
			if (!PendingLocalConstructionActors.Contains(Ticket.Id)
				|| (FMath::IsFinite(Now) && Now < Ticket.FinishesAt)) continue;
			if (LocalConstructionCompletionFailures.FindRef(Ticket.Id) != ConstructionAuthorityError)
			{
				UE_LOG(LogTemp, Error, TEXT("Local construction completion blocked by authority: id=%llu: %s"),
					static_cast<unsigned long long>(Ticket.Id), *ConstructionAuthorityError);
			}
			LocalConstructionCompletionFailures.Add(Ticket.Id, ConstructionAuthorityError);
		}
	}
	// A listener may inspect/register another visual part of a building. Deliver
	// only after iteration, and move the queue so reentrant completions wait.
	const TArray<FRokLocalBuildingUpgradeCompleted> Completed = MoveTemp(PendingCompletedUpgrades);
	for (const FRokLocalBuildingUpgradeCompleted& Result : Completed)
	{
		OnLocalUpgradeCompleted.Broadcast(Result);
	}
}

bool URokBuildingUpgradeSubsystem::CanUseLocalCitySimulation(FString& OutFailureReason) const
{
	if (!LocalCityContext.ValidateWorld(GetWorld(), OutFailureReason)) return false;
	return CanUseLocalEconomy(OutFailureReason);
}

FRokLocalEconomyAuthoritySnapshot URokBuildingUpgradeSubsystem::GetLocalEconomyAuthoritySnapshot() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (LocalEconomyAuthorityOverride.IsSet()) return LocalEconomyAuthorityOverride.GetValue();
#endif
	FRokLocalEconomyAuthoritySnapshot Snapshot;
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const URokUnityRuntimeStateSubsystem* Runtime = Instance
		? Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	if (!Runtime || !Runtime->IsRuntimeStateLoaded()) return Snapshot;

	Snapshot.bRuntimeLoaded = true;
	Snapshot.Profile = Runtime->GetPlayerProfileState();
	return Snapshot;
}

bool URokBuildingUpgradeSubsystem::CanUseLocalEconomy(FString& OutFailureReason) const
{
	const FRokLocalEconomyAuthoritySnapshot Snapshot = GetLocalEconomyAuthoritySnapshot();
	if (!Snapshot.bRuntimeLoaded)
	{
		OutFailureReason = TEXT("本地模拟玩家状态尚未加载。");
		return false;
	}
	if (Snapshot.Profile.StateKind != TEXT("local_preview")
		|| Snapshot.Profile.bHasRoleId || !Snapshot.Profile.RoleId.IsEmpty()
		|| Snapshot.Profile.bHasCivilization || Snapshot.Profile.CivilizationId != 0
		|| !Snapshot.Profile.CivilizationName.IsEmpty())
	{
		OutFailureReason = TEXT("真实账号状态不能写入隔离的本地升级账本。");
		return false;
	}
	OutFailureReason.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void URokBuildingUpgradeSubsystem::SetLocalEconomyAuthorityForTesting(
	const FRokLocalEconomyAuthoritySnapshot& Snapshot)
{
	LocalEconomyAuthorityOverride = Snapshot;
}

void URokBuildingUpgradeSubsystem::ClearLocalEconomyAuthorityForTesting()
{
	LocalEconomyAuthorityOverride.Reset();
}
#endif

bool URokBuildingUpgradeSubsystem::PreviewLocalConstructionEntry(
	int32 UnityBuildingTypeId, FRokLocalConstructionPreview& OutPreview) const
{
	FRokLocalConstructionRequest Request;
	Request.UnityBuildingTypeId = UnityBuildingTypeId;
	return EvaluateLocalConstruction(Request, false, OutPreview);
}

bool URokBuildingUpgradeSubsystem::PreviewLocalConstruction(
	const FRokLocalConstructionRequest& Request, FRokLocalConstructionPreview& OutPreview) const
{
	return EvaluateLocalConstruction(Request, true, OutPreview);
}

bool URokBuildingUpgradeSubsystem::EvaluateLocalConstruction(
	const FRokLocalConstructionRequest& Request, bool bCheckPosition,
	FRokLocalConstructionPreview& OutPreview) const
{
	OutPreview = FRokLocalConstructionPreview();
	if (!ConstructionGrid.IsLoaded() || !bConstructionSourcesLoaded || !bUnityBuildingConfigLoaded
		|| !bUnityInitialCityStateLoaded || !LocalCityContext.IsLoaded())
	{
		OutPreview.FailureReason = TEXT("建造源格图、模型、本地上下文或建筑配置尚未完整加载。");
		return false;
	}
	if (!CanUseLocalCitySimulation(OutPreview.FailureReason)) return false;
	OutPreview.bContextVerified = true;
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	auto* Catalog = Instance ? Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	if (!Catalog || (!Catalog->IsCatalogLoaded() && !Catalog->LoadCatalog()))
	{
		OutPreview.FailureReason = TEXT("建筑数量配置尚未加载。");
		return false;
	}
	FRokUnityCatalogBuildingType BuildingType;
	if ((Request.UnityBuildingTypeId != 2 && Request.UnityBuildingTypeId != 3
		&& Request.UnityBuildingTypeId != 13)
		|| !Catalog->GetBuildingType(Request.UnityBuildingTypeId, BuildingType))
	{
		OutPreview.FailureReason = TEXT("当前仅开放农场、木材厂和研究院的本地源建造类型。");
		return true;
	}
	if ((Request.UnityBuildingTypeId == 2 || Request.UnityBuildingTypeId == 3)
		&& BuildingType.DisplayNameZhCn.IsEmpty())
	{
		OutPreview.FailureReason = TEXT("资源建筑的源中文名称配置缺失。");
		return false;
	}
	const TMap<int32, FRokUnityBuildingLevelConfig>* Levels = UnityBuildingLevelConfigs.Find(Request.UnityBuildingTypeId);
	const FRokUnityBuildingLevelConfig* LevelOne = Levels ? Levels->Find(1) : nullptr;
	if (!LevelOne || !LevelOne->bPrerequisitesValid || LevelOne->Prerequisites.Num() != 3
		|| BuildingType.Width <= 0 || BuildingType.Length <= 0 || LevelOne->BuildSeconds <= 0)
	{
		OutPreview.FailureReason = TEXT("该建筑一级源配置不完整。");
		return false;
	}
	OutPreview.Footprint = FIntPoint(BuildingType.Width, BuildingType.Length);
	OutPreview.Cost = LevelOne->Cost;
	OutPreview.BuildSeconds = LevelOne->BuildSeconds;
	OutPreview.Power = LevelOne->Power;
	if (Request.UnityBuildingTypeId == 2 || Request.UnityBuildingTypeId == 3)
	{
		if (!LevelOne->bSourceEconomyValid)
		{
			OutPreview.FailureReason = TEXT("资源建筑一级源费用或建造时长配置无效。");
			return false;
		}
		const TMap<int32, FRokUnityBuildingProduceConfig>* ProduceLevels =
			UnityBuildingProduceConfigs.Find(Request.UnityBuildingTypeId);
		const FRokUnityBuildingProduceConfig* Produce = ProduceLevels ? ProduceLevels->Find(1) : nullptr;
		if (!bUnityBuildingProduceConfigLoaded || !Produce || Produce->ProducePerHour <= 0
			|| Produce->GatherMin <= 0 || Produce->GatherMax < Produce->GatherMin)
		{
			OutPreview.FailureReason = TEXT("资源建筑一级源产出配置不完整。");
			return false;
		}
	}
	TMap<int32, int32> CompletedLevels;
	if (!GetKnownCompletedBuildingLevels(CompletedLevels))
	{
		OutPreview.FailureReason = TEXT("已拥有建筑状态尚未同步。");
		return false;
	}
	FRokUnityBuildingPrerequisite Missing;
	int32 CurrentLevel = 0;
	if (!LevelOne->CheckPrerequisites(CompletedLevels, Missing, CurrentLevel))
	{
		OutPreview.MissingTypeId = Missing.TypeId;
		OutPreview.RequiredLevel = Missing.Level;
		OutPreview.CurrentLevel = CurrentLevel;
		FRokUnityCatalogBuildingType RequiredType;
		const FString RequiredName = Catalog->GetBuildingType(Missing.TypeId, RequiredType)
			&& !RequiredType.DisplayNameZhCn.IsEmpty()
			? RequiredType.DisplayNameZhCn : FString::Printf(TEXT("建筑 %d"), Missing.TypeId);
		OutPreview.FailureReason = FString::Printf(TEXT("需要 %d 级%s（当前 %d 级）。"),
			Missing.Level, *RequiredName, CurrentLevel);
		return true;
	}
	const int32 HallLevel = CompletedLevels.FindRef(1);
	OutPreview.CountLimit = BuildingType.GetCountLimit(HallLevel);
	for (const auto& Pair : UnityInitialBuildings)
	{
		OutPreview.ExistingCount += Pair.Value.TypeId == Request.UnityBuildingTypeId ? 1 : 0;
	}
	for (const FRokLocalConstructionTicket& Ticket : LocalConstructionLedger.GetTickets())
	{
		OutPreview.ExistingCount += Ticket.TypeId == Request.UnityBuildingTypeId ? 1 : 0;
	}
	if (OutPreview.ExistingCount >= OutPreview.CountLimit)
	{
		OutPreview.FailureReason = TEXT("当前市政厅等级的建筑数量上限已达到。");
		return true;
	}
	const int32 ActualAge = ConstructionGrid.GetAgeForTownHallLevel(HallLevel);
	OutPreview.ResolvedAge = ActualAge;
	OutPreview.VisualCivilization = LocalCityContext.GetVisualCivilization();
	if (!LocalCityContext.SupportsAge(ActualAge))
	{
		OutPreview.FailureReason = TEXT("源等级已到新城市年龄，但本地地图的年龄视觉转换尚未验证。");
		return true;
	}
	if ((Request.CivilizationId != 0 && Request.CivilizationId != OutPreview.VisualCivilization)
		|| (Request.Age != 0 && Request.Age != ActualAge))
	{
		OutPreview.FailureReason = TEXT("请求的视觉变体或城市年龄与已验证本地模拟上下文不一致。");
		return true;
	}
	FRokCityConstructionModel Model;
	if (!ConstructionSources.GetModel(Request.UnityBuildingTypeId, OutPreview.VisualCivilization, ActualAge, Model)
		|| !Model.bHasBuildingTypeConfig || !Model.HasVisualSource())
	{
		OutPreview.FailureReason = TEXT("对应本地视觉变体与年龄的源模型不完整。");
		return true;
	}
	OutPreview.SourceModelId = Model.ModelId;
	if (int64(GetActiveUpgradeCount()) + LocalConstructionLedger.GetActiveCount() >= GetOwnedBuildQueueCount())
	{
		OutPreview.FailureReason = TEXT("建造队列已满。");
		return true;
	}
	if (!CanAfford(OutPreview.Cost))
	{
		OutPreview.FailureReason = TEXT("建造资源不足。");
		return true;
	}
	if (bCheckPosition)
	{
		TArray<FRokCityConstructionFootprint> LocalFootprints;
		for (const FRokLocalConstructionTicket& Ticket : LocalConstructionLedger.GetTickets())
		{
			LocalFootprints.Add(Ticket.Footprint);
		}
		const FRokCityConstructionFootprint Footprint{Request.Grid, OutPreview.Footprint};
		OutPreview.bPositionEvaluated = true;
		if (!ConstructionGrid.CheckFootprint(ActualAge, Footprint, LocalFootprints,
			OutPreview.FailureReason, &OutPreview.SourceCellIndices))
		{
			return true;
		}
	}
	OutPreview.bSourceEligible = true;
	if (bCheckPosition && (Request.UnityBuildingTypeId == 2 || Request.UnityBuildingTypeId == 3)
		&& OutPreview.VisualCivilization == 101 && ActualAge == 1)
	{
		FRokCityPlacementWorldFrame Frame;
		if (!FRokCityPlacementWorldFrame::ResolveForWorld(World, Frame, OutPreview.FailureReason)
			|| !FRokLocalConstructionVisualFactory::Preflight(World, Frame, Request,
				OutPreview, OutPreview.FailureReason))
		{
			return true;
		}
		OutPreview.bCanCommit = true;
	}
	if (OutPreview.bCanCommit) OutPreview.FailureReason.Reset();
	else OutPreview.FailureReason = TEXT("源入口条件满足；尚未选择或验证可提交的本地视觉占地。");
	return true;
}

bool URokBuildingUpgradeSubsystem::StartLocalConstruction(
	const FRokLocalConstructionRequest& Request, uint64& OutLocalConstructionId, FString& OutFailureReason)
{
	OutLocalConstructionId = 0;
	FRokLocalConstructionPreview Preview;
	if (!PreviewLocalConstruction(Request, Preview) || !Preview.bCanCommit
		|| !Preview.bSourceEligible || !Preview.bPositionEvaluated
		|| (Request.UnityBuildingTypeId != 2 && Request.UnityBuildingTypeId != 3)
		|| Preview.VisualCivilization != 101 || Preview.ResolvedAge != 1)
	{
		OutFailureReason = Preview.FailureReason.IsEmpty()
			? TEXT("本地资源建筑源条件或视觉占地尚未验证。") : Preview.FailureReason;
		return false;
	}
	FRokCityPlacementWorldFrame Frame;
	if (!FRokCityPlacementWorldFrame::ResolveForWorld(GetWorld(), Frame, OutFailureReason)) return false;
	ARokCitySpriteActor* PreparedActor = nullptr;
	if (!FRokLocalConstructionVisualFactory::Prepare(GetWorld(), Frame, Request,
		Preview, PreparedActor, OutFailureReason)) return false;
	if (!IsValid(PreparedActor) || PreparedActor->GetWorld() != GetWorld())
	{
		if (IsValid(PreparedActor)) PreparedActor->Destroy();
		OutFailureReason = TEXT("源资源建筑工厂未返回有效的本世界视觉实例。");
		return false;
	}
	FRokLocalConstructionTicket Proposed;
	Proposed.TypeId = Request.UnityBuildingTypeId;
	Proposed.Civilization = Preview.VisualCivilization;
	Proposed.Age = Preview.ResolvedAge;
	Proposed.Footprint = {Request.Grid, Preview.Footprint};
	Proposed.Cost = {Preview.Cost.Food, Preview.Cost.Wood, Preview.Cost.Stone, Preview.Cost.Gold};
	Proposed.Power = Preview.Power;
	Proposed.StartedAt = GetWorld()->GetTimeSeconds();
	Proposed.FinishesAt = Proposed.StartedAt + Preview.BuildSeconds;
	FRokLocalConstructionResources Wallet{CurrentResources.Food, CurrentResources.Wood,
		CurrentResources.Stone, CurrentResources.Gold};
	int32 InitialCount = 0;
	for (const auto& Pair : UnityInitialBuildings)
	{
		InitialCount += Pair.Value.TypeId == Proposed.TypeId ? 1 : 0;
	}
	uint64 NewId = 0;
	if (!LocalConstructionLedger.TryBegin(ConstructionGrid, Proposed, GetActiveUpgradeCount(),
		GetOwnedBuildQueueCount(), InitialCount, Preview.CountLimit, Wallet, NewId, OutFailureReason))
	{
		PreparedActor->Destroy();
		return false;
	}
	// Pure TryBegin has committed all source limits and the debit to Wallet. No
	// fallible asset load, callback, or actor spawn occurs after this point.
	CurrentResources.Food = Wallet.Food;
	CurrentResources.Wood = Wallet.Wood;
	CurrentResources.Stone = Wallet.Stone;
	CurrentResources.Gold = Wallet.Gold;
	Proposed.Id = NewId;
	PendingLocalConstructionActors.Add(NewId, PreparedActor);
	FRokLocalConstructionVisualFactory::ActivatePending(*PreparedActor, Proposed);
	OutLocalConstructionId = NewId;
	OutFailureReason.Reset();
	return true;
}

bool URokBuildingUpgradeSubsystem::GetLocalConstructionTicket(
	uint64 LocalConstructionId, FRokLocalConstructionTicket& OutTicket) const
{
	return LocalConstructionLedger.Find(LocalConstructionId, OutTicket);
}

int32 URokBuildingUpgradeSubsystem::GetActiveLocalConstructionCount() const
{
	return LocalConstructionLedger.GetActiveCount();
}

ARokCitySpriteActor* URokBuildingUpgradeSubsystem::GetLocalConstructionActor(uint64 LocalConstructionId) const
{
	if (const TWeakObjectPtr<ARokCitySpriteActor>* Pending = PendingLocalConstructionActors.Find(LocalConstructionId))
	{
		return Pending->Get();
	}
	if (const TWeakObjectPtr<AActor>* Completed = LocalConstructionActors.Find(LocalConstructionId))
	{
		return Cast<ARokCitySpriteActor>(Completed->Get());
	}
	return nullptr;
}

bool URokBuildingUpgradeSubsystem::IsLocalConstructionPending(uint64 LocalConstructionId) const
{
	return PendingLocalConstructionActors.Contains(LocalConstructionId);
}

bool URokBuildingUpgradeSubsystem::GetLocalConstructionCompletionFailure(
	uint64 LocalConstructionId, FString& OutFailureReason) const
{
	const FString* Failure = LocalConstructionCompletionFailures.Find(LocalConstructionId);
	OutFailureReason = Failure ? *Failure : FString();
	return Failure && !Failure->IsEmpty();
}

bool URokBuildingUpgradeSubsystem::RegisterCompletedLocalConstructionActor(
	uint64 LocalConstructionId, ARokCitySpriteActor* Actor, FString& OutFailureReason)
{
	FRokLocalConstructionTicket Ticket;
	if (!CanUseLocalCitySimulation(OutFailureReason)) return false;
	const TWeakObjectPtr<ARokCitySpriteActor>* Pending = PendingLocalConstructionActors.Find(LocalConstructionId);
	if (!IsValid(Actor) || Actor->GetWorld() != GetWorld()
		|| !LocalConstructionLedger.Find(LocalConstructionId, Ticket)
		|| !Ticket.bCompleted || (Ticket.TypeId != 2 && Ticket.TypeId != 3)
		|| !Pending || Pending->Get() != Actor
		|| LocalConstructionActors.Contains(LocalConstructionId))
	{
		OutFailureReason = TEXT("本地建筑尚未完成，或视觉建筑已登记。");
		return false;
	}
	if (!FRokLocalConstructionVisualFactory::ValidateHeldActor(*Actor, Ticket, OutFailureReason)) return false;
	if (Actor->Tags.Contains(TEXT("RokUnityRuntimeInteriorPreview"))
		|| ExtractTaggedInt(Actor, TEXT("UnityInitId_")) != 0
		|| Actor->GetFootprintSize() != Ticket.Footprint.Size
		|| Actor->GetGameplayTile() != Ticket.Footprint.Origin)
	{
		OutFailureReason = TEXT("建筑视觉实例不是该本地建造的源占地。");
		return false;
	}
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance
		? Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	FRokUnityCatalogBuildingType BuildingType;
	if (!Catalog || !Catalog->IsCatalogLoaded()
		|| !Catalog->GetBuildingType(Ticket.TypeId, BuildingType)
		|| BuildingType.DisplayNameZhCn.IsEmpty())
	{
		OutFailureReason = TEXT("资源建筑的源中文名称配置在完工登记时不可用。");
		return false;
	}
	FRokLocalProductionSource Production;
	if (Ticket.TypeId == 2 || Ticket.TypeId == 3)
	{
		const TMap<int32, FRokUnityBuildingProduceConfig>* Levels = UnityBuildingProduceConfigs.Find(Ticket.TypeId);
		const FRokUnityBuildingProduceConfig* Config = Levels ? Levels->Find(1) : nullptr;
		if (!Config)
		{
			OutFailureReason = TEXT("资源建筑缺少一级源产出配置。");
			return false;
		}
		Production.TypeId = Ticket.TypeId;
		Production.Level = 1;
		Production.ProducePerHour = Config->ProducePerHour;
		Production.GatherMin = Config->GatherMin;
		Production.GatherMax = Config->GatherMax;
		const int32& Rate = Ticket.TypeId == 2 ? ProductionRates.FoodPerHour : ProductionRates.WoodPerHour;
		if (int64(Rate) + Production.ProducePerHour > MAX_int32)
		{
			OutFailureReason = TEXT("本地资源建筑总产出速率溢出。");
			return false;
		}
		if (!FMath::IsFinite(Ticket.FinishesAt) || Ticket.FinishesAt < 0.0
			|| Ticket.FinishesAt > 1.0e12)
		{
			OutFailureReason = TEXT("本地资源建筑完工时钟无效。");
			return false;
		}
		const int64 CompletionSeconds = static_cast<int64>(FMath::CeilToDouble(Ticket.FinishesAt));
		if (!LocalProductionLedger.RegisterCompleted(Ticket, Production, CompletionSeconds, OutFailureReason))
		{
			return false;
		}
	}
	const FName Key(*FString::Printf(TEXT("LocalConstruction_%llu"), static_cast<unsigned long long>(LocalConstructionId)));
	FRokBuildingState State;
	State.BuildingKey = Key;
	State.DisplayName = FText::FromString(BuildingType.DisplayNameZhCn);
	State.Type = MapUnityBuildingTypeId(Ticket.TypeId);
	State.UnityInitId = 0;
	State.UnityBuildingTypeId = Ticket.TypeId;
	State.UnityGridX = Ticket.Footprint.Origin.X;
	State.UnityGridY = Ticket.Footprint.Origin.Y;
	State.Level = 1;
	State.Power = Ticket.Power;
	State.ProducePerHour = Production.ProducePerHour;
	State.GatherMin = Production.GatherMin;
	State.GatherMax = Production.GatherMax;
	State.Icon = ResolveIcon(State.Type);
	BuildingStates.Add(Key, State);
	BuildingActors.Add(Key, Actor);
	LocalConstructionActors.Add(LocalConstructionId, Actor);
	Actor->Tags.AddUnique(TEXT("RokBuilding"));
	Actor->Tags.AddUnique(TEXT("RokLocalConstructedBuilding"));
	Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("RokLocalBuildingId_%llu"), static_cast<unsigned long long>(LocalConstructionId))));
	Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("UnityBuildingType_%d"), Ticket.TypeId)));
	Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("UnityGridX_%d"), Ticket.Footprint.Origin.X)));
	Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("UnityGridY_%d"), Ticket.Footprint.Origin.Y)));
	Actor->Tags.AddUnique(TEXT("UnityBuildingLevel_1"));
	if (Ticket.TypeId == 2) ProductionRates.FoodPerHour += Production.ProducePerHour;
	if (Ticket.TypeId == 3) ProductionRates.WoodPerHour += Production.ProducePerHour;
	FRokLocalBuildingUpgradeCompleted Completed;
	Completed.BuildingKey = Key;
	Completed.UnityInitId = 0;
	Completed.UnityBuildingTypeId = Ticket.TypeId;
	Completed.PreviousLevel = 0;
	Completed.CurrentLevel = 1;
	Completed.PreviousPower = 0;
	Completed.CurrentPower = Ticket.Power;
	PendingCompletedUpgrades.Add(Completed);
	OutFailureReason.Reset();
	return true;
}

bool URokBuildingUpgradeSubsystem::QueryLocalProduction(uint64 LocalConstructionId,
	FRokCityResourceProductionResult& OutResult, FString& OutFailureReason) const
{
	OutResult = {};
	if (!CanUseLocalCitySimulation(OutFailureReason)) return false;
	const TWeakObjectPtr<AActor>* Actor = LocalConstructionActors.Find(LocalConstructionId);
	if (!Actor || !Actor->IsValid())
	{
		OutFailureReason = TEXT("本地资源建筑尚未绑定可见实例。");
		return false;
	}
	int64 NowSeconds = 0;
	if (!ResolveLocalProductionNowSeconds(LocalConstructionId, NowSeconds, OutFailureReason)) return false;
	return LocalProductionLedger.Preview(LocalConstructionId, NowSeconds, OutResult, OutFailureReason);
}

bool URokBuildingUpgradeSubsystem::CollectLocalProduction(uint64 LocalConstructionId,
	int64& OutCollected, FString& OutFailureReason)
{
	OutCollected = 0;
	if (!CanUseLocalCitySimulation(OutFailureReason)) return false;
	const TWeakObjectPtr<AActor>* Actor = LocalConstructionActors.Find(LocalConstructionId);
	if (!Actor || !Actor->IsValid())
	{
		OutFailureReason = TEXT("本地资源建筑尚未绑定可见实例。");
		return false;
	}
	int64 NowSeconds = 0;
	if (!ResolveLocalProductionNowSeconds(LocalConstructionId, NowSeconds, OutFailureReason)) return false;
	FRokLocalConstructionResources Wallet{CurrentResources.Food, CurrentResources.Wood,
		CurrentResources.Stone, CurrentResources.Gold};
	if (!LocalProductionLedger.TryCollect(LocalConstructionId, NowSeconds, Wallet,
		OutCollected, OutFailureReason)) return false;
	CurrentResources.Food = Wallet.Food;
	CurrentResources.Wood = Wallet.Wood;
	CurrentResources.Stone = Wallet.Stone;
	CurrentResources.Gold = Wallet.Gold;
	return true;
}

bool URokBuildingUpgradeSubsystem::ResolveLocalProductionNowSeconds(
	uint64 LocalConstructionId, int64& OutNowSeconds, FString& OutFailureReason) const
{
	OutNowSeconds = 0;
	FRokLocalConstructionTicket Ticket;
	if (!LocalConstructionLedger.Find(LocalConstructionId, Ticket) || !Ticket.bCompleted
		|| !LocalProductionLedger.HasClock(LocalConstructionId))
	{
		OutFailureReason = TEXT("本地资源建筑尚未完成产出时钟登记。");
		return false;
	}
	const double WorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	if (!FMath::IsFinite(WorldSeconds) || WorldSeconds < 0.0 || WorldSeconds > 1.0e12
		|| !FMath::IsFinite(Ticket.FinishesAt) || Ticket.FinishesAt < 0.0
		|| Ticket.FinishesAt > WorldSeconds)
	{
		OutFailureReason = TEXT("本地世界模拟时钟无效或早于实际完工时间。");
		return false;
	}
	const int64 WholeNow = static_cast<int64>(FMath::FloorToDouble(WorldSeconds));
	const int64 FirstProductionSecond = static_cast<int64>(FMath::CeilToDouble(Ticket.FinishesAt));
	// The local production ledger starts at ceil(fractional finish). In only
	// the sub-second interval after real completion, floor(now) would precede
	// that clock. Clamp to its start for a valid zero-production preview; after
	// the boundary retain the source floor(now) behavior and never accrue early.
	OutNowSeconds = FMath::Max(WholeNow, FirstProductionSecond);
	OutFailureReason.Reset();
	return true;
}

int32 URokBuildingUpgradeSubsystem::GetLocalCompletedBuildingPowerDelta() const
{
	int64 Delta = 0;
	for (const auto& Pair : BuildingStates)
	{
		const FRokBuildingState& State = Pair.Value;
		if (State.UnityInitId == 0 && IsRegisteredLocalConstruction(State))
		{
			if (const FRokUnityBuildingLevelConfig* Level = FindUnityBuildingConfig(State, State.Level))
			{
				Delta += Level->Power;
			}
			continue;
		}
		const FRokUnityInitialBuildingState* Initial = UnityInitialBuildings.Find(State.UnityInitId);
		if (!Initial || Initial->TypeId != State.UnityBuildingTypeId || State.Level <= Initial->Level) continue;
		// Known source identities use one state even when represented by several actors.
		if (Pair.Key != FName(*FString::Printf(TEXT("UnityInitId_%d"), Initial->InitId))) continue;
		const FRokUnityBuildingLevelConfig* Current = FindUnityBuildingConfig(State, State.Level);
		const FRokUnityBuildingLevelConfig* Baseline = FindUnityBuildingConfig(State, Initial->Level);
		if (Current && Baseline) Delta += int64(Current->Power) - Baseline->Power;
	}
	return int32(FMath::Clamp<int64>(Delta, MIN_int32, MAX_int32));
}

TStatId URokBuildingUpgradeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokBuildingUpgradeSubsystem, STATGROUP_Tickables);
}

FRokBuildingState URokBuildingUpgradeSubsystem::GetBuildingStateForActor(AActor* Actor)
{
	if (!Actor)
	{
		return FRokBuildingState();
	}

	FRokBuildingState& State = GetOrCreateBuildingState(Actor);
	RefreshUpgradeCompletion(State);
	return State;
}

bool URokBuildingUpgradeSubsystem::TryPeekExistingBuildingStateForActor(
	const AActor* Actor, FRokBuildingState& OutState) const
{
	OutState = FRokBuildingState();
	if (!IsValid(Actor) || Actor->GetWorld() != GetWorld()) return false;
	const FRokBuildingState* Match = nullptr;
	for (const TPair<FName, TWeakObjectPtr<AActor>>& Pair : BuildingActors)
	{
		if (Pair.Value.Get() != Actor) continue;
		// A visual associated with multiple ledger keys is ambiguous.
		if (Match) return false;
		Match = BuildingStates.Find(Pair.Key);
		if (!Match || Match->BuildingKey != Pair.Key) return false;
	}
	if (!Match) return false;
	OutState = *Match;
	return true;
}

bool URokBuildingUpgradeSubsystem::StartUpgrade(AActor* Actor, FString& OutFailureReason)
{
	if (!CanStartUpgrade(Actor, OutFailureReason))
	{
		return false;
	}
	if (!CanUseLocalEconomy(OutFailureReason)) return false;

	FRokBuildingState& State = GetOrCreateBuildingState(Actor);
	RefreshUpgradeCompletion(State);
	const FRokResourceCost Cost = GetNextUpgradeCost(State);
	SpendResources(Cost);

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	State.bUpgradeInProgress = true;
	State.UpgradeStartedAt = Now;
	State.UpgradeFinishesAt = Now + GetUpgradeDurationSeconds(State);
	OutFailureReason.Reset();
	return true;
}

float URokBuildingUpgradeSubsystem::GetUpgradeProgress(AActor* Actor)
{
	if (!Actor)
	{
		return 0.0f;
	}

	FRokBuildingState& State = GetOrCreateBuildingState(Actor);
	RefreshUpgradeCompletion(State);
	if (!State.bUpgradeInProgress)
	{
		return 0.0f;
	}

	const float Duration = FMath::Max(0.01f, State.UpgradeFinishesAt - State.UpgradeStartedAt);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : State.UpgradeStartedAt;
	return FMath::Clamp((Now - State.UpgradeStartedAt) / Duration, 0.0f, 1.0f);
}

float URokBuildingUpgradeSubsystem::GetRemainingUpgradeSeconds(AActor* Actor)
{
	if (!Actor)
	{
		return 0.0f;
	}

	FRokBuildingState& State = GetOrCreateBuildingState(Actor);
	RefreshUpgradeCompletion(State);
	if (!State.bUpgradeInProgress)
	{
		return 0.0f;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : State.UpgradeFinishesAt;
	return FMath::Max(0.0f, State.UpgradeFinishesAt - Now);
}

AActor* URokBuildingUpgradeSubsystem::GetFirstActiveUpgradeActor() const
{
	for (const TPair<FName, FRokBuildingState>& Pair : BuildingStates)
	{
		if (!Pair.Value.bUpgradeInProgress)
		{
			continue;
		}

		if (const TWeakObjectPtr<AActor>* Actor = BuildingActors.Find(Pair.Key))
		{
			if (Actor->IsValid())
			{
				return Actor->Get();
			}
		}
	}

	return nullptr;
}

FRokResourceCost URokBuildingUpgradeSubsystem::GetNextUpgradeCost(AActor* Actor)
{
	if (!Actor)
	{
		return FRokResourceCost();
	}
	return GetNextUpgradeCost(GetOrCreateBuildingState(Actor));
}

bool URokBuildingUpgradeSubsystem::CanStartUpgrade(AActor* Actor, FString& OutFailureReason)
{
	if (!Actor)
	{
		OutFailureReason = TEXT("No building selected.");
		return false;
	}
	if (!CanUseLocalEconomy(OutFailureReason)) return false;

	FRokBuildingState& State = GetOrCreateBuildingState(Actor);
	RefreshUpgradeCompletion(State);
	if (State.Type == ERokBuildingType::Unknown)
	{
		OutFailureReason = TEXT("Selected actor is not a recognized RoK building.");
		return false;
	}
	const int32 MaxLevel = GetMaxBuildingLevelForState(State);
	if (State.Level >= MaxLevel)
	{
		OutFailureReason = FString::Printf(TEXT("Building is already at max level %d."), MaxLevel);
		return false;
	}
	if (State.bUpgradeInProgress)
	{
		OutFailureReason = TEXT("Upgrade already in progress.");
		return false;
	}
	if (!CheckUpgradePrerequisites(State, OutFailureReason))
	{
		return false;
	}
	if (State.UnityInitId == 0
		&& (State.UnityBuildingTypeId == 2 || State.UnityBuildingTypeId == 3))
	{
		uint64 ConstructionId = 0;
		FRokLocalProductionSource CurrentProduction;
		FRokLocalProductionSource NextProduction;
		int32 NextAggregateRate = 0;
		if (!ValidateLocalResourceLevelTransition(State, ConstructionId, CurrentProduction,
			NextProduction, NextAggregateRate, OutFailureReason)) return false;
	}
	if (int64(GetActiveUpgradeCount()) + LocalConstructionLedger.GetActiveCount() >= GetMaxConcurrentUpgrades())
	{
		OutFailureReason = FString::Printf(TEXT("Construction queue is full (%d/%d)."), GetActiveUpgradeCount(), GetMaxConcurrentUpgrades());
		return false;
	}

	const FRokResourceCost Cost = GetNextUpgradeCost(State);
	if (!CanAfford(Cost))
	{
		OutFailureReason = FString::Printf(
			TEXT("Not enough resources. Need Food %d, Wood %d, Stone %d, Gold %d."),
			Cost.Food,
			Cost.Wood,
			Cost.Stone,
			Cost.Gold);
		return false;
	}

	OutFailureReason.Reset();
	return true;
}

FRokResourceStock URokBuildingUpgradeSubsystem::GetCurrentResources() const
{
	return CurrentResources;
}

int32 URokBuildingUpgradeSubsystem::GetActiveUpgradeCount() const
{
	int32 Count = 0;
	for (const TPair<FName, FRokBuildingState>& Pair : BuildingStates)
	{
		if (Pair.Value.bUpgradeInProgress)
		{
			++Count;
		}
	}
	return Count;
}

int32 URokBuildingUpgradeSubsystem::GetMaxConcurrentUpgrades() const
{
	return GetOwnedBuildQueueCount();
}

int32 URokBuildingUpgradeSubsystem::GetMaxBuildingLevel() const
{
	if (bUnityBuildingConfigLoaded)
	{
		int32 MaxLevel = 0;
		for (const TPair<int32, TMap<int32, FRokUnityBuildingLevelConfig>>& TypePair : UnityBuildingLevelConfigs)
		{
			for (const TPair<int32, FRokUnityBuildingLevelConfig>& LevelPair : TypePair.Value)
			{
				MaxLevel = FMath::Max(MaxLevel, LevelPair.Key);
			}
		}
		if (MaxLevel > 0)
		{
			return MaxLevel;
		}
	}
	return 5;
}

int32 URokBuildingUpgradeSubsystem::GetMaxBuildingLevelForActor(AActor* Actor)
{
	if (!Actor)
	{
		return GetMaxBuildingLevel();
	}

	return GetMaxBuildingLevelForState(GetOrCreateBuildingState(Actor));
}

FRokResourceRates URokBuildingUpgradeSubsystem::GetProductionRates() const
{
	return ProductionRates;
}

int32 URokBuildingUpgradeSubsystem::GetInitialCityBuildingCount() const
{
	return UnityInitialBuildings.Num();
}

TArray<int32> URokBuildingUpgradeSubsystem::GetUnityInitialBuildingTypeIds() const
{
	TArray<int32> TypeIds;
	for (const TPair<int32, FRokUnityInitialBuildingState>& Pair : UnityInitialBuildings)
	{
		if (Pair.Value.TypeId > 0 && Pair.Value.Level > 0)
		{
			TypeIds.AddUnique(Pair.Value.TypeId);
		}
	}
	TypeIds.Sort();
	return TypeIds;
}

FText URokBuildingUpgradeSubsystem::GetBuildingTypeText(ERokBuildingType Type) const
{
	switch (Type)
	{
	case ERokBuildingType::TownCenter: return NSLOCTEXT("RokBuildings", "TownCenter", "Town Center");
	case ERokBuildingType::CityWall: return NSLOCTEXT("RokBuildings", "CityWall", "City Wall");
	case ERokBuildingType::GuardTower: return NSLOCTEXT("RokBuildings", "GuardTower", "Guard Tower");
	case ERokBuildingType::Barracks: return NSLOCTEXT("RokBuildings", "Barracks", "Barracks");
	case ERokBuildingType::Stable: return NSLOCTEXT("RokBuildings", "Stable", "Stable");
	case ERokBuildingType::ArcheryRange: return NSLOCTEXT("RokBuildings", "ArcheryRange", "Archery Range");
	case ERokBuildingType::Hospital: return NSLOCTEXT("RokBuildings", "Hospital", "Hospital");
	case ERokBuildingType::Farm: return NSLOCTEXT("RokBuildings", "Farm", "Farm");
	case ERokBuildingType::Lumbermill: return NSLOCTEXT("RokBuildings", "Lumbermill", "Lumber Mill");
	case ERokBuildingType::Quarry: return NSLOCTEXT("RokBuildings", "Quarry", "Quarry");
	case ERokBuildingType::Goldmine: return NSLOCTEXT("RokBuildings", "Goldmine", "Gold Mine");
	case ERokBuildingType::Storehouse: return NSLOCTEXT("RokBuildings", "Storehouse", "Storehouse");
	case ERokBuildingType::Tavern: return NSLOCTEXT("RokBuildings", "Tavern", "Tavern");
	case ERokBuildingType::Monument: return NSLOCTEXT("RokBuildings", "Monument", "Monument");
	case ERokBuildingType::Academy: return NSLOCTEXT("RokBuildings", "Academy", "研究院");
	case ERokBuildingType::ScoutCamp: return NSLOCTEXT("RokBuildings", "ScoutCamp", "Scout Camp");
	case ERokBuildingType::SiegeWorkshop: return NSLOCTEXT("RokBuildings", "SiegeWorkshop", "Siege Workshop");
	case ERokBuildingType::AllianceCenter: return NSLOCTEXT("RokBuildings", "AllianceCenter", "Alliance Center");
	case ERokBuildingType::BuilderHut: return NSLOCTEXT("RokBuildings", "BuilderHut", "Builder Hut");
	case ERokBuildingType::NoticeBoard: return NSLOCTEXT("RokBuildings", "NoticeBoard", "Notice Board");
	case ERokBuildingType::Decoration: return NSLOCTEXT("RokBuildings", "Decoration", "Decoration");
	default: return NSLOCTEXT("RokBuildings", "Unknown", "Unknown Building");
	}
}

bool URokBuildingUpgradeSubsystem::GetUpgradePreview(
	AActor* Actor,
	FRokBuildingUpgradePreview& OutPreview)
{
	OutPreview = FRokBuildingUpgradePreview();
	if (!Actor)
	{
		return false;
	}

	FRokBuildingState& State = GetOrCreateBuildingState(Actor);
	RefreshUpgradeCompletion(State);
	const int32 TargetLevel = State.Level + 1;
	const FRokUnityBuildingLevelConfig* CurrentLevelConfig = FindUnityBuildingConfig(State, State.Level);
	const FRokUnityBuildingLevelConfig* NextLevelConfig = FindUnityBuildingConfig(State, TargetLevel);
	const FRokUnityBuildingProduceConfig* CurrentProduceConfig = FindUnityBuildingProduceConfig(State, State.Level);
	const FRokUnityBuildingProduceConfig* NextProduceConfig = FindUnityBuildingProduceConfig(State, TargetLevel);

	if (CurrentLevelConfig)
	{
		State.Power = CurrentLevelConfig->Power;
	}
	if (CurrentProduceConfig)
	{
		State.ProducePerHour = CurrentProduceConfig->ProducePerHour;
		State.GatherMin = CurrentProduceConfig->GatherMin;
		State.GatherMax = CurrentProduceConfig->GatherMax;
	}

	OutPreview.CurrentState = State;
	OutPreview.TargetLevel = TargetLevel;
	OutPreview.CurrentPower = State.Power;
	OutPreview.NextPower = NextLevelConfig ? NextLevelConfig->Power : State.Power;
	OutPreview.CurrentProducePerHour = State.ProducePerHour;
	OutPreview.NextProducePerHour = NextProduceConfig
		? NextProduceConfig->ProducePerHour
		: State.ProducePerHour;
	OutPreview.CurrentGatherMax = State.GatherMax;
	OutPreview.NextGatherMax = NextProduceConfig
		? NextProduceConfig->GatherMax
		: State.GatherMax;
	OutPreview.Cost = GetNextUpgradeCost(State);
	OutPreview.BuildSeconds = NextLevelConfig
		? FMath::Max(1, NextLevelConfig->BuildSeconds)
		: FMath::RoundToInt(GetUpgradeDurationSeconds(State));
	OutPreview.bCanStartTimedUpgrade = CanStartUpgrade(Actor, OutPreview.FailureReason);
	return TargetLevel <= GetMaxBuildingLevelForState(State);
}

int32 URokBuildingUpgradeSubsystem::GetOwnedBuildQueueCount() const
{
	// The current Unity bootstrap state has only the permanent starter worker.
	// A second construction queue is a separately purchased/server-owned state
	// and must not be inferred from the local simulation's concurrency ceiling.
	return 1;
}

float URokBuildingUpgradeSubsystem::GetBuildSpeedBonusPercent() const
{
	// Player attribute migration has not yet supplied buildSpeedMulti.
	return 0.0f;
}

FRokBuildingState& URokBuildingUpgradeSubsystem::GetOrCreateBuildingState(AActor* Actor)
{
	const FName Key = MakeBuildingKey(Actor);
	BuildingActors.FindOrAdd(Key) = Actor;
	if (FRokBuildingState* Existing = BuildingStates.Find(Key))
	{
		return *Existing;
	}

	const FString Label = StripGeneratedSuffixes(GetActorDisplayString(Actor));
	FRokBuildingState NewState;
	NewState.BuildingKey = Key;
	if (const FRokUnityInitialBuildingState* UnityState = FindUnityInitialBuilding(Actor))
	{
		NewState.UnityInitId = UnityState->InitId;
		NewState.UnityBuildingTypeId = UnityState->TypeId;
		NewState.UnityGridX = UnityState->GridX;
		NewState.UnityGridY = UnityState->GridY;
		NewState.Type = MapUnityBuildingTypeId(UnityState->TypeId);
		NewState.DisplayName = UnityState->DisplayName.IsEmpty()
			? FText::FromString(Label.Replace(TEXT("RokLayout_"), TEXT("")).Replace(TEXT("Original"), TEXT("")))
			: FText::FromString(UnityState->DisplayName);
		NewState.Level = FMath::Max(1, UnityState->Level);
		NewState.Power = UnityState->Power;
		NewState.ProducePerHour = UnityState->ProducePerHour;
		NewState.GatherMin = UnityState->GatherMin;
		NewState.GatherMax = UnityState->GatherMax;
	}
	else
	{
		const int32 TaggedTypeId = ExtractTaggedInt(Actor, TEXT("UnityBuildingType_"));
		NewState.UnityInitId = ExtractTaggedInt(Actor, TEXT("UnityInitId_"));
		// There is no source-owned initial Academy. A loose visual/type tag is
		// never evidence that a new Academy was constructed by this local ledger.
		NewState.UnityBuildingTypeId = TaggedTypeId == 13 ? 0 : TaggedTypeId;
		NewState.UnityGridX = ExtractTaggedInt(Actor, TEXT("UnityGridX_"));
		NewState.UnityGridY = ExtractTaggedInt(Actor, TEXT("UnityGridY_"));
		NewState.Level = FMath::Max(1, ExtractTaggedInt(Actor, TEXT("UnityBuildingLevel_")));
		NewState.Type = TaggedTypeId == 13 ? ERokBuildingType::Unknown
			: (TaggedTypeId > 0 ? MapUnityBuildingTypeId(TaggedTypeId) : InferBuildingType(Label));
		NewState.DisplayName = FText::FromString(Label.Replace(TEXT("RokLayout_"), TEXT("")).Replace(TEXT("Original"), TEXT("")));
	}
	NewState.Icon = ResolveIcon(NewState.Type);
	return BuildingStates.Add(Key, NewState);
}

bool URokBuildingUpgradeSubsystem::RefreshUpgradeCompletion(FRokBuildingState& State)
{
	if (!State.bUpgradeInProgress)
	{
		return false;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now >= State.UpgradeFinishesAt)
	{
		const bool bLocalResource = State.UnityInitId == 0
			&& (State.UnityBuildingTypeId == 2 || State.UnityBuildingTypeId == 3);
		uint64 ConstructionId = 0;
		FRokLocalProductionSource CurrentProduction;
		FRokLocalProductionSource NextProduction;
		int32 NextAggregateRate = 0;
		if (bLocalResource)
		{
			// BuildingResourcesProxy.GetCurrencyNum/UpdateSingleRss use the current
			// level row with retained gain-info num/changeTime after completion.
			FString Failure;
			if (!ValidateLocalResourceLevelTransition(State, ConstructionId, CurrentProduction,
				NextProduction, NextAggregateRate, Failure))
			{
				UE_LOG(LogTemp, Warning, TEXT("Local resource upgrade %s held: %s"),
					*State.BuildingKey.ToString(), *Failure);
				return false;
			}
			if (!LocalProductionLedger.TransitionLevel(ConstructionId, CurrentProduction, NextProduction, Failure))
			{
				UE_LOG(LogTemp, Warning, TEXT("Local resource upgrade %s held: %s"),
					*State.BuildingKey.ToString(), *Failure);
				return false;
			}
		}
		FRokLocalBuildingUpgradeCompleted Completed;
		Completed.BuildingKey = State.BuildingKey;
		Completed.UnityInitId = State.UnityInitId;
		Completed.UnityBuildingTypeId = State.UnityBuildingTypeId;
		Completed.PreviousLevel = State.Level;
		const FRokUnityBuildingLevelConfig* PreviousConfig = FindUnityBuildingConfig(State, State.Level);
		Completed.PreviousPower = PreviousConfig ? PreviousConfig->Power : State.Power;
		State.bUpgradeInProgress = false;
		State.UpgradeStartedAt = 0.0f;
		State.UpgradeFinishesAt = 0.0f;
		State.Level += 1;
		if (const FRokUnityBuildingLevelConfig* LevelConfig = FindUnityBuildingConfig(State, State.Level))
		{
			State.Power = LevelConfig->Power;
		}
		if (const FRokUnityBuildingProduceConfig* ProduceConfig = FindUnityBuildingProduceConfig(State, State.Level))
		{
			State.ProducePerHour = ProduceConfig->ProducePerHour;
			State.GatherMin = ProduceConfig->GatherMin;
			State.GatherMax = ProduceConfig->GatherMax;
		}
		if (bLocalResource)
		{
			if (State.UnityBuildingTypeId == 2) ProductionRates.FoodPerHour = NextAggregateRate;
			else ProductionRates.WoodPerHour = NextAggregateRate;
		}
		Completed.CurrentLevel = State.Level;
		Completed.CurrentPower = State.Power;
		PendingCompletedUpgrades.Add(Completed);
		return true;
	}

	return false;
}

FName URokBuildingUpgradeSubsystem::MakeBuildingKey(AActor* Actor) const
{
	if (!Actor)
	{
		return NAME_None;
	}
	for (const auto& Pair : LocalConstructionActors)
	{
		if (Pair.Value.Get() == Actor)
		{
			return FName(*FString::Printf(TEXT("LocalConstruction_%llu"), static_cast<unsigned long long>(Pair.Key)));
		}
	}
	if (const FRokUnityInitialBuildingState* Initial = FindUnityInitialBuilding(Actor))
	{
		return FName(*FString::Printf(TEXT("UnityInitId_%d"), Initial->InitId));
	}
	const FString Label = StripGeneratedSuffixes(GetActorDisplayString(Actor));
	if (Label.StartsWith(TEXT("UnityInitId_")) || Label.StartsWith(TEXT("LocalConstruction_")))
	{
		return FName(*FString::Printf(TEXT("UnownedActor_%u"), Actor->GetUniqueID()));
	}
	return FName(*Label);
}

FString URokBuildingUpgradeSubsystem::GetActorDisplayString(AActor* Actor) const
{
	if (!Actor)
	{
		return FString();
	}
#if WITH_EDITOR
	return Actor->GetActorLabel();
#else
	return Actor->GetName();
#endif
}

int32 URokBuildingUpgradeSubsystem::ExtractTaggedInt(AActor* Actor, const TCHAR* Prefix) const
{
	if (!Actor || !Prefix)
	{
		return 0;
	}

	const FString PrefixString(Prefix);
	for (const FName& Tag : Actor->Tags)
	{
		FString TagString = Tag.ToString();
		if (TagString.StartsWith(PrefixString))
		{
			const FString ValueString = TagString.RightChop(PrefixString.Len());
			return FCString::Atoi(*ValueString);
		}
	}
	return 0;
}

const FRokUnityInitialBuildingState* URokBuildingUpgradeSubsystem::FindUnityInitialBuilding(AActor* Actor) const
{
	const int32 InitId = ExtractTaggedInt(Actor, TEXT("UnityInitId_"));
	return InitId > 0 ? UnityInitialBuildings.Find(InitId) : nullptr;
}

ERokBuildingType URokBuildingUpgradeSubsystem::MapUnityBuildingTypeId(int32 TypeId) const
{
	switch (TypeId)
	{
	case 1: return ERokBuildingType::TownCenter;
	case 2: return ERokBuildingType::Farm;
	case 3: return ERokBuildingType::Lumbermill;
	case 4: return ERokBuildingType::Quarry;
	case 5: return ERokBuildingType::Goldmine;
	case 6: return ERokBuildingType::CityWall;
	case 7: return ERokBuildingType::BuilderHut;
	case 8: return ERokBuildingType::GuardTower;
	case 9: return ERokBuildingType::Barracks;
	case 10: return ERokBuildingType::Stable;
	case 11: return ERokBuildingType::ArcheryRange;
	case 12: return ERokBuildingType::SiegeWorkshop;
	case 13: return ERokBuildingType::Academy;
	case 14: return ERokBuildingType::Hospital;
	case 15: return ERokBuildingType::Storehouse;
	case 21: return ERokBuildingType::AllianceCenter;
	case 28: return ERokBuildingType::ScoutCamp;
	case 29: return ERokBuildingType::NoticeBoard;
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
		return ERokBuildingType::Decoration;
	default:
		return ERokBuildingType::Unknown;
	}
}

ERokBuildingType URokBuildingUpgradeSubsystem::InferBuildingType(const FString& Label) const
{
	if (Label.Contains(TEXT("TownCenter")) || Label.Contains(TEXT("MainCity")) || Label.Contains(TEXT("Castle")) || Label.Contains(TEXT("Checkpoint"))) return ERokBuildingType::TownCenter;
	if (Label.Contains(TEXT("CityWall")) || Label.Contains(TEXT("Wall"))) return ERokBuildingType::CityWall;
	if (Label.Contains(TEXT("Tower")) || Label.Contains(TEXT("Guard"))) return ERokBuildingType::GuardTower;
	if (Label.Contains(TEXT("Barracks")) || Label.Contains(TEXT("WorkMan")) || Label.Contains(TEXT("Military_Camp_01"))) return ERokBuildingType::Barracks;
	if (Label.Contains(TEXT("Stable")) || Label.Contains(TEXT("Military_Camp_02"))) return ERokBuildingType::Stable;
	if (Label.Contains(TEXT("Archery")) || Label.Contains(TEXT("Arc_"))) return ERokBuildingType::ArcheryRange;
	if (Label.Contains(TEXT("Siege")) || Label.Contains(TEXT("Military_Camp_04"))) return ERokBuildingType::SiegeWorkshop;
	if (Label.Contains(TEXT("Hospital"))) return ERokBuildingType::Hospital;
	if (Label.Contains(TEXT("Farm"))) return ERokBuildingType::Farm;
	if (Label.Contains(TEXT("Lumber")) || Label.Contains(TEXT("Wood"))) return ERokBuildingType::Lumbermill;
	if (Label.Contains(TEXT("Quarry")) || Label.Contains(TEXT("Stone"))) return ERokBuildingType::Quarry;
	if (Label.Contains(TEXT("Gold"))) return ERokBuildingType::Goldmine;
	if (Label.Contains(TEXT("Storehouse")) || Label.Contains(TEXT("Shop"))) return ERokBuildingType::Storehouse;
	if (Label.Contains(TEXT("Tavern")) || Label.Contains(TEXT("Book"))) return ERokBuildingType::Tavern;
	if (Label.Contains(TEXT("Monument")) || Label.Contains(TEXT("Hero"))) return ERokBuildingType::Monument;
	if (Label.Contains(TEXT("Scout")) || Label.Contains(TEXT("Patrol"))) return ERokBuildingType::ScoutCamp;
	if (Label.Contains(TEXT("Alliance"))) return ERokBuildingType::AllianceCenter;
	if (Label.Contains(TEXT("Worker")) || Label.Contains(TEXT("WorkMan")) || Label.Contains(TEXT("Builder"))) return ERokBuildingType::BuilderHut;
	if (Label.Contains(TEXT("Notice")) || Label.Contains(TEXT("Announcement")) || Label.Contains(TEXT("Board"))) return ERokBuildingType::NoticeBoard;
	if (Label.Contains(TEXT("Decoration")) || Label.Contains(TEXT("Decor"))) return ERokBuildingType::Decoration;
	return ERokBuildingType::Unknown;
}

TSoftObjectPtr<UTexture2D> URokBuildingUpgradeSubsystem::ResolveIcon(ERokBuildingType Type) const
{
	switch (Type)
	{
	case ERokBuildingType::TownCenter: return MakeIcon(TEXT("/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_1_101_1.UnityGameApp_build_1_101_1"));
	case ERokBuildingType::GuardTower: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/GuardTowerUI_1_5.GuardTowerUI_1_5"));
	case ERokBuildingType::Barracks: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Barracks_1_5.Barracks_1_5"));
	case ERokBuildingType::Stable: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Stable_1_5.Stable_1_5"));
	case ERokBuildingType::ArcheryRange: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Archery_1_5.Archery_1_5"));
	case ERokBuildingType::Hospital: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Hospital_1_5.Hospital_1_5"));
	case ERokBuildingType::Farm: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/FarmWindmill_ani_5_00.FarmWindmill_ani_5_00"));
	case ERokBuildingType::Lumbermill: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Lumbermill_1_5.Lumbermill_1_5"));
	case ERokBuildingType::Quarry: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Quarry_1_5.Quarry_1_5"));
	case ERokBuildingType::Goldmine: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Goldmine_1_5.Goldmine_1_5"));
	case ERokBuildingType::Storehouse: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Shop_1_4.Shop_1_4"));
	case ERokBuildingType::Tavern: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Tavern_1_5.Tavern_1_5"));
	case ERokBuildingType::Monument: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Monument_1_4.Monument_1_4"));
	case ERokBuildingType::Academy: return TSoftObjectPtr<UTexture2D>();
	case ERokBuildingType::ScoutCamp: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/ScoutCamp_1_4.ScoutCamp_1_4"));
	case ERokBuildingType::BuilderHut: return MakeIcon(TEXT("/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_7_101_1.UnityGameApp_build_7_101_1"));
	case ERokBuildingType::NoticeBoard: return MakeIcon(TEXT("/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_29_101_1.UnityGameApp_build_29_101_1"));
	case ERokBuildingType::Decoration: return MakeIcon(TEXT("/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_51_101_1_1.UnityGameApp_build_51_101_1_1"));
	default: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/city.city"));
	}
}

float URokBuildingUpgradeSubsystem::GetUpgradeDurationSeconds(const FRokBuildingState& State) const
{
	if (const FRokUnityBuildingLevelConfig* Config = FindUnityBuildingConfig(State, State.Level + 1))
	{
		return FMath::Max(1.0f, static_cast<float>(Config->BuildSeconds));
	}

	const float BaseSeconds = State.Type == ERokBuildingType::TownCenter ? 18.0f : 10.0f;
	return BaseSeconds + static_cast<float>(State.Level - 1) * 6.0f;
}

FRokResourceCost URokBuildingUpgradeSubsystem::GetNextUpgradeCost(const FRokBuildingState& State) const
{
	if (const FRokUnityBuildingLevelConfig* Config = FindUnityBuildingConfig(State, State.Level + 1))
	{
		return Config->Cost;
	}

	const int32 LevelFactor = FMath::Max(1, State.Level);
	FRokResourceCost Cost;

	switch (State.Type)
	{
	case ERokBuildingType::TownCenter:
		Cost.Food = 220 * LevelFactor;
		Cost.Wood = 240 * LevelFactor;
		Cost.Stone = 120 * LevelFactor;
		Cost.Gold = 60 * LevelFactor;
		break;
	case ERokBuildingType::CityWall:
	case ERokBuildingType::GuardTower:
		Cost.Food = 80 * LevelFactor;
		Cost.Wood = 220 * LevelFactor;
		Cost.Stone = 160 * LevelFactor;
		Cost.Gold = 25 * LevelFactor;
		break;
	case ERokBuildingType::Farm:
	case ERokBuildingType::Lumbermill:
	case ERokBuildingType::Quarry:
	case ERokBuildingType::Goldmine:
		Cost.Food = 90 * LevelFactor;
		Cost.Wood = 110 * LevelFactor;
		Cost.Stone = State.Level >= 2 ? 50 * LevelFactor : 0;
		Cost.Gold = 10 * LevelFactor;
		break;
	case ERokBuildingType::Storehouse:
		Cost.Food = 120 * LevelFactor;
		Cost.Wood = 160 * LevelFactor;
		Cost.Stone = 90 * LevelFactor;
		Cost.Gold = 20 * LevelFactor;
		break;
	default:
		Cost.Food = 140 * LevelFactor;
		Cost.Wood = 180 * LevelFactor;
		Cost.Stone = State.Level >= 2 ? 90 * LevelFactor : 40 * LevelFactor;
		Cost.Gold = 25 * LevelFactor;
		break;
	}

	return Cost;
}

int32 URokBuildingUpgradeSubsystem::GetMaxBuildingLevelForState(const FRokBuildingState& State) const
{
	const int32 TypeId = State.UnityBuildingTypeId > 0 ? State.UnityBuildingTypeId : GetUnityBuildingTypeId(State.Type);
	if (TypeId > 0)
	{
		if (const TMap<int32, FRokUnityBuildingLevelConfig>* Levels = UnityBuildingLevelConfigs.Find(TypeId))
		{
			int32 MaxLevel = 0;
			for (const TPair<int32, FRokUnityBuildingLevelConfig>& LevelPair : *Levels)
			{
				MaxLevel = FMath::Max(MaxLevel, LevelPair.Key);
			}
			if (MaxLevel > 0)
			{
				return MaxLevel;
			}
		}
	}

	return GetMaxBuildingLevel();
}

int32 URokBuildingUpgradeSubsystem::GetUnityBuildingTypeId(ERokBuildingType Type) const
{
	switch (Type)
	{
	case ERokBuildingType::TownCenter: return 1;
	case ERokBuildingType::Farm: return 2;
	case ERokBuildingType::Lumbermill: return 3;
	case ERokBuildingType::Quarry: return 4;
	case ERokBuildingType::Goldmine: return 5;
	case ERokBuildingType::CityWall: return 6;
	case ERokBuildingType::GuardTower: return 8;
	case ERokBuildingType::Barracks: return 9;
	case ERokBuildingType::Stable: return 10;
	case ERokBuildingType::ArcheryRange: return 11;
	case ERokBuildingType::SiegeWorkshop: return 12;
	case ERokBuildingType::ScoutCamp: return 28;
	case ERokBuildingType::Academy: return 13;
	case ERokBuildingType::Hospital: return 14;
	case ERokBuildingType::Storehouse: return 15;
	case ERokBuildingType::AllianceCenter: return 21;
	case ERokBuildingType::BuilderHut: return 7;
	case ERokBuildingType::NoticeBoard: return 29;
	case ERokBuildingType::Decoration: return 51;
	default: return 0;
	}
}

const FRokUnityBuildingLevelConfig* URokBuildingUpgradeSubsystem::FindUnityBuildingConfig(const FRokBuildingState& State, int32 TargetLevel) const
{
	const int32 TypeId = State.UnityBuildingTypeId > 0 ? State.UnityBuildingTypeId : GetUnityBuildingTypeId(State.Type);
	if (TypeId <= 0 || TargetLevel <= 0)
	{
		return nullptr;
	}

	const TMap<int32, FRokUnityBuildingLevelConfig>* Levels = UnityBuildingLevelConfigs.Find(TypeId);
	if (!Levels)
	{
		return nullptr;
	}
	return Levels->Find(TargetLevel);
}

const FRokUnityBuildingProduceConfig* URokBuildingUpgradeSubsystem::FindUnityBuildingProduceConfig(
	const FRokBuildingState& State,
	int32 TargetLevel) const
{
	const int32 TypeId = State.UnityBuildingTypeId > 0
		? State.UnityBuildingTypeId
		: GetUnityBuildingTypeId(State.Type);
	if (TypeId <= 0 || TargetLevel <= 0)
	{
		return nullptr;
	}

	const TMap<int32, FRokUnityBuildingProduceConfig>* Levels = UnityBuildingProduceConfigs.Find(TypeId);
	return Levels ? Levels->Find(TargetLevel) : nullptr;
}

bool URokBuildingUpgradeSubsystem::LoadUnityBuildingLevelConfig()
{
	UnityBuildingLevelConfigs.Reset();
	bUnityBuildingConfigLoaded = false;

	auto IngestRows = [this](const TArray<TSharedPtr<FJsonObject>>& Rows)
	{
		for (const TSharedPtr<FJsonObject>& Row : Rows)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			FRokUnityBuildingLevelConfig Config;
			Config.TypeId = JsonInt(Row, TEXT("type"));
			Config.Level = JsonInt(Row, TEXT("level"));
			Config.Cost.Food = JsonInt(Row, TEXT("food"));
			Config.Cost.Wood = JsonInt(Row, TEXT("wood"));
			Config.Cost.Stone = JsonInt(Row, TEXT("stone"));
			Config.Cost.Gold = JsonInt(Row, TEXT("coin"));
			Config.BuildSeconds = JsonInt(Row, TEXT("buildingTime"));
			Config.Power = JsonInt(Row, TEXT("power"));
			if (!Config.ParseSourceEconomy(Row)
				&& (Config.TypeId == 2 || Config.TypeId == 3) && Config.Level == 2)
			{
				UE_LOG(LogTemp, Warning, TEXT("Invalid local resource upgrade economy: type=%d level=%d; upgrade will be blocked."),
					Config.TypeId, Config.Level);
			}
			if (!Config.ParsePrerequisites(Row))
			{
				UE_LOG(LogTemp, Warning, TEXT("Invalid building prerequisites: type=%d level=%d; upgrade will be blocked."), Config.TypeId, Config.Level);
			}

			if (Config.TypeId > 0 && Config.Level > 0)
			{
				TMap<int32, FRokUnityBuildingLevelConfig>& Levels = UnityBuildingLevelConfigs.FindOrAdd(Config.TypeId);
				if (Levels.Contains(Config.Level)
					&& (Config.TypeId == 2 || Config.TypeId == 3) && Config.Level == 2)
				{
					Config.bSourceEconomyValid = false;
					UE_LOG(LogTemp, Warning, TEXT("Duplicate local resource upgrade economy: type=%d level=%d; upgrade will be blocked."),
						Config.TypeId, Config.Level);
				}
				Levels.Add(Config.Level, Config);
			}
		}
	};

	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (URokUnityConfigSubsystem* ConfigSubsystem = GameInstance->GetSubsystem<URokUnityConfigSubsystem>())
			{
				if (const TArray<TSharedPtr<FJsonObject>>* Rows = ConfigSubsystem->GetTableRows(TEXT("BuildingLevelData")))
				{
					IngestRows(*Rows);
					bUnityBuildingConfigLoaded = UnityBuildingLevelConfigs.Num() > 0;
					UE_LOG(LogTemp, Log, TEXT("Loaded Rok Unity building level config from registry: types=%d rows=%d"), UnityBuildingLevelConfigs.Num(), Rows->Num());
					return bUnityBuildingConfigLoaded;
				}
			}
		}
	}

	const FString ConfigPath = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("RokUnityMigration"),
		TEXT("ConfigTables"),
		TEXT("BuildingLevelData.json"));

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity building config not found: %s"), *ConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity building config is invalid JSON: %s"), *ConfigPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("rows"), Rows) || !Rows)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity building config has no rows: %s"), *ConfigPath);
		return false;
	}

	TArray<TSharedPtr<FJsonObject>> ParsedRows;
	ParsedRows.Reserve(Rows->Num());
	for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
	{
		if (!RowValue.IsValid() || RowValue->Type != EJson::Object)
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok Unity building level config contains a non-object row: %s"), *ConfigPath);
			return false;
		}
		ParsedRows.Add(RowValue->AsObject());
	}
	IngestRows(ParsedRows);

	bUnityBuildingConfigLoaded = UnityBuildingLevelConfigs.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("Loaded Rok Unity building level config: types=%d rows=%d"), UnityBuildingLevelConfigs.Num(), Rows->Num());
	return bUnityBuildingConfigLoaded;
}

bool URokBuildingUpgradeSubsystem::LoadUnityBuildingProduceConfig()
{
	UnityBuildingProduceConfigs.Reset();
	bUnityBuildingProduceConfigLoaded = false;

	auto IngestRows = [this](const TArray<TSharedPtr<FJsonObject>>& Rows)
	{
		for (const TSharedPtr<FJsonObject>& Row : Rows)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			FRokUnityBuildingProduceConfig Config;
			Config.TypeId = JsonInt(Row, TEXT("type"));
			Config.Level = JsonInt(Row, TEXT("level"));
			Config.ProducePerHour = JsonInt(Row, TEXT("produceSpeed"));
			Config.GatherMin = JsonInt(Row, TEXT("gatherMin"));
			Config.GatherMax = JsonInt(Row, TEXT("gatherMax"));
			if (Config.TypeId > 0 && Config.Level > 0)
			{
				UnityBuildingProduceConfigs.FindOrAdd(Config.TypeId).Add(Config.Level, Config);
			}
		}
	};

	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (URokUnityConfigSubsystem* ConfigSubsystem = GameInstance->GetSubsystem<URokUnityConfigSubsystem>())
			{
				if (const TArray<TSharedPtr<FJsonObject>>* Rows = ConfigSubsystem->GetTableRows(TEXT("BuildingResourcesProduce")))
				{
					IngestRows(*Rows);
					bUnityBuildingProduceConfigLoaded = UnityBuildingProduceConfigs.Num() > 0;
					UE_LOG(LogTemp, Log, TEXT("Loaded Rok Unity building production config from registry: types=%d rows=%d"), UnityBuildingProduceConfigs.Num(), Rows->Num());
					return bUnityBuildingProduceConfigLoaded;
				}
			}
		}
	}

	const FString ConfigPath = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("RokUnityMigration"),
		TEXT("ConfigTables"),
		TEXT("BuildingResourcesProduce.json"));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity building production config not found: %s"), *ConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity building production config is invalid JSON: %s"), *ConfigPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("rows"), Rows) || !Rows)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity building production config has no rows: %s"), *ConfigPath);
		return false;
	}

	TArray<TSharedPtr<FJsonObject>> ParsedRows;
	ParsedRows.Reserve(Rows->Num());
	for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
		if (Row.IsValid())
		{
			ParsedRows.Add(Row);
		}
	}
	IngestRows(ParsedRows);
	bUnityBuildingProduceConfigLoaded = UnityBuildingProduceConfigs.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("Loaded Rok Unity building production config: types=%d rows=%d"), UnityBuildingProduceConfigs.Num(), Rows->Num());
	return bUnityBuildingProduceConfigLoaded;
}

bool URokBuildingUpgradeSubsystem::LoadUnityInitialCityState()
{
	UnityInitialBuildings.Reset();
	ProductionRates = FRokResourceRates();
	bUnityInitialCityStateLoaded = false;

	const FString StatePath = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("RokUnityMigration"),
		TEXT("unity_initial_city_state.json"));

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *StatePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity initial city state not found: %s"), *StatePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity initial city state is invalid JSON: %s"), *StatePath);
		return false;
	}

	if (const TSharedPtr<FJsonObject> BootstrapResources = JsonObject(RootObject, TEXT("bootstrap_resources")))
	{
		CurrentResources.Food = JsonInt(BootstrapResources, TEXT("food"));
		CurrentResources.Wood = JsonInt(BootstrapResources, TEXT("wood"));
		CurrentResources.Stone = JsonInt(BootstrapResources, TEXT("stone"));
		CurrentResources.Gold = JsonInt(BootstrapResources, TEXT("gold"));
	}

	if (const TSharedPtr<FJsonObject> Rates = JsonObject(RootObject, TEXT("production_per_hour")))
	{
		ProductionRates.FoodPerHour = JsonInt(Rates, TEXT("food"));
		ProductionRates.WoodPerHour = JsonInt(Rates, TEXT("wood"));
		ProductionRates.StonePerHour = JsonInt(Rates, TEXT("stone"));
		ProductionRates.GoldPerHour = JsonInt(Rates, TEXT("gold"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("buildings"), Buildings) || !Buildings)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity initial city state has no buildings array: %s"), *StatePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& BuildingValue : *Buildings)
	{
		const TSharedPtr<FJsonObject> Building = BuildingValue.IsValid() ? BuildingValue->AsObject() : nullptr;
		if (!Building.IsValid())
		{
			continue;
		}

		FRokUnityInitialBuildingState InitialState;
		InitialState.InitId = JsonInt(Building, TEXT("init_id"));
		InitialState.TypeId = JsonInt(Building, TEXT("type_id"));
		InitialState.DisplayName = JsonString(Building, TEXT("display_name"));
		InitialState.Level = FMath::Max(1, JsonInt(Building, TEXT("level")));
		InitialState.bDecoration = JsonBool(Building, TEXT("is_decoration"));

		if (const TSharedPtr<FJsonObject> Grid = JsonObject(Building, TEXT("grid")))
		{
			InitialState.GridX = JsonInt(Grid, TEXT("x"));
			InitialState.GridY = JsonInt(Grid, TEXT("y"));
		}
		if (const TSharedPtr<FJsonObject> Visual = JsonObject(Building, TEXT("visual")))
		{
			InitialState.SpriteName = JsonString(Visual, TEXT("sprite_name"));
		}
		if (const TSharedPtr<FJsonObject> LevelConfig = JsonObject(Building, TEXT("level_config")))
		{
			InitialState.Power = JsonInt(LevelConfig, TEXT("power"));
		}
		if (const TSharedPtr<FJsonObject> Production = JsonObject(Building, TEXT("production")))
		{
			InitialState.ProducePerHour = JsonInt(Production, TEXT("per_hour"));
			InitialState.GatherMin = JsonInt(Production, TEXT("gather_min"));
			InitialState.GatherMax = JsonInt(Production, TEXT("gather_max"));
		}

		if (InitialState.InitId > 0)
		{
			UnityInitialBuildings.Add(InitialState.InitId, InitialState);
		}
	}

	bUnityInitialCityStateLoaded = UnityInitialBuildings.Num() > 0;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Loaded Rok Unity initial city state: buildings=%d resources=(%d,%d,%d,%d) rates=(%d,%d,%d,%d)"),
		UnityInitialBuildings.Num(),
		CurrentResources.Food,
		CurrentResources.Wood,
		CurrentResources.Stone,
		CurrentResources.Gold,
		ProductionRates.FoodPerHour,
		ProductionRates.WoodPerHour,
		ProductionRates.StonePerHour,
		ProductionRates.GoldPerHour);
	return bUnityInitialCityStateLoaded;
}

bool URokBuildingUpgradeSubsystem::GetKnownCompletedBuildingLevels(TMap<int32, int32>& OutLevels) const
{
	OutLevels.Reset();
	if (!bUnityInitialCityStateLoaded) return false;
	// The source-owned city is complete even when its visual actors have not been selected.
	// Arbitrary actor tags/labels and target queue levels never supply prerequisite ownership.
	for (const auto& Pair : UnityInitialBuildings)
	{
		const auto& Initial = Pair.Value;
		if (Initial.bDecoration) continue;
		if (Initial.InitId <= 0 || Initial.TypeId <= 0 || Initial.Level <= 0) return false;
		const FName Key(*FString::Printf(TEXT("UnityInitId_%d"), Initial.InitId));
		int32 CompletedLevel = Initial.Level;
		if (const FRokBuildingState* Current = BuildingStates.Find(Key))
		{
			if (Current->BuildingKey != Key || Current->UnityInitId != Initial.InitId
				|| Current->UnityBuildingTypeId != Initial.TypeId || Current->Level < Initial.Level) return false;
			CompletedLevel = Current->Level;
		}
		const auto* Levels = UnityBuildingLevelConfigs.Find(Initial.TypeId);
		if (!Levels || !Levels->Contains(CompletedLevel)) return false;
		int32& Highest = OutLevels.FindOrAdd(Initial.TypeId);
		Highest = FMath::Max(Highest, CompletedLevel);
	}
	for (const auto& Pair : BuildingStates)
	{
		const FRokBuildingState& State = Pair.Value;
		if (!IsRegisteredLocalConstruction(State)) continue;
		const auto* Levels = UnityBuildingLevelConfigs.Find(State.UnityBuildingTypeId);
		if (!Levels || !Levels->Contains(State.Level)) return false;
		int32& Highest = OutLevels.FindOrAdd(State.UnityBuildingTypeId);
		Highest = FMath::Max(Highest, State.Level);
	}
	return true;
}

bool URokBuildingUpgradeSubsystem::IsRegisteredLocalConstruction(const FRokBuildingState& State) const
{
	uint64 ConstructionId = 0;
	return FindRegisteredLocalConstructionId(State, ConstructionId);
}

bool URokBuildingUpgradeSubsystem::FindRegisteredLocalConstructionId(
	const FRokBuildingState& State, uint64& OutId) const
{
	OutId = 0;
	if (State.UnityInitId != 0 || State.UnityBuildingTypeId <= 0 || State.Level <= 0)
	{
		return false;
	}
	for (const FRokLocalConstructionTicket& Ticket : LocalConstructionLedger.GetTickets())
	{
		if (!Ticket.bCompleted || Ticket.TypeId != State.UnityBuildingTypeId
			|| Ticket.Footprint.Origin.X != State.UnityGridX
			|| Ticket.Footprint.Origin.Y != State.UnityGridY
			|| State.BuildingKey != FName(*FString::Printf(TEXT("LocalConstruction_%llu"),
				static_cast<unsigned long long>(Ticket.Id))))
		{
			continue;
		}
		const TWeakObjectPtr<AActor>* Actor = LocalConstructionActors.Find(Ticket.Id);
		if (!Actor || !Actor->IsValid() || Actor->Get()->GetWorld() != GetWorld()
			|| BuildingActors.FindRef(State.BuildingKey).Get() != Actor->Get())
		{
			return false;
		}
		OutId = Ticket.Id;
		return true;
	}
	return false;
}

bool URokBuildingUpgradeSubsystem::ValidateLocalResourceLevelTransition(
	const FRokBuildingState& State, uint64& OutId,
	FRokLocalProductionSource& OutCurrent, FRokLocalProductionSource& OutNext,
	int32& OutNextAggregateRate, FString& OutFailureReason) const
{
	OutId = 0;
	OutCurrent = {};
	OutNext = {};
	OutNextAggregateRate = 0;
	if (!CanUseLocalCitySimulation(OutFailureReason)) return false;
	if (!FindRegisteredLocalConstructionId(State, OutId)
		|| !LocalProductionLedger.GetSource(OutId, OutCurrent, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty()) OutFailureReason = TEXT("本地资源建筑身份或生产时钟不匹配。");
		return false;
	}
	const FRokUnityBuildingProduceConfig* CurrentConfig = FindUnityBuildingProduceConfig(State, State.Level);
	const FRokUnityBuildingProduceConfig* NextConfig = FindUnityBuildingProduceConfig(State, State.Level + 1);
	const FRokUnityBuildingLevelConfig* NextLevelConfig = FindUnityBuildingConfig(State, State.Level + 1);
	if (State.Level != 1 || !CurrentConfig || !NextConfig || !NextLevelConfig
		|| !NextLevelConfig->bSourceEconomyValid
		|| NextLevelConfig->TypeId != State.UnityBuildingTypeId
		|| NextLevelConfig->Level != State.Level + 1
		|| NextLevelConfig->BuildSeconds <= 0 || NextLevelConfig->Power < 0
		|| NextLevelConfig->Cost.Food < 0 || NextLevelConfig->Cost.Wood < 0
		|| NextLevelConfig->Cost.Stone < 0 || NextLevelConfig->Cost.Gold < 0
		|| CurrentConfig->TypeId != State.UnityBuildingTypeId || CurrentConfig->Level != State.Level
		|| NextConfig->TypeId != State.UnityBuildingTypeId || NextConfig->Level != State.Level + 1
		|| OutCurrent.TypeId != State.UnityBuildingTypeId || OutCurrent.Level != State.Level
		|| OutCurrent.ProducePerHour != CurrentConfig->ProducePerHour
		|| OutCurrent.GatherMin != CurrentConfig->GatherMin
		|| OutCurrent.GatherMax != CurrentConfig->GatherMax
		|| State.ProducePerHour != OutCurrent.ProducePerHour
		|| State.GatherMin != OutCurrent.GatherMin
		|| State.GatherMax != OutCurrent.GatherMax
		|| NextConfig->ProducePerHour <= 0 || NextConfig->GatherMin <= 0
		|| NextConfig->GatherMax < NextConfig->GatherMin)
	{
		OutFailureReason = TEXT("本地资源建筑等级或源生产配置不匹配。");
		return false;
	}
	const int32 AggregateRate = State.UnityBuildingTypeId == 2
		? ProductionRates.FoodPerHour : ProductionRates.WoodPerHour;
	const int64 CandidateRate = int64(AggregateRate) - OutCurrent.ProducePerHour
		+ NextConfig->ProducePerHour;
	if (AggregateRate < OutCurrent.ProducePerHour || CandidateRate < 0 || CandidateRate > MAX_int32)
	{
		OutFailureReason = TEXT("本地资源建筑总产出速率溢出或不匹配。");
		return false;
	}
	OutNextAggregateRate = static_cast<int32>(CandidateRate);
	OutNext.TypeId = State.UnityBuildingTypeId;
	OutNext.Level = State.Level + 1;
	OutNext.ProducePerHour = NextConfig->ProducePerHour;
	OutNext.GatherMin = NextConfig->GatherMin;
	OutNext.GatherMax = NextConfig->GatherMax;
	OutFailureReason.Reset();
	return true;
}

bool URokBuildingUpgradeSubsystem::CheckUpgradePrerequisites(const FRokBuildingState& State, FString& OutFailureReason) const
{
	const auto* Initial = UnityInitialBuildings.Find(State.UnityInitId);
	const bool bKnownInitial = Initial && Initial->TypeId == State.UnityBuildingTypeId
		&& State.BuildingKey == FName(*FString::Printf(TEXT("UnityInitId_%d"), State.UnityInitId));
	if (!bKnownInitial && !IsRegisteredLocalConstruction(State))
	{
		OutFailureReason = TEXT("建筑状态尚未同步，暂时无法升级。"); return false;
	}
	const auto* Target = FindUnityBuildingConfig(State, State.Level + 1);
	if (!Target || !Target->bPrerequisitesValid || Target->Prerequisites.Num() != 3)
	{
		OutFailureReason = TEXT("建筑前置配置尚未加载，请稍后重试。"); return false;
	}
	TMap<int32, int32> CompletedLevels;
	if (!GetKnownCompletedBuildingLevels(CompletedLevels))
	{
		OutFailureReason = TEXT("建筑状态尚未同步，暂时无法升级。"); return false;
	}
	FRokUnityBuildingPrerequisite Missing; int32 CurrentLevel = 0;
	if (Target->CheckPrerequisites(CompletedLevels, Missing, CurrentLevel)) return true;
	FString Name = GetBuildingTypeText(MapUnityBuildingTypeId(Missing.TypeId)).ToString();
	if (const UWorld* World = GetWorld())
		if (const UGameInstance* Instance = World->GetGameInstance())
			if (const auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>())
			{
				FRokUnityCatalogBuildingType Type;
				if (Catalog->GetBuildingType(Missing.TypeId, Type) && !Type.DisplayNameZhCn.IsEmpty()) Name = Type.DisplayNameZhCn;
			}
	OutFailureReason = FString::Printf(TEXT("需要 %d 级%s（当前 %d 级）"), Missing.Level, *Name, CurrentLevel);
	return false;
}

bool URokBuildingUpgradeSubsystem::CanAfford(const FRokResourceCost& Cost) const
{
	return CurrentResources.Food >= Cost.Food
		&& CurrentResources.Wood >= Cost.Wood
		&& CurrentResources.Stone >= Cost.Stone
		&& CurrentResources.Gold >= Cost.Gold;
}

void URokBuildingUpgradeSubsystem::SpendResources(const FRokResourceCost& Cost)
{
	CurrentResources.Food = FMath::Max(0, CurrentResources.Food - Cost.Food);
	CurrentResources.Wood = FMath::Max(0, CurrentResources.Wood - Cost.Wood);
	CurrentResources.Stone = FMath::Max(0, CurrentResources.Stone - Cost.Stone);
	CurrentResources.Gold = FMath::Max(0, CurrentResources.Gold - Cost.Gold);
}
