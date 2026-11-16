#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokServerArmySubsystem.h"
#include "RokWorldMapTypes.h"
#include "RokWorldMapArmyActor.generated.h"

class FJsonObject;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class URokGameNetSubsystem;
struct FRokSpriteClip;

// Presentation of an owned Map_ObjectInfo army. Server data stays in the net subsystem.
UCLASS()
class ARokWorldMapArmyActor : public AActor
{
	GENERATED_BODY()

public:
	ARokWorldMapArmyActor();
	bool UpdateFromServer(const TSharedPtr<FJsonObject>& Row, const FRokWorldMapProjection& InProjection,
		const URokGameNetSubsystem& Net);
	bool IsFormationVisible() const { return !IsHidden() && bHasVisibleCards; }

protected:
	void Tick(float DeltaSeconds) override;

private:
	struct FUnit
	{
		FString Model;
		FVector2D Offset = FVector2D::ZeroVector;
		TWeakObjectPtr<UStaticMeshComponent> Card;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		TWeakObjectPtr<UTexture2D> LastTexture;
		bool operator==(const FUnit& Other) const { return Model == Other.Model && Offset == Other.Offset; }
	};

	void UpdateVisuals();
	void BuildCards(TArray<FUnit> Desired);
	TSharedPtr<FRokSpriteClip> GetClip(const FString& Model, const TCHAR* Pose);

	TWeakObjectPtr<const URokGameNetSubsystem> Network;
	TArray<FUnit> Units;
	TMap<FString, TSharedPtr<FRokSpriteClip>> Clips;
	TArray<FVector2D> Path;
	FRokWorldMapProjection Projection;
	FVector2D ServerPosition = FVector2D::ZeroVector;
	FVector2D Heading = FVector2D(0, 1);
	int64 StartTime = 0;
	int64 ArrivalTime = 0;
	int64 ObjectId = 0;
	double PathLength = 0;
	double AnimationElapsed = 0;
	ERokServerMarchPhase Phase = ERokServerMarchPhase::Unknown;
	bool bHasVisibleCards = false;
};
