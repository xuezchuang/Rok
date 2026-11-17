#include "RokUiTextBlock.h"

void URokUiTextBlock::ApplyFontRole()
{
	if (FontRole != ERokUiFontRole::Asset)
	{
		SetFont(RokUiFonts::Resolve(FontRole, GetFont()));
	}
}

TSharedRef<SWidget> URokUiTextBlock::RebuildWidget()
{
	ApplyFontRole();
	return Super::RebuildWidget();
}

#if WITH_EDITOR
const FText URokUiTextBlock::GetPaletteCategory()
{
	return NSLOCTEXT("RokUi", "PaletteCategory", "Rok");
}
#endif
