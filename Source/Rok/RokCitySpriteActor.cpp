#include "RokCitySpriteActor.h"

#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInterface.h"

ARokCitySpriteActor::ARokCitySpriteActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SpriteComponent = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("CitySprite"));
	RootComponent = SpriteComponent;

	SpriteComponent->SetHiddenInGame(false);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SpriteComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpriteComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SpriteComponent->CastShadow = false;
	SpriteComponent->bReceivesDecals = false;
}

void ARokCitySpriteActor::ConfigureSprite(UMaterialInterface* Material, float Width, float Height, int32 SortPriority)
{
	if (!SpriteComponent || !Material)
	{
		return;
	}

	TArray<FMaterialSpriteElement> EmptyElements;
	SpriteComponent->SetElements(EmptyElements);
	SpriteComponent->AddElement(Material, nullptr, false, Width, Height, nullptr);
	SpriteComponent->TranslucencySortPriority = SortPriority;
}

void ARokCitySpriteActor::ConfigurePlacement(FIntPoint InFootprintSize, FIntPoint InGameplayTile, FVector InVisualAnchorOffset)
{
	FootprintSize = InFootprintSize;
	GameplayTile = InGameplayTile;
	VisualAnchorOffset = InVisualAnchorOffset;
}

void ARokCitySpriteActor::SetSelectionCollisionEnabled(bool bEnabled)
{
	if (!SpriteComponent)
	{
		return;
	}

	SpriteComponent->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	SpriteComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpriteComponent->SetCollisionResponseToChannel(ECC_Visibility, bEnabled ? ECR_Block : ECR_Ignore);
}
