#include "RokWorldMapCityActor.h"

#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RokDataPaths.h"
#include "RokWorldMapCityPulse.h"
#include "RokWorldMapCameraPawn.h"
#include "Kismet/GameplayStatics.h"
#include "RokWorldMapTypes.h"

namespace
{
constexpr float CityFarMinDxf = 3000.0f;
constexpr float CityFarMaxDxf = 5400.0f;
constexpr float CityFarWorldHeightCm = 650.0f;

const FVector CityCardUp(0, 0.70710678f, 0.70710678f);
const FVector CityCardNormal(0, 0.70710678f, -0.70710678f);
}

ARokWorldMapCityActor::ARokWorldMapCityActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("WorldMapCityVisual"));
	VisualMeshComponent->SetupAttachment(SceneRoot);
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	VisualMeshComponent->SetGenerateOverlapEvents(false);
	VisualMeshComponent->CastShadow = false;
	VisualMeshComponent->bReceivesDecals = false;
	VisualMeshComponent->bUseAsOccluder = false;
	VisualMeshComponent->SetCanEverAffectNavigation(false);

	SelectionCollisionComponent = CreateDefaultSubobject<UBoxComponent>(
		TEXT("WorldMapCitySelectionCollision"));
	SelectionCollisionComponent->SetupAttachment(VisualMeshComponent);
	SelectionCollisionComponent->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);
	SelectionCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionCollisionComponent->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);
	SelectionCollisionComponent->ComponentTags.Add(
		FRokWorldMapMath::WorldMapSelectionColliderTag());
	SelectionCollisionComponent->SetGenerateOverlapEvents(false);
	SelectionCollisionComponent->SetCanEverAffectNavigation(false);
	SelectionCollisionComponent->SetVisibility(false);
	SelectionCollisionComponent->SetHiddenInGame(true);

	FarVisualComponent = CreateDefaultSubobject<UBillboardComponent>(
		TEXT("WorldMapCityFarVisual"));
	FarVisualComponent->SetupAttachment(SceneRoot);
	FarVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FarVisualComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	FarVisualComponent->SetGenerateOverlapEvents(false);
	FarVisualComponent->SetCanEverAffectNavigation(false);
	FarVisualComponent->SetCastShadow(false);
	FarVisualComponent->SetOpacityMaskRefVal(0.08f);
	FarVisualComponent->SetVisibility(false);
	FarVisualComponent->SetHiddenInGame(true);

	FarSelectionCollisionComponent =
		CreateDefaultSubobject<UBoxComponent>(
			TEXT("WorldMapCityFarSelectionCollision"));
	FarSelectionCollisionComponent->SetupAttachment(SceneRoot);
	FarSelectionCollisionComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	FarSelectionCollisionComponent->SetCollisionResponseToAllChannels(
		ECR_Ignore);
	FarSelectionCollisionComponent->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);
	FarSelectionCollisionComponent->ComponentTags.Add(
		FRokWorldMapMath::WorldMapSelectionColliderTag());
	FarSelectionCollisionComponent->SetGenerateOverlapEvents(false);
	FarSelectionCollisionComponent->SetCanEverAffectNavigation(false);
	FarSelectionCollisionComponent->SetVisibility(false);
	FarSelectionCollisionComponent->SetHiddenInGame(true);
	FarSelectionCollisionComponent->SetRelativeLocation(
		FVector(0.0f, 0.0f, CityFarWorldHeightCm * 0.5f));
	FarSelectionCollisionComponent->SetBoxExtent(
		FVector(900.0f, 900.0f, CityFarWorldHeightCm * 0.5f),
		false);
}

void ARokWorldMapCityActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureFarVisualLoaded();
}

