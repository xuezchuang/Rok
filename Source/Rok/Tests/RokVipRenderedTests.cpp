#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokGameNetSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckVipRendered final : public IAutomationLatentCommand
{
public:
	explicit FCheckVipRendered(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/VipAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	~FCheckVipRendered()
	{
		if (Role) Role->Values = MoveTemp(OriginalRole);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("VIP timed out in step %d. Run the offline VIP role replay."), Step));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (Step == 0) return OpenFromHud();
		if (!Test.TestTrue(TEXT("Live VIP host"), Screen.IsValid() && Role.IsValid())) return true;
		switch (Step)
		{
		case 1:
			if (Text(TEXT("VipProgressText")) != TEXT("150/200") || !IsArtReady()) return false;
			CheckClass(TEXT("VipCurrentLevelText"), TEXT("WBP_RokFeature_Vip_C"));
			CheckClass(TEXT("VipBenefit_0"), TEXT("WBP_RokFeature_VipBenefitRow_C"));
			Test.TestEqual(TEXT("Known current level"), Text(TEXT("VipCurrentLevelText")), FString(TEXT("0")));
			Test.TestTrue(TEXT("First login day's configured points"), Text(TEXT("VipTodayPoints")).Contains(TEXT("40")));
			Test.TestTrue(TEXT("Daily points and free chest show eligible state"), IsEnabled(TEXT("VipDailyPointChest"))
				&& IsEnabled(TEXT("VipClaimFreeChest")) && Find(TEXT("VipDailyPointRedPoint")) && Find(TEXT("VipFreeChestRedPoint")));
			Test.TestNull(TEXT("Server-shaped replay has no local reference label"), Find(TEXT("VipFixtureTag")));
			Capture(TEXT("Account"));
			break;
		case 2:
			if (!Click(TEXT("VipNextPage"))) return true;
			Test.TestTrue(TEXT("Next changes the preview level"), Text(TEXT("VipBenefitTitle")).Contains(TEXT("VIP 1")));
			Test.TestFalse(TEXT("Another level cannot claim this account's chest"), IsEnabled(TEXT("VipClaimFreeChest")));
			Capture(TEXT("Level1"));
			break;
		case 3:
			if (!Click(TEXT("VipPreviousPage")) || !Click(TEXT("VipHelp"))) return true;
			CheckClass(TEXT("VipDailyPointHelpTipText"), TEXT("WBP_RokFeature_VipDailyPointHelpTip_C"));
			if (!Test.TestTrue(TEXT("Help retains the original modal hit-test root"), Find(TEXT("VipDailyPointHelpTipRoot"))
				&& Find(TEXT("VipDailyPointHelpTipRoot"))->GetParent() == Find(TEXT("FeatureModeCanvas")))) return true;
			Test.TestTrue(TEXT("Help text is measured and uses the source font"), !Text(TEXT("VipDailyPointHelpTipText")).IsEmpty()
				&& FMath::IsNearlyEqual(CastChecked<UTextBlock>(Find(TEXT("VipDailyPointHelpTipText")))->GetFont().Size, 13.5f));
			if (!Click(TEXT("VipDailyPointHelpTipBodyHit"))) return true;
			Test.TestNotNull(TEXT("Clicking the tip body keeps the tip open"), Find(TEXT("VipDailyPointHelpTipRoot")));
			Capture(TEXT("Help"));
			break;
		case 4:
			if (!Click(TEXT("VipHelp"))) return true;
			Test.TestNull(TEXT("Second question click closes the tip"), Find(TEXT("VipDailyPointHelpTipRoot")));
			Role->SetNumberField(TEXT("vip"), 100000000.0);
			break;
		case 5:
			if (!Text(TEXT("VipProgressText")).Contains(TEXT("最高等级")) || !IsArtReady()) return false;
			Test.TestTrue(TEXT("Max level hides add points"), Find(TEXT("VipAddPoints")) && !Find(TEXT("VipAddPoints"))->IsVisible());
			Test.TestNull(TEXT("Max level has no next arrow"), Find(TEXT("VipNextPage")));
			Capture(TEXT("Maximum"));
			break;
		case 6:
			for (const TCHAR* Field : {TEXT("vip"), TEXT("continuousLoginDay"), TEXT("vipExpFlag"), TEXT("vipFreeBox")})
				Role->RemoveField(Field);
			break;
		case 7:
			if (!Text(TEXT("VipProgressText")).Contains(TEXT("未同步"))) return false;
			Test.TestTrue(TEXT("Unsynchronized state hides rewards and does not enable claim"), !IsEnabled(TEXT("VipClaimFreeChest"))
				&& !IsEnabled(TEXT("VipDailyPointChest")) && !Find(TEXT("VipDailyPointRedPoint")) && !Find(TEXT("VipFreeChestRedPoint")));
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			break;
		default:
			return !Screen->IsScreenOpen();
		}
		++Step;
		return false;
	}

private:
	bool OpenFromHud()
	{
		for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
		{
			if (!Hud->IsInViewport()) continue;
			const auto* Net = Hud->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
			if (!Net || !Net->IsSnapshotReplay() || !Net->GetRoleInfo()) continue;
			Role = Net->GetRoleInfo();
			OriginalRole = Role->Values;
			Screen = Hud->GetFeatureScreenWidgetForPlacement();
			auto* Button = Cast<UButton>(RokUiFind::Find(*Hud, TEXT("VipHitButton")));
			if (!Test.TestTrue(TEXT("HUD exposes the real VIP entry"), Screen.IsValid() && Button && Button->GetIsEnabled())) return true;
			Button->OnClicked.Broadcast();
			if (!Test.TestTrue(TEXT("HUD click opens VIP"), Screen->IsScreenOpen())) return true;
			++Step;
			return false;
		}
		return false;
	}

	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }
	bool IsEnabled(FName Name) const { const UWidget* Widget = Find(Name); return Widget && Widget->GetIsEnabled(); }
	FString Text(FName Name) const
	{
		const auto* Label = Cast<UTextBlock>(Find(Name));
		return Label ? Label->GetText().ToString() : FString();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Find(Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->GetIsEnabled() && Button->IsVisible())) return false;
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

	bool IsArtReady() const
	{
		if (Screen->GetWindowAnimationForTesting().IsAnimating()) return false;
		for (const TCHAR* Name : {TEXT("VipCurrentLevelBadge"), TEXT("VipFreeChest"), TEXT("VipSpecialChest"), TEXT("VipProgressFill")})
		{
			const auto* Image = Cast<UImage>(Find(Name));
			const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
			if (!Texture || !Texture->GetResource() || !Texture->GetResource()->TextureRHI.IsValid()) return false;
#if WITH_EDITOR
			if (Texture->IsCompiling()) return false;
#endif
		}
		return true;
	}

	void Capture(const TCHAR* Name)
	{
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok VIP audit capture: %s"), *CapturePath);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TSharedPtr<FJsonObject> Role;
	decltype(FJsonObject::Values) OriginalRole;
	FString Output;
	FString CapturePath;
	double Deadline = FPlatformTime::Seconds() + 45.0;
	int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokVipRenderedFlowTest, "Rok.Ui.Vip.RenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokVipRenderedFlowTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckVipRendered(*this));
	return true;
}

#endif
