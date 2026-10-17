#include "RokUnityTileRippleSimulation.h"
#include "RokTrainArmyMeshData.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool Finite(double Value) { return FMath::IsFinite(Value); }

bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double& Out)
{
	return Object.IsValid() && Object->TryGetNumberField(Key, Out) && Finite(Out);
}

bool ReadObject(const TSharedPtr<FJsonValue>& Value, TSharedPtr<FJsonObject>& Out)
{
	const TSharedPtr<FJsonObject>* Pointer = nullptr;
	if (!Value.IsValid() || !Value->TryGetObject(Pointer) || !Pointer || !Pointer->IsValid()) return false;
	Out = *Pointer;
	return true;
}

bool ReadColor(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FLinearColor& Out)
{
	const TSharedPtr<FJsonObject>* Color = nullptr;
	double R = 0, G = 0, B = 0, A = 0;
	if (!Object->TryGetObjectField(Key, Color) || !Color || !Color->IsValid()
		|| !ReadNumber(*Color, TEXT("r"), R) || !ReadNumber(*Color, TEXT("g"), G)
		|| !ReadNumber(*Color, TEXT("b"), B) || !ReadNumber(*Color, TEXT("a"), A)
		|| R < 0 || R > 1 || G < 0 || G > 1 || B < 0 || B > 1 || A < 0 || A > 1) return false;
	Out = FLinearColor(R, G, B, A);
	return true;
}

double Hermite(double A, double ATangent, double B, double BTangent, double T)
{
	const double T2 = T * T, T3 = T2 * T;
	return (2 * T3 - 3 * T2 + 1) * A + (T3 - 2 * T2 + T) * ATangent
		+ (-2 * T3 + 3 * T2) * B + (T3 - T2) * BTangent;
}
}

bool FRokTileRippleSourceConfig::Validate(FString& Error) const
{
	Error.Reset();
	if (SystemName.IsEmpty() || ParticleSystemFileId <= 0
		|| !Finite(DurationSeconds) || DurationSeconds <= 0
		|| !Finite(RatePerSecond) || RatePerSecond <= 0 || RatePerSecond > 1000
		|| !FMath::IsNearlyEqual(DurationSeconds * RatePerSecond,
			FMath::FloorToDouble(DurationSeconds * RatePerSecond + 0.5), 1.0e-9)
		|| !Finite(LifetimeMinSeconds) || !Finite(LifetimeMaxSeconds)
		|| LifetimeMinSeconds <= 0 || LifetimeMaxSeconds < LifetimeMinSeconds
		|| !Finite(StartSizeMin) || !Finite(StartSizeMax) || StartSizeMin <= 0 || StartSizeMax < StartSizeMin
		|| !Finite(ShapeEdgeRadius) || ShapeEdgeRadius < 0
		|| !Finite(ShapeArcDegrees) || ShapeArcDegrees < 0 || ShapeArcDegrees > 360
		|| !Finite(InitialRotationRadians)
		|| !Finite(LocalVelocity.X) || !Finite(LocalVelocity.Y) || !Finite(LocalVelocity.Z)
		|| MaxParticles <= 0 || RatePerSecond * LifetimeMaxSeconds + 2 > MaxParticles
		|| SizeKeys.Num() < 2 || AlphaKeys.Num() < 2)
	{
		Error = TEXT("Tile ripple source config has invalid scalar values or insufficient particle capacity");
		return false;
	}
	const auto ValidColor = [](const FLinearColor& C)
	{
		return FMath::IsFinite(C.R) && FMath::IsFinite(C.G) && FMath::IsFinite(C.B)
			&& FMath::IsFinite(C.A) && C.R >= 0 && C.R <= 1 && C.G >= 0 && C.G <= 1
			&& C.B >= 0 && C.B <= 1 && C.A >= 0 && C.A <= 1;
	};
	if (!ValidColor(StartColorMin) || !ValidColor(StartColorMax))
	{
		Error = TEXT("Tile ripple start-color range invalid"); return false;
	}
	for (int32 Index = 0; Index < SizeKeys.Num(); ++Index)
	{
		const FRokTileRippleCurveKey& K = SizeKeys[Index];
		if (!Finite(K.Time) || !Finite(K.Value) || !Finite(K.InSlope) || !Finite(K.OutSlope)
			|| K.Time < 0 || K.Time > 1 || K.Value < 0
			|| (Index > 0 && K.Time <= SizeKeys[Index - 1].Time))
		{
			Error = TEXT("Tile ripple size curve invalid"); return false;
		}
	}
	for (int32 Index = 0; Index < AlphaKeys.Num(); ++Index)
	{
		const FRokTileRippleColorKey& K = AlphaKeys[Index];
		if (!Finite(K.Time) || !Finite(K.Alpha) || K.Time < 0 || K.Time > 1
			|| K.Alpha < 0 || K.Alpha > 1
			|| (Index > 0 && K.Time <= AlphaKeys[Index - 1].Time))
		{
			Error = TEXT("Tile ripple alpha gradient invalid"); return false;
		}
	}
	if (!FMath::IsNearlyZero(SizeKeys[0].Time) || !FMath::IsNearlyEqual(SizeKeys.Last().Time, 1.0)
		|| !FMath::IsNearlyZero(AlphaKeys[0].Time) || !FMath::IsNearlyEqual(AlphaKeys.Last().Time, 1.0))
	{
		Error = TEXT("Tile ripple curves must cover normalized life 0..1"); return false;
	}
	return true;
}

