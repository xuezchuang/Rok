#pragma once

#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "RokUiWidgets.h"
#include "RokSproto.h"
#include "RokMailRewardDetails.h"

// Layout/style stays in the WBP. These helpers only bind values and read
// authored geometry for repeated entries and data-driven chart primitives.
namespace RokMailWbp
{
inline UWidget* Find(const URokUiPageWidget& Page, FName Name)
{
	return RokUiFind::Find(&Page, Name);
}
inline void SetText(const URokUiPageWidget& Page, FName Name, const FString& Value)
{
	if (auto* Text = Cast<UTextBlock>(Find(Page, Name)))
	{
		Text->SetText(FText::FromString(Value));
	}
}
inline void Show(const URokUiPageWidget& Page, FName Name, bool bShow)
{
	if (auto* Widget = Find(Page, Name))
	{
		Widget->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
inline FVector2D Size(const URokUiPageWidget& Page, FName MarkerName)
{
	const auto* Marker = Find(Page, MarkerName);
	const auto* Slot = Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
	return Slot ? Slot->GetSize() : FVector2D::ZeroVector;
}
inline FRokMailRewardPayload ReadRewards(const TSharedPtr<FJsonObject>& Rewards)
{
	FRokMailRewardPayload Payload;
	auto Number = [&Rewards](const TCHAR* Name)
	{
		int64 Value = 0;
		RokSprotoJson::TryGetInt64(Rewards, Name, Value);
		return Value;
	};
	Payload.Food = Number(TEXT("food"));
	Payload.Wood = Number(TEXT("wood"));
	Payload.Stone = Number(TEXT("stone"));
	Payload.Gold = Number(TEXT("gold"));
	Payload.Denar = Number(TEXT("denar"));
	auto Entries = [&Rewards](const TCHAR* Field, const TCHAR* IdField, const TCHAR* CountField, TArray<FRokMailRewardAmount>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Rewards && Rewards->TryGetArrayField(Field, Values))
		{
			for (const auto& Value : *Values)
			{
				if (Value && Value->Type == EJson::Object)
				{
					int64 Id = 0, Count = 0;
					RokSprotoJson::TryGetInt64(Value->AsObject(), IdField, Id);
					RokSprotoJson::TryGetInt64(Value->AsObject(), CountField, Count);
					Out.Add({Id, Count});
				}
			}
		}
	};
	Entries(TEXT("soldiers"), TEXT("id"), TEXT("num"), Payload.Soldiers);
	Entries(TEXT("items"), TEXT("itemId"), TEXT("itemNum"), Payload.Items);
	return Payload;
}
inline FString LocalTime(int64 UnixTime, const TCHAR* Format = TEXT("%Y/%m/%d %H:%M"))
{
	return UnixTime > 0 && UnixTime < 253402300799LL
		? (FDateTime::FromUnixTimestamp(UnixTime) + (FDateTime::Now() - FDateTime::UtcNow())).ToString(Format) : FString();
}
}
