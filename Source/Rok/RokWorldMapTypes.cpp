#include "RokWorldMapTypes.h"

bool FRokWorldMapProjection::IsValid() const
{
	return FMath::IsFinite(UeUnitsPerUnityUnit)
		&& UeUnitsPerUnityUnit > UE_SMALL_NUMBER
		&& FMath::IsFinite(DisplayCoordinateScale)
		&& DisplayCoordinateScale > UE_SMALL_NUMBER
		&& UnityBoundsMin.X <= UnityBoundsMax.X
		&& UnityBoundsMin.Y <= UnityBoundsMax.Y;
}

float FRokWorldMapProjection::GetSafeUeScale() const
{
	return FMath::IsFinite(UeUnitsPerUnityUnit) && UeUnitsPerUnityUnit > UE_SMALL_NUMBER
		? UeUnitsPerUnityUnit
		: 1.0f;
}

float FRokWorldMapProjection::GetSafeDisplayScale() const
{
	return FMath::IsFinite(DisplayCoordinateScale) && DisplayCoordinateScale > UE_SMALL_NUMBER
		? DisplayCoordinateScale
		: 1.0f;
}

FVector2D FRokWorldMapProjection::ClampUnity(const FVector2D& UnityPosition) const
{
	return FVector2D(
		FMath::Clamp(UnityPosition.X, UnityBoundsMin.X, UnityBoundsMax.X),
		FMath::Clamp(UnityPosition.Y, UnityBoundsMin.Y, UnityBoundsMax.Y));
}

FVector FRokWorldMapProjection::UnityToUe(const FVector2D& UnityPosition) const
{
	const float SafeScale = GetSafeUeScale();
	const FVector2D Rebased = UnityPosition - UnityRebaseOrigin;
	return FVector(-Rebased.X * SafeScale, Rebased.Y * SafeScale, GroundZ);
}

FVector2D FRokWorldMapProjection::UeToUnity(const FVector& UePosition) const
{
	const float SafeScale = GetSafeUeScale();
	return UnityRebaseOrigin + FVector2D(-UePosition.X / SafeScale, UePosition.Y / SafeScale);
}

FVector2D FRokWorldMapProjection::DisplayToUnity(const FVector2D& DisplayPosition) const
{
	return DisplayPosition * GetSafeDisplayScale();
}

FVector2D FRokWorldMapProjection::UnityToDisplay(const FVector2D& UnityPosition) const
{
	return UnityPosition / GetSafeDisplayScale();
}

FName FRokWorldMapMath::WorldMapSelectableTag()
{
	static const FName Tag(TEXT("RokWorldMapSelectable"));
	return Tag;
}

FName FRokWorldMapMath::WorldMapTerrainTag()
{
	static const FName Tag(TEXT("RokWorldMapTerrain"));
	return Tag;
}

FName FRokWorldMapMath::WorldMapSourceObjectTag()
{
	static const FName Tag(TEXT("RokWorldMapSourceObject"));
	return Tag;
}

FName FRokWorldMapMath::WorldMapTreeTag()
{
	static const FName Tag(TEXT("RokWorldMapTree"));
	return Tag;
}

FName FRokWorldMapMath::WorldMapPlayerCityTag()
{
	static const FName Tag(TEXT("RokWorldMapPlayerCity"));
	return Tag;
}

FName FRokWorldMapMath::WorldMapSelectionColliderTag()
{
	static const FName Tag(TEXT("RokWorldMapSelectionCollider"));
	return Tag;
}

FName FRokWorldMapMath::MinimumCameraKey()
{
	static const FName Key(TEXT("limit_min"));
	return Key;
}

FName FRokWorldMapMath::MaximumCameraKey()
{
	static const FName Key(TEXT("limit_max"));
	return Key;
}

FName FRokWorldMapMath::TacticalCameraKey()
{
	static const FName Key(TEXT("map_tactical"));
	return Key;
}

FName FRokWorldMapMath::CityEntryThresholdCameraKey()
{
	static const FName Key(TEXT("city_bound"));
	return Key;
}

