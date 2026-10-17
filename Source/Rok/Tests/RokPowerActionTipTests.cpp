#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokPowerActionTip.h"
#include "../RokUnityModalWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerActionTipTest,
	"Rok.Ui.PlayerProfile.ActionTip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPowerActionTipTest::RunTest(const FString&)
{
	using namespace RokPowerActionTip;
	TestEqual(TEXT("16 source pixels become 12 points"), FontPoints(16), 12.f);
	TestEqual(TEXT("18 source pixels retain fractional points"), FontPoints(18), 13.5f);
	TestEqual(TEXT("Source scale midpoint"), Scale(.11666667f * .5f), .5f);
	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize();
	const TSharedRef<SWidget> Slate = Modal->TakeWidget();
	Modal->OpenPlayerProfile(); Modal->AdvanceWindowAnimationForTesting(1);
	auto* Help = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ProfileActionHelpButton")));
	if (!TestNotNull(TEXT("Existing profile action entry"), Help)) return false;
	Help->OnClicked.Broadcast();
	auto* Tip = Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot"));
	if (!TestNotNull(TEXT("Action tip opens"), Tip)) return false;
	const auto* Slot = CastChecked<UCanvasPanelSlot>(Tip->Slot);
	const auto* AnchorSlot = CastChecked<UCanvasPanelSlot>(Help->Slot);
	TestEqual(TEXT("Source content plus HelpTip frame"), Slot->GetSize(), FVector2D(420, 280));
	TestEqual(TEXT("Arrow anchors below help center"), Slot->GetPosition().Y,
		AnchorSlot->GetPosition().Y + AnchorSlot->GetSize().Y * .5 + 12);
	TestEqual(TEXT("Entry begins at zero scale"), Tip->GetRenderTransform().Scale, FVector2D(0));
	TestEqual(TEXT("Source does not animate alpha"), Tip->GetRenderOpacity(), 1.f);
	auto* Description = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerActionTipDescription")));
	auto* Warning = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerActionTipWarning")));
	auto* Recovery = Cast<URichTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerActionTipRecovery")));
	if (!TestNotNull(TEXT("First source text"), Description) || !TestNotNull(TEXT("Last source text"), Warning)
		|| !TestNotNull(TEXT("Rich recovery text"), Recovery)) return false;
	TestEqual(TEXT("Body font matches source pixel size"), Description->GetFont().Size, 12.f);
	TestEqual(TEXT("Warning font matches source pixel size"), Warning->GetFont().Size, 12.f);
	TestEqual(TEXT("Recovery font matches source pixel size"), Recovery->GetDefaultTextStyle().Font.Size, 13.5f);
	const FString Unknown = TEXT("行动力恢复1点需要—秒\n（恢复速度提升：+ —%）\n\n全部恢复还需   <Remaining>—</>");
	TestEqual(TEXT("All three authoritative terms stay unknown"), Recovery->GetText().ToString(), Unknown);
	Modal->AdvanceWindowAnimationForTesting(.11666667f * .5f);
	TestEqual(TEXT("Clock samples original source scale"), Tip->GetRenderTransform().Scale, FVector2D(.5));
	Modal->AdvanceWindowAnimationForTesting(10);
	TestEqual(TEXT("No synthetic countdown after elapsed time"), Recovery->GetText().ToString(), Unknown);
	TestEqual(TEXT("Entry settles without overshoot"), Tip->GetRenderTransform().Scale, FVector2D(1));
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("PlayerActionTipInside")))->OnClicked.Broadcast();
	TestNotNull(TEXT("Clicking content preserves popup"), Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot")));
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("PlayerActionTipClickAway")))->OnClicked.Broadcast();
	TestNull(TEXT("Click away removes popup immediately"), Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot")));
	TestNotNull(TEXT("Profile remains below popup"), Modal->GetWidgetFromName(TEXT("ProfilePictureButton")));
	Help->OnClicked.Broadcast();
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("ProfileKillsHelpButton")))->OnClicked.Broadcast();
	TestNull(TEXT("Kill tooltip replaces action tooltip"), Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot")));
	TestNotNull(TEXT("Existing kill tooltip still opens"), Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot")));
	Help->OnClicked.Broadcast();
	TestNull(TEXT("Action tooltip replaces kill tooltip"), Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot")));
	Modal->OpenPlayerHeadSelection();
	TestNull(TEXT("Changing modal clears action tip"), Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot")));
	return true;
}
#endif
