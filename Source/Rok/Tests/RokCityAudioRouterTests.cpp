#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../RokCityAudioCatalog.h"
#include "../RokDataPaths.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityAudioRouterSourceTest,
	"Rok.Ui.Audio.SourceCatalogAndBgmSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityAudioRouterSourceTest::RunTest(const FString&)
{
	TMap<int32, FRokCityAudioEvent> Events;
	FString Error;
	if (!TestTrue(TEXT("Pinned source audio catalog loads"), RokCityAudioCatalog::Load(Events, Error)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("Only seven bounded source events"), Events.Num(), 7);
	const FRokCityAudioEvent* Day = Events.Find(1007);
	const FRokCityAudioEvent* Night = Events.Find(1008);
	const FRokCityAudioEvent* Feedback = Events.Find(4005);
	if (!TestNotNull(TEXT("Day event"), Day)
		|| !TestNotNull(TEXT("Night event"), Night)
		|| !TestNotNull(TEXT("Source slider feedback"), Feedback)) return false;
	TestEqual(TEXT("Day source minimum pause"), Day->IntervalMinSeconds, 3.0f);
	TestEqual(TEXT("Day source maximum pause"), Day->IntervalMaxSeconds, 8.0f);
	TestEqual(TEXT("Night source minimum pause"), Night->IntervalMinSeconds, 1.0f);
	TestEqual(TEXT("Night source maximum pause"), Night->IntervalMaxSeconds, 2.0f);
	TestEqual(TEXT("Slider feedback stays SFX"), Feedback->bMusic, false);
	TestEqual(TEXT("Slider feedback source clip"), Feedback->ClipNames[0], FString(TEXT("Sound_Ui_CommonScroll")));

	// Exercise the scheduler without starting a UWorld or an audio device.
	FRokCityBgmSequence Sequence;
	int32 Clip = INDEX_NONE;
	TestTrue(TEXT("Source random first day track"), Sequence.Start(*Day, 0.9f, Clip));
	TestEqual(TEXT("First draw chooses second of two tracks"), Clip, 1);
	TestTrue(TEXT("Natural end arms day interval"), Sequence.FinishNaturally(*Day, 0.5f));
	TestTrue(TEXT("Day midpoint is five and a half seconds"),
		FMath::IsNearlyEqual(Sequence.GetPendingDelaySeconds(), 5.5f, 1e-5f));
	TestFalse(TEXT("A pending track cannot finish twice"), Sequence.FinishNaturally(*Day, 0.0f));
	TestFalse(TEXT("No early restart, even when source draw repeats old clip"),
		Sequence.Advance(*Day, 5.49f, 0.9f, Clip));
	TestEqual(TEXT("No restart index before timer"), Clip, INDEX_NONE);
	TestTrue(TEXT("Natural deadline starts alternate track"),
		Sequence.Advance(*Day, 0.02f, 0.9f, Clip));
	TestEqual(TEXT("Random draw may not repeat previous track"), Clip, 0);
	TestEqual(TEXT("Only one naturally completed track"), Sequence.GetCompletedCount(), 1);
	TestFalse(TEXT("No second start without another natural finish"),
		Sequence.Advance(*Day, 20.0f, 0.0f, Clip));
	TestTrue(TEXT("Night source uses independent one-to-two second pause"),
		Sequence.Start(*Night, 0.1f, Clip) && Sequence.FinishNaturally(*Night, .75f));
	TestTrue(TEXT("Night source draw produces one and three-quarter seconds"),
		FMath::IsNearlyEqual(Sequence.GetPendingDelaySeconds(), 1.75f, 1e-5f));
	TestFalse(TEXT("NaN draw rejected, no invented track"),
		Sequence.Start(*Day, std::numeric_limits<float>::quiet_NaN(), Clip));

	TArray<uint8> SourceBytes;
	const FString Path = RokDataPaths::Resolve(TEXT("CityAudio/catalog.json"));
	if (!TestTrue(TEXT("Read generated source bytes"), FFileHelper::LoadFileToArray(SourceBytes, *Path))) return false;
	if (!TestTrue(TEXT("Exact source bytes accepted"),
		RokCityAudioCatalog::LoadBytesForTesting(SourceBytes, Events, Error))) return false;
	// Change a business event ID inside an in-memory copy: bytes must be
	// rejected before any row can be consumed, without rewriting the catalog.
	TArray<uint8> Tampered = SourceBytes;
	const ANSICHAR Before[] = "\"id\": 1007";
	const ANSICHAR After[] = "\"id\": 1006";
	int32 MatchCount = 0;
	for (int32 I = 0; I + UE_ARRAY_COUNT(Before) - 1 <= Tampered.Num(); ++I)
	{
		if (FMemory::Memcmp(Tampered.GetData() + I, Before, UE_ARRAY_COUNT(Before) - 1) == 0)
		{
			FMemory::Memcpy(Tampered.GetData() + I, After, UE_ARRAY_COUNT(After) - 1);
			++MatchCount;
		}
	}
	TestEqual(TEXT("One source ID mutation applied"), MatchCount, 1);
	TestFalse(TEXT("Mutated source bytes fail closed"),
		RokCityAudioCatalog::LoadBytesForTesting(Tampered, Events, Error));
	TestEqual(TEXT("Rejected catalog leaves no partially loaded events"), Events.Num(), 0);
	return true;
}
#endif
