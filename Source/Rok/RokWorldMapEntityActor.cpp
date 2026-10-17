#include "RokWorldMapEntityActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapTypes.h"
#include "RokFoodDepositComponent.h"
#include "RokWoodDepositComponent.h"
#include "RokStoneDepositComponent.h"
#include "RokVillagePresentationComponent.h"

namespace
{
// 45-degree tilted card basis shared with the runtime city sprite actors:
// U=(-1,0,0), V=(0,0.7071,0.7071), N=(0,0.7071,-0.7071).
const FVector CardNormal = FVector(
	0.0f,
	0.70710678f,
	-0.70710678f);
const FVector CardUp = FVector(0.0f, 0.70710678f, 0.70710678f);

FQuat CardRotation()
{
	return FMatrix(
		FVector::CrossProduct(CardUp, CardNormal),
		CardUp,
		CardNormal,
		FVector::ZeroVector).ToQuat();
}

// Cell.InitDummy/InitDummyS override the prefab rotation at runtime with
// Euler(45,0,0). Reading the identity prefab root alone squashes the troops.
const FVector UnitCardUp = CardUp;
const FVector UnitCardNormal = CardNormal;

FQuat UnitCardRotation()
{
	return FMatrix(
		FVector::CrossProduct(UnitCardUp, UnitCardNormal),
		UnitCardUp,
		UnitCardNormal,
		FVector::ZeroVector).ToQuat();
}

const TCHAR* EntitySpriteMaterialPath =
	TEXT("/Game/RokUnityWorldMapPrototype/Materials/"
		"M_RokWorldEntitySprite.M_RokWorldEntitySprite");

// Troops m_lod_array = [501, 900, 99999]: expansion runs at lod<=1,
// i.e. dxf at or below the second threshold (LevelDetailBase uses <=).
constexpr float FormationExpansionMaxDxf = 900.0f;

// The engine Plane's UVs are vertically opposite to Unity SpriteRenderer
// quads (UE texture V grows downward, and the card's local +X faces world
// -X), so an unadjusted card renders every sprite rotated 180 degrees on
// screen. Negate both in-plane scale axes to compensate - the same fix as
// the city slice's ROK_SINGLE_BUILDING_ENGINE_PLANE_FLIP_Y.
static FVector CardScale(float WidthCm, float HeightCm)
{
	return FVector(-WidthCm / 100.0f, -HeightCm / 100.0f, 1.0f);
}

void ConfigureCardComponent(UStaticMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}
	Component->SetHiddenInGame(false);
	Component->SetVisibility(true);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->bReceivesDecals = false;
	Component->bUseAsOccluder = false;
	Component->SetRelativeRotation(CardRotation());
}
}

ARokWorldMapEntityActor::ARokWorldMapEntityActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DetailVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("DetailVisual"));
	DetailVisual->SetupAttachment(SceneRoot);
	ConfigureCardComponent(DetailVisual);

	FarVisual = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("FarVisual"));
	FarVisual->SetupAttachment(SceneRoot);
	ConfigureCardComponent(FarVisual);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		DetailVisual->SetStaticMesh(PlaneMesh.Object);
		FarVisual->SetStaticMesh(PlaneMesh.Object);
	}

	SelectionCollision = CreateDefaultSubobject<UBoxComponent>(
		TEXT("SelectionCollision"));
	SelectionCollision->SetupAttachment(SceneRoot);
	SelectionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionCollision->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);
	SelectionCollision->ComponentTags.Add(
		FRokWorldMapMath::WorldMapSelectionColliderTag());
	SelectionCollision->SetGenerateOverlapEvents(false);
	SelectionCollision->SetCanEverAffectNavigation(false);
}

void ARokWorldMapEntityActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	const ARokWorldMapCameraPawn* CameraPawn =
		PlayerController
			? Cast<ARokWorldMapCameraPawn>(PlayerController->GetPawn())
			: nullptr;
	if (CameraPawn)
	{
		ApplyDxf(CameraPawn->GetCurrentDxf());
	}
	AdvanceIdleAnimation(DeltaSeconds);
}

