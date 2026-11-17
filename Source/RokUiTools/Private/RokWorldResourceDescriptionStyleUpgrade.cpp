#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogRokWorldResourceDescriptionStyle, Log, All);

constexpr TCHAR PageObjectPath[] = TEXT("/Game/RokUI/Hud/WBP_RokWorldObjectInfo.WBP_RokWorldObjectInfo");
constexpr TCHAR StylePackagePath[] = TEXT("/Game/RokUI/Hud/DT_RokWorldResourceDescriptionStyles");
constexpr TCHAR StyleObjectPath[] = TEXT("/Game/RokUI/Hud/DT_RokWorldResourceDescriptionStyles.DT_RokWorldResourceDescriptionStyles");
const FName Description(TEXT("WorldObjectInfoDetailsText"));

bool SaveAsset(UObject* Asset)
{
	UPackage* Package = Asset->GetOutermost();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
}

bool IsStyleAsset(const UDataTable* Styles)
{
	return Styles && Styles->IsAsset() && Styles->GetRowStruct() == FRichTextStyleRow::StaticStruct()
		&& Styles->FindRow<FRichTextStyleRow>(TEXT("Default"), TEXT("World resource description"), false)
		&& Styles->FindRow<FRichTextStyleRow>(TEXT("b"), TEXT("World resource description"), false);
}

UDataTable* LoadOrCreateStyleAsset()
{
	if (FPackageName::DoesPackageExist(StylePackagePath))
	{
		return LoadObject<UDataTable>(nullptr, StyleObjectPath);
	}
	auto* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	if (!Font)
	{
		return nullptr;
	}
	UPackage* Package = CreatePackage(StylePackagePath);
	auto* Styles = NewObject<UDataTable>(Package, TEXT("DT_RokWorldResourceDescriptionStyles"), RF_Public | RF_Standalone | RF_Transactional);
	Styles->RowStruct = FRichTextStyleRow::StaticStruct();
	FRichTextStyleRow Regular;
	Regular.TextStyle.SetFont(FSlateFontInfo(Font, 20, TEXT("Regular")));
	Regular.TextStyle.SetColorAndOpacity(FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1));
	Regular.TextStyle.SetShadowOffset(FVector2D::ZeroVector);
	Styles->AddRow(TEXT("Default"), Regular);
	FRichTextStyleRow Bold = Regular;
	Bold.TextStyle.SetFont(FSlateFontInfo(Font, 20, TEXT("Bold")));
	Styles->AddRow(TEXT("b"), Bold);
	FAssetRegistryModule::AssetCreated(Styles);
	Package->MarkPackageDirty();
	return SaveAsset(Styles) ? Styles : nullptr;
}

const URichTextBlock* FindGeneratedDescription(const UWidgetBlueprint* Blueprint)
{
	const auto* Generated = Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	const auto* Tree = Generated ? Generated->GetWidgetTreeArchetype() : nullptr;
	return Tree ? Tree->FindWidget<URichTextBlock>(Description) : nullptr;
}

void UpgradeWorldResourceDescriptionStyle(const TArray<FString>& Args)
{
	const bool bVerifyOnly = Args.Contains(TEXT("-VerifyOnly"));
	auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, PageObjectPath);
	auto* Text = Blueprint && Blueprint->WidgetTree ? Blueprint->WidgetTree->FindWidget<URichTextBlock>(Description) : nullptr;
	if (!Text || !FindGeneratedDescription(Blueprint))
	{
		UE_LOG(LogRokWorldResourceDescriptionStyle, Error, TEXT("RokUiWbp: WorldObjectInfo has no authored/generated description RichTextBlock"));
		return;
	}
	if (const UDataTable* Existing = Text->GetTextStyleSet())
	{
		if (!IsStyleAsset(Existing) || FindGeneratedDescription(Blueprint)->GetTextStyleSet() != Existing)
		{
			UE_LOG(LogRokWorldResourceDescriptionStyle, Error, TEXT("RokUiWbp: Existing world resource styles have incompatible or unsaved rows; assets preserved"));
			return;
		}
		UE_LOG(LogRokWorldResourceDescriptionStyle, Display, TEXT("RokUiWbp: world resource description style verified: %s; changed=0"), *Existing->GetPathName());
		return;
	}
	if (bVerifyOnly)
	{
		UE_LOG(LogRokWorldResourceDescriptionStyle, Error, TEXT("RokUiWbp: world resource description style is unset"));
		return;
	}
	UDataTable* Styles = LoadOrCreateStyleAsset();
	if (!IsStyleAsset(Styles))
	{
		UE_LOG(LogRokWorldResourceDescriptionStyle, Error, TEXT("RokUiWbp: could not prepare persistent world resource style rows"));
		return;
	}
	Blueprint->Modify();
	Text->Modify();
	Text->SetTextStyleSet(Styles);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	const auto* GeneratedText = FindGeneratedDescription(Blueprint);
	if (Blueprint->Status == BS_Error || !GeneratedText || GeneratedText->GetTextStyleSet() != Styles || !SaveAsset(Blueprint))
	{
		UE_LOG(LogRokWorldResourceDescriptionStyle, Error, TEXT("RokUiWbp: world resource description style could not be compiled and saved"));
		return;
	}
	UE_LOG(LogRokWorldResourceDescriptionStyle, Display, TEXT("RokUiWbp: world resource description style verified: %s; changed=1"), *Styles->GetPathName());
}

FAutoConsoleCommand WorldResourceDescriptionStyleCommand(
	TEXT("Rok.UiWbp.UpgradeWorldResourceDescriptionStyle"),
	TEXT("Assign persistent description styles to WorldObjectInfo. Pass -VerifyOnly to inspect the saved binding."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&UpgradeWorldResourceDescriptionStyle));
}
