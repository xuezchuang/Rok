#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "RokUiFonts.generated.h"

/**
 * Which runtime font a Rok text widget uses.
 *
 * The C++ builders draw text with composite fonts that only exist in memory
 * (Slate's default font, or Unity's Arial + Microsoft YaHei loaded from
 * %WINDIR%\Fonts). A Widget Blueprint cannot serialize those, so a WBP stores
 * the role plus size/typeface, and the role is resolved when the widget is built.
 */
UENUM(BlueprintType)
enum class ERokUiFontRole : uint8
{
	/** Use the serialized font object as it is. */
	Asset,
	/** Slate's default composite font (FCoreStyle::GetDefaultFontStyle). */
	EngineDefault,
	/** Unity's built-in Arial with the Microsoft YaHei fallback (URokMainHudWidget::MakeUnitySourceFont). */
	UnitySource,
};

namespace RokUiFonts
{
	/** Slate's default composite font. Stable for the lifetime of the process. */
	ROK_API TSharedPtr<const FCompositeFont> GetEngineDefaultComposite();

	/** Unity's Arial + YaHei composite, or null when the Windows fonts are missing. */
	ROK_API TSharedPtr<const FCompositeFont> GetUnitySourceComposite();

	/**
	 * The role a live font corresponds to. Returns false for a composite font
	 * that is neither of the known runtime composites (it cannot be represented).
	 */
	ROK_API bool Identify(const FSlateFontInfo& Font, ERokUiFontRole& OutRole);

	/** The serialized font with the role's runtime composite applied (size, typeface, outline kept). */
	ROK_API FSlateFontInfo Resolve(ERokUiFontRole Role, const FSlateFontInfo& Serialized);
}