UMaterialInstanceDynamic* ARokWorldMapEntityActor::MakeCardMaterial(
	UTexture2D* Texture)
{
	if (!CardMaterialParent && !bCardMaterialParentLoadFailed)
	{
		CardMaterialParent = Cast<UMaterial>(
			StaticLoadObject(
				UMaterial::StaticClass(),
				nullptr,
				EntitySpriteMaterialPath));
		bCardMaterialParentLoadFailed = !CardMaterialParent;
		if (bCardMaterialParentLoadFailed)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Rok world entity card material missing: %s"),
				EntitySpriteMaterialPath);
		}
	}
	if (!CardMaterialParent || !Texture)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Instance =
		UMaterialInstanceDynamic::Create(CardMaterialParent, this);
	Instance->SetTextureParameterValue(TEXT("Sprite"), Texture);
	return Instance;
}

bool ARokWorldMapEntityActor::Configure(
	const FRokWorldMapEntityRecord& InRecord,
	float InitialDxf)
{
	Record = InRecord;
	DetailTexture = LoadTransientTexture(Record.DetailTexturePath);
	FarTexture = LoadTransientTexture(Record.FarTexturePath);
	if (!DetailTexture || !FarTexture)
	{
		return false;
	}

	if (Record.Kind == TEXT("food"))
	{
		FoodDeposit = NewObject<URokFoodDepositComponent>(this, TEXT("FoodDeposit"));
		AddInstanceComponent(FoodDeposit);
		FoodDeposit->SetupAttachment(SceneRoot);
		FoodDeposit->RegisterComponent();
		if (!FoodDeposit->InitializeSource()) return false;
	}
	if (Record.Kind == TEXT("wood"))
	{
		WoodDeposit = NewObject<URokWoodDepositComponent>(this, TEXT("WoodDeposit"));
		AddInstanceComponent(WoodDeposit);
		WoodDeposit->SetupAttachment(SceneRoot);
		WoodDeposit->RegisterComponent();
		if (!WoodDeposit->InitializeSource()) return false;
	}
	if (Record.Kind == TEXT("stone"))
	{
		StoneDeposit = NewObject<URokStoneDepositComponent>(this, TEXT("StoneDeposit"));
		AddInstanceComponent(StoneDeposit);
		StoneDeposit->SetupAttachment(SceneRoot);
		StoneDeposit->RegisterComponent();
		if (!StoneDeposit->InitializeSource()) return false;
	}
	if (Record.Kind == TEXT("village"))
	{
		VillagePresentation = NewObject<URokVillagePresentationComponent>(this, TEXT("VillagePresentation"));
		AddInstanceComponent(VillagePresentation);
		VillagePresentation->SetupAttachment(SceneRoot);
		VillagePresentation->RegisterComponent();
		if (!VillagePresentation->InitializeSource()) return false;
	}
	BuildFormationUnits();
	// Composite cards keep the cluster card; formations replace it with
	// per-unit planes.
	const bool bHasFormation = FormationUnitVisuals.Num() > 0;
	if (DetailVisual)
	{
		if (UMaterialInstanceDynamic* Material = MakeCardMaterial(
				DetailTexture))
		{
			DetailVisual->SetMaterial(0, Material);
			DetailBaseWidth = Record.DetailWorldHeightCm
				* static_cast<float>(DetailTexture->GetSizeX())
				/ FMath::Max(
					static_cast<float>(DetailTexture->GetSizeY()),
					1.0f);
			DetailVisual->SetRelativeScale3D(CardScale(
				DetailBaseWidth,
				Record.DetailWorldHeightCm));
			DetailVisual->SetRelativeLocation(
				CardUp * (Record.DetailWorldHeightCm * 0.48f));
			DetailVisual->SetVisibility(!bHasFormation, true);
			DetailVisual->SetHiddenInGame(bHasFormation, true);
		}
	}
	if (FarVisual)
	{
		if (UMaterialInstanceDynamic* Material = MakeCardMaterial(
				FarTexture))
		{
			FarVisual->SetMaterial(0, Material);
			FarBaseWidth = Record.FarWorldHeightCm
				* static_cast<float>(FarTexture->GetSizeX())
				/ FMath::Max(
					static_cast<float>(FarTexture->GetSizeY()),
					1.0f);
			FarVisual->SetRelativeScale3D(CardScale(
				FarBaseWidth,
				Record.FarWorldHeightCm));
		// Unity lod3 pivot is (0.5,0.5) at the entity ground point. Its
		// transparent layer is drawn after terrain. Move toward the camera
		// (not up the screen) to clear UE's depth-writing ground while keeping
		// that projected center, instead of separating the icon from its badge.
		FarVisual->SetRelativeLocation(
			-CardNormal * (Record.FarWorldHeightCm * 0.5f));
		FarVisual->SetTranslucentSortPriority(47);
		}
	}

	const float MaxWidth = FMath::Max(DetailBaseWidth, FarBaseWidth);
	const float MaxHeight = FMath::Max(
		Record.DetailWorldHeightCm,
		Record.FarWorldHeightCm);
	SelectionCollision->SetRelativeLocation(
		FVector(0.0f, 0.0f, MaxHeight * 0.5f));
	SelectionCollision->SetBoxExtent(
		FVector(
			FMath::Max(MaxWidth * 0.48f, 300.0f),
			FMath::Max(MaxWidth * 0.32f, 300.0f),
			FMath::Max(MaxHeight * 0.5f, 500.0f)),
		false);

	Tags.AddUnique(FName(TEXT("RokWorldMapGeneratedEntity")));
	Tags.AddUnique(FName(TEXT("RokWorldMapExactUnitySource")));
	Tags.AddUnique(FName(*FString::Printf(
		TEXT("UnitySourceId_%d"),
		Record.SourceId)));
	Tags.AddUnique(FName(*FString::Printf(
		TEXT("RokWorldMapKind_%s"),
		*Record.Kind)));
	if (Record.bSelectable)
	{
		Tags.AddUnique(FRokWorldMapMath::WorldMapSelectableTag());
	}

	CurrentLodBand = ERokWorldMapEntityLodBand::Hidden;
	LastAppliedDxf = -1.0f;
	ApplyDxf(InitialDxf);
	return true;
}

