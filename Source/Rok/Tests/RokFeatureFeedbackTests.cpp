#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
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
class FCheckFeatureFeedback final : public IAutomationLatentCommand
{
public:
	explicit FCheckFeatureFeedback(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("Feature feedback did not complete the city commander and mail routes."));
			return true;
		}
		for (TObjectIterator<URokMainHudWidget> It; It; ++It)
		{
			if (!It->IsInViewport())
			{
				continue;
			}
			auto* Screen = It->GetFeatureScreenWidgetForPlacement();
			if (!Screen)
			{
				continue;
			}
			if (!bCommanderChecked)
			{
				auto* Upgrade = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSkillUpgrade")));
				if (!Screen->IsScreenOpen() || !Upgrade)
				{
					return false;
				}
				CheckFeedback(*Screen, TEXT("WBP_RokFeature_FeatureFeedback_C"), FString());
				Upgrade->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				CheckFeedback(*Screen, TEXT("WBP_RokFeature_FeatureFeedback_C"), TEXT("同步"));
				const auto* Label = Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback"));
				const auto* Layout = Label ? Cast<UCanvasPanelSlot>(Label->Slot) : nullptr;
				Test.TestTrue(TEXT("Commander feedback retains the existing placement"), Layout
					&& Layout->GetPosition().Equals(FVector2D(360, 728), 0.1)
					&& Layout->GetSize().Equals(FVector2D(720, 28), 0.1));
				auto* Back = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderBack")));
				if (!Test.TestNotNull(TEXT("Commander back control"), Back))
				{
					return true;
				}
				Back->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				bCommanderChecked = true;
				return false;
			}
			if (Screen->IsScreenOpen())
			{
				return false;
			}
			auto* Mail = Cast<UButton>(RokUiFind::Find(*It, TEXT("MailFeatureHitButton")));
			if (!Test.TestNotNull(TEXT("Actual city HUD mail entry"), Mail))
			{
				return true;
			}
			Mail->OnClicked.Broadcast();
			CheckFeedback(*Screen, TEXT("WBP_RokFeature_MailLocal_C"), FString());
			if (auto* Compose = Cast<UButton>(Screen->GetWidgetFromName(TEXT("MailCompose"))))
			{
				Compose->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				CheckFeedback(*Screen, TEXT("WBP_RokFeature_MailLocal_C"), TEXT("邮件服务尚未接入"));
			}
			else
			{
				Test.AddError(TEXT("The mail page did not expose its existing compose button."));
			}
			return true;
		}
		return false;
	}

private:
	void CheckFeedback(const URokUnityFeatureScreenWidget& Screen, const TCHAR* ExpectedPage, const FString& ExpectedText)
	{
		const auto* Label = Cast<UTextBlock>(Screen.GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
		const auto* Owner = Label ? Label->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(TEXT("Feedback uses the expected presentation owner"), Label
			&& (URokUiSettings::IsLegacyForced() ? !Owner : Owner && Owner->GetClass()->GetName() == ExpectedPage));
		if (ExpectedText.IsEmpty())
		{
			Test.TestTrue(TEXT("A fresh page starts with feedback hidden"), Label && !Label->IsVisible());
		}
		else
		{
			Test.TestTrue(TEXT("The original boundary message is shown"), Label
				&& Label->GetVisibility() == ESlateVisibility::HitTestInvisible
				&& Label->GetText().ToString().Contains(ExpectedText));
		}
	}

	FAutomationTestBase& Test;
	const double Deadline = FPlatformTime::Seconds() + 45.0;
	bool bCommanderChecked = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokFeatureFeedbackTest, "Rok.Ui.FeatureFeedback.CommanderAndMail",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokFeatureFeedbackTest::RunTest(const FString&)
{
	const TCHAR* Arguments = FCommandLine::Get();
	if (!FParse::Param(Arguments, TEXT("RokLocalOnly")) || !FParse::Param(Arguments, TEXT("RokNoNet")))
	{
		AddError(TEXT("This test requires the offline commander fixture and no network."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckFeatureFeedback(*this));
	return true;
}

#endif
