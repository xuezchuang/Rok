#if WITH_DEV_AUTOMATION_TESTS

#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokUiWidgets.h"
#include "RokUnityModalWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildingPreviewPaneWbpTest, "Rok.Ui.BuildingPreviewPane.RenderedFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FRokBuildingPreviewPaneWbpTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokBuildingPreviewPane"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Buildings = World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	auto* Hud = World ? NewObject<URokMainHudWidget>(World) : nullptr;
	if (!TestNotNull(TEXT("Local city world"), World)
		|| !TestNotNull(TEXT("Local building subsystem"), Buildings)
		|| !TestNotNull(TEXT("City HUD"), Hud)
		|| !TestTrue(TEXT("City HUD initializes"), Hud->Initialize()))
	{
		return false;
	}
	Hud->TakeWidget();
	auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	auto* Panel = Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget")));
	auto* InfoButton = Panel ? Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingInfoButton"))) : nullptr;
	auto* UpgradeButton = Panel ? Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingUpgradeButton"))) : nullptr;
	if (!TestNotNull(TEXT("Shared city modal"), Modal)
		|| !TestNotNull(TEXT("Building menu information action"), InfoButton)
		|| !TestNotNull(TEXT("Building menu upgrade action"), UpgradeButton))
	{
		return false;
	}

	const bool bLegacy = URokUiSettings::IsLegacyForced();
	const auto Find = [Modal](FName Name) { return RokUiFind::Find(Modal, Name); };
	const auto VerifyPreview = [this, &Find, bLegacy](FName ImageName)
	{
		UWidget* PaneCanvasWidget = Find(TEXT("BuildingPreviewPane"));
		auto* PaneCanvas = Cast<UCanvasPanel>(PaneCanvasWidget);
		TestNotNull(TEXT("Building preview pane canvas exists"), PaneCanvas);
		if (!PaneCanvas)
		{
			return;
		}
		TestEqual(TEXT("Preview clips to its WBP/native viewport"), PaneCanvas->GetClipping(), EWidgetClipping::ClipToBoundsAlways);
		if (bLegacy)
		{
			TestNull(TEXT("Legacy route does not mount the pane WBP"), Find(TEXT("BuildingPreviewPaneWbp")));
		}
		else
		{
			auto* PaneWbp = Cast<URokUiPageWidget>(Find(TEXT("BuildingPreviewPaneWbp")));
			TestNotNull(TEXT("Preview pane is a WBP on the WBP route"), PaneWbp);
			if (PaneWbp)
			{
				TestEqual(TEXT("Preview pane uses the registered WBP class"), PaneWbp->GetClass()->GetFName(),
					FName(TEXT("WBP_RokModal_BuildingPreviewPane_C")));
			}
			auto* FallbackText = Cast<UTextBlock>(Find(TEXT("BuildingSourcePreviewUnavailable")));
			TestNotNull(TEXT("WBP contains the authored missing-art fallback"), FallbackText);
			if (FallbackText)
			{
				TestTrue(TEXT("Fallback text is inside the preview pane WBP"), RokUiFind::IsInside(FallbackText, PaneCanvas));
				TestEqual(TEXT("Fallback text is hidden when source art loads"), FallbackText->GetVisibility(), ESlateVisibility::Collapsed);
			}
		}

		auto* Image = Cast<UImage>(Find(ImageName));
		TestNotNull(TEXT("Selected preview image exists"), Image);
		if (Image)
		{
			TestEqual(TEXT("Selected image is paintable"), Image->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestNotNull(TEXT("Selected building texture is loaded"), Image->GetBrush().GetResourceObject());
			if (!bLegacy)
			{
				TestTrue(TEXT("Selected image is authored inside the preview pane WBP"), RokUiFind::IsInside(Image, PaneCanvas));
			}
			const auto* ImageSlot = Cast<UCanvasPanelSlot>(Image->Slot);
			TestNotNull(TEXT("Selected source image has a canvas slot"), ImageSlot);
			if (ImageSlot)
			{
				TestTrue(TEXT("Source-fit image has positive geometry"),
					ImageSlot->GetSize().X > 0.0f && ImageSlot->GetSize().Y > 0.0f);
			}
		}
		const FName OtherImageName = ImageName == TEXT("BuildingInfoImage")
			? TEXT("BuildingUpgradeImage") : TEXT("BuildingInfoImage");
		if (const auto* OtherImage = Cast<UImage>(Find(OtherImageName)))
		{
			TestEqual(TEXT("Inactive page image remains hidden"), OtherImage->GetVisibility(), ESlateVisibility::Collapsed);
		}
		TestNotNull(TEXT("Existing dynamic glow remains mounted"), Find(TEXT("BuildingPreviewGlowWidget")));
	};

	auto* Farm = World->SpawnActor<AActor>();
	Buildings->ApplyServerBuilding(Farm, 2, 11, FIntPoint::ZeroValue);
	Hud->SetSelectedBuilding(Farm);
	InfoButton->OnClicked.Broadcast();
	if (!TestTrue(TEXT("Real building menu opens information"), Modal->IsModalOpen()))
	{
		return false;
	}
	VerifyPreview(TEXT("BuildingInfoImage"));
	Modal->CloseModal();

	Hud->SetSelectedBuilding(Farm);
	UpgradeButton->OnClicked.Broadcast();
	if (!TestTrue(TEXT("Real building menu opens upgrade"), Modal->IsModalOpen()))
	{
		return false;
	}
	VerifyPreview(TEXT("BuildingUpgradeImage"));
	Modal->CloseModal();
	return true;
}

#endif
