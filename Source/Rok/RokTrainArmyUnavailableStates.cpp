#include "RokUnityFeatureScreenWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "RokUiWidgets.h"

bool URokUnityFeatureScreenWidget::TryBuildTrainArmyUnavailableState(FName State,
	UCanvasPanel* Parent, const FIntRect* PortraitRect)
{
	if (!Parent)
	{
		return false;
	}
	const bool bDetail = State == TEXT("DetailMissing");
	const bool bCatalog = State == TEXT("CatalogMissing");
	const bool bModel = State == TEXT("ModelMissing");
	if (!bDetail && !bCatalog && !bModel)
	{
		return false;
	}
	const FName PageKey = bDetail ? TEXT("TrainArmyDetailUnavailable")
		: bCatalog ? TEXT("TrainArmyCatalogUnavailable") : TEXT("TrainArmyModelUnavailable");
	UClass* Class = ResolveFeaturePageClass(PageKey);
	auto* Page = Class ? CreateWidget<URokUiPageWidget>(this, Class) : nullptr;
	auto* Selected = Page ? Cast<UCanvasPanel>(RokUiFind::Find(Page, TEXT("TrainArmyUnavailableState"))) : nullptr;
	const auto* Layout = Selected ? Cast<UCanvasPanelSlot>(Selected->Slot) : nullptr;
	const FName MessageName = bDetail ? TEXT("TrainArmyDetailMissing")
		: bCatalog ? TEXT("TrainArmyMissingCatalog") : TEXT("TrainParameterStaticPreview");
	auto* Message = Cast<UTextBlock>(RokUiFind::Find(Page, MessageName));
	auto* Back = bDetail ? Cast<URokUnityIndexedButton>(RokUiFind::Find(Page, TEXT("TrainArmyDetailBackButton"))) : nullptr;
	auto* Portrait = bModel ? Cast<UImage>(RokUiFind::Find(Page, TEXT("TrainArmyUnavailablePortrait"))) : nullptr;
	if (!Selected || !Layout || !Message || (bDetail && !Back) || (bModel && !Portrait))
	{
		return false;
	}
	if (bDetail)
	{
		BindPageAction(*Page, TEXT("TrainArmyDetailBackButton"), 1951);
	}
	if (bModel)
	{
		// Only resource selection is runtime data; position, size and message stay in WBP.
		UTexture2D* Texture = PortraitRect ? LoadUiTexture(
			TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_troops_head.rok_fullscreen_troops_head")) : nullptr;
		Portrait->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture)
		{
			Portrait->SetBrush(MakeAtlasBrush(Texture, *PortraitRect, FIntPoint(1024, 1024), Portrait->GetBrush().GetImageSize()));
		}
		Page->LegacyNames.Add(TEXT("TrainArmyUnavailablePortrait"), bTrainArmyDetailVisible
			? TEXT("TrainArmyDetailPortrait") : TEXT("TrainArmySelectedIcon"));
	}
	Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* PageSlot = Parent->AddChildToCanvas(Page);
	PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PageSlot->SetOffsets(FMargin(0));
	PageSlot->SetZOrder(Layout->GetZOrder());
	return true;
}
