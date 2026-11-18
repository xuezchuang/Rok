#include "RokStrategyCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "RokCityViewTypes.h"
#include "RokMapPointerGesture.h"
#include "RokPlayerController.h"
#include "Components/SceneComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Engine/GameInstance.h"
#include "RokMapTravelSubsystem.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ARokStrategyCameraPawn::ARokStrategyCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);

	WorldMapLevel = TSoftObjectPtr<UWorld>(
		FSoftObjectPath(
			TEXT("/Game/Maps/L_RokUnityWorldMapPrototype.L_RokUnityWorldMapPrototype")));
}

void ARokStrategyCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyCityEntryOptions();
	ApplyCommandLineCameraDistanceOverride();
	ApplyCameraDefaults();

	// One-shot framing record for acceptance runs and comparison captures: the
	// starting zoom, the FOV the camera keeps, and the aspect behaviour.
	if (CameraComponent)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok city camera initialized: distance=%.3f dxf=%.3f vertical_fov=%.3f aspect=%.4f constrain_aspect=%d axis=%d."),
			CurrentCameraDistance,
			GetCurrentCityDxf(),
			CurrentVerticalFov,
			CameraComponent->AspectRatio,
			CameraComponent->bConstrainAspectRatio ? 1 : 0,
			static_cast<int32>(CameraComponent->AspectRatioAxisConstraint));
	}

	float AutomationReturnDelaySeconds = 0.0f;
	if (FApp::IsUnattended()
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("RokMapFlowAutomation"))
		&& FParse::Value(
			FCommandLine::Get(),
			TEXT("RokCityMapAutomationReturnDelaySeconds="),
			AutomationReturnDelaySeconds)
		&& AutomationReturnDelaySeconds > 0.0f)
	{
		AutomationWorldReturnNextStepSeconds =
			FPlatformTime::Seconds()
			+ FMath::Max(
				static_cast<double>(AutomationReturnDelaySeconds),
				0.25);
		AutomationWorldReturnStepCount = 0;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok main-city unattended world-map return scheduled after %.2f wall-clock seconds."),
			AutomationReturnDelaySeconds);
	}
}

void ARokStrategyCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const auto* RokController = Cast<ARokPlayerController>(GetController());
	if (!bCityPlacementDragInputSuspended && (!RokController || !RokController->IsCityCameraInputBlocked()))
	{
		UpdateKeyboardPan(DeltaSeconds);
		UpdateEdgePan(DeltaSeconds);
		UpdateMouseWheelZoom(DeltaSeconds);
	}

	if (AutomationWorldReturnNextStepSeconds > 0.0
		&& FPlatformTime::Seconds()
			>= AutomationWorldReturnNextStepSeconds)
	{
		DriveAutomationWorldMapReturn();
	}
}

void ARokStrategyCameraPawn::SetCityPlacementDragInputSuspended(bool bSuspended)
{
	bCityPlacementDragInputSuspended = bSuspended;
	if (bSuspended)
	{
		LastEdgeScrollDirection = FVector2D::ZeroVector;
	}
}

float ARokStrategyCameraPawn::GetCurrentCityDxf() const
{
	// Unity CameraParamLod and LevelDetailCastle use dxf = distance * vertical FOV.
	if (!FMath::IsFinite(CurrentCameraDistance) || !FMath::IsFinite(CurrentVerticalFov))
	{
		return 150.0f;
	}
	return CurrentCameraDistance / FMath::Max(UeUnitsPerUnityUnit, 1.0f) * CurrentVerticalFov;
}

