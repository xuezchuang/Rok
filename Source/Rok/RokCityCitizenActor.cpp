#include "RokCityCitizenActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RokCityGridMath.h"
#include "RokSpriteClip.h"
#include "RokStrategyCameraPawn.h"
#include "UObject/ConstructorHelpers.h"

ARokCityCitizenActor::ARokCityCitizenActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Feet")));
	SpriteMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sprite"));
	SpriteMeshComponent->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	SpriteMeshComponent->SetStaticMesh(Plane.Object);
	SpriteMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteMeshComponent->SetCastShadow(false);
	SpriteMeshComponent->bReceivesDecals = false;
	SpriteMeshComponent->bUseAsOccluder = false;
	SpriteMeshComponent->SetCanEverAffectNavigation(false);
	SpriteMeshComponent->TranslucencySortPriority = 3000; // cm_slg_soldiers, Unity Transparent queue.
	SpriteMeshComponent->SetVisibility(false);
}

bool ARokCityCitizenActor::ConfigureSprite(const FString& Directory, const FString& Model,
	const TCHAR* Pose, int32 InSeed)
{
	Clip = FRokSpriteClip::Load(Directory, Model, Pose);
	static TWeakObjectPtr<UMaterialInterface> SpriteParent;
	if (!SpriteParent.IsValid())
	{
		SpriteParent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldEntitySprite.M_RokWorldEntitySprite"));
	}
	if (!Clip || !SpriteParent.IsValid()) return false;
	SpriteMaterial = SpriteMeshComponent->CreateDynamicMaterialInstance(0, SpriteParent.Get());
	AnimationElapsed = InSeed * 0.073f;
	return SpriteMaterial != nullptr;
}

bool ARokCityCitizenActor::ConfigureWalker(const FString& Model, const FVector& Start, const FVector& End,
	const FTransform& InCityFrame, int32 InSeed)
{
	if (!ConfigureSprite(TEXT("CityLife"), Model, TEXT("walk"), InSeed)) return false;
	bWalking = true;
	CityFrame = InCityFrame;
	RouteStart = Start;
	RouteEnd = End;
	SetActorLocation(Start);
	SetHeading(FVector2D(CityFrame.InverseTransformVector(End - Start)));
	UpdateSprite();
	return true;
}

bool ARokCityCitizenActor::ConfigureSoldier(const FString& Model, const FVector& Position,
	const FVector2D& Heading, int32 InSeed)
{
	if (!ConfigureSprite(TEXT("WorldArmySprites"), Model, TEXT("idle"), InSeed)) return false;
	TrainingClip = FRokSpriteClip::Load(TEXT("WorldArmySprites"), Model, TEXT("attack"));
	SetActorLocation(Position);
	SetHeading(Heading);
	UpdateSprite();
	return true;
}

void ARokCityCitizenActor::SetHeading(const FVector2D& Heading)
{
	const double Degrees = FMath::RadiansToDegrees(FMath::Atan2(Heading.X, Heading.Y));
	Facing = (FMath::RoundToInt(Degrees / 45.0) + 8) % 8;
}

void ARokCityCitizenActor::SetTraining(bool bInTraining)
{
	bTraining = bInTraining;
}

void ARokCityCitizenActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Clip) return;
	AnimationElapsed += DeltaSeconds;
	if (bWalking)
	{
		const FVector Target = bTowardEnd ? RouteEnd : RouteStart;
		const FVector Delta = Target - GetActorLocation();
		const double Distance = Delta.Size();
		constexpr double WalkSpeed = 105.0;
		const double Travel = WalkSpeed * DeltaSeconds;
		if (Distance <= Travel)
		{
			SetActorLocation(Target);
			bTowardEnd = !bTowardEnd;
			SetHeading(FVector2D(CityFrame.InverseTransformVector(bTowardEnd ? RouteEnd - RouteStart : RouteStart - RouteEnd)));
		}
		else
		{
			SetActorLocation(GetActorLocation() + Delta * (Travel / Distance));
		}
	}
	UpdateSprite();
}

void ARokCityCitizenActor::UpdateSprite()
{
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const auto* Camera = Controller ? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	const bool bVisible = Camera && Camera->GetCurrentCityDxf() <= 270.0f;
	SetActorHiddenInGame(!bVisible);
	if (!bVisible || !Clip || !SpriteMaterial) return;

	const FRokSpriteClip& ActiveClip = bTraining && TrainingClip ? *TrainingClip : *Clip;
	const int32 Direction = FMath::Min(Facing, 8 - Facing);
	const auto& Frames = ActiveClip.Frames[Direction];
	if (Frames.IsEmpty()) return;
	UTexture2D* Texture = Frames[FMath::FloorToInt(AnimationElapsed / ActiveClip.FrameSeconds) % Frames.Num()].Get();
	if (Texture != LastFrame)
	{
		SpriteMaterial->SetTextureParameterValue(TEXT("Sprite"), Texture);
		LastFrame = Texture;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FRotationMatrix View(ViewRotation);
	const FVector Right = View.GetScaledAxis(EAxis::Y);
	const FVector Up = View.GetScaledAxis(EAxis::Z);
	const FVector Forward = View.GetScaledAxis(EAxis::X);
	const FVector2D Size = ActiveClip.SizeInUnity * FRokCityGridMath::UnityToUnrealScale
		* FRokSpriteClip::EvaluateUnityUnitScale(Camera->GetCurrentCityDxf());
	const bool bMirror = !(Facing > 0 && Facing < 4);
	const double PivotX = bMirror ? 1.0 - ActiveClip.Pivot.X : ActiveClip.Pivot.X;
	SpriteMeshComponent->SetWorldRotation(FMatrix(Right, Up, Forward, FVector::ZeroVector).ToQuat());
	SpriteMeshComponent->SetWorldScale3D({(bMirror ? Size.X : -Size.X) / 100.0, -Size.Y / 100.0, 1.0});
	SpriteMeshComponent->SetRelativeLocation(Right * ((0.5 - PivotX) * Size.X)
		+ Up * ((0.5 - ActiveClip.Pivot.Y) * Size.Y) - Forward * 8.0);
	SpriteMeshComponent->SetVisibility(true);

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
