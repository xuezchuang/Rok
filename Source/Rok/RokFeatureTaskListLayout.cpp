#include "RokUnityFeatureScreenWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "RokUiWidgets.h"

FVector2D URokUnityFeatureScreenWidget::GetTaskListRowSize(const UWidget* Row, FName LayoutName)
{
	const FVector2D Fallback(1088.0f, 150.0f);
	// MakeTaskRow can fall back even when a row class was supplied. Keep that
	// native path independent of the detached WBP layout.
	if (!Cast<URokUiPageWidget>(Row))
	{
		return Fallback;
	}
	if (!TaskListLayoutTemplate)
	{
		static const TSoftClassPtr<URokUiPageWidget> Class(FSoftObjectPath(
			TEXT("/Game/RokUI/Feature/WBP_RokFeature_TaskListLayouts.WBP_RokFeature_TaskListLayouts_C")));
		UClass* Loaded = URokUiSettings::Resolve(Class);
		TaskListLayoutTemplate = Loaded ? CreateWidget<URokUiPageWidget>(this, Loaded) : nullptr;
	}
	const auto* Marker = TaskListLayoutTemplate ? TaskListLayoutTemplate->Get<UWidget>(LayoutName) : nullptr;
	const auto* Slot = Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
	if (!Slot || Slot->GetAnchors().Minimum != Slot->GetAnchors().Maximum)
	{
		return Fallback;
	}
	const FVector2D Size = Slot->GetSize();
	return FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y) && Size.X > 0 && Size.Y > 0 ? Size : Fallback;
}
