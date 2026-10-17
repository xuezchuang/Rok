#pragma once
#include "CoreMinimal.h"

namespace RokBuildingPreviewGlow
{
struct FAlphaKey { double Time=0, Value=0, InSlope=0, OutSlope=0; };
struct FLayer
{
    FString Name;
    FVector2D Center=FVector2D::ZeroVector, Size=FVector2D::ZeroVector;
    FVector Scale=FVector::OneVector;
    FQuat InitialRotation=FQuat::Identity;
    double DegreesPerSecond=0;
    TArray<FAlphaKey> AlphaKeys;
    TArray<FVector> Vertices;
    // Original Unity bottom-left UV. Slate adapter must flip V exactly once.
    TArray<FVector2D> UV;
    TArray<int32> Triangles;
};
struct FLayerSample
{
    double Alpha=0;
    FQuat LocalRotation=FQuat::Identity;
    TArray<FVector2D> ScreenVertices;
};
class FData
{
public:
    bool Load(const FString& Path,FString& Error);
    bool Sample(double Seconds,TArray<FLayerSample>& Out) const;
    const TArray<FLayer>& GetLayers() const { return Layers; }
    bool IsLoaded() const { return Layers.Num()==2; }
    static FString DefaultPath();
private:
    TArray<FLayer> Layers;
};
// Explicit local presentation clock. No world tick, server time or visibility policy.
class FPlayback
{
public:
    void Reset() { Seconds=0; }
    bool Advance(double DeltaSeconds);
    double GetTime() const { return Seconds; }
    bool Sample(const FData& Data,TArray<FLayerSample>& Out) const { return Data.Sample(Seconds,Out); }
private:
    double Seconds=0;
};
}
