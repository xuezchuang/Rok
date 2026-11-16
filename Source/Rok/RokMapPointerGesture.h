#pragma once

#include "CoreMinimal.h"

class APlayerController;
class SWidget;
struct FInputKeyEventArgs;

// Unity InputManager and WorldCamera: a primary press belongs to either a
// map click or a ground drag. A drag stays a drag even if it returns to its start.
struct FRokMapPointerGesture
{
	struct FPress
	{
		FVector2D Position;
		bool bOverUi;
	};

	void CapturePress(const APlayerController& Controller, const FInputKeyEventArgs& Event, const SWidget* Hud);
	TOptional<FPress> TakePress()
	{
		TOptional<FPress> Result = Press;
		Press.Reset();
		return Result;
	}

	void Begin(FVector2D Position)
	{
		Start = Previous = Position;
		bActive = true;
		bDragged = false;
	}

	bool IsActive() const { return bActive; }
	void Cancel() { bActive = false; }

	template <typename FPan>
	void Update(FVector2D Position, FPan&& Pan)
	{
		constexpr double DragThresholdPixels = 10.0;
		if (!bActive || (!bDragged && (Position - Start).GetAbsMax() < DragThresholdPixels))
		{
			return;
		}
		bDragged = true;
		if (Position != Previous)
		{
			Pan(Previous, Position);
			Previous = Position;
		}
	}

	bool EndAsClick()
	{
		const bool bClick = bActive && !bDragged;
		Cancel();
		return bClick;
	}

	static bool IsOverUi(const SWidget* Hud);
	static bool ProjectToGround(const APlayerController& Controller, FVector2D Position,
		double GroundZ, FVector& GroundPoint);

private:
	TOptional<FPress> Press;
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D Previous = FVector2D::ZeroVector;
	bool bActive = false;
	bool bDragged = false;
};
