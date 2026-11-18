#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokMapTravelSubsystem.generated.h"

class FStreamableHandle;
class UWorld;
struct FWorldContext;

/** Loads the destination before tearing down the current city/world map. */
UCLASS()
class ROK_API URokMapTravelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool RequestTravel(UWorld* Source, TSoftObjectPtr<UWorld> Destination,
		const FString& Options, FSimpleDelegate OnFailure);

private:
	void CompletePreload(uint64 Generation);
	void HandlePreLoadMap(const FWorldContext& Context, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void ResetRequest(bool bFailed);

	// Keep the inactive destination alive through LoadMap's old-world GC.
	// Do not retain a world after travel: its actors belong to the new world context.
	UPROPERTY(Transient)
	TObjectPtr<UWorld> PreloadedWorld;

	TSharedPtr<FStreamableHandle> LoadHandle;
	TWeakObjectPtr<UWorld> SourceWorld;
	TSoftObjectPtr<UWorld> Target;
	FString TravelOptions;
	FSimpleDelegate FailureDelegate;
	uint64 RequestGeneration = 0;
	double RequestStartSeconds = 0.0;
	double LoadMapStartSeconds = 0.0;
	bool bPending = false;
	bool bCommitted = false;
	bool bLoadMapStarted = false;
};