void ARokWorldMapCityActor::ConfigureSourceVisual(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	FRotator VisualRotation,
	FVector InBaseVisualScale,
	FVector InSelectionAnchorOffset,
	FVector InBaseVisualCenterOffset,
	float InitialDxf,
	float InAgeLodScaleCap,
	int32 SortPriority)
{
	if (!VisualMeshComponent || !Mesh || !Material)
	{
		return;
	}

	BaseVisualScale = InBaseVisualScale;
	SelectionAnchorOffset = InSelectionAnchorOffset;
	BaseVisualCenterOffset = InBaseVisualCenterOffset;
	AgeLodScaleCap = FMath::Max(0.0f, InAgeLodScaleCap);
	CurrentLodScale = -1.0f;
	EnsureFarVisualLoaded();

	VisualMeshComponent->SetStaticMesh(Mesh);
	for (int32 MaterialIndex = 0;
			MaterialIndex < FMath::Max(VisualMeshComponent->GetNumMaterials(), 1);
			++MaterialIndex)
	{
		VisualMeshComponent->SetMaterial(MaterialIndex, Material);
	}
	VisualMeshComponent->SetRelativeRotation(VisualRotation);
	VisualMeshComponent->TranslucencySortPriority = SortPriority;
	if (SelectionCollisionComponent)
	{
		const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
		SelectionCollisionComponent->SetRelativeLocation(MeshBounds.Origin);
		SelectionCollisionComponent->SetRelativeRotation(FRotator::ZeroRotator);
		SelectionCollisionComponent->SetRelativeScale3D(FVector::OneVector);
		SelectionCollisionComponent->SetBoxExtent(
			FVector(
				FMath::Max(MeshBounds.BoxExtent.X, 1.0f),
				FMath::Max(MeshBounds.BoxExtent.Y, 1.0f),
				FMath::Max(MeshBounds.BoxExtent.Z, 50.0f)),
			false);
	}
	ApplyCameraDxf(InitialDxf);
}

