#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokCityPlacementPreviewMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityPlacementPreviewMathTest,
	"Rok.Gameplay.CityPlacementPreview.SourceSnapAndDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityPlacementPreviewMathTest::RunTest(const FString&)
{
	FString Error;
	FVector2D Center;
	FIntPoint Origin;
	FRokCityConstructionFootprint Footprint;
	Footprint.Origin = FIntPoint(-3, 4);
	Footprint.Size = FIntPoint(2, 3);
	TestTrue(TEXT("Non-square even/odd footprint maps to source center"),
		FRokCityPlacementPreviewMath::GridToLocalCenter(Footprint, Center, Error));
	TestTrue(TEXT("Source center X is -0.25"), FMath::IsNearlyEqual(Center.X, -0.25, 1e-9));
	TestTrue(TEXT("Source center Y is 0.5"), FMath::IsNearlyEqual(Center.Y, 0.5, 1e-9));
	TestTrue(TEXT("Source center snaps back to signed grid origin"),
		FRokCityPlacementPreviewMath::LocalCenterToGrid(Center, Footprint.Size, Origin, Error));
	TestEqual(TEXT("Signed grid X"), Origin.X, -3);
	TestEqual(TEXT("Signed grid Y"), Origin.Y, 4);
	Footprint.Origin = FIntPoint(-5, 5);
	Footprint.Size = FIntPoint(5, 5);
	TestTrue(TEXT("5x5 source footprint at signed grid"),
		FRokCityPlacementPreviewMath::GridToLocalCenter(Footprint, Center, Error));
	TestTrue(TEXT("5x5 center X uses four half-cell offsets"), FMath::IsNearlyEqual(Center.X, -0.3, 1e-9));
	TestTrue(TEXT("5x5 center Y uses four half-cell offsets"), FMath::IsNearlyEqual(Center.Y, 0.7, 1e-9));

	// CityGlobalMediator.ConvertCityObjLocalToTile uses floor((center-size*.05)*10+.55).
	TestTrue(TEXT("Positive just below source snap boundary"),
		FRokCityPlacementPreviewMath::LocalCenterToGrid(FVector2D(0.0949, 0), FIntPoint(1, 1), Origin, Error));
	TestEqual(TEXT("Still tile zero before +0.095"), Origin.X, 0);
	TestTrue(TEXT("Positive just above source snap boundary"),
		FRokCityPlacementPreviewMath::LocalCenterToGrid(FVector2D(0.0951, 0), FIntPoint(1, 1), Origin, Error));
	TestEqual(TEXT("Snaps to positive tile"), Origin.X, 1);
	TestTrue(TEXT("Negative just below source snap boundary"),
		FRokCityPlacementPreviewMath::LocalCenterToGrid(FVector2D(-0.0051, 0), FIntPoint(1, 1), Origin, Error));
	TestEqual(TEXT("Snaps to negative tile"), Origin.X, -1);
	TestTrue(TEXT("Negative just above source snap boundary"),
		FRokCityPlacementPreviewMath::LocalCenterToGrid(FVector2D(-0.0049, 0), FIntPoint(1, 1), Origin, Error));
	TestEqual(TEXT("Still tile zero before negative crossing"), Origin.X, 0);

	// The caller supplies its verified city-container transform. No global origin is assumed.
	const FTransform Container(FRotator(0, 45, 0), FVector(100, -50, 20), FVector(-1, 2, 1));
	const FVector ExpectedLocal(0.2, -0.3, 0);
	const FVector World = Container.TransformPosition(ExpectedLocal);
	FVector2D Recovered;
	TestTrue(TEXT("World hit converts through reflected and 45-degree city container"),
		FRokCityPlacementPreviewMath::WorldToCityLocal(Container, World, Recovered, Error));
	TestTrue(TEXT("Reflected local X recovers"), FMath::IsNearlyEqual(Recovered.X, ExpectedLocal.X, 1e-6));
	TestTrue(TEXT("Rotated local Y recovers"), FMath::IsNearlyEqual(Recovered.Y, ExpectedLocal.Y, 1e-6));

	FRokCityConstructionFootprint OneCell;
	OneCell.Origin = FIntPoint::ZeroValue;
	OneCell.Size = FIntPoint(1, 1);
	const auto WorldAt = [&Container](double X, double Y)
	{
		return Container.TransformPosition(FVector(X, Y, 0));
	};
	FRokCityPlacementDragState OneFrame;
	TestFalse(TEXT("UI-origin pointer cannot start drag"), FRokCityPlacementPreviewMath::BeginDrag(
		OneFrame, OneCell, Container, WorldAt(0, 0), true, Error));
	TestFalse(TEXT("Rejected UI begin leaves state inactive"), OneFrame.bActive);
	TestTrue(TEXT("Begin source drag"), FRokCityPlacementPreviewMath::BeginDrag(
		OneFrame, OneCell, Container, WorldAt(0, 0), false, Error));
	bool bChanged = false;
	TestTrue(TEXT("One-frame drag quantizes both signed axes"), FRokCityPlacementPreviewMath::MoveDrag(
		OneFrame, Container, WorldAt(0.12, -0.11), false, bChanged, Error));
	TestTrue(TEXT("One-frame grid changed"), bChanged);
	TestEqual(TEXT("One-frame positive X"), OneFrame.Footprint.Origin.X, 1);
	TestEqual(TEXT("One-frame negative Y"), OneFrame.Footprint.Origin.Y, -1);
	TestTrue(TEXT("Fractional X retained"), FMath::IsNearlyEqual(OneFrame.FractionalMove.X, 0.02, 1e-6));

	FRokCityPlacementDragState SplitFrames;
	TestTrue(TEXT("Begin split drag"), FRokCityPlacementPreviewMath::BeginDrag(
		SplitFrames, OneCell, Container, WorldAt(0, 0), false, Error));
	TestTrue(TEXT("First sub-cell pointer event"), FRokCityPlacementPreviewMath::MoveDrag(
		SplitFrames, Container, WorldAt(0.06, -0.04), false, bChanged, Error));
	TestFalse(TEXT("Sub-cell event does not snap or advance pointer"), bChanged);
	TestTrue(TEXT("Previous pointer unchanged before full step"), SplitFrames.PreviousPointerLocal.IsZero());
	TestTrue(TEXT("UI pointer event is ignored"), FRokCityPlacementPreviewMath::MoveDrag(
		SplitFrames, Container, WorldAt(0.6, 0.6), true, bChanged, Error));
	TestFalse(TEXT("UI event does not move grid"), bChanged);
	TestTrue(TEXT("UI event does not change pointer accumulator"), SplitFrames.PreviousPointerLocal.IsZero());
	TestTrue(TEXT("Second pointer event reaches same final hit"), FRokCityPlacementPreviewMath::MoveDrag(
		SplitFrames, Container, WorldAt(0.12, -0.11), false, bChanged, Error));
	TestEqual(TEXT("Frame splitting preserves snapped X"), SplitFrames.Footprint.Origin.X, OneFrame.Footprint.Origin.X);
	TestEqual(TEXT("Frame splitting preserves snapped Y"), SplitFrames.Footprint.Origin.Y, OneFrame.Footprint.Origin.Y);
	TestTrue(TEXT("Frame splitting preserves X carry"),
		FMath::IsNearlyEqual(SplitFrames.FractionalMove.X, OneFrame.FractionalMove.X, 1e-6));
	FRokCityPlacementPreviewMath::CancelDrag(SplitFrames);
	TestFalse(TEXT("Cancel clears active drag"), SplitFrames.bActive);
	TestTrue(TEXT("Cancel clears fractional accumulator"), SplitFrames.FractionalMove.IsZero());
	TestFalse(TEXT("Cancelled drag cannot move"), FRokCityPlacementPreviewMath::MoveDrag(
		SplitFrames, Container, WorldAt(0.3, 0), false, bChanged, Error));
	TestEqual(TEXT("Cancelled origin reset"), SplitFrames.Footprint.Origin, FIntPoint::ZeroValue);
	return true;
}
#endif
