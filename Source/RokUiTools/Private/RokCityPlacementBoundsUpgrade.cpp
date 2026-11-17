#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogRokCityPlacementBounds, Log, All);

constexpr TCHAR PagePath[] = TEXT("/Game/RokUI/Hud/WBP_RokCityPlacementControls.WBP_RokCityPlacementControls");
const FName BoundsName(TEXT("CityPlacementDefaultBounds"));

const UCanvasPanelSlot* FindBoundsSlot(const UWidgetTree* Tree)
{
	const auto* Marker = Tree ? Tree->FindWidget<UCanvasPanel>(BoundsName) : nullptr;
	const auto* BoundsSlot = Marker && Marker->GetParent() == Tree->RootWidget
		&& Marker->GetVisibility() == ESlateVisibility::Collapsed
		&& Marker->GetChildrenCount() == 0 ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
	if (!BoundsSlot || BoundsSlot->GetAnchors().Minimum != BoundsSlot->GetAnchors().Maximum
		|| BoundsSlot->GetSize().X <= 0 || BoundsSlot->GetSize().Y <= 0)
	{
		return nullptr;
	}
	return BoundsSlot;
}

const UWidgetTree* GetGeneratedTree(const UWidgetBlueprint* Blueprint)
{
	const auto* Generated = Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	return Generated ? Generated->GetWidgetTreeArchetype() : nullptr;
}

bool HasMatchingBounds(const UWidgetBlueprint* Blueprint)
{
	const auto* Designed = FindBoundsSlot(Blueprint->WidgetTree);
	const auto* Generated = FindBoundsSlot(GetGeneratedTree(Blueprint));
	return Designed && Generated && Designed->GetPosition() == Generated->GetPosition()
		&& Designed->GetSize() == Generated->GetSize() && Designed->GetAlignment() == Generated->GetAlignment()
		&& Designed->GetAnchors().Minimum == Generated->GetAnchors().Minimum;
}

void UpgradeCityPlacementBounds(const TArray<FString>& Args)
{
	auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, PagePath);
	auto* Tree = Blueprint ? Blueprint->WidgetTree.Get() : nullptr;
	auto* Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	if (!Root || !GetGeneratedTree(Blueprint)
		|| !Tree->FindWidget<UButton>(TEXT("CityPlacementConfirmButton"))
		|| !Tree->FindWidget<UButton>(TEXT("CityPlacementCancelButton")))
	{
		UE_LOG(LogRokCityPlacementBounds, Error, TEXT("RokUiWbp: City placement bounds require the existing Canvas and two commands"));
		return;
	}
	if (Tree->FindWidget(BoundsName) || GetGeneratedTree(Blueprint)->FindWidget(BoundsName))
	{
		if (!HasMatchingBounds(Blueprint))
		{
			UE_LOG(LogRokCityPlacementBounds, Error, TEXT("RokUiWbp: Existing city placement bounds disagree with the generated tree or have an incompatible layout"));
			return;
		}
		UE_LOG(LogRokCityPlacementBounds, Display, TEXT("RokUiWbp: city placement bounds verified; changed=0"));
		return;
	}
	if (Args.Contains(TEXT("-Inspect")))
	{
		UE_LOG(LogRokCityPlacementBounds, Display, TEXT("RokUiWbp: city placement bounds can be added; changed=0"));
		return;
	}
	if (Args.Contains(TEXT("-VerifyOnly")))
	{
		UE_LOG(LogRokCityPlacementBounds, Error, TEXT("RokUiWbp: City placement bounds were not saved"));
		return;
	}

	Blueprint->Modify();
	Tree->Modify();
	Root->Modify();
	auto* Marker = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), BoundsName);
	Marker->SetVisibility(ESlateVisibility::Collapsed);
	auto* BoundsSlot = Root->AddChildToCanvas(Marker);
	BoundsSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	BoundsSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	BoundsSlot->SetPosition(FVector2D(0.0f, -55.0f));
	BoundsSlot->SetSize(FVector2D(472.7f, 220.0f));
	Blueprint->OnVariableAdded(BoundsName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error || !HasMatchingBounds(Blueprint))
	{
		UE_LOG(LogRokCityPlacementBounds, Error, TEXT("RokUiWbp: City placement bounds did not compile; package was not saved"));
		return;
	}

	UPackage* Package = Blueprint->GetOutermost();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
	{
		UE_LOG(LogRokCityPlacementBounds, Error, TEXT("RokUiWbp: Could not save the city placement bounds"));
		return;
	}
	UE_LOG(LogRokCityPlacementBounds, Display, TEXT("RokUiWbp: city placement bounds verified; changed=1"));
}

FAutoConsoleCommand CityPlacementBoundsCommand(
	TEXT("Rok.UiWbp.UpgradeCityPlacementBounds"),
	TEXT("Add hidden host bounds for city placement. Pass -Inspect or -VerifyOnly for read-only checks."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&UpgradeCityPlacementBounds));
}
