#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "../RokMainHudWidget.h"
#include "../RokPlayerTopBar.h"
#include "../RokTrainResourceCost.h"

namespace
{
// Role_RoleInfo.roleInfo fields the top bar reads, with the values the Unity editor
// account's role had on 2026-11-04 (Saved/RokNet/LastSession.json).
TSharedPtr<FJsonObject> MakeRoleInfoFixture(int64 Level)
{
	TSharedPtr<FJsonObject> RoleInfo = MakeShared<FJsonObject>();
	RoleInfo->SetStringField(TEXT("name"), TEXT("snow"));
	RoleInfo->SetNumberField(TEXT("level"), Level);
	RoleInfo->SetNumberField(TEXT("combatPower"), 7357);
	RoleInfo->SetNumberField(TEXT("food"), 128788);
	RoleInfo->SetNumberField(TEXT("wood"), 112700);
	RoleInfo->SetNumberField(TEXT("stone"), 0);
	RoleInfo->SetNumberField(TEXT("gold"), 0);
	RoleInfo->SetNumberField(TEXT("denar"), 350);
	RoleInfo->SetNumberField(TEXT("vip"), 40);
	RoleInfo->SetBoolField(TEXT("vipFreeBox"), false);
	RoleInfo->SetBoolField(TEXT("vipExpFlag"), false);
	RoleInfo->SetNumberField(TEXT("headId"), 1000);
	RoleInfo->SetNumberField(TEXT("headFrameID"), 2000);
	TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
	Pos->SetNumberField(TEXT("x"), 687939);
	Pos->SetNumberField(TEXT("y"), 94263);
	RoleInfo->SetObjectField(TEXT("pos"), Pos);
	return RoleInfo;
}

FString TextOf(URokMainHudWidget* Hud, const TCHAR* Name)
{
	const UTextBlock* Text = Cast<UTextBlock>(Hud->GetWidgetFromName(Name));
	return Text ? Text->GetText().ToString() : FString(TEXT("<missing>"));
}
}

