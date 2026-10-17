#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "../RokMailRewardDetails.h"
#include "../RokUnityEconomyCatalogSubsystem.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokTrainArmyDetail.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMailRewardDetailsTest,
	"Rok.Ui.Mail.ReadOnlyRewardDetails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokMailRewardDetailsTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokMailRewardDetailsTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Economy = Instance->GetSubsystem<URokUnityEconomyCatalogSubsystem>();
	auto* Gameplay = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Economy catalog"), Economy) || !TestNotNull(TEXT("Gameplay catalog"), Gameplay)) return false;
	if (!TestTrue(TEXT("Source item catalog loads"), Economy->LoadCatalog())
		|| !TestTrue(TEXT("Source troop catalog loads"), Gameplay->LoadCatalog())) return false;
	const auto Items = Economy->GetItems();
	const auto* KnownItem = Items.FindByPredicate([](const FRokUnityEconomyItem& Item) { return Item.Id > 0 && !Item.Icon.IsEmpty() && !Item.DisplayNameZhCn.IsEmpty(); });
	const auto TroopIds = Gameplay->GetTroopIds();
	FRokUnityCatalogTroop KnownTroop;
	FRokTrainArmyDetail KnownTroopDetail; FString DetailError;
	bool bFoundLocalizedTroop = false;
	for (const int32 TroopId : TroopIds)
	{
		if (Gameplay->GetTroop(TroopId, KnownTroop) && RokTrainArmyDetail::Load(TroopId, KnownTroopDetail, DetailError)
			&& !KnownTroopDetail.Name.IsEmpty()) { bFoundLocalizedTroop = true; break; }
	}
	if (!TestNotNull(TEXT("Known source item"), KnownItem)
		|| !TestTrue(TEXT("Existing Arms package has a localized known troop"), bFoundLocalizedTroop)) return false;
	FRokMailRewardPayload Reference, Mixed, Empty; FString FixtureError;
	FRokMailRewardTemplate Template;
	if (!TestTrue(TEXT("Source mail template loads"), RokMailRewardDetails::LoadTemplate(Template, FixtureError)))
	{ AddError(FixtureError); return false; }
	TestTrue(TEXT("Mail source IDs and dynamic-time boundary"), Template.MailId == 100030
		&& Template.ActivityId == 10101 && Template.SendTime.IsEmpty());
	TestEqual(TEXT("Mail template title"), Template.Title, FString(TEXT("活动奖励补发")));
	TestEqual(TEXT("Activity placeholder resolved via ActivityCalendar"), Template.ActivityName, FString(TEXT("征服之始-领取宝箱")));
	TestEqual(TEXT("Mail template subtitle"), Template.Subtitle, FString(TEXT("征服之始-领取宝箱活动奖励补发")));
	TestTrue(TEXT("Mail template retains source body and claim text"), Template.BodyPlain.Contains(TEXT("尊敬的城主"))
		&& Template.BodyPlain.Contains(Template.ActivityName) && Template.ClaimLabel == TEXT("领取"));
	TestEqual(TEXT("Source body fragments exactly reconstruct plain text"),
		Template.BodyGreeting + TEXT("\n") + Template.BodyPrefix + Template.BodyActivityQuoted + Template.BodySuffix,
		Template.BodyPlain);
	if (!TestTrue(TEXT("Reference fixture loads"), RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::ReferenceSingle, Reference, FixtureError))
		|| !TestTrue(TEXT("Mixed source-shaped fixture loads"), RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::MixedAudit, Mixed, FixtureError))
		|| !TestTrue(TEXT("Empty alternate fixture loads"), RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::EmptyAlternate, Empty, FixtureError)))
	{ AddError(FixtureError); return false; }
	const auto ReferenceRows = RokMailRewardDetails::Normalize(Reference, Economy, Gameplay);
	const auto MixedRows = RokMailRewardDetails::Normalize(Mixed, Economy, Gameplay);
	TestEqual(TEXT("Reference retains one source item"), ReferenceRows.Num(), 1);
	if (ReferenceRows.Num() == 1)
	{
		TestTrue(TEXT("Reference item identity and source metadata"), ReferenceRows[0].Kind == ERokMailRewardKind::Item
			&& ReferenceRows[0].Id == 502090001 && ReferenceRows[0].Quantity == 3
			&& ReferenceRows[0].DisplayName == TEXT("赫雀瑟枝") && ReferenceRows[0].Icon == TEXT("item5[item_5_1031]")
			&& ReferenceRows[0].Quality == 4);
	}
	TestEqual(TEXT("Mixed fixture renders seven source-ordered entries"), MixedRows.Num(), 7);
	if (MixedRows.Num() == 7)
	{
		for (int32 Index = 0; Index < 5; ++Index)
			TestTrue(TEXT("Mixed currency source order"), MixedRows[Index].Kind == ERokMailRewardKind::Currency && MixedRows[Index].Id == 100 + Index);
		TestTrue(TEXT("Mixed soldier source metadata"), MixedRows[5].Kind == ERokMailRewardKind::Soldier
			&& MixedRows[5].Id == 10304 && MixedRows[5].Quantity == 7 && MixedRows[5].DisplayName == TEXT("弩兵"));
		TestTrue(TEXT("Mixed final item"), MixedRows[6].Kind == ERokMailRewardKind::Item
			&& MixedRows[6].Id == 502090001 && MixedRows[6].Quantity == 3);
	}
	TestEqual(TEXT("Alternate fixture is empty"), RokMailRewardDetails::Normalize(Empty, Economy, Gameplay).Num(), 0);

	FRokMailRewardPayload Payload;
	Payload.Food = 1234567890123LL;
	Payload.Wood = 0;
	Payload.Stone = -7;
	Payload.Gold = 2;
	Payload.Denar = 3;
	Payload.Soldiers = {{KnownTroop.Id, 4000}, {999999, 0}, {KnownTroop.Id, 2}, {-1, -123},
		{9000000000LL, 1}, {KnownTroop.Id, -9223372036854775807LL - 1}};
	Payload.Items = {{KnownItem->Id, 1234}, {KnownItem->Id, 2}, {9000000000LL, 3},
		{0, 0}, {-1, -1234}, {KnownItem->Id, 9223372036854775807LL}};
	const auto Rows = RokMailRewardDetails::Normalize(Payload, Economy, Gameplay);
	if (!TestEqual(TEXT("Only currency fields filter nonpositive; every list entry remains"), Rows.Num(), 15)) return false;
	TestTrue(TEXT("Currency order food first"), Rows[0].Kind == ERokMailRewardKind::Currency && Rows[0].Id == 100);
	TestEqual(TEXT("Large int64 N0 grouping"), Rows[0].QuantityText, FString(TEXT("1,234,567,890,123")));
	TestEqual(TEXT("Food icon comes from Currency.csv"), Rows[0].Icon, FString(TEXT("ui_common[img_com_cur1]")));
	TestTrue(TEXT("Skipped wood and stone"), Rows[1].Id == 103 && Rows[2].Id == 104);
	TestEqual(TEXT("Denar localized from Currency.csv language"), Rows[2].DisplayName, FString(TEXT("钻石")));
	TestTrue(TEXT("Soldiers precede items"), Rows[3].Kind == ERokMailRewardKind::Soldier && Rows[9].Kind == ERokMailRewardKind::Item);
	TestTrue(TEXT("Known troop metadata resolves"), Rows[3].bKnownMetadata && Rows[3].Id == KnownTroop.Id && Rows[3].Icon == KnownTroop.Icon
		&& Rows[3].TroopLevel == KnownTroop.Level && Rows[3].TroopArmsType == KnownTroop.ArmsType);
	TestEqual(TEXT("Known troop uses existing Arms localized name"), Rows[3].DisplayName, KnownTroopDetail.Name);
	TestEqual(TEXT("Soldier quantity grouped"), Rows[3].QuantityText, FString(TEXT("4,000")));
	TestTrue(TEXT("Zero soldier remains in source list position"), Rows[4].Id == 999999 && Rows[4].Quantity == 0 && Rows[4].QuantityText == TEXT("0"));
	TestTrue(TEXT("Duplicate soldier ID keeps second row"), Rows[5].Id == KnownTroop.Id && Rows[5].Quantity == 2 && Rows[5].DisplayName == KnownTroopDetail.Name);
	TestTrue(TEXT("Negative soldier remains without comma after sign"), Rows[6].Id == -1 && Rows[6].QuantityText == TEXT("-123"));
	TestTrue(TEXT("Unknown wide troop ID stays readable"), !Rows[7].bKnownMetadata && Rows[7].Id == 9000000000LL && Rows[7].DisplayName.Contains(TEXT("9000000000")));
	TestEqual(TEXT("INT64_MIN soldier N0 grouping"), Rows[8].QuantityText, FString(TEXT("-9,223,372,036,854,775,808")));
	TestTrue(TEXT("Known item metadata resolves"), Rows[9].bKnownMetadata && Rows[9].Id == KnownItem->Id
		&& Rows[9].DisplayName == KnownItem->DisplayNameZhCn && Rows[9].Icon == KnownItem->Icon && Rows[9].Quality == KnownItem->Quality);
	TestEqual(TEXT("Item quantity grouped"), Rows[9].QuantityText, FString(TEXT("1,234")));
	TestTrue(TEXT("Duplicate item ID keeps second row"), Rows[10].Id == KnownItem->Id && Rows[10].Quantity == 2);
	TestTrue(TEXT("Unknown wide item ID stays readable"), !Rows[11].bKnownMetadata && Rows[11].Id == 9000000000LL && Rows[11].DisplayName.Contains(TEXT("9000000000")));
	TestTrue(TEXT("Zero item remains in source list position"), Rows[12].Id == 0 && Rows[12].Quantity == 0 && Rows[12].QuantityText == TEXT("0"));
	TestTrue(TEXT("Negative item remains with signed N0 grouping"), Rows[13].Id == -1 && Rows[13].QuantityText == TEXT("-1,234"));
	TestEqual(TEXT("INT64_MAX item N0 grouping"), Rows[14].QuantityText, FString(TEXT("9,223,372,036,854,775,807")));
	TestEqual(TEXT("Input soldier payload remains unchanged"), Payload.Soldiers.Num(), 6);
	TestEqual(TEXT("Input item payload remains unchanged"), Payload.Items.Num(), 6);
	TestEqual(TEXT("Null catalogs still retain every list entry"), RokMailRewardDetails::Normalize(Payload, nullptr, nullptr).Num(), 15);
	TestEqual(TEXT("Empty payload emits no invented rewards"), RokMailRewardDetails::Normalize(FRokMailRewardPayload(), Economy, Gameplay).Num(), 0);
	return true;
}
#endif
