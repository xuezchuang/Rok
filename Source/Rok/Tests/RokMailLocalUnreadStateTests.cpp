#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokMailLocalUnreadState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMailLocalUnreadStateTest,
	"Rok.Ui.Mail.LocalUnreadState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokMailLocalUnreadStateTest::RunTest(const FString&)
{
	FRokMailLocalUnreadState State;
	State.SeedOnce();
	TestEqual(TEXT("system initially has one unread"), State.UnreadCount(0), 1);
	TestEqual(TEXT("report initially has one unread"), State.UnreadCount(1), 1);
	TestEqual(TEXT("sent category has no synthetic rows"), State.FirstIndexForTab(4), INDEX_NONE);
	TestEqual(TEXT("sent category has no unread badge"), State.UnreadCount(4), 0);
	TestEqual(TEXT("opening system category selects first row"), State.SelectFirstAndMarkReadForTab(0), 0);
	TestTrue(TEXT("opening system category marks first row read"), State.Find(0)->bRead);
	TestEqual(TEXT("system badge clears"), State.UnreadCount(0), 0);
	TestEqual(TEXT("report category selects first report"), State.SelectFirstAndMarkReadForTab(1), 2);
	TestTrue(TEXT("report first row is read"), State.Find(2)->bRead);
	TestEqual(TEXT("report badge clears"), State.UnreadCount(1), 0);
	TestEqual(TEXT("returning to system retains first selection"), State.SelectFirstAndMarkReadForTab(0), 0);
	TestTrue(TEXT("returning to system preserves read state"), State.Find(0)->bRead);
	TestFalse(TEXT("reselecting read row is idempotent"), State.MarkRead(0, 0));
	TestFalse(TEXT("other category cannot mutate row"), State.MarkRead(2, 0));
	TestEqual(TEXT("empty sent category has no selection"), State.SelectFirstAndMarkReadForTab(4), INDEX_NONE);
	TestEqual(TEXT("empty category leaves alliance unread"), State.UnreadCount(2), 1);
	TestEqual(TEXT("empty category leaves personal unread"), State.UnreadCount(3), 1);
	State.SeedOnce(); // Same widget instance opens the fixture again after closing.
	TestTrue(TEXT("reopen preserves read state"), State.Find(0)->bRead);
	TestEqual(TEXT("reopen selects same system row"), State.SelectFirstAndMarkReadForTab(0), 0);
	TestEqual(TEXT("reopen keeps system badge clear"), State.UnreadCount(0), 0);
	return true;
}
#endif
