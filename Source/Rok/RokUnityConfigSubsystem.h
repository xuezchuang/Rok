#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokUnityConfigSubsystem.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct FRokUnityConfigTableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Config")
	FName TableName;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Config")
	FString SourcePath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Config")
	FString JsonPath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Config")
	FString CsvPath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Config")
	FName KeyColumn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Config")
	int32 ColumnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Config")
	int32 RowCount = 0;
};

UCLASS()
class ROK_API URokUnityConfigSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Rok Unity Config")
	bool LoadManifest();

	UFUNCTION(BlueprintPure, Category="Rok Unity Config")
	bool IsManifestLoaded() const { return bManifestLoaded; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Config")
	int32 GetTableCount() const { return TableInfos.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Config")
	int32 GetTotalRowCount() const { return TotalRowCount; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Config")
	TArray<FName> GetKnownTableNames() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Config")
	bool GetTableInfo(FName TableName, FRokUnityConfigTableInfo& OutInfo) const;

	UFUNCTION(BlueprintCallable, Category="Rok Unity Config")
	bool LoadTable(FName TableName);

	UFUNCTION(BlueprintPure, Category="Rok Unity Config")
	bool IsTableLoaded(FName TableName) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Config")
	int32 GetTableRowCount(FName TableName) const;

	UFUNCTION(BlueprintCallable, Category="Rok Unity Config")
	bool GetIntFieldByKey(FName TableName, FName KeyField, int32 KeyValue, FName FieldName, int32& OutValue);

	UFUNCTION(BlueprintCallable, Category="Rok Unity Config")
	bool GetFloatFieldByKey(FName TableName, FName KeyField, int32 KeyValue, FName FieldName, float& OutValue);

	UFUNCTION(BlueprintCallable, Category="Rok Unity Config")
	bool GetStringFieldByKey(FName TableName, FName KeyField, int32 KeyValue, FName FieldName, FString& OutValue);

	const TArray<TSharedPtr<FJsonObject>>* GetTableRows(FName TableName);

private:
	TMap<FName, FRokUnityConfigTableInfo> TableInfos;
	TMap<FName, TArray<TSharedPtr<FJsonObject>>> LoadedTables;
	bool bManifestLoaded = false;
	int32 TotalRowCount = 0;

	FString ResolveProjectRelativePath(const FString& RelativePath) const;
	const TSharedPtr<FJsonObject>* FindRowByIntKey(FName TableName, FName KeyField, int32 KeyValue);
	static bool ReadNumberField(const TSharedPtr<FJsonObject>& Row, FName FieldName, double& OutValue);
	static bool ReadStringField(const TSharedPtr<FJsonObject>& Row, FName FieldName, FString& OutValue);
};
