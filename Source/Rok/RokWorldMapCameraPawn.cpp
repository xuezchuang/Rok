#include "RokWorldMapCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "RokWorldMapCityActor.h"
#include "RokWorldMapConfigActor.h"
#include "RokWorldMapPlayerController.h"

ARokWorldMapCameraPawn::ARokWorldMapCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);

	ActiveCameraKeys = FRokCityViewMath::MakeDefaultCameraKeys();
}

void ARokWorldMapCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	ResolveConfiguration();
	ApplyWorldEntryOptions();
	ResolveTreeVisuals();
	ResolveCityVisuals();
	ApplyCameraState();
}

void ARokWorldMapCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (UpdatePlayerCityEntry(DeltaSeconds))
	{
		return;
	}
	UpdateKeyboardPan(DeltaSeconds);
	UpdateDragPan();
	UpdateMouseWheelZoom();
}

void ARokWorldMapCameraPawn::SetViewCenterUnity(FVector2D NewViewCenterUnity)
{
	CameraState.ViewCenterUnity = GetProjection().ClampUnity(NewViewCenterUnity);
	ApplyCameraState();
}

FVector2D ARokWorldMapCameraPawn::GetViewCenterDisplay() const
{
	return GetProjection().UnityToDisplay(CameraState.ViewCenterUnity);
}

void ARokWorldMapCameraPawn::SetCurrentDxf(float NewDxf)
{
	CameraState.CurrentDxf = FRokWorldMapMath::ClampDxfByNamedKeys(
		ActiveCameraKeys,
		NewDxf,
		MinimumCameraKeyId,
		MaximumCameraKeyId);
	ApplyCameraState();
}

void ARokWorldMapCameraPawn::FocusDisplayCoordinate(FVector2D DisplayCoordinate)
{
	SetViewCenterUnity(GetProjection().DisplayToUnity(DisplayCoordinate));
}

const FRokWorldMapProjection& ARokWorldMapCameraPawn::GetProjection() const
{
	return RuntimeConfig ? RuntimeConfig->Projection : FallbackProjection;
}

FVector ARokWorldMapCameraPawn::GetUnityCameraForward() const
{
	return RuntimeConfig ? RuntimeConfig->UnityCameraForward : FallbackUnityCameraForward;
}

void ARokWorldMapCameraPawn::ResolveConfiguration()
{
	RuntimeConfig = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ARokWorldMapConfigActor> ConfigIt(World); ConfigIt; ++ConfigIt)
		{
			RuntimeConfig = *ConfigIt;
			break;
		}
	}

	if (RuntimeConfig)
	{
		ActiveCameraKeys = RuntimeConfig->CameraKeys;
		MinimumCameraKeyId = RuntimeConfig->MinimumCameraKeyId;
		MaximumCameraKeyId = RuntimeConfig->MaximumCameraKeyId;
		CameraState.ViewCenterUnity = RuntimeConfig->DefaultFocusUnity;
		CameraState.CurrentDxf = RuntimeConfig->GetDefaultDxf();
	}
	else
	{
		ActiveCameraKeys = FRokCityViewMath::MakeDefaultCameraKeys();
		MinimumCameraKeyId = FRokWorldMapMath::MinimumCameraKey();
		MaximumCameraKeyId = FRokWorldMapMath::MaximumCameraKey();
		CameraState.ViewCenterUnity = FallbackProjection.ClampUnity(FallbackFocusUnity);
		CameraState.CurrentDxf = FRokWorldMapMath::GetNamedDxf(
			ActiveCameraKeys,
			FRokWorldMapMath::TacticalCameraKey(),
			1509.0f);
	}

	ActiveCameraKeys.Sort([](const FRokCityViewCameraKey& Left, const FRokCityViewCameraKey& Right)
	{
		return Left.Dxf < Right.Dxf;
	});
	CameraState.CurrentDxf = FRokWorldMapMath::ClampDxfByNamedKeys(
		ActiveCameraKeys,
		CameraState.CurrentDxf,
		MinimumCameraKeyId,
		MaximumCameraKeyId);
	CameraState.ViewCenterUnity = GetProjection().ClampUnity(CameraState.ViewCenterUnity);
}