bool FRokTileRippleSourceConfig::LoadSourceFixture(const FString& AbsolutePath,
	TArray<FRokTileRippleSourceConfig>& OutConfigs, FString& Error)
{
	OutConfigs.Reset(); Error.Reset();
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *AbsolutePath) || Bytes.IsEmpty() || Bytes.Num() > 65536
		|| RokTrainArmyMesh::Sha256(Bytes) != TEXT("cc365fb2e20dfaaa8da4aa9e0766e4754bffa6e2130cec053349ccc26b8ce126"))
	{
		Error = TEXT("Tile ripple fixture missing, oversized or source bytes changed"); return false;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	const FString Json(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		Error = TEXT("Tile ripple fixture JSON invalid"); return false;
	}
	FString Schema, SourceHash, ContractHash, NativeHash, GeometryHash;
	const TArray<TSharedPtr<FJsonValue>>* Systems = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.tile_ripple.simulation_source.v1")
		|| !Root->TryGetStringField(TEXT("source_v1_sha256"), SourceHash)
		|| SourceHash != TEXT("3ac643b894e56d1a6b07935beb1ddee94967d453798bb8dcb23f63b0fb4f2161")
		|| !Root->TryGetStringField(TEXT("contract_revision2_sha256"), ContractHash) || ContractHash.Len() != 64
		|| !Root->TryGetStringField(TEXT("native_v2_sha256"), NativeHash) || NativeHash.Len() != 64
		|| !Root->TryGetStringField(TEXT("geometry_v4_sha256"), GeometryHash)
		|| GeometryHash != TEXT("11483cc64cc1564c1a22fca88f6da817d70b6bdfab259900f562c379d5eff1d4")
		|| !Root->TryGetArrayField(TEXT("systems"), Systems) || !Systems || Systems->Num() != 2)
	{
		Error = TEXT("Tile ripple fixture source identity or two-system schema invalid"); return false;
	}
	TArray<FRokTileRippleSourceConfig> Parsed;
	for (int32 SystemIndex = 0; SystemIndex < 2; ++SystemIndex)
	{
		TSharedPtr<FJsonObject> Object;
		FRokTileRippleSourceConfig C;
		FString FileId;
		double MaxParticlesNumber = 0;
		const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Alpha = nullptr;
		const TSharedPtr<FJsonObject>* Velocity = nullptr;
		if (!ReadObject((*Systems)[SystemIndex], Object)
			|| !Object->TryGetStringField(TEXT("name"), C.SystemName)
			|| C.SystemName != (SystemIndex == 0 ? TEXT("bowen") : TEXT("bowen (1)"))
			|| !Object->TryGetStringField(TEXT("particle_system_file_id"), FileId)
			|| !LexTryParseString(C.ParticleSystemFileId, *FileId)
			|| !ReadNumber(Object, TEXT("duration_seconds"), C.DurationSeconds)
			|| !ReadNumber(Object, TEXT("rate_per_second"), C.RatePerSecond)
			|| !ReadNumber(Object, TEXT("lifetime_min_seconds"), C.LifetimeMinSeconds)
			|| !ReadNumber(Object, TEXT("lifetime_max_seconds"), C.LifetimeMaxSeconds)
			|| !ReadNumber(Object, TEXT("start_size_min"), C.StartSizeMin)
			|| !ReadNumber(Object, TEXT("start_size_max"), C.StartSizeMax)
			|| !ReadNumber(Object, TEXT("shape_edge_radius"), C.ShapeEdgeRadius)
			|| !ReadNumber(Object, TEXT("shape_arc_degrees_serialized"), C.ShapeArcDegrees)
			|| !ReadNumber(Object, TEXT("initial_rotation_radians"), C.InitialRotationRadians)
			|| !ReadNumber(Object, TEXT("max_particles"), MaxParticlesNumber)
			|| MaxParticlesNumber < 1 || MaxParticlesNumber > MAX_int32
			|| MaxParticlesNumber != FMath::FloorToDouble(MaxParticlesNumber)
			|| !Object->TryGetObjectField(TEXT("local_velocity"), Velocity) || !Velocity || !Velocity->IsValid()
			|| !ReadNumber(*Velocity, TEXT("x"), C.LocalVelocity.X)
			|| !ReadNumber(*Velocity, TEXT("y"), C.LocalVelocity.Y)
			|| !ReadNumber(*Velocity, TEXT("z"), C.LocalVelocity.Z)
			|| !ReadColor(Object, TEXT("start_color_min"), C.StartColorMin)
			|| !ReadColor(Object, TEXT("start_color_max"), C.StartColorMax)
			|| !Object->TryGetArrayField(TEXT("size_keys"), Size) || !Size || Size->Num() != 2
			|| !Object->TryGetArrayField(TEXT("alpha_keys"), Alpha) || !Alpha || Alpha->Num() != 3)
		{
			Error = TEXT("Tile ripple fixture system fields invalid"); return false;
		}
		C.MaxParticles = static_cast<int32>(MaxParticlesNumber);
		for (const TSharedPtr<FJsonValue>& Value : *Size)
		{
			TSharedPtr<FJsonObject> Key;
			FRokTileRippleCurveKey K;
			if (!ReadObject(Value, Key) || !ReadNumber(Key, TEXT("time"), K.Time)
				|| !ReadNumber(Key, TEXT("value"), K.Value)
				|| !ReadNumber(Key, TEXT("in_slope"), K.InSlope)
				|| !ReadNumber(Key, TEXT("out_slope"), K.OutSlope))
			{
				Error = TEXT("Tile ripple size key invalid"); return false;
			}
			C.SizeKeys.Add(K);
		}
		for (const TSharedPtr<FJsonValue>& Value : *Alpha)
		{
			TSharedPtr<FJsonObject> Key;
			FRokTileRippleColorKey K;
			if (!ReadObject(Value, Key) || !ReadNumber(Key, TEXT("time"), K.Time)
				|| !ReadNumber(Key, TEXT("alpha"), K.Alpha))
			{
				Error = TEXT("Tile ripple alpha key invalid"); return false;
			}
			C.AlphaKeys.Add(K);
		}
		if (!C.Validate(Error)) return false;
		Parsed.Add(MoveTemp(C));
	}
	OutConfigs = MoveTemp(Parsed);
	return true;
}

