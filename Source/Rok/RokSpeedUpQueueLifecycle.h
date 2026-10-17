#pragma once

#include "CoreMinimal.h"
#include "RokBuildingUpgradeSubsystem.h"

enum class ERokSpeedUpQueueStatus : uint8 { Preview, Active, Completed, Lost, Replaced };

/** Tracks one existing local building queue. Never creates, cancels or speeds up work.
 * Unity AddSpeedMediator.OnTimerComplete closes only its bound queue at finishTime.
 * A missing/replaced UE queue must not be presented as natural completion.
 */
struct ROK_API FRokSpeedUpQueueLifecycle
{
	void Reset();
	void Bind(int32 QueueType, const FRokBuildingState* State);
	ERokSpeedUpQueueStatus Observe(const FRokBuildingState* State, double WorldSeconds);
	ERokSpeedUpQueueStatus GetStatus() const { return Status; }
	bool ConsumeNaturalCompletion();
private:
	ERokSpeedUpQueueStatus Status = ERokSpeedUpQueueStatus::Preview;
	FName BuildingKey;
	int32 InitId = 0, BuildingType = 0, StartingLevel = 0;
	double StartedAt = 0, FinishesAt = 0;
	bool bCompletionConsumed = false;
};
