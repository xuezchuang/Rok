#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "../RokPlayerHeadLayout.h"
#include "../RokUnityModalWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPlayerHeadLayoutTest,
	"Rok.Ui.PlayerHead.SourceLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPlayerHeadLayoutTest::RunTest(const FString&)
{
	using namespace RokPlayerHeadLayout;
	TestEqual(TEXT("Source still has four columns"), CellPosition(3), FVector2D(461.8, 0));
	TestEqual(TEXT("ListView overrides saved prefab row offset"), CellPosition(4), FVector2D(26.8, 135));
	TestEqual(TEXT("Sixteen frame rows have native content height"), ContentHeight(16), 540.0);
	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize();
	const TSharedRef<SWidget> Slate = Modal->TakeWidget();
	Modal->OpenPlayerProfile(); Modal->AdvanceWindowAnimationForTesting(1);
	auto* SourceNote = Cast<UTextBlock>(RokUiFind::Find(Modal, TEXT("ProfileSourceNote")));
	if (!TestNotNull(TEXT("Profile retains local authority explanation"), SourceNote)) return false;
	TestEqual(TEXT("Formula does not overflow profile frame"), SourceNote->GetText().ToString(), FString(TEXT("本地预览 · 角色资料待同步")));
	CastChecked<UButton>(RokUiFind::Find(Modal, TEXT("ProfilePictureButton")))->OnClicked.Broadcast();
	auto* Scroll = CastChecked<UScrollBox>(RokUiFind::Find(Modal, TEXT("PlayerHeadGridScroll")));
	TestTrue(TEXT("Chinese directory is on the right"), CastChecked<UCanvasPanelSlot>(Scroll->Slot)->GetPosition().Equals(ViewportPosition(), .001));
	TestTrue(TEXT("Source viewport uses ten pixel inset"), CastChecked<UCanvasPanelSlot>(Scroll->Slot)->GetSize().Equals(ViewportSize(), .001));
	auto* Preview = CastChecked<UBorder>(RokUiFind::Find(Modal, TEXT("PlayerHeadPreviewPanel")));
	TestTrue(TEXT("Source preview group has no invented dark backing"), Preview->Background.DrawAs == ESlateBrushDrawType::NoDrawType);
	TestTrue(TEXT("Chinese preview panel stays left"), CastChecked<UCanvasPanelSlot>(Preview->Slot)->GetPosition().Equals(FVector2D(208, 111), .001));
	auto* Name = CastChecked<UTextBlock>(RokUiFind::Find(Modal, TEXT("PlayerHeadPreviewName")));
	TestEqual(TEXT("Source 26px name uses 19.5pt"), Name->GetFont().Size, 19.5f);
	auto* Desc = CastChecked<UTextBlock>(RokUiFind::Find(Modal, TEXT("PlayerHeadPreviewDescription")));
	TestEqual(TEXT("Source 18px description uses 13.5pt"), Desc->GetFont().Size, 13.5f);
	TestEqual(TEXT("Source description has no shadow"), Desc->GetShadowOffset(), FVector2D::ZeroVector);
	auto* Selection = CastChecked<UBorder>(RokUiFind::Find(Modal, TEXT("PlayerHeadCell_1000")));
	auto* Portrait = RokUiFind::Find(Modal, TEXT("PlayerHeadChoiceImage_1000"));
	if (!TestNotNull(TEXT("Selected catalog portrait is present without server profile state"), Portrait)) return false;
	TestTrue(TEXT("Opaque source selection disk must render behind portrait"),
		CastChecked<UCanvasPanelSlot>(Selection->Slot)->GetZOrder() < CastChecked<UCanvasPanelSlot>(Portrait->Slot)->GetZOrder());
	auto* Glow = CastChecked<UImage>(RokUiFind::Find(Modal, TEXT("PlayerHeadSelectionRing_1000")));
	TestEqual(TEXT("Source glow alpha is not weakened to conceal ordering errors"), Glow->GetColorAndOpacity(), FLinearColor::White);
	const FBox2f GlowUV = Glow->GetBrush().GetUVRegion();
	TestTrue(TEXT("Selection keeps original atlas UV min"), GlowUV.Min.Equals(FVector2f(405.f / 1024, 203.f / 1024), .000001f));
	TestTrue(TEXT("Selection keeps original atlas UV max"), GlowUV.Max.Equals(FVector2f(540.f / 1024, 338.f / 1024), .000001f));
	auto* FrameTab = CastChecked<UButton>(RokUiFind::Find(Modal, TEXT("PlayerHeadPageButton_1")));
	TestTrue(TEXT("Side tab retains source external position"), CastChecked<UCanvasPanelSlot>(FrameTab->Slot)->GetPosition().Equals(TabPosition(1), .001));
	const auto* InactiveFrameIcon = Cast<UImage>(RokUiFind::Find(Modal, TEXT("PlayerHeadTabIconDark_1")));
	TestTrue(TEXT("Inactive frame tab draws its source icon"),
		InactiveFrameIcon
			&& UWidget::ConvertSerializedVisibilityToRuntime(InactiveFrameIcon->GetVisibility()).IsVisible()
			&& InactiveFrameIcon->GetBrush().GetResourceObject());
	FrameTab->OnClicked.Broadcast();
	const auto* ActiveFrameIcon = Cast<UImage>(RokUiFind::Find(Modal, TEXT("PlayerHeadTabIconSelected_1")));
	TestTrue(TEXT("Active frame tab draws its source icon"),
		ActiveFrameIcon
			&& UWidget::ConvertSerializedVisibilityToRuntime(ActiveFrameIcon->GetVisibility()).IsVisible()
			&& ActiveFrameIcon->GetBrush().GetResourceObject());
	InactiveFrameIcon = Cast<UImage>(RokUiFind::Find(Modal, TEXT("PlayerHeadTabIconDark_1")));
	TestFalse(TEXT("Active frame tab hides the inactive icon"),
		InactiveFrameIcon && UWidget::ConvertSerializedVisibilityToRuntime(InactiveFrameIcon->GetVisibility()).IsVisible());
	Scroll = CastChecked<UScrollBox>(RokUiFind::Find(Modal, TEXT("PlayerHeadGridScroll")));
	const TSharedRef<SWidget> ScrollSlate = Scroll->TakeWidget();
	Scroll->SetScrollOffset(30);
	TestEqual(TEXT("Selection test starts with real Slate offset"), Scroll->GetScrollOffset(), 30.f);
	auto* Last = CastChecked<UButton>(RokUiFind::Find(Modal, TEXT("PlayerHeadChoiceButton_15")));
	const UWidget* Grid = RokUiFind::Find(Modal, TEXT("PlayerHeadGrid"));
	TestTrue(TEXT("Last hit box belongs to the grid"), Grid && RokUiFind::IsInside(Last, Grid));
	TestTrue(TEXT("Last hit box retains the source inset in the grid"),
		RokUiFind::CanvasPosition(Last, Grid).Equals(CellPosition(15) + FVector2D(4.5), .001));
	TestTrue(TEXT("Last hit box is 126 by 126"), CastChecked<UCanvasPanelSlot>(Last->Slot)->GetSize().Equals(FVector2D(126), .001));
	Last->OnClicked.Broadcast();
	Scroll = CastChecked<UScrollBox>(RokUiFind::Find(Modal, TEXT("PlayerHeadGridScroll")));
	const TSharedRef<SWidget> RebuiltScrollSlate = Scroll->TakeWidget();
	TestEqual(TEXT("Selection preserves requested scroll offset"), Scroll->GetScrollOffset(), 30.f);
	TestNotNull(TEXT("Missing last frame remains explicit"), RokUiFind::Find(Modal, TEXT("PlayerHeadPreviewMissingArt")));
	TestFalse(TEXT("Source layout never implies server permission"), CastChecked<UButton>(RokUiFind::Find(Modal, TEXT("PlayerHeadConfirmButton")))->GetIsEnabled());
	TestNotNull(TEXT("Existing close name remains compatible"), RokUiFind::Find(Modal, TEXT("Type1CloseButton")));
	Modal->CloseModal();
	return true;
}
#endif
