#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
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
#include "../RokUnityGameplayCatalogSubsystem.h"

namespace
{
class FCheckAllianceGiftReference final : public IAutomationLatentCommand
{
public:
	explicit FCheckAllianceGiftReference(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/AllianceGiftReferenceAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Alliance gift reference timed out in step %d. Start on the city map with -RokLocalOnly -RokOpenModal=alliance_joined."), Step));
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
				if (!Find(TEXT("AllianceJoinedFixtureTag")) || !WindowSettled()) return false;
				Catalog = Hud->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
				if (!Test.TestTrue(TEXT("Offline static gift catalog"), Catalog.IsValid() && Catalog->LoadAllianceGiftReadonly())) return true;
				Test.TestEqual(TEXT("Configured gift levels"), Catalog->GetAllianceGiftLevelReadonlyRows().Num(), 50);
				Test.TestEqual(TEXT("Configured reward definitions"), Catalog->GetAllianceGiftRewardReadonlyRows().Num(), 360);
				if (!Click(TEXT("AllianceFeatureHit_6"))) return true;
				++Step;
				break;
			}
			return false;
		}
		if (!Test.TestTrue(TEXT("Live city alliance reference"), Screen.IsValid() && Catalog.IsValid() && Hud.IsValid())) return true;
		switch (Step)
		{
		case 1:
			if (!CaptureState(1, TEXT("Level1"))) return false;
			CheckLayout(TEXT("AllianceGiftReferenceBack"), TEXT("WBP_RokFeature_AllianceGiftReference_C"));
			CheckLayout(TEXT("AllianceGiftReferenceDefinition_1000001"), TEXT("WBP_RokFeature_AllianceGiftReferenceRow_C"));
			Test.TestTrue(TEXT("No claim or account data is represented"), Label(TEXT("AllianceGiftReferenceBoundary")).Contains(TEXT("领取状态均需服务器数据")));
			break;
		case 2:
			if (!Click(TEXT("AllianceGiftReferencePrevious"))) return true;
			CheckDefinitions(1);
			if (!Click(TEXT("AllianceGiftReferenceNext"))) return true;
			break;
		case 3:
			if (!CaptureState(2, TEXT("Level2"))) return false;
			break;
		case 4:
			if (CurrentLevel < 50)
			{
				if (!Click(TEXT("AllianceGiftReferenceNext"))) return true;
				++CurrentLevel;
				return false;
			}
			if (!CaptureState(50, TEXT("Level50"))) return false;
			break;
		case 5:
			if (!Click(TEXT("AllianceGiftReferenceNext"))) return true;
			CheckDefinitions(50);
			if (!Click(TEXT("AllianceGiftReferenceSpecial"))) return true;
			break;
		case 6:
			if (!CaptureState(0, TEXT("Special"))) return false;
			break;
		case 7:
			if (!Click(TEXT("AllianceGiftReferenceSpecial"))) return true;
			CheckDefinitions(50);
			if (!Click(TEXT("AllianceGiftReferenceSpecial")) || !Click(TEXT("AllianceGiftReferencePrevious"))) return true;
			CheckDefinitions(49);
			if (!Click(TEXT("AllianceGiftReferenceBack"))) return true;
			Test.TestTrue(TEXT("Back starts the existing close animation with the gift page mounted"),
				Find(TEXT("AllianceGiftReferenceBack")) && Screen->GetWindowAnimationForTesting().IsAnimating());
			break;
		case 8:
			if (!Find(TEXT("AllianceFeatureHit_6")) || !WindowSettled()) return false;
			Test.TestNull(TEXT("Animated back detaches gift page"), Find(TEXT("AllianceGiftReferenceBack")));
			if (!Click(TEXT("AllianceFeatureHit_6"))) return true;
			break;
		case 9:
			if (!WindowSettled()) return false;
			CheckDefinitions(1);
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			break;
		case 10:
		{
			if (Screen->IsScreenOpen()) return false;
			auto* Button = Cast<UButton>(RokUiFind::Find(Hud, TEXT("AllianceFeatureButton")));
			if (!Test.TestTrue(TEXT("Live city HUD has the normal alliance entry"), Button && Button->GetIsEnabled())) return true;
			Button->OnClicked.Broadcast();
			Test.TestNotNull(TEXT("Normal alliance opens the unjoined welcome"), Find(TEXT("AllianceWelcomeJoin")));
			Test.TestNull(TEXT("Normal alliance cannot open the gift reference from welcome"), Find(TEXT("AllianceFeatureHit_6")));
			return true;
		}
		}
		++Step;
		return false;
	}