FText ARokWorldMapEntityActor::GetSelectionText() const
{
	if (Record.Level > 0)
	{
		return FText::Format(
			NSLOCTEXT(
				"RokWorldMap",
				"EntitySelectionWithLevel",
				"{0}  等级 {1}"),
			FText::FromString(Record.DisplayName),
			FText::AsNumber(Record.Level));
	}
	return FText::FromString(Record.DisplayName);
}

FVector ARokWorldMapEntityActor::GetSelectionAnchorWorldPosition() const
{
	return SelectionCollision
		? SelectionCollision->Bounds.Origin
		: GetActorLocation();
}

bool ARokWorldMapEntityActor::ShouldExpandFormation(float Dxf)
{
	return FMath::IsFinite(Dxf) && Dxf >= 0.0f
		&& Dxf <= FormationExpansionMaxDxf;
}

ERokWorldMapEntityLodBand ARokWorldMapEntityActor::ResolveLodBand(
	float Dxf,
	float CloseDetailMaxDxf,
	float TacticalDetailMaxDxf,
	float FarIconMaxDxf)
{
	if (!FMath::IsFinite(Dxf) || Dxf < 0.0f)
	{
		return ERokWorldMapEntityLodBand::Hidden;
	}
	if (Dxf < CloseDetailMaxDxf)
	{
		return ERokWorldMapEntityLodBand::CloseDetail;
	}
	// LevelDetailCamera.GetLodLevelByDxf keeps exact 3000 in Tactical.
	if (Dxf <= TacticalDetailMaxDxf)
	{
		return ERokWorldMapEntityLodBand::TacticalDetail;
	}
	if (Dxf <= FarIconMaxDxf)
	{
		return ERokWorldMapEntityLodBand::FarIcon;
	}
	return ERokWorldMapEntityLodBand::Hidden;
}

namespace
{
// Idle-frame textures are shared across all entity actors; importing the
// same 8-frame sheet once per unit per entity would re-decode dozens of
// duplicate Texture2Ds every spawn.
TMap<FString, TWeakObjectPtr<UTexture2D>>& GetSharedTextureCache()
{
	static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
	return Cache;
}
}

