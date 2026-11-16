#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokCityCitizenActor.generated.h"

class UMaterialInstanceDynamic;
struct FRokSpriteClip;
class USceneComponent;
class UStaticMeshComponent;

// Animated, non-interactive city resident or training-yard soldier.
UCLASS()
class ROK_API ARokCityCitizenActor : public AActor
{
	GENERATED_BODY()

public:
	ARokCityCitizenActor();

	virtual void Tick(float DeltaSeconds) override;

	bool ConfigureWalker(const FString& Model, const FVector& Start, const FVector& End,
		const FTransform& InCityFrame, int32 InSeed);
	bool ConfigureSoldier(const FString& Model, const FVector& Position, const FVector2D& Heading,
		int32 InSeed);
	void SetTraining(bool bTraining);
	bool IsTraining() const { return bTraining; }

	// Deterministic target for (seed, step); pure so the wander stays testable.
	static FVector EvaluateWanderTarget(
		const FVector& Home,
		float RadiusCm,
		int32 Seed,
		int32 Step);

private:
	bool ConfigureSprite(const FString& Directory, const FString& Model, const TCHAR* Pose, int32 InSeed);
	void SetHeading(const FVector2D& Heading);
	void UpdateSprite();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SpriteMeshComponent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SpriteMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2D> LastFrame;

	TSharedPtr<FRokSpriteClip> Clip;
	TSharedPtr<FRokSpriteClip> TrainingClip;
	FTransform CityFrame;
	FVector RouteStart = FVector::ZeroVector;
	FVector RouteEnd = FVector::ZeroVector;
	float AnimationElapsed = 0.0f;
	int32 Facing = 0;
	bool bWalking = false;
	bool bTraining = false;
	bool bTowardEnd = true;
};
