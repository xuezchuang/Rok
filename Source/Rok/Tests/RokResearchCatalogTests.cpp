#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokResearchCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResearchCatalogTest,
	"Rok.Data.Research.SourceCatalog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokResearchCatalogTest::RunTest(const FString&)
{
	FRokResearchCatalog Catalog;
	FString Error;
	if (!TestTrue(TEXT("Study/StudyData source catalog loads"), Catalog.Load(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("All Study level rows"), Catalog.NumRows(), 410);
	TestEqual(TEXT("Civilization-specific StudyData rows"), Catalog.NumCivilizationOverrides(), 8);
	TArray<FRokResearchRow> Economy, Military;
	Catalog.GetRowsByTypeAndLevel(1, 1, Economy);
	Catalog.GetRowsByTypeAndLevel(2, 1, Military);
	TestEqual(TEXT("Economy level-one nodes"), Economy.Num(), 28);
	TestEqual(TEXT("Military level-one nodes"), Military.Num(), 43);
	FRokResearchRow First;
	if (!TestTrue(TEXT("Source row 110101 exists"), Catalog.GetByRowId(110101, First))) return false;
	TestEqual(TEXT("Study identity"), First.StudyType, 101);
	TestEqual(TEXT("Study level"), First.Level, 1);
	TestEqual(TEXT("Study one-level cap"), First.MaxLevel, 1);
	TestEqual(TEXT("First row base food"), First.BaseFood, int64(100));
	TestEqual(TEXT("First row base wood"), First.BaseWood, int64(100));
	TestEqual(TEXT("First row base seconds"), First.BaseSeconds, int64(60));
	TestEqual(TEXT("First row source icon"), First.IconSource, FString(TEXT("research_icon1[research_icon1_00]")));
	FRokResearchRow Prerequisite;
	if (!TestTrue(TEXT("Study type and level lookup"), Catalog.GetByStudyLevel(102, 1, Prerequisite))) return false;
	TestEqual(TEXT("Second source row ID"), Prerequisite.Id, 110201);
	TestEqual(TEXT("Five configured levels"), Prerequisite.MaxLevel, 5);
	TestEqual(TEXT("Four source prerequisite slots"), Prerequisite.Preconditions.Num(), 4);
	if (Prerequisite.Preconditions.Num() == 4)
	{
		TestEqual(TEXT("First prerequisite study type"), Prerequisite.Preconditions[0].StudyType, 101);
		TestEqual(TEXT("First prerequisite level"), Prerequisite.Preconditions[0].Level, 1);
		TestEqual(TEXT("Unused slot stays explicit"), Prerequisite.Preconditions[1].StudyType, 0);
	}
	FRokResearchRow Branch;
	if (!TestTrue(TEXT("Two-source branch row exists"), Catalog.GetByRowId(110501, Branch))) return false;
	TestEqual(TEXT("Branch first requirement"), Branch.Preconditions[0].StudyType, 102);
	TestEqual(TEXT("Branch second requirement"), Branch.Preconditions[1].StudyType, 103);
	FRokResearchRow LastLevel;
	if (!TestTrue(TEXT("Study level five exists"), Catalog.GetByStudyLevel(102, 5, LastLevel))) return false;
	TestEqual(TEXT("Level five source row"), LastLevel.Id, 110205);
	TestEqual(TEXT("Level five base seconds"), LastLevel.BaseSeconds, int64(10000));
	FRokResearchCivilizationOverride Override;
	TestTrue(TEXT("Civilization override uses Study row ID"), Catalog.GetCivilizationOverride(101, 220601, Override));
	TestEqual(TEXT("Override source ID"), Override.Id, 101);
	TestFalse(TEXT("Study type is not the override key"), Catalog.GetCivilizationOverride(101, 206, Override));
	TestFalse(TEXT("Unknown player level is not synthesized"), Catalog.GetByStudyLevel(101, 0, First));
	return true;
}
#endif
