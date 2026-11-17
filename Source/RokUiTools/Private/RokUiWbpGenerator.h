#pragma once

#include "CoreMinimal.h"

class UPanelWidget;
class UUserWidget;
class UWidget;
class UWidgetBlueprint;

namespace RokUiWbp
{
	struct FCopyReport
	{
		int32 Widgets = 0;
		int32 RoleTexts = 0;
		int32 AnchoredSlices = 0;
		TArray<FString> Warnings;
	};

	struct FGenerateRequest
	{
		/** Long package name of the new asset, e.g. /Game/RokUI/Modal/WBP_RokModalWindowType2. */
		FString AssetPath;
		/** Parent class of the generated blueprint (a URokUiReusableWidget subclass for pieces). */
		TSubclassOf<UUserWidget> ParentClass;
		/** Name of the generated root canvas. */
		FName RootName = TEXT("PageRoot");
		/** Live widgets copied (with their canvas slots) under the root canvas. */
		TArray<const UWidget*> Sources;
		/** Stripped from the start of names; becomes {Prefix} in the legacy-name patterns. */
		FString Prefix;
		/** Stripped once from names (e.g. "_0"); becomes {Suffix} in the legacy-name patterns. */
		FString Suffix;
		/** Like Suffix, for names that do not contain it (an item's list index); becomes {Index}. */
		FString IndexSuffix;
		/** Exact renames (old flat name -> new name), applied before prefix/suffix stripping. */
		TMap<FString, FString> Renames;
		/** Panels copied without their children (by old name), e.g. the list's row canvas. */
		TSet<FString> SkipChildrenOf;
		/** Panels whose slot is stretched to fill their parent canvas (by new name). */
		TSet<FString> FillParent;
		/** Widgets (by new name) that start collapsed; a page records their designed visibility. */
		TSet<FString> Collapse;
	};

	/**
	 * Copies live C++-built widgets into a new Widget Blueprint (create-only:
	 * refuses when the asset exists), compiles and saves it. Nine-slice
	 * canvases ("*Slices" with "*Slice_<i>" children) get edge/corner anchors so
	 * the piece can be resized in the designer and at runtime.
	 */
	UWidgetBlueprint* Generate(const FGenerateRequest& Request, FCopyReport& Report);

	/** Writes the live tree under Root as JSON (class, name, slot layout) for tree diffs. */
	bool DumpTree(const UWidget* Root, const FString& Filename);
}
