#include "RokUnityModalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "RokBuildingSourcePreviewData.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokUiWidgets.h"

void URokUnityModalWidget::AddBuildingPreview(UCanvasPanel* Canvas, FName ImageName, int32 Type,
	const FVector2D& PanePosition, const FVector2D& PaneSize, const FVector2D& PreviewCenter)
{
	URokUiPageWidget* PanePresentation = nullptr;
	if (!URokUiSettings::IsLegacyForced())
	{
		const TArray<FRokUiPageEntry>& Pages = GetDefault<URokUiSettings>()->ModalPages;
		if (Pages.ContainsByPredicate([](const FRokUiPageEntry& Entry)
			{
				return Entry.Page == TEXT("BuildingPreviewPane") && !Entry.Class.IsNull();
			}))
		{
			if (UClass* PaneClass = URokUiSettings::ResolvePage(Pages, TEXT("BuildingPreviewPane")))
			{
				PanePresentation = CreateWidget<URokUiPageWidget>(this, PaneClass, TEXT("BuildingPreviewPaneWbp"));
			}
		}
	}

	UCanvasPanel* Pane = nullptr;
	if (PanePresentation)
	{
		Pane = Cast<UCanvasPanel>(RokUiFind::Find(PanePresentation, TEXT("BuildingPreviewPane")));
		if (Pane)
		{
			PlaceOnCanvas(Canvas, PanePresentation, PanePosition, PaneSize, 1);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Building preview WBP has no BuildingPreviewPane canvas; using the native pane"));
			PanePresentation = nullptr;
			Pane = nullptr;
		}
	}
	if (!PanePresentation)
	{
		Pane = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingPreviewPane"));
		Pane->SetVisibility(ESlateVisibility::HitTestInvisible);
		Pane->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		PlaceOnCanvas(Canvas, Pane, PanePosition, PaneSize, 1);
	}
	const FVector2D LocalCenter = PreviewCenter - PanePosition;
	AddBuildingPreviewGlow(Pane, LocalCenter);

	UImage* BuildingInfoImage = Cast<UImage>(PanePresentation
		? RokUiFind::Find(PanePresentation, TEXT("BuildingInfoImage")) : nullptr);
	UImage* BuildingUpgradeImage = Cast<UImage>(PanePresentation
		? RokUiFind::Find(PanePresentation, TEXT("BuildingUpgradeImage")) : nullptr);
	UTextBlock* UnavailableText = Cast<UTextBlock>(PanePresentation
		? RokUiFind::Find(PanePresentation, TEXT("BuildingSourcePreviewUnavailable")) : nullptr);
	if (BuildingInfoImage)
	{
		BuildingInfoImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BuildingUpgradeImage)
	{
		BuildingUpgradeImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UnavailableText)
	{
		UnavailableText->SetVisibility(ESlateVisibility::Collapsed);
	}

	int32 HallLevel = 1;
	if (auto* Upgrade = GetUpgradeSubsystem(); Upgrade && GetWorld())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("UnityInitId_5")) && It->ActorHasTag(TEXT("UnityBuildingType_1")))
			{
				HallLevel = Upgrade->GetBuildingStateForActor(*It).Level;
				break;
			}
		}
	}
	const FIntPoint Profile = RokBuildingSourcePreview::ResolveProfile(GetGameInstance(), HallLevel);

	int32 InitId = 0;
	FString SceneModel;
	if (const AActor* Actor = SelectedBuildingActor.Get())
	{
		for (const FName Tag : Actor->Tags)
		{
			const FString Name = Tag.ToString();
			if (Name.StartsWith(TEXT("RokLocalSourceModel_"))) SceneModel = Name.RightChop(20);
			else if (Name.StartsWith(TEXT("UnityInitId_"))) LexTryParseString(InitId, *Name.RightChop(12));
		}
	}

	RokBuildingSourcePreview::FVisual Art;
	const bool bHasArt = RokBuildingSourcePreview::FindVisual(Type, Profile.X, Profile.Y, Art)
		|| RokBuildingSourcePreview::FindSceneVisual(Type, InitId, SceneModel, Art)
		|| RokBuildingSourcePreview::FindVisual(Type, 101, 1, Art);
	UTexture2D* Texture = bHasArt ? RokBuildingSourcePreview::LoadTexture(Art) : nullptr;
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Building preview missing art: type=%d country=%d age=%d"), Type, Profile.X, Profile.Y);
		if (UnavailableText)
		{
			UnavailableText->SetText(NSLOCTEXT("RokModal", "BuildingSourcePreviewUnavailable", "建筑外观源数据不可用"));
			UnavailableText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(UnavailableText->Slot))
			{
				const FVector2D ExistingPosition = TextSlot->GetPosition();
				TextSlot->SetPosition(FVector2D(ExistingPosition.X, LocalCenter.Y - 20));
			}
		}
		else
		{
			auto* Text = MakeModalText(TEXT("BuildingSourcePreviewUnavailable"), 15, FLinearColor::White,
				false, ETextJustify::Center);
			Text->SetText(NSLOCTEXT("RokModal", "BuildingSourcePreviewUnavailable", "建筑外观源数据不可用"));
			PlaceOnCanvas(Pane, Text, FVector2D(0, LocalCenter.Y - 20), FVector2D(PaneSize.X, 40), 2);
		}
		return;
	}

	const FBox2D Draw = Art.FitInPane(LocalCenter, PaneSize);
	UImage* Image = PanePresentation ? Cast<UImage>(RokUiFind::Find(PanePresentation, ImageName)) : nullptr;
	if (!Image)
	{
		Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), ImageName);
		PlaceOnCanvas(Pane, Image, Draw.Min, Draw.GetSize(), 2);
	}
	else if (UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(Image->Slot))
	{
		ImageSlot->SetPosition(Draw.Min);
		ImageSlot->SetSize(Draw.GetSize());
	}
	if (PanePresentation)
	{
		FSlateBrush Brush = Image->GetBrush();
		Brush.SetResourceObject(Texture);
		Image->SetBrush(Brush);
	}
	else
	{
		Image->SetBrushFromTexture(Texture);
	}
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}
