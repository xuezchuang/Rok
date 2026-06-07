#include "RokCityViewTypes.h"

namespace
{
bool SolveDistFovBetweenKeys(
	double DistMin,
	double FovMin,
	double DistMax,
	double FovMax,
	double Dxf,
	double& OutDist,
	double& OutFov)
{
	OutDist = 0.0;
	OutFov = 0.0;

	const double MinProduct = DistMin * FovMin;
	const double MaxProduct = DistMax * FovMax;
	if (MinProduct > MaxProduct || Dxf < MinProduct || Dxf > MaxProduct)
	{
		return false;
	}

	if (FMath::IsNearlyEqual(Dxf, MinProduct))
	{
		OutDist = DistMin;
		OutFov = FovMin;
		return true;
	}

	if (FMath::IsNearlyEqual(Dxf, MaxProduct))
	{
		OutDist = DistMax;
		OutFov = FovMax;
		return true;
	}

	if (FMath::IsNearlyEqual(DistMax, DistMin))
	{
		OutDist = DistMin;
		OutFov = Dxf / DistMin;
		return true;
	}

	if (FMath::IsNearlyEqual(FovMax, FovMin))
	{
		OutFov = FovMin;
		OutDist = Dxf / FovMin;
		return true;
	}

	double Low = 0.0;
	double High = 1.0;
	double Value = -1.0;
	for (int32 Index = 0; Index < 24; ++Index)
	{
		const double Mid = (High + Low) * 0.5;
		const double Product = (DistMin + Mid * (DistMax - DistMin)) * (FovMin + Mid * (FovMax - FovMin));
		if (FMath::IsNearlyEqual(Dxf, Product))
		{
			Value = Mid;
			break;
		}

		if (Dxf > Product)
		{
			if (FMath::IsNearlyEqual(Low, Mid))
			{
				Value = Mid;
				break;
			}
			Low = Mid;
		}
		else
		{
			if (FMath::IsNearlyEqual(High, Mid))
			{
				Value = Mid;
				break;
			}
			High = Mid;
		}
	}

	if (Value < 0.0)
	{
		Value = Low;
	}

	OutDist = DistMin + Value * (DistMax - DistMin);
	OutFov = FovMin + Value * (FovMax - FovMin);
	return true;
}
}

TArray<FRokCityViewCameraKey> FRokCityViewMath::MakeDefaultCameraKeys()
{
	TArray<FRokCityViewCameraKey> Keys;
	auto AddKey = [&Keys](const TCHAR* Id, float Distance, float VerticalFov, float Dxf)
	{
		FRokCityViewCameraKey Key;
		Key.Id = FName(Id);
		Key.ViewDistance = Distance;
		Key.VerticalFov = VerticalFov;
		Key.Dxf = Dxf;
		Keys.Add(Key);
	};

	AddKey(TEXT("limit_min"), 14.47f, 5.3f, 76.691f);
	AddKey(TEXT("min"), 15.47f, 5.33f, 82.4551f);
	AddKey(TEXT("city_default"), 20.0f, 7.5f, 150.0f);
	AddKey(TEXT("city_age"), 23.6f, 10.0f, 236.0f);
	AddKey(TEXT("city_bound"), 25.0f, 10.0f, 250.0f);
	AddKey(TEXT("hide_tactics_UI"), 24.69f, 11.0f, 271.59f);
	AddKey(TEXT("city"), 24.693f, 11.0f, 271.623f);
	AddKey(TEXT("showbuild"), 33.333f, 30.0f, 1000.0f);
	AddKey(TEXT("FTE_Tactics"), 37.0f, 30.0f, 1110.0f);
	AddKey(TEXT("init"), 45.0f, 30.0f, 1350.0f);
	AddKey(TEXT("map_tactical"), 50.3f, 30.0f, 1509.0f);
	AddKey(TEXT("FTE_Scout"), 99.8f, 30.0f, 2994.0f);
	AddKey(TEXT("dispatch"), 99.9f, 30.0f, 2997.0f);
	AddKey(TEXT("TacticsToStrategy1"), 150.0f, 30.0f, 4500.0f);
	AddKey(TEXT("TacticsToStrategy2"), 180.0f, 30.0f, 5400.0f);
	AddKey(TEXT("startgame"), 200.0f, 30.0f, 6000.0f);
	AddKey(TEXT("strategic_min"), 668.0f, 30.0f, 20040.0f);
	AddKey(TEXT("max"), 2424.0f, 30.0f, 72720.0f);
	AddKey(TEXT("limit_max"), 2425.0f, 30.0f, 72750.0f);
	AddKey(TEXT("map_max"), 5000.0f, 30.0f, 150000.0f);
	return Keys;
}