FRokUnityTileRippleSimulation::FRokUnityTileRippleSimulation(const FRokTileRippleSourceConfig& InConfig)
	: Config(InConfig) {}

bool FRokUnityTileRippleSimulation::Reset(int32 Seed, FString& Error)
{
	if (!Config.Validate(Error)) return false;
	Random.Initialize(Seed);
	LiveParticles.Reset();
	ElapsedSeconds = 0.0;
	NextBirthOrdinal = 1;
	TotalBirths = 0;
	bReady = true;
	return true;
}

void FRokUnityTileRippleSimulation::SpawnAt(double BirthSeconds)
{
	// One per-particle UE-local stream makes catch-up and time partitioning stable.
	// Unity's internal RNG sequence is not claimed to match this one.
	FRandomStream SpawnRandom(static_cast<int32>(HashCombine(
		GetTypeHash(Random.GetInitialSeed()), GetTypeHash(NextBirthOrdinal))));
	const auto Range = [&SpawnRandom](double Min, double Max)
	{
		return Min + static_cast<double>(SpawnRandom.FRand()) * (Max - Min);
	};
	FParticle Particle;
	Particle.Ordinal = NextBirthOrdinal;
	Particle.BirthSeconds = BirthSeconds;
	Particle.LifetimeSeconds = Range(Config.LifetimeMinSeconds, Config.LifetimeMaxSeconds);
	Particle.StartSize = Range(Config.StartSizeMin, Config.StartSizeMax);
	const double ColorT = static_cast<double>(SpawnRandom.FRand());
	Particle.StartColor = FMath::Lerp(Config.StartColorMin, Config.StartColorMax, static_cast<float>(ColorT));
	Particle.SpawnPosition.X = Range(-Config.ShapeEdgeRadius, Config.ShapeEdgeRadius);
	Particle.SpawnPosition.Y = 0.0;
	Particle.SpawnPosition.Z = 0.0;
	LiveParticles.Add(Particle);
}

