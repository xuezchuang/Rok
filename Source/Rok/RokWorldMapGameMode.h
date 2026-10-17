#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RokWorldMapGameMode.generated.h"

class UPrimitiveComponent;
class ARokWorldMapEntityActor;

UCLASS()
class ROK_API ARokWorldMapGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARokWorldMapGameMode();

	static bool ApplyWorldMapCollisionPolicy(
		const TArray<FName>& ActorTags,
		UPrimitiveComponent* PrimitiveComponent);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ARokWorldMapEntityActor>> RuntimeEntityActors;

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
