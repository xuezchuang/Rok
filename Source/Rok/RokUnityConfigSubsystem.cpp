#include "RokUnityConfigSubsystem.h"

#include "RokDataPaths.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* ManifestRelativePath = TEXT("ConfigTables/config_tables_manifest.json");

FName JsonNameField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? FName(*Value) : NAME_None;
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

	double Value = 0.0;
	if (Object->TryGetNumberField(FieldName, Value))
	{
		return FMath::RoundToInt(Value);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return FCString::Atoi(*StringValue);
	}
	return 0;
}
}

void URokUnityConfigSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadManifest();
}

bool URokUnityConfigSubsystem::LoadManifest()
{
	TableInfos.Reset();
	LoadedTables.Reset();
	TotalRowCount = 0;
	bManifestLoaded = false;

	const FString ManifestPath = FPaths::ConvertRelativePathToFull(RokDataPaths::Resolve(ManifestRelativePath));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity config manifest not found: %s"), *ManifestPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity config manifest is invalid JSON: %s"), *ManifestPath);
		return false;
	}

	TotalRowCount = JsonIntField(RootObject, TEXT("total_rows"));
	const TArray<TSharedPtr<FJsonValue>>* Tables = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("tables"), Tables) || !Tables)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity config manifest has no tables array: %s"), *ManifestPath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& TableValue : *Tables)
	{
		const TSharedPtr<FJsonObject> TableObject = TableValue.IsValid() ? TableValue->AsObject() : nullptr;
		if (!TableObject.IsValid())
		{
			continue;
		}

		FRokUnityConfigTableInfo Info;
		Info.TableName = JsonNameField(TableObject, TEXT("table_name"));
		Info.SourcePath = JsonStringField(TableObject, TEXT("source_path"));
		Info.JsonPath = JsonStringField(TableObject, TEXT("json_path"));
		Info.CsvPath = JsonStringField(TableObject, TEXT("csv_path"));
		Info.KeyColumn = JsonNameField(TableObject, TEXT("key_column"));
		Info.ColumnCount = JsonIntField(TableObject, TEXT("column_count"));
		Info.RowCount = JsonIntField(TableObject, TEXT("row_count"));
		if (!Info.TableName.IsNone() && !Info.JsonPath.IsEmpty())
		{
			TableInfos.Add(Info.TableName, Info);
		}
	}

	bManifestLoaded = TableInfos.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("Rok Unity config manifest loaded: tables=%d total_rows=%d"), TableInfos.Num(), TotalRowCount);
	return bManifestLoaded;
}

TArray<FName> URokUnityConfigSubsystem::GetKnownTableNames() const
{
	TArray<FName> Names;
	TableInfos.GetKeys(Names);
	Names.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
	return Names;
}

bool URokUnityConfigSubsystem::GetTableInfo(FName TableName, FRokUnityConfigTableInfo& OutInfo) const
{
	if (const FRokUnityConfigTableInfo* Info = TableInfos.Find(TableName))
	{
		OutInfo = *Info;
		return true;
	}
	OutInfo = FRokUnityConfigTableInfo();
	return false;
}

bool URokUnityConfigSubsystem::LoadTable(FName TableName)
{
	if (LoadedTables.Contains(TableName))
	{
		return true;
	}
	if (!bManifestLoaded && !LoadManifest())
	{
		return false;
	}

	const FRokUnityConfigTableInfo* Info = TableInfos.Find(TableName);
	if (!Info)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity config table is unknown: %s"), *TableName.ToString());
		return false;
	}

	const FString TablePath = ResolveProjectRelativePath(Info->JsonPath);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *TablePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity config table not found: %s"), *TablePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity config table is invalid JSON: %s"), *TablePath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("rows"), Rows) || !Rows)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity config table has no rows: %s"), *TablePath);
		return false;
	}

	TArray<TSharedPtr<FJsonObject>> ParsedRows;
	ParsedRows.Reserve(Rows->Num());
	for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
	{
		TSharedPtr<FJsonObject> RowObject = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
		if (RowObject.IsValid())
		{
			ParsedRows.Add(RowObject);
		}
	}

	LoadedTables.Add(TableName, MoveTemp(ParsedRows));
	UE_LOG(LogTemp, Log, TEXT("Rok Unity config table loaded: %s rows=%d"), *TableName.ToString(), LoadedTables[TableName].Num());
	return true;
}

bool URokUnityConfigSubsystem::IsTableLoaded(FName TableName) const
{
	return LoadedTables.Contains(TableName);
}