UTexture2D* ARokWorldMapEntityActor::LoadSharedTexture(
	const FString& FilePath)
{
	TMap<FString, TWeakObjectPtr<UTexture2D>>& Cache = GetSharedTextureCache();
	if (const TWeakObjectPtr<UTexture2D>* Found = Cache.Find(FilePath);
		Found && Found->IsValid())
	{
		return Found->Get();
	}
	UTexture2D* Texture = LoadTransientTexture(FilePath);
	if (Texture)
	{
		Cache.Add(FilePath, Texture);
	}
	return Texture;
}

UTexture2D* ARokWorldMapEntityActor::LoadTransientTexture(
	const FString& FilePath)
{
	if (!FPaths::FileExists(FilePath))
	{
		return nullptr;
	}
	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FilePath);
	if (Texture)
	{
		Texture->NeverStream = true;
		Texture->Filter = TF_Bilinear;
		Texture->UpdateResource();
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok entity texture %s -> %dx%d fmt=%d"),
			*FPaths::GetBaseFilename(FilePath),
			Texture->GetSizeX(),
			Texture->GetSizeY(),
			static_cast<int64>(Texture->GetPixelFormat()));
	}
	return Texture;
}

void ARokWorldMapEntityActor::BuildFormationUnits()
{
	FormationUnitVisuals.Reset();
	IdleFrameTextures.Reset();
	UStaticMesh* CardMesh =
		DetailVisual ? DetailVisual->GetStaticMesh() : nullptr;
	if (!CardMesh)
	{
		return;
	}
	for (const FRokWorldMapFormationUnit& Unit : Record.FormationUnits)
	{
		FFormationUnitVisual Visual;
		Visual.Unit = Unit;
		for (const FString& FramePath : Unit.FrameTexturePaths)
		{
			if (UTexture2D* Texture = LoadSharedTexture(FramePath))
			{
				Visual.Frames.Add(Texture);
				IdleFrameTextures.AddUnique(Texture);
			}
		}
		for (const FString& FramePath : Unit.FadeFrameTexturePaths)
		{
			if (UTexture2D* Texture = LoadSharedTexture(FramePath))
			{
				Visual.FadeFrames.Add(Texture);
				IdleFrameTextures.AddUnique(Texture);
			}
		}
		if (Visual.Frames.Num() != Unit.FrameTexturePaths.Num()
			|| Visual.FadeFrames.Num() != Visual.Frames.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Rok world entity %s lacks matching source fade frames"), *Unit.UnitPrefab);
			continue;
		}
		if (Visual.Frames.Num() == 0)
		{
			continue;
		}
		// Every instantiated unit (including clones) owns an independent
		// AnimationBase in Unity. Pool OnSpawn resets the frame to zero
		// after OnEnable randomizes its timer phase.
		FRandomStream Random(FMath::Rand());
		for (FRokWorldMapSpriteAnimation& Animation : Visual.Animation)
		{
			Animation.Reset(Random, Visual.Frames.Num(), Unit.IdleRandomOffset);
		}

		Visual.TacticalLocation = Unit.UeOffsetCm;
		if (Unit.CloseLodOffsetsCm.Num() == 3)
		{
			Visual.CloseLocation = Unit.CloseLodOffsetsCm[0];
			Visual.CloneALocation = Unit.CloseLodOffsetsCm[1];
			Visual.CloneBLocation = Unit.CloseLodOffsetsCm[2];
		}
		else
		{
			Visual.CloseLocation = Visual.TacticalLocation;
		}

		auto MakeUnitCard = [&](TWeakObjectPtr<UMaterialInstanceDynamic>&
			                   OutMaterial)
		{
			UTexture2D* FirstTexture = Visual.Frames[0];
			UMaterialInstanceDynamic* Material =
				MakeCardMaterial(FirstTexture);
			if (!Material)
			{
				return static_cast<UStaticMeshComponent*>(nullptr);
			}
			UStaticMeshComponent* Card =
				NewObject<UStaticMeshComponent>(this);
			ConfigureCardComponent(Card);
			Card->SetStaticMesh(CardMesh);
			Card->AttachToComponent(
				SceneRoot,
				FAttachmentTransformRules::KeepRelativeTransform);
			Card->SetMaterial(0, Material);
			Card->RegisterComponent();
			// Match Cell.InitDummy's runtime 45-degree body rotation.
			Card->SetRelativeRotation(UnitCardRotation());
			Card->SetRelativeScale3D(
				CardScale(Unit.WidthCm, Unit.HeightCm));
			OutMaterial = Material;
			return Card;
		};

		Visual.Component = MakeUnitCard(Visual.Material);
		if (!Visual.Component)
		{
			continue;
		}
		Visual.Component->SetRelativeLocation(Visual.TacticalLocation);
		if (Unit.CloseLodOffsetsCm.Num() == 3)
		{
			Visual.CloneA = MakeUnitCard(Visual.CloneMaterialA);
			Visual.CloneB = MakeUnitCard(Visual.CloneMaterialB);
			if (Visual.CloneA)
			{
				Visual.CloneA->SetRelativeLocation(
					Visual.CloneALocation);
			}
			if (Visual.CloneB)
			{
				Visual.CloneB->SetRelativeLocation(
					Visual.CloneBLocation);
			}
		}
		FormationUnitVisuals.Add(MoveTemp(Visual));
	}
	ApplyUnitScale(LastAppliedDxf);
}

