#if WITH_DEV_AUTOMATION_TESTS
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "../RokWorldMapPlayerController.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokWorldMapCameraPawn.h"
#include "../RokWorldMapEntityActor.h"
#include "../RokWorldMapHudWidget.h"

namespace
{
class FCheckGatherFlow final : public IAutomationLatentCommand
{
public:
	explicit FCheckGatherFlow(FAutomationTestBase& InTest) : Test(InTest)
	{
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/Gather")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now < Resume) return false;
		if (!PendingCapture.IsEmpty())
		{
			if (IFileManager::Get().FileSize(*PendingCapture) <= 0)
			{
				if (Now < Deadline) return false;
				Test.AddError(TEXT("No render captured: ") + PendingCapture);
				return true;
			}
			PendingCapture.Reset();
		}
		if (Step == 0)
		{
			for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport() || !It->GetWorld()) continue;
				const auto* Camera = Cast<ARokWorldMapCameraPawn>(It->GetOwningPlayerPawn());
				if (!Camera) continue;
				ARokWorldMapEntityActor* Target = nullptr;
				double Distance = TNumericLimits<double>::Max();
				for (TActorIterator<ARokWorldMapEntityActor> Entity(It->GetWorld()); Entity; ++Entity)
				{
					if (Entity->GetEntityKind() != TEXT("wood") || Entity->GetServerObjectId().IsEmpty()
						|| Entity->GetCollectorRoleId() > 0 || Entity->GetEntityLevel() != 3) continue;
					const double Candidate = FVector2D::DistSquared(Camera->GetViewCenterUnity(), Entity->GetUnityPosition());
					if (Candidate < Distance) { Distance = Candidate; Target = *Entity; }
				}
				if (!Target) continue;
				Hud = *It;
				Screen = Cast<URokUnityFeatureScreenWidget>(Hud->GetWidgetFromName(TEXT("WorldMapUnityFeatureScreenWidget")));
				if (!Test.TestNotNull(TEXT("World map owns the shared feature screen"), Screen.Get())) return true;
				Hud->ShowWorldObjectInfo(Target);
				if (!Click(Hud.Get(), TEXT("WorldObjectInfoGatherButton"))) return true;
				Test.TestTrue(TEXT("Gather opens the queue first"), Visible(Hud.Get(), TEXT("WorldGatherDispatch")));
				Test.TestFalse(TEXT("Gather does not skip the queue"), Screen->IsScreenOpen());
				++Step;
				Capture(TEXT("DispatchQueue"));
				return false;
			}
			if (Now < Deadline) return false;
			Test.AddError(TEXT("No available level-3 wood point in the runtime snapshot."));
			return true;
		}
		switch (Step++)
		{
		case 1:
			if (!Click(Hud.Get(), TEXT("WorldGatherCreateArmy"))) return true;
			Test.TestFalse(TEXT("Queue closes before the composer"), Visible(Hud.Get(), TEXT("WorldGatherDispatch")));
			Test.TestTrue(TEXT("Create opens the shared composer"), Screen->IsScreenOpen());
			Test.TestNotNull(TEXT("The commander portrait renders"), Screen->GetWidgetFromName(TEXT("ExpeditionArmyPortrait")));
			Test.TestNotNull(TEXT("City soldiers have the shared sliders"), Screen->GetWidgetFromName(TEXT("ExpeditionSoldierSlider_10101")));
			Test.TestTrue(TEXT("Gather uses the march action"), Visible(Screen.Get(), TEXT("GatherMarchLabel")));
			// The first creation triggers async source atlas compilation and streaming.
			Resume = Now + 2.0;
			return false;
		case 2:
			if (const auto* Count = Screen->GetWidgetFromName(TEXT("ExpeditionArmyCount")))
			{
				const auto* Help = Screen->GetWidgetFromName(TEXT("GatherArmyHelp"));
				const FGeometry& CountGeometry = Count->GetCachedGeometry();
				const double CountRight = CountGeometry.LocalToAbsolute({CountGeometry.GetLocalSize().X, 0}).X;
				Test.TestTrue(TEXT("Troop count stays clear of the help icon"),
					Help && CountRight < Help->GetCachedGeometry().GetAbsolutePosition().X);
			}
			Capture(TEXT("CreateArmy"));
			return false;
		case 3:
			if (!Click(Screen.Get(), TEXT("ExpeditionArmyReady"))) return true;
			if (const auto* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("GatherArmyFeedback"))))
				Test.TestTrue(TEXT("Replay explains the write guard"), Feedback->GetText().ToString().Contains(TEXT("快照")));
			else Test.AddError(TEXT("Missing gather feedback"));
			if (!Click(Screen.Get(), TEXT("ExpeditionArmyClear"))) return true;
			Test.TestFalse(TEXT("March disables when troops are cleared"),
				Screen->GetWidgetFromName(TEXT("ExpeditionArmyReady"))->GetIsEnabled());
			if (auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("ExpeditionSoldierSlider_10101"))))
			{
				Slider->SetValue(100);
				Slider->OnValueChanged.Broadcast(100);
			}
			Test.TestTrue(TEXT("Moving a troop slider enables march"),
				Screen->GetWidgetFromName(TEXT("ExpeditionArmyReady"))->GetIsEnabled());
			Click(Screen.Get(), TEXT("FeatureFrameClose"));
			Test.TestFalse(TEXT("Close returns to the map"), Screen->IsScreenOpen());
			return true;
		default:
			return true;
		}
	}

