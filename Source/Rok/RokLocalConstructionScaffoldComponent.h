#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RokLocalConstructionScaffoldComponent.generated.h"

class UStaticMesh;
class UTexture2D;
class USceneComponent;
class URokBuildingUpgradeSubsystem;
struct FRokLocalConstructionTicket;

// Positions and rotations in this API remain in Unity local space. The city
// parent transform, reflected axes, source-Gamma material and particles are
// intentionally outside this component's current presentation contract.
struct FRokScaffoldSourcePose
{
	FString Path;
	FString ParentPath;
	FVector3d Position = FVector3d::ZeroVector;
	FVector3d Scale = FVector3d::OneVector;
	FQuat4d Rotation = FQuat4d::Identity;
};

struct FRokScaffoldCurveKey
{
	double Time = 0.0;
	double Value = 0.0;
	double InTangent = 0.0;
	double OutTangent = 0.0;
};

struct FRokScaffoldCurve
{
	FString Property;
	TArray<FRokScaffoldCurveKey> Keys;
};

struct FRokScaffoldClip
{
	FString GroupPath;
	double LengthSeconds = 0.0;
	TArray<FRokScaffoldCurve> Curves;
};

struct FRokScaffoldLayerSource
{
	FString NodePath;
	FString MeshPath;
	FString SpriteTexturePath;
	FString MaskTexturePath;
	FIntPoint TextureSize = FIntPoint::ZeroValue;
	int32 SourceTriangles = 0;
	int32 ImportedTriangles = 0;
};

struct FRokScaffoldSourceBundle
{
	TArray<FRokScaffoldSourcePose> Nodes;
	TArray<FRokScaffoldLayerSource> Layers;
	TArray<FRokScaffoldClip> Clips;
};

struct FRokScaffoldSample
{
	uint64 TicketId = 0;
	double SourceElapsedSeconds = 0.0;
	bool bFourAnimationsEnabled = false;
	bool bParticleNodeActiveInSource = false;
	TArray<FRokScaffoldSourcePose> Nodes;
};

namespace RokLocalConstructionScaffold
{
	// Fail closed on the sealed four evidence packages, their cross-links and
	// every imported mesh/texture byte hash. This only reads files and assets.
	ROK_API bool LoadValidatedBundle(FRokScaffoldSourceBundle& Out, FString& Error);
	ROK_API double EvaluateCurve(const FRokScaffoldCurve& Curve, double ClipSeconds);
	ROK_API bool SampleBundle(const FRokScaffoldSourceBundle& Bundle, double ElapsedSeconds,
		FRokScaffoldSample& Out, FString& Error);
}

// Pure ticket gate shared by the component and automation tests. No ledger,
// wallet or world state is mutated by this type.
struct FRokScaffoldTicketLifecycle
{
	enum class EPhase : uint8 { Empty, Prepared, Playing, Stopped, Retired };
	EPhase Phase = EPhase::Empty;
	const UWorld* OwnerWorld = nullptr;
	uint64 TicketId = 0;
	double StartedAt = 0.0;
	double FinishesAt = 0.0;
	double LastSampledWorldSeconds = 0.0;

	bool Prepare(const UWorld* World, FString& Error);
	bool Begin(const UWorld* World, const FRokLocalConstructionTicket& Ticket, FString& Error);
	bool Sample(double WorldSeconds, double& OutElapsed, bool& bOutPlaying, FString& Error);
	bool Retire(const UWorld* World, uint64 Id, bool bExternallyRegistered, FString& Error);
	void Reset();
};

/** Hidden, non-colliding source hierarchy and ticket-clock animation sampler.
 * This does not install a fallback material or show a sprite card. */
UCLASS(ClassGroup=(Rok), meta=(BlueprintSpawnableComponent))
class ROK_API URokLocalConstructionScaffoldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URokLocalConstructionScaffoldComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Call on a hidden, pre-debit actor. A failure removes every node created by
	// this component and leaves the owner's existing building body untouched.
	bool PrepareHiddenSource(USceneComponent* ShelfParent, FString& Error);
	bool BeginForCommittedTicket(const FRokLocalConstructionTicket& Ticket, FString& Error);
	bool SampleAtWorldSeconds(double WorldSeconds, FRokScaffoldSample& Out, FString& Error);
	// The subsystem must already have registered the completed owner actor.
	bool RetireAfterRegisteredCompletion(URokBuildingUpgradeSubsystem* Buildings,
		uint64 TicketId, FString& Error);
	void CancelPrepared();
	const FRokScaffoldSample& GetSourceSnapshot() const { return LastSample; }
	const FRokScaffoldTicketLifecycle& GetTicketLifecycle() const { return Lifecycle; }
	int32 GetOwnedSourceNodeCount() const { return OwnedNodes.Num(); }
	int32 GetLoadedSourceMeshCount() const { return SourceMeshes.Num(); }
	int32 GetLoadedSourceTextureCount() const { return SourceTextures.Num(); }
	bool AreOwnedSourceNodesHidden() const;
	bool IsSourcePresentationVisible() const { return false; }

private:
	void ReleaseOwnedNodes();

	FRokScaffoldSourceBundle Bundle;
	FRokScaffoldSample LastSample;
	FRokScaffoldTicketLifecycle Lifecycle;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> OwnedNodes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> SourceMeshes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> SourceTextures;
};
