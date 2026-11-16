#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../RokServerBuildQueue.h"

namespace
{
TSharedPtr<FJsonObject> QueueRow(int32 Index, int32 Building, int64 Finish, int64 Expiry = -1)
{
	auto Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("queueIndex"), Index);
	Row->SetNumberField(TEXT("buildingIndex"), Building);
	Row->SetNumberField(TEXT("finishTime"), double(Finish));
	Row->SetNumberField(TEXT("expiredTime"), double(Expiry));
	return Row;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerBuildQueueParseTest, "Rok.Net.ServerQueue.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokServerBuildQueueParseTest::RunTest(const FString&)
{
	auto Role = MakeShared<FJsonObject>();
	Role->SetArrayField(TEXT("buildQueue"), {MakeShared<FJsonValueObject>(QueueRow(2, 0, -1, 1800000200)),
		MakeShared<FJsonValueObject>(QueueRow(1, 5, 1800000100))});
	TArray<FRokServerBuildQueue> Rows;
	FString Error;
	TestTrue(TEXT("Wire array parses"), RokServerBuildQueue::Parse(Role, Rows, Error));
	if (!TestEqual(TEXT("Both workers"), Rows.Num(), 2)) return false;
	TestEqual(TEXT("Index sorted"), Rows[0].QueueIndex, 1);
	TestEqual(TEXT("Building identity"), Rows[0].BuildingIndex, 5);
	TestEqual(TEXT("Seconds retained exactly"), Rows[0].FinishTime, int64(1800000100));
	auto Keyed = MakeShared<FJsonObject>();
	Keyed->SetObjectField(TEXT("1"), QueueRow(1, 0, -1));
	Role->SetObjectField(TEXT("buildQueue"), Keyed);
	TestTrue(TEXT("Merged keyed collection parses"), RokServerBuildQueue::Parse(Role, Rows, Error));
	Keyed->SetObjectField(TEXT("duplicate"), QueueRow(1, 0, -1));
	TestFalse(TEXT("Duplicate identity fails"), RokServerBuildQueue::Parse(Role, Rows, Error));
	TestTrue(TEXT("Rejected parse is atomic"), Rows.IsEmpty());
	Keyed->RemoveField(TEXT("duplicate"));
	Keyed->SetObjectField(TEXT("2"), QueueRow(2, -4, -1));
	TestFalse(TEXT("Invalid building identity fails"), RokServerBuildQueue::Parse(Role, Rows, Error));
	Role->RemoveField(TEXT("buildQueue"));
	TestFalse(TEXT("Absent data is not idle"), RokServerBuildQueue::Parse(Role, Rows, Error));
	Role->SetArrayField(TEXT("buildQueue"), {});
	TestTrue(TEXT("An explicitly empty collection is valid"), RokServerBuildQueue::Parse(Role, Rows, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerBuildQueueStatesTest, "Rok.Net.ServerQueue.StatesAndLease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokServerBuildQueueStatesTest::RunTest(const FString&)
{
	constexpr double Now = 1800000000.0;
	TArray<FRokServerBuildQueue> Rows{{1, 0, -1, 0, 0, -1}, {2, 0, -1, 0, 0, int64(Now)}};
	auto Snapshot = RokServerBuildQueue::Project(Rows, true, true, Now + 0.9);
	TestEqual(TEXT("Main idle"), Snapshot.FindQueue(1)->State, ERokServerBuildQueueState::Idle);
	TestEqual(TEXT("Expiry equality remains valid for the entire source second"),
		Snapshot.FindQueue(2)->State, ERokServerBuildQueueState::Idle);
	Snapshot = RokServerBuildQueue::Project(Rows, true, true, Now + 1.0);
	TestEqual(TEXT("Expired secondary locked"), Snapshot.FindQueue(2)->State, ERokServerBuildQueueState::Locked);
	Snapshot = RokServerBuildQueue::Project(Rows, true, false, 0.0);
	TestEqual(TEXT("Main idle needs no inferred time"), Snapshot.FindQueue(1)->State, ERokServerBuildQueueState::Idle);
	TestEqual(TEXT("Finite lease awaits server clock"), Snapshot.FindQueue(2)->State, ERokServerBuildQueueState::Unknown);
	Rows[1].ExpiredTime = -1;
	Snapshot = RokServerBuildQueue::Project(Rows, true, false, 0.0);
	TestEqual(TEXT("Permanent lease needs no clock"), Snapshot.FindQueue(2)->State, ERokServerBuildQueueState::Idle);
	TestEqual(TEXT("Permanent sentinel retained"), Snapshot.FindQueue(2)->LeaseRemainingSeconds, -1.0);
	Rows.RemoveAt(1);
	Snapshot = RokServerBuildQueue::Project(Rows, true, true, Now);
	TestEqual(TEXT("Absent second worker locked"), Snapshot.FindQueue(2)->State, ERokServerBuildQueueState::Locked);
	Snapshot = RokServerBuildQueue::Project(Rows, false, true, Now);
	TestTrue(TEXT("Server retains authority while awaiting fields"), Snapshot.bAvailable);
	TestEqual(TEXT("Missing queue data stays unknown"), Snapshot.FindQueue(1)->State, ERokServerBuildQueueState::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerBuildQueueClockTest, "Rok.Net.ServerQueue.CountdownAndConfirmation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokServerBuildQueueClockTest::RunTest(const FString&)
{
	constexpr int64 Now = 1800000000;
	// Original duration 200s; a speedup moved the finish earlier by 50s.
	TArray<FRokServerBuildQueue> Rows{{1, 5, Now + 50, Now - 100, Now + 100, -1}};
	auto Snapshot = RokServerBuildQueue::Project(Rows, true, true, double(Now));
	const auto* View = Snapshot.FindQueue(1);
	TestTrue(TEXT("Server countdown known"), View->bCountdownKnown);
	TestEqual(TEXT("Fifty seconds remain"), View->RemainingSeconds, 50.0);
	TestEqual(TEXT("First finish preserves original duration"), View->TotalSeconds, 200.0);
	TestEqual(TEXT("Speedup raises progress to 75 percent"), View->Progress, 0.75f);
	Snapshot = RokServerBuildQueue::Project(Rows, true, true, double(Now + 60));
	View = Snapshot.FindQueue(1);
	TestEqual(TEXT("Past timer does not locally free worker"), View->State, ERokServerBuildQueueState::Busy);
	TestTrue(TEXT("Past timer awaits authoritative push"), View->bWaitingForServer);
	TestEqual(TEXT("Past timer stays at zero"), View->RemainingSeconds, 0.0);
	TestEqual(TEXT("Past timer bar full"), View->Progress, 1.0f);
	TestEqual(TEXT("Projection never rewrites server finish"), Rows[0].FinishTime, Now + 50);
	Rows[0].FinishTime = -1;
	Snapshot = RokServerBuildQueue::Project(Rows, true, true, double(Now + 60));
	TestEqual(TEXT("Server reset frees worker"), Snapshot.FindQueue(1)->State, ERokServerBuildQueueState::Idle);
	TestFalse(TEXT("Server reset removes bar"), Snapshot.FindQueue(1)->bCountdownKnown);
	Rows[0].FinishTime = -2;
	Snapshot = RokServerBuildQueue::Project(Rows, true, true, double(Now));
	TestFalse(TEXT("Cancellation sentinel removes bar"), Snapshot.FindQueue(1)->bCountdownKnown);
	Rows[0].FinishTime = Now + 50;
	Rows[0].FirstFinishTime = Rows[0].BeginTime;
	Snapshot = RokServerBuildQueue::Project(Rows, true, true, double(Now));
	TestFalse(TEXT("Missing cost duration does not fabricate progress"), Snapshot.FindQueue(1)->bProgressKnown);
	TestTrue(TEXT("Remainder is still usable"), Snapshot.FindQueue(1)->bCountdownKnown);
	Rows[0].QueueIndex = 2;
	Rows[0].ExpiredTime = Now - 1;
	Snapshot = RokServerBuildQueue::Project(Rows, true, true, double(Now));
	TestEqual(TEXT("Expired busy hire locks slot"), Snapshot.FindQueue(2)->State, ERokServerBuildQueueState::Locked);
	TestTrue(TEXT("Existing building keeps its clock after hire expires"), Snapshot.FindQueue(2)->bCountdownKnown);
	Snapshot = RokServerBuildQueue::Project(Rows, true, false, 0.0);
	TestFalse(TEXT("No countdown before synchronization"), Snapshot.FindQueue(2)->bCountdownKnown);
	return true;
}

#endif
