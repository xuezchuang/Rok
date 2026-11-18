#pragma once

#include "Components/Image.h"
#include "RokUiWidgets.h"

/** Detached designer brushes; create once for a screen build or march refresh. */
namespace RokArmyStateStyles
{
inline FName RarityName(int32 Rare)
{
	return FName(Rare == 5 ? TEXT("ArmyFrameLegendary") : Rare == 4 ? TEXT("ArmyFrameEpic")
		: Rare == 3 ? TEXT("ArmyFrameElite") : Rare == 2 ? TEXT("ArmyFrameAdvanced") : TEXT("ArmyFrameBase"));
}

struct FBatch
{
	explicit FBatch(UUserWidget* Owner)
	{
		static const TSoftClassPtr<URokUiPageWidget> Class(FSoftObjectPath(
			TEXT("/Game/RokUI/Hud/WBP_RokWorldArmyStateStyles.WBP_RokWorldArmyStateStyles_C")));
		if (UClass* Resolved = URokUiSettings::Resolve(Class))
		{
			Page = CreateWidget<URokUiPageWidget>(Owner, Resolved);
		}
	}

	bool Apply(UImage* Target, FName Name, const FVector2D& ImageSize, bool bMultiplyTint,
		bool bPreserveTargetBrush = false) const
	{
		const UImage* Source = Page ? Cast<UImage>(RokUiFind::Find(Page, Name)) : nullptr;
		if (!Target || !Source || !Source->GetBrush().GetResourceObject())
		{
			return false;
		}
		FSlateBrush Brush = bPreserveTargetBrush ? Target->GetBrush() : Source->GetBrush();
		if (bPreserveTargetBrush)
		{
			Brush.SetResourceObject(Source->GetBrush().GetResourceObject());
			Brush.SetUVRegion(static_cast<FBox2f>(Source->GetBrush().GetUVRegion()));
		}
		Brush.ImageSize = ImageSize;
		Target->SetBrush(Brush);
		Target->SetColorAndOpacity(bMultiplyTint
			? Target->GetColorAndOpacity() * Source->GetColorAndOpacity() : Source->GetColorAndOpacity());
		return true;
	}

	// The batch is synchronous and never enters the visible hierarchy.
	URokUiPageWidget* Page = nullptr;
};
}