void ARokWorldMapCameraPawn::ApplyWorldEntryOptions()
{
	const UWorld* World = GetWorld();
	const TCHAR* EntryDxfOption =
		World
			? World->URL.GetOption(TEXT("RokWorldEntryDxf="), nullptr)
			: nullptr;
	if (EntryDxfOption)
	{
		const float EntryDxf = FCString::Atof(EntryDxfOption);
		if (!FMath::IsFinite(EntryDxf) || EntryDxf <= 0.0f)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Rok world-map entry ignored invalid dxf option: %s."),
				EntryDxfOption);
		}
		else
		{
			CameraState.CurrentDxf = FRokWorldMapMath::ClampDxfByNamedKeys(
				ActiveCameraKeys,
				EntryDxf,
				MinimumCameraKeyId,
				MaximumCameraKeyId);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok world-map entry initialized: requested_dxf=%.3f applied_dxf=%.3f."),
				EntryDxf,
				CameraState.CurrentDxf);
		}
	}

	const TCHAR* EntryXOption =
		World
			? World->URL.GetOption(TEXT("RokWorldEntryX="), nullptr)
			: nullptr;
	const TCHAR* EntryYOption =
		World
			? World->URL.GetOption(TEXT("RokWorldEntryY="), nullptr)
			: nullptr;
	if (EntryXOption && EntryYOption)
	{
		const FVector2D DisplayCoordinate(
			FCString::Atof(EntryXOption),
			FCString::Atof(EntryYOption));
		if (FMath::IsFinite(DisplayCoordinate.X)
			&& FMath::IsFinite(DisplayCoordinate.Y))
		{
			CameraState.ViewCenterUnity = GetProjection().ClampUnity(
				GetProjection().DisplayToUnity(DisplayCoordinate));
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok world-map entry focused display coordinate X=%.3f Y=%.3f."),
				DisplayCoordinate.X,
				DisplayCoordinate.Y);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Rok world-map entry ignored invalid display coordinate X=%s Y=%s."),
				EntryXOption,
				EntryYOption);
		}
	}
	else if (EntryXOption || EntryYOption)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world-map entry ignored incomplete display coordinate options."));
	}
}

float ARokWorldMapCameraPawn::EvaluateUnityTreeScale(float Dxf)
{
	struct FTreeScaleKey
	{
		float Dxf;
		float Scale;
		float InSlope;
		float OutSlope;
	};
	static constexpr FTreeScaleKey CurveKeys[] = {
		{80.0f, 0.23f, -0.000033767436f, 0.00027361486f},
		{250.0f, 0.4f, 0.0003291402f, 0.0015314083f},
		{896.1033f, 0.8f, 0.0006190961f, 0.00044234627f},
		{1800.3723f, 1.2f, 0.0007099115f, 0.0f},
	};

	if (!FMath::IsFinite(Dxf) || Dxf <= CurveKeys[0].Dxf)
	{
		return CurveKeys[0].Scale;
	}
	for (int32 KeyIndex = 1;
		KeyIndex < UE_ARRAY_COUNT(CurveKeys);
		++KeyIndex)
	{
		const FTreeScaleKey& Left = CurveKeys[KeyIndex - 1];
		const FTreeScaleKey& Right = CurveKeys[KeyIndex];
		if (Dxf > Right.Dxf)
		{
			continue;
		}

		const float Duration = Right.Dxf - Left.Dxf;
		const float T = FMath::Clamp(
			(Dxf - Left.Dxf) / Duration,
			0.0f,
			1.0f);
		const float T2 = T * T;
		const float T3 = T2 * T;
		const float H00 = 2.0f * T3 - 3.0f * T2 + 1.0f;
		const float H10 = T3 - 2.0f * T2 + T;
		const float H01 = -2.0f * T3 + 3.0f * T2;
		const float H11 = T3 - T2;
		return H00 * Left.Scale
			+ H10 * Duration * Left.OutSlope
			+ H01 * Right.Scale
			+ H11 * Duration * Right.InSlope;
	}

	return CurveKeys[UE_ARRAY_COUNT(CurveKeys) - 1].Scale;
}

