#pragma once

#include "CoreMinimal.h"
#include "RokCityViewTypes.h"
#include "RokWorldMapTypes.generated.h"

USTRUCT(BlueprintType)
struct FRokWorldMapProjection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	FVector2D UnityBoundsMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	FVector2D UnityBoundsMax = FVector2D(7200.0f, 7200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	FVector2D UnityRebaseOrigin = FVector2D(6876.0f, 942.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	float UeUnitsPerUnityUnit = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	float GroundZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	float DisplayCoordinateScale = 6.0f;

	bool IsValid() const;
	float GetSafeUeScale() const;
	float GetSafeDisplayScale() const;
	FVector2D ClampUnity(const FVector2D& UnityPosition) const;
	FVector UnityToUe(const FVector2D& UnityPosition) const;
	FVector2D UeToUnity(const FVector& UePosition) const;
	FVector2D DisplayToUnity(const FVector2D& DisplayPosition) const;
	FVector2D UnityToDisplay(const FVector2D& UnityPosition) const;
};

USTRUCT(BlueprintType)
struct FRokWorldMapCameraState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	FVector2D ViewCenterUnity = FVector2D(6876.0f, 942.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|World Map")
	float CurrentDxf = 1509.0f;
};

struct FRokWorldMapMath
{
	static FName WorldMapSelectableTag();
	static FName WorldMapTerrainTag();
	static FName WorldMapSourceObjectTag();
	static FName WorldMapTreeTag();
	static FName WorldMapPlayerCityTag();
	static FName WorldMapSelectionColliderTag();
	static FName MinimumCameraKey();
	static FName MaximumCameraKey();
	static FName TacticalCameraKey();
	static FName CityEntryThresholdCameraKey();
	static FName CityEntryTargetCameraKey();

	static FVector UnityDirectionToUe(const FVector& UnityDirection);
	static bool AreCameraKeysSortedByDxf(const TArray<FRokCityViewCameraKey>& CameraKeys);
	static float GetNamedDxf(
		const TArray<FRokCityViewCameraKey>& CameraKeys,
		FName KeyId,
		float FallbackDxf);
	static float ClampDxfByNamedKeys(
		const TArray<FRokCityViewCameraKey>& CameraKeys,
		float Dxf,
		FName MinimumKeyId,
		FName MaximumKeyId);
	static bool ShouldBeginPlayerCityEntry(
		float CurrentDxf,
		float RequestedDxf,
		float ThresholdDxf);
	static bool ShouldDisableWorldMapCollision(
		const TArray<FName>& ActorTags);
	static bool AreTagsWorldMapSelectable(const TArray<FName>& ActorTags);
	static bool IsWorldMapTree(
		const TArray<FName>& ActorTags,
		FName MaterialName);
};
