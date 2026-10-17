#include "RokWorldMapHud.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapCityActor.h"
#include "RokWorldMapEntityActor.h"
#include "RokWorldMapHudWidget.h"
#include "RokWorldMapPlayerController.h"

void ARokWorldMapHud::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner)
	{
		return;
	}

	const ARokWorldMapCameraPawn* CameraPawn = Cast<ARokWorldMapCameraPawn>(PlayerOwner->GetPawn());
	if (!CameraPawn)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	if (FParse::Param(
			FCommandLine::Get(),
			TEXT("RokWorldMapDebugHud")))
	{
		const FVector2D ViewCenterDisplay =
			CameraPawn->GetViewCenterDisplay();
		const int32 DisplayX =
			FMath::FloorToInt(ViewCenterDisplay.X);
		const int32 DisplayY =
			FMath::FloorToInt(ViewCenterDisplay.Y);
		const FString StatusText = FString::Printf(
			TEXT("#1  X:%d Y:%d | dxf:%.1f    WASD / RMB drag / wheel zoom / click object"),
			DisplayX,
			DisplayY,
			CameraPawn->GetCurrentDxf());

		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		Canvas->StrLen(
			Font,
			StatusText,
			TextWidth,
			TextHeight);
		constexpr float HorizontalPadding = 18.0f;
		constexpr float VerticalPadding = 8.0f;
		const float BarWidth =
			TextWidth + HorizontalPadding * 2.0f;
		const float BarHeight =
			TextHeight + VerticalPadding * 2.0f;
		const float BarX =
			(Canvas->ClipX - BarWidth) * 0.5f;
		constexpr float BarY = 16.0f;

		DrawRect(
			FLinearColor(0.015f, 0.02f, 0.025f, 0.82f),
			BarX,
			BarY,
			BarWidth,
			BarHeight);
		DrawText(
			StatusText,
			FLinearColor(0.94f, 0.9f, 0.72f, 1.0f),
			BarX + HorizontalPadding,
			BarY + VerticalPadding,
			Font,
			1.0f,
			false);
	}

	const ARokWorldMapPlayerController* WorldMapController =
		Cast<ARokWorldMapPlayerController>(PlayerOwner);
	AActor* SelectedActor = WorldMapController
		? WorldMapController->GetSelectedWorldMapActor()
		: nullptr;
	if (!SelectedActor)
	{
		return;
	}
	if (const ARokWorldMapEntityActor* EntityActor =
			Cast<ARokWorldMapEntityActor>(SelectedActor);
		EntityActor
		&& URokWorldMapHudWidget::IsResourceEntityKind(
			EntityActor->GetEntityKind()))
	{
		// Resource selection is now represented by the interactive UMG popup.
		// Keep the legacy canvas marker only for entity types whose dedicated
		// Unity popup has not been migrated yet.
		return;
	}

	FVector SelectionAnchor = SelectedActor->GetActorLocation();
	if (const ARokWorldMapCityActor* CityActor =
			Cast<ARokWorldMapCityActor>(SelectedActor))
	{
		SelectionAnchor = CityActor->GetSelectionAnchorWorldPosition();
	}
	else if (const ARokWorldMapEntityActor* EntityActor =
			Cast<ARokWorldMapEntityActor>(SelectedActor))
	{
		SelectionAnchor =
			EntityActor->GetSelectionAnchorWorldPosition();
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!PlayerOwner->ProjectWorldLocationToScreen(
			SelectionAnchor,
			ScreenPosition,
			true))
	{
		return;
	}

	constexpr float MarkerRadius = 18.0f;
	const FLinearColor SelectionColor(0.12f, 1.0f, 0.38f, 1.0f);
	DrawLine(
		ScreenPosition.X - MarkerRadius,
		ScreenPosition.Y,
		ScreenPosition.X + MarkerRadius,
		ScreenPosition.Y,
		SelectionColor,
		2.0f);
	DrawLine(
		ScreenPosition.X,
		ScreenPosition.Y - MarkerRadius,
		ScreenPosition.X,
		ScreenPosition.Y + MarkerRadius,
		SelectionColor,
		2.0f);

	FString SelectionText(TEXT("SELECTED CITY"));
	if (const ARokWorldMapEntityActor* EntityActor =
			Cast<ARokWorldMapEntityActor>(SelectedActor))
	{
		SelectionText = EntityActor->GetSelectionText().ToString();
	}
	float SelectionTextWidth = 0.0f;
	float SelectionTextHeight = 0.0f;
	Canvas->StrLen(
		Font,
		SelectionText,
		SelectionTextWidth,
		SelectionTextHeight);
	const float SelectionTextX =
		ScreenPosition.X - SelectionTextWidth * 0.5f;
	const float SelectionTextY =
		ScreenPosition.Y - MarkerRadius - SelectionTextHeight - 12.0f;
	DrawRect(
		FLinearColor(0.01f, 0.08f, 0.025f, 0.86f),
		SelectionTextX - 8.0f,
		SelectionTextY - 4.0f,
		SelectionTextWidth + 16.0f,
		SelectionTextHeight + 8.0f);
	DrawText(
		SelectionText,
		SelectionColor,
		SelectionTextX,
		SelectionTextY,
		Font,
		1.0f,
		false);
}