void ARokWorldMapCameraPawn::ResolveTreeVisuals()
{
	TreeVisuals.Reset();
	LastAppliedTreeScaleRatio = -1.0f;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> ActorIterator(World);
		ActorIterator;
		++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (!Actor
			|| !Actor->Tags.Contains(
				FRokWorldMapMath::WorldMapSourceObjectTag()))
		{
			continue;
		}

		UStaticMeshComponent* StaticMeshComponent =
			Actor->FindComponentByClass<UStaticMeshComponent>();
		UMaterialInterface* Material =
			StaticMeshComponent
				? StaticMeshComponent->GetMaterial(0)
				: nullptr;
		if (!StaticMeshComponent
			|| !FRokWorldMapMath::IsWorldMapTree(
				Actor->Tags,
				Material ? Material->GetFName() : NAME_None))
		{
			continue;
		}

		StaticMeshComponent->TranslucencySortPriority = 6;
		StaticMeshComponent->MarkRenderStateDirty();
		FTreeVisual& TreeVisual = TreeVisuals.AddDefaulted_GetRef();
		TreeVisual.Actor = Actor;
		TreeVisual.ActorScaleAtTactical = Actor->GetActorScale3D();
		if (Actor->ActorHasTag(TEXT("RokGroveCenterScaleV1")))
		{
			// A tagged but malformed upgrade must not silently return to actor scaling.
			TreeVisual.CenterScaleComponent = StaticMeshComponent;
			UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
			float Parameter = 0.0f;
			if (!Actor->ActorHasTag(TEXT("UnityPrefab_Grove_co_tree_01_adorning"))
				|| !Mesh || Mesh->GetName() != TEXT("SM_RokGroveCenterScale")
				|| !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty()
				|| Mesh->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() != 3
				|| !Mesh->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs()
				|| !Material || !Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RokSourceTreeScale")), Parameter))
			{
				UE_LOG(LogTemp, Error, TEXT("Invalid exact Grove center transport: %s"), *Actor->GetName());
				TreeVisual.Actor.Reset();
				TreeVisual.CenterScaleComponent.Reset();
				continue;
			}
			if (!Cast<UMaterialInstanceDynamic>(Material))
			{
				// Keep the source material package prefix: continuous lighting discovers
				// these materials by that prefix, regardless of BeginPlay ordering.
				StaticMeshComponent->SetMaterial(0, UMaterialInstanceDynamic::Create(Material, Material));
			}
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map tree LOD initialized: components=%d dxf=%.3f source_scale=%.9f."),
		TreeVisuals.Num(),
		CameraState.CurrentDxf,
		EvaluateUnityTreeScale(CameraState.CurrentDxf));
}

void ARokWorldMapCameraPawn::ResolveCityVisuals()
{
	CityVisuals.Reset();
	LastAppliedCityDxf = -1.0f;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ARokWorldMapCityActor> CityIterator(World);
			CityIterator;
			++CityIterator)
		{
			CityVisuals.Add(*CityIterator);
		}
	}
}

