#include "RokUiSpec.h"

#include "RokUiFonts.h"
#include "RokUiWidgets.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

namespace RokUiSpec
{
bool IsStructuralProperty(const FProperty* Property)
{
	static const TSet<FName> Structural = {
		TEXT("Slot"), TEXT("Slots"), TEXT("Parent"), TEXT("Content"),
		TEXT("ToolTipWidget"), TEXT("Navigation"), TEXT("AccessibleWidgetData"),
		TEXT("NativeBindings"), TEXT("bCreatedByConstructionScript"), TEXT("DesignerFlags"),
		TEXT("bIsVariable"), TEXT("DisplayLabel"), TEXT("CategoryName"),
		TEXT("bExpandedInDesigner"), TEXT("bLockedInDesigner"), TEXT("bHiddenInDesigner"),
		TEXT("WidgetTree"), TEXT("NamedSlotBindings"), TEXT("Extensions"), TEXT("DesiredFocusWidget"),
	};
	if (Structural.Contains(Property->GetFName()))
	{
		return true;
	}
	if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
	{
		return true;
	}
	return Property->IsA<FDelegateProperty>() || Property->IsA<FMulticastDelegateProperty>();
}

namespace
{
	constexpr int32 SpecVersion = 1;

	/** Objects a saved Blueprint cannot point at: runtime textures, dynamic materials, world objects. */
	bool IsRuntimeOnly(const UObject* Object)
	{
		return Object->GetPackage() == GetTransientPackage()
			|| Object->HasAnyFlags(RF_Transient)
			|| !(Object->IsAsset() || Object->IsA<UField>());
	}

