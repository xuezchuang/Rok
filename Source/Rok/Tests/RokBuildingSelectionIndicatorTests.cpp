#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../RokBuildingSelectionIndicator.h"
#include "../RokCityGridMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokBuildingSelectionIndicatorUnityAlignmentTest,
	"Rok.City.SelectionIndicator.UnityAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokBuildingSelectionIndicatorUnityAlignmentTest::RunTest(const FString& Parameters)
{
	const FIntPoint Footprint(6, 4);
	const FVector Up = ARokBuildingSelectionIndicator::CalculateArrowOffset(
		Footprint,
		ERokBuildingSelectionArrowDirection::Up);
	const FVector Right = ARokBuildingSelectionIndicator::CalculateArrowOffset(
		Footprint,
		ERokBuildingSelectionArrowDirection::Right);
	const FVector Down = ARokBuildingSelectionIndicator::CalculateArrowOffset(
		Footprint,
		ERokBuildingSelectionArrowDirection::Down);
	const FVector Left = ARokBuildingSelectionIndicator::CalculateArrowOffset(
		Footprint,
		ERokBuildingSelectionArrowDirection::Left);

	TestTrue(TEXT("Unity +Z arrow lies on the positive UE city-grid diagonal"), Up.X > 0.0f && Up.Y > 0.0f);
	TestTrue(TEXT("Unity +X arrow lies on the second UE city-grid diagonal"), Right.X < 0.0f && Right.Y > 0.0f);
	TestTrue(TEXT("Up/down offsets are symmetric"), Down.Equals(-Up, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Right/left offsets are symmetric"), Left.Equals(-Right, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("All arrow offsets stay on the city ground"),
		FMath::IsNearlyZero(Up.Z) && FMath::IsNearlyZero(Right.Z));
	TestTrue(TEXT("Width uses Unity (size.x + 1) * 0.05 at 500 UE units"),
		FMath::IsNearlyEqual(Up.Size2D(), 175.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Length uses Unity (size.y + 1) * 0.05 at 500 UE units"),
		FMath::IsNearlyEqual(Right.Size2D(), 125.0f, KINDA_SMALL_NUMBER));

	const FVector SmallerUp = ARokBuildingSelectionIndicator::CalculateArrowOffset(
		FIntPoint(3, 3),
		ERokBuildingSelectionArrowDirection::Up);
	TestTrue(TEXT("Different Unity footprints produce different arrow spacing"), SmallerUp.Size2D() < Up.Size2D());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCitySelectionFootprintUnityAlignmentTest,
	"Rok.City.SelectionFootprint.UnityAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCitySelectionFootprintUnityAlignmentTest::RunTest(const FString& Parameters)
{
	const FVector Center(120.0f, -80.0f, 5.0f);
	const FIntPoint Footprint(6, 4);
	TStaticArray<FVector, 4> Corners;
	TestTrue(TEXT("A valid Unity footprint produces four corners"),
		FRokCityGridMath::TryGetFootprintCorners(Center, Footprint, Corners));

	const FVector WidthAxis = FRokCityGridMath::GetWidthAxis();
	const FVector LengthAxis = FRokCityGridMath::GetLengthAxis();
	for (int32 CornerIndex = 0; CornerIndex < Corners.Num(); ++CornerIndex)
	{
		const FVector Delta = Corners[CornerIndex] - Center;
		TestTrue(TEXT("6-cell width has exact 150 UE half extent"),
			FMath::IsNearlyEqual(FMath::Abs(FVector::DotProduct(Delta, WidthAxis)), 150.0f, 0.01f));
		TestTrue(TEXT("4-cell length has exact 100 UE half extent"),
			FMath::IsNearlyEqual(FMath::Abs(FVector::DotProduct(Delta, LengthAxis)), 100.0f, 0.01f));
		TestTrue(TEXT("Footprint corners stay on the Unity root plane"),
			FMath::IsNearlyEqual(Corners[CornerIndex].Z, Center.Z));
	}

	TestTrue(TEXT("Footprint center is selectable"),
		FRokCityGridMath::IsPointInsideFootprint(Center, Center, Footprint));
	TestTrue(TEXT("Last occupied cell center is selectable"),
		FRokCityGridMath::IsPointInsideFootprint(
			Center + WidthAxis * 125.0f + LengthAxis * 75.0f,
			Center,
			Footprint));
	TestFalse(TEXT("Point beyond the full Unity width is not selectable"),
		FRokCityGridMath::IsPointInsideFootprint(
			Center + WidthAxis * 151.0f,
			Center,
			Footprint));

	TStaticArray<FVector, 4> InvalidCorners;
	TestFalse(TEXT("Zero-size footprints are rejected"),
		FRokCityGridMath::TryGetFootprintCorners(Center, FIntPoint::ZeroValue, InvalidCorners));
	return true;
}

#endif
