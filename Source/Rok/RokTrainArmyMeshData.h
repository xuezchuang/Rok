#pragma once
#include "CoreMinimal.h"
namespace RokTrainArmyMesh
{
struct FDraw { FString Identity, Texture; TArray<FVector2f> XY, UV; TArray<uint16> Indices; FLinearColor Tint; };
struct FFrame { TArray<FDraw> Draws; };
struct FClip { int32 ArmsId = 0; FString ArmsShow; float Duration = 0; TArray<FFrame> Frames; TMap<FString, FString> Sources; };
FString Sha256(TConstArrayView<uint8> Bytes);
bool Parse(const FString& Json, int32 ArmsId, const FString& ArmsShow, FClip& Out, FString& Error);
inline FVector2f Project(FVector2f P) { return FVector2f(481.f + P.X * 85.f, 676.7f - P.Y * 85.f); }
inline float Advance(float Time, float Delta, float Duration) { return FMath::IsFinite(Delta) && Duration > 0 ? FMath::Fmod(Time + FMath::Max(0.f, Delta), Duration) : Time; }
inline FLinearColor PremultipliedTint(FLinearColor Source, FLinearColor Parent)
{ const float A = Source.A * Parent.A; return FLinearColor(Source.R * Parent.R * A, Source.G * Parent.G * A, Source.B * Parent.B * A, A); }
inline bool CanInterpolate(const FDraw& A, const FDraw& B)
{ return A.Identity == B.Identity && A.Texture == B.Texture && A.Indices == B.Indices && A.UV == B.UV && A.XY.Num() == B.XY.Num(); }
}