	int32 ClearRuntimeObjects(const UStruct* Struct, void* Data)
	{
		int32 Cleared = 0;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				void* Value = It->ContainerPtrToValuePtr<void>(Data, Index);
				if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(*It))
				{
					const UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
					if (Object && IsRuntimeOnly(Object))
					{
						ObjectProperty->SetObjectPropertyValue(Value, nullptr);
						++Cleared;
					}
				}
				else if (const FStructProperty* StructProperty = CastField<FStructProperty>(*It))
				{
					Cleared += ClearRuntimeObjects(StructProperty->Struct, Value);
				}
			}
		}
		return Cleared;
	}

	/** Visits every FSlateFontInfo nested in Struct, with its dotted property path. */
	void ForEachFont(const UStruct* Struct, void* Data, const FString& Prefix, TFunctionRef<void(FSlateFontInfo&, const FString&)> Visit)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(*It);
			if (!StructProperty || IsStructuralProperty(StructProperty))
			{
				continue;
			}
			for (int32 Index = 0; Index < StructProperty->ArrayDim; ++Index)
			{
				void* Value = StructProperty->ContainerPtrToValuePtr<void>(Data, Index);
				FString Path = Prefix.IsEmpty() ? StructProperty->GetName() : Prefix + TEXT(".") + StructProperty->GetName();
				if (StructProperty->ArrayDim > 1)
				{
					Path += FString::Printf(TEXT("[%d]"), Index);
				}
				if (StructProperty->Struct == FSlateFontInfo::StaticStruct())
				{
					Visit(*static_cast<FSlateFontInfo*>(Value), Path);
				}
				else
				{
					ForEachFont(StructProperty->Struct, Value, Path, Visit);
				}
			}
		}
	}

	FString PropertyKey(const FProperty* Property, int32 Index)
	{
		return Property->ArrayDim > 1
			? FString::Printf(TEXT("%s[%d]"), *Property->GetName(), Index)
			: Property->GetName();
	}

	/** The object's properties that differ from its class defaults, as export text. */
	TSharedRef<FJsonObject> ExportProperties(const UObject* Object, const FString& Label, TArray<FString>& Notes)
	{
		TSharedRef<FJsonObject> Props = MakeShared<FJsonObject>();
		const UObject* Defaults = Object->GetClass()->GetDefaultObject();
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (IsStructuralProperty(Property))
			{
				continue;
			}
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
				ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance))
			{
				continue;
			}
			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				const void* Value = Property->ContainerPtrToValuePtr<void>(Object, Index);
				const void* Default = Property->ContainerPtrToValuePtr<void>(Defaults, Index);
				if (Property->Identical(Value, Default, PPF_None))
				{
					continue;
				}
				void* Copy = Property->AllocateAndInitializeValue();
				Property->CopySingleValue(Copy, Value);
				int32 Cleared = 0;
				if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
				{
					const UObject* Referenced = ObjectProperty->GetObjectPropertyValue(Copy);
					if (Referenced && IsRuntimeOnly(Referenced))
					{
						ObjectProperty->SetObjectPropertyValue(Copy, nullptr);
						Cleared = 1;
					}
				}
				else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					Cleared = ClearRuntimeObjects(StructProperty->Struct, Copy);
				}
				FString Text;
				Property->ExportTextItem_Direct(Text, Copy, nullptr, nullptr, PPF_None);
				Property->DestroyAndFreeValue(Copy);
				Props->SetStringField(PropertyKey(Property, Index), Text);
				if (Cleared > 0)
				{
					Notes.Add(FString::Printf(TEXT("%s.%s: %d runtime-only object reference(s) written as None"),
						*Label, *PropertyKey(Property, Index), Cleared));
				}
			}
		}
		return Props;
	}

	TSharedRef<FJsonObject> ExportWidget(const UWidget* Widget, TArray<FString>& Notes)
	{
		const FString Label = Widget->GetName();
		TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("name"), Label);
		Node->SetStringField(TEXT("class"), Widget->GetClass()->GetPathName());
		Node->SetObjectField(TEXT("props"), ExportProperties(Widget, Label, Notes));

		TSharedRef<FJsonObject> Fonts = MakeShared<FJsonObject>();
		ForEachFont(Widget->GetClass(), const_cast<UWidget*>(Widget), FString(), [&](FSlateFontInfo& Font, const FString& Path)
		{
			if (Font.FontObject || !Font.CompositeFont.IsValid())
			{
				return;
			}
			ERokUiFontRole Role = ERokUiFontRole::Asset;
			Fonts->SetStringField(Path, RokUiFonts::Identify(Font, Role)
				? StaticEnum<ERokUiFontRole>()->GetNameStringByValue(static_cast<int64>(Role))
				: TEXT("Unknown"));
		});
		if (Fonts->Values.Num() > 0)
		{
			Node->SetObjectField(TEXT("fonts"), Fonts);
		}

		if (Widget->Slot)
		{
			TSharedRef<FJsonObject> Slot = MakeShared<FJsonObject>();
			Slot->SetStringField(TEXT("class"), Widget->Slot->GetClass()->GetPathName());
			Slot->SetObjectField(TEXT("props"), ExportProperties(Widget->Slot, Label + TEXT(".Slot"), Notes));
			Node->SetObjectField(TEXT("slot"), Slot);
		}

		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			TArray<TSharedPtr<FJsonValue>> Children;
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				const UWidget* Child = Panel->GetChildAt(Index);
				if (Child && !Child->IsA<URokUiReusableWidget>())
				{
					Children.Add(MakeShared<FJsonValueObject>(ExportWidget(Child, Notes)));
				}
			}
			Node->SetArrayField(TEXT("children"), Children);
		}
		return Node;
	}

	/** Collects import errors (unresolved objects, bad text) as warnings instead of log spam. */
	class FWarningCollector final : public FOutputDevice
	{
	public:
		FWarningCollector(TArray<FString>& InWarnings, FString InLabel)
			: Warnings(InWarnings), Label(MoveTemp(InLabel))
		{
		}

		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			Warnings.Add(FString::Printf(TEXT("%s: %s"), *Label, Message));
		}

	private:
		TArray<FString>& Warnings;
		FString Label;
	};

	/**
	 * Struct export without defaults (ExportTextItem_Direct with a null delta, as
	 * ExportProperties writes) leaves out every field that is zero, false, None
	 * or empty. So before a struct is read back, its exported fields are zeroed;
	 * a field missing from the text then gets the value it had when written,
	 * not the class default (e.g. a button brush's DrawAs=NoDrawType is 0).
	 */
	void ZeroPortedFields(const UStruct* Struct, void* Data)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!It->ShouldPort(PPF_None))
			{
				continue;
			}
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				void* Value = It->ContainerPtrToValuePtr<void>(Data, Index);
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(*It))
				{
					ZeroPortedFields(StructProperty->Struct, Value);
				}
				else
				{
					It->ClearValue(Value);
				}
			}
		}
	}

	void ImportProperties(UObject* Object, const TSharedPtr<FJsonObject>& Props, const FString& Label, TArray<FString>& Warnings)
	{
		if (!Props.IsValid())
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Props->Values)
		{
			FString Name = Pair.Key;
			int32 Index = 0;
			int32 Bracket = INDEX_NONE;
			if (Name.FindChar(TEXT('['), Bracket))
			{
				Index = FCString::Atoi(*Name.Mid(Bracket + 1));
				Name.LeftInline(Bracket);
			}
			const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), *Name);
			if (!Property || IsStructuralProperty(Property) || Index < 0 || Index >= Property->ArrayDim)
			{
				Warnings.Add(FString::Printf(TEXT("%s.%s: no such property on %s"), *Label, *Pair.Key, *Object->GetClass()->GetName()));
				continue;
			}
			FString Text;
			Pair.Value->TryGetString(Text);
			FWarningCollector Errors(Warnings, Label + TEXT(".") + Pair.Key);
			void* Value = Property->ContainerPtrToValuePtr<void>(Object, Index);
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				ZeroPortedFields(StructProperty->Struct, Value);
			}
			if (!Property->ImportText_Direct(*Text, Value, Object, PPF_None, &Errors))
			{
				Warnings.Add(FString::Printf(TEXT("%s.%s: could not import \"%s\""), *Label, *Pair.Key, *Text.Left(120)));
			}
		}
	}

	void ImportFonts(UWidget* Widget, const TSharedPtr<FJsonObject>& Fonts, TArray<FString>& Warnings)
	{
		if (!Fonts.IsValid())
		{
			return;
		}
		ForEachFont(Widget->GetClass(), Widget, FString(), [&](FSlateFontInfo& Font, const FString& Path)
		{
			FString RoleName;
			if (!Fonts->TryGetStringField(Path, RoleName))
			{
				return;
			}
			const int64 Role = StaticEnum<ERokUiFontRole>()->GetValueByNameString(RoleName);
			if (Role == INDEX_NONE)
			{
				Warnings.Add(FString::Printf(TEXT("%s.%s: font role %s cannot be rebuilt"), *Widget->GetName(), *Path, *RoleName));
				return;
			}
			Font = RokUiFonts::Resolve(static_cast<ERokUiFontRole>(Role), Font);
		});
	}

	struct FImportContext
	{
		UWidgetTree* Tree;
		TArray<FName>& Added;
		TArray<FString>& Warnings;
	};

	void ImportNode(const TSharedPtr<FJsonObject>& Node, UPanelWidget* Parent, FImportContext& Context)
	{
		const FName Name(*Node->GetStringField(TEXT("name")));
		UWidget* Widget = Context.Tree->FindWidget(Name);
		if (!Widget)
		{
			const FString ClassPath = Node->GetStringField(TEXT("class"));
			UClass* Class = FSoftClassPath(ClassPath).TryLoadClass<UWidget>();
			if (!Class)
			{
				Context.Warnings.Add(FString::Printf(TEXT("%s: class %s not found; subtree skipped"), *Name.ToString(), *ClassPath));
				return;
			}
			if (StaticFindObjectFast(nullptr, Context.Tree, Name))
			{
				Context.Warnings.Add(FString::Printf(TEXT("%s: name already used outside the tree; subtree skipped"), *Name.ToString()));
				return;
			}
			Widget = Context.Tree->ConstructWidget<UWidget>(Class, Name);
			const FString Label = Name.ToString();
			ImportProperties(Widget, Node->GetObjectField(TEXT("props")), Label, Context.Warnings);
			const TSharedPtr<FJsonObject>* Fonts = nullptr;
			if (Node->TryGetObjectField(TEXT("fonts"), Fonts))
			{
				ImportFonts(Widget, *Fonts, Context.Warnings);
			}
			UPanelSlot* Slot = Parent->AddChild(Widget);
			const TSharedPtr<FJsonObject>* SlotNode = nullptr;
			if (Slot && Node->TryGetObjectField(TEXT("slot"), SlotNode))
			{
				if (Slot->GetClass()->GetPathName() == (*SlotNode)->GetStringField(TEXT("class")))
				{
					ImportProperties(Slot, (*SlotNode)->GetObjectField(TEXT("props")), Label + TEXT(".Slot"), Context.Warnings);
				}
				else
				{
					Context.Warnings.Add(FString::Printf(TEXT("%s: slot class %s became %s"), *Label,
						*(*SlotNode)->GetStringField(TEXT("class")), *Slot->GetClass()->GetPathName()));
				}
			}
			Context.Added.Add(Name);
		}

		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
		UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
		if (Panel && Node->TryGetArrayField(TEXT("children"), Children))
		{
			for (const TSharedPtr<FJsonValue>& Child : *Children)
			{
				ImportNode(Child->AsObject(), Panel, Context);
			}
		}
	}
}

