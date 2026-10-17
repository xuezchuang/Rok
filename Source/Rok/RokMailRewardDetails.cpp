#include "RokMailRewardDetails.h"

#include "RokDataPaths.h"
#include "RokUnityEconomyCatalogSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokTrainArmyDetail.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString N0(int64 Quantity)
{
	const FString Digits = LexToString(Quantity);
	const int32 Start = Digits.StartsWith(TEXT("-")) ? 1 : 0;
	FString Result;
	Result.Reserve(Digits.Len() + Digits.Len() / 3);
	if (Start) Result.AppendChar(TEXT('-'));
	for (int32 Index = Start; Index < Digits.Len(); ++Index)
	{
		if (Index > Start && (Digits.Len() - Index) % 3 == 0) Result.AppendChar(TEXT(','));
		Result.AppendChar(Digits[Index]);
	}
	return Result;
}
}

bool RokMailRewardDetails::LoadFixture(ERokMailRewardFixture Fixture, FRokMailRewardPayload& Out, FString& Error)
{
	Out = FRokMailRewardPayload(); Error.Reset();
	const TCHAR* Key = nullptr;
	switch (Fixture)
	{
	case ERokMailRewardFixture::ReferenceSingle: Key = TEXT("reference_single"); break;
	case ERokMailRewardFixture::MixedAudit: Key = TEXT("mixed_audit"); break;
	case ERokMailRewardFixture::EmptyAlternate: Key = TEXT("empty_alternate"); break;
	default: Error = TEXT("Unsupported mail reward fixture"); return false;
	}
	FString Json; TSharedPtr<FJsonObject> Root;
	const FString Path = RokDataPaths::Resolve(TEXT("mail_reward_presentation.json"));
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{ Error = TEXT("Mail reward presentation missing/invalid: run BuildRokMailRewardPresentation.py"); return false; }
	FString Schema;
	const TSharedPtr<FJsonObject>* Fixtures = nullptr;
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	const TSharedPtr<FJsonObject>* Payload = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.mail_reward_presentation.v1")
		|| !Root->TryGetObjectField(TEXT("fixtures"), Fixtures) || !Fixtures || !Fixtures->IsValid()
		|| !(*Fixtures)->TryGetObjectField(Key, Entry) || !Entry || !Entry->IsValid()
		|| !(*Entry)->TryGetObjectField(TEXT("payload"), Payload) || !Payload || !Payload->IsValid())
	{ Error = TEXT("Mail reward presentation schema/fixture invalid"); return false; }
	FRokMailRewardPayload Parsed;
	if (!(*Payload)->TryGetNumberField(TEXT("food"), Parsed.Food)
		|| !(*Payload)->TryGetNumberField(TEXT("wood"), Parsed.Wood)
		|| !(*Payload)->TryGetNumberField(TEXT("stone"), Parsed.Stone)
		|| !(*Payload)->TryGetNumberField(TEXT("gold"), Parsed.Gold)
		|| !(*Payload)->TryGetNumberField(TEXT("denar"), Parsed.Denar))
	{ Error = TEXT("Mail reward fixture currency fields invalid"); return false; }
	auto ReadAmounts = [&Error, &Payload](const TCHAR* Field, const TCHAR* IdField, const TCHAR* NumField,
		TArray<FRokMailRewardAmount>& Values) -> bool
	{
		const TArray<TSharedPtr<FJsonValue>>* Source = nullptr;
		if (!(*Payload)->TryGetArrayField(Field, Source) || !Source)
		{ Error = FString::Printf(TEXT("Mail reward fixture %s missing"), Field); return false; }
		for (const TSharedPtr<FJsonValue>& Value : *Source)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			FRokMailRewardAmount Amount;
			if (!Object.IsValid() || !Object->TryGetNumberField(IdField, Amount.Id)
				|| !Object->TryGetNumberField(NumField, Amount.Quantity))
			{ Error = FString::Printf(TEXT("Mail reward fixture %s entry invalid"), Field); return false; }
			Values.Add(Amount);
		}
		return true;
	};
	if (!ReadAmounts(TEXT("soldiers"), TEXT("id"), TEXT("num"), Parsed.Soldiers)
		|| !ReadAmounts(TEXT("items"), TEXT("itemId"), TEXT("itemNum"), Parsed.Items)) return false;
	Out = MoveTemp(Parsed);
	return true;
}

bool RokMailRewardDetails::LoadTemplate(FRokMailRewardTemplate& Out, FString& Error)
{
	Out = FRokMailRewardTemplate(); Error.Reset();
	FString Json; TSharedPtr<FJsonObject> Root;
	const FString Path = RokDataPaths::Resolve(TEXT("mail_reward_presentation.json"));
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{ Error = TEXT("Mail reward presentation missing/invalid: run BuildRokMailRewardPresentation.py"); return false; }
	FString Schema;
	const TSharedPtr<FJsonObject>* Template = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.mail_reward_presentation.v1")
		|| !Root->TryGetObjectField(TEXT("mail_template"), Template) || !Template || !Template->IsValid())
	{ Error = TEXT("Mail reward template schema invalid"); return false; }
	FRokMailRewardTemplate Parsed;
	const TSharedPtr<FJsonValue> Time = (*Template)->TryGetField(TEXT("send_time"));
	if (!(*Template)->TryGetNumberField(TEXT("mail_id"), Parsed.MailId) || Parsed.MailId != 100030
		|| !(*Template)->TryGetNumberField(TEXT("activity_id"), Parsed.ActivityId) || Parsed.ActivityId != 10101
		|| !(*Template)->TryGetStringField(TEXT("title"), Parsed.Title) || Parsed.Title.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("activity_name"), Parsed.ActivityName) || Parsed.ActivityName.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("subtitle"), Parsed.Subtitle) || Parsed.Subtitle.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("body_rich"), Parsed.BodyRich) || Parsed.BodyRich.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("body_plain"), Parsed.BodyPlain) || Parsed.BodyPlain.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("body_greeting"), Parsed.BodyGreeting) || Parsed.BodyGreeting.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("body_prefix"), Parsed.BodyPrefix) || Parsed.BodyPrefix.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("body_activity_quoted"), Parsed.BodyActivityQuoted) || Parsed.BodyActivityQuoted.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("body_suffix"), Parsed.BodySuffix) || Parsed.BodySuffix.IsEmpty()
		|| !(*Template)->TryGetStringField(TEXT("claim_label"), Parsed.ClaimLabel) || Parsed.ClaimLabel.IsEmpty()
		|| !Time.IsValid() || Time->Type != EJson::Null)
	{ Error = TEXT("Mail reward template fields invalid"); return false; }
	if (Parsed.BodyPlain != Parsed.BodyGreeting + TEXT("\n") + Parsed.BodyPrefix
		+ Parsed.BodyActivityQuoted + Parsed.BodySuffix)
	{ Error = TEXT("Mail reward template body fragments inconsistent"); return false; }
	Out = MoveTemp(Parsed);
	return true;
}

