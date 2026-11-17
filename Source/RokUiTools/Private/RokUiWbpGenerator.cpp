#include "RokUiWbpGenerator.h"

#include "RokUiFonts.h"
#include "RokUiSpec.h"
#include "RokUiTextBlock.h"
#include "RokUiWidgets.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/Font.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

namespace RokUiWbp
{
namespace
{
	/** Properties that describe tree structure, editor state or runtime bindings, never style. */
	bool IsSkippedProperty(const FProperty* Property)
	{
		return RokUiSpec::IsStructuralProperty(Property);
	}

	void CopyProperties(const UObject* Source, UObject* Target, const FString& Label, FCopyReport& Report)
	{
		for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (IsSkippedProperty(Property) || Property->Identical_InContainer(Source, Target))
			{
				continue;
			}
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
				ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance))
			{
				Report.Warnings.Add(FString::Printf(TEXT("%s.%s: instanced object not copied"), *Label, *Property->GetName()));
				continue;
			}
			Property->CopyCompleteValue_InContainer(Target, Source);
		}
	}

	UFont* GetPlaceholderFont()
	{
		// Shown by the Details panel only; URokUiTextBlock resolves its role at build time.
		static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		return Roboto;
	}

	/** Replaces in-memory composite fonts nested in style structs with the placeholder asset. */
	void FixNestedFonts(const UStruct* Struct, void* Data, const FString& Path, FCopyReport& Report)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(*It);
			if (!StructProperty)
			{
				continue;
			}
			for (int32 Index = 0; Index < StructProperty->ArrayDim; ++Index)
			{
				void* Value = StructProperty->ContainerPtrToValuePtr<void>(Data, Index);
				const FString ChildPath = Path + TEXT(".") + StructProperty->GetName();
				if (StructProperty->Struct == FSlateFontInfo::StaticStruct())
				{
					FSlateFontInfo& Font = *static_cast<FSlateFontInfo*>(Value);
					if (!Font.FontObject && Font.CompositeFont.IsValid())
					{
						ERokUiFontRole Role = ERokUiFontRole::Asset;
						const bool bKnown = RokUiFonts::Identify(Font, Role);
						Report.Warnings.Add(FString::Printf(TEXT("%s: in-memory font (%s) replaced by the Roboto asset"),
							*ChildPath, bKnown ? *UEnum::GetValueAsString(Role) : TEXT("unknown composite")));
						Font.CompositeFont.Reset();
						Font.FontObject = GetPlaceholderFont();
					}
				}
				else
				{
					FixNestedFonts(StructProperty->Struct, Value, ChildPath, Report);
				}
			}
		}
	}

	void ReportTransientReferences(UObject* Object, const FString& Label, FCopyReport& Report)
	{
		TArray<UObject*> References;
		FReferenceFinder Finder(References, nullptr, false, true, false, false);
		Finder.FindReferences(Object);
		for (const UObject* Reference : References)
		{
			if (Reference && Reference != Object
				&& (Reference->GetPackage() == GetTransientPackage() || Reference->HasAnyFlags(RF_Transient)))
			{
				Report.Warnings.Add(FString::Printf(TEXT("%s: references transient %s %s (lost on save)"),
					*Label, *Reference->GetClass()->GetName(), *Reference->GetName()));
			}
		}
	}

	UClass* PickTargetClass(const UWidget* Source, ERokUiFontRole& OutRole, bool& bOutUnknownFont)
	{
		OutRole = ERokUiFontRole::Asset;
		bOutUnknownFont = false;
		if (Source->GetClass() == UTextBlock::StaticClass())
		{
			const FSlateFontInfo Font = CastChecked<UTextBlock>(Source)->GetFont();
			if (!RokUiFonts::Identify(Font, OutRole))
			{
				bOutUnknownFont = true;
			}
			else if (OutRole != ERokUiFontRole::Asset)
			{
				return URokUiTextBlock::StaticClass();
			}
		}
		return Source->GetClass();
	}

	struct FCopyContext
	{
		const FGenerateRequest& Request;
		UWidgetBlueprint* Blueprint;
		FCopyReport& Report;
		TMap<FName, FString> LegacyNames;
		TMap<FName, ESlateVisibility> ShownVisibility;
	};

	/** New generic name plus the legacy pattern ({Prefix}/{Suffix}) it answers to. */
	FString RenameWidget(const FGenerateRequest& Request, const FString& Legacy, FString& OutPattern)
	{
		if (const FString* Explicit = Request.Renames.Find(Legacy))
		{
			OutPattern = Legacy;
			return *Explicit;
		}
		FString Rest = Legacy;
		FString Pattern;
		if (!Request.Prefix.IsEmpty() && Rest.StartsWith(Request.Prefix, ESearchCase::CaseSensitive) && Rest.Len() > Request.Prefix.Len())
		{
			Rest.RightChopInline(Request.Prefix.Len());
			Pattern = TEXT("{Prefix}");
		}
		const TPair<const FString*, const TCHAR*> Markers[] = {
			{ &Request.Suffix, TEXT("{Suffix}") }, { &Request.IndexSuffix, TEXT("{Index}") } };
		for (const TPair<const FString*, const TCHAR*>& Marker : Markers)
		{
			const int32 At = Marker.Key->IsEmpty() ? INDEX_NONE : Rest.Find(*Marker.Key, ESearchCase::CaseSensitive);
			if (At != INDEX_NONE)
			{
				const FString Before = Rest.Left(At);
				const FString After = Rest.Mid(At + Marker.Key->Len());
				OutPattern = Pattern + Before + Marker.Value + After;
				return Before + After;
			}
		}
		OutPattern = Pattern + Rest;
		return Rest;
	}

	/** Clears brush textures that only exist in memory (imported at runtime); C++ sets them per row. */
	void ClearTransientBrushes(const UStruct* Struct, void* Data, const FString& Path, FCopyReport& Report)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(*It);
			if (!StructProperty)
			{
				continue;
			}
			for (int32 Index = 0; Index < StructProperty->ArrayDim; ++Index)
			{
				void* Value = StructProperty->ContainerPtrToValuePtr<void>(Data, Index);
				if (StructProperty->Struct == FSlateBrush::StaticStruct())
				{
					FSlateBrush& Brush = *static_cast<FSlateBrush*>(Value);
					const UObject* Resource = Brush.GetResourceObject();
					if (Resource && (Resource->GetPackage() == GetTransientPackage() || Resource->HasAnyFlags(RF_Transient)))
					{
						Report.Warnings.Add(FString::Printf(TEXT("%s.%s: runtime-only texture cleared (set by C++)"),
							*Path, *StructProperty->GetName()));
						Brush.SetResourceObject(nullptr);
					}
				}
				else
				{
					ClearTransientBrushes(StructProperty->Struct, Value, Path + TEXT(".") + StructProperty->GetName(), Report);
				}
			}
		}
	}

	/**
	 * RokModalNineSlice draws a sliced sprite as nine images at absolute
	 * positions for one size. Anchoring corners to their corner and stretching
	 * edges/centre keeps the same pixels at that size and makes it resizable.
	 * Patch index i is column i % 3, row i / 3 (RokModalNineSlice::Patch).
	 */
	void AnchorNineSlice(const UPanelWidget* SourceSlices, UPanelWidget* TargetSlices, FCopyReport& Report)
	{
		const UWidget* Owner = SourceSlices->GetParent();
		const UCanvasPanelSlot* OwnerSlot = Owner ? Cast<UCanvasPanelSlot>(Owner->Slot) : nullptr;
		if (!Cast<UCanvasPanel>(TargetSlices) || !OwnerSlot || !SourceSlices->GetName().EndsWith(TEXT("Slices")))
		{
			return;
		}
		const FVector2D Size = OwnerSlot->GetSize();
		auto Axis = [](int32 Cell, double Pos, double Extent, double Total, double& AnchorMin, double& AnchorMax, float& Near, float& Far)
		{
			if (Cell == 0) { AnchorMin = AnchorMax = 0.0; Near = Pos; Far = Extent; }
			else if (Cell == 2) { AnchorMin = AnchorMax = 1.0; Near = Pos - Total; Far = Extent; }
			else { AnchorMin = 0.0; AnchorMax = 1.0; Near = Pos; Far = Total - (Pos + Extent); }
		};
		for (int32 ChildIndex = 0; ChildIndex < TargetSlices->GetChildrenCount(); ++ChildIndex)
		{
			UWidget* Patch = TargetSlices->GetChildAt(ChildIndex);
			UCanvasPanelSlot* PatchSlot = Patch ? Cast<UCanvasPanelSlot>(Patch->Slot) : nullptr;
			const FString Name = Patch ? Patch->GetName() : FString();
			int32 Marker = INDEX_NONE;
			if (!PatchSlot || !Name.FindLastChar(TEXT('_'), Marker) || !Name.Mid(Marker + 1).IsNumeric())
			{
				continue;
			}
			const int32 Index = FCString::Atoi(*Name.Mid(Marker + 1));
			const FVector2D Position = PatchSlot->GetPosition();
			const FVector2D PatchSize = PatchSlot->GetSize();
			FAnchors Anchors;
			FMargin Offsets;
			Axis(Index % 3, Position.X, PatchSize.X, Size.X, Anchors.Minimum.X, Anchors.Maximum.X, Offsets.Left, Offsets.Right);
			Axis(Index / 3, Position.Y, PatchSize.Y, Size.Y, Anchors.Minimum.Y, Anchors.Maximum.Y, Offsets.Top, Offsets.Bottom);
			PatchSlot->SetAnchors(Anchors);
			PatchSlot->SetOffsets(Offsets);
			++Report.AnchoredSlices;
		}
	}

	void CopyWidget(const UWidget* Source, UPanelWidget* TargetParent, FCopyContext& Context)
	{
		FCopyReport& Report = Context.Report;
		UWidgetBlueprint* Blueprint = Context.Blueprint;
		const FString Label = Source->GetName();
		FString Pattern;
		const FString NewName = RenameWidget(Context.Request, Label, Pattern);
		if (Pattern != NewName)
		{
			Context.LegacyNames.Add(FName(*NewName), Pattern);
		}
		ERokUiFontRole Role;
		bool bUnknownFont;
		UClass* TargetClass = PickTargetClass(Source, Role, bUnknownFont);
		UWidget* Target = Blueprint->WidgetTree->ConstructWidget<UWidget>(TargetClass, FName(*NewName));
		CopyProperties(Source, Target, Label, Report);
		Target->bIsVariable = false;

		if (URokUiTextBlock* RoleText = Cast<URokUiTextBlock>(Target))
		{
			RoleText->FontRole = Role;
			FSlateFontInfo Font = RoleText->GetFont();
			Font.CompositeFont.Reset();
			Font.FontObject = GetPlaceholderFont();
			RoleText->SetFont(Font);
			++Report.RoleTexts;
		}
		else if (bUnknownFont)
		{
			Report.Warnings.Add(FString::Printf(TEXT("%s: text font is an unknown in-memory composite"), *Label));
		}
		FixNestedFonts(Target->GetClass(), Target, Label, Report);
		ClearTransientBrushes(Target->GetClass(), Target, Label, Report);
		if (Context.Request.Collapse.Contains(NewName))
		{
			// A widget only some states show: collapsed in the designer, shown by C++.
			Context.ShownVisibility.Add(FName(*NewName), Target->GetVisibility());
			Target->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (Source->IsA<UUserWidget>())
		{
			Report.Warnings.Add(FString::Printf(TEXT("%s: nested user widget %s copied as an instance; its own class builds its content"),
				*Label, *Source->GetClass()->GetName()));
		}

		UPanelSlot* TargetSlot = TargetParent->AddChild(Target);
		if (TargetSlot && Source->Slot)
		{
			if (TargetSlot->GetClass() == Source->Slot->GetClass())
			{
				CopyProperties(Source->Slot, TargetSlot, Label + TEXT(".Slot"), Report);
			}
			else
			{
				Report.Warnings.Add(FString::Printf(TEXT("%s: slot class %s became %s"), *Label,
					*Source->Slot->GetClass()->GetName(), *TargetSlot->GetClass()->GetName()));
			}
		}
		if (Context.Request.FillParent.Contains(NewName))
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TargetSlot))
			{
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CanvasSlot->SetOffsets(FMargin(0.0f));
			}
		}
		Blueprint->OnVariableAdded(Target->GetFName());
		ReportTransientReferences(Target, Label, Report);
		++Report.Widgets;

		const UPanelWidget* SourcePanel = Cast<UPanelWidget>(Source);
		if (SourcePanel && !Context.Request.SkipChildrenOf.Contains(Label))
		{
			UPanelWidget* TargetPanel = CastChecked<UPanelWidget>(Target);
			for (int32 Index = 0; Index < SourcePanel->GetChildrenCount(); ++Index)
			{
				if (const UWidget* Child = SourcePanel->GetChildAt(Index))
				{
					CopyWidget(Child, TargetPanel, Context);
				}
			}
			AnchorNineSlice(SourcePanel, TargetPanel, Report);
		}
	}

	TSharedRef<FJsonObject> DescribeWidget(const UWidget* Widget)
	{
		TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("name"), Widget->GetName());
		Node->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
		Node->SetStringField(TEXT("visibility"), UEnum::GetValueAsString(Widget->GetVisibility()));
		if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			const FAnchorData Layout = CanvasSlot->GetLayout();
			TSharedRef<FJsonObject> Slot = MakeShared<FJsonObject>();
			Slot->SetStringField(TEXT("offsets"), FString::Printf(TEXT("L=%.3f T=%.3f R=%.3f B=%.3f"),
				Layout.Offsets.Left, Layout.Offsets.Top, Layout.Offsets.Right, Layout.Offsets.Bottom));
			Slot->SetStringField(TEXT("anchors"), FString::Printf(TEXT("%s|%s"),
				*Layout.Anchors.Minimum.ToString(), *Layout.Anchors.Maximum.ToString()));
			Slot->SetStringField(TEXT("alignment"), Layout.Alignment.ToString());
			Slot->SetNumberField(TEXT("z"), CanvasSlot->GetZOrder());
			Slot->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
			Node->SetObjectField(TEXT("slot"), Slot);
		}
		else if (Widget->Slot)
		{
			Node->SetStringField(TEXT("slot_class"), Widget->Slot->GetClass()->GetName());
		}
		if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			Node->SetStringField(TEXT("text"), Text->GetText().ToString());
			Node->SetNumberField(TEXT("font_size"), Text->GetFont().Size);
			Node->SetStringField(TEXT("typeface"), Text->GetFont().TypefaceFontName.ToString());
		}
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			TArray<TSharedPtr<FJsonValue>> Children;
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (const UWidget* Child = Panel->GetChildAt(Index))
				{
					Children.Add(MakeShared<FJsonValueObject>(DescribeWidget(Child)));
				}
			}
			Node->SetArrayField(TEXT("children"), Children);
		}
		return Node;
	}
}