float ARokStrategyCameraPawn::CalculateCameraDistanceForDxf(
	float Dxf,
	float VerticalFov,
	float UnitsPerUnityUnit)
{
	if (!FMath::IsFinite(Dxf)
		|| !FMath::IsFinite(VerticalFov)
		|| !FMath::IsFinite(UnitsPerUnityUnit)
		|| Dxf <= 0.0f
		|| VerticalFov <= UE_SMALL_NUMBER
		|| UnitsPerUnityUnit <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return Dxf / VerticalFov * UnitsPerUnityUnit;
}

bool ARokStrategyCameraPawn::ShouldReturnToWorldMap(
	float CurrentDxf,
	float RequestedDxf,
	float ReturnThresholdDxf)
{
	return FMath::IsFinite(CurrentDxf)
		&& FMath::IsFinite(RequestedDxf)
		&& FMath::IsFinite(ReturnThresholdDxf)
		&& ReturnThresholdDxf > 0.0f
		&& RequestedDxf > CurrentDxf
		&& RequestedDxf >= ReturnThresholdDxf;
}

float ARokStrategyCameraPawn::GetDefaultCityDxf()
{
	return FRokCityViewMath::GetCameraDxfById(
		FRokCityViewMath::MakeDefaultCameraKeys(),
		TEXT("city_default"),
		150.0f);
}

float ARokStrategyCameraPawn::GetCityMinDxf()
{
	return FRokCityViewMath::GetCameraDxfById(
		FRokCityViewMath::MakeDefaultCameraKeys(),
		TEXT("min"),
		82.4551f);
}

float ARokStrategyCameraPawn::GetCityMaxDxf()
{
	return FRokCityViewMath::GetCameraDxfById(
		FRokCityViewMath::MakeDefaultCameraKeys(),
		TEXT("city_bound"),
		250.0f);
}

float ARokStrategyCameraPawn::ClampCityDxf(float Dxf)
{
	if (!FMath::IsFinite(Dxf))
	{
		return GetDefaultCityDxf();
	}
	return FMath::Clamp(Dxf, GetCityMinDxf(), GetCityMaxDxf());
}

float ARokStrategyCameraPawn::CalculateWheelZoomDxf(float CurrentDxf, float ZoomDirection)
{
	if (!FMath::IsFinite(CurrentDxf) || CurrentDxf <= 0.0f || FMath::IsNearlyZero(ZoomDirection))
	{
		return CurrentDxf;
	}

	// Unity Skyunion InputManager feeds the wheel as scrollRate = 1/(mouseWheel
	// + 1) with mouseWheel = -0.1 to zoom out and +0.1 to zoom in (axis
	// sensitivity 0.1), i.e. dxf *= 10/9 out and dxf *= 10/11 in. Unity applies
	// the new dxf immediately; there is no time-based smoothing to mirror.
	const float WheelFactor = ZoomDirection > 0.0f ? 10.0f / 9.0f : 10.0f / 11.0f;
	return CurrentDxf * WheelFactor;
}

void ARokStrategyCameraPawn::ApplyCityCameraProjection(
	UCameraComponent* Camera,
	float VerticalFovDegrees,
	float ReferenceAspectRatio)
{
	if (!Camera
		|| !FMath::IsFinite(VerticalFovDegrees)
		|| VerticalFovDegrees <= 0.0f
		|| !FMath::IsFinite(ReferenceAspectRatio)
		|| ReferenceAspectRatio <= 0.0f)
	{
		return;
	}

	// Unity writes the vertical FOV and never letterboxes the city view
	// (WorldCamera.cs:704; no aspect constraint anywhere in the camera path).
	// UE reads FieldOfView as horizontal at CameraComponent->AspectRatio and,
	// with AspectRatio_MaintainYFOV, converts it back through that same aspect
	// ratio before fixing the vertical axis
	// (CameraStackTypes.cpp:326-333), so the vertical FOV is preserved while a
	// wider window widens the horizontal FOV instead of adding black bars.
	Camera->AspectRatio = ReferenceAspectRatio;
	Camera->bConstrainAspectRatio = false;
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->AspectRatioAxisConstraint =
		EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
	Camera->SetFieldOfView(
		FRokCityViewMath::VerticalToHorizontalFov(
			VerticalFovDegrees,
			ReferenceAspectRatio));
}

bool ARokStrategyCameraPawn::RequestReturnToWorldMap()
{
	// CityGlobalMediator.ExitCity uses map_tactical for the HUD button.
	return RequestReturnToWorldMapInternal({}, 0, true);
}

bool ARokStrategyCameraPawn::RequestReturnToWorldMapAtDisplayCoordinate(
	FVector2D TargetDisplayCoordinate)
{
	if (!FMath::IsFinite(TargetDisplayCoordinate.X)
		|| !FMath::IsFinite(TargetDisplayCoordinate.Y))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok main-city coordinate travel ignored invalid target X=%f Y=%f."),
			TargetDisplayCoordinate.X,
			TargetDisplayCoordinate.Y);
		return false;
	}

	return RequestReturnToWorldMapInternal(TargetDisplayCoordinate);
}

