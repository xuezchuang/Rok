#include "RokCityCitizenActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARokCityCitizenActor::ARokCityCitizenActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SpriteMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CitizenSpritePlane"));
	SpriteMeshComponent->SetupAttachment(SceneRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		SpriteMeshComponent->SetStaticMesh(PlaneMesh.Object);
	}
	SpriteMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteMeshComponent->SetCastShadow(false);
	SpriteMeshComponent->bReceivesDecals = false;
	SpriteMeshComponent->bUseAsOccluder = false;
	SpriteMeshComponent->SetCanEverAffectNavigation(false);
}

void ARokCityCitizenActor::ConfigureCitizen(
	UMaterialInterface* Material,
	float SizeCm,
	float SpeedCmPerSecond,
	const FVector& InHome,
	int32 InSeed)
{
	HomeLocation = FVector(InHome.X, InHome.Y, InHome.Z);
	MoveSpeed = FMath::Max(10.0f, SpeedCmPerSecond);
	Seed = InSeed;
	Step = 0;
	SetActorLocation(HomeLocation);
	WanderTarget = EvaluateWanderTarget(HomeLocation, WanderRadius, Seed, Step);
	if (SpriteMeshComponent)
	{
		if (Material)
		{
			SpriteMeshComponent->SetMaterial(0, Material);
		}
		const float Size = FMath::Max(10.0f, SizeCm);
		// The source plane is 100 units wide; the material maps the sprite cell.
		SpriteMeshComponent->SetRelativeScale3D(FVector(Size / 100.0f, Size / 100.0f, 1.0f));
		SpriteMeshComponent->SetRelativeLocation(FVector::ZeroVector);
	}
	FaceCamera();
}

void ARokCityCitizenActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MoveSpeed <= 0.0f)
	{
		return;
	}
	FVector Location = GetActorLocation();
	const FVector ToTarget = WanderTarget - Location;
	const FVector Planar(ToTarget.X, ToTarget.Y, 0.0f);
	const float Distance = Planar.Size();
	if (Distance <= 12.0f)
	{
		PickNextTarget();
		FaceCamera();
		return;
	}
	const FVector Move = Planar / Distance * FMath::Min(Distance, MoveSpeed * DeltaSeconds);
	SetActorLocation(Location + Move);
	FaceCamera();
}

void ARokCityCitizenActor::PickNextTarget()
{
	++Step;
	WanderTarget = EvaluateWanderTarget(HomeLocation, WanderRadius, Seed, Step);
}

void ARokCityCitizenActor::FaceCamera()
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const AActor* ViewTarget = PlayerController ? PlayerController->GetViewTarget() : nullptr;
	if (!ViewTarget)
	{
		return;
	}
	const FVector Direction = ViewTarget->GetActorLocation() - GetActorLocation();
	if (Direction.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	// Plane normal is +Z, so place +Z along the view direction.
	SetActorRotation(FRotationMatrix::MakeFromZ(Direction.GetSafeNormal()).Rotator());
}

FVector ARokCityCitizenActor::EvaluateWanderTarget(
	const FVector& Home,
	float RadiusCm,
	int32 InSeed,
	int32 InStep)
{
	if (RadiusCm <= 0.0f)
	{
		return Home;
	}
	const uint32 Hash = HashCombine(GetTypeHash(InSeed), GetTypeHash(InStep));
	const uint32 AngleBits = Hash % 1000u;
	const uint32 DistanceBits = (Hash / 1000u) % 1000u;
	const float Angle = (AngleBits / 1000.0f) * 2.0f * UE_PI;
	// Keep walkers between 35% and 100% of the radius so they never idle at the
	// exact home point.
	const float Distance = RadiusCm * (0.35f + 0.65f * (DistanceBits / 1000.0f));
	return Home + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
}
