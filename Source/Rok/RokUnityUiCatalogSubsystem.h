#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokUnityUiCatalogSubsystem.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct FRokUnityUiCatalogSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 MainInterfaceModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 UiSliceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 TextureSourceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 ExistingTextureSourceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 TextureTargetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 ExistingTextureTargetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 AcceptedRuntimeTargetCount = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityUiPrefabStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	bool bExists = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 NamedObjectCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	int32 SpriteReferenceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	TArray<FString> SampleNames;
};

USTRUCT(BlueprintType)
struct FRokUnityUiSlice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FString PrefabPath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FString ViewPath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FString MediatorPath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FString UeTargetPath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	bool bPrefabSourceExists = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	bool bViewSourceExists = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	bool bMediatorSourceExists = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	bool bUeTargetExists = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FRokUnityUiPrefabStats PrefabStats;
};

USTRUCT(BlueprintType)
struct FRokUnityUiTextureStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	FString Path;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity UI")
	bool bExists = false;
};

UCLASS()
class ROK_API URokUnityUiCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Rok Unity UI")
	bool LoadUiCatalog();

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	bool IsUiCatalogLoaded() const { return bUiCatalogLoaded; }

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	FRokUnityUiCatalogSummary GetSummary() const { return Summary; }

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	TArray<FString> GetMainInterfaceModules() const { return MainInterfaceModules; }

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	TArray<FRokUnityUiSlice> GetUiSlices() const { return UiSlices; }

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	TArray<FRokUnityUiTextureStatus> GetUiTextureSources() const { return UiTextureSources; }

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	TArray<FRokUnityUiTextureStatus> GetUeTextureTargets() const { return UeTextureTargets; }

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	TArray<FString> GetAcceptedRuntimeTargets() const { return AcceptedRuntimeTargets; }

	UFUNCTION(BlueprintPure, Category="Rok Unity UI")
	FText GetUiCatalogStatusText() const;

private:
	FRokUnityUiCatalogSummary Summary;
	TArray<FString> MainInterfaceModules;
	TArray<FRokUnityUiSlice> UiSlices;
	TArray<FRokUnityUiTextureStatus> UiTextureSources;
	TArray<FRokUnityUiTextureStatus> UeTextureTargets;
	TArray<FString> AcceptedRuntimeTargets;
	bool bUiCatalogLoaded = false;

	static FRokUnityUiSlice ParseUiSlice(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityUiPrefabStats ParsePrefabStats(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityUiTextureStatus ParseTextureStatus(const TSharedPtr<FJsonObject>& Object);
};
