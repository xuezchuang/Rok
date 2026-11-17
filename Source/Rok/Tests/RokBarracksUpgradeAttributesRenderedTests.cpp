#if WITH_DEV_AUTOMATION_TESTS

#include "../RokBarracksSourceTextWidget.h"
#include "../RokBarracksUpgradeAttributes.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokUiSpec.h"
#include "../RokUiWidgets.h"
#include "../RokUnityModalWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

namespace
{
class FCheckBarracksUpgradeAttributes final : public IAutomationLatentCommand
{
public:
	explicit FCheckBarracksUpgradeAttributes(FAutomationTestBase& InTest)
		: Test(InTest)
	{
	}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Barracks upgrade attribute check timed out at step %d"), Step));
			return true;
		}
		if (Step == 0)
		{
			return FindCityAndOpen();
		}
		if (!Test.TestTrue(TEXT("Barracks and modal remain in the city"), Modal.IsValid() && Buildings.IsValid() && Barracks.IsValid()))
		{
			return true;
		}
		if (Step == 1)
		{
			return VerifyAndCapture();
		}
		if (Step == 2)
		{
			if (IFileManager::Get().FileSize(*CapturePath) <= 0)
			{
				return false;
			}
			auto* Close = Cast<UButton>(RokUiFind::Find(Modal, TEXT("Type2CloseButton")));
			if (!Test.TestTrue(TEXT("Original Type2 close button enabled"), Close && Close->GetIsEnabled() && Close->IsVisible()))
			{
				return true;
			}
			Close->OnClicked.Broadcast();
			++Step;
			return false;
		}
		if (Modal->IsModalOpen())
		{
			return false;
		}
		Test.TestNull(TEXT("Close releases Barracks attribute block"), RokUiFind::Find(Modal, TEXT("BarracksUpgradeAttributes")));
		Test.TestEqual(TEXT("Preview leaves level unchanged"), Buildings->GetBuildingStateForActor(Barracks.Get()).Level, 1);
		Test.TestEqual(TEXT("Preview leaves queues unchanged"), Buildings->GetActiveUpgradeCount(), ActiveQueues);
		const FRokResourceStock After = Buildings->GetCurrentResources();
		Test.TestTrue(TEXT("Preview spends no resources"), After.Food == Stock.Food && After.Wood == Stock.Wood
			&& After.Stone == Stock.Stone && After.Gold == Stock.Gold && After.Denar == Stock.Denar);
		return true;
	}

