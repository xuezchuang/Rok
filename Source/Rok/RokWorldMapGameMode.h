#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RokWorldMapGameMode.generated.h"

class UPrimitiveComponent;
class ARokWorldMapEntityActor;
class ARokWorldMapArmyActor;
class ARokWorldMapCityActor;
class ARokWorldMapConfigActor;
class URokGameNetSubsystem;

USTRUCT(BlueprintType)
struct FRokWorldMapServerPopulation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bServerMode = false;
	UPROPERTY(BlueprintReadOnly) bool bRoleReady = false;
	UPROPERTY(BlueprintReadOnly) int32 ActiveObjects = 0;
	UPROPERTY(BlueprintReadOnly) int32 EntityActors = 0;
	UPROPERTY(BlueprintReadOnly) int32 OwnCityObjects = 0;
	UPROPERTY(BlueprintReadOnly) TMap<int32, int32> UnmappedTypes;
};

UCLASS()
class ROK_API ARokWorldMapGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARokWorldMapGameMode();

	UFUNCTION(BlueprintPure, Category = "Rok|World Map")
	FRokWorldMapServerPopulation GetServerPopulation() const { return ServerPopulation; }
	bool HasVisibleArmyFormation(int64 ObjectId) const;

	static bool ApplyWorldMapCollisionPolicy(
		const TArray<FName>& ActorTags,
		UPrimitiveComponent* PrimitiveComponent);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ARokWorldMapEntityActor>> RuntimeEntityActors;

	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<ARokWorldMapEntityActor>> ServerEntityActors;

	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<ARokWorldMapArmyActor>> ServerArmyActors;

	TWeakObjectPtr<ARokWorldMapCityActor> PlayerCityActor;
	TWeakObjectPtr<ARokWorldMapConfigActor> ServerMapConfig;
	FRokWorldMapServerPopulation ServerPopulation;
	uint64 LastAoiGeneration = MAX_uint64;
	uint64 LastAoiRevision = MAX_uint64;
	float ServerPopulationElapsed = 0.0f;
	bool bServerHomePositionApplied = false;

	void StartServerPopulation();
	void UpdateServerPopulation();
	void ApplyServerHomePosition(const URokGameNetSubsystem& Net);
	void ClearServerPopulation();

	double AutomationCaptureDeadlineSeconds = 0.0;
	double AutomationSelectionDeadlineSeconds = 0.0;
	double AutomationCityEntryNextStepSeconds = 0.0;
	double AutomationFlowExitDeadlineSeconds = 0.0;
	int32 AutomationCityEntryStepCount = 0;

	void NormalizeWorldMapCollision();
	void SpawnWorldMapPopulation();
	void SelectAutomationWorldMapCity();
	void DriveAutomationPlayerCityEntry();
	void CaptureDelayedAutomationScreenshot();
};
