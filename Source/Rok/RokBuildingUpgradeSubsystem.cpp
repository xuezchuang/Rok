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

void URokBuildingUpgradeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadUnityBuildingLevelConfig();
	LoadUnityInitialCityState();
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
	case ERokBuildingType::BuilderHut: return NSLOCTEXT("RokBuildings", "BuilderHut", "Builder Hut");
	case ERokBuildingType::NoticeBoard: return NSLOCTEXT("RokBuildings", "NoticeBoard", "Notice Board");
	case ERokBuildingType::Decoration: return NSLOCTEXT("RokBuildings", "Decoration", "Decoration");
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
		NewState.UnityBuildingTypeId = TaggedTypeId;
		NewState.UnityGridX = ExtractTaggedInt(Actor, TEXT("UnityGridX_"));
		NewState.UnityGridY = ExtractTaggedInt(Actor, TEXT("UnityGridY_"));
		NewState.Level = FMath::Max(1, ExtractTaggedInt(Actor, TEXT("UnityBuildingLevel_")));
		NewState.Type = TaggedTypeId > 0 ? MapUnityBuildingTypeId(TaggedTypeId) : InferBuildingType(Label);
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
	case 12: return ERokBuildingType::ScoutCamp;
	case 13: return ERokBuildingType::Monument;
	case 14: return ERokBuildingType::Hospital;
	case 15: return ERokBuildingType::Storehouse;
	case 21: return ERokBuildingType::AllianceCenter;
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
	case ERokBuildingType::ScoutCamp: return 12;
	case ERokBuildingType::Tavern: return 13;
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

			if (Config.TypeId > 0 && Config.Level > 0)
			{
				UnityBuildingLevelConfigs.FindOrAdd(Config.TypeId).Add(Config.Level, Config);
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
		const TSharedPtr<FJsonObject> Row = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
		if (Row.IsValid())
		{
			ParsedRows.Add(Row);
		}
	}
	IngestRows(ParsedRows);

	bUnityBuildingConfigLoaded = UnityBuildingLevelConfigs.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("Loaded Rok Unity building level config: types=%d rows=%d"), UnityBuildingLevelConfigs.Num(), Rows->Num());
	return bUnityBuildingConfigLoaded;
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
