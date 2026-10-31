#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokCityCitizenActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/**
 * City-life preview: a worker sprite that wanders between deterministic points
 * inside the city (Unity `citizen.prefab` + `People.cs` walkers). Presentation
 * only: it owns no gameplay state, never touches the economy or the server.
 */
UCLASS()
class ROK_API ARokCityCitizenActor : public AActor
{
	GENERATED_BODY()

public:
	ARokCityCitizenActor();

	virtual void Tick(float DeltaSeconds) override;

	void ConfigureCitizen(
		UMaterialInterface* Material,
		float SizeCm,
		float SpeedCmPerSecond,
		const FVector& InHome,
		int32 InSeed);

	// Deterministic target for (seed, step); pure so the wander stays testable.
	static FVector EvaluateWanderTarget(
		const FVector& Home,
		float RadiusCm,
		int32 Seed,
		int32 Step);

	FVector GetWanderTarget() const { return WanderTarget; }
	int32 GetWanderStep() const { return Step; }

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SpriteMeshComponent;

	FVector HomeLocation = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;
	float WanderRadius = 900.0f;
	float MoveSpeed = 130.0f;
	int32 Seed = 0;
	int32 Step = 0;

	void PickNextTarget();
	void FaceCamera();
};
