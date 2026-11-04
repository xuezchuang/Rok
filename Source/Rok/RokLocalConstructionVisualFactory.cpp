#include "RokLocalConstructionVisualFactory.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokCitySpriteActor.h"
#include "RokLocalCitySimulationContext.h"
#include "RokStrategyCameraPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

namespace
{
// Farm/Wood: CityResourcePresentationAssets/ue_assets_verify.json.
// Commander hall: CityBuildingPresentationAssets/ue_assets_create.json (mesh_topology).
// Child poses are the prefab main sprite child (float values widened to double).
const FRokLocalSourceBuildingVisual SourceVisuals[] = {
	{2, TEXT("build_2_101_1"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/SM_build_2_101_1_SourceSpriteMesh.SM_build_2_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/M_build_2_101_1_SourceRG.M_build_2_101_1_SourceRG"),
		FVector(0.0430000014603138, 0.05299999937415123, 0.03099999949336052), 0.13229629397392273,
		133, 163, FVector(-234.9999847412, 0.0, -222.0), FVector(243.0, 0.0, 240.0000152588), true},
	{3, TEXT("build_3_101_1"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/SM_build_3_101_1_SourceSpriteMesh.SM_build_3_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/M_build_3_101_1_SourceRG.M_build_3_101_1_SourceRG"),
		FVector(0.003000000026077032, 0.04100000113248825, 0.0560000017285347), 0.1359577178955078,
		43, 41, FVector(-236.9999847412, 0.0, -190.5), FVector(256.0, 0.0, 208.5), true},
	{18, TEXT("build_18_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_18_101_1/SM_build_18_101_1_SourceSpriteMesh.SM_build_18_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_18_101_1/M_build_18_101_1_SourceRG.M_build_18_101_1_SourceRG"),
		FVector(0.020999999716877937, 0.03700000047683716, 0.03200000151991844), 0.1124904677271843,
		47, 46, FVector(-256.0, 0.0, -216.0), FVector(256.0, 0.0, 216.0), false},
};
constexpr TCHAR PendingTag[] = TEXT("RokLocalConstructionPending");
constexpr TCHAR SourceLevelZeroTag[] = TEXT("RokLocalSourceLevel_0");
constexpr TCHAR SourceLevelOneTag[] = TEXT("RokLocalSourceLevel_1");
constexpr double BoundsToleranceCm = 0.05;

struct FSourceVisual
{
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	FString ModelId;
	FTransform Child = FTransform::Identity;
	FTransform Actor = FTransform::Identity;
	FRokCityConstructionFootprint Footprint;
	float CityDxf = 0.0f;
};

FName IndexedTag(const TCHAR* Prefix, int64 Value)
{
	return FName(*FString::Printf(TEXT("%s%lld"), Prefix, static_cast<long long>(Value)));
}

FName TicketTag(uint64 Id)
{
	return FName(*FString::Printf(TEXT("RokLocalBuildingId_%llu"), static_cast<unsigned long long>(Id)));
}

FName SourceModelTag(const FString& ModelId)
{
	return FName(*FString::Printf(TEXT("RokLocalSourceModel_%s"), *ModelId));
}

FQuat UnityRotationToCity(const FQuat& Q)
{
	// Same reflected-axis conversion as RokCityPlacementPreviewActor.
	return FQuat(-Q.X, -Q.Z, -Q.Y, Q.W);
}

FTransform SourceChildTransform(const FRokLocalSourceBuildingVisual& Visual)
{
	const FVector& Position = Visual.ChildPosition;
	const double Scale = Visual.ChildScale;
	// Identical main sprite child rotation in every supported source prefab.
	const FQuat SourceRotation(0.3535533845424652, 0.3535533845424652,
		-0.1464466005563736, 0.8535534739494324);
	// Source OBJ uses 100 cm/unit; city container owns the 500 UU/unit scale.
	return FTransform(UnityRotationToCity(SourceRotation),
		FVector(Position.X, Position.Z, Position.Y),
		FVector(-Scale / 100.0, Scale / 100.0, Scale / 100.0));
}

bool MatchBounds(const UStaticMesh& Mesh, const FRokLocalSourceBuildingVisual& Visual)
{
	const FBoxSphereBounds Bounds = Mesh.GetBounds();
	const FVector Lo = Bounds.Origin - Bounds.BoxExtent;
	const FVector Hi = Bounds.Origin + Bounds.BoxExtent;
	return Lo.Equals(Visual.BoundsLo, BoundsToleranceCm)
		&& Hi.Equals(Visual.BoundsHi, BoundsToleranceCm);
}

bool ValidateSourceAsset(UStaticMesh* Mesh, UMaterialInterface* Material,
	const FRokLocalSourceBuildingVisual& Visual, FString& OutError)
{
	if (!IsValid(Mesh) || !IsValid(Material)
		|| Mesh->GetPathName() != Visual.MeshPath || Material->GetPathName() != Visual.MaterialPath
		|| Mesh->GetNumLODs() != 1 || Mesh->GetNumVertices(0) != Visual.Vertices
		|| Mesh->GetNumTriangles(0) != Visual.Triangles || Mesh->GetNumTexCoords(0) != 1
		|| Mesh->GetMaterial(0) != Material || !MatchBounds(*Mesh, Visual))
	{
		OutError = FString::Printf(TEXT("%s source mesh, RG material, LOD0 or pivot differs from verified assets"),
			Visual.ModelId);
		return false;
	}
	return true;
}

bool ResolveSource(UWorld* World, const FRokCityPlacementWorldFrame& Frame,
	const FRokLocalConstructionRequest& Request, const FRokLocalConstructionPreview& Preview,
	FSourceVisual& Out, FString& OutError)
{
	Out = FSourceVisual();
	OutError.Reset();
	const FRokLocalSourceBuildingVisual* Visual =
		FRokLocalConstructionVisualFactory::FindSourceVisual(Request.UnityBuildingTypeId);
	if (!IsValid(World) || !World->IsGameWorld() || !Frame.IsValid()
		|| !Preview.bContextVerified || !Preview.bSourceEligible || !Preview.bPositionEvaluated
		|| !Visual
		|| (Request.CivilizationId != 0 && Request.CivilizationId != 101)
		|| (Request.Age != 0 && Request.Age != 1)
		|| Preview.VisualCivilization != 101 || Preview.ResolvedAge != 1
		|| Preview.Footprint != FIntPoint(5, 5) || Preview.SourceCellIndices.Num() != 25)
	{
		OutError = TEXT("Only eligible source-visual 101 age-one 5x5 city construction is supported");
		return false;
	}
	Out.ModelId = Visual->ModelId;
	if (Preview.SourceModelId != Out.ModelId)
	{
		OutError = TEXT("Local construction source model ID differs from the verified visual variant");
		return false;
	}
	FRokLocalCitySimulationContext Context;
	FRokCityPlacementWorldFrame ResolvedFrame;
	if (!Context.Load(OutError) || !Context.ValidateWorld(World, OutError)
		|| Context.GetVisualCivilization() != 101 || Context.GetInitialAge() != 1
		|| !FRokCityPlacementWorldFrame::ResolveForWorld(World, ResolvedFrame, OutError)
		|| Frame.GetGridSize() != ResolvedFrame.GetGridSize()
		|| !Frame.GetCityContainerTransform().Equals(ResolvedFrame.GetCityContainerTransform(), 0.01))
	{
		if (OutError.IsEmpty()) OutError = TEXT("City source frame or world identity differs");
		return false;
	}
	Out.Footprint = {Request.Grid, Preview.Footprint};
	FVector CenterWorld;
	if (!Frame.GridCenterToWorld(Out.Footprint, CenterWorld, OutError))
	{
		return false;
	}
	Out.Actor = Frame.GetCityContainerTransform();
	Out.Actor.SetLocation(CenterWorld);
	Out.Child = SourceChildTransform(*Visual);
	if (!Out.Actor.IsValid() || !Out.Child.IsValid())
	{
		OutError = TEXT("Source city root or child pose is invalid");
		return false;
	}
	const APlayerController* Controller = World->GetFirstPlayerController();
	const ARokStrategyCameraPawn* Camera = Controller
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	Out.CityDxf = Camera ? Camera->GetCurrentCityDxf() : 0.0f;
	if (!FMath::IsFinite(Out.CityDxf) || Out.CityDxf <= 0.0f || Out.CityDxf >= 3000.0f)
	{
		OutError = TEXT("Verified city camera LOD state is unavailable");
		return false;
	}
	Out.Mesh = Cast<UStaticMesh>(FSoftObjectPath(Visual->MeshPath).TryLoad());
	Out.Material = Cast<UMaterialInterface>(FSoftObjectPath(Visual->MaterialPath).TryLoad());
	return ValidateSourceAsset(Out.Mesh, Out.Material, *Visual, OutError);
}

bool HasForbiddenIdentity(const ARokCitySpriteActor& Actor)
{
	for (const FName& Tag : Actor.Tags)
	{
		const FString Value = Tag.ToString();
		if (Value.StartsWith(TEXT("UnityInitId_")) || Value == TEXT("RokUnityConfigInitBuilding")
			|| Value == TEXT("RokPlacementPreviewTemporary"))
		{
			return true;
		}
	}
	return false;
}
}

const FRokLocalSourceBuildingVisual* FRokLocalConstructionVisualFactory::FindSourceVisual(int32 TypeId)
{
	for (const FRokLocalSourceBuildingVisual& Visual : SourceVisuals)
	{
		if (Visual.TypeId == TypeId) return &Visual;
	}
	return nullptr;
}

bool FRokLocalConstructionVisualFactory::Preflight(UWorld* World,
	const FRokCityPlacementWorldFrame& Frame,
	const FRokLocalConstructionRequest& Request,
	const FRokLocalConstructionPreview& Preview, FString& OutError)
{
	FSourceVisual Source;
	return ResolveSource(World, Frame, Request, Preview, Source, OutError);
}

bool FRokLocalConstructionVisualFactory::Prepare(UWorld* World,
	const FRokCityPlacementWorldFrame& Frame,
	const FRokLocalConstructionRequest& Request,
	const FRokLocalConstructionPreview& Preview,
	ARokCitySpriteActor*& OutHiddenActor, FString& OutError)
{
	OutHiddenActor = nullptr;
	FSourceVisual Source;
	if (!ResolveSource(World, Frame, Request, Preview, Source, OutError)) return false;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ARokCitySpriteActor* Actor = World->SpawnActor<ARokCitySpriteActor>(
		ARokCitySpriteActor::StaticClass(), Source.Actor, Params);
	if (!IsValid(Actor))
	{
		OutError = TEXT("Source city building actor could not be spawned");
		return false;
	}
	Actor->SetActorHiddenInGame(true);
	Actor->SetSelectionCollisionEnabled(false);
	Actor->ConfigureSourceMesh(Source.Mesh, Source.Material, Source.Child, 2995);
	Actor->ConfigurePlacement(Source.Footprint.Size, Source.Footprint.Origin, FVector::ZeroVector);
	Actor->ConfigureUnityCameraLod(FVector::ZeroVector, Source.CityDxf);
	UStaticMeshComponent* Component = Actor->GetSpriteMeshComponent();
	if (!Component || Component->GetStaticMesh().Get() != Source.Mesh
		|| Component->GetMaterial(0) != Source.Material
		|| Actor->GetGameplayTile() != Source.Footprint.Origin
		|| Actor->GetFootprintSize() != Source.Footprint.Size
		|| !Actor->GetActorTransform().Equals(Source.Actor, 0.01))
	{
		Actor->Destroy();
		OutError = TEXT("Prepared source actor did not preserve mesh, material, grid or city pose");
		return false;
	}
	Actor->Tags.AddUnique(SourceModelTag(Source.ModelId));
	Actor->Tags.AddUnique(FName(SourceLevelZeroTag));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityBuildingType_"), Request.UnityBuildingTypeId));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridX_"), Request.Grid.X));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridY_"), Request.Grid.Y));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridWidth_"), Source.Footprint.Size.X));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridLength_"), Source.Footprint.Size.Y));
	OutHiddenActor = Actor;
	OutError.Reset();
	return true;
}