// Unity sources: ClientUtils.CurrencyFormat/FormatComma, PlayerProxy.GetCurVipInfo,
// MainInterfaceMediator.OnAgeChange/OnWorldViewChange, UI_Item_PlayerResources_SubView.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokPlayerTopBarRulesTest,
	"Rok.Net.TopBar.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokPlayerTopBarRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Food truncates, never rounds"), RokTrainResourceCost::Format(128788), FString(TEXT("128.7K")));
	TestEqual(TEXT("Wood"), RokTrainResourceCost::Format(112700), FString(TEXT("112.7K")));
	TestEqual(TEXT("Gems below 1000 stay whole"), RokTrainResourceCost::Format(350), FString(TEXT("350")));
	TestEqual(TEXT("Units start at 1000, not 10000"), RokTrainResourceCost::Format(5600), FString(TEXT("5.6K")));

	TestEqual(TEXT("Power uses N0 grouping"), RokPlayerTopBar::FormatComma(7357), FString(TEXT("7,357")));
	TestEqual(TEXT("N0 millions"), RokPlayerTopBar::FormatComma(1234567), FString(TEXT("1,234,567")));
	TestEqual(TEXT("N0 zero"), RokPlayerTopBar::FormatComma(0), FString(TEXT("0")));
	TestEqual(TEXT("N0 negative"), RokPlayerTopBar::FormatComma(-1234), FString(TEXT("-1,234")));

	// Vip.csv: Vip0 point 200 (l_nameID 800001), Vip1 point 400 (800002).
	TestEqual(TEXT("40 points is Vip0"), RokPlayerTopBar::FindVipNameId(40), 800001);
	TestEqual(TEXT("199 points is still Vip0"), RokPlayerTopBar::FindVipNameId(199), 800001);
	TestEqual(TEXT("200 points reaches Vip1"), RokPlayerTopBar::FindVipNameId(200), 800002);
	TestTrue(TEXT("Past the last threshold keeps the last row"), RokPlayerTopBar::FindVipNameId(MAX_int64) > 800002);

	// CityAgeSize.csv: townLevel 1 -> 300040 (Stone Age), 4 -> 300029 (Classical Age).
	TestEqual(TEXT("Level 1 Stone Age"), RokPlayerTopBar::FindAgeNameId(1), 300040);
	TestEqual(TEXT("Level 3 still Stone Age"), RokPlayerTopBar::FindAgeNameId(3), 300040);
	TestEqual(TEXT("Level 4 Classical Age"), RokPlayerTopBar::FindAgeNameId(4), 300029);
	TestEqual(TEXT("Level 0 has no age"), RokPlayerTopBar::FindAgeNameId(0), 0);

	TestEqual(TEXT("Coordinate box"), RokPlayerTopBar::FormatCoordinate(1146, 157), FString(TEXT("#1    X:1146 Y:157")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokPlayerTopBarRoleInfoTest,
	"Rok.Net.TopBar.RoleInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokPlayerTopBarRoleInfoTest::RunTest(const FString& Parameters)
{
	FRokPlayerTopBar TopBar;
	FString Error;
	if (!TestTrue(TEXT("Role info converts"), RokPlayerTopBar::FromRoleInfo(MakeRoleInfoFixture(2), TopBar, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Name"), TopBar.Name, FString(TEXT("snow")));
	TestEqual(TEXT("Power"), TopBar.Power, int64(7357));
	TestEqual(TEXT("Food"), TopBar.Food, int64(128788));
	TestEqual(TEXT("Gems are denar"), TopBar.Denar, int64(350));
	TestEqual(TEXT("PosHelper X: floor(687939 / 100) / 6"), TopBar.PosX, 1146);
	TestEqual(TEXT("PosHelper Y: floor(94263 / 100) / 6"), TopBar.PosY, 157);
	TestEqual(TEXT("VIP label"), TopBar.VipText, FString(TEXT("VIP0")));
	TestTrue(TEXT("Unclaimed VIP box shows the red dot"), TopBar.bVipRedPoint);
	TestEqual(TEXT("Age label"), TopBar.AgeText, FString(TEXT("远古时代")));
	TestEqual(TEXT("Head"), TopBar.HeadId, 1000);
	TestEqual(TEXT("Frame"), TopBar.HeadFrameId, 2000);
	TestTrue(TEXT("Level 2 shows food"), TopBar.bShowFood);
	TestTrue(TEXT("Level 2 shows wood"), TopBar.bShowWood);
	TestFalse(TEXT("Level 2 hides stone (stoneShowLev 4)"), TopBar.bShowStone);
	TestFalse(TEXT("Level 2 hides gold (goldShowLev 10)"), TopBar.bShowGold);
	TestTrue(TEXT("denarShowStep 0 shows gems"), TopBar.bShowGem);

	FRokPlayerTopBar Level4;
	TestTrue(TEXT("Level 4 converts"), RokPlayerTopBar::FromRoleInfo(MakeRoleInfoFixture(4), Level4, Error));
	TestTrue(TEXT("Level 4 shows stone"), Level4.bShowStone);
	TestEqual(TEXT("Level 4 is the Classical Age"), Level4.AgeText, FString(TEXT("古典时代")));

	TSharedPtr<FJsonObject> Claimed = MakeRoleInfoFixture(2);
	Claimed->SetBoolField(TEXT("vipFreeBox"), true);
	Claimed->SetBoolField(TEXT("vipExpFlag"), true);
	FRokPlayerTopBar ClaimedTopBar;
	TestTrue(TEXT("Claimed role info converts"), RokPlayerTopBar::FromRoleInfo(Claimed, ClaimedTopBar, Error));
	TestFalse(TEXT("Both VIP dailies claimed hides the red dot"), ClaimedTopBar.bVipRedPoint);

	FRokPlayerTopBar Missing;
	TestFalse(TEXT("A role info without level is rejected"),
		RokPlayerTopBar::FromRoleInfo(MakeShared<FJsonObject>(), Missing, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokPlayerTopBarHudTest,
	"Rok.Net.TopBar.Hud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokPlayerTopBarHudTest::RunTest(const FString& Parameters)
{
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("HUD"), Hud) || !TestTrue(TEXT("HUD initializes"), Hud->Initialize()))
	{
		return false;
	}
	Hud->TakeWidget();

	FRokPlayerTopBar TopBar;
	FString Error;
	if (!TestTrue(TEXT("Role info converts"), RokPlayerTopBar::FromRoleInfo(MakeRoleInfoFixture(2), TopBar, Error)))
	{
		return false;
	}
	TopBar.HeadId = 1001;
	Hud->ApplyServerTopBar(TopBar);

	TestEqual(TEXT("Food cell"), TextOf(Hud, TEXT("FoodResourceValue")), FString(TEXT("128.7K")));
	TestEqual(TEXT("Wood cell"), TextOf(Hud, TEXT("WoodResourceValue")), FString(TEXT("112.7K")));
	TestEqual(TEXT("Gem cell"), TextOf(Hud, TEXT("GemResourceValue")), FString(TEXT("350")));
	TestEqual(TEXT("VIP"), TextOf(Hud, TEXT("VipText")), FString(TEXT("VIP0")));
	TestEqual(TEXT("Coordinates"), TextOf(Hud, TEXT("CoordinateText")), FString(TEXT("#1    X:1146 Y:157")));
	TestEqual(TEXT("Age"), TextOf(Hud, TEXT("AgeText")), FString(TEXT("远古时代")));
	TestEqual(TEXT("City name"), TextOf(Hud, TEXT("CityNameText")), FString(TEXT("snow")));
	TestEqual(TEXT("City level is the town hall level"), TextOf(Hud, TEXT("CityLevelText")), FString(TEXT("2")));
	const UWidget* VipRedPoint = Hud->GetWidgetFromName(TEXT("VipRedPoint"));
	TestTrue(TEXT("VIP red dot shows"), VipRedPoint && VipRedPoint->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Head follows headId"), Hud->GetShownHeadIdForTesting(), 1001);
	TestEqual(TEXT("Frame stays 2000"), Hud->GetShownHeadFrameIdForTesting(), 2000);
	return true;
}

#endif
