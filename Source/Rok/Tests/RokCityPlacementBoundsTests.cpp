#if WITH_DEV_AUTOMATION_TESTS

#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokPlayerController.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectIterator.h"

namespace
{
class FCheckCityPlacementBounds final : public IAutomationLatentCommand
{
public:
	explicit FCheckCityPlacementBounds(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(FString::Printf(TEXT("City placement did not reach test step %d"), static_cast<int32>(Step)));
			return true;
		}
		if (!Hud.IsValid())
		{
			for (TObjectIterator<URokMainHudWidget> It; It; ++It)
			{
				const UWorld* World = It->GetWorld();
				if (It->IsInViewport() && World && World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityGameAppCity"))
				{
					Hud = *It;
					break;
				}
			}
			if (!Hud.IsValid()) return false;
		}
		auto* Controller = Cast<ARokPlayerController>(Hud->GetOwningPlayer());
		auto* Feature = Hud->GetFeatureScreenWidgetForPlacement();
		auto* Host = Hud->GetWidgetFromName(TEXT("CityPlacementControls"));
		auto* HostSlot = Host ? Cast<UCanvasPanelSlot>(Host->Slot) : nullptr;
		auto* Cancel = Cast<UButton>(RokUiFind::Find(Hud.Get(), TEXT("CityPlacementCancelButton")));
		auto* Confirm = Cast<UButton>(RokUiFind::Find(Hud.Get(), TEXT("CityPlacementConfirmButton")));
		auto* Reason = Cast<UTextBlock>(RokUiFind::Find(Hud.Get(), TEXT("CityPlacementReason")));
		if (!Controller || !HostSlot || !Cancel || !Confirm || !Reason || !Feature
			|| !Feature->OnBuildCityPlacementRequested.IsBound())
		{
			return false;
		}

		switch (Step)
		{
		case EStep::Open:
			if (!SetDesignerFixture()) return true;
			if (const int32 Type = FindAvailableBuilding(); Type > 0)
			{
				Feature->OnBuildCityPlacementRequested.Broadcast(Type);
				if (!Test.TestTrue(TEXT("The real City controller starts a preview-only construction"), Controller->GetCityPlacementPreviewSnapshot().bActive))
				{
					return true;
				}
				Step = EStep::Projected;
				return false;
			}
			Test.AddError(TEXT("No eligible local farm or lumber mill in the actual city map fixture"));
			return true;
		case EStep::Projected:
			if (!Host->IsVisible() || HostSlot->GetAnchors().Minimum != FVector2D::ZeroVector) return false;
			Test.TestTrue(TEXT("Footprint projection retains the authored host size and centered alignment"),
				HostSlot->GetSize().Equals(BoundsSize) && HostSlot->GetAlignment().Equals(FVector2D(0.5, 0.5)));
			Test.TestTrue(TEXT("Projected mode uses the live position and scale"),
				!HostSlot->GetPosition().Equals(BoundsPosition) && Host->GetRenderTransform().Scale.X > 0
				&& Controller->GetCityPlacementPreviewSnapshot().PreviewActor.IsValid() && !Reason->IsVisible());
			if (!Test.TestTrue(TEXT("Preview can be cancelled without building"), Cancel->GetIsEnabled())) return true;
			Cancel->OnClicked.Broadcast();
			Step = EStep::CancelledPreview;
			return false;
		case EStep::CancelledPreview:
			if (Controller->GetCityPlacementPreviewSnapshot().bActive || Host->IsVisible()) return false;
			Feature->OnBuildCityPlacementRequested.Broadcast(INDEX_NONE);
			Step = EStep::Feedback;
			return false;
		case EStep::Feedback:
			if (!Host->IsVisible() || !Reason->IsVisible()) return false;
			Test.TestTrue(TEXT("Failure feedback returns to the authored anchors and bounds"),
				HostSlot->GetAnchors().Minimum.Equals(BoundsAnchor) && HostSlot->GetAnchors().Maximum.Equals(BoundsAnchor)
				&& HostSlot->GetAlignment().Equals(BoundsAlignment) && HostSlot->GetPosition().Equals(BoundsPosition)
				&& HostSlot->GetSize().Equals(BoundsSize));
			Test.TestTrue(TEXT("Failure explains the missing preview with confirmation disabled"),
				!Controller->GetCityPlacementPreviewSnapshot().bActive && !Reason->GetText().IsEmpty() && !Confirm->GetIsEnabled());
			if (!Test.TestTrue(TEXT("Feedback keeps the actual Cancel command available"), Cancel->GetIsEnabled())) return true;
			Cancel->OnClicked.Broadcast();
			Step = EStep::CancelledFeedback;
			return false;
		case EStep::CancelledFeedback:
			return !Host->IsVisible() && !Controller->GetCityPlacementPreviewSnapshot().bActive;
		}
		return true;
	}

private:
	bool SetDesignerFixture()
	{
		const bool bLegacy = URokUiSettings::IsLegacyForced();
		auto* Marker = RokUiFind::Find(Hud.Get(), TEXT("CityPlacementDefaultBounds"));
		if (bLegacy) return Test.TestNull(TEXT("Legacy uses the original native placement rectangle"), Marker);
		auto* BoundsSlot = Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
		if (!Test.TestTrue(TEXT("Live city mounts the WBP's collapsed Designer marker"),
			BoundsSlot && Marker->GetVisibility() == ESlateVisibility::Collapsed)) return false;
		BoundsAnchor = FVector2D(0.42, 1);
		BoundsAlignment = FVector2D(0.5, 0.9);
		BoundsPosition = FVector2D(13, -67);
		BoundsSize = FVector2D(520, 235);
		BoundsSlot->SetAnchors(FAnchors(BoundsAnchor.X, BoundsAnchor.Y));
		BoundsSlot->SetAlignment(BoundsAlignment);
		BoundsSlot->SetPosition(BoundsPosition);
		BoundsSlot->SetSize(BoundsSize);
		return true;
	}

	int32 FindAvailableBuilding() const
	{
		const auto* Buildings = Hud->GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
		for (const int32 Type : {2, 3})
		{
			FRokLocalConstructionPreview Entry;
			if (Buildings && Buildings->PreviewConstructionEntry(Type, Entry) && Entry.bContextVerified && Entry.bSourceEligible)
			{
				return Type;
			}
		}
		return INDEX_NONE;
	}

	enum class EStep { Open, Projected, CancelledPreview, Feedback, CancelledFeedback };
	FAutomationTestBase& Test;
	const double Deadline = FPlatformTime::Seconds() + 35.0;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	EStep Step = EStep::Open;
	FVector2D BoundsAnchor{0.5, 1};
	FVector2D BoundsAlignment{0.5, 1};
	FVector2D BoundsPosition{0, -55};
	FVector2D BoundsSize{472.7f, 220};
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityPlacementBoundsTest, "Rok.Ui.City.Placement.DesignerBounds",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FRokCityPlacementBoundsTest::RunTest(const FString&)
{
	if (!TestTrue(TEXT("City placement verification runs in the isolated offscreen city"),
		FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) && FParse::Param(FCommandLine::Get(), TEXT("RokNoNet"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen")))) return false;
	AddExpectedError(TEXT("Rok city placement preview refused:"), EAutomationExpectedErrorFlags::Contains, 1);
	ADD_LATENT_AUTOMATION_COMMAND(FCheckCityPlacementBounds(*this));
	return true;
}

#endif