void ARokWorldMapCameraPawn::ApplyCameraState()
{
	if (!CameraComponent || ActiveCameraKeys.IsEmpty())
	{
		return;
	}

	float ViewDistanceUnity = 0.0f;
	float VerticalFov = 0.0f;
	if (!FRokCityViewMath::GetDistFovByDxf(
		ActiveCameraKeys,
		CameraState.CurrentDxf,
		ViewDistanceUnity,
		VerticalFov))
	{
		return;
	}

	const FRokWorldMapProjection& Projection = GetProjection();
	const FVector CameraForward = FRokWorldMapMath::UnityDirectionToUe(GetUnityCameraForward());
	const FVector CameraTarget = Projection.UnityToUe(CameraState.ViewCenterUnity);
	const float ViewDistanceUe = ViewDistanceUnity * Projection.GetSafeUeScale();
	SetActorLocation(CameraTarget - CameraForward * ViewDistanceUe);
	SetActorRotation(CameraForward.Rotation());

	CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
	CameraComponent->AspectRatio = DefaultAspectRatio;
	CameraComponent->bConstrainAspectRatio = true;
	CameraComponent->SetFieldOfView(FRokCityViewMath::VerticalToHorizontalFov(VerticalFov, DefaultAspectRatio));
	CameraComponent->PostProcessSettings.bOverride_AutoExposureMethod = true;
	CameraComponent->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CameraComponent->PostProcessSettings.AutoExposureBias = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	CameraComponent->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	CameraComponent->PostProcessSettings.bOverride_ToneCurveAmount = true;
	CameraComponent->PostProcessSettings.ToneCurveAmount = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_ExpandGamut = true;
	CameraComponent->PostProcessSettings.ExpandGamut = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_BlueCorrection = true;
	CameraComponent->PostProcessSettings.BlueCorrection = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_BloomIntensity = true;
	CameraComponent->PostProcessSettings.BloomIntensity = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_VignetteIntensity = true;
	CameraComponent->PostProcessSettings.VignetteIntensity = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CameraComponent->PostProcessSettings.MotionBlurAmount = 0.0f;
	ApplyUnityTreeScale();
	ApplyUnityCityLod();
}

void ARokWorldMapCameraPawn::ApplyUnityTreeScale()
{
	const float TacticalScale = EvaluateUnityTreeScale(1509.0f);
	const float CurrentScale = EvaluateUnityTreeScale(CameraState.CurrentDxf);
	const float ScaleRatio =
		TacticalScale > UE_SMALL_NUMBER
			? CurrentScale / TacticalScale
			: 1.0f;
	const bool bRatioUnchanged = FMath::IsNearlyEqual(
			LastAppliedTreeScaleRatio,
			ScaleRatio);
	LastAppliedTreeScaleRatio = ScaleRatio;
	for (const FTreeVisual& TreeVisual : TreeVisuals)
	{
		if (UStaticMeshComponent* Component = TreeVisual.CenterScaleComponent.Get())
		{
			// Lighting may replace the MID after camera initialization. Always address
			// the currently rendered MID, not a stale cached parent instance.
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
			{
				MID->SetScalarParameterValue(TEXT("RokSourceTreeScale"), CurrentScale);
			}
			continue;
		}
		if (bRatioUnchanged) continue;
		if (AActor* TreeActor = TreeVisual.Actor.Get())
		{
			TreeActor->SetActorScale3D(
				TreeVisual.ActorScaleAtTactical * ScaleRatio);
		}
	}
}

void ARokWorldMapCameraPawn::ApplyUnityCityLod()
{
	if (FMath::IsNearlyEqual(
			LastAppliedCityDxf,
			CameraState.CurrentDxf))
	{
		return;
	}
	LastAppliedCityDxf = CameraState.CurrentDxf;
	for (const TWeakObjectPtr<ARokWorldMapCityActor>& CityVisual :
		CityVisuals)
	{
		if (ARokWorldMapCityActor* CityActor = CityVisual.Get())
		{
			CityActor->ApplyCameraDxf(CameraState.CurrentDxf);
		}
	}
}

void ARokWorldMapCameraPawn::UpdateKeyboardPan(float DeltaSeconds)
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	FVector2D Movement = FVector2D::ZeroVector;
	Movement.Y += PlayerController->IsInputKeyDown(EKeys::W) || PlayerController->IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f;
	Movement.Y -= PlayerController->IsInputKeyDown(EKeys::S) || PlayerController->IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f;
	Movement.X += PlayerController->IsInputKeyDown(EKeys::D) || PlayerController->IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f;
	Movement.X -= PlayerController->IsInputKeyDown(EKeys::A) || PlayerController->IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f;
	if (Movement.IsNearlyZero())
	{
		return;
	}

	const float SpeedScale = PlayerController->IsInputKeyDown(EKeys::LeftShift) ? FastPanMultiplier : 1.0f;
	PanByScreenDirection(Movement.GetSafeNormal(), DeltaSeconds, SpeedScale);
}