FName FRokWorldMapMath::CityEntryTargetCameraKey()
{
	static const FName Key(TEXT("city_default"));
	return Key;
}

FVector FRokWorldMapMath::UnityDirectionToUe(const FVector& UnityDirection)
{
	const FVector Converted(-UnityDirection.X, UnityDirection.Z, UnityDirection.Y);
	return Converted.IsNearlyZero()
		? FVector(0.0f, 0.7f, -0.7f).GetSafeNormal()
		: Converted.GetSafeNormal();
}

bool FRokWorldMapMath::AreCameraKeysSortedByDxf(const TArray<FRokCityViewCameraKey>& CameraKeys)
{
	for (int32 Index = 1; Index < CameraKeys.Num(); ++Index)
	{
		if (CameraKeys[Index - 1].Dxf > CameraKeys[Index].Dxf)
		{
			return false;
		}
	}
	return true;
}

float FRokWorldMapMath::GetNamedDxf(
	const TArray<FRokCityViewCameraKey>& CameraKeys,
	FName KeyId,
	float FallbackDxf)
{
	return FRokCityViewMath::GetCameraDxfById(CameraKeys, KeyId, FallbackDxf);
}

float FRokWorldMapMath::ClampDxfByNamedKeys(
	const TArray<FRokCityViewCameraKey>& CameraKeys,
	float Dxf,
	FName MinimumKeyId,
	FName MaximumKeyId)
{
	if (CameraKeys.IsEmpty())
	{
		return Dxf;
	}

	const float FirstDxf = CameraKeys[0].Dxf;
	const float LastDxf = CameraKeys.Last().Dxf;
	float MinimumDxf = GetNamedDxf(CameraKeys, MinimumKeyId, FirstDxf);
	float MaximumDxf = GetNamedDxf(CameraKeys, MaximumKeyId, LastDxf);
	if (MinimumDxf > MaximumDxf)
	{
		Swap(MinimumDxf, MaximumDxf);
	}
	return FMath::Clamp(Dxf, MinimumDxf, MaximumDxf);
}

bool FRokWorldMapMath::ShouldBeginPlayerCityEntry(
	float CurrentDxf,
	float RequestedDxf,
	float ThresholdDxf)
{
	return FMath::IsFinite(CurrentDxf)
		&& FMath::IsFinite(RequestedDxf)
		&& FMath::IsFinite(ThresholdDxf)
		&& RequestedDxf < CurrentDxf
		&& RequestedDxf <= ThresholdDxf;
}

bool FRokWorldMapMath::AreTagsWorldMapSelectable(const TArray<FName>& ActorTags)
{
	if (!ActorTags.Contains(WorldMapSelectableTag()))
	{
		return false;
	}

	static const TArray<FName> CityOnlyTags = {
		FName(TEXT("RokBuilding")),
		FName(TEXT("CityBuilding")),
		FName(TEXT("RokCitySelectable")),
		FName(TEXT("RokCitySpriteActor")),
	};
	for (const FName& CityTag : CityOnlyTags)
	{
		if (ActorTags.Contains(CityTag))
		{
			return false;
		}
	}
	return true;
}

bool FRokWorldMapMath::ShouldDisableWorldMapCollision(
	const TArray<FName>& ActorTags)
{
	return ActorTags.Contains(WorldMapTerrainTag())
		|| ActorTags.Contains(WorldMapSourceObjectTag());
}

bool FRokWorldMapMath::IsWorldMapTree(
	const TArray<FName>& ActorTags,
	FName MaterialName)
{
	if (ActorTags.Contains(WorldMapTreeTag()))
	{
		return true;
	}

	static const FName TreeMaterialName(
		TEXT("M_WorldSource_map_tree_nomask"));
	static const FName TreeMaterialVariantName(
		TEXT("M_WorldSource_map_tree_nomask_1"));
	return MaterialName == TreeMaterialName
		|| MaterialName == TreeMaterialVariantName;
}
