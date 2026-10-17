#include "RokFoodDepositComponent.h"

#include "RokWorldDayNightSubsystem.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
FVector Convert(FVector P) { return FVector(-P.X, P.Z, P.Y); }
UTexture2D* LoadRawTexture(const FString& Path)
{
	static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
	if (const auto* Existing = Cache.Find(Path))
		if (Existing->IsValid()) return Existing->Get();
	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Path);
	if (Texture)
	{
		// Unity Gamma project: both MainTex and Mask are sampled as raw values.
		// The dedicated material converts the completed shader RGB to UE linear.
		Texture->SRGB = false;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->UpdateResource();
		Cache.Add(Path, Texture);
	}
	return Texture;
}
}

bool URokFoodDepositComponent::InitializeSource()
{
	if (!Data.Load())
	{
		UE_LOG(LogTemp, Error, TEXT("Food deposit source manifest missing or invalid"));
		return false;
	}
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokFoodDepositDay.M_RokFoodDepositDay"));
	if (!Plane || !Parent)
	{
		UE_LOG(LogTemp, Error, TEXT("Food deposit source material/plane missing; run the dedicated material importer"));
		return false;
	}
	for (const auto& Item : Data.Items)
	{
		UTexture2D* Texture = LoadRawTexture(Item.TexturePath);
		UTexture2D* Mask = LoadRawTexture(Item.MaskPath);
		if (!Texture || !Mask)
		{
			UE_LOG(LogTemp, Error, TEXT("Food deposit source texture missing: %s"), *Item.Name);
			return false;
		}
		Textures.Add(Texture);
		Textures.Add(Mask);
		UStaticMeshComponent* Card = NewObject<UStaticMeshComponent>(GetOwner(), FName(*FString::Printf(TEXT("Food_%s"), *Item.Name)));
		GetOwner()->AddInstanceComponent(Card);
		Card->SetupAttachment(this);
		Card->SetStaticMesh(Plane);
		Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Card->SetGenerateOverlapEvents(false);
		Card->SetCanEverAffectNavigation(false);
		Card->SetCastShadow(false);
		Card->bReceivesDecals = false;
		Card->bUseAsOccluder = false;
		const FVector Right = Convert(Item.SpriteRight).GetSafeNormal();
		const FVector Up = Convert(Item.SpriteUp).GetSafeNormal();
		const FVector Normal = FVector::CrossProduct(Right, Up).GetSafeNormal();
		Card->SetRelativeRotation(FMatrix(Right, Up, Normal, FVector::ZeroVector).ToQuat());
		Card->SetTranslucentSortPriority(46);
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
		Material->SetTextureParameterValue(TEXT("Sprite"), Texture);
		Material->SetTextureParameterValue(TEXT("Mask"), Mask);
		Material->SetVectorParameterValue(TEXT("Color1"), Item.Color1);
		Material->SetVectorParameterValue(TEXT("Color2"), Item.Color2);
		Card->SetMaterial(0, Material);
		Card->ComponentTags.Add(FName(*Item.Name));
		Card->RegisterComponent();
		Cards.Add(Card);
		if (auto* Lighting = GetWorld()->GetSubsystem<URokWorldDayNightSubsystem>())
			Lighting->RegisterSourceCard(Card, Item.Name);
	}
	State.Reset(Data.Items.Num());
	return true;
}

void URokFoodDepositComponent::UpdatePresentation(float Dxf, bool bShowDetail)
{
	if (Cards.Num() != Data.Items.Num() || Cards.IsEmpty()) return;
	const float Scale = Data.EvaluateScale(Dxf);
	State.Update(Data, Scale);
	for (int32 I = 0; I < Cards.Num(); ++I)
	{
		const auto& Item = Data.Items[I];
		// Source GridCollideItem changes the child scale, not its root position.
		// Preserve the full texture and sprite pivot; do not use alpha-tight crops.
		const FVector LocalCenter = Item.SpritePosition
			+ Item.SpriteRight * ((0.5 - Item.Pivot.X) * Item.SpriteSize.X)
			+ Item.SpriteUp * ((0.5 - Item.Pivot.Y) * Item.SpriteSize.Y);
		Cards[I]->SetRelativeLocation(Convert(Item.Position + LocalCenter * Scale) * Data.UnitsToCentimeters);
		// This component already reconstructs the source SpriteRight/Up basis.
		// Native Unity 480/594/1500 references expose the old double-negative
		// entity-card convention as a horizontal mirror here (house blue awning
		// belongs on the right). Keep X positive; only V needs the Plane flip.
		Cards[I]->SetRelativeScale3D(FVector(Item.SpriteSize.X * Scale * Data.UnitsToCentimeters / 100,
			-Item.SpriteSize.Y * Scale * Data.UnitsToCentimeters / 100, 1));
		// Source GridCollide SetActive edge; rendering-only far LOD is separate.
		if (auto* Lighting = GetWorld()->GetSubsystem<URokWorldDayNightSubsystem>())
			Lighting->UpdateSourceActive(Cards[I], State.Active[I]);
		Cards[I]->SetVisibility(bShowDetail && State.Active[I]);
		Cards[I]->SetHiddenInGame(!bShowDetail || !State.Active[I]);
	}
}

TArray<FString> URokFoodDepositComponent::GetActiveSourceItems() const
{
	TArray<FString> Result;
	for (int32 I = 0; I < State.Active.Num(); ++I)
		if (State.Active[I]) Result.Add(Data.Items[I].Name);
	return Result;
}
