#include "RokCityPlacementPreviewActor.h"

#include "RokCityPlacementPreviewMath.h"
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
constexpr TCHAR FarmMeshPath[] = TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/SM_build_2_101_1_SourceSpriteMesh.SM_build_2_101_1_SourceSpriteMesh");
constexpr TCHAR FarmMaterialPath[] = TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/M_build_2_101_1_SourceRG.M_build_2_101_1_SourceRG");
constexpr TCHAR WoodMeshPath[] = TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/SM_build_3_101_1_SourceSpriteMesh.SM_build_3_101_1_SourceSpriteMesh");
constexpr TCHAR WoodMaterialPath[] = TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/M_build_3_101_1_SourceRG.M_build_3_101_1_SourceRG");

// Unity 2019 native probe: UnityCityResourceVisualProbe/OutputRuns/20261008_161052_827_45112.
// CityGlobalMediator.CreateCityBuilding explicitly zeros each root's local Euler
// after parenting. The bottom prefab is world yaw -45; SetParent(worldPositionStays)
// under the source yaw -45 city container also makes its runtime local yaw zero.
// OBJ vertices use (-UnityX, UnityZ, UnityY)*100 cm; CityContainer accepts
// (UnityX, UnityZ, UnityY) in Unity units and includes 500 UU/unit. The local
// bridge is reflection X plus 1/100, not another 500 multiplier or rotation.
const FQuat RootToCity = FQuat::Identity;

FQuat UnityRotationToCity(const FQuat& Q)
{
	// Conjugation by the Unity XYZ -> city-local XZY reflection.
	return FQuat(-Q.X, -Q.Z, -Q.Y, Q.W);
}

FVector UnityPositionToCity(const FVector& P) { return FVector(P.X, P.Z, P.Y); }

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

bool ARokCityPlacementPreviewActor::InitializePreview(int32 TypeId, int32 VisualCivilization, int32 Age, FString& OutError)
{
	OutError.Reset();
	bInitialized = false;
	SetActorHiddenInGame(true);
	if ((TypeId != 2 && TypeId != 3) || VisualCivilization != 101 || Age != 1)
	{
		OutError = TEXT("Only source Farm 2/Wood 3 civilization 101 age 1 have verified preview assets");
		return false;
	}
	const TCHAR* MeshPath = TypeId == 2 ? FarmMeshPath : WoodMeshPath;
	const TCHAR* MaterialPath = TypeId == 2 ? FarmMaterialPath : WoodMaterialPath;
	UStaticMesh* SourceMesh = LoadExact<UStaticMesh>(MeshPath);
	UMaterialInterface* SourceMaterial = LoadExact<UMaterialInterface>(MaterialPath);
	UStaticMesh* SourceBottomMesh = LoadExact<UStaticMesh>(BottomMeshPath);
	UMaterialInterface* SourceBottomMaterial = LoadExact<UMaterialInterface>(BottomMaterialPath);
	if (!SourceMesh || !SourceMaterial || !SourceBottomMesh || !SourceBottomMaterial)
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

	// Preserve native root/child pivot and pose. CityContainer owns the 500x unit scale;
	// the source OBJ was authored at 100 cm/unit, so component scale divides by 100.
	const FVector ChildPosition = TypeId == 2
		? FVector(0.0430000014603138, 0.05299999937415123, 0.03099999949336052)
		: FVector(0.003000000026077032, 0.04100000113248825, 0.0560000017285347);
	const double ChildScale = TypeId == 2 ? 0.13229629397392273 : 0.1359577178955078;
	const FQuat ChildUnityRotation(0.3535533845424652, 0.3535533845424652,
		-0.1464466005563736, 0.8535534739494324);
	const FQuat ChildCityRotation = UnityRotationToCity(ChildUnityRotation);
	const FVector ChildCityPosition = RootToCity.RotateVector(UnityPositionToCity(ChildPosition));
	BuildingMesh->SetRelativeTransform(FTransform(RootToCity * ChildCityRotation,
		ChildCityPosition, FVector(-ChildScale / 100.0, ChildScale / 100.0, ChildScale / 100.0)));
	BottomMesh->SetRelativeRotation(RootToCity);
	bInitialized = true;
	return true;
}

void ARokCityPlacementPreviewActor::SetCandidate(const FTransform& CityContainer,
	const FRokCityConstructionFootprint& Footprint, bool bGridValid)
{
	FVector2D LocalCenter;
	FString Error;
	if (!bInitialized || Footprint.Size != FIntPoint(5, 5) || !CityContainer.IsValid()
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
	BottomMaterial->SetVectorParameterValue(TEXT("Tint"), bGridValid ? FLinearColor::Green : FLinearColor::Red);
	BottomMaterial->SetVectorParameterValue(TEXT("Tiling"),
		FLinearColor(Footprint.Size.X, Footprint.Size.Y, 0.0f, 0.0f));
	SetActorHiddenInGame(false);
}
