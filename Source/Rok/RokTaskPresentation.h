#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UTextBlock;
class UUserWidget;
class UWidget;
class URokUiPageWidget;

/** A row's Designer font is the fitting ceiling; runtime shrinkage never changes it. */
struct FRokTaskTextFit
{
	FSlateFontInfo Font;
	FVector2D Bounds = FVector2D::ZeroVector;
	FText LastText;
	bool bApplied = false;
};

namespace RokTaskPresentation
{
	URokUiPageWidget* CreateTemplate(UUserWidget& Owner);
	bool ApplyTitleColor(const URokUiPageWidget* Template, UTextBlock& Label, bool bSide);
	bool ApplyServerRowText(const URokUiPageWidget* Template, UTextBlock& Label, bool bDescription);
	bool ApplyTabBadge(const URokUiPageWidget* Template, UTextBlock& Label, bool bOverflow);
	bool CaptureFit(UTextBlock& Label, const UWidget& Bounds, FRokTaskTextFit& OutFit);
	void FitText(UTextBlock& Label, const FText& Text, FRokTaskTextFit& Fit);
}
