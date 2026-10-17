#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokModalNineSlice.h"
#include "../RokUnityModalWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokModalNineSliceTest,
	"Rok.Ui.Modal.NineSlice", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokModalNineSliceTest::RunTest(const FString&)
{
	using namespace RokModalNineSlice;
	const FIntRect Rect(436, 165, 499, 220);
	const FMargin Margin(16.f / 63, 13.f / 55, 15.f / 63, 13.f / 55);
	const auto TopLeft = Patch(Rect, Margin, FVector2D(1008, 179.3), 0);
	TestEqual(TEXT("Sprite pixel border instead of 2048 atlas margin"), TopLeft.Size, FVector2D(16, 13));
	TestEqual(TEXT("Unity top row reads the upper source strip"), TopLeft.Source, FIntRect(436, 207, 452, 220));
	const auto BottomRight = Patch(Rect, Margin, FVector2D(1008, 179.3), 8);
	TestTrue(TEXT("Patches cover requested target"), (BottomRight.Position + BottomRight.Size).Equals(FVector2D(1008, 179.3), 0.00001));
	TestEqual(TEXT("Bottom source row retains Unity atlas orientation"), BottomRight.Source, FIntRect(484, 165, 499, 178));
	const auto SmallLeft = Patch(Rect, Margin, FVector2D(15.5, 13), 0);
	const auto SmallRight = Patch(Rect, Margin, FVector2D(15.5, 13), 8);
	TestEqual(TEXT("Small destination scales opposing borders proportionally"), SmallLeft.Size, FVector2D(8, 6.5));
	TestEqual(TEXT("Asymmetric right remains proportional"), SmallRight.Size, FVector2D(7.5, 6.5));
	TestFalse(TEXT("Small target omits collapsed center"), Patch(Rect, Margin, FVector2D(15.5, 13), 4).IsVisible());
	TestFalse(TEXT("Zero border omits corner"), Patch(Rect, FMargin(0), FVector2D(50, 50), 0).IsVisible());
	TestEqual(TEXT("Unsliced center keeps whole source"), Patch(Rect, FMargin(0), FVector2D(50, 50), 4).Source, Rect);
	TestFalse(TEXT("Empty target does not create reversed geometry"), Patch(Rect, Margin, FVector2D(-1, 0), 0).IsVisible());
	const auto Strip = Patch(FIntRect(10, 20, 130, 50), FMargin(43.f / 120, 0, 41.f / 120, 0), FVector2D(120, 26.12), 3);
	TestTrue(TEXT("Zero vertical border preserves a three-piece horizontal strip"), Strip.IsVisible());
	TestEqual(TEXT("Horizontal strip source height retained"), Strip.Source.Height(), 30);
	const auto CoincidentUV = Patch(FIntRect(0, 0, 20, 23), FMargin(.5f, 7.f / 23, .5f, 6.f / 23), FVector2D(528.15, 22), 4);
	TestTrue(TEXT("Action background emits center despite coincident inner UVs"), CoincidentUV.IsVisible());
	TestEqual(TEXT("Coincident center samples the same source U"), CoincidentUV.Source.Width(), 0);
	TestTrue(TEXT("Coincident center covers bar interior"), CoincidentUV.Size.Equals(FVector2D(508.15, 9), .00001));

	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize();
	const TSharedRef<SWidget> Slate = Modal->TakeWidget();
	Modal->OpenPlayerProfile();
	auto* Frame = Cast<UBorder>(Modal->GetWidgetFromName(TEXT("Type1BodyRight")));
	if (!TestNotNull(TEXT("Shared helper retains UBorder identity"), Frame)) return false;
	TestTrue(TEXT("Container no longer batches full-atlas Box geometry"), Frame->Background.DrawAs == ESlateBrushDrawType::NoDrawType);
	TestNotNull(TEXT("Nine slices remain inside border content"), Cast<UCanvasPanel>(Frame->GetContent()));
	TestEqual(TEXT("Whole source half keeps mirror transform"), Frame->GetRenderTransform().Scale, FVector2D(-1, 1));
	auto* Image = Cast<UImage>(Modal->GetWidgetFromName(TEXT("Type1BodyRightSlice_0")));
	if (!TestNotNull(TEXT("Real profile frame has explicit image patch"), Image)) return false;
	TestTrue(TEXT("Patch draws only image UV region"), Image->GetBrush().DrawAs == ESlateBrushDrawType::Image);
	TestEqual(TEXT("Real frame corner uses sprite borders"), CastChecked<UCanvasPanelSlot>(Image->Slot)->GetSize(), FVector2D(493, 278));
	const FLinearColor Tint(.2f, .3f, .4f, .8f);
	SetTint(Frame, Tint);
	TestEqual(TEXT("Brush color remains observable"), Frame->GetBrushColor(), Tint);
	TestEqual(TEXT("Children receive tint including alpha"), Frame->GetContentColorAndOpacity(), Tint);
	TestEqual(TEXT("Child has no duplicate tint multiplication"), Image->GetColorAndOpacity(), FLinearColor::White);
	auto* ActionCenter = Cast<UImage>(Modal->GetWidgetFromName(TEXT("ActionBackgroundSlice_4")));
	if (!TestNotNull(TEXT("Real action background retains coincident-UV center"), ActionCenter)) return false;
	const FBox2f ActionUV = ActionCenter->GetBrush().GetUVRegion();
	TestTrue(TEXT("Coincident UV box remains valid for Slate"), ActionUV.bIsValid);
	TestEqual(TEXT("Coincident source U is not widened or omitted"), ActionUV.Min.X, ActionUV.Max.X);
	auto* Picture = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ProfilePictureButton")));
	if (!TestNotNull(TEXT("Actual profile picture entry"), Picture)) return false;
	Picture->OnClicked.Broadcast();
	auto* Grid = Cast<UBorder>(Modal->GetWidgetFromName(TEXT("PlayerHeadGridPanel")));
	if (!TestNotNull(TEXT("Real tinted atlas caller opens"), Grid)) return false;
	TestEqual(TEXT("Source player-head background retains authored white tint"), Grid->GetBrushColor(), FLinearColor::White);
	TestEqual(TEXT("Existing caller forwards tint to slice subtree"), Grid->GetContentColorAndOpacity(), Grid->GetBrushColor());
	TestNotNull(TEXT("Existing caller retains slice content"), Cast<UCanvasPanel>(Grid->GetContent()));
	return true;
}
#endif
