#include "RokCitySpriteActor.h"

#include "RokStrategyCameraPawn.h"

#include "Components/MaterialBillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARokCitySpriteActor::ARokCitySpriteActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

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

void ARokCitySpriteActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bUseUnityCameraLod)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const ARokStrategyCameraPawn* CameraPawn = PlayerController ? Cast<ARokStrategyCameraPawn>(PlayerController->GetPawn()) : nullptr;
	if (CameraPawn)
	{
		ApplyUnityLodScale(EvaluateUnityCityLodScale(CameraPawn->GetCurrentCityDxf()));
	}
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

void ARokCitySpriteActor::ConfigureUnityCameraLod(FVector InBaseVisualCenterOffset, float InitialDxf)
{
	if (!SpriteMeshComponent)
	{
		return;
	}

	UnityBaseVisualCenterOffset = InBaseVisualCenterOffset;
	UnityBaseSpriteScale = SpriteMeshComponent->GetRelativeScale3D();
	bUseUnityCameraLod = true;
	SetActorTickEnabled(true);
	ApplyUnityLodScale(EvaluateUnityCityLodScale(InitialDxf));
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

void ARokCitySpriteActor::SetSelected(bool bSelected)
{
	if (!SpriteMeshComponent || bSelectionHighlighted == bSelected)
	{
		return;
	}

	if (bUseUnityCameraLod)
	{
		bSelectionHighlighted = bSelected;
		ApplyUnityLodScale(CurrentUnityLodScale);
	}
	else if (bSelected)
	{
		SelectionOriginalSpriteScale = SpriteMeshComponent->GetRelativeScale3D();
		SpriteMeshComponent->SetRelativeScale3D(SelectionOriginalSpriteScale * 1.035f);
	}
	else
	{
		SpriteMeshComponent->SetRelativeScale3D(SelectionOriginalSpriteScale);
	}

	bSelectionHighlighted = bSelected;
	SpriteMeshComponent->SetRenderCustomDepth(bSelected);
	SpriteMeshComponent->SetCustomDepthStencilValue(bSelected ? 2 : 0);
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

FVector ARokCitySpriteActor::GetVisualBoundsExtent() const
{
	return SpriteMeshComponent ? SpriteMeshComponent->Bounds.BoxExtent : FVector::ZeroVector;
}

FIntPoint ARokCitySpriteActor::GetFootprintSize() const
{
	return FootprintSize;
}

FVector ARokCitySpriteActor::GetUnityBaseVisualCenterOffset() const
{
	return UnityBaseVisualCenterOffset;
}

float ARokCitySpriteActor::GetUnityLodScale() const
{
	return CurrentUnityLodScale;
}

float ARokCitySpriteActor::EvaluateUnityCityLodScale(float Dxf)
{
	// Source: CityBuildingContainer.prefab LevelDetailCastle.m_scale_curve.
	// The active city camera range stays below the curve's 3000 discontinuity.
	if (!FMath::IsFinite(Dxf))
	{
		return 1.0f;
	}

	float CurveValue = 1.0f;
	if (Dxf > 250.0f && Dxf < 1500.0f)
	{
		CurveValue = 1.0f + (Dxf - 250.0f) * 0.0056f;
	}
	else if (Dxf >= 1500.0f && Dxf < 3000.0f)
	{
		CurveValue = 8.0f;
	}
	else if (Dxf >= 3000.0f)
	{
		CurveValue = 0.0f;
	}

	return FMath::Clamp(CurveValue, 0.0f, 7.8f);
}

void ARokCitySpriteActor::ApplyUnityLodScale(float LodScale)
{
	if (!SpriteMeshComponent)
	{
		return;
	}

	CurrentUnityLodScale = LodScale;
	const float SelectionScale = bSelectionHighlighted ? 1.035f : 1.0f;
	SpriteMeshComponent->SetRelativeScale3D(UnityBaseSpriteScale * LodScale * SelectionScale);
	SpriteMeshComponent->SetWorldLocation(GetVisualAnchorWorldPosition() + UnityBaseVisualCenterOffset * LodScale);
}

int32 ARokCitySpriteActor::GetUnityBuildingType() const
{
	for (const FName& Tag : Tags)
	{
		FString TagText = Tag.ToString();
		if (TagText.RemoveFromStart(TEXT("UnityBuildingType_")))
		{
			return FCString::Atoi(*TagText);
		}
	}

	return INDEX_NONE;
}

bool ARokCitySpriteActor::CanShowMoveArrows() const
{
	const int32 UnityBuildingType = GetUnityBuildingType();
	return UnityBuildingType != INDEX_NONE
		&& UnityBuildingType != 6 // CityWall
		&& UnityBuildingType != 8 // GuardTower
		// Unity IsSelectBuildCanMove also rejects a selected building whose
		// transform has been enlarged beyond its normal root scale.
		&& GetActorScale3D().X <= 1.0f + KINDA_SMALL_NUMBER;
}

bool ARokCitySpriteActor::IsDecoration() const
{
	const int32 UnityBuildingType = GetUnityBuildingType();
	return UnityBuildingType >= 51 && UnityBuildingType <= 54;
}
