#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../RokWorldMapEntityActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapUnitScaleSourceTest,
	"Rok.WorldMap.EntityPopulation.SourceScaleCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapUnitScaleSourceTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	URokWorldMapEntitySubsystem* Population = NewObject<URokWorldMapEntitySubsystem>(GameInstance);
	if (!TestTrue(TEXT("Generated population loads"), Population->LoadPopulation()))
	{
		return false;
	}
	int32 Formations = 0;
	for (const FRokWorldMapEntityRecord& Entity : Population->GetEntities())
	{
		if (Entity.Kind != TEXT("barbarian"))
		{
			continue;
		}
		++Formations;
		TestEqual(TEXT("Source formation has 15 base units"), Entity.FormationUnits.Num(), 15);
		const FRichCurve& Curve = Entity.UnitScaleCurveWithTangents;
		TestEqual(TEXT("All source keys loaded"), Curve.GetNumKeys(), 12);
		// GameApp.unity m_unit_scale_curve: Hermite slopes on 900..1500.
		TestTrue(TEXT("Tactical midpoint uses source tangents, not linear lerp"),
			FMath::IsNearlyEqual(Curve.Eval(1200.0f), 1.8972963f, 0.00001f));
		TestTrue(TEXT("1500 source key"), FMath::IsNearlyEqual(Curve.Eval(1500.0f), 2.1f));
		TestTrue(TEXT("Unit scale stays full until the LOD switch"),
			FMath::IsNearlyEqual(Curve.Eval(2999.995f), 2.5f));
		TestEqual(TEXT("At 3000 source units disappear for icons"), Curve.Eval(3000.0f), 0.0f);
		TestEqual(TEXT("Clamp below first key"), Curve.Eval(0.0f), 0.19f);
		TestEqual(TEXT("Clamp beyond last key"), Curve.Eval(3500.0f), 0.0f);
	}
	TestTrue(TEXT("At least one real formation verified"), Formations > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapEntityLodBandsTest,
	"Rok.WorldMap.EntityPopulation.LodBands",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapEntityLodBandsTest::RunTest(
	const FString& Parameters)
{
	constexpr float CloseMax = 1500.0f;
	constexpr float TacticalMax = 3000.0f;
	constexpr float FarMax = 5400.0f;
	TestTrue(TEXT("Transit close view expands soldier rows"),
		ARokWorldMapEntityActor::ShouldExpandFormation(899.0f));
	TestTrue(TEXT("900 dxf still includes expanded formation"),
		ARokWorldMapEntityActor::ShouldExpandFormation(900.0f));
	TestFalse(TEXT("Only above 900 dxf returns to base formation"),
		ARokWorldMapEntityActor::ShouldExpandFormation(900.01f));
	TestFalse(TEXT("Unity 1537 reference uses 15 units, not 43"),
		ARokWorldMapEntityActor::ShouldExpandFormation(1537.0f));
	TestFalse(TEXT("Overview icons never expand soldier rows"),
		ARokWorldMapEntityActor::ShouldExpandFormation(3500.0f));
	TestEqual(
		TEXT("Below 1500 dxf uses close detail"),
		ARokWorldMapEntityActor::ResolveLodBand(
			1499.0f,
			CloseMax,
			TacticalMax,
			FarMax),
		ERokWorldMapEntityLodBand::CloseDetail);
	TestEqual(
		TEXT("1500 dxf starts tactical detail"),
		ARokWorldMapEntityActor::ResolveLodBand(
			1500.0f,
			CloseMax,
			TacticalMax,
			FarMax),
		ERokWorldMapEntityLodBand::TacticalDetail);
	TestEqual(
		TEXT("Current reference dxf keeps Unity detail visuals"),
		ARokWorldMapEntityActor::ResolveLodBand(
			2479.0f,
			CloseMax,
			TacticalMax,
			FarMax),
		ERokWorldMapEntityLodBand::TacticalDetail);
	TestEqual(
		TEXT("3000 dxf remains Unity Tactical at inclusive upper bound"),
		ARokWorldMapEntityActor::ResolveLodBand(
			3000.0f,
			CloseMax,
			TacticalMax,
			FarMax),
		ERokWorldMapEntityLodBand::TacticalDetail);
	TestEqual(
		TEXT("Above 3000 dxf switches to Unity far icon"),
		ARokWorldMapEntityActor::ResolveLodBand(
			3000.01f,
			CloseMax,
			TacticalMax,
			FarMax),
		ERokWorldMapEntityLodBand::FarIcon);
	TestEqual(
		TEXT("5400 dxf keeps the far icon"),
		ARokWorldMapEntityActor::ResolveLodBand(
			5400.0f,
			CloseMax,
			TacticalMax,
			FarMax),
		ERokWorldMapEntityLodBand::FarIcon);
	TestEqual(
		TEXT("Beyond the prototype far curve is hidden"),
		ARokWorldMapEntityActor::ResolveLodBand(
			5400.1f,
			CloseMax,
			TacticalMax,
			FarMax),
		ERokWorldMapEntityLodBand::Hidden);
	return true;
}

#endif
