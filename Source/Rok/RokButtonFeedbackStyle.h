#pragma once

#include "CoreMinimal.h"
#include "RokUiWidgets.h"
#include "Components/Button.h"

/** Designer-owned source ColorBlock values; C++ still decides state and tween timing. */
struct FRokButtonFeedbackStyle
{
	struct FColors
	{
		FLinearColor Normal;
		FLinearColor Hovered;
		FLinearColor Pressed;
		FLinearColor Disabled;

		FLinearColor ForState(bool bEnabled, bool bPressed, bool bHighlighted) const
		{
			return !bEnabled ? Disabled : (bPressed ? Pressed : (bHighlighted ? Hovered : Normal));
		}
	};

	bool bAttempted = false;
	TOptional<FColors> AuthoredColors;

	const FColors* Resolve(UUserWidget* Owner, FName StyleName)
	{
		if (!bAttempted)
		{
			bAttempted = true;
			static const TSoftClassPtr<URokUiPageWidget> Class(FSoftObjectPath(
				TEXT("/Game/RokUI/Hud/WBP_RokButtonFeedbackStyles.WBP_RokButtonFeedbackStyles_C")));
			UClass* Resolved = URokUiSettings::Resolve(Class);
			auto* Page = Resolved ? CreateWidget<URokUiPageWidget>(Owner, Resolved) : nullptr;
			const auto* Button = Page ? Cast<UButton>(Page->GetWidgetFromName(StyleName)) : nullptr;
			if (Button)
			{
				// Copy authored values: the style-only page never enters the visible hierarchy.
				const auto& Style = Button->GetStyle();
				AuthoredColors = FColors{Style.Normal.TintColor.GetSpecifiedColor(), Style.Hovered.TintColor.GetSpecifiedColor(),
					Style.Pressed.TintColor.GetSpecifiedColor(), Style.Disabled.TintColor.GetSpecifiedColor()};
			}
		}
		return AuthoredColors.IsSet() ? &AuthoredColors.GetValue() : nullptr;
	}
};
