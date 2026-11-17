#if WITH_DEV_AUTOMATION_TESTS
#include "Components/Button.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextBlock.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokMainHudWidget.h"
#include "../RokServerCitySubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUiWidgets.h"

namespace
{
class FCheckServerTrainingScreen final : public IAutomationLatentCommand
{
public:
	explicit FCheckServerTrainingScreen(FAutomationTestBase& InTest)
		: Test(InTest), bReady(FParse::Param(FCommandLine::Get(), TEXT("RokTrainingReady")))
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("MigrationReadiness/TrainingParity_20261108");
		IFileManager::Get().MakeDirectory(*Dir, true);
		Output = FPaths::ConvertRelativePathToFull(Dir / (bReady ? TEXT("ReadyToClaim.png") : TEXT("TrainingIdle.png")));
		IFileManager::Get().Delete(*Output);
	}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now >= Deadline)
		{
			Test.AddError(TEXT("Server training screen or screenshot was unavailable."));
			return true;
		}
		if (CaptureTime > 0)
		{
			if (Now < CaptureTime) return false;
			if (!bCaptured)
			{
				FScreenshotRequest::RequestScreenshot(Output, true, false);
				bCaptured = true;
				return false;
			}
			if (IFileManager::Get().FileSize(*Output) <= 0) return false;
			auto* Screen = OpenScreen.Get();
			if (!Test.TestNotNull(TEXT("Training screen after capture"), Screen)) return true;
			auto* Action = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyStart")));
			if (!Test.TestNotNull(TEXT("Training action after capture"), Action)) return true;
			Action->OnClicked.Broadcast();
			Screen->FlushPendingActionForTesting();
			auto* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
			Test.TestTrue(TEXT("Replay explains its write guard"), Feedback && Feedback->GetText().ToString().Contains(TEXT("快照")));
			if (!bReady)
			{
				auto Click = [this, Screen](const TCHAR* Name)
				{
					auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
					if (!Test.TestTrue(Name, Button && Button->GetIsEnabled())) return false;
					Button->OnClicked.Broadcast();
					Screen->FlushPendingActionForTesting();
					return true;
				};
				if (!Click(TEXT("TrainArmyInstant"))) return true;
				const auto* Confirmation = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyConfirmText")));
				Test.TestTrue(TEXT("Instant action asks for explicit gem confirmation"),
					Confirmation && Confirmation->GetText().ToString().Contains(TEXT("13 宝石"))
						&& Confirmation->GetText().ToString().Contains(TEXT("20 名士兵")));
				if (!Click(TEXT("TrainArmyInstantCancel"))) return true;
				Test.TestNull(TEXT("Cancel closes confirmation"), Screen->GetWidgetFromName(TEXT("TrainArmyInstantConfirmation")));
				if (Feedback) Feedback->SetText(FText::GetEmpty());
				if (!Click(TEXT("TrainArmyInstant")) || !Click(TEXT("TrainArmyInstantConfirm"))) return true;
				Test.TestTrue(TEXT("Confirmed action respects the replay write guard"),
					Feedback && Feedback->GetText().ToString().Contains(TEXT("快照")));
			}
			return true;
		}

		for (TObjectIterator<URokMainHudWidget> It; It; ++It)
		{
			if (!It->IsInViewport() || !It->GetWorld()) continue;
			auto* Screen = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
			const auto* City = It->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
			AActor* Building = City ? City->FindBuildingBarAnchor(30) : nullptr;
			if (!Screen || !Building) continue;
			if (bReady)
			{
				const auto* Ready = RokUiFind::Find(*It, TEXT("TrainingPopReady_0"));
				auto* Bubble = Cast<UButton>(RokUiFind::Find(*It, TEXT("TrainingPopReadyAction_0")));
				if (!Ready || !Ready->IsVisible() || !Bubble) continue;
				Bubble->OnClicked.Broadcast();
			}
			else
			{
				Screen->OpenTrainArmy(Building, 3);
			}
			auto* Action = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyStart")));
			const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyStartLabel")));
			Test.TestTrue(TEXT("Training screen binds the actual archery range"), Action && Action->GetIsEnabled());
			Test.TestTrue(TEXT("Action matches the server queue"), Label
				&& Label->GetText().ToString() == (bReady ? TEXT("领取") : TEXT("训练")));
			if (!bReady)
			{
				Test.TestEqual(TEXT("Archery range starts with the source training capacity"), Screen->GetSelectedTrainCountForAudit(), 20);
				auto Text = [Screen](const TCHAR* Name)
				{
					const auto* Widget = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
					return Widget ? Widget->GetText().ToString() : FString();
				};
				Test.TestEqual(TEXT("T1 count under the portrait"), Text(TEXT("TrainArmyTierLabel_0")), FString(TEXT("1,080")));
				Test.TestEqual(TEXT("Locked tier displays a known zero"), Text(TEXT("TrainArmyTierLabel_1")), FString(TEXT("0")));
				const auto* Portrait = Cast<UImage>(Screen->GetWidgetFromName(TEXT("TrainArmyTierIcon_1")));
				Test.TestTrue(TEXT("Locked portrait uses grayscale material"),
					Portrait && Cast<UMaterialInstanceDynamic>(Portrait->GetBrush().GetResourceObject()));
				Test.TestEqual(TEXT("Train button's time"), Text(TEXT("TrainResourceBaseTime")), FString(TEXT("00:05:00")));
				Test.TestEqual(TEXT("Source 300-second instant price"), Text(TEXT("TrainArmyInstantPrice")), FString(TEXT("13")));
				const auto* Status = Screen->GetWidgetFromName(TEXT("TrainArmyServerQueueStatus"));
				Test.TestTrue(TEXT("Idle status no longer overlays character"), Status && !Status->IsVisible());
			}
			OpenScreen = Screen;
			CaptureTime = Now + 1.5;
			return false;
		}
		return false;
	}

private:
	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> OpenScreen;
	FString Output;
	const bool bReady;
	bool bCaptured = false;
	double CaptureTime = 0;
	double Deadline = FPlatformTime::Seconds() + 45;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerTrainingRenderedTest, "Rok.Ui.TrainArmy.ServerRendered",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokServerTrainingRenderedTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckServerTrainingScreen(*this));
	return true;
}
#endif
