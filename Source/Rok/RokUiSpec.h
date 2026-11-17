#pragma once

#include "CoreMinimal.h"

class UPanelWidget;
class UWidgetTree;

/**
 * Widget-tree specs (JSON) for the WBP conversion. A C++-built tree is
 * written while the game runs, where its data and state exist
 * (-RokUiExportSpec=); the editor tool (RokUiTools) reads it back as
 * transient widgets and turns them into a Widget Blueprint.
 *
 * Per widget: name, class, the properties that differ from the class
 * defaults (as UE export text), the slot, the font role of each in-memory
 * font, and the children. Object references a Blueprint cannot save
 * (runtime textures, dynamic materials) are written as None. Inside a
 * struct, fields that are zero / false / None / empty are left out (UE's
 * export against no defaults), so the importer zeroes a struct before it
 * reads the text back.
 */
namespace RokUiSpec
{
	/** Tree structure, editor state and runtime bindings: never part of a spec or a copy. */
	ROK_API bool IsStructuralProperty(const FProperty* Property);

	/**
	 * Writes the children of Root (not Root itself). Widgets that are already
	 * Widget Blueprint pieces (URokUiReusableWidget) are left out.
	 */
	ROK_API bool ExportChildren(const UPanelWidget* Root, const FString& Filename, const FString& Label, FString& OutError);

	/**
	 * Rebuilds a spec's widgets under Parent, a panel in Tree. A widget whose
	 * name already exists in Tree is not created again; its children are
	 * merged into it. That is how several specs of one mode (one per state) are
	 * combined. Widgets a later spec adds are reported in OutAdded.
	 */
	ROK_API bool ImportChildren(
		const FString& Filename,
		UWidgetTree* Tree,
		UPanelWidget* Parent,
		TArray<FName>& OutAdded,
		TArray<FString>& OutWarnings);
}
