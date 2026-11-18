#include "RokUnityFeatureScreenWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "RokUiTextBlock.h"
#include "RokUiWidgets.h"
#include "Styling/SlateTypes.h"

namespace
{
FTextBlockStyle ReadTextState(UTextBlock& Label)
{
	// State templates are deliberately not mounted. Resolve the role before
	// reading the font; RebuildWidget would normally do this for visible text.
	if (auto* RoleText = Cast<URokUiTextBlock>(&Label))
	{
		RoleText->ApplyFontRole();
	}
	return FTextBlockStyle().SetFont(Label.GetFont()).SetColorAndOpacity(Label.GetColorAndOpacity())
		.SetShadowOffset(Label.GetShadowOffset()).SetShadowColorAndOpacity(Label.GetShadowColorAndOpacity());
}
}

bool URokUnityFeatureScreenWidget::TryGetFeatureTextStyle(FName StateName, FTextBlockStyle& OutStyle)
{
	if (!FeatureTextStateTemplate)
	{
		UClass* Class = ResolveFeaturePageClass(TEXT("FeatureTextStates"));
		FeatureTextStateTemplate = Class ? CreateWidget<URokUiPageWidget>(this, Class) : nullptr;
	}
	auto* Label = FeatureTextStateTemplate
		? Cast<UTextBlock>(RokUiFind::Find(FeatureTextStateTemplate, StateName)) : nullptr;
	if (!Label)
	{
		return false;
	}
	OutStyle = ReadTextState(*Label);
	return true;
}

bool URokUnityFeatureScreenWidget::TryGetFeatureTextColor(FName StateName, FSlateColor& OutColor)
{
	if (!FeatureTextColorTemplate)
	{
		UClass* Class = ResolveFeaturePageClass(TEXT("FeatureTextColors"));
		FeatureTextColorTemplate = Class ? CreateWidget<URokUiPageWidget>(this, Class) : nullptr;
	}
	auto* Label = FeatureTextColorTemplate
		? Cast<UTextBlock>(RokUiFind::Find(FeatureTextColorTemplate, StateName)) : nullptr;
	if (!Label)
	{
		return false;
	}
	// Preserve Slate's authored color mode; the mounted page still owns font/layout.
	OutColor = Label->GetColorAndOpacity();
	return true;
}

bool URokUnityFeatureScreenWidget::ApplyTaskRewardTextOnlyState(UTextBlock& Label)
{
	if (!TaskRewardTextStateTemplate)
	{
		UClass* Class = ResolveFeaturePageClass(TEXT("TaskRewardTextOnly"));
		TaskRewardTextStateTemplate = Class ? CreateWidget<URokUiPageWidget>(this, Class) : nullptr;
	}
	auto* Source = TaskRewardTextStateTemplate
		? Cast<URokUiTextBlock>(RokUiFind::Find(TaskRewardTextStateTemplate, TEXT("TaskRewardTextOnlyLabel"))) : nullptr;
	const auto* Layout = Source ? Cast<UCanvasPanelSlot>(Source->Slot) : nullptr;
	auto* TargetLayout = Cast<UCanvasPanelSlot>(Label.Slot);
	if (!Source || !Layout || !TargetLayout)
	{
		return false;
	}
	const auto Style = ReadTextState(*Source);
	TargetLayout->SetLayout(Layout->GetLayout());
	Label.SetFont(Style.Font);
	Label.SetColorAndOpacity(Style.ColorAndOpacity);
	Label.SetShadowOffset(Style.ShadowOffset);
	Label.SetShadowColorAndOpacity(Style.ShadowColorAndOpacity);
	Label.SetJustification(Source->GetDesignerJustification());
	return true;
}
