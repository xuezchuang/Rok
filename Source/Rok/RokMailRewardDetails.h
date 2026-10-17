#pragma once

#include "CoreMinimal.h"

class URokUnityEconomyCatalogSubsystem;
class URokUnityGameplayCatalogSubsystem;

// Sproto RewardItem(itemId,itemNum) and SoldierInfo(id,num) payload entries.
struct FRokMailRewardAmount
{
	int64 Id = 0;
	int64 Quantity = 0;
};

// Supplied by the caller for one selected email. Static mail templates are not rewards.
struct FRokMailRewardPayload
{
	int64 Food = 0, Wood = 0, Stone = 0, Gold = 0, Denar = 0;
	TArray<FRokMailRewardAmount> Soldiers;
	TArray<FRokMailRewardAmount> Items;
};

enum class ERokMailRewardKind : uint8 { Currency, Soldier, Item };
enum class ERokMailRewardFixture : uint8 { ReferenceSingle, MixedAudit, EmptyAlternate };

struct FRokMailRewardRow
{
	ERokMailRewardKind Kind = ERokMailRewardKind::Currency;
	int64 Id = 0;
	int64 Quantity = 0;
	FString QuantityText;
	FString DisplayName;
	FString Icon;
	int32 Quality = 0;
	int32 TroopLevel = 0;
	int32 TroopArmsType = 0;
	bool bKnownMetadata = false;
};

// Source Mail.csv template paired with the read-only reference payload.
// SendTime is intentionally empty: no EmailInfo timestamp was supplied.
struct FRokMailRewardTemplate
{
	int32 MailId = 0;
	int32 ActivityId = 0;
	FString Title;
	FString ActivityName;
	FString Subtitle;
	FString BodyRich;
	FString BodyPlain;
	FString BodyGreeting;
	FString BodyPrefix;
	FString BodyActivityQuoted;
	FString BodySuffix;
	FString ClaimLabel;
	FString SendTime;
};

namespace RokMailRewardDetails
{
	// Explicit, source-traced read-only fixtures. Callers must provide live mail
	// RewardInfo separately; these synthetic values are never account state.
	ROK_API bool LoadFixture(ERokMailRewardFixture Fixture, FRokMailRewardPayload& Out, FString& Error);
	ROK_API bool LoadTemplate(FRokMailRewardTemplate& Out, FString& Error);

	// Unity MailMediator.OnEnclosureList order: positive currencies, then every
	// soldier and item payload entry, including zero and negative quantities.
	// Pure read-only projection; duplicate payload entries retain their original order.
	ROK_API TArray<FRokMailRewardRow> Normalize(
		const FRokMailRewardPayload& Payload,
		const URokUnityEconomyCatalogSubsystem* EconomyCatalog,
		const URokUnityGameplayCatalogSubsystem* GameplayCatalog);
}