void ARokWorldMapCityActor::ApplyCameraDxf(float Dxf)
{
	EnsureFarVisualLoaded();
	const bool bInFarBand =
		FMath::IsFinite(Dxf)
		&& Dxf >= CityFarMinDxf
		&& Dxf <= (ActorHasTag(FRokWorldMapMath::WorldMapPlayerCityTag()) ? 72720.0f : CityFarMaxDxf);
	const bool bShowFar = bInFarBand && FarSpriteComponent;
	const bool bShowPulse = bShowFar && ActorHasTag(FRokWorldMapMath::WorldMapPlayerCityTag());
	if (bShowPulse) EnsurePulseLoaded();
	if (bShowPulse && !bPulseVisible) PulseSeconds = 0.0;
	bPulseVisible = bShowPulse;
	float UnityScale = 500.0f;
	if (const auto* Camera = Cast<ARokWorldMapCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		UnityScale = Camera->GetWorldUnitsPerUnityUnit();
	// city_lod3.m_scale_explore_curve continues through Nationwide for own city.
	const float OverviewScale = EvaluateOwnCityOverviewScale(Dxf);
	PulseDiameter = 2.0f * UnityScale * OverviewScale;
	for (const auto& Plane : PulsePlanes) Plane->SetVisibility(bShowPulse);
	float DetailLodScale = EvaluateUnityCityLodScale(
		Dxf,
		AgeLodScaleCap);
	if (bInFarBand && !bShowFar)
	{
		DetailLodScale = FMath::Min(AgeLodScaleCap, 6.0f);
	}
	ApplyUnityCityLodScale(DetailLodScale);

	const bool bShowDetail =
		!bShowFar && DetailLodScale > UE_SMALL_NUMBER;
	if (VisualMeshComponent)
	{
		VisualMeshComponent->SetVisibility(bShowDetail, false);
		VisualMeshComponent->SetHiddenInGame(!bShowDetail, false);
	}
	if (FarSpriteComponent)
	{
		FarSpriteComponent->SetVisibility(bShowFar, true);
		FarSpriteComponent->SetHiddenInGame(!bShowFar, true);
		if (bShowFar)
		{
			// ui_map/img_map_1014: full rect, centered pivot, 100 PPU.
			const float Width = FarTexture->GetSizeX() / 100.0f * UnityScale * OverviewScale;
			const float Height = FarTexture->GetSizeY() / 100.0f * UnityScale * OverviewScale;
			FarSpriteComponent->SetRelativeScale3D(FVector(-Width / 100.0f, -Height / 100.0f, 1));
			// Keep the source screen anchor while clearing UE's depth-writing terrain.
			const FVector Offset = -CityCardNormal * (Height * 0.5f + 12.0f);
			FarSpriteComponent->SetRelativeLocation(Offset);
			FarSelectionCollisionComponent->SetRelativeLocation(Offset);
			FarSelectionCollisionComponent->SetRelativeRotation(FarSpriteComponent->GetRelativeRotation());
			FarSelectionCollisionComponent->SetBoxExtent(FVector(Width * 0.5f, Height * 0.5f, 50));
		}
	}
	if (SelectionCollisionComponent)
	{
		SelectionCollisionComponent->SetCollisionEnabled(
			bShowDetail
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
	}
	if (FarSelectionCollisionComponent)
	{
		FarSelectionCollisionComponent->SetCollisionEnabled(
			bShowFar
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
	}
}

void ARokWorldMapCityActor::EnsurePulseLoaded()
{
	if (!PulsePlanes.IsEmpty()) return;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldCityPulse.M_RokWorldCityPulse"));
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Base || !PlaneMesh) return;
	PulseTexture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectDir() /
		TEXT("ref/client-unity/Assets/BundleAssets/Effect/Textures/shared_effecttextures/circle006.png"));
	if (!PulseTexture) return;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Plane = NewObject<UStaticMeshComponent>(this);
		Plane->SetupAttachment(SceneRoot);
		Plane->SetStaticMesh(PlaneMesh);
		Plane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plane->SetCastShadow(false);
		Plane->bReceivesDecals = false;
		Plane->SetCanEverAffectNavigation(false);
		// SetCityObj_Effect.SetParent preserves world rotation; emitter X90
		// therefore remains a horizontal ring. Small depth separation clears terrain.
		Plane->SetRelativeLocation(FVector(0, 0, 8));
		auto* Material = UMaterialInstanceDynamic::Create(Base, this);
		Material->SetTextureParameterValue(TEXT("Sprite"), PulseTexture);
		Material->SetVectorParameterValue(TEXT("Tint"), Index < 3
			? FLinearColor(0, 1, 0) : FLinearColor(0.7254905f, 1, 0.3726415f));
		Material->SetScalarParameterValue(TEXT("ParticleAlpha"), 0);
		Plane->SetMaterial(0, Material);
		Plane->RegisterComponent();
		PulsePlanes.Add(Plane);
		PulseMaterials.Add(Material);
	}
}

void ARokWorldMapCityActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPulseVisible) return;
	PulseSeconds += FMath::Max(0.0f, DeltaSeconds);
	for (int32 Index = 0; Index < PulsePlanes.Num(); ++Index)
	{
		const FVector2D State = RokWorldMapCityPulse::Evaluate(PulseSeconds, Index);
		PulsePlanes[Index]->SetRelativeScale3D(FVector(
			PulseDiameter * State.X / 100.0, PulseDiameter * State.X / 100.0, 1));
		PulseMaterials[Index]->SetScalarParameterValue(TEXT("ParticleAlpha"), State.Y);
	}
}

FVector ARokWorldMapCityActor::GetVisualCenterWorldPosition() const
{
	if (FarSpriteComponent && FarSpriteComponent->IsVisible())
	{
		return FarSpriteComponent->GetComponentLocation();
	}
	return VisualMeshComponent
		? VisualMeshComponent->Bounds.Origin
		: GetActorLocation();
}

FVector ARokWorldMapCityActor::GetVisualBoundsExtent() const
{
	return VisualMeshComponent
		? VisualMeshComponent->Bounds.BoxExtent
		: FVector::ZeroVector;
}

FVector ARokWorldMapCityActor::GetSelectionAnchorWorldPosition() const
{
	return GetActorTransform().TransformPosition(SelectionAnchorOffset);
}

