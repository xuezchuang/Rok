#include "RokCityPlacementWorldFrame.h"

#include "RokCityGridMath.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCitySpriteActor.h"
#include "RokDataPaths.h"
#include "RokLocalCitySimulationContext.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SceneView.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "String/LexFromString.h"
#include "UnrealClient.h"

namespace
{
constexpr double UnityGroundAnchorHeight = 0.01;
constexpr double MaximumAnchorErrorUU = 1.0;
constexpr int32 ExpectedInitialFootprints = 24;

bool TryTagInt(const AActor& Actor, const TCHAR* Prefix, int32& OutValue)
{
	for (const FName& Tag : Actor.Tags)
	{
		FString Text = Tag.ToString();
		if (Text.RemoveFromStart(Prefix) && LexTryParseString(OutValue, *Text))
		{
			return true;
		}
	}
	return false;
}

bool ReadSourceGridSize(int32& OutSize)
{
	FString Json;
	const FString Path = RokDataPaths::Resolve(TEXT("LocalCitySimulationContext/catalog.json"));
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		|| !Root.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Grid = nullptr;
	double Size = 0;
	if (!Root->TryGetObjectField(TEXT("grid"), Grid) || !Grid || !Grid->IsValid()
		|| !(*Grid)->TryGetNumberField(TEXT("size"), Size)
		|| !FMath::IsFinite(Size) || Size < 3 || Size > 1001
		|| FMath::FloorToDouble(Size) != Size || int32(Size) % 2 == 0)
	{
		return false;
	}
	OutSize = int32(Size);
	return true;
}

bool GetInitialActor(const UWorld* World, int32 InitId, const ARokCitySpriteActor*& OutActor)
{
	OutActor = nullptr;
	for (TActorIterator<ARokCitySpriteActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const ARokCitySpriteActor* Candidate = *It;
		int32 TaggedId = 0;
		if (Candidate && Candidate->Tags.Contains(TEXT("RokUnityConfigInitBuilding"))
			&& TryTagInt(*Candidate, TEXT("UnityInitId_"), TaggedId) && TaggedId == InitId)
		{
			if (OutActor) return false;
			OutActor = Candidate;
		}
	}
	return OutActor != nullptr;
}

bool CheckTaggedFootprint(const ARokCitySpriteActor& Actor,
	const FRokCityConstructionInitialOccupant& Initial)
{
	int32 X = 0, Y = 0, Width = 0, Length = 0;
	return TryTagInt(Actor, TEXT("UnityGridX_"), X)
		&& TryTagInt(Actor, TEXT("UnityGridY_"), Y)
		&& TryTagInt(Actor, TEXT("UnityGridWidth_"), Width)
		&& TryTagInt(Actor, TEXT("UnityGridLength_"), Length)
		&& FIntPoint(X, Y) == Initial.Footprint.Origin
		&& FIntPoint(Width, Length) == Initial.Footprint.Size
		&& Actor.GetGameplayTile() == Initial.Footprint.Origin
		&& Actor.GetFootprintSize() == Initial.Footprint.Size;
}
}

bool FRokCityPlacementWorldFrame::ResolveForWorld(const UWorld* World,
	FRokCityPlacementWorldFrame& OutFrame, FString& OutError)
{
	OutFrame = FRokCityPlacementWorldFrame();
	OutError.Reset();
	FRokLocalCitySimulationContext Context;
	FRokCityConstructionGrid SourceGrid;
	int32 SourceGridSize = 0;
	if (!Context.Load(OutError) || !Context.ValidateWorld(World, OutError)
		|| !SourceGrid.Load(OutError) || !ReadSourceGridSize(SourceGridSize)
		|| Context.GetInitialAge() != 1 || SourceGrid.GetAgeForTownHallLevel(1) != 1)
	{
		if (OutError.IsEmpty()) OutError = TEXT("Placement source grid or local city frame is unavailable");
		return false;
	}

	const FRokCityConstructionInitialOccupant* Hall = nullptr;
	for (const FRokCityConstructionInitialOccupant& Initial : SourceGrid.GetInitialOccupancy())
	{
		if (Initial.InitId == 5 && Initial.TypeId == 1
			&& Initial.Footprint.Origin == Context.GetInitialHallTile())
		{
			Hall = &Initial;
			break;
		}
	}
	const ARokCitySpriteActor* HallActor = nullptr;
	if (!Hall || Hall->Footprint.Size != FIntPoint(9, 9)
		|| !GetInitialActor(World, Hall->InitId, HallActor)
		|| !CheckTaggedFootprint(*HallActor, *Hall))
	{
		OutError = TEXT("Source city hall footprint or live actor identity changed");
		return false;
	}

	// The source horizontal axes are left-handed relative to UE Up. A proper
	// rotation with signed Z scale preserves both axes and source ground height.
	const FQuat Rotation = FRotationMatrix::MakeFromXY(
		FRokCityGridMath::GetWidthAxis(), FRokCityGridMath::GetLengthAxis()).ToQuat();
	const FTransform Basis(Rotation, FVector::ZeroVector,
		FVector(FRokCityGridMath::UnityToUnrealScale,
			FRokCityGridMath::UnityToUnrealScale,
			-FRokCityGridMath::UnityToUnrealScale));
	FVector2D HallCenter;
	if (!FRokCityPlacementPreviewMath::GridToLocalCenter(Hall->Footprint, HallCenter, OutError))
	{
		return false;
	}
	const FVector HallLocal(HallCenter.X, HallCenter.Y, UnityGroundAnchorHeight);
	const FVector Origin = HallActor->GetVisualAnchorWorldPosition()
		- Basis.TransformPosition(HallLocal);
	const FTransform Container(Rotation, Origin, Basis.GetScale3D());
	if (!Container.IsValid())
	{
		OutError = TEXT("City placement frame is not a finite transform");
		return false;
	}

	int32 Verified = 0;
	for (const FRokCityConstructionInitialOccupant& Initial : SourceGrid.GetInitialOccupancy())
	{
		if (!Initial.Footprint.IsValid()) continue;
		const ARokCitySpriteActor* Actor = nullptr;
		FVector2D LocalCenter;
		if (!GetInitialActor(World, Initial.InitId, Actor)
			|| !CheckTaggedFootprint(*Actor, Initial)
			|| !FRokCityPlacementPreviewMath::GridToLocalCenter(
				Initial.Footprint, LocalCenter, OutError))
		{
			OutError = FString::Printf(TEXT("Initial city actor %d does not match source footprint"),
				Initial.InitId);
			return false;
		}
		const FVector Expected = Container.TransformPosition(
			FVector(LocalCenter.X, LocalCenter.Y, UnityGroundAnchorHeight));
		const FVector Actual = Actor->GetVisualAnchorWorldPosition();
		FVector2D RoundTrip;
		if (!FVector::PointsAreNear(Expected, Actual, MaximumAnchorErrorUU)
			|| !FRokCityPlacementPreviewMath::WorldToCityLocal(
				Container, Actual, RoundTrip, OutError)
			|| !RoundTrip.Equals(LocalCenter, 0.002))
		{
			OutError = FString::Printf(TEXT("Initial city actor %d is off the source city frame"),
				Initial.InitId);
			return false;
		}
		++Verified;
	}
	if (Verified != ExpectedInitialFootprints)
	{
		OutError = TEXT("Expected 24 source initial city footprints in current world");
		return false;
	}
	OutFrame.CityContainer = Container;
	OutFrame.GridSize = SourceGridSize;
	OutFrame.bValid = true;
	return true;
}