bool ARokStrategyCameraPawn::RequestWorldBarbarianSearch(int32 Level)
{
	return Level >= 1 && Level <= 25 && RequestReturnToWorldMapInternal({}, Level);
}

bool ARokStrategyCameraPawn::RequestReturnToWorldMapInternal(
	const TOptional<FVector2D>& TargetDisplayCoordinate, int32 BarbarianLevel, bool bTacticalEntry)
{
	if (bWorldMapTravelRequested || WorldMapLevel.IsNull())
	{
		return false;
	}

	bWorldMapTravelRequested = true;
	const float EntryDxf = bTacticalEntry || BarbarianLevel > 0
		? FRokCityViewMath::GetCameraDxfById(FRokCityViewMath::MakeDefaultCameraKeys(), TEXT("map_tactical"), 1509.0f)
		: WorldMapEntryDxf;
	FString Options = FString::Printf(TEXT("RokWorldEntryDxf=%.3f"), EntryDxf);
	if (BarbarianLevel > 0)
	{
		Options += FString::Printf(TEXT("?RokTaskBarbarianLevel=%d"), BarbarianLevel);
	}
	if (TargetDisplayCoordinate.IsSet())
	{
		const FVector2D Target = TargetDisplayCoordinate.GetValue();
		Options += FString::Printf(
			TEXT("?RokWorldEntryX=%.3f?RokWorldEntryY=%.3f"),
			Target.X,
			Target.Y);
	}
	if (FApp::IsUnattended()
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("RokMapFlowAutomation")))
	{
		Options += TEXT("?RokMapFlowCompleted=1");
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok main-city return to world map requested: level=%s entry_dxf=%.3f target=%s."),
		*WorldMapLevel.ToSoftObjectPath().ToString(),
		EntryDxf,
		TargetDisplayCoordinate.IsSet()
			? *TargetDisplayCoordinate.GetValue().ToString()
			: TEXT("none"));
	auto* Travel = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokMapTravelSubsystem>() : nullptr;
	if (!Travel || !Travel->RequestTravel(GetWorld(), WorldMapLevel, Options,
		FSimpleDelegate::CreateWeakLambda(this, [this]() { bWorldMapTravelRequested = false; })))
	{
		bWorldMapTravelRequested = false;
		return false;
	}
	return true;
}

void ARokStrategyCameraPawn::UpdateKeyboardPan(float DeltaSeconds)
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

	if (!Movement.IsNearlyZero())
	{
		const float SpeedScale = PlayerController->IsInputKeyDown(EKeys::LeftShift) ? FastMoveMultiplier : 1.0f;
		ApplyPlanarMovement(Movement.GetSafeNormal() * SpeedScale, DeltaSeconds);
	}
}