private:
	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }

	bool WindowSettled() const
	{
		return Screen.IsValid() && !Screen->GetWindowAnimationForTesting().IsAnimating();
	}

	FString Label(FName Name) const
	{
		const auto* Text = Cast<UTextBlock>(Find(Name));
		return Text ? Text->GetText().ToString() : FString();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Find(Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->IsVisible() && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	void CheckLayout(FName Name, const TCHAR* ClassName)
	{
		const auto* Widget = Find(Name);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s comes from the expected layout"), *Name.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
	}

	void CheckDefinitions(int32 DisplayedLevel)
	{
		const auto* Level = Catalog->FindAllianceGiftLevelReadonly(DisplayedLevel);
		Test.TestEqual(TEXT("Gift level label"), Label(TEXT("AllianceGiftReferenceLevel")), Level
			? FString::Printf(TEXT("配置等级 %d / 50"), DisplayedLevel) : FString(TEXT("特殊类型 · 配置等级 0")));
		Test.TestEqual(TEXT("Gift threshold label"), Label(TEXT("AllianceGiftReferenceThreshold")), Level
			? FString::Printf(TEXT("该等级配置阈值 exp=%d · 宝箱配置 ID=%d"), Level->ExpThreshold, Level->TreasureId)
			: FString(TEXT("特殊类型无等级阈值；下列仅为静态定义")));
		Test.TestEqual(TEXT("Toggle labels the other catalog view"), Label(TEXT("AllianceGiftReferenceSpecialLabel")),
			Level ? FString(TEXT("特殊类型定义")) : FString(TEXT("常规等级定义")));
		int32 Count = 0;
		for (const auto& Definition : Catalog->GetAllianceGiftRewardReadonlyRows())
		{
			if (Definition.GiftLevel != DisplayedLevel) continue;
			const auto* Border = Cast<UBorder>(Find(*FString::Printf(TEXT("AllianceGiftReferenceRow_%d"), Definition.Id)));
			const FLinearColor Expected = Count % 2 == 0 ? FLinearColor(0.19f, 0.18f, 0.16f, 0.98f) : FLinearColor(0.24f, 0.22f, 0.19f, 0.98f);
			Test.TestTrue(TEXT("Row ID has a visible alternating border"), Border && Border->IsVisible() && Border->GetBrushColor().Equals(Expected));
			Test.TestEqual(TEXT("Gift row renders the static definition fields"), Label(*FString::Printf(TEXT("AllianceGiftReferenceDefinition_%d"), Definition.Id)),
				FString::Printf(TEXT("类型 %d · 定义 ID %d · 奖励包 ID %d · 礼物积分 %d · 钥匙点数 %d"),
					Definition.GiftType, Definition.Id, Definition.RewardPackageId, Definition.GiftPoint, Definition.KeyPoint));
			++Count;
		}
		Test.TestEqual(TEXT("Expected static definitions in this view"), Count, Level ? 7 : 10);
		Test.TestNull(TEXT("Rows from the preceding level are detached"), Find(DisplayedLevel == 1
			? TEXT("AllianceGiftReferenceDefinition_2000000") : TEXT("AllianceGiftReferenceDefinition_1000001")));
	}

	bool CaptureState(int32 DisplayedLevel, const TCHAR* Name)
	{
		bool bReady = WindowSettled();
		for (const TCHAR* Art : {TEXT("FeatureFrameBodyLeft_Center"), TEXT("AllianceGiftReferenceBackBackground_Center")})
		{
			const auto* Image = Cast<UImage>(Find(Art));
			const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
			bReady = bReady && Texture && Texture->GetResource() && Texture->GetResource()->TextureRHI.IsValid();
#if WITH_EDITOR
			bReady = bReady && !Texture->IsCompiling();
#endif
		}
		SettleFrames = bReady ? SettleFrames + 1 : 0;
		if (SettleFrames < 3) return false;
		SettleFrames = 0;
		CheckDefinitions(DisplayedLevel);
		if (URokUiSettings::IsLegacyForced() && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceGiftExportSpecs")))
		{
			const FString SpecDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Specs"));
			IFileManager::Get().MakeDirectory(*SpecDir, true);
			FString Error;
			if (!Screen->ExportCurrentModeSpec(SpecDir / FString::Printf(TEXT("feature_AllianceGiftReference%s.json"), Name), Error))
			{
				Test.AddError(FString::Printf(TEXT("Exporting gift layout %s failed: %s"), Name, *Error));
			}
		}
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok alliance gift audit capture: %s"), *CapturePath);
		return true;
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokUnityGameplayCatalogSubsystem> Catalog;
	FString Output;
	FString CapturePath;
	double Deadline = FPlatformTime::Seconds() + 55.0;
	int32 Step = 0;
	int32 CurrentLevel = 2;
	int32 SettleFrames = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceGiftReferenceRenderedTest, "Rok.Ui.Alliance.GiftReferenceRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokAllianceGiftReferenceRenderedTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Gift reference replay is explicitly offline"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckAllianceGiftReference(*this));
	return true;
}

#endif
