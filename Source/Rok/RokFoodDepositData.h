#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"

struct FRokFoodDepositItem
{
	FString Name;
	FVector Position = FVector::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	int32 Priority = 0;
	FString TexturePath;
	FString MaskPath;
	FVector SpritePosition = FVector::ZeroVector;
	FVector SpriteRight = FVector::ForwardVector;
	FVector SpriteUp = FVector::UpVector;
	FVector2D SpriteSize = FVector2D::ZeroVector;
	FVector2D Pivot = FVector2D(0.5, 0.5);
	FLinearColor Color1 = FLinearColor::White;
	FLinearColor Color2 = FLinearColor::White;
};

struct FRokFoodDepositData
{
	TArray<FRokFoodDepositItem> Items;
	FRichCurve ScaleCurve;
	FVector2D BoundsSize = FVector2D::ZeroVector;
	float MaxScale = 0;
	float UnitsToCentimeters = 0;
	bool Load();
	bool LoadJson(const FString& Text);
	float EvaluateScale(float Dxf) const;
};

// Source GridCollideMgr state survives zoom updates. Enlarging considers only
// active items; shrinking reconsiders all. Equal-priority order is the captured
// native Unity probe order stored in the manifest, not a fabricated tie-break.
struct FRokFoodDepositState
{
	TArray<bool> Active;
	float LastScale = -1;
	void Reset(int32 Count);
	void Update(const FRokFoodDepositData& Data, float Scale);
};
