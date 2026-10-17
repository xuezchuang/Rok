#pragma once

#include "CoreMinimal.h"
#include "RokCityConstructionGrid.h"

/** Pure preview state. City-local X/Y correspond to Unity city-container X/Z. */
struct FRokCityPlacementDragState
{
	FRokCityConstructionFootprint Footprint;
	FVector2D LocalCenter = FVector2D::ZeroVector;
	FVector2D PreviousPointerLocal = FVector2D::ZeroVector;
	FVector2D FractionalMove = FVector2D::ZeroVector;
	bool bActive = false;
};

/**
 * Source: ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/CityGlobalMediator.cs
 * ConvertCityObjTileToLocal, ConvertCityObjLocalToTile, MakeLocalPos, OnTouchMoved.
 * Grid size 0.1 comes from ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/CityObjData.cs.
 * The caller supplies a verified city-container
 * transform and an already resolved world-plane pointer hit; no camera or map origin is inferred.
 */
class ROK_API FRokCityPlacementPreviewMath
{
public:
	static bool GridToLocalCenter(const FRokCityConstructionFootprint& Footprint,
		FVector2D& OutCenter, FString& OutError);
	static bool LocalCenterToGrid(const FVector2D& LocalCenter, const FIntPoint& Size,
		FIntPoint& OutOrigin, FString& OutError);
	static bool WorldToCityLocal(const FTransform& CityContainer, const FVector& WorldHit,
		FVector2D& OutLocal, FString& OutError);

	static bool BeginDrag(FRokCityPlacementDragState& InOutState,
		const FRokCityConstructionFootprint& Footprint, const FTransform& CityContainer,
		const FVector& WorldHit, bool bPointerFromUI, FString& OutError);
	static bool MoveDrag(FRokCityPlacementDragState& InOutState, const FTransform& CityContainer,
		const FVector& WorldHit, bool bPointerFromUI, bool& bOutGridChanged, FString& OutError);
	static void CancelDrag(FRokCityPlacementDragState& InOutState);
};
