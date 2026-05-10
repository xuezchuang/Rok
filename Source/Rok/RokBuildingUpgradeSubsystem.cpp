#include "RokBuildingUpgradeSubsystem.h"

#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Stats/Stats.h"

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
}

void URokBuildingUpgradeSubsystem::Tick(float DeltaTime)
{
	for (TPair<FName, FRokBuildingState>& Pair : BuildingStates)
	{
		RefreshUpgradeCompletion(Pair.Value);
	}
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

bool URokBuildingUpgradeSubsystem::StartUpgrade(AActor* Actor, FString& OutFailureReason)
{
	if (!CanStartUpgrade(Actor, OutFailureReason))
	{
		return false;
	}

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

	FRokBuildingState& State = GetOrCreateBuildingState(Actor);
	RefreshUpgradeCompletion(State);
	if (State.Type == ERokBuildingType::Unknown)
	{
		OutFailureReason = TEXT("Selected actor is not a recognized RoK building.");
		return false;
	}
	if (State.Level >= GetMaxBuildingLevel())
	{
		OutFailureReason = FString::Printf(TEXT("Building is already at max level %d."), GetMaxBuildingLevel());
		return false;
	}
	if (State.bUpgradeInProgress)
	{
		OutFailureReason = TEXT("Upgrade already in progress.");
		return false;
	}
	if (GetActiveUpgradeCount() >= GetMaxConcurrentUpgrades())
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
	return 2;
}

int32 URokBuildingUpgradeSubsystem::GetMaxBuildingLevel() const
{
	return 5;
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
	case ERokBuildingType::ScoutCamp: return NSLOCTEXT("RokBuildings", "ScoutCamp", "Scout Camp");
	case ERokBuildingType::AllianceCenter: return NSLOCTEXT("RokBuildings", "AllianceCenter", "Alliance Center");
	default: return NSLOCTEXT("RokBuildings", "Unknown", "Unknown Building");
	}
}

FRokBuildingState& URokBuildingUpgradeSubsystem::GetOrCreateBuildingState(AActor* Actor)
{
	const FName Key = MakeBuildingKey(Actor);
	if (FRokBuildingState* Existing = BuildingStates.Find(Key))
	{
		return *Existing;
	}

	const FString Label = StripGeneratedSuffixes(GetActorDisplayString(Actor));
	FRokBuildingState NewState;
	NewState.BuildingKey = Key;
	NewState.Type = InferBuildingType(Label);
	NewState.DisplayName = FText::FromString(Label.Replace(TEXT("RokLayout_"), TEXT("")).Replace(TEXT("Original"), TEXT("")));
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
		State.bUpgradeInProgress = false;
		State.UpgradeStartedAt = 0.0f;
		State.UpgradeFinishesAt = 0.0f;
		State.Level += 1;
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
	return FName(*StripGeneratedSuffixes(GetActorDisplayString(Actor)));
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

ERokBuildingType URokBuildingUpgradeSubsystem::InferBuildingType(const FString& Label) const
{
	if (Label.Contains(TEXT("TownCenter")) || Label.Contains(TEXT("MainCity")) || Label.Contains(TEXT("Castle")) || Label.Contains(TEXT("Checkpoint"))) return ERokBuildingType::TownCenter;
	if (Label.Contains(TEXT("CityWall")) || Label.Contains(TEXT("Wall"))) return ERokBuildingType::CityWall;
	if (Label.Contains(TEXT("Tower")) || Label.Contains(TEXT("Guard"))) return ERokBuildingType::GuardTower;
	if (Label.Contains(TEXT("Barracks")) || Label.Contains(TEXT("WorkMan")) || Label.Contains(TEXT("Military_Camp_01"))) return ERokBuildingType::Barracks;
	if (Label.Contains(TEXT("Stable")) || Label.Contains(TEXT("Military_Camp_02"))) return ERokBuildingType::Stable;
	if (Label.Contains(TEXT("Archery")) || Label.Contains(TEXT("Arc_"))) return ERokBuildingType::ArcheryRange;
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
	return ERokBuildingType::Unknown;
}

TSoftObjectPtr<UTexture2D> URokBuildingUpgradeSubsystem::ResolveIcon(ERokBuildingType Type) const
{
	switch (Type)
	{
	case ERokBuildingType::TownCenter: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/Castle_1_5.Castle_1_5"));
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
	case ERokBuildingType::ScoutCamp: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/ScoutCamp_1_4.ScoutCamp_1_4"));
	default: return MakeIcon(TEXT("/Game/RokPrototype/Textures/ReferenceSprites/city.city"));
	}
}

float URokBuildingUpgradeSubsystem::GetUpgradeDurationSeconds(const FRokBuildingState& State) const
{
	const float BaseSeconds = State.Type == ERokBuildingType::TownCenter ? 18.0f : 10.0f;
	return BaseSeconds + static_cast<float>(State.Level - 1) * 6.0f;
}

FRokResourceCost URokBuildingUpgradeSubsystem::GetNextUpgradeCost(const FRokBuildingState& State) const
{
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
