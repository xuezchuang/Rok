#include "RokVillagePresentationComponent.h"

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

bool URokVillagePresentationComponent::InitializeSource()
{
	if (!Data.Load())
	{
		UE_LOG(LogTemp, Error, TEXT("Village presentation source manifest missing or invalid"));
		return false;
	}
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokVillagePresentationDay.M_RokVillagePresentationDay"));
	if (!Plane || !Parent)
	{
		UE_LOG(LogTemp, Error, TEXT("Village presentation source material/plane missing; run the dedicated material importer"));
		return false;
	}
	for (const auto& Item : Data.Items)
	{
		UTexture2D* Texture = LoadRawTexture(Item.TexturePath);
		UTexture2D* Mask = LoadRawTexture(Item.MaskPath);
		if (!Texture || !Mask)
		{
			UE_LOG(LogTemp, Error, TEXT("Village presentation source texture missing: %s"), *Item.SourceTransformId);
			return false;
		}
		Textures.Add(Texture);
		Textures.Add(Mask);
		UStaticMeshComponent* Card = NewObject<UStaticMeshComponent>(GetOwner(), FName(*FString::Printf(TEXT("Village_%s"), *Item.SourceTransformId)));
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
		Card->ComponentTags.Add(FName(*Item.SourceTransformId));
		Card->ComponentTags.Add(FName(*Item.SourceName));
		Card->ComponentTags.Add(FName(*(TEXT("UnitySpriteRenderer_") + Item.SourceRendererId)));
		Card->RegisterComponent();
		Cards.Add(Card);
		if (auto* Lighting = GetWorld()->GetSubsystem<URokWorldDayNightSubsystem>())
			Lighting->RegisterSourceCard(Card, Item.SourceTransformId);
	}
	State.Reset(Data.Items.Num());
	return true;
}

void URokVillagePresentationComponent::UpdatePresentation(float Dxf, bool bShowDetail)
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
		// The native SpriteRenderer convention requires positive horizontal
		// scale in this reconstructed basis; only V needs the Plane flip.
		// Village has its own native graphics reference acceptance gate.
		Cards[I]->SetRelativeScale3D(FVector(Item.SpriteSize.X * Scale * Data.UnitsToCentimeters / 100,
			-Item.SpriteSize.Y * Scale * Data.UnitsToCentimeters / 100, 1));
		// Source GridCollide SetActive edge; rendering-only far LOD is separate.
		if (auto* Lighting = GetWorld()->GetSubsystem<URokWorldDayNightSubsystem>())
			Lighting->UpdateSourceActive(Cards[I], State.Active[I]);
		Cards[I]->SetVisibility(bShowDetail && State.Active[I]);
		Cards[I]->SetHiddenInGame(!bShowDetail || !State.Active[I]);
	}
}

TArray<FString> URokVillagePresentationComponent::GetActiveSourceItems() const
{
	TArray<FString> Result;
	for (int32 I = 0; I < State.Active.Num(); ++I)
		if (State.Active[I]) Result.Add(Data.Items[I].SourceTransformId);
	return Result;
}

bool URokVillagePresentationComponent::ApplyLocalDayNightSample(const FRokVillageDayNightSample& Sample)
{
	if (Cards.IsEmpty() || Cards.Num() != Data.Items.Num()) return false;
	if (!bDayNightDataLoaded)
	{
		if (!DayNightData.Load()) return false;
		bDayNightDataLoaded = true;
	}
	FRokVillageDayNightResult Result;
	if (!DayNightData.Evaluate(Sample, Result)) return false;
	// Refuse old day-only assets instead of silently pretending night was applied.
	TArray<UMaterialInstanceDynamic*> Materials;
	for (const auto& Card : Cards)
	{
		auto* Material = Card ? Cast<UMaterialInstanceDynamic>(Card->GetMaterial(0)) : nullptr;
		if (!Material) return false;
		for (const TCHAR* Name : {TEXT("Color1"), TEXT("Color2"), TEXT("SpriteColor"), TEXT("NightLight")})
		{
			FLinearColor Value;
			if (!Material->GetVectorParameterValue(FMaterialParameterInfo(Name), Value)) return false;
		}
		Materials.Add(Material);
	}
	for (auto* Material : Materials)
	{
		Material->SetVectorParameterValue(TEXT("Color1"), Result.Color1);
		Material->SetVectorParameterValue(TEXT("Color2"), Result.Color2);
		Material->SetVectorParameterValue(TEXT("SpriteColor"), Result.SpriteColor);
		Material->SetVectorParameterValue(TEXT("NightLight"), Result.NightLight);
	}
	DayNightState = Result; bHasDayNightSample = true;
	return true;
}
