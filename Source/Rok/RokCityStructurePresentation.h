#pragma once

#include "CoreMinimal.h"

class AActor;
class UStaticMeshComponent;
class UWorld;

class FRokCityStructurePresentation
{
public:
	bool Load();
	AActor* Spawn(UWorld& World, const FString& Model, bool bBoundary, const FVector& Location);
	void Update(UWorld& World);
	void Reset();

private:
	struct FPart
	{
		FSoftObjectPath Mesh;
		FSoftObjectPath Material;
		TArray<FTransform> Frames;
		int32 Order = 0;
		bool bSprite = false;
	};
	struct FAnimation
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		const TArray<FTransform>* Frames = nullptr;
		double StartedAt = 0.0;
	};
	TMap<FString, TArray<FPart>> Models;
	TArray<FAnimation> Animations;
	TArray<TWeakObjectPtr<AActor>> ScaledActors;
};
