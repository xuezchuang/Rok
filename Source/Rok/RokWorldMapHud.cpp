#include "RokWorldMapHud.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RokUiSpec.h"
#include "RokUiWidgets.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapCityActor.h"
#include "RokWorldMapEntityActor.h"
#include "RokWorldMapHudWidget.h"
#include "RokWorldMapPlayerController.h"

namespace
{
constexpr float CanvasHudFontSize = 12.0f;
}

void ARokWorldMapHud::InitializeStyles()
{
	bStyleInitialized = true;
	static const TSoftClassPtr<URokUiPageWidget> Class(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldCanvasHudStyle.WBP_RokWorldCanvasHudStyle_C")));
	if (UClass* Loaded = URokUiSettings::Resolve(Class))
	{
		StylePage = CreateWidget<URokUiPageWidget>(PlayerOwner.Get(), Loaded);
	}
	if (StylePage)
	{
		const auto ReadCaption = [this](FName Name, FCaptionStyle& Style)
		{
			const auto* Border = StylePage->Get<UBorder>(Name);
			const auto* Text = Border ? Cast<UTextBlock>(Border->GetContent()) : nullptr;
			const auto* BorderSlot = Border ? Cast<UCanvasPanelSlot>(Border->Slot) : nullptr;
			if (Border && Text && BorderSlot)
			{
				Style.Background = Border->GetBrushColor();
				Style.Color = Text->GetColorAndOpacity().GetSpecifiedColor();
				Style.Offset = BorderSlot->GetPosition();
				Style.Padding = Border->GetPadding();
				Style.TextScale = Text->GetFont().Size / CanvasHudFontSize;
			}
		};
		ReadCaption(TEXT("DebugStatus"), DebugStatus);
		ReadCaption(TEXT("SelectionCaption"), SelectionCaption);
		if (const auto* Cross = StylePage->Get<UImage>(TEXT("SelectionCross")))
		{
			if (const auto* CrossSlot = Cast<UCanvasPanelSlot>(Cross->Slot))
			{
				SelectionCrossColor = Cross->GetColorAndOpacity();
				SelectionCrossOffset = CrossSlot->GetPosition();
				SelectionRadius = CrossSlot->GetSize().X * 0.5f;
				SelectionThickness = CrossSlot->GetSize().Y;
			}
		}
	}
#if WITH_EDITOR
	FString Filename;
	if (URokUiSettings::IsLegacyForced()
		&& FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldCanvasStyle="), Filename))
	{
		auto* Tree = NewObject<UWidgetTree>(this);
		auto* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldCanvasHudStyle"));
		Tree->RootWidget = Root;
		const auto AddCaption = [&](FName Name, FName TextName, const FCaptionStyle& Style,
			const FText& Sample, const FVector2D& PreviewSize)
		{
			auto* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
			Border->SetBrush(FSlateColorBrush(FLinearColor::White));
			Border->SetBrushColor(Style.Background);
			Border->SetPadding(Style.Padding);
			auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
			Text->SetText(Sample);
			Text->SetColorAndOpacity(Style.Color);
			Text->SetFont(FSlateFontInfo(GEngine->GetSmallFont(), CanvasHudFontSize));
			Border->SetContent(Text);
			auto* BorderSlot = Root->AddChildToCanvas(Border);
			BorderSlot->SetPosition(Style.Offset);
			BorderSlot->SetSize(PreviewSize);
		};
		AddCaption(TEXT("DebugStatus"), TEXT("DebugStatusText"), DebugStatus,
			FText::FromString(TEXT("#1  X:1146 Y:157 | dxf:1509.0")), {290, 28});
		AddCaption(TEXT("SelectionCaption"), TEXT("SelectionCaptionText"), SelectionCaption,
			FText::FromString(TEXT("SELECTED CITY")), {120, 20});
		auto* Cross = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SelectionCross"));
		Cross->SetBrush(FSlateColorBrush(FLinearColor::White));
		Cross->SetColorAndOpacity(SelectionCrossColor);
		auto* CrossSlot = Root->AddChildToCanvas(Cross);
		CrossSlot->SetPosition(SelectionCrossOffset);
		CrossSlot->SetSize({SelectionRadius * 2, SelectionThickness});
		FString Error;
		if (!RokUiSpec::ExportChildren(Root, Filename, TEXT("world_canvas_hud_style"), Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Rok UI: world Canvas HUD style export failed: %s"), *Error);
		}
	}
#endif
}

void ARokWorldMapHud::DrawCaption(const FString& Text, UFont* Font, const FCaptionStyle& Style,
	const FVector2D& Anchor, bool bAboveAnchor)
{
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	Canvas->StrLen(Font, Text, TextWidth, TextHeight);
	TextWidth *= Style.TextScale;
	TextHeight *= Style.TextScale;
	const float Width = TextWidth + Style.Padding.Left + Style.Padding.Right;
	const float Height = TextHeight + Style.Padding.Top + Style.Padding.Bottom;
	const float BoxX = Anchor.X + Style.Offset.X - Width * 0.5f;
	const float BoxY = Anchor.Y + Style.Offset.Y - (bAboveAnchor ? TextHeight + Style.Padding.Top : 0.0f);
	DrawRect(Style.Background, BoxX, BoxY, Width, Height);
	DrawText(Text, Style.Color, BoxX + Style.Padding.Left, BoxY + Style.Padding.Top, Font, Style.TextScale, false);
}

void ARokWorldMapHud::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner)
	{
		return;
	}

	const auto* CameraPawn = Cast<ARokWorldMapCameraPawn>(PlayerOwner->GetPawn());
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!CameraPawn || !Font)
	{
		return;
	}
	if (!bStyleInitialized) InitializeStyles();

	if (FParse::Param(FCommandLine::Get(), TEXT("RokWorldMapDebugHud")))
	{
		const FVector2D ViewCenter = CameraPawn->GetViewCenterDisplay();
		const FString StatusText = FString::Printf(
			TEXT("#1  X:%d Y:%d | dxf:%.1f    WASD / RMB drag / wheel zoom / click object"),
			FMath::FloorToInt(ViewCenter.X), FMath::FloorToInt(ViewCenter.Y), CameraPawn->GetCurrentDxf());
		DrawCaption(StatusText, Font, DebugStatus, {Canvas->ClipX * 0.5f, 0.0f}, false);
	}

	const auto* WorldMapController = Cast<ARokWorldMapPlayerController>(PlayerOwner);
	AActor* SelectedActor = WorldMapController ? WorldMapController->GetSelectedWorldMapActor() : nullptr;
	if (!SelectedActor)
	{
		return;
	}
	const auto* Entity = Cast<ARokWorldMapEntityActor>(SelectedActor);
	if (Entity && URokWorldMapHudWidget::IsResourceEntityKind(Entity->GetEntityKind()))
	{
		return;
	}

	FVector Anchor = SelectedActor->GetActorLocation();
	if (const auto* City = Cast<ARokWorldMapCityActor>(SelectedActor))
	{
		Anchor = City->GetSelectionAnchorWorldPosition();
	}
	else if (Entity)
	{
		Anchor = Entity->GetSelectionAnchorWorldPosition();
	}
	FVector2D Screen = FVector2D::ZeroVector;
	if (!PlayerOwner->ProjectWorldLocationToScreen(Anchor, Screen, true))
	{
		return;
	}
	Screen += SelectionCrossOffset;
	DrawLine(Screen.X - SelectionRadius, Screen.Y, Screen.X + SelectionRadius, Screen.Y,
		SelectionCrossColor, SelectionThickness);
	DrawLine(Screen.X, Screen.Y - SelectionRadius, Screen.X, Screen.Y + SelectionRadius,
		SelectionCrossColor, SelectionThickness);
	const FString SelectionText = Entity ? Entity->GetSelectionText().ToString() : TEXT("SELECTED CITY");
	DrawCaption(SelectionText, Font, SelectionCaption, {Screen.X, Screen.Y - SelectionRadius}, true);
}
