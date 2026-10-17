#pragma once

#include "CoreMinimal.h"

// Pending, source-shaped CPU lifecycle only. Coordinates remain in the Unity
// particle system's local frame; the renderer owns prefab/world conversion.
struct FRokTileRippleCurveKey
{
	double Time = 0.0;
	double Value = 0.0;
	double InSlope = 0.0;
	double OutSlope = 0.0;
};

struct FRokTileRippleColorKey
{
	double Time = 0.0;
	double Alpha = 0.0;
};

struct FRokTileRippleSourceConfig
{
	FString SystemName;
	int64 ParticleSystemFileId = 0;
	double DurationSeconds = 0.0;
	double RatePerSecond = 0.0;
	double LifetimeMinSeconds = 0.0;
	double LifetimeMaxSeconds = 0.0;
	double StartSizeMin = 0.0;
	double StartSizeMax = 0.0;
	double ShapeEdgeRadius = 0.0;
	double ShapeArcDegrees = 0.0; // Serialized; SingleSidedEdge does not consume arc.
	double InitialRotationRadians = 0.0;
	FVector3d LocalVelocity = FVector3d::ZeroVector;
	FLinearColor StartColorMin = FLinearColor::White;
	FLinearColor StartColorMax = FLinearColor::White;
	int32 MaxParticles = 0;
	TArray<FRokTileRippleCurveKey> SizeKeys;
	TArray<FRokTileRippleColorKey> AlphaKeys;

	bool Validate(FString& Error) const;
	// Loads the generated, source-traced two-emitter fixture. Caller chooses path.
	static bool LoadSourceFixture(const FString& AbsolutePath,
		TArray<FRokTileRippleSourceConfig>& OutConfigs, FString& Error);
};

struct FRokTileRippleParticleSnapshot
{
	uint64 BirthOrdinal = 0;
	double BirthSeconds = 0.0;
	double AgeSeconds = 0.0;
	double LifetimeSeconds = 0.0;
	FVector3d LocalPosition = FVector3d::ZeroVector;
	double CurrentSize = 0.0;
	FLinearColor CurrentColor = FLinearColor::Transparent;
	double InitialRotationRadians = 0.0; // Scalar source config only; no axis or Quad matrix is inferred.
};

class FRokUnityTileRippleSimulation
{
public:
	explicit FRokUnityTileRippleSimulation(const FRokTileRippleSourceConfig& InConfig);
	bool Reset(int32 Seed, FString& Error);
	bool Advance(double DeltaSeconds, FString& Error);
	void Snapshot(TArray<FRokTileRippleParticleSnapshot>& OutParticles) const;
	double GetElapsedSeconds() const { return ElapsedSeconds; }
	// A constant integral rate over the source five-second loop is seamless at
	// cycle boundaries; this is the current source cycle phase for diagnostics.
	double GetCycleSeconds() const { return FMath::Fmod(ElapsedSeconds, Config.DurationSeconds); }
	uint64 GetTotalBirths() const { return TotalBirths; }
	int32 GetLiveCount() const { return LiveParticles.Num(); }

private:
	struct FParticle
	{
		uint64 Ordinal = 0;
		double BirthSeconds = 0.0;
		double LifetimeSeconds = 0.0;
		double StartSize = 0.0;
		FVector3d SpawnPosition = FVector3d::ZeroVector;
		FLinearColor StartColor = FLinearColor::White;
	};

	FRokTileRippleSourceConfig Config;
	FRandomStream Random;
	TArray<FParticle> LiveParticles;
	double ElapsedSeconds = 0.0;
	uint64 NextBirthOrdinal = 1;
	uint64 TotalBirths = 0;
	bool bReady = false;

	static double SampleSize(const TArray<FRokTileRippleCurveKey>& Keys, double T);
	static double SampleAlpha(const TArray<FRokTileRippleColorKey>& Keys, double T);
	void SpawnAt(double BirthSeconds);
};
