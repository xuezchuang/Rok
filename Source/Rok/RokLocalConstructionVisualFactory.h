#pragma once

#include "CoreMinimal.h"

class ARokCitySpriteActor;
class UWorld;
class UStaticMesh;
class UMaterialInterface;
struct FRokCityPlacementWorldFrame;
struct FRokLocalConstructionPreview;
struct FRokLocalConstructionRequest;
struct FRokLocalConstructionTicket;

/** Pinned Unity tight Sprite.mesh for a city building model. */
struct FRokLocalSourceBuildingVisual
{
	int32 TypeId = 0;
	const TCHAR* ModelId = nullptr;
	const TCHAR* MeshPath = nullptr;
	const TCHAR* MaterialPath = nullptr;
	/** Unity local position and horizontal scale of the prefab's main sprite child. */
	FVector ChildPosition = FVector::ZeroVector;
	double ChildScale = 0.0;
	/** UE LOD0 counts and local bounds pinned by the presentation-asset verify report. */
	int32 Vertices = 0;
	int32 Triangles = 0;
	FVector BoundsLo = FVector::ZeroVector;
	FVector BoundsHi = FVector::ZeroVector;
	/** Farm/Wood register a production clock on completion; other buildings only add power. */
	bool bProducesResource = false;
	/** True for models supported by the isolated age-one local economy. */
	bool bLocalConstruction = true;
	/** The academy asset binds its source material at the component. */
	const TCHAR* MeshDefaultMaterialPath = nullptr;
	/** Optional Unity Y scale for source sprites with nonuniform scale. */
	double ChildVerticalScale = 0.0;
	/** Optional Unity Z scale of the planar sprite normal. */
	double ChildNormalScale = 0.0;
};

/** Source-backed building body only; scaffold and effect presentation are separate. */
class ROK_API FRokLocalConstructionVisualFactory
{
public:
	/** Null unless the type has verified source assets and is locally constructible. */
	static const FRokLocalSourceBuildingVisual* FindSourceVisual(int32 TypeId);

	/** Any verified source visual, including display-only rows used for server buildings. */
	static const FRokLocalSourceBuildingVisual* FindBuildingVisual(int32 TypeId, const FString& ModelId = FString());
	static FTransform GetSourceChildTransform(const FRokLocalSourceBuildingVisual& Visual);
	static bool LoadVerifiedSourceMesh(const FRokLocalSourceBuildingVisual& Visual,
		UStaticMesh*& OutMesh, UMaterialInterface*& OutMaterial, FString& OutError);

	static bool ApplyInitialBuildingVisual(ARokCitySpriteActor& Actor, const FRokCityPlacementWorldFrame& Frame,
		int32 TypeId, const FString& ModelId, FString& OutError);

	/**
	 * Visible, selectable finished building at a server grid footprint (Build_BuildingInfo).
	 * No ledger or ticket: the server owns the building. Null with OutError when the type
	 * has no verified source visual.
	 */
	static ARokCitySpriteActor* SpawnServerBuilding(UWorld* World, const FRokCityPlacementWorldFrame& Frame,
		const FRokLocalSourceBuildingVisual& Visual, const FIntPoint& Origin, const FIntPoint& Footprint, FString& OutError);

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