UWidgetBlueprint* Generate(const FGenerateRequest& Request, FCopyReport& Report)
{
	if (Request.Sources.IsEmpty() || !Request.ParentClass)
	{
		Report.Warnings.Add(TEXT("no source widgets or parent class"));
		return nullptr;
	}
	const FString PackageName = Request.AssetPath;
	FText Reason;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &Reason))
	{
		Report.Warnings.Add(FString::Printf(TEXT("invalid asset path %s: %s"), *PackageName, *Reason.ToString()));
		return nullptr;
	}
	if (FPackageName::DoesPackageExist(PackageName) || FindPackage(nullptr, *PackageName))
	{
		// A WBP is the source of truth once it exists; never overwrite it from code.
		Report.Warnings.Add(FString::Printf(TEXT("%s already exists; not overwritten"), *PackageName));
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackageName);
	const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
	UWidgetBlueprint* Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
		Package, AssetName, BPTYPE_Normal, Request.ParentClass, nullptr, TEXT("RokUiWbp"), false);
	if (!Blueprint)
	{
		Report.Warnings.Add(TEXT("CreateWidgetBlueprint failed"));
		return nullptr;
	}

	UCanvasPanel* Root = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Request.RootName);
	Blueprint->WidgetTree->RootWidget = Root;
	Blueprint->OnVariableAdded(Root->GetFName());
	FCopyContext Context{Request, Blueprint, Report, {}, {}};
	for (const UWidget* Source : Request.Sources)
	{
		if (Source)
		{
			CopyWidget(Source, Root, Context);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		Report.Warnings.Add(TEXT("blueprint failed to compile (check BindWidget names)"));
		return nullptr;
	}
	if (URokUiReusableWidget* Defaults = Blueprint->GeneratedClass
		? Cast<URokUiReusableWidget>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr)
	{
		Defaults->LegacyNames = Context.LegacyNames;
		if (URokUiPageWidget* Page = Cast<URokUiPageWidget>(Defaults))
		{
			Page->ShownVisibility = Context.ShownVisibility;
		}
	}
	else if (!Context.LegacyNames.IsEmpty())
	{
		Report.Warnings.Add(TEXT("widgets were renamed, but the parent class cannot store legacy names"));
	}

	FAssetRegistryModule::AssetCreated(Blueprint);
	Package->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
	{
		Report.Warnings.Add(FString::Printf(TEXT("saving %s failed"), *Filename));
		return nullptr;
	}
	return Blueprint;
}

bool DumpTree(const UWidget* Root, const FString& Filename)
{
	if (!Root)
	{
		return false;
	}
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(DescribeWidget(Root), Writer);
	return FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
}
