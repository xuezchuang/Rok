#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RokUiWidgets.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogRokAllianceFlagStyleUpgrade, Log, All);

struct FStyleSource
{
	const TCHAR* Asset;
	int32 TabCount;
};

constexpr FStyleSource Sources[] = {
	{TEXT("WBP_RokFeature_AllianceFlagEditor"), 4},
	{TEXT("WBP_RokFeature_AllianceFlagSpriteOption"), 0},
	{TEXT("WBP_RokFeature_AllianceFlagColorOption"), 0},
};

struct FStylePatch
{
	UBorder* Original = nullptr;
	UBorder* Selected = nullptr;
	UCanvasPanel* Parent = nullptr;
	UCanvasPanelSlot* Slot = nullptr;
	FName SelectedName;
	bool bTab = false;
	bool bOriginallySelected = false;
};

struct FAssetPatch
{
	UWidgetBlueprint* Blueprint = nullptr;
	FString Filename;
	TArray<FStylePatch> Styles;
};

bool ReadAsset(const FStyleSource& Source, FAssetPatch& Out)
{
	const FString PackageName = FString(TEXT("/Game/RokUI/Feature/")) + Source.Asset;
	const FString ObjectPath = PackageName + TEXT(".") + Source.Asset;
	Out.Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	Out.Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	const auto* Defaults = Out.Blueprint && Out.Blueprint->GeneratedClass
		? Cast<URokUiPageWidget>(Out.Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Defaults || !Out.Blueprint->WidgetTree || !IFileManager::Get().FileExists(*Out.Filename))
	{
		UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: alliance flag styles cannot load %s"), *ObjectPath);
		return false;
	}
	const bool bTabs = Source.TabCount > 0;
	for (int32 Index = 1; Index <= FMath::Max(1, Source.TabCount); ++Index)
	{
		const FString BorderName = bTabs ? FString::Printf(TEXT("AllianceFlagTabPanel_%d"), Index) : FString(TEXT("Panel"));
		FStylePatch Patch;
		Patch.Original = Out.Blueprint->WidgetTree->FindWidget<UBorder>(*BorderName);
		Patch.SelectedName = FName(*(BorderName + TEXT("Selected")));
		Patch.bTab = bTabs;
		Patch.bOriginallySelected = bTabs && Index == 3;
		Patch.Parent = Patch.Original ? Cast<UCanvasPanel>(Patch.Original->GetParent()) : nullptr;
		Patch.Slot = Patch.Original ? Cast<UCanvasPanelSlot>(Patch.Original->Slot) : nullptr;
		UWidget* Existing = Out.Blueprint->WidgetTree->FindWidget(Patch.SelectedName);
		Patch.Selected = Cast<UBorder>(Existing);
		if (!Patch.Parent || !Patch.Slot || Patch.Original->GetChildrenCount() != 0
			|| (Existing && (!Patch.Selected || Patch.Selected->GetParent() != Patch.Parent)))
		{
			UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: alliance flag styles unexpected structure at %s.%s"), Source.Asset, *BorderName);
			return false;
		}
		Out.Styles.Add(Patch);
	}
	return true;
}

bool HasBackup(const FAssetPatch& Asset, const FString& Directory)
{
	TArray<uint8> Original;
	TArray<uint8> Backup;
	const FString BackupPath = Directory / FPaths::GetCleanFilename(Asset.Filename);
	return FFileHelper::LoadFileToArray(Original, *Asset.Filename) && FFileHelper::LoadFileToArray(Backup, *BackupPath)
		&& Original == Backup && !IFileManager::Get().IsReadOnly(*Asset.Filename);
}

bool NeedsPatch(const FAssetPatch& Asset, const FStylePatch& Style)
{
	const FGuid* Guid = Asset.Blueprint->WidgetVariableNameToGuidMap.Find(Style.SelectedName);
	return !Style.Selected || !Guid || !Guid->IsValid();
}

