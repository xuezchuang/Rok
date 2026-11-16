#include "RokMapPointerGesture.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Layout/WidgetPath.h"

void FRokMapPointerGesture::CapturePress(const APlayerController& Controller,
	const FInputKeyEventArgs& Event, const SWidget* Hud)
{
	if (Event.Key != EKeys::LeftMouseButton || (Event.Event != IE_Pressed && Event.Event != IE_DoubleClick))
	{
		return;
	}
	// Key bindings run on the next input tick. A quick drag may have already
	// moved the viewport cursor by then, so capture the press at the source.
	Press.Reset();
	FVector2D Position;
	if (Controller.GetMousePosition(Position.X, Position.Y))
	{
		Press.Emplace(FPress{Position, Hud && IsOverUi(Hud)});
	}
}

bool FRokMapPointerGesture::IsOverUi(const SWidget* Hud)
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	const FWidgetPath Path = Slate.LocateWindowUnderMouse(
		Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows());
	if (!Path.IsValid())
	{
		return true;
	}
	if (Hud && Path.ContainsWidget(Hud))
	{
		return true;
	}
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		if (Path.Widgets[Index].Widget->GetTypeAsString().Contains(TEXT("SObjectWidget")))
		{
			return true;
		}
	}
	return false;
}

bool FRokMapPointerGesture::ProjectToGround(const APlayerController& Controller,
	FVector2D Position, double GroundZ, FVector& GroundPoint)
{
	FVector RayOrigin, RayDirection;
	if (!Controller.DeprojectScreenPositionToWorld(Position.X, Position.Y, RayOrigin, RayDirection)
		|| FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}
	const double Distance = (GroundZ - RayOrigin.Z) / RayDirection.Z;
	if (Distance < 0.0)
	{
		return false;
	}
	GroundPoint = RayOrigin + RayDirection * Distance;
	return true;
}
