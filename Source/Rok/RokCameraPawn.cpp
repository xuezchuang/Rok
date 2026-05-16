#include "RokCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Scene.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ARokCameraPawn::ARokCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SceneRoot);
	ApplyFixedCameraSettings();
}

void ARokCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	SetActorLocation(DefaultCameraLocation);
	SetActorRotation(DefaultCameraRotation);
	ApplyFixedCameraSettings();

	float RuntimeOrthoWidth = DefaultOrthoWidth;
	FParse::Value(FCommandLine::Get(), TEXT("RokSingleOrthoWidth="), RuntimeOrthoWidth);
	CameraComponent->SetOrthoWidth(FMath::Clamp(RuntimeOrthoWidth, MinOrthoWidth, MaxOrthoWidth));
}

void ARokCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateKeyboardPan(DeltaSeconds);
	UpdateMouseWheelZoom();
}

void ARokCameraPawn::ApplyFixedCameraSettings()
{
	if (!CameraComponent)
	{
		return;
	}

	CameraComponent->SetProjectionMode(ECameraProjectionMode::Orthographic);
	CameraComponent->SetOrthoWidth(DefaultOrthoWidth);
	CameraComponent->bConstrainAspectRatio = false;

	CameraComponent->PostProcessSettings.bOverride_AutoExposureMethod = true;
	CameraComponent->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	CameraComponent->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	CameraComponent->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
	CameraComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CameraComponent->PostProcessSettings.AutoExposureBias = 0.0f;
}

void ARokCameraPawn::UpdateKeyboardPan(float DeltaSeconds)
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

void ARokCameraPawn::UpdateMouseWheelZoom()
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

	const float NewOrthoWidth = FMath::Clamp(CameraComponent->OrthoWidth + ZoomDirection * ZoomStep, MinOrthoWidth, MaxOrthoWidth);
	CameraComponent->SetOrthoWidth(NewOrthoWidth);
}

void ARokCameraPawn::ApplyPlanarMovement(const FVector2D& Movement, float DeltaSeconds)
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