void ARokWorldMapCameraPawn::UpdateDragPan()
{
	if (!bEnableDragPan)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	const bool bDragButtonDown =
		PlayerController->IsInputKeyDown(EKeys::RightMouseButton)
		|| PlayerController->IsInputKeyDown(EKeys::MiddleMouseButton);
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!bDragButtonDown || !PlayerController->GetMousePosition(MouseX, MouseY))
	{
		bDragging = false;
		return;
	}

	const FVector2D CurrentScreenPosition(MouseX, MouseY);
	if (!bDragging)
	{
		bDragging = true;
		LastDragScreenPosition = CurrentScreenPosition;
		return;
	}

	FVector PreviousGround = FVector::ZeroVector;
	FVector CurrentGround = FVector::ZeroVector;
	if (TryIntersectGroundPlaneAtScreen(LastDragScreenPosition, PreviousGround)
		&& TryIntersectGroundPlaneAtScreen(CurrentScreenPosition, CurrentGround))
	{
		PanByUeWorldDelta(FVector2D(PreviousGround.X - CurrentGround.X, PreviousGround.Y - CurrentGround.Y));
	}
	LastDragScreenPosition = CurrentScreenPosition;
}

void ARokWorldMapCameraPawn::UpdateMouseWheelZoom()
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	const float ZoomDirection =
		(PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? -1.0f : 0.0f)
		+ (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1.0f : 0.0f);
	if (FMath::IsNearlyZero(ZoomDirection))
	{
		return;
	}

	ApplyZoomInput(ZoomDirection, true);
}

