#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "../RokWorldMapEntitySubsystem.h"

namespace
{
TSharedPtr<FJsonObject> MapRow(int64 ObjectId, int32 ObjectType,
	const TCHAR* ConfigField, int32 ConfigId)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("objectId"), ObjectId);
	Row->SetNumberField(TEXT("objectType"), ObjectType);
	Row->SetNumberField(ConfigField, ConfigId);
	TSharedPtr<FJsonObject> Position = MakeShared<FJsonObject>();
	Position->SetNumberField(TEXT("x"), 687650);
	Position->SetNumberField(TEXT("y"), 94225);
	Row->SetObjectField(TEXT("objectPos"), Position);
	return Row;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerWorldEntityProjectionTest,
	"Rok.WorldMap.ServerEntity.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerWorldEntityProjectionTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	URokWorldMapEntitySubsystem* Population = NewObject<URokWorldMapEntitySubsystem>(Instance);
	if (!TestTrue(TEXT("Packaged visual population and config load"), Population->LoadPopulation()))
		return false;
	const int32 OfflineCount = Population->GetEntityCount();
	FRokWorldMapEntityRecord First, Second, Monster, Village, Wood, Stone, StoneFour, FoodTwo, Gold;
	auto FoodRow = MapRow(81101, 5, TEXT("resourceId"), 10003);
	FoodRow->SetNumberField(TEXT("resourceAmount"), 378000);
	FoodRow->SetNumberField(TEXT("collectRid"), 303);
	FoodRow->SetStringField(TEXT("name"), TEXT("Collector"));
	FoodRow->SetStringField(TEXT("guildAbbName"), TEXT("ABC"));
	FoodRow->SetStringField(TEXT("resourceGuildAbbName"), TEXT("DEF"));
	TestTrue(TEXT("Food resource projects"), Population->ResolveServerEntity(FoodRow, First));
	TestEqual(TEXT("Stable object identity"), First.ServerObjectId, int64(81101));
	TestEqual(TEXT("Config identity"), First.SourceId, 10003);
	TestEqual(TEXT("Config table"), First.SourceTable, FString(TEXT("ResourceGatherType")));
	TestEqual(TEXT("Object type"), First.ServerObjectType, 5);
	TestEqual(TEXT("Resource amount"), First.ResourceAmount, int64(378000));
	TestEqual(TEXT("Source resource capacity"), First.ResourceCapacity, int64(378000));
	TestEqual(TEXT("Server collector identity"), First.CollectorRoleId, int64(303));
	TestEqual(TEXT("Collector's alliance is part of their display name"), First.CollectorName, FString(TEXT("[ABC]Collector")));
	TestEqual(TEXT("Resource's owner alliance is independent"), First.ResourceAlliance, FString(TEXT("DEF")));
	TestEqual(TEXT("Food level"), First.Level, 3);
	TestEqual(TEXT("Food name from config"), First.DisplayName, FString(TEXT("等级3农田")));
	TestTrue(TEXT("Server X in Unity units"), FMath::IsNearlyEqual(First.UnityPosition.X, 6876.5));
	TestTrue(TEXT("Server Y in Unity units"), FMath::IsNearlyEqual(First.UnityPosition.Y, 942.25));
	TestTrue(TEXT("Food detail visual exists"), !First.DetailTexturePath.IsEmpty());
	TestTrue(TEXT("Food far visual exists"), !First.FarTexturePath.IsEmpty());

	TestTrue(TEXT("Second instance projects"), Population->ResolveServerEntity(
		MapRow(81102, 5, TEXT("resourceId"), 10003), Second));
	TestEqual(TEXT("Same source config"), Second.SourceId, First.SourceId);
	TestTrue(TEXT("Different server object identity"), Second.ServerObjectId != First.ServerObjectId);
	TestEqual(TEXT("Absent amount sentinel"), Second.ResourceAmount, int64(-1));
	TestTrue(TEXT("Wood projects"), Population->ResolveServerEntity(
		MapRow(81103, 6, TEXT("resourceId"), 20003), Wood));
	TestEqual(TEXT("Wood kind"), Wood.Kind, FString(TEXT("wood")));
	TestTrue(TEXT("Stone projects"), Population->ResolveServerEntity(
		MapRow(81104, 4, TEXT("resourceId"), 30003), Stone));
	TestEqual(TEXT("Stone kind"), Stone.Kind, FString(TEXT("stone")));
	TestTrue(TEXT("Level-four stone shares verified source visual"), Population->ResolveServerEntity(
		MapRow(81105, 4, TEXT("resourceId"), 30004), StoneFour));
	TestEqual(TEXT("Level-four stone name"), StoneFour.DisplayName, FString(TEXT("等级4石矿床")));
	TestEqual(TEXT("Level-four stone level"), StoneFour.Level, 4);
	TestEqual(TEXT("Level-four stone detail visual"), StoneFour.DetailTexturePath, Stone.DetailTexturePath);
	TestEqual(TEXT("Level-four stone far icon"), StoneFour.FarTexturePath, Stone.FarTexturePath);
	TestTrue(TEXT("Level-two food shares verified source visual"), Population->ResolveServerEntity(
		MapRow(81106, 5, TEXT("resourceId"), 10002), FoodTwo));
	TestEqual(TEXT("Level-two food level"), FoodTwo.Level, 2);
	TestTrue(TEXT("Gold uses source map icon"), Population->ResolveServerEntity(
		MapRow(81107, 7, TEXT("resourceId"), 40003), Gold));
	TestEqual(TEXT("Gold config ID"), Gold.SourceId, 40003);
	TestTrue(TEXT("Gold is explicitly icon-only"), Gold.bIconOnlyVisual);
	TestEqual(TEXT("Gold icon at both LODs"), Gold.DetailTexturePath, Gold.FarTexturePath);
	TestTrue(TEXT("Barbarian projects"), Population->ResolveServerEntity(
		MapRow(81201, 2, TEXT("monsterId"), 1014), Monster));
	TestEqual(TEXT("Monster config identity"), Monster.SourceId, 1014);
	TestEqual(TEXT("Monster source table"), Monster.SourceTable, FString(TEXT("Monster")));
	TestEqual(TEXT("Monster level"), Monster.Level, 14);
	TestEqual(TEXT("Formation from level 14 source"), Monster.FormationUnits.Num(), 15);
	for (const int32 MonsterId : {1007, 1018, 1019, 1020, 1021, 1022})
	{
		FRokWorldMapEntityRecord IconMonster;
		TestTrue(*FString::Printf(TEXT("Monster %d source formation projects"), MonsterId),
			Population->ResolveServerEntity(MapRow(82000 + MonsterId, 2, TEXT("monsterId"), MonsterId), IconMonster));
		TestFalse(TEXT("Source formation replaces map icon"), IconMonster.bIconOnlyVisual);
		TestFalse(TEXT("Exact source formation units loaded"), IconMonster.FormationUnits.IsEmpty());
		TestEqual(TEXT("Correct config ID"), IconMonster.SourceId, MonsterId);
	}
	for (const int32 CitadelId : {2001, 2003, 2004})
	{
		FRokWorldMapEntityRecord Citadel;
		TestTrue(*FString::Printf(TEXT("Citadel %d source icon projects"), CitadelId),
			Population->ResolveServerEntity(MapRow(84000 + CitadelId, 28, TEXT("monsterId"), CitadelId), Citadel));
		TestEqual(TEXT("Citadel kind"), Citadel.Kind, FString(TEXT("barbarian_citadel")));
		TestTrue(TEXT("Citadel icon-only"), Citadel.bIconOnlyVisual);
		TestEqual(TEXT("Citadel level"), Citadel.Level, CitadelId - 2000);
	}
	TestTrue(TEXT("Village projects"), Population->ResolveServerEntity(
		MapRow(81301, 10, TEXT("resourcePointId"), 639), Village));
	TestEqual(TEXT("Fixed-point config identity"), Village.SourceId, 639);
	TestEqual(TEXT("Fixed-point table"), Village.SourceTable, FString(TEXT("MapFixPoint")));
	TestEqual(TEXT("Village kind"), Village.Kind, FString(TEXT("village")));
	TestTrue(TEXT("Village uses source MapFixPoint X"), FMath::IsNearlyEqual(Village.UnityPosition.X, 6852.0));
	TestTrue(TEXT("Village uses source MapFixPoint Y"), FMath::IsNearlyEqual(Village.UnityPosition.Y, 948.0));
	FRokWorldMapEntityRecord GuildFood, GuildWood, GuildStone, GuildGold, Checkpoint, HolyLand1, HolyLand2;
	const TSharedPtr<FJsonObject> GuildFoodRow = MapRow(60911, 16, TEXT("resourceId"), 0);
	GuildFoodRow->RemoveField(TEXT("resourceId"));
	TestTrue(TEXT("Guild food source icon projects without config ID in packet"),
		Population->ResolveServerEntity(GuildFoodRow, GuildFood));
	TestEqual(TEXT("Guild food config type"), GuildFood.SourceId, 4);
	TestEqual(TEXT("Guild food table"), GuildFood.SourceTable, FString(TEXT("AllianceBuildingType")));
	TestEqual(TEXT("Guild food name"), GuildFood.DisplayName, FString(TEXT("联盟农田")));
	TestTrue(TEXT("Guild food icon-only"), GuildFood.bIconOnlyVisual);
	const TSharedPtr<FJsonObject> GuildRow = MapRow(60912, 17, TEXT("resourceId"), 0);
	GuildRow->RemoveField(TEXT("resourceId"));
	TestTrue(TEXT("Guild wood source icon projects without config ID in packet"),
		Population->ResolveServerEntity(GuildRow, GuildWood));
	TestEqual(TEXT("Guild wood config type"), GuildWood.SourceId, 5);
	TestEqual(TEXT("Guild wood table"), GuildWood.SourceTable, FString(TEXT("AllianceBuildingType")));
	TestEqual(TEXT("Guild wood name"), GuildWood.DisplayName, FString(TEXT("联盟伐木场")));
	TestTrue(TEXT("Guild wood icon-only"), GuildWood.bIconOnlyVisual);
	TestTrue(TEXT("Guild stone projects"), Population->ResolveServerEntity(
		MapRow(60913, 18, TEXT("resourceId"), 0), GuildStone));
	TestEqual(TEXT("Guild stone config type"), GuildStone.SourceId, 6);
	TestTrue(TEXT("Guild gold projects"), Population->ResolveServerEntity(
		MapRow(60914, 19, TEXT("resourceId"), 0), GuildGold));
	TestEqual(TEXT("Guild gold config type"), GuildGold.SourceId, 7);
	TestTrue(TEXT("Guild resource icons are distinct"),
		GuildFood.DetailTexturePath != GuildWood.DetailTexturePath
		&& GuildWood.DetailTexturePath != GuildStone.DetailTexturePath
		&& GuildStone.DetailTexturePath != GuildGold.DetailTexturePath);
	TestTrue(TEXT("Checkpoint resolves through StrongHoldData type"), Population->ResolveServerEntity(
		MapRow(62001, 25, TEXT("strongHoldId"), 90005), Checkpoint));
	TestEqual(TEXT("Checkpoint source ID"), Checkpoint.SourceId, 90005);
	TestEqual(TEXT("Checkpoint level"), Checkpoint.Level, 1);
	TestEqual(TEXT("Checkpoint orientation comes from StrongHoldData.posTo"), Checkpoint.SourceYawDegrees, 180.0f);
	TestEqual(TEXT("Checkpoint name"), Checkpoint.DisplayName, FString(TEXT("1级关卡")));
	TestEqual(TEXT("Checkpoint table"), Checkpoint.SourceTable, FString(TEXT("StrongHoldData")));
	TestTrue(TEXT("Holy land group one projects"), Population->ResolveServerEntity(
		MapRow(62002, 26, TEXT("strongHoldId"), 10011), HolyLand1));
	TestEqual(TEXT("Holy land one name"), HolyLand1.DisplayName, FString(TEXT("萨法加")));
	TestTrue(TEXT("Holy land group two projects"), Population->ResolveServerEntity(
		MapRow(62003, 26, TEXT("strongHoldId"), 20005), HolyLand2));
	TestEqual(TEXT("Holy land two name"), HolyLand2.DisplayName, FString(TEXT("埃德夫")));
	TestTrue(TEXT("Holy land groups have distinct source icons"),
		HolyLand1.DetailTexturePath != HolyLand2.DetailTexturePath);
	TestEqual(TEXT("Offline fixture count preserved"), Population->GetEntityCount(), OfflineCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerWorldEntityRejectTest,
	"Rok.WorldMap.ServerEntity.RejectUnsupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerWorldEntityRejectTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	URokWorldMapEntitySubsystem* Population = NewObject<URokWorldMapEntitySubsystem>(Instance);
	if (!Population->LoadPopulation()) return false;
	FRokWorldMapEntityRecord Record;
	TestFalse(TEXT("Zero object ID"), Population->ResolveServerEntity(
		MapRow(0, 5, TEXT("resourceId"), 10003), Record));
	TestFalse(TEXT("Resource type mismatch"), Population->ResolveServerEntity(
		MapRow(81, 4, TEXT("resourceId"), 10003), Record));
	TestFalse(TEXT("Unknown resource config"), Population->ResolveServerEntity(
		MapRow(82, 5, TEXT("resourceId"), 10007), Record));
	TestFalse(TEXT("Unknown gold config"), Population->ResolveServerEntity(
		MapRow(83, 7, TEXT("resourceId"), 40007), Record));
	TestFalse(TEXT("Unknown monster config"), Population->ResolveServerEntity(
		MapRow(84, 2, TEXT("monsterId"), 999999), Record));
	TestFalse(TEXT("Unrelated monster model"), Population->ResolveServerEntity(
		MapRow(85, 2, TEXT("monsterId"), 3001), Record));
	TestFalse(TEXT("Stronghold type mismatch"), Population->ResolveServerEntity(
		MapRow(90, 25, TEXT("strongHoldId"), 10011), Record));
	TestFalse(TEXT("Cave visual is unmigrated"), Population->ResolveServerEntity(
		MapRow(86, 11, TEXT("resourcePointId"), 1), Record));
	TestFalse(TEXT("Unknown type"), Population->ResolveServerEntity(
		MapRow(87, 99, TEXT("resourceId"), 10003), Record));
	const TSharedPtr<FJsonObject> WrongAmount = MapRow(89, 5, TEXT("resourceId"), 10003);
	WrongAmount->SetStringField(TEXT("resourceAmount"), TEXT("invalid"));
	TestFalse(TEXT("Malformed resource amount"), Population->ResolveServerEntity(WrongAmount, Record));
	const TSharedPtr<FJsonObject> MissingPos = MapRow(88, 5, TEXT("resourceId"), 10003);
	MissingPos->RemoveField(TEXT("objectPos"));
	TestFalse(TEXT("Missing position"), Population->ResolveServerEntity(MissingPos, Record));
	TestEqual(TEXT("Failure clears old identity"), Record.ServerObjectId, int64(0));
	return true;
}

#endif