bool FRokCityViewMath::GetDistFovByDxf(const TArray<FRokCityViewCameraKey>& CameraKeys, float Dxf, float& OutDistance, float& OutVerticalFov)
{
	OutDistance = 0.0f;
	OutVerticalFov = 0.0f;
	if (CameraKeys.Num() == 0)
	{
		return false;
	}

	const FRokCityViewCameraKey& MinKey = CameraKeys[0];
	const FRokCityViewCameraKey& MaxKey = CameraKeys.Last();
	const float ClampedDxf = FMath::Clamp(Dxf, MinKey.Dxf, MaxKey.Dxf);

	if (ClampedDxf <= MinKey.Dxf)
	{
		OutDistance = MinKey.ViewDistance;
		OutVerticalFov = MinKey.VerticalFov;
		return true;
	}

	if (ClampedDxf >= MaxKey.Dxf)
	{
		OutDistance = MaxKey.ViewDistance;
		OutVerticalFov = MaxKey.VerticalFov;
		return true;
	}

	for (int32 Index = 0; Index < CameraKeys.Num() - 1; ++Index)
	{
		const FRokCityViewCameraKey& LowKey = CameraKeys[Index];
		const FRokCityViewCameraKey& HighKey = CameraKeys[Index + 1];
		if (FMath::IsNearlyEqual(ClampedDxf, HighKey.Dxf))
		{
			OutDistance = HighKey.ViewDistance;
			OutVerticalFov = HighKey.VerticalFov;
			return true;
		}

		if (ClampedDxf > LowKey.Dxf && ClampedDxf < HighKey.Dxf)
		{
			double SolvedDistance = 0.0;
			double SolvedFov = 0.0;
			if (SolveDistFovBetweenKeys(
				LowKey.ViewDistance,
				LowKey.VerticalFov,
				HighKey.ViewDistance,
				HighKey.VerticalFov,
				ClampedDxf,
				SolvedDistance,
				SolvedFov))
			{
				OutDistance = static_cast<float>(SolvedDistance);
				OutVerticalFov = static_cast<float>(SolvedFov);
				return true;
			}
		}
	}

	return false;
}

float FRokCityViewMath::GetCameraDxfById(const TArray<FRokCityViewCameraKey>& CameraKeys, FName Id, float FallbackDxf)
{
	for (const FRokCityViewCameraKey& Key : CameraKeys)
	{
		if (Key.Id == Id)
		{
			return Key.Dxf;
		}
	}
	return FallbackDxf;
}

float FRokCityViewMath::VerticalToHorizontalFov(float VerticalFov, float AspectRatio)
{
	const float SafeAspectRatio = FMath::Max(AspectRatio, 0.01f);
	return FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFov) * 0.5f) * SafeAspectRatio));
}

FVector FRokCityViewMath::UnityForwardToUeForward(const FVector& UnityForward)
{
	const FVector Converted(UnityForward.X, UnityForward.Z, UnityForward.Y);
	return Converted.IsNearlyZero() ? FVector(0.0f, 0.7f, -0.7f).GetSafeNormal() : Converted.GetSafeNormal();
}

FVector FRokCityViewMath::UnityPlanarCenterToUe(const FVector2D& ViewCenter, float UeUnitsPerUnityUnit)
{
	return FVector(-ViewCenter.X * UeUnitsPerUnityUnit, ViewCenter.Y * UeUnitsPerUnityUnit, 0.0f);
}