void ARokWorldMapEntityActor::ApplyUnitScale(float Dxf)
{
	// LevelDetailScalerManager.m_unit_scale_curve: every unit multiplies
	// its native sprite dimensions by this LOD-dependent factor, and the
	// sprite pivot stays anchored on the dummy position as the card
	// grows/shrinks.
	const float UnitFactor = Record.UnitScaleCurveWithTangents.Eval(Dxf, 1.0f);
	const bool bCloseExpansion = ShouldExpandFormation(Dxf);
	for (FFormationUnitVisual& Unit : FormationUnitVisuals)
	{
		// Keep the card bottom above the ground; the baked sprite shadow
		// must not intersect the terrain when the tilted card grows.
		const float Lift =
			Unit.Unit.HeightCm * UnitFactor * 0.5f;
		const FVector SpriteOffset = UnitCardUp * Lift
			+ FVector::CrossProduct(UnitCardUp, UnitCardNormal)
				* ((0.5f - Unit.Unit.PivotX) * Unit.Unit.WidthCm * UnitFactor);
		const FVector& Base = bCloseExpansion
			? Unit.CloseLocation
			: Unit.TacticalLocation;
		if (Unit.Component)
		{
			Unit.Component->SetRelativeLocation(Base + SpriteOffset);
			Unit.Component->SetRelativeScale3D(CardScale(
				Unit.Unit.WidthCm * UnitFactor,
				Unit.Unit.HeightCm * UnitFactor));
		}
		if (Unit.CloneA)
		{
			Unit.CloneA->SetRelativeLocation(
				Unit.CloneALocation + SpriteOffset);
			Unit.CloneA->SetRelativeScale3D(CardScale(
				Unit.Unit.WidthCm * UnitFactor,
				Unit.Unit.HeightCm * UnitFactor));
		}
		if (Unit.CloneB)
		{
			Unit.CloneB->SetRelativeLocation(
				Unit.CloneBLocation + SpriteOffset);
			Unit.CloneB->SetRelativeScale3D(CardScale(
				Unit.Unit.WidthCm * UnitFactor,
				Unit.Unit.HeightCm * UnitFactor));
		}
	}
}

void ARokWorldMapEntityActor::AdvanceIdleAnimation(float DeltaSeconds)
{
	if (!bVisibleBandDetailed || FormationUnitVisuals.IsEmpty())
	{
		return;
	}
	for (FFormationUnitVisual& Unit : FormationUnitVisuals)
	{
		const TWeakObjectPtr<UMaterialInstanceDynamic> Materials[] =
			{ Unit.Material, Unit.CloneMaterialA, Unit.CloneMaterialB };
		for (int32 CardIndex = 0; CardIndex < 3; ++CardIndex)
		{
			UMaterialInstanceDynamic* Material = Materials[CardIndex].Get();
			if (!Material || (CardIndex > 0 && !ShouldExpandFormation(LastAppliedDxf)))
			{
				continue;
			}
			FRokWorldMapSpriteAnimation& Animation = Unit.Animation[CardIndex];
			const bool bFrameAdvanced = Animation.Advance(DeltaSeconds, Unit.Unit.IdleUpdateRate, Unit.Frames.Num());
			const bool bWasFading = Unit.FadeAmount[CardIndex] < 1.0f;
			if (bWasFading)
			{
				Unit.FadeAmount[CardIndex] = FRokWorldMapSpriteAnimation::AdvanceSpawnFade(Unit.FadeAmount[CardIndex], DeltaSeconds);
				Material->SetScalarParameterValue(TEXT("FadeAmount"), Unit.FadeAmount[CardIndex]);
				Material->SetScalarParameterValue(TEXT("SpriteAlphaMultiplier"), Unit.FadeAmount[CardIndex] < 1.0f ? 2.0f : 1.0f);
			}
			if (bFrameAdvanced || bWasFading)
			{
				Material->SetTextureParameterValue(TEXT("Sprite"), Unit.FadeAmount[CardIndex] < 1.0f
					? Unit.FadeFrames[Animation.Frame] : Unit.Frames[Animation.Frame]);
			}
		}
	}
}

