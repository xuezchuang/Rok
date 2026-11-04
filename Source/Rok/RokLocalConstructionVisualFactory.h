#pragma once

#include "CoreMinimal.h"

class ARokCitySpriteActor;
class UWorld;
struct FRokCityPlacementWorldFrame;
struct FRokLocalConstructionPreview;
struct FRokLocalConstructionRequest;
struct FRokLocalConstructionTicket;

/** Pinned Unity tight Sprite.mesh of one locally constructible building (civilization 101, age 1, 5x5). */
struct FRokLocalSourceBuildingVisual
{
	int32 TypeId = 0;
	const TCHAR* ModelId = nullptr;
	const TCHAR* MeshPath = nullptr;
	const TCHAR* MaterialPath = nullptr;
	/** Unity local position and uniform scale of the prefab's main sprite child. */
	FVector ChildPosition = FVector::ZeroVector;
	double ChildScale = 0.0;
	/** UE LOD0 counts and local bounds pinned by the presentation-asset verify report. */
	int32 Vertices = 0;
	int32 Triangles = 0;
	FVector BoundsLo = FVector::ZeroVector;
	FVector BoundsHi = FVector::ZeroVector;
	/** Farm/Wood register a production clock on completion; other buildings only add power. */
	bool bProducesResource = false;
};

/** Source-backed building body only; scaffold and effect presentation are separate. */
class ROK_API FRokLocalConstructionVisualFactory
{
public:
	/** Null unless the type has verified source assets. */
	static const FRokLocalSourceBuildingVisual* FindSourceVisual(int32 TypeId);

	static bool Preflight(UWorld* World, const FRokCityPlacementWorldFrame& Frame,
		const FRokLocalConstructionRequest& Request, const FRokLocalConstructionPreview& Preview,
		FString& OutError);

	/** Hidden, nonselectable actor. The caller destroys it if the ledger rejects TryBegin. */
	static bool Prepare(UWorld* World, const FRokCityPlacementWorldFrame& Frame,
		const FRokLocalConstructionRequest& Request, const FRokLocalConstructionPreview& Preview,
		ARokCitySpriteActor*& OutHiddenActor, FString& OutError);

	/** After TryBegin only: no loading, validation, callback or failable work. */
	static void ActivatePending(ARokCitySpriteActor& Actor, const FRokLocalConstructionTicket& Ticket);

	/** Strict read-only identity check before the subsystem registers a completed actor. */
	static bool ValidateHeldActor(const ARokCitySpriteActor& Actor,
		const FRokLocalConstructionTicket& Ticket, FString& OutError);

	/** After successful subsystem registration only. */
	static void MarkCompletedVisual(ARokCitySpriteActor& Actor,
		const FRokLocalConstructionTicket& Ticket);
};
