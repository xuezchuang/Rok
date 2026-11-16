#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// QueueInfo times are Unix seconds; RoleInfo/Role_Heart serverTime is milliseconds.
struct FRokServerBuildQueue
{
	int32 QueueIndex = 0;
	int32 BuildingIndex = 0;
	int64 FinishTime = -1;
	int64 BeginTime = 0;
	int64 FirstFinishTime = 0;
	int64 ExpiredTime = 0;
};

enum class ERokServerBuildQueueState : uint8 { Unknown, Locked, Idle, Busy };

struct FRokServerBuildQueueView
{
	int32 QueueIndex = 0;
	int32 BuildingIndex = 0;
	ERokServerBuildQueueState State = ERokServerBuildQueueState::Unknown;
	bool bCountdownKnown = false;
	bool bWaitingForServer = false;
	bool bProgressKnown = false;
	bool bNewBuilding = false;
	double RemainingSeconds = 0.0;
	double TotalSeconds = 0.0;
	double LeaseRemainingSeconds = 0.0; // -1 = permanent; meaningful with synchronized time
	float Progress = 0.0f;
	FString BuildingName;
	int32 TargetLevel = 0;
};

struct FRokServerBuildQueueSnapshot
{
	// Available means the server owns this view, even while fields/time are pending.
	bool bAvailable = false;
	bool bTimeSynchronized = false;
	TArray<FRokServerBuildQueueView> Queues;

	const FRokServerBuildQueueView* FindQueue(int32 Index) const
	{
		return Queues.FindByPredicate([Index](const FRokServerBuildQueueView& Row) { return Row.QueueIndex == Index; });
	}
};

namespace RokServerBuildQueue
{
	// Accept both wire-decoded arrays and keyed merged collections. Rejected input
	// leaves no partial rows, so a malformed push cannot become an invented idle queue.
	ROK_API bool Parse(const TSharedPtr<FJsonObject>& RoleInfo,
		TArray<FRokServerBuildQueue>& OutQueues, FString& OutError);
	ROK_API FRokServerBuildQueueSnapshot Project(const TArray<FRokServerBuildQueue>& Queues,
		bool bHasQueueData, bool bHasServerTime, double ServerNowSeconds);
}
