#include "RokStrategyCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/LocalPlayer.h"
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
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Orthographic);
	CameraComponent->SetOrthoWidth(DefaultOrthoWidth);
}

void ARokStrategyCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	SetActorLocation(DefaultCameraLocation);
	SetActorRotation(DefaultCameraRotation);
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Orthographic);
	float RuntimeOrthoWidth = DefaultOrthoWidth;
	FParse::Value(FCommandLine::Get(), TEXT("RokOrthoWidth="), RuntimeOrthoWidth);
	CameraComponent->SetOrthoWidth(FMath::Clamp(RuntimeOrthoWidth, MinOrthoWidth, MaxOrthoWidth));
}

void ARokStrategyCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateKeyboardPan(DeltaSeconds);
	UpdateEdgePan(DeltaSeconds);
	UpdateMouseWheelZoom(DeltaSeconds);
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

	const float NewOrthoWidth = FMath::Clamp(
		CameraComponent->OrthoWidth + ZoomDirection * ZoomSpeed * DeltaSeconds * 12.0f,
		MinOrthoWidth,
		MaxOrthoWidth);
	CameraComponent->SetOrthoWidth(NewOrthoWidth);
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
