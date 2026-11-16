#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "../RokHospitalTreatment.h"
#include "../RokUnityConfigSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHospitalTreatmentQuoteTest, "Rok.Hospital.TreatmentProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHospitalTreatmentQuoteTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokHospitalTreatmentQuote"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Config = Instance->GetSubsystem<URokUnityConfigSubsystem>();
	if (!TestNotNull(TEXT("Source config available"), Config))
	{
		return false;
	}

	auto MakeWounded = [](int32 Id, int32 Type, int32 Count)
	{
		auto Soldier = MakeShared<FJsonObject>();
		Soldier->SetNumberField(TEXT("id"), Id);
		Soldier->SetNumberField(TEXT("type"), Type);
		Soldier->SetNumberField(TEXT("level"), 1);
		Soldier->SetNumberField(TEXT("num"), Count);
		return MakeShared<FJsonValueObject>(Soldier);
	};
	const auto Role = MakeShared<FJsonObject>();
	Role->SetNumberField(TEXT("food"), 1000);
	Role->SetNumberField(TEXT("wood"), 1000);
	Role->SetNumberField(TEXT("denar"), 5);
	Role->SetArrayField(TEXT("seriousInjured"), {
		MakeWounded(10301, 3, 13), MakeWounded(10101, 1, 4), MakeWounded(10201, 2, 0)});
	const FRokHospitalState Hospital = FRokHospitalState::Read(Role);
	if (TestEqual(TEXT("Only actual injured soldiers are shown"), Hospital.Wounded.Num(), 2))
	{
		TestEqual(TEXT("Infantry precedes archers"), Hospital.Wounded[0].Id, 10101);
		TestEqual(TEXT("Archers retain their authoritative count"), Hospital.Wounded[1].Count, 13);
	}
	const auto Full = FRokHospitalQuote::Calculate(*Config, {{10101, 4}, {10301, 13}}, Role);
	TestTrue(TEXT("Current account can start either treatment"), Full.bValid && Full.bHasResources && Full.bHasGems);
	TestEqual(TEXT("Food matches Unity capture"), Full.Resources[0], int64(288));
	TestEqual(TEXT("Wood matches Unity capture"), Full.Resources[1], int64(392));
	TestEqual(TEXT("T1 uses the configured minimum duration"), Full.Seconds, int64(3));
	TestEqual(TEXT("Instant treatment costs one gem with enough materials"), Full.Gems, int64(1));
	const auto Partial = FRokHospitalQuote::Calculate(*Config, {{10301, 1}}, Role);
	TestEqual(TEXT("Slider selection changes only the selected unit cost"), Partial.Resources[1], int64(24));
	TestFalse(TEXT("No selected soldiers cannot start a treatment"), FRokHospitalQuote::Calculate(*Config, {}, Role).bValid);
	TestFalse(TEXT("Disconnected account stays unknown"), FRokHospitalState::Read(nullptr).bKnown);
	return true;
}
#endif