namespace
{
FRokMailRewardRow BaseRow(ERokMailRewardKind Kind, int64 Id, int64 Quantity)
{
	FRokMailRewardRow Row;
	Row.Kind = Kind;
	Row.Id = Id;
	Row.Quantity = Quantity;
	Row.QuantityText = N0(Quantity);
	return Row;
}
}

TArray<FRokMailRewardRow> RokMailRewardDetails::Normalize(
	const FRokMailRewardPayload& Payload,
	const URokUnityEconomyCatalogSubsystem* EconomyCatalog,
	const URokUnityGameplayCatalogSubsystem* GameplayCatalog)
{
	TArray<FRokMailRewardRow> Rows;
	// Currency.csv IDs, l_desID Chinese text and iconID; same order as OnEnclosureList.
	struct FCurrency { int32 Id; int64 Quantity; const TCHAR* Name; const TCHAR* Icon; };
	const FCurrency Currencies[] = {
		{100, Payload.Food, TEXT("食物"), TEXT("ui_common[img_com_cur1]")},
		{101, Payload.Wood, TEXT("木材"), TEXT("ui_common[img_com_cur2]")},
		{102, Payload.Stone, TEXT("铁矿石"), TEXT("ui_common[img_com_cur3]")},
		{103, Payload.Gold, TEXT("金币"), TEXT("ui_common[img_com_cur4]")},
		{104, Payload.Denar, TEXT("钻石"), TEXT("ui_common[img_com_cur5]")},
	};
	for (const FCurrency& Currency : Currencies)
	{
		if (Currency.Quantity <= 0) continue;
		FRokMailRewardRow Row = BaseRow(ERokMailRewardKind::Currency, Currency.Id, Currency.Quantity);
		Row.DisplayName = Currency.Name;
		Row.Icon = Currency.Icon;
		Row.bKnownMetadata = true;
		Rows.Add(MoveTemp(Row));
	}

	TMap<int32, FString> TroopNameCache;
	for (const FRokMailRewardAmount& Soldier : Payload.Soldiers)
	{
		FRokMailRewardRow Row = BaseRow(ERokMailRewardKind::Soldier, Soldier.Id, Soldier.Quantity);
		Row.DisplayName = FString::Printf(TEXT("兵种 #%lld"), static_cast<long long>(Soldier.Id));
		FRokUnityCatalogTroop Troop;
		if (GameplayCatalog && Soldier.Id >= MIN_int32 && Soldier.Id <= MAX_int32
			&& GameplayCatalog->GetTroop(static_cast<int32>(Soldier.Id), Troop))
		{
			Row.Icon = Troop.Icon;
			Row.TroopLevel = Troop.Level;
			Row.TroopArmsType = Troop.ArmsType;
			Row.bKnownMetadata = true;
			const int32 TroopId = static_cast<int32>(Soldier.Id);
			const FString* KnownName = TroopNameCache.Find(TroopId);
			if (!KnownName)
			{
				FRokTrainArmyDetail Detail; FString Error;
				TroopNameCache.Add(TroopId, RokTrainArmyDetail::Load(TroopId, Detail, Error) ? Detail.Name : FString());
				KnownName = TroopNameCache.Find(TroopId);
			}
			if (KnownName && !KnownName->IsEmpty()) Row.DisplayName = *KnownName;
		}
		Rows.Add(MoveTemp(Row));
	}

	const TArray<FRokUnityEconomyItem> CatalogItems = EconomyCatalog ? EconomyCatalog->GetItems() : TArray<FRokUnityEconomyItem>();
	for (const FRokMailRewardAmount& Item : Payload.Items)
	{
		FRokMailRewardRow Row = BaseRow(ERokMailRewardKind::Item, Item.Id, Item.Quantity);
		Row.DisplayName = FString::Printf(TEXT("物品 #%lld"), static_cast<long long>(Item.Id));
		if (Item.Id >= MIN_int32 && Item.Id <= MAX_int32)
		{
			const FRokUnityEconomyItem* Metadata = CatalogItems.FindByPredicate(
				[&Item](const FRokUnityEconomyItem& Candidate) { return Candidate.Id == Item.Id; });
			if (Metadata)
			{
				if (!Metadata->DisplayNameZhCn.IsEmpty()) Row.DisplayName = Metadata->DisplayNameZhCn;
				Row.Icon = Metadata->Icon;
				Row.Quality = Metadata->Quality;
				Row.bKnownMetadata = true;
			}
		}
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}
