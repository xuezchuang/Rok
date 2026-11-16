#include "RokServerBuildQueue.h"

#include "RokSproto.h"

bool RokServerBuildQueue::Parse(const TSharedPtr<FJsonObject>& RoleInfo,
	TArray<FRokServerBuildQueue>& OutQueues, FString& OutError)
{
	OutQueues.Reset();
	OutError.Reset();
	const TSharedPtr<FJsonValue> Field = RoleInfo.IsValid() ? RoleInfo->TryGetField(TEXT("buildQueue")) : nullptr;
	TArray<TSharedPtr<FJsonValue>> Rows;
	if (Field.IsValid() && Field->Type == EJson::Array) Rows = Field->AsArray();
	else if (Field.IsValid() && Field->Type == EJson::Object)
	{
		for (const auto& Pair : Field->AsObject()->Values) Rows.Add(Pair.Value);
	}
	else
	{
		OutError = TEXT("RoleInfo.buildQueue is missing or is not a collection");
		return false;
	}
	TArray<FRokServerBuildQueue> Parsed;
	TSet<int32> Indices;
	for (const TSharedPtr<FJsonValue>& Value : Rows)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		int64 Index = 0, Building = 0, Finish = -1, Begin = 0, FirstFinish = 0, Expiry = 0;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !Object->IsValid()
			|| !RokSprotoJson::TryGetInt64(*Object, TEXT("queueIndex"), Index) || Index <= 0 || Index > MAX_int32)
		{
			OutError = TEXT("buildQueue row lacks a valid queueIndex");
			return false;
		}
		const auto Read = [Object](const TCHAR* Name, int64& Out)
		{
			return !(*Object)->HasField(Name) || RokSprotoJson::TryGetInt64(*Object, Name, Out);
		};
		if (!Read(TEXT("buildingIndex"), Building) || Building < 0 || Building > MAX_int32
			|| !Read(TEXT("finishTime"), Finish) || Finish < -2
			|| !Read(TEXT("beginTime"), Begin) || Begin < 0
			|| !Read(TEXT("firstFinishTime"), FirstFinish) || FirstFinish < -1
			|| !Read(TEXT("expiredTime"), Expiry) || Expiry < -1
			|| (Building > 0 && !(*Object)->HasField(TEXT("finishTime")))
			|| Indices.Contains(int32(Index)))
		{
			OutError = FString::Printf(TEXT("buildQueue[%lld] has invalid or duplicate fields"), Index);
			return false;
		}
		Indices.Add(int32(Index));
		Parsed.Add({int32(Index), int32(Building), Finish, Begin, FirstFinish, Expiry});
	}
	Parsed.Sort([](const auto& A, const auto& B) { return A.QueueIndex < B.QueueIndex; });
	OutQueues = MoveTemp(Parsed);
	return true;
}

FRokServerBuildQueueSnapshot RokServerBuildQueue::Project(const TArray<FRokServerBuildQueue>& Queues,
	bool bHasQueueData, bool bHasServerTime, double ServerNowSeconds)
{
	FRokServerBuildQueueSnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.bTimeSynchronized = bHasServerTime && FMath::IsFinite(ServerNowSeconds) && ServerNowSeconds > 0.0;
	for (int32 Index : {1, 2})
	{
		FRokServerBuildQueueView& View = Snapshot.Queues.AddDefaulted_GetRef();
		View.QueueIndex = Index;
		if (!bHasQueueData) continue;
		const FRokServerBuildQueue* Row = Queues.FindByPredicate(
			[Index](const FRokServerBuildQueue& Q) { return Q.QueueIndex == Index; });
		if (!Row)
		{
			// WorkerProxy: no secondary entry means it has not been unlocked.
			if (Index == 2) View.State = ERokServerBuildQueueState::Locked;
			continue;
		}
		View.BuildingIndex = Row->BuildingIndex;
		View.LeaseRemainingSeconds = Row->ExpiredTime == -1 ? -1.0
			: (Snapshot.bTimeSynchronized ? FMath::Max(0.0, double(Row->ExpiredTime) - ServerNowSeconds) : 0.0);
		// An expired hire locks the worker slot, but must not hide an already
		// running building's HUD clock (the source building bar is independent).
		if (Row->BuildingIndex > 0 && Row->FinishTime >= 0 && Snapshot.bTimeSynchronized)
		{
			View.bCountdownKnown = true;
			View.RemainingSeconds = FMath::Max(0.0, double(Row->FinishTime) - ServerNowSeconds);
			View.bWaitingForServer = Row->FinishTime <= ServerNowSeconds;
			View.TotalSeconds = double(Row->FirstFinishTime) - double(Row->BeginTime);
			View.bProgressKnown = Row->BeginTime > 0 && View.TotalSeconds > 0.0;
			if (View.bProgressKnown)
			{
				View.Progress = float(FMath::Clamp(1.0 - View.RemainingSeconds / View.TotalSeconds, 0.0, 1.0));
			}
		}
		if (Index == 2 && Row->ExpiredTime != -1)
		{
			if (!Snapshot.bTimeSynchronized) continue;
			// Unity uses whole server seconds and considers equality still valid.
			if (Row->ExpiredTime < FMath::FloorToDouble(ServerNowSeconds))
			{
				View.State = ERokServerBuildQueueState::Locked;
				continue;
			}
		}
		View.State = Row->FinishTime == -1 || Row->BuildingIndex == 0
			? ERokServerBuildQueueState::Idle : ERokServerBuildQueueState::Busy;
	}
	return Snapshot;
}
