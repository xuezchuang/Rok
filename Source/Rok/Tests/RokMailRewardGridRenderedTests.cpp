#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RokMainHudWidget.h"
#include "RokUiWidgets.h"
#include "RokUnityFeatureScreenWidget.h"
#include "UObject/UObjectIterator.h"

namespace
{
class FCheckMailRewardGrid final : public IAutomationLatentCommand
{
public:
	explicit FCheckMailRewardGrid(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("The city mail_rewards_preview route did not mount its mixed attachments."));
			return true;
		}
		for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
		{
			if (!Hud->IsInViewport())
			{
				continue;
			}
			auto* Screen = Hud->GetFeatureScreenWidgetForPlacement();
			if (!Screen || !Screen->IsScreenOpen() || !Screen->GetWidgetFromName(TEXT("MailRewardFixtureIcon_6")))
			{
				return false;
			}
			return RunGridChecks(*Screen);
		}
		return false;
	}

private:
	bool RunGridChecks(URokUnityFeatureScreenWidget& Screen)
	{
		const auto* Grid = Cast<UCanvasPanel>(Screen.GetWidgetFromName(TEXT("MailRewardFixtureGrid")));
		if (!Test.TestNotNull(TEXT("The mixed attachment grid is mounted"), Grid))
		{
			return true;
		}
		CheckOwner(*Grid);
		Test.TestEqual(TEXT("Seven rendered cards occupy the grid"), Grid->GetChildrenCount(), 7);
		Test.TestEqual(TEXT("Seven attachments still match the mixed payload"), Screen.GetRenderedMailRewardRowsForAudit().Num(), 7);
		CheckSlot(*Grid, FVector2D(5, 237), FVector2D(820, 310));
		const auto First = RokUiFind::CanvasPosition(Screen.GetWidgetFromName(TEXT("MailRewardFixtureIcon_0")), Grid);
		const auto FirstIconOfSecondRow = RokUiFind::CanvasPosition(Screen.GetWidgetFromName(TEXT("MailRewardFixtureIcon_5")), Grid);
		const auto* FirstCard = Grid->GetChildAt(0);
		const auto* SixthCard = Grid->GetChildAt(5);
		const auto* FirstCardSlot = FirstCard ? Cast<UCanvasPanelSlot>(FirstCard->Slot) : nullptr;
		const auto* SixthCardSlot = SixthCard ? Cast<UCanvasPanelSlot>(SixthCard->Slot) : nullptr;
		Test.TestTrue(TEXT("Five columns wrap to the authored second row"), FirstCardSlot && SixthCardSlot
			&& FirstCardSlot->GetPosition().Equals(FVector2D(20, 0), 0.1)
			&& SixthCardSlot->GetPosition().Equals(FVector2D(20, 170), 0.1)
			&& FirstIconOfSecondRow.Y > First.Y);
		CheckClaimY(Screen, 591.0f);
		Test.TestNull(TEXT("Currency card does not gain an item action"), Screen.GetWidgetFromName(TEXT("MailRewardFixtureHit_0")));
		if (!Click(Screen, TEXT("MailRewardFixtureHit_6")))
		{
			return true;
		}
		Test.TestEqual(TEXT("The second-row item's tooltip uses its original reward index"),
			Screen.GetActiveMailRewardTooltipIndexForAudit(), 6);
		Test.TestNotNull(TEXT("Tooltip arrow hit remains available"), Screen.GetWidgetFromName(TEXT("MailRewardTooltipArrowHit")));
		const FString Fingerprint = Screen.GetMailRewardFixtureFingerprintForAudit();
		if (!Click(Screen, TEXT("MailClaim")))
		{
			return true;
		}
		Test.TestEqual(TEXT("The actual claim button reaches the read-only boundary"), Screen.GetLastMailBoundaryActionIdForAudit(), 240);
		Test.TestEqual(TEXT("The fixture does not mutate any reward"), Screen.GetMailRewardFixtureFingerprintForAudit(), Fingerprint);
		if (!Click(Screen, TEXT("MailRowHit_1")))
		{
			return true;
		}
		const auto* EmptyGrid = Cast<UCanvasPanel>(Screen.GetWidgetFromName(TEXT("MailRewardFixtureGrid")));
		if (Test.TestNotNull(TEXT("An explicit empty attachment list keeps the grid"), EmptyGrid))
		{
			CheckOwner(*EmptyGrid);
			CheckSlot(*EmptyGrid, FVector2D(5, 237), FVector2D(820, 0));
			Test.TestEqual(TEXT("No placeholder reward is created"), EmptyGrid->GetChildrenCount(), 0);
		}
		CheckClaimY(Screen, 281.0f);
		Test.TestNull(TEXT("An empty payload does not invent a missing-payload note"), Screen.GetWidgetFromName(TEXT("MailRewardFixtureEmpty")));
		Test.TestNull(TEXT("The item tooltip closes when switching mail"), Screen.GetWidgetFromName(TEXT("MailRewardTooltipArrowHit")));
		if (!Click(Screen, TEXT("MailRowHit_2")))
		{
			return true;
		}
		const auto* Missing = Cast<UTextBlock>(Screen.GetWidgetFromName(TEXT("MailRewardFixtureEmpty")));
		if (Test.TestNotNull(TEXT("Missing payload uses its existing feedback label"), Missing))
		{
			CheckOwner(*Missing);
			Test.TestTrue(TEXT("The missing attachment note is visible"), Missing->IsVisible() && !Missing->GetText().IsEmpty());
		}
		Test.TestNull(TEXT("Missing payload does not create a grid"), Screen.GetWidgetFromName(TEXT("MailRewardFixtureGrid")));
		if (Click(Screen, TEXT("MailRowHit_0")))
		{
			Test.TestEqual(TEXT("The original mixed attachments return"), Screen.GetRenderedMailRewardRowsForAudit().Num(), 7);
			Test.TestEqual(TEXT("No payload changed across the three states"), Screen.GetMailRewardFixtureFingerprintForAudit(), Fingerprint);
		}
		return true;
	}

	void CheckOwner(const UWidget& Widget)
	{
		const auto* Owner = Widget.GetTypedOuter<URokUiPageWidget>();
		Test.TestTrue(TEXT("The grid state is authored on the expected path"), URokUiSettings::IsLegacyForced()
			? !Owner : Owner && Owner->GetClass()->GetName() == TEXT("WBP_RokFeature_MailRewardGrid_C"));
	}

	void CheckSlot(const UWidget& Widget, FVector2D Position, FVector2D Size)
	{
		const auto* WidgetSlot = Cast<UCanvasPanelSlot>(Widget.Slot);
		Test.TestTrue(TEXT("The grid keeps its source position and size"), WidgetSlot
			&& WidgetSlot->GetPosition().Equals(Position, 0.1) && WidgetSlot->GetSize().Equals(Size, 0.1));
	}

	void CheckClaimY(const URokUnityFeatureScreenWidget& Screen, float Expected)
	{
		const auto* Claim = Screen.GetWidgetFromName(TEXT("MailClaim"));
		const auto* ClaimSlot = Claim ? Cast<UCanvasPanelSlot>(Claim->Slot) : nullptr;
		Test.TestTrue(TEXT("Claim stays below the attachment region"), ClaimSlot
			&& FMath::IsNearlyEqual(ClaimSlot->GetPosition().Y, static_cast<double>(Expected), 0.1));
	}

	bool Click(URokUnityFeatureScreenWidget& Screen, FName Name)
	{
		auto* Button = Cast<UButton>(Screen.GetWidgetFromName(Name));
		if (!Test.TestTrue(Name.ToString(), Button && Button->GetIsEnabled() && Button->IsVisible()))
		{
			return false;
		}
		Button->OnClicked.Broadcast();
		Screen.FlushPendingActionForTesting();
		return true;
	}

	FAutomationTestBase& Test;
	const double Deadline = FPlatformTime::Seconds() + 45.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMailRewardGridRenderedTest, "Rok.Ui.Mail.RewardGridRendered",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokMailRewardGridRenderedTest::RunTest(const FString&)
{
	const TCHAR* Arguments = FCommandLine::Get();
	if (!FParse::Param(Arguments, TEXT("RokLocalOnly")) || !FParse::Param(Arguments, TEXT("RokNoNet")))
	{
		AddError(TEXT("Run the mail_rewards_preview fixture with -RokLocalOnly and -RokNoNet."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckMailRewardGrid(*this));
	return true;
}

#endif
