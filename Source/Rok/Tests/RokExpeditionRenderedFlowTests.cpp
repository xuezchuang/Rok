#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokMainHudWidget.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckExpeditionFlow final : public IAutomationLatentCommand
{
public:
	explicit FCheckExpeditionFlow(FAutomationTestBase& InTest) : Test(InTest)
	{
		Deadline = FPlatformTime::Seconds() + 25.0;
		bLocked = FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionFlowLocked"));
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
			/ TEXT("Automation/Expedition") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (!Screen.IsValid())
		{
			for (TObjectIterator<URokMainHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport()) continue;
				Screen = It->GetFeatureScreenWidgetForPlacement();
				if (!Screen.IsValid()) continue;
				auto* Entry = Cast<UButton>(It->GetWidgetFromName(TEXT("CampaignFeatureButton")));
				if (!Test.TestNotNull(TEXT("City campaign entry"), Entry)) return true;
				IFileManager::Get().MakeDirectory(*Output, true);
				Entry->OnClicked.Broadcast();
				if (bLocked)
				{
					Test.TestFalse(TEXT("Level 4 stays in the city"), Screen->IsScreenOpen());
					const auto* Tip = Cast<UTextBlock>(It->GetWidgetFromName(TEXT("ExpeditionUnlockTip")));
					Test.TestTrue(TEXT("Lock tip shows level 5"), Tip && Tip->IsVisible() && Tip->GetText().ToString().Contains(TEXT("5")));
					Capture(TEXT("Locked"));
				}
				else
				{
					Test.TestTrue(TEXT("Level 5 opens Expedition"), Screen->IsExpeditionModeForAudit());
					Resume = Now + 1.0;
				}
				return false;
			}
			if (Now < Deadline) return false;
			Test.AddError(TEXT("Standalone city HUD did not initialize."));
			return true;
		}
		if (!Pending.IsEmpty())
		{
			if (IFileManager::Get().FileSize(*Pending) <= 0 && Now < Deadline) return false;
			Test.TestTrue(Pending, IFileManager::Get().FileSize(*Pending) > 0);
			Pending.Reset();
			if (bLocked) return true;
		}
		if (Now < Resume) return false;
		Resume = Now + 0.85;
		switch (Step++)
		{
		case 0:
			Test.TestEqual(TEXT("All source stages"), Screen->GetExpeditionRenderedStageCountForAudit(), 80);
			Test.TestTrue(TEXT("Replay account progress is known"), Screen->IsExpeditionAccountStateKnownForAudit());
			for (int32 Id = 1; Id <= 80; ++Id)
			{
				const auto* Flag = Cast<UImage>(Screen->GetWidgetFromName(*FString::Printf(TEXT("ExpeditionStageFlag_%d"), Id)));
				Test.TestTrue(FString::Printf(TEXT("Stage %d has source flag art"), Id), Flag && Flag->GetBrush().GetResourceObject());
			}
			Test.TestNotNull(TEXT("Stage 1 first and normal reward group results"), Screen->GetWidgetFromName(TEXT("ExpeditionReward_5Count")));
			Capture(TEXT("Map"));
			return false;
		case 1:
			if (!Click(TEXT("ExpeditionStageButton_2"))) return true;
			Test.TestFalse(TEXT("Stage 2 locked before stage 1 is cleared"), Enabled(TEXT("ExpeditionChallenge")));
			if (!Click(TEXT("ExpeditionStageButton_1")) || !Click(TEXT("ExpeditionChallenge"))) return true;
			return false;
		case 2:
			Test.TestTrue(TEXT("Challenge opens the objective screen"), Screen->IsExpeditionTaskConfigVisibleForAudit());
			Test.TestNotNull(TEXT("Stage 1 has its second enemy commander"), Screen->GetWidgetFromName(TEXT("ExpeditionEnemyHero_1")));
			Test.TestFalse(TEXT("Start needs a prepared squad"), Enabled(TEXT("ExpeditionChallenge")));
			Test.TestFalse(TEXT("Second formation slot locked in stage 1"), Enabled(TEXT("ExpeditionSquadButton_1")));
			Capture(TEXT("Objectives"));
			return false;
		case 3:
			if (!Click(TEXT("ExpeditionSquadButton_0"))) return true;
			Test.TestNotNull(TEXT("Owned commander has its full-body portrait"), Screen->GetWidgetFromName(TEXT("ExpeditionArmyPortrait")));
			Test.TestNotNull(TEXT("History troops have quantity controls"), Screen->GetWidgetFromName(TEXT("ExpeditionSoldierSlider_10101")));
			return false;
		case 4:
			Capture(TEXT("CreateArmy"));
			return false;
		case 5:
			if (!Click(TEXT("ExpeditionArmyClear"))) return true;
			Test.TestFalse(TEXT("Empty army cannot be prepared"), Enabled(TEXT("ExpeditionArmyReady")));
			if (auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("ExpeditionSoldierSlider_10101"))))
				Slider->SetValue(100);
			if (!Click(TEXT("ExpeditionArmyReady"))) return true;
			Test.TestTrue(TEXT("Prepared squad enables Start"), Enabled(TEXT("ExpeditionChallenge")));
			Capture(TEXT("Prepared"));
			return false;
		case 6:
			if (!Click(TEXT("ExpeditionChallenge"))) return true;
			Test.TestEqual(TEXT("Start stays on the battle integration boundary"), Screen->GetExpeditionLastBoundaryActionIdForAudit(), 1120);
			if (!Click(TEXT("ExpeditionBackButton"))) return true;
			Test.TestFalse(TEXT("Back returns to the selected map stage"), Screen->IsExpeditionTaskConfigVisibleForAudit());
			Test.TestEqual(TEXT("Selected stage preserved"), Screen->GetSelectedExpeditionStageIdForAudit(), 1);
			return true;
		default:
			return true;
		}
	}

private:
	bool Enabled(FName Name) const
	{
		const auto* Button = Screen->GetWidgetFromName(Name);
		return Button && Button->GetIsEnabled();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!Test.TestTrue(Name.ToString(), Button && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	void Capture(const TCHAR* Name)
	{
		Pending = Output / (FString(Name) + TEXT(".png"));
		Deadline = FPlatformTime::Seconds() + 8.0;
		FScreenshotRequest::RequestScreenshot(Pending, true, false);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FString Output;
	FString Pending;
	double Deadline = 0;
	double Resume = 0;
	int32 Step = 0;
	bool bLocked = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokExpeditionRenderedFlowTest, "Rok.Ui.Expedition.RenderedGateAndSetup",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokExpeditionRenderedFlowTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckExpeditionFlow(*this));
	return true;
}

#endif