bool FRokCityPlacementWorldFrame::TryScreenToGround(const APlayerController& Controller,
	const FVector2D& ScreenPoint, FVector& OutWorldHit, FString& OutError) const
{
	OutError.Reset();
	OutWorldHit = FVector::ZeroVector;
	int32 Width = 0, Height = 0;
	Controller.GetViewportSize(Width, Height);
	if (!bValid || Width <= 0 || Height <= 0
		|| !FMath::IsFinite(ScreenPoint.X) || !FMath::IsFinite(ScreenPoint.Y)
		|| ScreenPoint.X < 0 || ScreenPoint.Y < 0
		|| ScreenPoint.X >= Width || ScreenPoint.Y >= Height)
	{
		OutError = TEXT("Pointer ray is outside the verified city viewport");
		return false;
	}
	const ULocalPlayer* LocalPlayer = Controller.GetLocalPlayer();
	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer || !LocalPlayer->ViewportClient
		|| !LocalPlayer->ViewportClient->Viewport
		|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData)
		|| !ProjectionData.IsValidViewRectangle())
	{
		OutError = TEXT("Pointer ray projection data is unavailable");
		return false;
	}
	// GetMousePosition is local to the whole viewport. UE deprojection uses
	// backbuffer coordinates; only add the viewport origin, not any letterbox inset.
	const FVector2D ProjectionPoint = ScreenPoint
		+ FVector2D(LocalPlayer->ViewportClient->Viewport->GetInitialPositionXY());
	FVector RayOrigin, RayDirection;
	if (!Controller.DeprojectScreenPositionToWorld(
			ProjectionPoint.X, ProjectionPoint.Y, RayOrigin, RayDirection)
		|| RayOrigin.ContainsNaN() || RayDirection.ContainsNaN()
		|| FMath::IsNearlyZero(RayDirection.Z))
	{
		OutError = TEXT("Pointer ray is outside the verified city viewport");
		return false;
	}
	const double GroundZ = CityContainer.GetLocation().Z;
	const double Distance = (GroundZ - RayOrigin.Z) / RayDirection.Z;
	if (!FMath::IsFinite(Distance) || Distance <= 0)
	{
		OutError = TEXT("Pointer ray does not meet the city ground plane");
		return false;
	}
	OutWorldHit = RayOrigin + RayDirection * Distance;
	if (OutWorldHit.ContainsNaN())
	{
		OutError = TEXT("Pointer ray produced a non-finite city ground hit");
		return false;
	}
	return true;
}

bool FRokCityPlacementWorldFrame::GridCenterToWorld(
	const FRokCityConstructionFootprint& Footprint, FVector& OutWorldCenter,
	FString& OutError) const
{
	OutError.Reset();
	OutWorldCenter = FVector::ZeroVector;
	FVector2D LocalCenter;
	if (!bValid || !FRokCityPlacementPreviewMath::GridToLocalCenter(
		Footprint, LocalCenter, OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("City placement frame is unavailable");
		return false;
	}
	OutWorldCenter = CityContainer.TransformPosition(
		FVector(LocalCenter.X, LocalCenter.Y, UnityGroundAnchorHeight));
	return !OutWorldCenter.ContainsNaN();
}