bool FRokUnityTileRippleSimulation::Advance(double DeltaSeconds, FString& Error)
{
	Error.Reset();
	if (!bReady || !Finite(DeltaSeconds) || DeltaSeconds < 0
		|| !Finite(ElapsedSeconds + DeltaSeconds)
		|| ElapsedSeconds + DeltaSeconds > 1.0e9)
	{
		Error = TEXT("Tile ripple clock invalid or simulation not reset"); return false;
	}
	// Nanosecond quantization removes accumulated 0.1+... boundary drift.
	const double NewTime = FMath::FloorToDouble((ElapsedSeconds + DeltaSeconds) * 1.0e9 + 0.5) / 1.0e9;
	const double LastAsDouble = FMath::FloorToDouble(NewTime * Config.RatePerSecond + 1.0e-9);
	if (!Finite(LastAsDouble) || LastAsDouble > 1.0e12)
	{
		Error = TEXT("Tile ripple birth ordinal exceeds bounded simulation range"); return false;
	}
	const uint64 LastOrdinal = static_cast<uint64>(LastAsDouble);
	if (LastOrdinal + 1 < NextBirthOrdinal)
	{
		Error = TEXT("Tile ripple clock cannot move backwards"); return false;
	}
	LiveParticles.RemoveAll([NewTime](const FParticle& P)
	{
		return NewTime - P.BirthSeconds >= P.LifetimeSeconds;
	});
	// A large legal delta need not instantiate particles already guaranteed dead.
	const double OldestLiveOrdinal = FMath::FloorToDouble(
		FMath::Max(0.0, NewTime - Config.LifetimeMaxSeconds) * Config.RatePerSecond) + 1;
	const uint64 FirstToInstantiate = FMath::Max(NextBirthOrdinal, static_cast<uint64>(OldestLiveOrdinal));
	for (uint64 Ordinal = FirstToInstantiate; Ordinal <= LastOrdinal; ++Ordinal)
	{
		NextBirthOrdinal = Ordinal;
		const double BirthSeconds = static_cast<double>(Ordinal) / Config.RatePerSecond;
		SpawnAt(BirthSeconds);
	}
	LiveParticles.RemoveAll([NewTime](const FParticle& P)
	{
		return NewTime - P.BirthSeconds >= P.LifetimeSeconds;
	});
	ElapsedSeconds = NewTime;
	NextBirthOrdinal = LastOrdinal + 1;
	TotalBirths = LastOrdinal;
	return true;
}

double FRokUnityTileRippleSimulation::SampleSize(const TArray<FRokTileRippleCurveKey>& Keys, double T)
{
	const double Clamped = FMath::Clamp(T, 0.0, 1.0);
	for (int32 I = 1; I < Keys.Num(); ++I)
	{
		if (Clamped <= Keys[I].Time)
		{
			const double Span = Keys[I].Time - Keys[I - 1].Time;
			return Hermite(Keys[I - 1].Value, Keys[I - 1].OutSlope * Span,
				Keys[I].Value, Keys[I].InSlope * Span,
				(Clamped - Keys[I - 1].Time) / Span);
		}
	}
	return Keys.Last().Value;
}

double FRokUnityTileRippleSimulation::SampleAlpha(const TArray<FRokTileRippleColorKey>& Keys, double T)
{
	const double Clamped = FMath::Clamp(T, 0.0, 1.0);
	for (int32 I = 1; I < Keys.Num(); ++I)
	{
		if (Clamped <= Keys[I].Time)
		{
			const double F = (Clamped - Keys[I - 1].Time) / (Keys[I].Time - Keys[I - 1].Time);
			return FMath::Lerp(Keys[I - 1].Alpha, Keys[I].Alpha, F);
		}
	}
	return Keys.Last().Alpha;
}

void FRokUnityTileRippleSimulation::Snapshot(TArray<FRokTileRippleParticleSnapshot>& OutParticles) const
{
	OutParticles.Reset();
	for (const FParticle& Particle : LiveParticles)
	{
		FRokTileRippleParticleSnapshot View;
		View.BirthOrdinal = Particle.Ordinal;
		View.BirthSeconds = Particle.BirthSeconds;
		View.AgeSeconds = FMath::Max(0.0, ElapsedSeconds - Particle.BirthSeconds);
		View.LifetimeSeconds = Particle.LifetimeSeconds;
		View.LocalPosition = Particle.SpawnPosition + Config.LocalVelocity * View.AgeSeconds;
		View.CurrentSize = Particle.StartSize * SampleSize(Config.SizeKeys, View.AgeSeconds / View.LifetimeSeconds);
		View.CurrentColor = Particle.StartColor;
		View.CurrentColor.A *= static_cast<float>(SampleAlpha(Config.AlphaKeys, View.AgeSeconds / View.LifetimeSeconds));
		View.InitialRotationRadians = Config.InitialRotationRadians;
		OutParticles.Add(View);
	}
}
