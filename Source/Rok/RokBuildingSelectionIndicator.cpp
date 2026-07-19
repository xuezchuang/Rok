#include "RokBuildingSelectionIndicator.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "RokCitySpriteActor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float UnityUnitsToUnrealUnits = 500.0f;
constexpr float UnityArrowOffsetPerCell = 0.05f;
constexpr float ArrowOffsetPerCell = UnityUnitsToUnrealUnits * UnityArrowOffsetPerCell;
constexpr float ArrowPlaneSize = 64.0f;
constexpr float ArrowGroundClearance = 1.0f;
constexpr int32 UnityArrowRenderQueue = 5000;

// CityBuildingContainer and building_move_arrow are rendered at Unity yaw -45.
// After the project's handedness conversion (Unity x/y/z -> UE -x/z/y), these
// are the two city-grid axes on the UE ground plane.
const FVector UnityPositiveZInUnreal(UE_INV_SQRT_2, UE_INV_SQRT_2, 0.0f);
const FVector UnityPositiveXInUnreal(-UE_INV_SQRT_2, UE_INV_SQRT_2, 0.0f);

void ConfigureArrowComponent(UStaticMeshComponent* Component, UStaticMesh* PlaneMesh)
{
	if (!Component)
	{
		return;
	}

	Component->SetStaticMesh(PlaneMesh);
	// Unity rotates each SpriteRenderer +90 degrees around X before the prefab's
	// -45 degree city-grid yaw is applied. On UE's Engine Plane, image up is
	// plane -Y and image right is plane +X; yaw 135 maps those axes to the
	// converted Unity +Z and +X city-grid directions respectively.
	Component->SetRelativeRotation(FRotator(0.0f, 135.0f, 0.0f));
	Component->SetRelativeScale3D(FVector(ArrowPlaneSize / 100.0f, ArrowPlaneSize / 100.0f, 1.0f));
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetGenerateOverlapEvents(false);
	Component->SetRenderCustomDepth(false);
	Component->SetCastShadow(false);
	Component->bReceivesDecals = false;
	Component->bUseAsOccluder = false;
	Component->SetCanEverAffectNavigation(false);
	Component->TranslucencySortPriority = UnityArrowRenderQueue;
}
}

ARokBuildingSelectionIndicator::ARokBuildingSelectionIndicator()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	UpArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionArrowUp"));
	RightArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionArrowRight"));
	DownArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionArrowDown"));
	LeftArrowComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionArrowLeft"));

	UpArrowComponent->SetupAttachment(SceneRoot);
	RightArrowComponent->SetupAttachment(SceneRoot);
	DownArrowComponent->SetupAttachment(SceneRoot);
	LeftArrowComponent->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	ConfigureArrowComponent(UpArrowComponent, PlaneMesh.Object);
	ConfigureArrowComponent(RightArrowComponent, PlaneMesh.Object);
	ConfigureArrowComponent(DownArrowComponent, PlaneMesh.Object);
	ConfigureArrowComponent(LeftArrowComponent, PlaneMesh.Object);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UpMaterial(
		TEXT("/Game/RokPrototype/Materials/RuntimeCity/M_RuntimeCitySprite_RokBuildingSelectionArrowUp_SoftAlpha.M_RuntimeCitySprite_RokBuildingSelectionArrowUp_SoftAlpha"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RightMaterial(
		TEXT("/Game/RokPrototype/Materials/RuntimeCity/M_RuntimeCitySprite_RokBuildingSelectionArrowRight_SoftAlpha.M_RuntimeCitySprite_RokBuildingSelectionArrowRight_SoftAlpha"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DownMaterial(
		TEXT("/Game/RokPrototype/Materials/RuntimeCity/M_RuntimeCitySprite_RokBuildingSelectionArrowDown_SoftAlpha.M_RuntimeCitySprite_RokBuildingSelectionArrowDown_SoftAlpha"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeftMaterial(
		TEXT("/Game/RokPrototype/Materials/RuntimeCity/M_RuntimeCitySprite_RokBuildingSelectionArrowLeft_SoftAlpha.M_RuntimeCitySprite_RokBuildingSelectionArrowLeft_SoftAlpha"));

	UpArrowComponent->SetMaterial(0, UpMaterial.Object);
	RightArrowComponent->SetMaterial(0, RightMaterial.Object);
	DownArrowComponent->SetMaterial(0, DownMaterial.Object);
	LeftArrowComponent->SetMaterial(0, LeftMaterial.Object);

	SetActorHiddenInGame(true);
}

void ARokBuildingSelectionIndicator::ShowForBuilding(const ARokCitySpriteActor* BuildingActor)
{
	if (!BuildingActor || !BuildingActor->CanShowMoveArrows())
	{
		HideIndicator();
		return;
	}

	const FIntPoint FootprintSize = BuildingActor->GetFootprintSize();
	if (FootprintSize.X <= 0 || FootprintSize.Y <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok selection arrows: %s has no Unity footprint data."), *BuildingActor->GetName());
		HideIndicator();
		return;
	}

	SetActorLocation(BuildingActor->GetVisualAnchorWorldPosition() + FVector(0.0f, 0.0f, ArrowGroundClearance));
	UpArrowComponent->SetRelativeLocation(CalculateArrowOffset(FootprintSize, ERokBuildingSelectionArrowDirection::Up));
	RightArrowComponent->SetRelativeLocation(CalculateArrowOffset(FootprintSize, ERokBuildingSelectionArrowDirection::Right));
	DownArrowComponent->SetRelativeLocation(CalculateArrowOffset(FootprintSize, ERokBuildingSelectionArrowDirection::Down));
	LeftArrowComponent->SetRelativeLocation(CalculateArrowOffset(FootprintSize, ERokBuildingSelectionArrowDirection::Left));
	SetActorHiddenInGame(false);
}

void ARokBuildingSelectionIndicator::HideIndicator()
{
	SetActorHiddenInGame(true);
}

FVector ARokBuildingSelectionIndicator::CalculateArrowOffset(
	FIntPoint FootprintSize,
	ERokBuildingSelectionArrowDirection Direction)
{
	const float ForwardDistance = (FMath::Max(1, FootprintSize.X) + 1) * ArrowOffsetPerCell;
	const float RightDistance = (FMath::Max(1, FootprintSize.Y) + 1) * ArrowOffsetPerCell;

	switch (Direction)
	{
	case ERokBuildingSelectionArrowDirection::Up:
		return UnityPositiveZInUnreal * ForwardDistance;
	case ERokBuildingSelectionArrowDirection::Right:
		return UnityPositiveXInUnreal * RightDistance;
	case ERokBuildingSelectionArrowDirection::Down:
		return -UnityPositiveZInUnreal * ForwardDistance;
	case ERokBuildingSelectionArrowDirection::Left:
		return -UnityPositiveXInUnreal * RightDistance;
	default:
		return FVector::ZeroVector;
	}
}
