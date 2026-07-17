#include "RokCitySpriteActor.h"

#include "Components/MaterialBillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARokCitySpriteActor::ARokCitySpriteActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SpriteMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CitySpritePlane"));
	SpriteMeshComponent->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		SpriteMeshComponent->SetStaticMesh(PlaneMesh.Object);
	}

	SpriteMeshComponent->SetHiddenInGame(false);
	SpriteMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SpriteMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpriteMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SpriteMeshComponent->CastShadow = false;
	SpriteMeshComponent->bReceivesDecals = false;
	SpriteMeshComponent->bUseAsOccluder = false;
	SpriteMeshComponent->SetCanEverAffectNavigation(false);

	// Kept for older editor scripts that query the billboard component type.
	SpriteComponent = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("LegacyCitySpriteBillboard"));
	SpriteComponent->SetupAttachment(SceneRoot);
	SpriteComponent->SetHiddenInGame(true);
	SpriteComponent->SetVisibility(false);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteComponent->CastShadow = false;
	SpriteComponent->bReceivesDecals = false;
}

void ARokCitySpriteActor::ConfigureSprite(UMaterialInterface* Material, float Width, float Height, int32 SortPriority)
{
	if (!Material)
	{
		return;
	}

	ViewSortPriority = SortPriority;
	if (SpriteMeshComponent)
	{
		SpriteMeshComponent->SetMaterial(0, Material);
		SpriteMeshComponent->SetRelativeScale3D(FVector(Width / 100.0f, FMath::Abs(Height / 100.0f), 1.0f));
		SpriteMeshComponent->TranslucencySortPriority = SortPriority;
	}

	if (SpriteComponent)
	{
		TArray<FMaterialSpriteElement> EmptyElements;
		SpriteComponent->SetElements(EmptyElements);
		SpriteComponent->SetHiddenInGame(true);
		SpriteComponent->SetVisibility(false);
		SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ARokCitySpriteActor::ConfigureViewLayer(ERokCityViewLayer InViewLayer, int32 InSortPriority)
{
	ViewLayer = InViewLayer;
	ViewSortPriority = InSortPriority;

	if (SpriteMeshComponent)
	{
		SpriteMeshComponent->TranslucencySortPriority = InSortPriority;
	}
}

void ARokCitySpriteActor::ConfigurePlacement(FIntPoint InFootprintSize, FIntPoint InGameplayTile, FVector InVisualAnchorOffset)
{
	FootprintSize = InFootprintSize;
	GameplayTile = InGameplayTile;
	VisualAnchorOffset = InVisualAnchorOffset;
}

void ARokCitySpriteActor::SetSelectionCollisionEnabled(bool bEnabled)
{
	if (!SpriteMeshComponent)
	{
		return;
	}

	SpriteMeshComponent->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	SpriteMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpriteMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, bEnabled ? ECR_Block : ECR_Ignore);
}

FVector ARokCitySpriteActor::GetVisualAnchorWorldPosition() const
{
	// Placement scripts store this as a world-space offset from the center of the
	// camera-facing sprite card to the Unity building/grid anchor on the ground.
	return GetActorLocation() + VisualAnchorOffset;
}

FVector ARokCitySpriteActor::GetVisualCenterWorldPosition() const
{
	if (!SpriteMeshComponent)
	{
		return GetActorLocation();
	}

	return SpriteMeshComponent->Bounds.Origin;
}

bool ARokCitySpriteActor::IsDecoration() const
{
	for (const FName& Tag : Tags)
	{
		FString TagText = Tag.ToString();
		if (!TagText.RemoveFromStart(TEXT("UnityBuildingType_")))
		{
			continue;
		}

		const int32 UnityBuildingType = FCString::Atoi(*TagText);
		return UnityBuildingType >= 51 && UnityBuildingType <= 54;
	}

	return false;
}
