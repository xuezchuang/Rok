#include "RokUnityUiCatalogSubsystem.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* UiCatalogRelativePath = TEXT("Saved/RokUnityMigration/unity_ui_catalog.json");

TSharedPtr<FJsonObject> JsonObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	return Value && Value->IsValid() ? (*Value)->AsObject() : nullptr;
}

FString JsonStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : FString();
}

int32 JsonIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return 0;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return FMath::RoundToInt(NumberValue);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return FCString::Atoi(*StringValue);
	}
	return 0;
}

bool JsonBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return false;
	}

	bool BoolValue = false;
	if (Object->TryGetBoolField(FieldName, BoolValue))
	{
		return BoolValue;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return !FMath::IsNearlyZero(NumberValue);
	}
	return false;
}

void ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, TArray<FString>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	OutValues.Reserve(Values->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (Value.IsValid())
		{
			OutValues.Add(Value->AsString());
		}
	}
}

template <typename ParsedType>
void ReadObjectArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, TFunctionRef<ParsedType(const TSharedPtr<FJsonObject>&)> Parser, TArray<ParsedType>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	OutValues.Reserve(Values->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> ValueObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (ValueObject.IsValid())
		{
			OutValues.Add(Parser(ValueObject));
		}
	}
}

int32 CountExistingTextures(const TArray<FRokUnityUiTextureStatus>& Textures)
{
	int32 Count = 0;
	for (const FRokUnityUiTextureStatus& Texture : Textures)
	{
		if (Texture.bExists)
		{
			++Count;
		}
	}
	return Count;
}
}

void URokUnityUiCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadUiCatalog();
}

bool URokUnityUiCatalogSubsystem::LoadUiCatalog()
{
	Summary = FRokUnityUiCatalogSummary();
	MainInterfaceModules.Reset();
	UiSlices.Reset();
	UiTextureSources.Reset();
	UeTextureTargets.Reset();
	AcceptedRuntimeTargets.Reset();
	bUiCatalogLoaded = false;

	const FString UiCatalogPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), UiCatalogRelativePath);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *UiCatalogPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity UI catalog not found: %s"), *UiCatalogPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity UI catalog is invalid JSON: %s"), *UiCatalogPath);
		return false;
	}

	FString Schema;
	RootObject->TryGetStringField(TEXT("schema"), Schema);
	if (Schema != TEXT("rok.unity_ui_catalog.v1"))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity UI catalog has unexpected schema '%s': %s"), *Schema, *UiCatalogPath);
		return false;
	}

	ReadStringArray(RootObject, TEXT("main_interface_modules"), MainInterfaceModules);
	ReadObjectArray<FRokUnityUiSlice>(
		RootObject,
		TEXT("ui_slices"),
		[](const TSharedPtr<FJsonObject>& Object) { return URokUnityUiCatalogSubsystem::ParseUiSlice(Object); },
		UiSlices);
	ReadObjectArray<FRokUnityUiTextureStatus>(
		RootObject,
		TEXT("ui_texture_sources"),
		[](const TSharedPtr<FJsonObject>& Object) { return URokUnityUiCatalogSubsystem::ParseTextureStatus(Object); },
		UiTextureSources);
	ReadObjectArray<FRokUnityUiTextureStatus>(
		RootObject,
		TEXT("ue_texture_targets"),
		[](const TSharedPtr<FJsonObject>& Object) { return URokUnityUiCatalogSubsystem::ParseTextureStatus(Object); },
		UeTextureTargets);
	ReadStringArray(RootObject, TEXT("accepted_runtime_targets"), AcceptedRuntimeTargets);

	Summary.MainInterfaceModuleCount = MainInterfaceModules.Num();
	Summary.UiSliceCount = UiSlices.Num();
	Summary.TextureSourceCount = UiTextureSources.Num();
	Summary.ExistingTextureSourceCount = CountExistingTextures(UiTextureSources);
	Summary.TextureTargetCount = UeTextureTargets.Num();
	Summary.ExistingTextureTargetCount = CountExistingTextures(UeTextureTargets);
	Summary.AcceptedRuntimeTargetCount = AcceptedRuntimeTargets.Num();

	bUiCatalogLoaded = Summary.MainInterfaceModuleCount > 0 && Summary.UiSliceCount > 0 && Summary.ExistingTextureSourceCount == Summary.TextureSourceCount && Summary.ExistingTextureTargetCount == Summary.TextureTargetCount;
	if (bUiCatalogLoaded)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Loaded Rok Unity UI catalog: modules=%d slices=%d texture_sources=%d/%d texture_targets=%d/%d runtime_targets=%d"),
			Summary.MainInterfaceModuleCount,
			Summary.UiSliceCount,
			Summary.ExistingTextureSourceCount,
			Summary.TextureSourceCount,
			Summary.ExistingTextureTargetCount,
			Summary.TextureTargetCount,
			Summary.AcceptedRuntimeTargetCount);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity UI catalog loaded with missing sections: %s"), *UiCatalogPath);
	}
	return bUiCatalogLoaded;
}