void ARokStrategyCameraPawn::UpdateEdgePan(float DeltaSeconds)
{
	if (!bEnableEdgeScroll)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return;
	}
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		if (bContinueEdgeScrollOutsideViewport && !LastEdgeScrollDirection.IsNearlyZero())
		{
			const float SavedMoveSpeed = MoveSpeed;
			MoveSpeed = EdgeScrollSpeed;
			ApplyPlanarMovement(LastEdgeScrollDirection, DeltaSeconds);
			MoveSpeed = SavedMoveSpeed;
		}
		return;
	}

	FVector2D Movement = FVector2D::ZeroVector;
	Movement.X -= MouseX <= EdgeScrollMargin ? 1.0f : 0.0f;
	Movement.X += MouseX >= static_cast<float>(ViewportSizeX) - EdgeScrollMargin ? 1.0f : 0.0f;
	Movement.Y += MouseY <= EdgeScrollMargin ? 1.0f : 0.0f;
	Movement.Y -= MouseY >= static_cast<float>(ViewportSizeY) - EdgeScrollMargin ? 1.0f : 0.0f;

	if (!Movement.IsNearlyZero())
	{
		LastEdgeScrollDirection = Movement.GetSafeNormal();
		const float SavedMoveSpeed = MoveSpeed;
		MoveSpeed = EdgeScrollSpeed;
		ApplyPlanarMovement(LastEdgeScrollDirection, DeltaSeconds);
		MoveSpeed = SavedMoveSpeed;
	}
	else
	{
		LastEdgeScrollDirection = FVector2D::ZeroVector;
	}
}

void ARokStrategyCameraPawn::UpdateMouseWheelZoom(float DeltaSeconds)
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

	ApplyZoomInput(ZoomDirection, DeltaSeconds);
}

bool ARokStrategyCameraPawn::ApplyZoomInput(
	float ZoomDirection,
	float DeltaSeconds)
{
	if (bCityPlacementDragInputSuspended || bWorldMapTravelRequested
		|| FMath::IsNearlyZero(ZoomDirection)
		|| !FMath::IsFinite(DeltaSeconds)
		|| DeltaSeconds <= 0.0f)
	{
		return false;
	}

	// One call is one wheel notch: the requested dxf is the current dxf times
	// the Unity wheel factor, so the zoom step is frame-rate independent.
	const float CurrentDxf = GetCurrentCityDxf();
	const float RequestedDxf = CalculateWheelZoomDxf(CurrentDxf, ZoomDirection);
	if (bReturnToWorldMapOnZoomOut
		&& ShouldReturnToWorldMap(
			CurrentDxf,
			RequestedDxf,
			WorldMapReturnThresholdDxf)
		&& RequestReturnToWorldMapInternal({}))
	{
		return true;
	}

	const float ClampedDxf = ClampCityDxf(RequestedDxf);
	DefaultCameraDistance = CalculateCameraDistanceForDxf(
		ClampedDxf,
		CurrentVerticalFov,
		UeUnitsPerUnityUnit);
	ApplyCameraDefaults();
	return false;
}

void ARokStrategyCameraPawn::DriveAutomationWorldMapReturn()
{
	const float DxfBeforeZoom = GetCurrentCityDxf();
	const bool bReturnStarted = ApplyZoomInput(1.0f, 0.05f);
	++AutomationWorldReturnStepCount;
	if (bReturnStarted)
	{
		AutomationWorldReturnNextStepSeconds = 0.0;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok main-city automation world-map return: result=started steps=%d current_dxf=%.3f."),
			AutomationWorldReturnStepCount,
			DxfBeforeZoom);
		return;
	}

	const float DxfAfterZoom = GetCurrentCityDxf();
	if (AutomationWorldReturnStepCount >= 64
		|| DxfAfterZoom <= DxfBeforeZoom + UE_SMALL_NUMBER)
	{
		AutomationWorldReturnNextStepSeconds = 0.0;
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok main-city automation world-map return: result=failed reason=zoom_stalled steps=%d before_dxf=%.3f after_dxf=%.3f."),
			AutomationWorldReturnStepCount,
			DxfBeforeZoom,
			DxfAfterZoom);
		return;
	}

	AutomationWorldReturnNextStepSeconds =
		FPlatformTime::Seconds() + 0.05;
}

void ARokStrategyCameraPawn::ApplyPlanarMovement(const FVector2D& Movement, float DeltaSeconds)
{
	const FRotator YawRotation(0.0f, GetActorRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector Delta = (Forward * Movement.Y + Right * Movement.X) * MoveSpeed * DeltaSeconds;

	PanByWorldDelta(Delta);
}

void ARokStrategyCameraPawn::PanFromScreenDrag(FVector2D Previous, FVector2D Current)
{
	const auto* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || bCityPlacementDragInputSuspended || bWorldMapTravelRequested)
	{
		return;
	}

	FVector PreviousGround, CurrentGround;
	if (FRokMapPointerGesture::ProjectToGround(*PlayerController, Previous, CurrentCameraTarget.Z, PreviousGround)
		&& FRokMapPointerGesture::ProjectToGround(*PlayerController, Current, CurrentCameraTarget.Z, CurrentGround))
	{
		PanByWorldDelta(PreviousGround - CurrentGround);
	}
}