bool ARokWorldMapCameraPawn::ApplyZoomInput(
	float ZoomDirection,
	bool bPreserveCursor)
{
	const APlayerController* PlayerController =
		Cast<APlayerController>(GetController());
	if (!PlayerController
		|| bEnteringPlayerCity
		|| FMath::IsNearlyZero(ZoomDirection))
	{
		return false;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	FVector GroundBeforeZoom = FVector::ZeroVector;
	const bool bCanPreserveCursor =
		bPreserveCursor
		&& PlayerController->GetMousePosition(MouseX, MouseY)
		&& TryIntersectGroundPlaneAtScreen(FVector2D(MouseX, MouseY), GroundBeforeZoom);

	const float ZoomMultiplier = ZoomDirection < 0.0f ? (1.0f / ZoomFactorPerNotch) : ZoomFactorPerNotch;
	const float RequestedDxf = CameraState.CurrentDxf * ZoomMultiplier;
	if (TryBeginPlayerCityEntry(RequestedDxf))
	{
		return true;
	}
	// Unity's WorldCameraImpl raises the wheel-zoom floor away from the
	// player city (GlobalViewLevelMediator pushes setAdditionHeightForMinDxf
	// toward CityBoundDxf=500), so a resting world view never parks in the
	// sparse dxf 500-1100 transit band; the tactical states start at
	// "FTE_Tactics" (1110) where m_unit_scale_curve renders the dense mass.
	// Near/over the player city the floor stays open so the existing
	// zoom-into-city flow (threshold 250) keeps working.
	const bool bPlayerCityVisible = IsPlayerCityInViewport(FindPlayerCity());
	CameraState.CurrentDxf = FRokWorldMapMath::ClampDxfByNamedKeys(
		ActiveCameraKeys,
		RequestedDxf,
		bPlayerCityVisible ? MinimumCameraKeyId : UserZoomMinimumCameraKeyId,
		MaximumCameraKeyId);
	ApplyCameraState();

	FVector GroundAfterZoom = FVector::ZeroVector;
	if (bCanPreserveCursor
		&& TryIntersectGroundPlaneAtScreen(FVector2D(MouseX, MouseY), GroundAfterZoom))
	{
		PanByUeWorldDelta(FVector2D(
			GroundBeforeZoom.X - GroundAfterZoom.X,
			GroundBeforeZoom.Y - GroundAfterZoom.Y));
	}
	return false;
}

bool ARokWorldMapCameraPawn::TryBeginPlayerCityEntry(float RequestedDxf)
{
	if (bEnteringPlayerCity
		|| (RuntimeConfig && !RuntimeConfig->bEnterPlayerCityOnZoom))
	{
		return false;
	}

	const float ThresholdDxf =
		RuntimeConfig
			? RuntimeConfig->GetCityEntryThresholdDxf()
			: FRokWorldMapMath::GetNamedDxf(
				ActiveCameraKeys,
				FRokWorldMapMath::CityEntryThresholdCameraKey(),
				250.0f);
	if (!FRokWorldMapMath::ShouldBeginPlayerCityEntry(
			CameraState.CurrentDxf,
			RequestedDxf,
			ThresholdDxf))
	{
		return false;
	}

	ARokWorldMapCityActor* PlayerCity = FindPlayerCity();
	if (!PlayerCity || !IsPlayerCityInViewport(PlayerCity))
	{
		return false;
	}

	PlayerCityEntryStartCenterUnity = CameraState.ViewCenterUnity;
	PlayerCityEntryTargetCenterUnity = GetProjection().ClampUnity(
		GetProjection().UeToUnity(PlayerCity->GetActorLocation()));
	PlayerCityEntryStartDxf = CameraState.CurrentDxf;
	PlayerCityEntryTargetDxf =
		RuntimeConfig
			? RuntimeConfig->GetCityEntryTargetDxf()
			: FRokWorldMapMath::ClampDxfByNamedKeys(
				ActiveCameraKeys,
				FRokWorldMapMath::GetNamedDxf(
					ActiveCameraKeys,
					FRokWorldMapMath::CityEntryTargetCameraKey(),
					150.0f),
				MinimumCameraKeyId,
				MaximumCameraKeyId);
	PlayerCityEntryDurationSeconds =
		FMath::Max(
			RuntimeConfig
				? RuntimeConfig->CityEntryTransitionSeconds
				: 0.5f,
			0.05f);
	PlayerCityEntryElapsedSeconds = 0.0f;
	bEnteringPlayerCity = true;
	bDragging = false;

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map player-city entry started: actor=%s current_dxf=%.3f threshold_dxf=%.3f target_dxf=%.3f duration=%.3f."),
		*PlayerCity->GetActorNameOrLabel(),
		PlayerCityEntryStartDxf,
		ThresholdDxf,
		PlayerCityEntryTargetDxf,
		PlayerCityEntryDurationSeconds);
	return true;
}

bool ARokWorldMapCameraPawn::UpdatePlayerCityEntry(float DeltaSeconds)
{
	if (!bEnteringPlayerCity)
	{
		return false;
	}

	PlayerCityEntryElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
	const float LinearAlpha = FMath::Clamp(
		PlayerCityEntryElapsedSeconds / PlayerCityEntryDurationSeconds,
		0.0f,
		1.0f);
	const float SmoothAlpha =
		LinearAlpha * LinearAlpha * (3.0f - 2.0f * LinearAlpha);
	CameraState.ViewCenterUnity = FMath::Lerp(
		PlayerCityEntryStartCenterUnity,
		PlayerCityEntryTargetCenterUnity,
		SmoothAlpha);
	CameraState.CurrentDxf = FMath::Lerp(
		PlayerCityEntryStartDxf,
		PlayerCityEntryTargetDxf,
		SmoothAlpha);
	ApplyCameraState();

	if (LinearAlpha < 1.0f)
	{
		return true;
	}

	bEnteringPlayerCity = false;
	const TSoftObjectPtr<UWorld> PlayerCityLevel =
		RuntimeConfig
			? RuntimeConfig->PlayerCityLevel
			: TSoftObjectPtr<UWorld>(
				FSoftObjectPath(
					TEXT("/Game/Maps/L_RokUnityGameAppCity.L_RokUnityGameAppCity")));
	ARokWorldMapPlayerController* PlayerController =
		Cast<ARokWorldMapPlayerController>(GetController());
	const bool bTravelRequested =
		PlayerController
		&& PlayerController->RequestEnterPlayerCity(
			PlayerCityLevel,
			PlayerCityEntryTargetDxf);
	if (bTravelRequested)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map player-city entry handoff: requested=true level=%s entry_dxf=%.3f."),
			*PlayerCityLevel.ToSoftObjectPath().ToString(),
			PlayerCityEntryTargetDxf);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok world-map player-city entry handoff: requested=false level=%s entry_dxf=%.3f."),
			*PlayerCityLevel.ToSoftObjectPath().ToString(),
			PlayerCityEntryTargetDxf);
	}
	return true;
}

