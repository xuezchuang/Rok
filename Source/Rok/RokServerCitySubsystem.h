#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokCityConstructionParticles.h"
#include "RokCityStructurePresentation.h"
#include "RokServerBuildQueue.h"
#include "RokServerCitySubsystem.generated.h"

class ARokCitySpriteActor;

// One Build_BuildingInfo.buildingInfo row (Unity BuildingInfoEntity / CityBuildingProxy).
struct FRokServerBuilding
{
	int32 Index = 0;  // buildingIndex; 1-26 are the source initial buildings (UnityInitId_N)
	int32 Type = 0;   // BuildingTypeConfig type
	int32 Level = 0;  // 0 while a new building is still under construction
	FIntPoint Pos = FIntPoint::ZeroValue; // CityGlobalMediator grid origin, same as UnityGridX/Y
	int64 FinishTime = -1; // -1 when no build or upgrade is running

	bool operator==(const FRokServerBuilding& Other) const
	{
		return Index == Other.Index && Type == Other.Type && Level == Other.Level
			&& Pos == Other.Pos && FinishTime == Other.FinishTime;
	}
};

namespace RokServerCity
{
	// Parses the merged buildingInfo collection (keyed by buildingIndex), sorted by index.
	ROK_API bool ParseBuildings(const TSharedPtr<FJsonObject>& BuildingInfo,
		TArray<FRokServerBuilding>& Out, FString& OutError);
}

// Shows the server's city instead of the source initial city while UE is logged in
// (-RokNet). Unity builds the city from Build_BuildingInfo in CityBuildingProxy and
// places each building at its pos with CityGlobalMediator; this subsystem does the same
// for the UE map: initial buildings (index 1-26) keep their source actors, moved to the
// server pos and set to the server level, and player-built buildings (27+) are spawned
// from FRokLocalConstructionVisualFactory's source visual table. It only reads server
// state and supplies authoritative building identities to the command path.
UCLASS()
class ROK_API URokServerCitySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// The city frame resolved before the server layout moved any source actor. Once a
	// server city is applied the source-footprint check in ResolveForWorld no longer
	// holds, so FRokCityPlacementWorldFrame::ResolveForWorld returns this frame instead.
	bool GetAppliedFrame(FRokCityPlacementWorldFrame& OutFrame) const;
	bool HasAppliedServerCity() const { return !AppliedBuildings.IsEmpty(); }
	const TArray<FRokServerBuilding>& GetAppliedBuildings() const { return AppliedBuildings; }
	ARokCitySpriteActor* FindSpawnedBuilding(int32 Index) const;
	ARokCitySpriteActor* FindConstructionBody(int32 Index) const;
	AActor* FindBuildingBarAnchor(int32 Index) const;
	AActor* FindConstructionScaffold(int32 Index) const { return ConstructionScaffolds.FindRef(Index).Get(); }
	const FRokServerBuilding* FindBuildingForActor(const AActor* Actor) const;
	FRokServerBuildQueueSnapshot GetBuildQueueSnapshot() const;
#if WITH_DEV_AUTOMATION_TESTS
	AActor* FindConstructionEffectForTesting(int32 Index, FName Name) const { return Particles.Find(Index, Name); }
	void SetBuildQueueSnapshotForTesting(const FRokServerBuildQueueSnapshot& Snapshot) { QueueSnapshotOverride = Snapshot; }
	void ClearBuildQueueSnapshotForTesting() { QueueSnapshotOverride.Reset(); }
#endif

	// Applies a parsed server city to the world. Public so tests can drive it without
	// a server.
	bool ApplyBuildings(const TArray<FRokServerBuilding>& Buildings, FString& OutError);

private:
	friend class FRokResearchWidgetTest;
	bool CacheFrame(FString& OutError);
	AActor* ApplyBoundary(const FRokServerBuilding& Building, const FString& Model);
	void SetMapBoundaryVisible(int32 Type, bool bVisible);
	void UpdateConstructionScaffold(const FRokServerBuilding& Building, int32 Length,
		const FVector& Location, bool bHasShelf);
	void ApplyConstructionEffects(const TArray<FRokServerBuilding>& Buildings, int32 Civilization, int32 HallLevel);

	FRokCityPlacementWorldFrame Frame;
	bool bFrameCached = false;
	TArray<FRokServerBuilding> AppliedBuildings;
	int32 AppliedCivilizationId = 0;
	// Where each source initial building's actors are now (starts at the source origin).
	TMap<int32, FIntPoint> InitialOrigins;
	TMap<int32, FIntPoint> InitialSizes;
	TMap<int32, TWeakObjectPtr<ARokCitySpriteActor>> SpawnedBuildings;
	// Level-zero queue anchors are independent of the visual shelf and its animation.
	TMap<int32, TWeakObjectPtr<AActor>> ConstructionAnchors;
	TMap<int32, TWeakObjectPtr<ARokCitySpriteActor>> ConstructionBodies;
	TMap<int32, TWeakObjectPtr<AActor>> BoundaryActors;
	TMap<int32, TWeakObjectPtr<AActor>> ConstructionScaffolds;
	TArray<TWeakObjectPtr<AActor>> MapWallActors;
	TArray<TWeakObjectPtr<AActor>> MapTowerActors;
	FRokCityStructurePresentation Structures;
	FRokCityConstructionParticles Particles;
	TMap<int32, TWeakObjectPtr<AActor>> BuildingBarAnchors;
	TSet<int32> ReportedMissingVisuals;
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<FRokServerBuildQueueSnapshot> QueueSnapshotOverride;
#endif
	double NextPollSeconds = 0.0;
	FString LastFailure;
	FString LastQueueSummary;
};
