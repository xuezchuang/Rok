#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_GuildFortress: rows join AllianceBuildingType.csv names with
// AllianceBuildingData.csv per-order costs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokGuildFortressLayoutTest,
	"Rok.Ui.GuildFortress.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGuildFortressLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Guild-fortress modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Guild-fortress modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenGuildFortress();

	if (!TestNotNull(TEXT("The first structure row exists"),
		Modal->GetWidgetFromName(TEXT("GuildFortressRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GuildFortressRowTitle_0")));
	TestTrue(TEXT("The row names a real alliance structure"),
		Title != nullptr
			&& (Title->GetText().ToString().Contains(TEXT("联盟主要塞"))
				|| Title->GetText().ToString().Contains(TEXT("联盟"))));
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GuildFortressRowDetail_0")));
	TestTrue(TEXT("The detail lists the resource cost"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("粮"))
			&& Detail->GetText().ToString().Contains(TEXT("石")));
	TestTrue(TEXT("At least two costed structure types are listed"),
		Modal->GetWidgetFromName(TEXT("GuildFortressRow_1")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the guild-fortress mode"), Modal->IsModalOpen());
	return true;
}

#endif
