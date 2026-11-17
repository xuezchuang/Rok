#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_GuildPurview: rows come from AllianceMemberJurisdiction.csv; the R1..R5
// columns become the allowed-rank detail line.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokGuildPurviewLayoutTest,
	"Rok.Ui.GuildPurview.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGuildPurviewLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Guild-purview modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Guild-purview modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenGuildPurview();

	if (!TestNotNull(TEXT("The first permission row exists"),
		RokUiFind::Find(Modal, TEXT("GuildPurviewRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("GuildPurviewRowTitle_0")));
	TestTrue(TEXT("The first row resolves the permission name"),
		Title != nullptr && Title->GetText().ToString() == TEXT("发送联盟邮件"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("GuildPurviewRowDetail_0")));
	TestTrue(TEXT("The detail lists R2..R5 as allowed and omits R1"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("R2"))
			&& Detail->GetText().ToString().Contains(TEXT("R5"))
			&& !Detail->GetText().ToString().Contains(TEXT("R1")));
	TestNotNull(TEXT("The footer note is present"),
		RokUiFind::Find(Modal, TEXT("GuildPurviewFooter")));
	TestNotNull(TEXT("Rows live in the source-style scroll view"),
		RokUiFind::Find(Modal, TEXT("GuildPurviewScroll")));
	TestTrue(TEXT("The list is no longer truncated at six rows (18 permissions)"),
		RokUiFind::Find(Modal, TEXT("GuildPurviewRow_17")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the guild-purview mode"), Modal->IsModalOpen());
	return true;
}

#endif
