#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "../RokServerClock.h"
#include "../RokGameNetSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerClockTest, "Rok.Net.ServerClock.UnitsRttAndMonotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokServerClockTest::RunTest(const FString&)
{
	FRokServerClock Clock;
	double Now = -1.0;
	TestFalse(TEXT("No local-wall-clock fallback before sync"), Clock.TryGetServerTimeSeconds(100.0, Now));
	TestFalse(TEXT("Seconds rejected as milliseconds"), Clock.SeedFromServerMilliseconds(1700000000LL, 100.0));
	TestFalse(TEXT("Microseconds rejected as milliseconds"), Clock.SeedFromServerMilliseconds(1700000000000000LL, 100.0));
	TestTrue(TEXT("RoleInfo Unix milliseconds seed"), Clock.SeedFromServerMilliseconds(1700000000000LL, 100.0));
	TestTrue(TEXT("Seed is readable"), Clock.TryGetServerTimeSeconds(101.25, Now));
	TestTrue(TEXT("Monotonic interpolation uses elapsed seconds"), FMath::IsNearlyEqual(Now, 1700000001.25, 0.0001));
	TestEqual(TEXT("Seed has no RTT"), Clock.GetPingMilliseconds(), -1.0);
	TestFalse(TEXT("Later RoleInfo cannot overwrite heartbeat/clock"), Clock.SeedFromServerMilliseconds(1700000020000LL, 102.0));

	// A sample captured at send time, delivered 200 ms later, estimates 100 ms
	// of one-way travel and is then advanced by the monotonic clock.
	TestTrue(TEXT("Heartbeat sample accepted"), Clock.UpdateFromHeartbeat(1700000002000LL, 101.0, 101.2));
	TestTrue(TEXT("Heartbeat clock readable"), Clock.TryGetServerTimeSeconds(101.5, Now));
	TestTrue(TEXT("Heartbeat adds half RTT"), FMath::IsNearlyEqual(Now, 1700000002.4, 0.0001));
	TestTrue(TEXT("RTT reported in ms"), FMath::IsNearlyEqual(Clock.GetPingMilliseconds(), 200.0, 0.001));

	TestTrue(TEXT("Older valid sample accepted without clock reversal"), Clock.UpdateFromHeartbeat(1700000001000LL, 101.5, 101.7));
	double Later = 0.0;
	Clock.TryGetServerTimeSeconds(101.7, Later);
	TestTrue(TEXT("Correction never reverses time"), Later >= Now);
	Clock.TryGetServerTimeSeconds(101.6, Now);
	TestTrue(TEXT("Earlier query cannot reverse time"), Now >= Later);
	Clock.TryGetServerTimeSeconds(103.5, Later);
	TestTrue(TEXT("Slow correction eventually catches up"), FMath::IsNearlyEqual(Later, 1700000002.9, 0.0001));
	TestFalse(TEXT("Zero server timestamp rejected"), Clock.UpdateFromHeartbeat(0, 102.0, 102.1));
	TestFalse(TEXT("Reversed send/receive rejected"), Clock.UpdateFromHeartbeat(1700000003000LL, 102.1, 102.0));
	TestFalse(TEXT("Extreme RTT rejected"), Clock.UpdateFromHeartbeat(1700000003000LL, 100.0, 200.0));
	Clock.Reset();
	TestFalse(TEXT("Reset clears synchronization"), Clock.TryGetServerTimeSeconds(200.0, Now));
	TestEqual(TEXT("Reset clears ping"), Clock.GetPingMilliseconds(), -1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetServerClockMessageTest, "Rok.Net.ServerClock.MessageIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGameNetServerClockMessageTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(GameInstance);
	const int64 EpochMilliseconds = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
	TSharedPtr<FJsonObject> Role = MakeShared<FJsonObject>();
	Role->SetNumberField(TEXT("serverTime"), double(EpochMilliseconds));
	TSharedPtr<FJsonObject> RolePush = MakeShared<FJsonObject>();
	RolePush->SetObjectField(TEXT("roleInfo"), Role);
	Net->HandleMessage(TEXT("Role_RoleInfo"), false, RolePush);
	TestTrue(TEXT("RoleInfo push seeds clock"), Net->HasServerTime());
	TestEqual(TEXT("RoleInfo has no ping"), Net->GetServerPingMilliseconds(), -1.0);

	const int64 EchoTicks = 17000000000000000LL;
	Net->PendingHeartSends.Add(EchoTicks, FPlatformTime::Seconds() - 0.2);
	TSharedPtr<FJsonObject> Heart = MakeShared<FJsonObject>();
	Heart->SetNumberField(TEXT("clientTime"), double(EchoTicks));
	Heart->SetNumberField(TEXT("serverTime"), double(EpochMilliseconds + 1000));
	Net->HandleMessage(TEXT("Role_Heart"), true, Heart);
	TestTrue(TEXT("Matched heartbeat computes RTT"), Net->GetServerPingMilliseconds() >= 190.0);
	double ServerSeconds = 0.0;
	TestTrue(TEXT("Public server time available"), Net->TryGetServerTimeSeconds(ServerSeconds));
	TestTrue(TEXT("Heartbeat refines Unix time"), ServerSeconds >= double(EpochMilliseconds + 1000) / 1000.0);
	Net->Disconnect(TEXT("test reset"));
	TestFalse(TEXT("Disconnect clears clock"), Net->HasServerTime());
	TestFalse(TEXT("Disconnect clears public time"), Net->TryGetServerTimeSeconds(ServerSeconds));

	const FString ReplayPath = FPaths::Combine(FPaths::ProjectSavedDir(),
		FString::Printf(TEXT("RokServerClockReplayTest_%lld.json"), FDateTime::UtcNow().GetTicks()));
	const FString ReplayJson = FString::Printf(
		TEXT("{\"rid\":42,\"replay_server_time_ms\":%lld,\"state_by_protocol\":{\"Role_RoleInfo\":{\"roleInfo\":{\"serverTime\":1700000000000}}}}"),
		EpochMilliseconds);
	if (TestTrue(TEXT("Replay fixture written"), FFileHelper::SaveStringToFile(ReplayJson, *ReplayPath)))
	{
		FString Error;
		TestTrue(TEXT("Snapshot replay loads"), Net->StartSnapshotReplay(ReplayPath, Error));
		TestTrue(TEXT("Replay is marked"), Net->IsSnapshotReplay());
		TestEqual(TEXT("Replay role id"), Net->GetRoleId(), int64(42));
		TestTrue(TEXT("Replay clock uses explicit override"), Net->TryGetServerTimeSeconds(ServerSeconds)
			&& ServerSeconds >= double(EpochMilliseconds) / 1000.0);
		Net->Disconnect(TEXT("test replay reset"));
		TestFalse(TEXT("Replay flag clears"), Net->IsSnapshotReplay());
		TestFalse(TEXT("Replay clock clears"), Net->HasServerTime());
		IFileManager::Get().Delete(*ReplayPath);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetPushMergeTest, "Rok.Net.Push.NestedKeyedQueueMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGameNetPushMergeTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(GameInstance);
	FString Error;
	if (!TestTrue(TEXT("Protocol schema loads"), Net->LoadSchema(Error))) return false;
	const FRokSprotoProtocol* RoleProtocol = Net->Schema.FindProtocol(TEXT("Role_RoleInfo"));
	const FRokSprotoProtocol* BuildProtocol = Net->Schema.FindProtocol(TEXT("Build_BuildingInfo"));
	if (!TestTrue(TEXT("Push protocols exist"), RoleProtocol && BuildProtocol)) return false;
	Net->MergedState = MakeShared<FJsonObject>();

	auto Queue = [](int32 Index, int64 Finish, int64 Building, int64 Expired, int64 Begin, int64 FirstFinish)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("queueIndex"), Index);
		Row->SetNumberField(TEXT("finishTime"), double(Finish));
		Row->SetNumberField(TEXT("buildingIndex"), double(Building));
		Row->SetNumberField(TEXT("expiredTime"), double(Expired));
		Row->SetNumberField(TEXT("beginTime"), double(Begin));
		Row->SetNumberField(TEXT("firstFinishTime"), double(FirstFinish));
		return MakeShared<FJsonValueObject>(Row);
	};
	auto RolePush = [](const TArray<TSharedPtr<FJsonValue>>& Queues)
	{
		TSharedPtr<FJsonObject> Role = MakeShared<FJsonObject>();
		Role->SetArrayField(TEXT("buildQueue"), Queues);
		TSharedPtr<FJsonObject> Push = MakeShared<FJsonObject>();
		Push->SetObjectField(TEXT("roleInfo"), Role);
		return Push;
	};
	TArray<TSharedPtr<FJsonValue>> InitialQueues;
	InitialQueues.Add(Queue(1, 1700000060, 71, -1, 1700000000, 1700000060));
	InitialQueues.Add(Queue(2, -1, -1, 1700000300, 1700000000, -1));
	Net->MergePush(*RoleProtocol, RolePush(InitialQueues));

	TSharedPtr<FJsonObject> DeltaRow = MakeShared<FJsonObject>();
	DeltaRow->SetNumberField(TEXT("queueIndex"), 1);
	DeltaRow->SetNumberField(TEXT("finishTime"), 1700000040);
	TArray<TSharedPtr<FJsonValue>> DeltaQueues;
	DeltaQueues.Add(MakeShared<FJsonValueObject>(DeltaRow));
	Net->MergePush(*RoleProtocol, RolePush(DeltaQueues));

	auto FindQueue = [&Net](int64 Index)
	{
		const TSharedPtr<FJsonObject> Role = Net->GetRoleInfo();
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!Role.IsValid() || !Role->TryGetArrayField(TEXT("buildQueue"), Rows)) return TSharedPtr<FJsonObject>();
		for (const TSharedPtr<FJsonValue>& Value : *Rows)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			int64 RowIndex = 0;
			if (Value.IsValid() && Value->TryGetObject(Row)
				&& RokSprotoJson::TryGetInt64(*Row, TEXT("queueIndex"), RowIndex) && RowIndex == Index) return *Row;
		}
		return TSharedPtr<FJsonObject>();
	};
	const TSharedPtr<FJsonObject> First = FindQueue(1);
	const TSharedPtr<FJsonObject> Second = FindQueue(2);
	if (!TestTrue(TEXT("Both queue rows retained"), First.IsValid() && Second.IsValid())) return false;
	int64 Value = 0;
	TestTrue(TEXT("Queue 1 finish updated"), RokSprotoJson::TryGetInt64(First, TEXT("finishTime"), Value) && Value == 1700000040);
	TestTrue(TEXT("Queue 1 building preserved"), RokSprotoJson::TryGetInt64(First, TEXT("buildingIndex"), Value) && Value == 71);
	TestTrue(TEXT("Queue 1 begin preserved"), RokSprotoJson::TryGetInt64(First, TEXT("beginTime"), Value) && Value == 1700000000);
	TestTrue(TEXT("Queue 1 original finish preserved"), RokSprotoJson::TryGetInt64(First, TEXT("firstFinishTime"), Value) && Value == 1700000060);
	TestTrue(TEXT("Queue 2 lease preserved"), RokSprotoJson::TryGetInt64(Second, TEXT("expiredTime"), Value) && Value == 1700000300);

	DeltaRow = MakeShared<FJsonObject>();
	DeltaRow->SetNumberField(TEXT("queueIndex"), 1);
	DeltaRow->SetNumberField(TEXT("finishTime"), -1);
	DeltaQueues.Reset();
	DeltaQueues.Add(MakeShared<FJsonValueObject>(DeltaRow));
	Net->MergePush(*RoleProtocol, RolePush(DeltaQueues));
	TestTrue(TEXT("Cancel sentinel updates only queue 1 finish"),
		RokSprotoJson::TryGetInt64(FindQueue(1), TEXT("finishTime"), Value) && Value == -1);
	TestTrue(TEXT("Cancel retains queue 1 building"),
		RokSprotoJson::TryGetInt64(FindQueue(1), TEXT("buildingIndex"), Value) && Value == 71);
	TestTrue(TEXT("Cancel leaves queue 2 lease"),
		RokSprotoJson::TryGetInt64(FindQueue(2), TEXT("expiredTime"), Value) && Value == 1700000300);

	TSharedPtr<FJsonObject> Building = MakeShared<FJsonObject>();
	Building->SetNumberField(TEXT("buildingIndex"), 71);
	TArray<TSharedPtr<FJsonValue>> Buildings;
	Buildings.Add(MakeShared<FJsonValueObject>(Building));
	TSharedPtr<FJsonObject> BuildPush = MakeShared<FJsonObject>();
	BuildPush->SetArrayField(TEXT("buildingInfo"), Buildings);
	Net->MergePush(*BuildProtocol, BuildPush);
	const TSharedPtr<FJsonObject> Collection = Net->GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo"));
	TestTrue(TEXT("Top-level keyed collection remains an object"), Collection.IsValid() && Collection->HasField(TEXT("71")));
	Net->Disconnect(TEXT("test reset"));
	return true;
}
#endif
