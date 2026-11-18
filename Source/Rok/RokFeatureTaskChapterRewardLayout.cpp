#include "RokUnityFeatureScreenWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "RokUiWidgets.h"

bool URokUnityFeatureScreenWidget::PlaceTaskChapterRewardFromLayout(
	UCanvasPanel& Host, UWidget& Cell, FName LayoutName, int32 Index)
{
	if (!TaskChapterRewardLayoutTemplate)
	{
		UClass* Class = ResolveFeaturePageClass(TEXT("TaskChapterRewardLayouts"));
		TaskChapterRewardLayoutTemplate = Class ? CreateWidget<URokUiPageWidget>(this, Class) : nullptr;
	}
	const FString Prefix = LayoutName.ToString();
	const auto* Bounds = TaskChapterRewardLayoutTemplate
		? TaskChapterRewardLayoutTemplate->Get<UWidget>(*(Prefix + TEXT("Cell"))) : nullptr;
	const auto* Next = TaskChapterRewardLayoutTemplate
		? TaskChapterRewardLayoutTemplate->Get<UWidget>(*(Prefix + TEXT("Next"))) : nullptr;
	const auto* BoundsSlot = Bounds ? Cast<UCanvasPanelSlot>(Bounds->Slot) : nullptr;
	const auto* NextSlot = Next ? Cast<UCanvasPanelSlot>(Next->Slot) : nullptr;
	if (!BoundsSlot || !NextSlot || Index < 0 || BoundsSlot->GetSize().X <= 0 || BoundsSlot->GetSize().Y <= 0)
	{
		return false;
	}

	// The unattached WBP owns the first cell and pitch. Right anchors in the
	// catalog variants follow the actual mounted host width, including resizing.
	FAnchorData Layout = BoundsSlot->GetLayout();
	const FVector2D Delta = (NextSlot->GetPosition() - BoundsSlot->GetPosition()) * Index;
	Layout.Offsets.Left += Delta.X;
	Layout.Offsets.Top += Delta.Y;
	if (Layout.Anchors.Minimum.X != Layout.Anchors.Maximum.X)
	{
		Layout.Offsets.Right -= Delta.X;
	}
	if (Layout.Anchors.Minimum.Y != Layout.Anchors.Maximum.Y)
	{
		Layout.Offsets.Bottom -= Delta.Y;
	}
	auto* Target = Host.AddChildToCanvas(&Cell);
	Target->SetLayout(Layout);
	Target->SetAutoSize(BoundsSlot->GetAutoSize());
	Target->SetZOrder(BoundsSlot->GetZOrder());
	return true;
}
