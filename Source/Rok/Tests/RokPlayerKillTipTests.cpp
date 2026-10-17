#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokPlayerKillTipAnimation.h"
#include "../RokUnityModalWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPlayerKillTipTest,
	"Rok.Ui.PlayerProfile.KillTip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPlayerKillTipTest::RunTest(const FString&)
{
	TestEqual(TEXT("Source entry midpoint"), RokPlayerKillTipAnimation::Scale(0.11666667f * 0.5f), 0.5f);
	TestEqual(TEXT("Source has no overshoot"), RokPlayerKillTipAnimation::Scale(0.11666667f), 1.0f);
	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize(); Modal->TakeWidget();
	Modal->OpenPlayerProfile(); Modal->AdvanceWindowAnimationForTesting(1);
	auto* Help = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ProfileKillsHelpButton")));
	if (!TestNotNull(TEXT("Existing profile entry"), Help)) return false;
	Help->OnClicked.Broadcast();
	auto* Tip = Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot"));
	if (!TestNotNull(TEXT("Real tooltip group"), Tip)) return false;
	TestEqual(TEXT("Starts at source zero scale"), Tip->GetRenderTransform().Scale, FVector2D(0));
	TestNotNull(TEXT("Profile remains underneath"), Modal->GetWidgetFromName(TEXT("ProfilePictureButton")));
	TestEqual(TEXT("Source wrapper size"), CastChecked<UCanvasPanelSlot>(Tip->Slot)->GetSize(), FVector2D(372.0f, 203.37f));
	const auto* AnchorSlot = CastChecked<UCanvasPanelSlot>(Help->Slot);
	TestEqual(TEXT("Anchors below help center plus arrow height"), CastChecked<UCanvasPanelSlot>(Tip->Slot)->GetPosition().Y,
		AnchorSlot->GetPosition().Y + AnchorSlot->GetSize().Y * 0.5 + 12.0);
	for (int32 Tier = 1; Tier <= 5; ++Tier)
	{
		const auto* Icon = Modal->GetWidgetFromName(*FString::Printf(TEXT("PlayerKillTipTierIcon_%d"), Tier));
		if (!TestNotNull(TEXT("Source tier icon"), Icon)) return false;
		TestEqual(TEXT("Chinese Awake flips saved grid to upper-left"), CastChecked<UCanvasPanelSlot>(Icon->Slot)->GetPosition().X,
			43.0 + ((Tier - 1) % 2) * 144.0);
		auto* Count = Cast<UTextBlock>(Modal->GetWidgetFromName(*FString::Printf(TEXT("PlayerKillTipTierCount_%d"), Tier)));
		if (!TestNotNull(TEXT("All five source tier rows"), Count)) return false;
		TestEqual(TEXT("Unknown server count is not fabricated zero"), Count->GetText().ToString(), FString(TEXT("—")));
	}
	Modal->AdvanceWindowAnimationForTesting(0.25f);
	TestEqual(TEXT("Entry settles"), Tip->GetRenderTransform().Scale, FVector2D(1));
	auto* Inside = Cast<UButton>(Modal->GetWidgetFromName(TEXT("PlayerKillTipInside")));
	if (!TestNotNull(TEXT("Inside hit surface"), Inside)) return false;
	Inside->OnClicked.Broadcast();
	TestNotNull(TEXT("Inside does not close tooltip"), Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot")));
	auto* Away = Cast<UButton>(Modal->GetWidgetFromName(TEXT("PlayerKillTipClickAway")));
	if (!TestNotNull(TEXT("Click-away mask"), Away)) return false;
	Away->OnClicked.Broadcast();
	TestTrue(TEXT("Click away preserves profile modal"), Modal->IsModalOpen());
	Help->OnClicked.Broadcast();
	Modal->OpenPlayerHeadSelection();
	TestEqual(TEXT("Changing screen still uses Type1 lifecycle"), Modal->GetWindowAnimationForTesting().GetSample().Scale, 1.05f);
	Modal->CloseModal();
	return true;
}
#endif
