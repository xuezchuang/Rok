#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UTexture2D;

struct FRokSpriteClip
{
	TArray<TStrongObjectPtr<UTexture2D>> Frames[5];
	FVector2D SizeInUnity = FVector2D::ZeroVector;
	FVector2D Pivot = FVector2D::ZeroVector;
	float FrameSeconds = 0.15f;

	static float EvaluateUnityUnitScale(float Dxf);
	static TSharedPtr<FRokSpriteClip> Load(const FString& Directory, const FString& Model, const TCHAR* Pose);
};
