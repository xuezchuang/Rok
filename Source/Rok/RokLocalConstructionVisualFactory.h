#pragma once

#include "CoreMinimal.h"

class ARokCitySpriteActor;
class UWorld;
struct FRokCityPlacementWorldFrame;
struct FRokLocalConstructionPreview;
struct FRokLocalConstructionRequest;
struct FRokLocalConstructionTicket;

/** Source-backed Farm/Wood body only; scaffold and effect presentation are separate. */
class ROK_API FRokLocalConstructionVisualFactory
{
public:
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
