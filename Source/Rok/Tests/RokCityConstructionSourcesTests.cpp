#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokCityConstructionSources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityConstructionSourcesTest,
	"Rok.Data.CityConstruction.ExactModelSources", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityConstructionSourcesTest::RunTest(const FString&)
{
	FRokCityConstructionSources Sources;
	FString Error;
	if (!TestTrue(TEXT("Source model index loads"), Sources.Load(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("All BuildingModelData identities"), Sources.NumModels(), 870);
	FRokCityConstructionModel AcademyAgeOne, AcademyAgeTwo;
	if (!TestTrue(TEXT("Academy civilization 101 age 1"), Sources.GetModel(13, 101, 1, AcademyAgeOne))
		|| !TestTrue(TEXT("Academy civilization 101 age 2"), Sources.GetModel(13, 101, 2, AcademyAgeTwo))) return false;
	TestEqual(TEXT("Age one exact source row"), AcademyAgeOne.Id, 131011);
	TestEqual(TEXT("Age two exact source row"), AcademyAgeTwo.Id, 131012);
	TestEqual(TEXT("Both ages share source model"), AcademyAgeOne.ModelId, AcademyAgeTwo.ModelId);
	TestEqual(TEXT("Academy source model"), AcademyAgeTwo.ModelId, FString(TEXT("build_13_101_2")));
	TestTrue(TEXT("Academy source prefabs exist"), AcademyAgeTwo.HasVisualSource());
	FRokCityConstructionModel MissingVisual;
	if (!TestTrue(TEXT("Configuration lookup does not hide missing source art"), Sources.GetModel(4, 301, 1, MissingVisual))) return false;
	TestFalse(TEXT("Missing scene/UI prefabs remain explicit"), MissingVisual.HasVisualSource());
	TestFalse(TEXT("No invented model for configured type 54"), Sources.GetModel(54, 101, 1, MissingVisual));
	TestFalse(TEXT("No invented model for configured type 55"), Sources.GetModel(55, 101, 1, MissingVisual));
	TestFalse(TEXT("No civilization fallback"), Sources.GetModel(13, 999, 2, MissingVisual));
	TestFalse(TEXT("No age fallback"), Sources.GetModel(13, 101, 0, MissingVisual));
	FRokCityConstructionModel ModelOnly;
	TestTrue(TEXT("Type 99 remains a model source row"), Sources.GetModel(99, 201, 5, ModelOnly));
	TestFalse(TEXT("Model-only type 99 is not a constructible type"), ModelOnly.bHasBuildingTypeConfig);
	return true;
}
#endif