private:
	bool FindCityAndOpen()
	{
		for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
		{
			UWorld* World = Hud->GetWorld();
			if (!Hud->IsInViewport() || !World || World->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
			{
				continue;
			}
			Modal = Cast<URokUnityModalWidget>(RokUiFind::Find(*Hud, TEXT("UnityModalWidget")));
			Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
			if (!Test.TestTrue(TEXT("Live city HUD and building subsystem"), Modal.IsValid() && Buildings.IsValid()))
			{
				return true;
			}
			for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
			{
				if (Actor->ActorHasTag(TEXT("UnityInitId_6")) && Actor->ActorHasTag(TEXT("UnityBuildingType_9")))
				{
					Barracks = *Actor;
					break;
				}
			}
			if (!Barracks.IsValid())
			{
				return false;
			}
			const FRokBuildingState State = Buildings->GetBuildingStateForActor(Barracks.Get());
			if (!Test.TestTrue(TEXT("Actual source barracks type 9 level 1 is idle"), State.UnityInitId == 6
				&& State.UnityBuildingTypeId == 9 && State.Level == 1 && !State.bUpgradeInProgress))
			{
				return true;
			}
			Stock = Buildings->GetCurrentResources();
			ActiveQueues = Buildings->GetActiveUpgradeCount();
			Hud->SetSelectedBuilding(Barracks.Get());
			auto* Upgrade = Cast<UButton>(RokUiFind::Find(*Hud, TEXT("BuildingUpgradeButton")));
			if (!Test.TestTrue(TEXT("Real building-panel upgrade button is enabled"), Upgrade && Upgrade->GetIsEnabled() && Upgrade->IsVisible()))
			{
				return true;
			}
			Upgrade->OnClicked.Broadcast();
			++Step;
			return false;
		}
		return false;
	}

	bool VerifyAndCapture()
	{
		if (!Test.TestTrue(TEXT("Upgrade button opens modal for the selected barracks"), Modal->IsModalOpen()
			&& Modal->GetSelectedBuildingActorForAudit() == Barracks.Get()))
		{
			return true;
		}
		auto* Attributes = Cast<URokBarracksUpgradeAttributes>(RokUiFind::Find(Modal, TEXT("BarracksUpgradeAttributes")));
		if (!Test.TestNotNull(TEXT("Actual mounted attributes"), Attributes))
		{
			return true;
		}
		const bool bLegacy = URokUiSettings::IsLegacyForced();
		if (!Test.TestEqual(TEXT("Expected runtime class selected"), Attributes->GetClass()->GetFName(), bLegacy
			? URokBarracksUpgradeAttributes::StaticClass()->GetFName() : FName(TEXT("WBP_RokModalItem_BarracksUpgradeAttributes_C"))))
		{
			return true;
		}
		auto* Root = Cast<UCanvasPanel>(Attributes->GetWidgetFromName(TEXT("BarracksUpgradeAttributeRoot")));
		if (!Test.TestNotNull(TEXT("Attribute root preserves original direct name"), Root))
		{
			return true;
		}
		const FVector2D RootSize = Root->GetCachedGeometry().GetLocalSize();
		if (Modal->GetWindowAnimationForTesting().IsAnimating() || RootSize.IsNearlyZero())
		{
			return false;
		}
		const auto* Background = Cast<UImage>(Attributes->GetWidgetFromName(TEXT("BarracksUpgradeAttributeBackground_4")));
		const auto* Atlas = Background ? Cast<UTexture2D>(Background->GetBrush().GetResourceObject()) : nullptr;
		if (!Test.TestNotNull(TEXT("Center slice retains source atlas"), Atlas))
		{
			return true;
		}
		bool bReady = !Atlas->IsCompiling() && Atlas->GetResource() && Atlas->GetResource()->TextureRHI.IsValid();
		const TCHAR* Labels[] = {TEXT("步兵单位训练容量"), TEXT("战力")};
		const TCHAR* Values[] = {TEXT("20 + <color=green>30</color>"), TEXT("5 + <color=green>5</color>")};
		for (int32 Row = 0; Row < 2; ++Row)
		{
			const FString Index = FString::FromInt(Row);
			const auto* Label = Cast<URokBarracksSourceTextWidget>(Attributes->GetWidgetFromName(*(TEXT("UpgradeStatLabel_") + Index)));
			const auto* Value = Cast<URokBarracksSourceTextWidget>(Attributes->GetWidgetFromName(*(TEXT("UpgradeStatCurrent_") + Index)));
			const auto* Line = Cast<UImage>(Attributes->GetWidgetFromName(*(TEXT("UpgradeStatSeparator_") + Index)));
			if (!Test.TestTrue(TEXT("Bound label/value retain exact Unity glyph keys and values"), Label && Value
				&& Label->GetText().ToString() == Labels[Row] && Value->GetText().ToString() == Values[Row]
				&& Label->GetSourceKey() == TEXT("label_") + Index && Value->GetSourceKey() == TEXT("value_") + Index)
				|| !Test.TestTrue(TEXT("Separator visible only after the first row"), Line && Line->IsVisible() == (Row == 0)))
			{
				return true;
			}
			bReady = bReady && Label->GetPaintCount() > 0 && Value->GetPaintCount() > 0
				&& Label->GetLastPaintSourceKey() == Label->GetSourceKey() && Value->GetLastPaintSourceKey() == Value->GetSourceKey();
		}
		StableFrames = bReady ? StableFrames + 1 : 0;
		if (StableFrames < 3)
		{
			return false;
		}
		Test.TestEqual(TEXT("Only two source rows"), Attributes->GetAttributeCount(), 2);
		Test.TestTrue(TEXT("Original attribute viewport size"), RootSize.Equals(FVector2D(536, 110), 0.02));
		Test.TestNull(TEXT("No unchanged attack third row"), Attributes->GetWidgetFromName(TEXT("UpgradeStatLabel_2")));
		for (int32 Patch = 0; Patch < 9; ++Patch)
		{
			Test.TestNotNull(TEXT("All nine source slices are mounted"), Cast<UImage>(Attributes->GetWidgetFromName(
				*FString::Printf(TEXT("BarracksUpgradeAttributeBackground_%d"), Patch))));
		}
		if (bLegacy && FParse::Param(FCommandLine::Get(), TEXT("RokBarracksAttributesExportSpec")))
		{
			const FString SpecDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Specs"));
			IFileManager::Get().MakeDirectory(*SpecDir, true);
			FString Error;
			if (!RokUiSpec::ExportChildren(Root, SpecDir / TEXT("modal_BarracksUpgradeAttributes.json"), TEXT("BarracksUpgradeAttributes"), Error))
			{
				Test.AddError(Error);
				return true;
			}
		}
		FString Output;
		if (!FParse::Value(FCommandLine::Get(), TEXT("RokBarracksAttributesAuditOutput="), Output))
		{
			Output = FPaths::ProjectSavedDir() / TEXT("RokUi/BarracksAttributesAudit") / FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
		}
		Output = FPaths::ConvertRelativePathToFull(Output);
		IFileManager::Get().MakeDirectory(*Output, true);
		const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Modal.Get());
		const FVector2D PixelScale = UWidgetLayoutLibrary::GetViewportSize(Modal.Get()) / Viewport.GetLocalSize();
		const FVector2D Min = Viewport.AbsoluteToLocal(Root->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector)) * PixelScale;
		const FVector2D Max = Viewport.AbsoluteToLocal(Root->GetCachedGeometry().LocalToAbsolute(RootSize)) * PixelScale;
		const FString Region = FString::Printf(TEXT("{\"left\":%.6f,\"top\":%.6f,\"right\":%.6f,\"bottom\":%.6f}"), Min.X, Min.Y, Max.X, Max.Y);
		if (!Test.TestTrue(TEXT("Write attribute screenshot region"), FFileHelper::SaveStringToFile(Region, *(Output / TEXT("roi.json")))))
		{
			return true;
		}
		CapturePath = Output / TEXT("BarracksUpgradeAttributes.png");
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok barracks attribute capture: %s"), *CapturePath);
		++Step;
		return false;
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<AActor> Barracks;
	FRokResourceStock Stock;
	FString CapturePath;
	double Deadline = FPlatformTime::Seconds() + 55.0;
	int32 ActiveQueues = 0;
	int32 Step = 0;
	int32 StableFrames = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBarracksUpgradeAttributesRenderedTest, "Rok.Ui.BuildingUpgrade.BarracksAttributesRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRokBarracksUpgradeAttributesRenderedTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Real city barracks test runs offline"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckBarracksUpgradeAttributes(*this));
	return true;
}

#endif