bool ApplyAsset(FAssetPatch& Asset)
{
	UWidgetBlueprint* Blueprint = Asset.Blueprint;
	const auto* OldDefaults = CastChecked<URokUiPageWidget>(Blueprint->GeneratedClass->GetDefaultObject());
	const auto LegacyNames = OldDefaults->LegacyNames;
	const auto ShownVisibility = OldDefaults->ShownVisibility;
	Blueprint->Modify();
	Blueprint->WidgetTree->Modify();
	for (FStylePatch& Style : Asset.Styles)
	{
		if (!NeedsPatch(Asset, Style)) continue;
		const FGuid* Guid = Blueprint->WidgetVariableNameToGuidMap.Find(Style.SelectedName);
		if (!Guid || !Guid->IsValid())
		{
			Blueprint->WidgetVariableNameToGuidMap.Remove(Style.SelectedName);
			Blueprint->OnVariableAdded(Style.SelectedName);
		}
		if (Style.Selected) continue;
		Style.Parent->Modify();
		Style.Original->Modify();
		Style.Selected = DuplicateObject<UBorder>(Style.Original, Blueprint->WidgetTree, Style.SelectedName);
		Style.Selected->Slot = nullptr;
		UCanvasPanelSlot* Slot = Style.Parent->AddChildToCanvas(Style.Selected);
		if (!Slot) return false;
		Slot->SetLayout(Style.Slot->GetLayout());
		Slot->SetAutoSize(Style.Slot->GetAutoSize());
		Slot->SetZOrder(Style.Slot->GetZOrder());
		// The page was captured on the emblem tab. Preserve its authored selected appearance.
		if (Style.bOriginallySelected)
		{
			Style.Original->SetBrushColor(FLinearColor(0.22f, 0.21f, 0.20f, 1.0f));
			Style.Original->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Style.Selected->SetBrushColor(Style.bTab ? FLinearColor(0.48f, 0.36f, 0.14f, 1.0f)
				: FLinearColor(0.67f, 0.48f, 0.13f, 1.0f));
			Style.Selected->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	auto* NewDefaults = Blueprint->GeneratedClass ? Cast<URokUiPageWidget>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (Blueprint->Status == BS_Error || !NewDefaults) return false;
	NewDefaults->LegacyNames = LegacyNames;
	NewDefaults->ShownVisibility = ShownVisibility;
	Blueprint->MarkPackageDirty();
	return true;
}

bool VerifyCompiledStyles(const FAssetPatch& Asset)
{
	UWidgetBlueprint* Blueprint = Asset.Blueprint;
	UPackage* Package = Blueprint->GetOutermost();
	const bool bWasDirty = Package->IsDirty();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Package->SetDirtyFlag(bWasDirty);
	const auto* Class = Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	const auto* Tree = Class ? Class->GetWidgetTreeArchetype() : nullptr;
	if (Blueprint->Status == BS_Error || !Tree) return false;
	return !Asset.Styles.ContainsByPredicate([Tree](const FStylePatch& Style)
	{
		return !Tree->FindWidget<UBorder>(Style.SelectedName);
	});
}

void Upgrade(const TArray<FString>& Args)
{
	const bool bValidate = Args.Num() == 1 && Args[0] == TEXT("ValidateOnly");
	const bool bVerify = Args.Num() == 1 && Args[0] == TEXT("VerifyOnly");
	bool bApply = Args.Num() == 2 && Args[0] == TEXT("Apply") && !Args[1].IsEmpty();
	if (bApply)
	{
		for (TCHAR Character : Args[1])
		{
			if (FChar::IsDigit(Character) || Character == TEXT('-')) continue;
			bApply = false;
			break;
		}
	}
	if (!bValidate && !bVerify && !bApply)
	{
		UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: use UpgradeAllianceFlagStyles ValidateOnly, VerifyOnly, or Apply <backup id>"));
		return;
	}
	TArray<FAssetPatch> Assets;
	int32 Pending = 0;
	for (const FStyleSource& Source : Sources)
	{
		FAssetPatch& Asset = Assets.AddDefaulted_GetRef();
		if (!ReadAsset(Source, Asset)) return;
		for (const FStylePatch& Style : Asset.Styles) if (NeedsPatch(Asset, Style)) ++Pending;
	}
	if (bValidate || bVerify)
	{
		if (bVerify && Pending > 0)
		{
			UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: alliance flag styles verification failed pending=%d"), Pending);
			return;
		}
		if (bVerify)
		{
			for (const FAssetPatch& Asset : Assets)
			{
				if (VerifyCompiledStyles(Asset)) continue;
				UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: alliance flag styles verification could not compile %s"), *Asset.Filename);
				return;
			}
		}
		UE_LOG(LogRokAllianceFlagStyleUpgrade, Display, TEXT("RokUiWbp: alliance flag styles %s passed pending=%d"),
			bValidate ? TEXT("preflight") : TEXT("verification"), Pending);
		return;
	}
	if (Pending == 0)
	{
		UE_LOG(LogRokAllianceFlagStyleUpgrade, Display, TEXT("RokUiWbp: alliance flag styles already upgraded; no assets saved"));
		return;
	}
	const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("RokUi/AllianceFlagStyleUpgradeBackups") / Args[1];
	for (const FAssetPatch& Asset : Assets)
	{
		if (!HasBackup(Asset, BackupDir))
		{
			UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: missing byte-matched backup or asset is read-only %s"), *Asset.Filename);
			return;
		}
	}
	TArray<const FAssetPatch*> Changed;
	for (FAssetPatch& Asset : Assets)
	{
		if (!Asset.Styles.ContainsByPredicate([&Asset](const FStylePatch& Style) { return NeedsPatch(Asset, Style); })) continue;
		if (!ApplyAsset(Asset))
		{
			UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: alliance flag styles could not compile %s"), *Asset.Filename);
			return;
		}
		Changed.Add(&Asset);
	}
	for (const FAssetPatch* Asset : Changed)
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		if (!UPackage::SavePackage(Asset->Blueprint->GetOutermost(), Asset->Blueprint, *Asset->Filename, SaveArgs))
		{
			UE_LOG(LogRokAllianceFlagStyleUpgrade, Error, TEXT("RokUiWbp: alliance flag styles failed to save %s"), *Asset->Filename);
			return;
		}
	}
	UE_LOG(LogRokAllianceFlagStyleUpgrade, Display, TEXT("RokUiWbp: alliance flag styles upgrade saved assets=%d styles=%d"), Changed.Num(), Pending);
}

FAutoConsoleCommand UpgradeAllianceFlagStyles(
	TEXT("Rok.UiWbp.UpgradeAllianceFlagStyles"),
	TEXT("Upgrade alliance flag selection styles in place. ValidateOnly, VerifyOnly, or Apply <backup id>."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&Upgrade));
}
