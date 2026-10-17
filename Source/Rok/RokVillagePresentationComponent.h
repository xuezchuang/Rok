#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RokVillagePresentationData.h"
#include "RokVillageDayNight.h"
#include "RokVillagePresentationComponent.generated.h"

class UStaticMeshComponent;
class UTexture2D;

UCLASS()
class ROK_API URokVillagePresentationComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	bool InitializeSource();
	void UpdatePresentation(float Dxf, bool bShowDetail);
	float GetSourceScale() const { return State.LastScale; }
	TArray<FString> GetActiveSourceItems() const;
	// Local source-derived post-delay snapshot only. Does not consult server
	// time or schedule NightingObject's random coroutine/OnEnable behavior.
	bool ApplyLocalDayNightSample(const FRokVillageDayNightSample& Sample);
	bool HasLocalDayNightSample() const { return bHasDayNightSample; }
	const FRokVillageDayNightResult& GetLocalDayNightState() const { return DayNightState; }
private:
	FRokVillageDayNightData DayNightData;
	FRokVillageDayNightResult DayNightState;
	bool bDayNightDataLoaded = false;
	bool bHasDayNightSample = false;
	FRokVillagePresentationData Data;
	FRokVillagePresentationState State;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
};
