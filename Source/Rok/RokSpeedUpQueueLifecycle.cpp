#include "RokSpeedUpQueueLifecycle.h"

void FRokSpeedUpQueueLifecycle::Reset() { *this = FRokSpeedUpQueueLifecycle(); }

void FRokSpeedUpQueueLifecycle::Bind(int32 QueueType, const FRokBuildingState* State)
{
	Reset();
	// Other queue types are still source catalog previews, not local training queues.
	if (QueueType != 20102 || !State || !State->bUpgradeInProgress) return;
	if (State->BuildingKey.IsNone() || State->UnityInitId <= 0 || State->UnityBuildingTypeId <= 0
		|| State->Level <= 0 || !FMath::IsFinite(State->UpgradeStartedAt)
		|| !FMath::IsFinite(State->UpgradeFinishesAt) || State->UpgradeStartedAt < 0
		|| State->UpgradeFinishesAt <= State->UpgradeStartedAt)
	{ Status = ERokSpeedUpQueueStatus::Lost; return; }
	BuildingKey = State->BuildingKey; InitId = State->UnityInitId;
	BuildingType = State->UnityBuildingTypeId; StartingLevel = State->Level;
	StartedAt = State->UpgradeStartedAt; FinishesAt = State->UpgradeFinishesAt;
	Status = ERokSpeedUpQueueStatus::Active;
}

ERokSpeedUpQueueStatus FRokSpeedUpQueueLifecycle::Observe(const FRokBuildingState* State, double WorldSeconds)
{
	if (Status != ERokSpeedUpQueueStatus::Active) return Status;
	if (!State || !FMath::IsFinite(WorldSeconds) || WorldSeconds < 0)
		return Status = ERokSpeedUpQueueStatus::Lost;
	// BuildingUpgradeSubsystem stores timestamps and evaluates completion in float.
	// Match that exact clock projection; a raw double can be one half-ULP below a committed deadline.
	const float QueueSeconds = static_cast<float>(WorldSeconds);
	if (!FMath::IsFinite(QueueSeconds) || QueueSeconds < StartedAt)
		return Status = ERokSpeedUpQueueStatus::Lost;
	if (State->BuildingKey != BuildingKey || State->UnityInitId != InitId || State->UnityBuildingTypeId != BuildingType)
		return Status = ERokSpeedUpQueueStatus::Replaced;
	if (State->bUpgradeInProgress)
	{
		if (State->Level != StartingLevel || State->UpgradeStartedAt != StartedAt || State->UpgradeFinishesAt != FinishesAt)
			Status = ERokSpeedUpQueueStatus::Replaced;
		return Status;
	}
	if (QueueSeconds >= FinishesAt && State->Level == StartingLevel + 1
		&& State->UpgradeStartedAt == 0 && State->UpgradeFinishesAt == 0)
		Status = ERokSpeedUpQueueStatus::Completed;
	else
		Status = ERokSpeedUpQueueStatus::Lost;
	return Status;
}

bool FRokSpeedUpQueueLifecycle::ConsumeNaturalCompletion()
{
	if (Status != ERokSpeedUpQueueStatus::Completed || bCompletionConsumed) return false;
	bCompletionConsumed = true;
	return true;
}
