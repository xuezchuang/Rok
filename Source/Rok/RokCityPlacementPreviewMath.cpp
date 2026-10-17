#include "RokCityPlacementPreviewMath.h"

namespace
{
constexpr double GridSize = 0.1;
constexpr double HalfGrid = 0.05;
constexpr double GridReciprocal = 10.0;

bool IsFinite(const FVector2D& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
}

double QuantizeMove(double Delta)
{
	// CityGlobalMediator.OnTouchMoved: floor(abs(delta) * 10) * 0.1 * sign(delta).
	return FMath::FloorToDouble(FMath::Abs(Delta) * GridReciprocal) * GridSize * (Delta < 0.0 ? -1.0 : 1.0);
}

bool SnapAxis(double Center, int32 Size, int32& OutCell)
{
	// CityGlobalMediator.ConvertCityObjLocalToTile; retain the source's asymmetric +0.55.
	const double Cell = FMath::FloorToDouble((Center - double(Size) * HalfGrid) * GridReciprocal + 0.55);
	if (!FMath::IsFinite(Cell) || Cell < double(TNumericLimits<int32>::Min())
		|| Cell > double(TNumericLimits<int32>::Max()))
	{
		return false;
	}
	OutCell = static_cast<int32>(Cell);
	return true;
}
}

bool FRokCityPlacementPreviewMath::GridToLocalCenter(const FRokCityConstructionFootprint& Footprint,
	FVector2D& OutCenter, FString& OutError)
{
	OutError.Reset();
	if (!Footprint.IsValid())
	{
		OutError = TEXT("Placement footprint must have positive width and length");
		return false;
	}
	// Unity's horizontal axes are local X/Z. UE preview exposes the same plane as local X/Y.
	const FVector2D Center(double(Footprint.Origin.X) * GridSize + double(Footprint.Size.X - 1) * HalfGrid,
		double(Footprint.Origin.Y) * GridSize + double(Footprint.Size.Y - 1) * HalfGrid);
	if (!IsFinite(Center))
	{
		OutError = TEXT("Placement center is not finite");
		return false;
	}
	OutCenter = Center;
	return true;
}

bool FRokCityPlacementPreviewMath::LocalCenterToGrid(const FVector2D& LocalCenter,
	const FIntPoint& Size, FIntPoint& OutOrigin, FString& OutError)
{
	OutError.Reset();
	if (Size.X <= 0 || Size.Y <= 0 || !IsFinite(LocalCenter))
	{
		OutError = TEXT("Placement size or local center is invalid");
		return false;
	}
	FIntPoint Candidate;
	if (!SnapAxis(LocalCenter.X, Size.X, Candidate.X)
		|| !SnapAxis(LocalCenter.Y, Size.Y, Candidate.Y))
	{
		OutError = TEXT("Placement grid coordinate exceeds int32 range");
		return false;
	}
	OutOrigin = Candidate;
	return true;
}

bool FRokCityPlacementPreviewMath::WorldToCityLocal(const FTransform& CityContainer,
	const FVector& WorldHit, FVector2D& OutLocal, FString& OutError)
{
	OutError.Reset();
	const FVector Scale = CityContainer.GetScale3D();
	if (!CityContainer.IsValid() || WorldHit.ContainsNaN()
		|| FMath::IsNearlyZero(Scale.X) || FMath::IsNearlyZero(Scale.Y) || FMath::IsNearlyZero(Scale.Z))
	{
		OutError = TEXT("City container transform or world hit is invalid");
		return false;
	}
	// CityGlobalMediator.MakeLocalPos uses m_cityBuildingContainer.InverseTransformPoint.
	const FVector Local = CityContainer.InverseTransformPosition(WorldHit);
	const FVector2D Candidate(Local.X, Local.Y);
	if (!IsFinite(Candidate))
	{
		OutError = TEXT("City-local pointer is not finite");
		return false;
	}
	OutLocal = Candidate;
	return true;
}

bool FRokCityPlacementPreviewMath::BeginDrag(FRokCityPlacementDragState& InOutState,
	const FRokCityConstructionFootprint& Footprint, const FTransform& CityContainer,
	const FVector& WorldHit, bool bPointerFromUI, FString& OutError)
{
	OutError.Reset();
	if (bPointerFromUI)
	{
		OutError = TEXT("UI-owned pointer cannot begin a city placement drag");
		return false;
	}
	FRokCityPlacementDragState Candidate;
	if (!GridToLocalCenter(Footprint, Candidate.LocalCenter, OutError)
		|| !WorldToCityLocal(CityContainer, WorldHit, Candidate.PreviousPointerLocal, OutError))
	{
		return false;
	}
	Candidate.Footprint = Footprint;
	Candidate.bActive = true;
	InOutState = Candidate;
	return true;
}

bool FRokCityPlacementPreviewMath::MoveDrag(FRokCityPlacementDragState& InOutState,
	const FTransform& CityContainer, const FVector& WorldHit, bool bPointerFromUI,
	bool& bOutGridChanged, FString& OutError)
{
	bOutGridChanged = false;
	OutError.Reset();
	if (!InOutState.bActive)
	{
		OutError = TEXT("Placement drag is not active");
		return false;
	}
	if (bPointerFromUI)
	{
		// CityGlobalMediator.OnTouchMoved stops while m_uiPressed; do not advance pointer or carry.
		return true;
	}
	FVector2D PointerLocal;
	if (!WorldToCityLocal(CityContainer, WorldHit, PointerLocal, OutError))
	{
		return false;
	}
	const FVector2D Unquantized = PointerLocal - InOutState.PreviousPointerLocal + InOutState.FractionalMove;
	if (!IsFinite(Unquantized))
	{
		OutError = TEXT("Placement pointer delta is not finite");
		return false;
	}
	const FVector2D Step(QuantizeMove(Unquantized.X), QuantizeMove(Unquantized.Y));
	if (!IsFinite(Step))
	{
		OutError = TEXT("Placement quantized pointer step is not finite");
		return false;
	}
	if (Step.IsZero())
	{
		// Source returns before updating m_startMovePos or m_fixedMoveStep.
		return true;
	}
	const FVector2D CandidateCenter = InOutState.LocalCenter + Step;
	FIntPoint CandidateOrigin;
	if (!LocalCenterToGrid(CandidateCenter, InOutState.Footprint.Size, CandidateOrigin, OutError))
	{
		return false;
	}
	FVector2D SnappedCenter;
	FRokCityConstructionFootprint CandidateFootprint = InOutState.Footprint;
	CandidateFootprint.Origin = CandidateOrigin;
	if (!GridToLocalCenter(CandidateFootprint, SnappedCenter, OutError))
	{
		return false;
	}
	bOutGridChanged = CandidateOrigin != InOutState.Footprint.Origin;
	InOutState.Footprint = CandidateFootprint;
	InOutState.LocalCenter = SnappedCenter;
	InOutState.FractionalMove = Unquantized - Step;
	InOutState.PreviousPointerLocal = PointerLocal;
	return true;
}

void FRokCityPlacementPreviewMath::CancelDrag(FRokCityPlacementDragState& InOutState)
{
	InOutState = FRokCityPlacementDragState();
}