private:
	bool Click(UUserWidget* Widget, FName Name)
	{
		auto* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
		if (!Test.TestTrue(Name.ToString(), Button && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		if (Widget == Screen.Get()) Screen->FlushPendingActionForTesting();
		return true;
	}

	static bool Visible(UUserWidget* Widget, FName Name)
	{
		const auto* Child = Widget->GetWidgetFromName(Name);
		return Child && Child->IsVisible();
	}

	void Capture(const TCHAR* Name)
	{
		IFileManager::Get().MakeDirectory(*Output, true);
		PendingCapture = Output / (FString(Name) + TEXT(".png"));
		Deadline = FPlatformTime::Seconds() + 15.0;
		FScreenshotRequest::RequestScreenshot(PendingCapture, true, false);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokWorldMapHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FString Output;
	FString PendingCapture;
	double Deadline = FPlatformTime::Seconds() + 30.0;
	double Resume = 0.0;
	int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGatherRenderedFlowTest, "Rok.Ui.WorldGather.RenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokGatherRenderedFlowTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckGatherFlow(*this));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGatherActiveRenderedTest, "Rok.Ui.WorldGather.ActiveRendered",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokGatherActiveRenderedTest::RunTest(const FString&)
{
	const FString Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
		TEXT("MigrationReadiness/WorldGatherActive_20261108/Gathering.png"));
	class FCheckActiveGather final : public IAutomationLatentCommand
	{
	public:
		FCheckActiveGather(FAutomationTestBase& InTest, const FString& InOutput)
			: Test(InTest), Output(InOutput) {}
		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (Now > Deadline)
			{
				Test.AddError(TEXT("Active gathering HUD or screenshot was unavailable."));
				return true;
			}
			if (CaptureAt > 0 && Now >= CaptureAt && !bCaptureRequested)
			{
				bCaptureRequested = true;
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output), true);
				IFileManager::Get().Delete(*Output);
				FScreenshotRequest::RequestScreenshot(Output, true, false);
				return false;
			}
			if (bCaptureRequested)
			{
				if (IFileManager::Get().FileSize(*Output) <= 0) return false;
				auto* Hud = ActiveHud.Get();
				if (!Test.TestNotNull(TEXT("Active HUD after capture"), Hud)) return true;
				auto* Recall = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoGatherButton")));
				if (!Test.TestTrue(TEXT("Recall is actionable for the owned point"), Recall && Recall->GetIsEnabled())) return true;
				Recall->OnClicked.Broadcast();
				const auto* Feedback = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoFeedback")));
				Test.TestTrue(TEXT("Snapshot prevents a real recall"), Feedback
					&& Feedback->GetText().ToString().Contains(TEXT("快照")));
				return true;
			}
			if (CaptureAt > 0) return false;
			for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
			{
				auto* Hud = *It;
				if (!Hud->IsInViewport()) continue;
				auto* List = Cast<UScrollBox>(Hud->GetWidgetFromName(TEXT("WorldMarchQueueList")));
				if (!List || !List->IsVisible() || List->GetChildrenCount() != 1) continue;
				auto* Slot = Cast<UContentWidget>(List->GetChildAt(0));
				auto* Row = Slot ? Cast<UButton>(Slot->GetContent()) : nullptr;
				if (!Test.TestNotNull(TEXT("Clickable commander row"), Row)) return true;
				auto* Toggle = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldMarchQueueToggle")));
				if (!Test.TestNotNull(TEXT("Queue toggle"), Toggle)) return true;
				Toggle->OnClicked.Broadcast();
				Test.TestFalse(TEXT("Queue folds"), List->IsVisible());
				Toggle->OnClicked.Broadcast();
				Test.TestTrue(TEXT("Queue expands"), List->IsVisible());
				Row->OnClicked.Broadcast();
				Test.TestTrue(TEXT("Portrait opens the active resource point"), Hud->IsWorldObjectInfoOpen());
				const auto* Count = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("WorldMarchQueueCount")));
				Test.TestTrue(TEXT("Dispatch count reflects the role"), Count && Count->GetText().ToString() == TEXT("1/2"));
				const auto* Collector = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoCollector")));
				Test.TestTrue(TEXT("Collector name comes from the server cityName field"),
					Collector && Collector->GetText().ToString() == TEXT("snow"));
				const auto* Bar = Cast<UProgressBar>(Hud->GetWidgetFromName(TEXT("WorldGatherProgressBar")));
				Test.TestTrue(TEXT("Current amount fills the progress bar"), Bar && Bar->GetPercent() > 0 && Bar->GetPercent() < 1);
				const auto* Amount = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("WorldGatherProgressAmount")));
				Test.TestTrue(TEXT("Gathering uses the recorded army's load"), Amount && Amount->GetText().ToString().Contains(TEXT("33,680")));
				const auto* Label = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoGatherButtonText")));
				Test.TestTrue(TEXT("Owned point shows recall"), Label && Label->GetText().ToString() == TEXT("召回"));
				ActiveHud = Hud;
				CaptureAt = Now + 0.75;
				return false;
			}
			return false;
		}
	private:
		FAutomationTestBase& Test;
		FString Output;
		TWeakObjectPtr<URokWorldMapHudWidget> ActiveHud;
		double Deadline = FPlatformTime::Seconds() + 45;
		double CaptureAt = 0;
		bool bCaptureRequested = false;
	};
	ADD_LATENT_AUTOMATION_COMMAND(FCheckActiveGather(*this, Output));
	return true;
}
namespace
{
class FCheckBarbarianAttackFlow final : public IAutomationLatentCommand
{
public:
	explicit FCheckBarbarianAttackFlow(FAutomationTestBase& InTest) : Test(InTest)
	{
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/BarbarianAttack")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Barbarian attack flow timed out in step %d."), Step));
			return true;
		}
		if (Now < Resume) return false;
		if (!PendingCapture.IsEmpty() && IFileManager::Get().FileSize(*PendingCapture) <= 0) return false;

		if (Step == 0)
		{
			for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport()) continue;
				auto* Feature = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("WorldMapUnityFeatureScreenWidget")));
				if (!Feature || !Feature->IsResourceSearchOpen()) continue;
				Hud = *It;
				Screen = Feature;
				if (!Click(Screen.Get(), TEXT("ResourceSearchSubmit"))) return true;
				Step = 1;
				return false;
			}
			return false;
		}
		if (!Hud.IsValid() || !Screen.IsValid()) { Test.AddError(TEXT("Attack UI went away")); return true; }
		if (Step == 1)
		{
			auto* Controller = Cast<ARokWorldMapPlayerController>(Hud->GetOwningPlayer());
			auto* Target = Controller ? Cast<ARokWorldMapEntityActor>(Controller->GetSelectedWorldMapActor()) : nullptr;
			if (!Target || Screen->IsResourceSearchOpen()) return false;
			if (!Test.TestEqual(TEXT("Search has selected the requested barbarian"), Target->GetServerObjectId(), FString(TEXT("987650001"))))
				return true;
			Test.TestFalse(TEXT("Search marker waits for a click before opening the attack card"), Hud->IsWorldObjectInfoOpen());
			FVector2D ScreenPosition;
			if (!Test.TestTrue(TEXT("The barbarian can be picked on the map"),
				Controller->ProjectWorldLocationToScreen(Target->GetSelectionAnchorWorldPosition(), ScreenPosition, true)
				&& Controller->SelectWorldMapActorAtScreenPosition(ScreenPosition))) return true;
			Test.TestTrue(TEXT("Click opens the attack card"), Hud->IsWorldObjectInfoOpen()
				&& Text(Hud.Get(), TEXT("WorldBarbarianAttackLabel")) == TEXT("攻击"));
			Test.TestTrue(TEXT("Action cost uses the selected monster"), Text(Hud.Get(), TEXT("WorldBarbarianCost")).Contains(TEXT("50")));
			TArray<UTexture*> RewardAtlases;
			for (int32 Index = 0; Index < 6; ++Index)
			{
				const FString Prefix = FString::Printf(TEXT("WorldBarbarianReward_%d"), Index);
				const auto* Icon = Cast<UImage>(Hud->GetWidgetFromName(*(Prefix + TEXT("Icon"))));
				auto* Texture = Icon ? Cast<UTexture>(Icon->GetBrush().GetResourceObject()) : nullptr;
				Test.TestTrue(TEXT("The configured reward has an atlas and quantity"),
					Texture && !Text(Hud.Get(), *(Prefix + TEXT("Count"))).IsEmpty());
				if (Texture) RewardAtlases.AddUnique(Texture);
			}
#if WITH_EDITOR
			FTextureCompilingManager::Get().FinishCompilation(RewardAtlases);
#endif
			Test.TestFalse(TEXT("The server expiry clock is shown"),
				Text(Hud.Get(), TEXT("WorldBarbarianTimeRemaining")).IsEmpty());
			if (!Click(Hud.Get(), TEXT("WorldBarbarianDetails")) || !Click(Hud.Get(), TEXT("WorldBarbarianDetailsBack"))) return true;
			Step = 2;
			Resume = Now + 0.45;
			return false;
		}

		switch (Step++)
		{
		case 2:
			Capture(TEXT("AttackPopup"));
			return false;
		case 3:
			if (auto* Stay = Cast<UCheckBox>(Hud->GetWidgetFromName(TEXT("WorldBarbarianStay")))) Stay->SetIsChecked(true);
			if (!Click(Hud.Get(), TEXT("WorldBarbarianAttack"))) return true;
			if (!Click(Hud.Get(), TEXT("WorldAttackArmy_27"))) return true;
			Test.TestTrue(TEXT("Existing army can be selected"), Text(Hud.Get(), TEXT("WorldGatherCreateLabel")) == TEXT("出征"));
			Resume = Now + 0.4;
			return false;
		case 4:
			Capture(TEXT("ChooseArmy"));
			return false;
		case 5:
			if (!Click(Hud.Get(), TEXT("WorldGatherCreateArmy"))) return true;
			Test.TestTrue(TEXT("Existing army submit respects the replay write guard"),
				Text(Hud.Get(), TEXT("WorldGatherCreateTip")).Contains(TEXT("快照")));
			if (!Click(Hud.Get(), TEXT("WorldGatherSelectNew")) || !Click(Hud.Get(), TEXT("WorldGatherCreateArmy"))) return true;
			Test.TestTrue(TEXT("New army opens the combat composer"), Screen->IsScreenOpen()
				&& Text(Screen.Get(), TEXT("GatherMarchLabel")) == TEXT("出征"));
			Test.TestNotNull(TEXT("City soldiers can be assigned"), Screen->GetWidgetFromName(TEXT("ExpeditionSoldierSlider_10101")));
			Resume = Now + 1.1;
			return false;
		case 6:
			Capture(TEXT("CreateArmy"));
			return false;
		case 7:
		{
			if (!Click(Screen.Get(), TEXT("ExpeditionArmyClear"))) return true;
			auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("ExpeditionSoldierSlider_10101")));
			if (!Test.TestNotNull(TEXT("Soldier slider"), Slider)) return true;
			Slider->SetValue(100);
			Slider->OnValueChanged.Broadcast(100);
			if (!Click(Screen.Get(), TEXT("ExpeditionArmyReady"))) return true;
			Test.TestTrue(TEXT("New army submit respects the replay write guard"),
				Text(Screen.Get(), TEXT("GatherArmyFeedback")).Contains(TEXT("快照")));
			return true;
		}
		default:
			return true;
		}
	}

private:
	bool Click(UUserWidget* Widget, FName Name)
	{
		auto* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
		if (!Test.TestTrue(Name.ToString(), Button && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		if (Widget == Screen.Get()) Screen->FlushPendingActionForTesting();
		return true;
	}

	static FString Text(UUserWidget* Widget, FName Name)
	{
		const auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	}

	void Capture(const TCHAR* Name)
	{
		PendingCapture = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(PendingCapture, true, false);
		Resume = FPlatformTime::Seconds() + 0.15;
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokWorldMapHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FString Output, PendingCapture;
	double Deadline = FPlatformTime::Seconds() + 45.0;
	double Resume = 0.0;
	int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBarbarianAttackRenderedTest, "Rok.Ui.BarbarianAttack.RenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokBarbarianAttackRenderedTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckBarbarianAttackFlow(*this));
	return true;
}
#endif
