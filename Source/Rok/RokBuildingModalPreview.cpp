#include "RokUnityModalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "RokBuildingSourcePreviewData.h"
#include "RokBuildingUpgradeSubsystem.h"

void URokUnityModalWidget::AddBuildingPreview(UCanvasPanel* Canvas, FName ImageName, int32 Type,
	const FVector2D& PanePosition, const FVector2D& PaneSize, const FVector2D& PreviewCenter)
{
	auto* Pane = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingPreviewPane"));
	Pane->SetVisibility(ESlateVisibility::HitTestInvisible);
	Pane->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(Canvas, Pane, PanePosition, PaneSize, 1);
	const FVector2D LocalCenter = PreviewCenter - PanePosition;
	AddBuildingPreviewGlow(Pane, LocalCenter);

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
		auto* Text = MakeModalText(TEXT("BuildingSourcePreviewUnavailable"), 15, FLinearColor::White,
			false, ETextJustify::Center);
		Text->SetText(NSLOCTEXT("RokModal", "BuildingSourcePreviewUnavailable", "建筑外观源数据不可用"));
		PlaceOnCanvas(Pane, Text, FVector2D(0, LocalCenter.Y - 20), FVector2D(PaneSize.X, 40), 2);
		return;
	}

	const FBox2D Draw = Art.FitInPane(LocalCenter, PaneSize);
	auto* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), ImageName);
	Image->SetBrushFromTexture(Texture);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Pane, Image, Draw.Min, Draw.GetSize(), 2);
}
