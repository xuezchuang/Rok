#pragma once

#include "Components/Image.h"
#include "RokUiWidgets.h"

/** Detached presentation defaults; callers resolve once per popup or reward rebuild. */
namespace RokWorldObjectStyles
{
inline const TSoftClassPtr<URokUiPageWidget>& Class()
{
	static const TSoftClassPtr<URokUiPageWidget> StylesClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldObjectStyles.WBP_RokWorldObjectStyles_C")));
	return StylesClass;
}

inline FName ResourceName(const FString& Kind, bool bProgress)
{
	if (Kind.Equals(TEXT("food"), ESearchCase::IgnoreCase))
	{
		return bProgress ? TEXT("WorldResourceFoodProgress") : TEXT("WorldResourceFoodInfo");
	}
	if (Kind.Equals(TEXT("wood"), ESearchCase::IgnoreCase))
	{
		return bProgress ? TEXT("WorldResourceWoodProgress") : TEXT("WorldResourceWoodInfo");
	}
	if (Kind.Equals(TEXT("stone"), ESearchCase::IgnoreCase))
	{
		return bProgress ? TEXT("WorldResourceStoneProgress") : TEXT("WorldResourceStoneInfo");
	}
	return bProgress ? TEXT("WorldResourceGoldProgress") : TEXT("WorldResourceGoldInfo");
}

inline FName QualityName(int32 Quality)
{
	switch (Quality)
	{
	case 2: return TEXT("WorldRewardQualityBlue");
	case 3: return TEXT("WorldRewardQualityPurple");
	case 4: return TEXT("WorldRewardQualityEpic");
	case 5: return TEXT("WorldRewardQualityLegendary");
	default: return TEXT("WorldRewardQualityCommon");
	}
}

inline bool Apply(URokUiPageWidget* Page, UImage* Target, FName Name)
{
	const UImage* Source = Page ? Page->Get<UImage>(Name) : nullptr;
	if (!Target || !Source || !Source->GetBrush().GetResourceObject())
	{
		return false;
	}
	// Copy the authored size and brush tint; the visible WBP keeps its slot and image tint.
	Target->SetBrush(Source->GetBrush());
	return true;
}
}
