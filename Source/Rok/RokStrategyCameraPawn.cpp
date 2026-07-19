#include "RokStrategyCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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
}

void ARokStrategyCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyCameraDefaults();
}

void ARokStrategyCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateKeyboardPan(DeltaSeconds);
	UpdateEdgePan(DeltaSeconds);
	UpdateMouseWheelZoom(DeltaSeconds);
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

	DefaultCameraDistance = FMath::Clamp(
		DefaultCameraDistance + ZoomDirection * ZoomSpeed * DeltaSeconds * 12.0f,
		MinCameraDistance,
		MaxCameraDistance);
	ApplyCameraDefaults();
}

void ARokStrategyCameraPawn::ApplyPlanarMovement(const FVector2D& Movement, float DeltaSeconds)
{
	const FRotator YawRotation(0.0f, GetActorRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector Delta = (Forward * Movement.Y + Right * Movement.X) * MoveSpeed * DeltaSeconds;

	FVector Location = GetActorLocation() + FVector(Delta.X, Delta.Y, 0.0f);
	Location.X = FMath::Clamp(Location.X, -MapExtent, MapExtent);
	Location.Y = FMath::Clamp(Location.Y, -MapExtent, MapExtent);
	SetActorLocation(Location);
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
	FParse::Value(FCommandLine::Get(), TEXT("RokCameraDistance="), RuntimeDistance);
	FParse::Value(FCommandLine::Get(), TEXT("RokCameraYaw="), RuntimeYaw);
	FParse::Value(FCommandLine::Get(), TEXT("RokCameraPitch="), RuntimePitch);
	FParse::Value(FCommandLine::Get(), TEXT("RokCameraVerticalFov="), RuntimeVerticalFov);
	RuntimeDistance = FMath::Clamp(RuntimeDistance, MinCameraDistance, MaxCameraDistance);
	CurrentCameraDistance = RuntimeDistance;
	CurrentVerticalFov = RuntimeVerticalFov;

	const FRotator RuntimeRotation(RuntimePitch, RuntimeYaw, 0.0f);
	const FVector RuntimeForward = RuntimeRotation.Vector();
	SetActorLocation(DefaultCameraTarget - RuntimeForward * RuntimeDistance);
	SetActorRotation(RuntimeRotation);

	CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
	CameraComponent->AspectRatio = DefaultAspectRatio;
	CameraComponent->bConstrainAspectRatio = true;
	const float HorizontalFov = FMath::RadiansToDegrees(2.0f * FMath::Atan(
		FMath::Tan(FMath::DegreesToRadians(RuntimeVerticalFov) * 0.5f) * DefaultAspectRatio));
	CameraComponent->SetFieldOfView(HorizontalFov);
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
