#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokUnityTileRippleSimulation.h"
#include "RokUnityTileRippleRenderer.generated.h"

class UProceduralMeshComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

/** Source-local renderer matrix from the sealed V5 BakeMesh probe. */
struct FRokTileRippleEmitterGeometry
{
	FString SourceName;
	int64 SourceFileId = 0;
	TArray<double> RowMajor4x4;
	FVector3d SourceOrigin = FVector3d::ZeroVector;
};

struct FRokTileRippleGeometryContract
{
	FVector3d SourceAnchor = FVector3d::ZeroVector;
	double SourceUnitsToUe = 100.0;
	FRokTileRippleEmitterGeometry Emitters[2];

	static bool LoadPinned(const FString& AbsolutePath, FRokTileRippleGeometryContract& Out, FString& Error);
};

/** Dynamic geometry, never a Unity frame replay or a BasicShapes card. */
struct FRokTileRippleMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FColor> Colors;

	static FVector3d SourceCenter(const FRokTileRippleEmitterGeometry& Emitter, const FVector3d& LocalParticle);
	static bool Build(const FRokTileRippleEmitterGeometry& Emitter, const FVector3d& Anchor,
		double UnitsToUe, const TArray<FRokTileRippleParticleSnapshot>& Particles,
		FRokTileRippleMeshData& Out, FString& Error);
};

/** Opt-in isolated world-map diagnostic. A caller places this actor at the intended tile anchor. */
UCLASS(NotBlueprintable)
class ROK_API ARokUnityTileRippleDiagnosticActor : public AActor
{
	GENERATED_BODY()
public:
	ARokUnityTileRippleDiagnosticActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	int32 GetLiveParticleCount(int32 EmitterIndex) const;
	double GetSimElapsedSeconds() const;
	UProceduralMeshComponent* GetEmitterMesh(int32 EmitterIndex) const;
	UTexture2D* GetSourceTexture() const { return SourceTexture; }
	bool IsSourceRendererReady() const { return bReady; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Bowen;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Bowen1;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SourceTexture;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BowenMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Bowen1Material;
	FRokTileRippleGeometryContract Geometry;
	TUniquePtr<FRokUnityTileRippleSimulation> Simulations[2];
	bool bReady = false;
	void UpdateEmitter(int32 Index);
};
