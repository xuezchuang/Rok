#pragma once

#include "CoreMinimal.h"
#include "RokCityConstructionGrid.h"

class APlayerController;
class UWorld;

/** Verified mapping of source city-local (Unity X,Z,Y) to the current UE city. */
struct ROK_API FRokCityPlacementWorldFrame
{
	static bool ResolveForWorld(const UWorld* World, FRokCityPlacementWorldFrame& OutFrame,
		FString& OutError);

	bool TryScreenToGround(const APlayerController& Controller, const FVector2D& ScreenPoint,
		FVector& OutWorldHit, FString& OutError) const;
	bool GridCenterToWorld(const FRokCityConstructionFootprint& Footprint,
		FVector& OutWorldCenter, FString& OutError) const;

	const FTransform& GetCityContainerTransform() const { return CityContainer; }
	int32 GetGridSize() const { return GridSize; }
	bool IsValid() const { return bValid; }

private:
	FTransform CityContainer = FTransform::Identity;
	int32 GridSize = 0;
	bool bValid = false;
};