void FRokLocalConstructionVisualFactory::ActivatePending(
	ARokCitySpriteActor& Actor, const FRokLocalConstructionTicket& Ticket)
{
	Actor.Tags.AddUnique(TicketTag(Ticket.Id));
	Actor.Tags.AddUnique(FName(PendingTag));
	Actor.SetActorHiddenInGame(false);
}

bool FRokLocalConstructionVisualFactory::ValidateHeldActor(const ARokCitySpriteActor& Actor,
	const FRokLocalConstructionTicket& Ticket, FString& OutError)
{
	OutError.Reset();
	const int32 TypeId = Ticket.TypeId;
	const FRokLocalSourceBuildingVisual* Visual = FindSourceVisual(TypeId);
	const UStaticMeshComponent* Component = Actor.GetSpriteMeshComponent();
	if (!Visual || Ticket.Civilization != 101 || Ticket.Age != 1
		|| Ticket.Id == 0 || Ticket.Footprint.Size != FIntPoint(5, 5)
		|| !Actor.Tags.Contains(FName(PendingTag)) || !Actor.Tags.Contains(TicketTag(Ticket.Id))
		|| !Actor.Tags.Contains(SourceModelTag(Visual->ModelId))
		|| !Actor.Tags.Contains(FName(SourceLevelZeroTag)) || HasForbiddenIdentity(Actor)
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityBuildingType_"), TypeId))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridX_"), Ticket.Footprint.Origin.X))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridY_"), Ticket.Footprint.Origin.Y))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridWidth_"), Ticket.Footprint.Size.X))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridLength_"), Ticket.Footprint.Size.Y))
		|| Actor.GetGameplayTile() != Ticket.Footprint.Origin
		|| Actor.GetFootprintSize() != Ticket.Footprint.Size || Actor.IsHidden()
		|| !Component || Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision
		|| !ValidateSourceAsset(Component ? Component->GetStaticMesh().Get() : nullptr,
			Component ? Component->GetMaterial(0) : nullptr, *Visual, OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("Pending local actor source identity or footprint differs");
		return false;
	}
	FRokCityPlacementWorldFrame Frame;
	FVector CenterWorld;
	if (!FRokCityPlacementWorldFrame::ResolveForWorld(Actor.GetWorld(), Frame, OutError)
		|| !Frame.GridCenterToWorld(Ticket.Footprint, CenterWorld, OutError))
	{
		return false;
	}
	FTransform ExpectedActor = Frame.GetCityContainerTransform();
	ExpectedActor.SetLocation(CenterWorld);
	const FTransform Child = SourceChildTransform(*Visual);
	const double Lod = Actor.GetUnityLodScale();
	if (!FMath::IsFinite(Lod) || Lod <= 0.0 || Lod > 7.8
		|| !Actor.GetActorTransform().Equals(ExpectedActor, 0.01)
		|| !Component->GetRelativeRotation().Equals(Child.GetRotation().Rotator(), 0.01)
		|| !Component->GetRelativeLocation().Equals(Child.GetLocation() * Lod, 0.001)
		|| !Component->GetRelativeScale3D().Equals(Child.GetScale3D() * Lod, 0.00001))
	{
		OutError = TEXT("Pending local actor source pivot, child transform or LOD differs");
		return false;
	}
	return true;
}

void FRokLocalConstructionVisualFactory::MarkCompletedVisual(
	ARokCitySpriteActor& Actor, const FRokLocalConstructionTicket&)
{
	Actor.Tags.Remove(FName(PendingTag));
	Actor.Tags.Remove(FName(SourceLevelZeroTag));
	Actor.Tags.AddUnique(FName(SourceLevelOneTag));
	Actor.SetSelectionCollisionEnabled(true);
}
