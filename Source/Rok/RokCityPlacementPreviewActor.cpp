#include "RokCityPlacementPreviewActor.h"

#include "RokCityPlacementPreviewMath.h"
#include "RokLocalConstructionVisualFactory.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

namespace
{
constexpr TCHAR PreviewTag[] = TEXT("RokPlacementPreviewTemporary");
constexpr TCHAR BottomMeshPath[] = TEXT("/Game/RokUnity/City/PlacementPreview/SM_building_move_bottom_SourceQuad.SM_building_move_bottom_SourceQuad");
constexpr TCHAR BottomMaterialPath[] = TEXT("/Game/RokUnity/City/PlacementPreview/M_building_move_bottom_SourceAlpha.M_building_move_bottom_SourceAlpha");

// Unity 2019 native probe: UnityCityResourceVisualProbe/OutputRuns/20261008_161052_827_45112.
// CityGlobalMediator.CreateCityBuilding explicitly zeros each root's local Euler
// after parenting. The bottom prefab is world yaw -45; SetParent(worldPositionStays)
// under the source yaw -45 city container also makes its runtime local yaw zero.
// OBJ vertices use (-UnityX, UnityZ, UnityY)*100 cm; CityContainer accepts
// (UnityX, UnityZ, UnityY) in Unity units and includes 500 UU/unit. The local
// bridge is reflection X plus 1/100, not another 500 multiplier or rotation.
const FQuat RootToCity = FQuat::Identity;

void ConfigureNoninteractive(UStaticMeshComponent* Component, int32 SortPriority)
{
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetGenerateOverlapEvents(false);
	Component->SetRenderCustomDepth(false);
	Component->SetCastShadow(false);
	Component->bReceivesDecals = false;
	Component->bUseAsOccluder = false;
	Component->SetCanEverAffectNavigation(false);
	Component->TranslucencySortPriority = SortPriority;
}

template <typename TObject>
TObject* LoadExact(const TCHAR* ObjectPath)
{
	return Cast<TObject>(FSoftObjectPath(ObjectPath).TryLoad());
}
}

ARokCityPlacementPreviewActor::ARokCityPlacementPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetFlags(RF_Transient);
	SetActorEnableCollision(false);
	Tags.AddUnique(FName(PreviewTag));
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewCityFrame"));
	RootComponent = SceneRoot;
	CandidateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SourceGridCenter"));
	CandidateRoot->SetupAttachment(SceneRoot);
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SourceBuildingTightMesh"));
	BuildingMesh->SetupAttachment(CandidateRoot);
	BottomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SourceMoveBottomMesh"));
	BottomMesh->SetupAttachment(CandidateRoot);
	// Source shaders: map_building_with_mask-5 is Transparent-5 (2995),
	// customcitybuilding_move_bottom-4 is Transparent-4 (2996).
	ConfigureNoninteractive(BuildingMesh, 2995);
	ConfigureNoninteractive(BottomMesh, 2996);
	SetActorHiddenInGame(true);
}

bool ARokCityPlacementPreviewActor::InitializePreview(int32 TypeId, const FString& ModelId, FString& OutError)
{
	OutError.Reset();
	bInitialized = false;
	SetActorHiddenInGame(true);
	const FRokLocalSourceBuildingVisual* Visual = ModelId.IsEmpty() ? nullptr
		: FRokLocalConstructionVisualFactory::FindBuildingVisual(TypeId, ModelId);
	if (!Visual)
	{
		OutError = TEXT("该建筑在当前时代的摆放模型尚未接入。");
		return false;
	}
	UStaticMesh* SourceMesh = nullptr;
	UMaterialInterface* SourceMaterial = nullptr;
	if (!FRokLocalConstructionVisualFactory::LoadVerifiedSourceMesh(*Visual, SourceMesh, SourceMaterial, OutError))
	{
		return false;
	}
	UStaticMesh* SourceBottomMesh = LoadExact<UStaticMesh>(BottomMeshPath);
	UMaterialInterface* SourceBottomMaterial = LoadExact<UMaterialInterface>(BottomMaterialPath);
	if (!SourceBottomMesh || !SourceBottomMaterial)
	{
		OutError = TEXT("Source tight mesh, source display material, or source move-bottom asset is missing");
		return false;
	}
	BottomMaterial = UMaterialInstanceDynamic::Create(SourceBottomMaterial, this);
	if (!BottomMaterial)
	{
		OutError = TEXT("Source move-bottom material instance could not be created");
		return false;
	}
	BuildingMesh->SetStaticMesh(SourceMesh);
	BuildingMesh->SetMaterial(0, SourceMaterial);
	BottomMesh->SetStaticMesh(SourceBottomMesh);
	BottomMesh->SetMaterial(0, BottomMaterial);
	BottomMesh->SetVisibility(false);

	BuildingMesh->SetRelativeTransform(FRokLocalConstructionVisualFactory::GetSourceChildTransform(*Visual));
	BottomMesh->SetRelativeRotation(RootToCity);
	bInitialized = true;
	return true;
}

void ARokCityPlacementPreviewActor::SetCandidate(const FTransform& CityContainer,
	const FRokCityConstructionFootprint& Footprint)
{
	FVector2D LocalCenter;
	FString Error;
	if (!bInitialized || !CityContainer.IsValid()
		|| CityContainer.GetScale3D().IsNearlyZero()
		|| !FRokCityPlacementPreviewMath::GridToLocalCenter(Footprint, LocalCenter, Error))
	{
		SetActorHiddenInGame(true);
		return;
	}
	// Source bottom prefab's one-cell quad is scaled by width and length; material UV
	// repeats by the same dimensions. Bottom remains at selected root local Y=0.01.
	SetActorTransform(CityContainer);
	CandidateRoot->SetRelativeLocation(FVector(LocalCenter.X, LocalCenter.Y, 0.01));
	BottomMesh->SetRelativeScale3D(FVector(-double(Footprint.Size.X) / 100.0,
		double(Footprint.Size.Y) / 100.0, 0.01 / 100.0));
	BottomMaterial->SetVectorParameterValue(TEXT("Tiling"),
		FLinearColor(Footprint.Size.X, Footprint.Size.Y, 0.0f, 0.0f));
	SetActorHiddenInGame(false);
}

void ARokCityPlacementPreviewActor::UpdateFootprintFeedback(bool bDragging, bool bGridValid)
{
	// CityGlobalMediator.SetBuildBottomColor shows the tinted grid during movement.
	BottomMesh->SetVisibility(bDragging || !bGridValid);
	FootprintTint = bGridValid ? FLinearColor::Green : FLinearColor::Red;
	SetFootprintLightingCompensation(FootprintCompensation);
}

void ARokCityPlacementPreviewActor::SetFootprintLightingCompensation(const FLinearColor& Compensation)
{
	FootprintCompensation = Compensation;
	if (BottomMaterial)
		BottomMaterial->SetVectorParameterValue(TEXT("Tint"), FootprintTint * FootprintCompensation);
}
