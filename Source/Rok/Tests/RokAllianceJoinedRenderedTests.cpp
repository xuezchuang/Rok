#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokMainHudWidget.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckAllianceJoinedRendered final : public IAutomationLatentCommand
{
public:
	explicit FCheckAllianceJoinedRendered(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/AllianceJoinedAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Joined alliance audit timed out in step %d. Open the city with -RokLocalOnly -RokOpenModal=alliance_joined."), Step));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (Step == 0)
		{
			for (TObjectIterator<URokMainHudWidget> Candidate; Candidate; ++Candidate)
			{
				if (!Candidate->IsInViewport()) continue;
				Hud = *Candidate;
				Screen = Hud->GetFeatureScreenWidgetForPlacement();
				if (Find(TEXT("AllianceJoinedFixtureTag"))) ++Step;
				break;
			}
			return false;
		}
		if (!Test.TestTrue(TEXT("Live city alliance panel"), Screen.IsValid() && Hud.IsValid())) return true;
		switch (Step)
		{
		case 1:
			if (!IsArtReady(TEXT("AllianceJoinedFlagCloth"))) return false;
			CheckClass(TEXT("AllianceJoinedName"), TEXT("WBP_RokFeature_AllianceJoined_C"));
			Test.TestTrue(TEXT("Explicit joined preview is marked as a local fixture"), Text(TEXT("AllianceJoinedFixtureTag")).Contains(TEXT("非账号状态")));
			Test.TestEqual(TEXT("Overview title"), Text(TEXT("FeatureFrameTitle")), FString(TEXT("联盟")));
			Capture(TEXT("Overview"));
			break;
		case 2:
			if (!Click(TEXT("AllianceMail"))) return true;
			Test.TestEqual(TEXT("Alliance mail retains its server boundary"), Text(TEXT("FeatureBoundaryFeedback")), FString(TEXT("该联盟功能暂未开放。")));
			Test.TestNotNull(TEXT("Boundary leaves overview intact"), Find(TEXT("AllianceJoinedName")));
			if (!Click(TEXT("AlliancePageHit_1"))) return true;
			break;
		case 3:
			if (!IsArtReady(TEXT("AllianceLeaderRank"))) return false;
			CheckClass(TEXT("AllianceOfficeDefinitionHit_1001"), TEXT("WBP_RokFeature_AllianceJoinedOfficeRow_C"));
			CheckClass(TEXT("AllianceOfficeDefinitionHit_4001"), TEXT("WBP_RokFeature_AllianceJoinedOfficeRow_C"));
			Test.TestEqual(TEXT("Members title"), Text(TEXT("FeatureFrameTitle")), FString(TEXT("联盟成员")));
			Test.TestNull(TEXT("Inactive overview actions are detached"), Find(TEXT("AllianceMail")));
			Test.TestTrue(TEXT("Office rows display configured text and boundary"),
				!Text(TEXT("AllianceOfficeName_2001")).IsEmpty() && !Text(TEXT("AllianceOfficeDescription_2001")).IsEmpty()
				&& !Text(TEXT("AllianceOfficeBonus_2001")).IsEmpty() && Text(TEXT("AllianceOfficeReferenceNote")).Contains(TEXT("不代表当前任命")));
			Capture(TEXT("Members"));
			break;
		case 4:
		{
			const auto* First = Cast<UBorder>(Find(TEXT("AllianceOfficeRow_1001")));
			if (!Test.TestNotNull(TEXT("First office row aliases the selected border"), First)) return true;
			const FLinearColor SelectedColor = First->GetBrushColor();
			if (!Click(TEXT("AllianceOfficeDefinitionHit_2001"))) return true;
			const auto* Second = Cast<UBorder>(Find(TEXT("AllianceOfficeRow_2001")));
			First = Cast<UBorder>(Find(TEXT("AllianceOfficeRow_1001")));
			Test.TestTrue(TEXT("Office selection changes only the selected reference style"),
				First && Second && Second->GetBrushColor().Equals(SelectedColor) && !First->GetBrushColor().Equals(SelectedColor));
			if (!Click(TEXT("AllianceMemberOfficeReferenceOpen"))) return true;
			Test.TestNotNull(TEXT("Member link opens the existing office definitions"), Find(TEXT("AllianceOfficeReferenceIntro")));
			if (!Click(TEXT("AllianceOfficeReferenceBack"))) return true;
			Second = Cast<UBorder>(Find(TEXT("AllianceOfficeRow_2001")));
			Test.TestTrue(TEXT("Office back restores member selection"), Second && Second->GetBrushColor().Equals(SelectedColor));
			if (!Click(TEXT("AlliancePageHit_2"))) return true;
			break;
		}
		case 5:
			if (!IsArtReady(TEXT("AllianceSettingIcon_0"))) return false;
			Test.TestEqual(TEXT("Settings title"), Text(TEXT("FeatureFrameTitle")), FString(TEXT("联盟设置")));
			Test.TestNull(TEXT("Inactive member row is detached"), Find(TEXT("AllianceOfficeDefinitionHit_2001")));
			Capture(TEXT("Settings"));
			break;
		case 6:
			if (!Click(TEXT("AllianceSettingHit_2"))) return true;
			Test.TestNotNull(TEXT("Ranking opens its existing reference page"), Find(TEXT("AllianceRankingBack")));
			if (!Click(TEXT("AllianceRankingBack"))) return true;
			Test.TestNotNull(TEXT("Ranking back restores settings"), Find(TEXT("AllianceSettingHit_5")));
			if (!Click(TEXT("AllianceSettingHit_5"))) return true;
			Test.TestNotNull(TEXT("Fixture exit restores unjoined welcome"), Find(TEXT("AllianceWelcomeJoin")));
			Test.TestNull(TEXT("Fixture exit removes joined layout"), Find(TEXT("AllianceJoinedFixtureTag")));
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			break;
		case 7:
		{
			if (Screen->IsScreenOpen()) return false;
			auto* Button = Cast<UButton>(RokUiFind::Find(Hud, TEXT("AllianceFeatureButton")));
			if (!Test.TestTrue(TEXT("Live city HUD exposes the normal alliance entry"), Button && Button->GetIsEnabled())) return true;
			Button->OnClicked.Broadcast();
			Test.TestNotNull(TEXT("Normal HUD entry still opens unjoined alliance"), Find(TEXT("AllianceWelcomeJoin")));
			Test.TestNull(TEXT("Normal HUD entry has no joined fixture"), Find(TEXT("AllianceJoinedFixtureTag")));
			return true;
		}
		}
		++Step;
		return false;
	}

private:
	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }

	FString Text(FName Name) const
	{
		const auto* Label = Cast<UTextBlock>(Find(Name));
		return Label ? Label->GetText().ToString() : FString();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Find(Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->IsVisible() && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	void CheckClass(FName Name, const TCHAR* ClassName)
	{
		const auto* Widget = Find(Name);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s comes from the expected layout"), *Name.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
	}

	bool IsArtReady(FName ImageName)
	{
		const auto* Image = Cast<UImage>(Find(ImageName));
		const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
		bool bReady = !Screen->GetWindowAnimationForTesting().IsAnimating()
			&& Texture && Texture->GetResource() && Texture->GetResource()->TextureRHI.IsValid();
#if WITH_EDITOR
		bReady = bReady && !Texture->IsCompiling();
#endif
		ReadyFrames = bReady ? ReadyFrames + 1 : 0;
		return ReadyFrames >= 3;
	}

	void Capture(const TCHAR* Name)
	{
		ReadyFrames = 0;
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok joined alliance audit capture: %s"), *CapturePath);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FString Output;
	FString CapturePath;
	double Deadline = FPlatformTime::Seconds() + 45.0;
	int32 Step = 0;
	int32 ReadyFrames = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceJoinedRenderedTest, "Rok.Ui.Alliance.JoinedRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokAllianceJoinedRenderedTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Joined reference replay is explicitly offline"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckAllianceJoinedRendered(*this));
	return true;
}

#endif
