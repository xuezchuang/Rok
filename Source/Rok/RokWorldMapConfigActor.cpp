#include "RokWorldMapConfigActor.h"

#include "Components/SceneComponent.h"

ARokWorldMapConfigActor::ARokWorldMapConfigActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MinimumCameraKeyId = FRokWorldMapMath::MinimumCameraKey();
	MaximumCameraKeyId = FRokWorldMapMath::MaximumCameraKey();
	DefaultCameraKeyId = FRokWorldMapMath::TacticalCameraKey();
	CityEntryThresholdCameraKeyId =
		FRokWorldMapMath::CityEntryThresholdCameraKey();
	CityEntryTargetCameraKeyId =
		FRokWorldMapMath::CityEntryTargetCameraKey();
	PlayerCityLevel = TSoftObjectPtr<UWorld>(
		FSoftObjectPath(
			TEXT("/Game/Maps/L_RokUnityGameAppCity.L_RokUnityGameAppCity")));
	CameraKeys = FRokCityViewMath::MakeDefaultCameraKeys();
	NormalizeConfiguration();
}

void ARokWorldMapConfigActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NormalizeConfiguration();
}

float ARokWorldMapConfigActor::ClampDxf(float Dxf) const
{
	return FRokWorldMapMath::ClampDxfByNamedKeys(
		CameraKeys,
		Dxf,
		MinimumCameraKeyId,
		MaximumCameraKeyId);
}

float ARokWorldMapConfigActor::GetDefaultDxf() const
{
	const float FallbackDxf = CameraKeys.IsEmpty() ? 1509.0f : CameraKeys[0].Dxf;
	return ClampDxf(FRokWorldMapMath::GetNamedDxf(CameraKeys, DefaultCameraKeyId, FallbackDxf));
}

float ARokWorldMapConfigActor::GetCityEntryThresholdDxf() const
{
	return ClampDxf(FRokWorldMapMath::GetNamedDxf(
		CameraKeys,
		CityEntryThresholdCameraKeyId,
		250.0f));
}

float ARokWorldMapConfigActor::GetCityEntryTargetDxf() const
{
	return ClampDxf(FRokWorldMapMath::GetNamedDxf(
		CameraKeys,
		CityEntryTargetCameraKeyId,
		150.0f));
}

void ARokWorldMapConfigActor::NormalizeConfiguration()
{
	if (CameraKeys.IsEmpty())
	{
		CameraKeys = FRokCityViewMath::MakeDefaultCameraKeys();
	}
	CameraKeys.Sort([](const FRokCityViewCameraKey& Left, const FRokCityViewCameraKey& Right)
	{
		return Left.Dxf < Right.Dxf;
	});
	DefaultFocusUnity = Projection.ClampUnity(DefaultFocusUnity);
	if (CityEntryThresholdCameraKeyId.IsNone())
	{
		CityEntryThresholdCameraKeyId =
			FRokWorldMapMath::CityEntryThresholdCameraKey();
	}
	if (CityEntryTargetCameraKeyId.IsNone())
	{
		CityEntryTargetCameraKeyId =
			FRokWorldMapMath::CityEntryTargetCameraKey();
	}
	CityEntryTransitionSeconds = FMath::Max(
		FMath::IsFinite(CityEntryTransitionSeconds)
			? CityEntryTransitionSeconds
			: 0.5f,
		0.05f);
	if (PlayerCityLevel.IsNull())
	{
		PlayerCityLevel = TSoftObjectPtr<UWorld>(
			FSoftObjectPath(
				TEXT("/Game/Maps/L_RokUnityGameAppCity.L_RokUnityGameAppCity")));
	}
}
