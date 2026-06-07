#pragma once

#include "CoreMinimal.h"
#include "RokCityViewTypes.generated.h"

UENUM(BlueprintType)
enum class ERokCityViewLayer : uint8
{
	Terrain UMETA(DisplayName = "Terrain"),
	CityGround UMETA(DisplayName = "City Ground"),
	Road UMETA(DisplayName = "Road"),
	Shadow UMETA(DisplayName = "Shadow"),
	Wall UMETA(DisplayName = "Wall"),
	BuildingGround UMETA(DisplayName = "Building Ground"),
	BuildingBody UMETA(DisplayName = "Building Body"),
	Decoration UMETA(DisplayName = "Decoration"),
	Unit UMETA(DisplayName = "Unit"),
	Hud UMETA(DisplayName = "HUD"),
	Debug UMETA(DisplayName = "Debug"),
};

USTRUCT(BlueprintType)
struct FRokCityViewCameraKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float ViewDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float VerticalFov = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float Dxf = 0.0f;
};

USTRUCT(BlueprintType)
struct FRokCityViewCameraState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	FVector2D ViewCenter = FVector2D(3.5f, -0.48f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	FVector2D WorldX = FVector2D(-5.8f, 5.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	FVector2D WorldY = FVector2D(-5.8f, 5.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	FVector UnityCameraForward = FVector(0.0f, -0.7f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float CurrentDxf = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float ViewDistance = 80.9339f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float VerticalFov = 8.03125f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float AspectRatio = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|View")
	float UeUnitsPerUnityUnit = 500.0f;
};

struct FRokCityViewMath
{
	static TArray<FRokCityViewCameraKey> MakeDefaultCameraKeys();
	static bool GetDistFovByDxf(const TArray<FRokCityViewCameraKey>& CameraKeys, float Dxf, float& OutDistance, float& OutVerticalFov);
	static float GetCameraDxfById(const TArray<FRokCityViewCameraKey>& CameraKeys, FName Id, float FallbackDxf);
	static float VerticalToHorizontalFov(float VerticalFov, float AspectRatio);
	static FVector UnityForwardToUeForward(const FVector& UnityForward);
	static FVector UnityPlanarCenterToUe(const FVector2D& ViewCenter, float UeUnitsPerUnityUnit);
};
