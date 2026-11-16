#pragma once

#include "RokCityConstructionGrid.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityLifeSubsystem.generated.h"

class ARokCityCitizenActor;

/** Spawns the city pedestrians and the barracks yard presentation. */
UCLASS()
class URokCityLifeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	bool TryGetBarracksIdleMarkerLocation(FVector& OutLocation) const;
	void Tick(float DeltaSeconds) override;
	TStatId GetStatId() const override;
	void Deinitialize() override;
	bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
	void ClearPeople();

	FRokCityConstructionGrid Grid;
	TArray<TWeakObjectPtr<ARokCityCitizenActor>> People;
	TArray<TWeakObjectPtr<ARokCityCitizenActor>> Soldiers;
	uint32 LayoutHash = 0;
	float RefreshDelay = 0.0f;
	bool bHasLayout = false;
};
