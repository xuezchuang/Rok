#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokCityPlacementFirstFit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityPlacementFirstFitTest,
	"Rok.Gameplay.CityPlacementPreview.SourceFirstFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityPlacementFirstFitTest::RunTest(const FString&)
{
	FIntPoint Found(999, 999);
	TArray<FIntPoint> Seen;
	TestTrue(TEXT("First radius follows source horizontal then vertical order"),
		FRokCityPlacementFirstFit::Find(41, FIntPoint(5, 5),
			[&Seen](const FIntPoint& Candidate)
			{
				Seen.Add(Candidate);
				return Candidate == FIntPoint(1, -1);
			}, Found));
	if (Seen.Num() < 3)
	{
		AddError(TEXT("Source first-fit did not visit the expected perimeter cells"));
		return false;
	}
	TestEqual(TEXT("First corner starts at negative X, positive Y"), Seen[0], FIntPoint(-1, 1));
	TestEqual(TEXT("First corner advances X"), Seen[1], FIntPoint(0, 1));
	TestEqual(TEXT("Second corner starts downward"), Seen[2], FIntPoint(-1, -1));
	TestEqual(TEXT("Selected candidate comes from source order"), Found, FIntPoint(1, -1));

	Found = FIntPoint(999, 999);
	TestFalse(TEXT("No free footprint is a failure, not Unity fallback tile"),
		FRokCityPlacementFirstFit::Find(41, FIntPoint(5, 5),
			[](const FIntPoint&) { return false; }, Found));
	TestEqual(TEXT("No free result clears stale tile"), Found, FIntPoint::ZeroValue);
	TestFalse(TEXT("Even source grid is rejected"),
		FRokCityPlacementFirstFit::Find(40, FIntPoint(5, 5),
			[](const FIntPoint&) { return true; }, Found));
	return true;
}
#endif