float ARokWorldMapCityActor::EvaluateUnityCityLodScale(
	float Dxf,
	float LodScaleCap)
{
	if (!FMath::IsFinite(Dxf) || !FMath::IsFinite(LodScaleCap))
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

	return FMath::Clamp(
		CurveValue,
		0.0f,
		FMath::Max(0.0f, LodScaleCap));
}

void ARokWorldMapCityActor::EnsureFarVisualLoaded()
{
	if (FarSpriteComponent)
	{
		return;
	}
	const FString TexturePath = FPaths::ConvertRelativePathToFull(
		RokDataPaths::Resolve(TEXT("WorldEntityPopulation/player_city_lod3.png")));
	if (!FPaths::FileExists(TexturePath))
	{
		return;
	}
	FarTexture = FImageUtils::ImportFileAsTexture2D(TexturePath);
	if (!FarTexture)
	{
		return;
	}
	FarTexture->NeverStream = true;
	FarTexture->Filter = TF_Bilinear;
	FarTexture->UpdateResource();
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldEntitySprite.M_RokWorldEntitySprite"));
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Base || !Mesh) return;
	FarSpriteComponent = NewObject<UStaticMeshComponent>(this, TEXT("WorldCityOverviewSprite"));
	FarSpriteComponent->SetupAttachment(SceneRoot);
	FarSpriteComponent->SetStaticMesh(Mesh);
	FarSpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FarSpriteComponent->SetCastShadow(false);
	FarSpriteComponent->bReceivesDecals = false;
	FarSpriteComponent->SetCanEverAffectNavigation(false);
	FarSpriteComponent->TranslucencySortPriority = 47;
	FarSpriteComponent->SetRelativeRotation(FMatrix(FVector::CrossProduct(CityCardUp, CityCardNormal),
		CityCardUp, CityCardNormal, FVector::ZeroVector).ToQuat());
	auto* Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetTextureParameterValue(TEXT("Sprite"), FarTexture);
	FarSpriteComponent->SetMaterial(0, Material);
	FarSpriteComponent->SetVisibility(false);
	FarSpriteComponent->RegisterComponent();
	// Retain the serialized legacy subobject, but never render its masked billboard.
	FarVisualComponent->SetVisibility(false);
	FarVisualComponent->SetHiddenInGame(true);
}

float ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(float Dxf)
{
	// city_lod3.prefab m_scale_explore_curve (IsExplore=true for own city).
	if (!FMath::IsFinite(Dxf) || Dxf <= 3000.0f) return 0.0f;
	if (Dxf < 3000.001f)
		return FMath::CubicInterp(0.0f, 6.0f, 6.0f, 6.0f,
			(Dxf - 3000.0f) / (3000.001f - 3000.0f));
	if (Dxf <= 5400.0f)
		return FMath::Lerp(6.0f, 6.4f, (Dxf - 3000.001f) / (5400.0f - 3000.001f));
	if (Dxf <= 44939.875f)
	{
		const float Span = 44939.875f - 5400.0f;
		return FMath::CubicInterp(6.4f, 0.00097622967f * Span, 45.0f,
			0.00097622967f * Span, (Dxf - 5400.0f) / Span);
	}
	const float Span = 72720.0f - 44939.875f;
	return FMath::CubicInterp(45.0f, 0.0003599696f * Span, 55.0f,
		0.0003599696f * Span, FMath::Clamp((Dxf - 44939.875f) / Span, 0.0f, 1.0f));
}

void ARokWorldMapCityActor::ApplyUnityCityLodScale(float LodScale)
{
	if (!VisualMeshComponent
		|| FMath::IsNearlyEqual(CurrentLodScale, LodScale))
	{
		return;
	}

	CurrentLodScale = LodScale;
	VisualMeshComponent->SetRelativeScale3D(FVector(
		BaseVisualScale.X * LodScale,
		BaseVisualScale.Y * LodScale,
		BaseVisualScale.Z));
	VisualMeshComponent->SetRelativeLocation(
		SelectionAnchorOffset + BaseVisualCenterOffset * LodScale);
}
