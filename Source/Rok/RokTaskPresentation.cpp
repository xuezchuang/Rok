#include "RokTaskPresentation.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "RokUiTextBlock.h"
#include "RokUiWidgets.h"
#include "Widgets/SWidget.h"

namespace
{
UTextBlock* ReadState(const URokUiPageWidget* Template, FName Name)
{
	auto* Label = Template ? Template->Get<UTextBlock>(Name) : nullptr;
	if (auto* RoleText = Cast<URokUiTextBlock>(Label))
	{
		RoleText->ApplyFontRole();
	}
	return Label;
}
}

URokUiPageWidget* RokTaskPresentation::CreateTemplate(UUserWidget& Owner)
{
	static const TSoftClassPtr<URokUiPageWidget> Class(FSoftObjectPath(
		TEXT("/Game/RokUI/Styles/WBP_RokTaskPresentation.WBP_RokTaskPresentation_C")));
	UClass* Loaded = URokUiSettings::Resolve(Class);
	return Loaded ? CreateWidget<URokUiPageWidget>(&Owner, Loaded) : nullptr;
}

bool RokTaskPresentation::ApplyTitleColor(const URokUiPageWidget* Template, UTextBlock& Label, bool bSide)
{
	const auto* State = Template ? Template->Get<UTextBlock>(bSide ? TEXT("HudSideTitle") : TEXT("HudMainTitle")) : nullptr;
	if (!State)
	{
		return false;
	}
	Label.SetColorAndOpacity(State->GetColorAndOpacity());
	return true;
}

bool RokTaskPresentation::ApplyServerRowText(const URokUiPageWidget* Template, UTextBlock& Label, bool bDescription)
{
	const auto* State = ReadState(Template, bDescription ? TEXT("ServerDescription") : TEXT("ServerTitle"));
	if (!State)
	{
		return false;
	}
	Label.SetFont(State->GetFont());
	Label.SetTextOverflowPolicy(State->GetTextOverflowPolicy());
	return true;
}

bool RokTaskPresentation::ApplyTabBadge(const URokUiPageWidget* Template, UTextBlock& Label, bool bOverflow)
{
	const auto* State = ReadState(Template, bOverflow ? TEXT("TabBadgeOverflow") : TEXT("TabBadgeNormal"));
	if (!State)
	{
		return false;
	}
	Label.SetFont(State->GetFont());
	Label.SetColorAndOpacity(State->GetColorAndOpacity());
	// Offset is an authored transform relative to the existing page's label slot.
	// Setting it absolutely makes repeated binds and 99+ -> normal idempotent.
	Label.SetRenderTransform(State->GetRenderTransform());
	return true;
}

bool RokTaskPresentation::CaptureFit(UTextBlock& Label, const UWidget& Bounds, FRokTaskTextFit& OutFit)
{
	const auto* Slot = Cast<UCanvasPanelSlot>(Bounds.Slot);
	if (!Slot || Slot->GetAnchors().Minimum != Slot->GetAnchors().Maximum
		|| Slot->GetSize().X <= 0 || Slot->GetSize().Y <= 0)
	{
		return false;
	}
	if (auto* RoleText = Cast<URokUiTextBlock>(&Label))
	{
		RoleText->ApplyFontRole();
	}
	OutFit.Font = Label.GetFont();
	OutFit.Bounds = Slot->GetSize();
	return OutFit.Font.Size > 0;
}

void RokTaskPresentation::FitText(UTextBlock& Label, const FText& Text, FRokTaskTextFit& Fit)
{
	if (Fit.bApplied && Label.GetWrapTextAt() == Fit.Bounds.X && Fit.LastText.EqualTo(Text))
	{
		return;
	}
	Label.SetWrapTextAt(Fit.Bounds.X);
	Label.SetText(Text);
	const TSharedRef<SWidget> SlateText = Label.TakeWidget();
	FSlateFontInfo Font = Fit.Font;
	const float MaximumSize = static_cast<float>(Fit.Font.Size);
	const float MinimumSize = FMath::Min(1.5f, MaximumSize);
	for (float Size = MaximumSize;; Size = FMath::Max(MinimumSize, Size - 0.75f))
	{
		// Unity's one-pixel fitting step corresponds to 0.75 Slate points at 96 DPI.
		Font.Size = Size;
		Label.SetFont(Font);
		SlateText->SlatePrepass(1.0f);
		const FVector2D DesiredSize = SlateText->GetDesiredSize();
		if ((DesiredSize.X <= Fit.Bounds.X && DesiredSize.Y <= Fit.Bounds.Y) || Size <= MinimumSize)
		{
			break;
		}
	}
	Fit.LastText = Text;
	Fit.bApplied = true;
}