int32 URokUnityConfigSubsystem::GetTableRowCount(FName TableName) const
{
	if (const TArray<TSharedPtr<FJsonObject>>* Rows = LoadedTables.Find(TableName))
	{
		return Rows->Num();
	}
	if (const FRokUnityConfigTableInfo* Info = TableInfos.Find(TableName))
	{
		return Info->RowCount;
	}
	return 0;
}

bool URokUnityConfigSubsystem::GetIntFieldByKey(FName TableName, FName KeyField, int32 KeyValue, FName FieldName, int32& OutValue)
{
	double NumberValue = 0.0;
	const TSharedPtr<FJsonObject>* Row = FindRowByIntKey(TableName, KeyField, KeyValue);
	if (!Row || !ReadNumberField(*Row, FieldName, NumberValue))
	{
		OutValue = 0;
		return false;
	}
	OutValue = FMath::RoundToInt(NumberValue);
	return true;
}

bool URokUnityConfigSubsystem::GetFloatFieldByKey(FName TableName, FName KeyField, int32 KeyValue, FName FieldName, float& OutValue)
{
	double NumberValue = 0.0;
	const TSharedPtr<FJsonObject>* Row = FindRowByIntKey(TableName, KeyField, KeyValue);
	if (!Row || !ReadNumberField(*Row, FieldName, NumberValue))
	{
		OutValue = 0.0f;
		return false;
	}
	OutValue = static_cast<float>(NumberValue);
	return true;
}

bool URokUnityConfigSubsystem::GetStringFieldByKey(FName TableName, FName KeyField, int32 KeyValue, FName FieldName, FString& OutValue)
{
	const TSharedPtr<FJsonObject>* Row = FindRowByIntKey(TableName, KeyField, KeyValue);
	if (!Row || !ReadStringField(*Row, FieldName, OutValue))
	{
		OutValue.Reset();
		return false;
	}
	return true;
}

const TArray<TSharedPtr<FJsonObject>>* URokUnityConfigSubsystem::GetTableRows(FName TableName)
{
	if (!LoadedTables.Contains(TableName) && !LoadTable(TableName))
	{
		return nullptr;
	}
	return LoadedTables.Find(TableName);
}

FString URokUnityConfigSubsystem::ResolveProjectRelativePath(const FString& RelativePath) const
{
	if (FPaths::IsRelative(RelativePath))
	{
		return FPaths::ConvertRelativePathToFull(RokDataPaths::ResolveProjectPath(RelativePath));
	}
	return RelativePath;
}

const TSharedPtr<FJsonObject>* URokUnityConfigSubsystem::FindRowByIntKey(FName TableName, FName KeyField, int32 KeyValue)
{
	const TArray<TSharedPtr<FJsonObject>>* Rows = GetTableRows(TableName);
	if (!Rows)
	{
		return nullptr;
	}

	for (const TSharedPtr<FJsonObject>& Row : *Rows)
	{
		double NumberValue = 0.0;
		if (ReadNumberField(Row, KeyField, NumberValue) && FMath::RoundToInt(NumberValue) == KeyValue)
		{
			return &Row;
		}
	}
	return nullptr;
}

bool URokUnityConfigSubsystem::ReadNumberField(const TSharedPtr<FJsonObject>& Row, FName FieldName, double& OutValue)
{
	if (!Row.IsValid())
	{
		return false;
	}

	const FString Field = FieldName.ToString();
	if (Row->TryGetNumberField(Field, OutValue))
	{
		return true;
	}

	FString StringValue;
	if (Row->TryGetStringField(Field, StringValue) && !StringValue.IsEmpty())
	{
		OutValue = FCString::Atod(*StringValue);
		return true;
	}
	return false;
}

bool URokUnityConfigSubsystem::ReadStringField(const TSharedPtr<FJsonObject>& Row, FName FieldName, FString& OutValue)
{
	if (!Row.IsValid())
	{
		return false;
	}

	const FString Field = FieldName.ToString();
	if (Row->TryGetStringField(Field, OutValue))
	{
		return true;
	}

	double NumberValue = 0.0;
	if (Row->TryGetNumberField(Field, NumberValue))
	{
		OutValue = FString::SanitizeFloat(NumberValue);
		return true;
	}

	bool BoolValue = false;
	if (Row->TryGetBoolField(Field, BoolValue))
	{
		OutValue = BoolValue ? TEXT("true") : TEXT("false");
		return true;
	}
	return false;
}