ARokWorldMapCityActor* ARokWorldMapCameraPawn::FindPlayerCity() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ARokWorldMapCityActor> CityIterator(World);
		CityIterator;
		++CityIterator)
	{
		if (CityIterator->Tags.Contains(
				FRokWorldMapMath::WorldMapPlayerCityTag()))
		{
			return *CityIterator;
		}
	}
	return nullptr;
}

bool ARokWorldMapCameraPawn::IsPlayerCityInViewport(
	const ARokWorldMapCityActor* PlayerCity) const
{
	const APlayerController* PlayerController =
		Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerCity || PlayerCity->IsHidden())
	{
		return false;
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	return ViewportSizeX > 0
		&& ViewportSizeY > 0
		&& PlayerController->ProjectWorldLocationToScreen(
			PlayerCity->GetActorLocation(),
			ScreenPosition,
			true)
		&& ScreenPosition.X >= 0.0f
		&& ScreenPosition.X <= static_cast<float>(ViewportSizeX)
		&& ScreenPosition.Y >= 0.0f
		&& ScreenPosition.Y <= static_cast<float>(ViewportSizeY);
}

void ARokWorldMapCameraPawn::PanByScreenDirection(
	const FVector2D& ScreenDirection,
	float DeltaSeconds,
	float SpeedScale)
{
	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();
	FVector Right = GetActorRightVector();
	Right.Z = 0.0f;
	Right.Normalize();

	const float ZoomSpeedScale = FMath::Sqrt(FMath::Max(CameraState.CurrentDxf / 1509.0f, 0.1f));
	const float DistanceUe =
		PanSpeedUnityUnitsPerSecond
		* GetProjection().GetSafeUeScale()
		* DeltaSeconds
		* SpeedScale
		* ZoomSpeedScale;
	const FVector DeltaUe = (Forward * ScreenDirection.Y + Right * ScreenDirection.X) * DistanceUe;
	PanByUeWorldDelta(FVector2D(DeltaUe.X, DeltaUe.Y));
}

void ARokWorldMapCameraPawn::PanByUeWorldDelta(const FVector2D& UeWorldDelta)
{
	const FRokWorldMapProjection& Projection = GetProjection();
	const FVector CurrentCenterUe = Projection.UnityToUe(CameraState.ViewCenterUnity);
	const FVector ShiftedCenterUe(
		CurrentCenterUe.X + UeWorldDelta.X,
		CurrentCenterUe.Y + UeWorldDelta.Y,
		Projection.GroundZ);
	CameraState.ViewCenterUnity = Projection.ClampUnity(Projection.UeToUnity(ShiftedCenterUe));
	ApplyCameraState();
}

bool ARokWorldMapCameraPawn::TryIntersectGroundPlaneAtScreen(
	const FVector2D& ScreenPosition,
	FVector& OutGroundPoint) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return false;
	}

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!PlayerController->DeprojectScreenPositionToWorld(
		ScreenPosition.X,
		ScreenPosition.Y,
		RayOrigin,
		RayDirection)
		|| FMath::Abs(RayDirection.Z) <= UE_SMALL_NUMBER)
	{
		return false;
	}

	const float RayDistance = (GetProjection().GroundZ - RayOrigin.Z) / RayDirection.Z;
	if (RayDistance < 0.0f)
	{
		return false;
	}
	OutGroundPoint = RayOrigin + RayDirection * RayDistance;
	return true;
}