FText URokUnityUiCatalogSubsystem::GetUiCatalogStatusText() const
{
	if (!bUiCatalogLoaded)
	{
		return NSLOCTEXT("RokUnityUI", "UiCatalogNotLoaded", "UI加载中");
	}

	return FText::Format(
		NSLOCTEXT("RokUnityUI", "UiCatalogStatus", "UI 模块{0} 界面{1} 素材{2}/{3}"),
		FText::AsNumber(Summary.MainInterfaceModuleCount),
		FText::AsNumber(Summary.UiSliceCount),
		FText::AsNumber(Summary.ExistingTextureSourceCount),
		FText::AsNumber(Summary.ExistingTextureTargetCount));
}

FRokUnityUiSlice URokUnityUiCatalogSubsystem::ParseUiSlice(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityUiSlice Parsed;
	Parsed.Id = JsonStringField(Object, TEXT("id"));
	Parsed.Name = JsonStringField(Object, TEXT("name"));
	Parsed.PrefabPath = JsonStringField(Object, TEXT("prefab"));
	Parsed.ViewPath = JsonStringField(Object, TEXT("view"));
	Parsed.MediatorPath = JsonStringField(Object, TEXT("mediator"));
	Parsed.UeTargetPath = JsonStringField(Object, TEXT("ue_target"));
	Parsed.bUeTargetExists = JsonBoolField(Object, TEXT("ue_target_exists"));
	Parsed.PrefabStats = ParsePrefabStats(JsonObjectField(Object, TEXT("prefab_stats")));

	const TSharedPtr<FJsonObject> SourceExistsObject = JsonObjectField(Object, TEXT("source_exists"));
	Parsed.bPrefabSourceExists = JsonBoolField(SourceExistsObject, TEXT("prefab"));
	Parsed.bViewSourceExists = JsonBoolField(SourceExistsObject, TEXT("view"));
	Parsed.bMediatorSourceExists = JsonBoolField(SourceExistsObject, TEXT("mediator"));
	return Parsed;
}

FRokUnityUiPrefabStats URokUnityUiCatalogSubsystem::ParsePrefabStats(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityUiPrefabStats Parsed;
	Parsed.bExists = JsonBoolField(Object, TEXT("exists"));
	Parsed.NamedObjectCount = JsonIntField(Object, TEXT("named_object_count"));
	Parsed.SpriteReferenceCount = JsonIntField(Object, TEXT("sprite_reference_count"));
	ReadStringArray(Object, TEXT("sample_names"), Parsed.SampleNames);
	return Parsed;
}

FRokUnityUiTextureStatus URokUnityUiCatalogSubsystem::ParseTextureStatus(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityUiTextureStatus Parsed;
	Parsed.Path = JsonStringField(Object, TEXT("path"));
	Parsed.bExists = JsonBoolField(Object, TEXT("exists"));
	return Parsed;
}