void ARokStrategyCameraPawn::PanByWorldDelta(const FVector& Delta)
{
	// Clamp the point on the ground, not the offset camera behind it.
	const FVector OldTarget = CurrentCameraTarget;
	CurrentCameraTarget.X = FMath::Clamp(OldTarget.X + Delta.X, -MapExtent, MapExtent);
	CurrentCameraTarget.Y = FMath::Clamp(OldTarget.Y + Delta.Y, -MapExtent, MapExtent);
	AddActorWorldOffset(CurrentCameraTarget - OldTarget);
}

void ARokStrategyCameraPawn::FocusCityGroundPoint(const FVector& WorldPoint)
{
	if (WorldPoint.ContainsNaN() || !FMath::IsFinite(CurrentCameraDistance) || CurrentCameraDistance <= 0.0f)
	{
		return;
	}
	CurrentCameraTarget = WorldPoint;
	SetActorLocation(CurrentCameraTarget - GetActorForwardVector() * CurrentCameraDistance);
}

void ARokStrategyCameraPawn::FrameCityCenter()
{
	// Unity btn_city (MainInterfaceMediator.SetCityButton): return to the city's
	// default framing after the camera has drifted outside the city band.
	CurrentCameraTarget = DefaultCameraTarget;
	const float VerticalFov =
		FMath::IsFinite(CurrentVerticalFov) && CurrentVerticalFov > UE_SMALL_NUMBER
			? CurrentVerticalFov
			: DefaultVerticalFov;
	const float Distance = CalculateCameraDistanceForDxf(
		GetDefaultCityDxf(),
		VerticalFov,
		UeUnitsPerUnityUnit);
	if (Distance > 0.0f)
	{
		DefaultCameraDistance = Distance;
		ApplyCameraDefaults();
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok city camera focus requested: dxf=%.3f distance=%.3f."),
		GetCurrentCityDxf(),
		CurrentCameraDistance);
}

void ARokStrategyCameraPawn::ApplyCityEntryOptions()
{
	const UWorld* World = GetWorld();
	const TCHAR* EntryDxfOption =
		World
			? World->URL.GetOption(TEXT("RokCityEntryDxf="), nullptr)
			: nullptr;
	if (!EntryDxfOption)
	{
		return;
	}

	const float EntryDxf = FCString::Atof(EntryDxfOption);
	if (!FMath::IsFinite(EntryDxf) || EntryDxf <= 0.0f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok city entry camera ignored invalid dxf option: %s."),
			EntryDxfOption);
		return;
	}
	const float ClampedEntryDxf = ClampCityDxf(EntryDxf);
	const float EntryDistance = CalculateCameraDistanceForDxf(
		ClampedEntryDxf,
		DefaultVerticalFov,
		UeUnitsPerUnityUnit);
	if (EntryDistance <= 0.0f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok city entry camera ignored invalid dxf option: %s."),
			EntryDxfOption);
		return;
	}

	DefaultCameraDistance = EntryDistance;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok city entry camera initialized: entry_dxf=%.3f clamped_dxf=%.3f distance=%.3f."),
		EntryDxf,
		ClampedEntryDxf,
		DefaultCameraDistance);
	if (!FMath::IsNearlyEqual(EntryDxf, ClampedEntryDxf, 0.001f))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok city entry camera clamped entry dxf %.3f into the city range %.3f..%.3f."),
			EntryDxf,
			GetCityMinDxf(),
			GetCityMaxDxf());
	}
}

