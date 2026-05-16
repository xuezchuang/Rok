#include "RokBuildingActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARokBuildingActor::ARokBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Tags.AddUnique(FName(TEXT("RokBuilding")));
	Tags.AddUnique(FName(TEXT("CityBuilding")));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(SceneRoot);

	CardMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardMeshComponent"));
	CardMeshComponent->SetupAttachment(VisualRoot);

	FootprintCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("FootprintCollision"));
	FootprintCollision->SetupAttachment(SceneRoot);

	FootprintHighlightComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FootprintHighlightComponent"));
	FootprintHighlightComponent->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		CardMeshComponent->SetStaticMesh(PlaneMesh.Object);
		FootprintHighlightComponent->SetStaticMesh(PlaneMesh.Object);
	}

	CardMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CardMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CardMeshComponent->SetGenerateOverlapEvents(false);
	CardMeshComponent->SetRenderCustomDepth(false);
	CardMeshComponent->CastShadow = false;
	CardMeshComponent->bReceivesDecals = false;
	CardMeshComponent->bUseAsOccluder = false;
	CardMeshComponent->SetCanEverAffectNavigation(false);

	FootprintCollision->SetBoxExtent(FVector(110.0f, 110.0f, 32.0f));
	FootprintCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
	FootprintCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FootprintCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	FootprintCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	FootprintCollision->SetGenerateOverlapEvents(false);
	FootprintCollision->CanCharacterStepUpOn = ECB_No;
	FootprintCollision->SetCanEverAffectNavigation(false);

	FootprintHighlightComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	FootprintHighlightComponent->SetRelativeScale3D(FVector(2.2f, 2.2f, 1.0f));
	FootprintHighlightComponent->SetHiddenInGame(true);
	FootprintHighlightComponent->SetVisibility(false);
	FootprintHighlightComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FootprintHighlightComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	FootprintHighlightComponent->SetGenerateOverlapEvents(false);
	FootprintHighlightComponent->SetRenderCustomDepth(false);
	FootprintHighlightComponent->CastShadow = false;
	FootprintHighlightComponent->bReceivesDecals = false;
	FootprintHighlightComponent->bUseAsOccluder = false;
	FootprintHighlightComponent->SetCanEverAffectNavigation(false);
}

void ARokBuildingActor::ConfigureCard(UMaterialInterface* Material, float Width, float Height, FVector InPivotOffset, FRotator InCardRotation)
{
	BuildingPivotOffset = InPivotOffset;
	CardSize = FVector2D(FMath::Max(1.0f, Width), FMath::Max(1.0f, Height));

	VisualRoot->SetRelativeLocation(BuildingPivotOffset);
	VisualRoot->SetRelativeRotation(InCardRotation);

	if (Material)
	{
		CardMeshComponent->SetMaterial(0, Material);
	}

	CardMeshComponent->SetRelativeLocation(FVector::ZeroVector);
	CardMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
	CardMeshComponent->SetRelativeScale3D(FVector(CardSize.X / 100.0f, CardSize.Y / 100.0f, 1.0f));
	CardMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CardMeshComponent->SetRenderCustomDepth(false);
}

void ARokBuildingActor::ConfigureFootprint(float Width, float Length, UMaterialInterface* HighlightMaterial, float YawDegrees)
{
	FootprintSize = FVector2D(FMath::Max(1.0f, Width), FMath::Max(1.0f, Length));
	FootprintCollision->SetBoxExtent(FVector(FootprintSize.X * 0.5f, FootprintSize.Y * 0.5f, 32.0f));
	FootprintCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
	FootprintCollision->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));

	FootprintHighlightComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	FootprintHighlightComponent->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));
	FootprintHighlightComponent->SetRelativeScale3D(FVector(FootprintSize.X / 100.0f, FootprintSize.Y / 100.0f, 1.0f));
	if (HighlightMaterial)
	{
		FootprintHighlightComponent->SetMaterial(0, HighlightMaterial);
	}
	SetSelected(bSelected);
}

void ARokBuildingActor::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	FootprintHighlightComponent->SetVisibility(bSelected);
	FootprintHighlightComponent->SetHiddenInGame(!bSelected);
	CardMeshComponent->SetRenderCustomDepth(false);
	FootprintHighlightComponent->SetRenderCustomDepth(false);
}
