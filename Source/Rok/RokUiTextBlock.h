#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "RokUiFonts.h"
#include "RokUiTextBlock.generated.h"

/**
 * A text block whose font face comes from a Rok font role.
 *
 * The WBP keeps size, typeface ("Regular"/"Bold"), outline and colour; the
 * role supplies the in-memory composite font at build time, so the designer
 * preview and the game draw the same glyphs as the C++ builders did.
 */
UCLASS(meta = (DisplayName = "Rok Text"))
class ROK_API URokUiTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok UI")
	ERokUiFontRole FontRole = ERokUiFontRole::EngineDefault;

	/** Re-applies the role to the current font (keeps size, typeface and outline). */
	void ApplyFontRole();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
};