bool ExportChildren(const UPanelWidget* Root, const FString& Filename, const FString& Label, FString& OutError)
{
	if (!Root)
	{
		OutError = TEXT("no root panel");
		return false;
	}
	TArray<FString> Notes;
	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (int32 Index = 0; Index < Root->GetChildrenCount(); ++Index)
	{
		const UWidget* Child = Root->GetChildAt(Index);
		if (Child && !Child->IsA<URokUiReusableWidget>())
		{
			Nodes.Add(MakeShared<FJsonValueObject>(ExportWidget(Child, Notes)));
		}
	}
	TSharedRef<FJsonObject> Spec = MakeShared<FJsonObject>();
	Spec->SetNumberField(TEXT("version"), SpecVersion);
	Spec->SetStringField(TEXT("label"), Label);
	Spec->SetStringField(TEXT("root"), Root->GetName());
	Spec->SetArrayField(TEXT("nodes"), Nodes);
	TArray<TSharedPtr<FJsonValue>> NoteValues;
	for (const FString& Note : Notes)
	{
		NoteValues.Add(MakeShared<FJsonValueString>(Note));
	}
	Spec->SetArrayField(TEXT("notes"), NoteValues);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Spec, Writer))
	{
		OutError = TEXT("JSON serialization failed");
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	if (!FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("cannot write %s"), *Filename);
		return false;
	}
	return true;
}

bool ImportChildren(const FString& Filename, UWidgetTree* Tree, UPanelWidget* Parent, TArray<FName>& OutAdded, TArray<FString>& OutWarnings)
{
	FString Json;
	if (!Tree || !Parent || !FFileHelper::LoadFileToString(Json, *Filename))
	{
		OutWarnings.Add(FString::Printf(TEXT("cannot read %s"), *Filename));
		return false;
	}
	TSharedPtr<FJsonObject> Spec;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Spec) || !Spec.IsValid()
		|| Spec->GetIntegerField(TEXT("version")) != SpecVersion)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s is not a version %d widget spec"), *Filename, SpecVersion));
		return false;
	}
	FImportContext Context{Tree, OutAdded, OutWarnings};
	for (const TSharedPtr<FJsonValue>& Node : Spec->GetArrayField(TEXT("nodes")))
	{
		ImportNode(Node->AsObject(), Parent, Context);
	}
	return true;
}
}
