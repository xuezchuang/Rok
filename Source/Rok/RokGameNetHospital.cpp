#include "RokGameNetSubsystem.h"

#include "Dom/JsonObject.h"
#include "RokHospitalTreatment.h"

bool URokGameNetSubsystem::RequestHospitalTreatment(const TMap<int32, int32>& Counts, bool bImmediately,
	bool bCollect, TFunction<void(const FString&)> OnResult, FString& Error)
{
	Error.Reset();
	if (bSnapshotReplay) Error = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) Error = TEXT("连接服务器后可治疗伤员。");
	else if (bHospitalWriteNeedsReconnect) Error = TEXT("请重新连接后确认上一次治疗操作结果。");
	else if (PendingHospitalWrite.IsSet()) Error = TEXT("正在等待服务器确认治疗操作。");
	if (!Error.IsEmpty()) return false;

	const FRokHospitalState Hospital = FRokHospitalState::Read(GetRoleInfo());
	if (!Hospital.bKnown)
	{
		Error = TEXT("医院伤员尚未同步。");
		return false;
	}

	FHospitalWrite Write;
	Write.BeforeQueue = Hospital.FinishTime;
	Write.bStartsQueue = !bCollect && !bImmediately;
	const bool bUsesQueue = Hospital.FinishTime > 0;
	double Now = 0.0;
	const bool bClockKnown = TryGetServerTimeSeconds(Now);
	const TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	if (bUsesQueue)
	{
		if (!bClockKnown || Hospital.InTreatment.IsEmpty()
			|| (bCollect ? Now < Hospital.FinishTime
				: !bImmediately || Now >= Hospital.FinishTime || Hospital.QueueIndex <= 0))
		{
			Error = TEXT("治疗队列状态已变更，请确认后重试。");
			return false;
		}
		if (!bCollect) Body->SetNumberField(TEXT("treatmentQueueIndex"), Hospital.QueueIndex);
	}
	else
	{
		if (bCollect || Counts.IsEmpty())
		{
			Error = TEXT("请选择要治疗的伤员。");
			return false;
		}
		TArray<TSharedPtr<FJsonValue>> Soldiers;
		for (const auto& Pair : Counts)
		{
			const auto* Wounded = Hospital.Wounded.FindByPredicate([&Pair](const FRokHospitalSoldier& Unit)
			{
				return Unit.Id == Pair.Key;
			});
			if (!Wounded || Pair.Value <= 0 || Pair.Value > Wounded->Count)
			{
				Error = TEXT("伤员数量已变更，请确认后重试。");
				return false;
			}
			auto Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("id"), Wounded->Id);
			Row->SetNumberField(TEXT("type"), Wounded->Type);
			Row->SetNumberField(TEXT("level"), Wounded->Level);
			Row->SetNumberField(TEXT("num"), Pair.Value);
			Soldiers.Add(MakeShared<FJsonValueObject>(Row));
			if (bImmediately) Write.ExpectedWounded.Add(Pair.Key, Wounded->Count - Pair.Value);
		}
		Body->SetArrayField(TEXT("soldiers"), MoveTemp(Soldiers));
	}
	if (bImmediately) Body->SetBoolField(TEXT("immediately"), true);
	if (!SendRequest(bCollect ? TEXT("Role_AwardTreatment") : TEXT("Role_Treatment"), bCollect ? nullptr : Body))
	{
		Error = TEXT("治疗请求发送失败。");
		return false;
	}
	Write.OuterSession = SprotoSession;
	Write.InnerSession = SprotoSession - 1;
	Write.Deadline = FPlatformTime::Seconds() + 12.0;
	Write.OnResult = MoveTemp(OnResult);
	PendingHospitalWrite = MoveTemp(Write);
	return true;
}

void URokGameNetSubsystem::CheckHospitalWrite(double Now)
{
	if (!PendingHospitalWrite) return;
	const auto& Write = *PendingHospitalWrite;
	if (Write.bAcknowledged)
	{
		const auto Hospital = FRokHospitalState::Read(GetRoleInfo());
		bool bUpdated = Hospital.bKnown;
		if (Write.bStartsQueue) bUpdated &= Hospital.FinishTime > 0 && !Hospital.InTreatment.IsEmpty();
		else if (Write.BeforeQueue > 0) bUpdated &= Hospital.FinishTime <= 0;
		for (const auto& Pair : Write.ExpectedWounded)
		{
			const auto* Wounded = Hospital.Wounded.FindByPredicate([&Pair](const FRokHospitalSoldier& Unit)
			{
				return Unit.Id == Pair.Key;
			});
			bUpdated &= (Wounded ? Wounded->Count : 0) <= Pair.Value;
		}
		if (bUpdated)
		{
			FinishHospitalWrite(FString());
			return;
		}
	}
	if (Now >= Write.Deadline)
	{
		bHospitalWriteNeedsReconnect = true;
		FinishHospitalWrite(TEXT("治疗结果待确认，请重新连接后查看。"));
	}
}

void URokGameNetSubsystem::FinishHospitalWrite(const FString& Error)
{
	auto Callback = MoveTemp(PendingHospitalWrite->OnResult);
	PendingHospitalWrite.Reset();
	if (Callback) Callback(Error);
}