void ARokWorldMapEntityActor::ApplyDxf(float Dxf)
{
	if (FMath::IsNearlyEqual(LastAppliedDxf, Dxf))
	{
		return;
	}
	const bool bWasDetailed = bVisibleBandDetailed;
	const bool bWereClonesVisible = bWasDetailed && ShouldExpandFormation(LastAppliedDxf);
	const bool bFormationLodChanged = FRokWorldMapSpriteAnimation::FormationLod(LastAppliedDxf)
		!= FRokWorldMapSpriteAnimation::FormationLod(Dxf);
	LastAppliedDxf = Dxf;
	const ERokWorldMapEntityLodBand NewBand = ResolveLodBand(
		Dxf,
		Record.CloseDetailMaxDxf,
		Record.TacticalDetailMaxDxf,
		Record.FarIconMaxDxf);
	CurrentLodBand = NewBand;
	const bool bShowDetailBand =
		NewBand == ERokWorldMapEntityLodBand::CloseDetail
		|| NewBand == ERokWorldMapEntityLodBand::TacticalDetail;
	const bool bShowFar =
		NewBand == ERokWorldMapEntityLodBand::FarIcon;
	const bool bShowCompositeDetail =
		bShowDetailBand && FormationUnitVisuals.IsEmpty() && !FoodDeposit && !WoodDeposit && !StoneDeposit && !VillagePresentation;
	if (FoodDeposit) FoodDeposit->UpdatePresentation(Dxf, bShowDetailBand);
	if (WoodDeposit) WoodDeposit->UpdatePresentation(Dxf, bShowDetailBand);
	if (StoneDeposit) StoneDeposit->UpdatePresentation(Dxf, bShowDetailBand);
	if (VillagePresentation) VillagePresentation->UpdatePresentation(Dxf, bShowDetailBand);
	// MatrixRow.UpdateUnitPositionBarabarianLod: soldier rows expand only
	// at Unity lod<=1. Troops m_lod_array is [501, 900, 99999], so lod<=1
	// means dxf <= 900 - the CloseDetail band (1500) alone is not the
	// trigger, and Unity's lod:1537 strategic view still shows the tight
	// base formation.
	const bool bCloseExpansion =
		bShowDetailBand && ShouldExpandFormation(Dxf);
	bVisibleBandDetailed = bShowDetailBand;
	for (FFormationUnitVisual& Unit : FormationUnitVisuals)
	{
		// MatrixRow.ResetUnit recreates all cards across troop LOD changes,
		// including the main cards when only clone visibility changes.
		if ((bShowDetailBand && (!bWasDetailed || bFormationLodChanged))
			|| (bCloseExpansion && (!bWereClonesVisible || bFormationLodChanged)))
		{
			FRandomStream Random(FMath::Rand());
			if (bShowDetailBand && (!bWasDetailed || bFormationLodChanged))
			{
				Unit.Animation[0].Reset(Random, Unit.Frames.Num(), Unit.Unit.IdleRandomOffset);
				Unit.FadeAmount[0] = 0.0f;
				if (UMaterialInstanceDynamic* Material = Unit.Material.Get())
				{
					Material->SetScalarParameterValue(TEXT("FadeAmount"), 0.0f);
					Material->SetScalarParameterValue(TEXT("SpriteAlphaMultiplier"), 2.0f);
					Material->SetTextureParameterValue(TEXT("Sprite"), Unit.FadeFrames[0]);
				}
			}
			if (bCloseExpansion && (!bWereClonesVisible || bFormationLodChanged))
			{
				Unit.Animation[1].Reset(Random, Unit.Frames.Num(), Unit.Unit.IdleRandomOffset);
				Unit.Animation[2].Reset(Random, Unit.Frames.Num(), Unit.Unit.IdleRandomOffset);
				const TWeakObjectPtr<UMaterialInstanceDynamic> CloneMaterials[] = { Unit.CloneMaterialA, Unit.CloneMaterialB };
				for (int32 CloneIndex = 0; CloneIndex < 2; ++CloneIndex)
				{
					Unit.FadeAmount[CloneIndex + 1] = 0.0f;
					if (UMaterialInstanceDynamic* Material = CloneMaterials[CloneIndex].Get())
					{
						Material->SetScalarParameterValue(TEXT("FadeAmount"), 0.0f);
						Material->SetScalarParameterValue(TEXT("SpriteAlphaMultiplier"), 2.0f);
						Material->SetTextureParameterValue(TEXT("Sprite"), Unit.FadeFrames[0]);
					}
				}
			}
		}
		if (Unit.Component)
		{
			Unit.Component->SetVisibility(bShowDetailBand, true);
			Unit.Component->SetHiddenInGame(!bShowDetailBand, true);
		}
		const bool bClonesVisible = bCloseExpansion;
		if (Unit.CloneA)
		{
			Unit.CloneA->SetVisibility(bClonesVisible, true);
			Unit.CloneA->SetHiddenInGame(!bClonesVisible, true);
		}
		if (Unit.CloneB)
		{
			Unit.CloneB->SetVisibility(bClonesVisible, true);
			Unit.CloneB->SetHiddenInGame(!bClonesVisible, true);
		}
	}
	ApplyUnitScale(Dxf);
	if (DetailVisual)
	{
		DetailVisual->SetVisibility(bShowCompositeDetail, true);
		DetailVisual->SetHiddenInGame(!bShowCompositeDetail, true);

		float ScaleMultiplier = 1.0f;
		if (NewBand == ERokWorldMapEntityLodBand::TacticalDetail)
		{
			const float Alpha = FMath::GetRangePct(
				Record.CloseDetailMaxDxf,
				Record.TacticalDetailMaxDxf,
				Dxf);
			ScaleMultiplier = FMath::Lerp(
				0.92f,
				1.22f,
				FMath::Clamp(Alpha, 0.0f, 1.0f));
		}
		DetailVisual->SetRelativeScale3D(CardScale(
			DetailBaseWidth * ScaleMultiplier,
			Record.DetailWorldHeightCm * ScaleMultiplier));
		DetailVisual->SetRelativeLocation(
			CardUp
			* (Record.DetailWorldHeightCm
				* ScaleMultiplier
				* 0.48f));
	}
	if (FarVisual)
	{
		FarVisual->SetVisibility(bShowFar, true);
		FarVisual->SetHiddenInGame(!bShowFar, true);

		if (bShowFar)
		{
			const float Alpha = FMath::GetRangePct(
				Record.TacticalDetailMaxDxf,
				Record.FarIconMaxDxf,
				Dxf);
			const float ScaleMultiplier = FMath::Lerp(
				1.0f,
				Record.FarEndScaleMultiplier,
				FMath::Clamp(Alpha, 0.0f, 1.0f));
			FarVisual->SetRelativeScale3D(CardScale(
				FarBaseWidth * ScaleMultiplier,
				Record.FarWorldHeightCm * ScaleMultiplier));
			FarVisual->SetRelativeLocation(
				-CardNormal
				* (Record.FarWorldHeightCm
					* ScaleMultiplier
					* 0.5f));
		}
	}
	SetActorHiddenInGame(NewBand == ERokWorldMapEntityLodBand::Hidden);
	SelectionCollision->SetCollisionEnabled(
		NewBand == ERokWorldMapEntityLodBand::Hidden
			? ECollisionEnabled::NoCollision
			: ECollisionEnabled::QueryOnly);
}