void ARokStrategyCameraPawn::ApplyCommandLineCameraDistanceOverride()
{
	float CommandLineDistance = 0.0f;
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("RokCameraDistance="),
			CommandLineDistance))
	{
		return;
	}
	if (!FMath::IsFinite(CommandLineDistance) || CommandLineDistance <= 0.0f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok camera ignored invalid -RokCameraDistance=%.3f."),
			CommandLineDistance);
		return;
	}

	// Capture/debug override. It is accepted verbatim, including dxf levels
	// outside the interactive city range (for example the 313.6 zoomed-out
	// acceptance shot), so a framing can be reproduced without the gameplay
	// clamp. It is applied once, so wheel zoom still runs from this start.
	DefaultCameraDistance = CommandLineDistance;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok camera distance override: distance=%.3f dxf=%.3f at fov %.3f (interactive city clamp %.3f..%.3f bypassed)."),
		CommandLineDistance,
		CommandLineDistance / FMath::Max(UeUnitsPerUnityUnit, 1.0f) * DefaultVerticalFov,
		DefaultVerticalFov,
		GetCityMinDxf(),
		GetCityMaxDxf());
}

void ARokStrategyCameraPawn::ApplyCameraDefaults()
{
	if (!CameraComponent)
	{
		return;
	}

	float RuntimeDistance = DefaultCameraDistance;
	float RuntimeYaw = DefaultCameraYaw;
	float RuntimePitch = DefaultCameraPitch;
	float RuntimeVerticalFov = DefaultVerticalFov;
	FParse::Value(FCommandLine::Get(), TEXT("RokCameraYaw="), RuntimeYaw);
	FParse::Value(FCommandLine::Get(), TEXT("RokCameraPitch="), RuntimePitch);
	FParse::Value(FCommandLine::Get(), TEXT("RokCameraVerticalFov="), RuntimeVerticalFov);
	if (!FMath::IsFinite(RuntimeDistance) || RuntimeDistance <= 0.0f)
	{
		RuntimeDistance = CalculateCameraDistanceForDxf(
			GetDefaultCityDxf(),
			FMath::IsFinite(RuntimeVerticalFov) && RuntimeVerticalFov > 0.0f
				? RuntimeVerticalFov
				: DefaultVerticalFov,
			UeUnitsPerUnityUnit);
	}
	CurrentCameraDistance = RuntimeDistance;
	CurrentVerticalFov = RuntimeVerticalFov;

	const FRotator RuntimeRotation(RuntimePitch, RuntimeYaw, 0.0f);
	const FVector RuntimeForward = RuntimeRotation.Vector();
	SetActorLocation(CurrentCameraTarget - RuntimeForward * RuntimeDistance);
	SetActorRotation(RuntimeRotation);

	CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
	ApplyCityCameraProjection(
		CameraComponent,
		RuntimeVerticalFov,
		DefaultAspectRatio);
	float RuntimeOrthoWidth = DefaultOrthoWidth;
	FParse::Value(FCommandLine::Get(), TEXT("RokOrthoWidth="), RuntimeOrthoWidth);
	CameraComponent->SetOrthoWidth(FMath::Clamp(RuntimeOrthoWidth, MinOrthoWidth, MaxOrthoWidth));
	CameraComponent->PostProcessSettings.bOverride_AutoExposureMethod = true;
	CameraComponent->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	CameraComponent->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	CameraComponent->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CameraComponent->PostProcessSettings.AutoExposureBias = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	CameraComponent->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	CameraComponent->PostProcessSettings.bOverride_ToneCurveAmount = true;
	CameraComponent->PostProcessSettings.ToneCurveAmount = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_BlueCorrection = true;
	CameraComponent->PostProcessSettings.BlueCorrection = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_ExpandGamut = true;
	CameraComponent->PostProcessSettings.ExpandGamut = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_BloomIntensity = true;
	CameraComponent->PostProcessSettings.BloomIntensity = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_VignetteIntensity = true;
	CameraComponent->PostProcessSettings.VignetteIntensity = 0.0f;
	CameraComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CameraComponent->PostProcessSettings.MotionBlurAmount = 0.0f;
}
