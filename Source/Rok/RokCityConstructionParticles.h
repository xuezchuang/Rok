#pragma once

#include "CoreMinimal.h"

class AActor;
class UProceduralMeshComponent;
class UWorld;

class FRokCityConstructionParticles
{
public:
	bool Load();
	void Play(UWorld& World, int32 BuildingIndex, FName Name, const FVector& Location, double Scale = 1.0);
	void Stop(int32 BuildingIndex);
	void Update(double Now);
	void Reset();
	AActor* Find(int32 BuildingIndex, FName Name) const;
	FVector GetCompletionOffset(FName Model) const { return CompletionOffsets.FindRef(Model); }

private:
	struct FGeometry
	{
		TArray<FVector> Points;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<int32> Indices;
	};
	struct FEffect
	{
		TArray<FSoftObjectPath> Materials;
		TArray<TArray<FGeometry>> Frames;
	};
	struct FPlayback
	{
		int32 BuildingIndex = 0;
		FName Name;
		TWeakObjectPtr<AActor> Actor;
		TArray<TWeakObjectPtr<UProceduralMeshComponent>> Emitters;
		int32 LastFrame = INDEX_NONE;
		double StartedAt = 0.0;
	};
	TMap<FName, FEffect> Effects;
	TMap<FName, FVector> CompletionOffsets;
	TArray<FPlayback> Active;
};
