#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokWorldMapEntitySubsystem.generated.h"

USTRUCT(BlueprintType)
struct FRokWorldMapFormationUnit
{
	GENERATED_BODY()

	// Unity BarbarianFormation_{modelId} unit prefab name (e.g. Arms2_1).
	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FString UnitPrefab;

	// Formation-root-relative offset in UE centimeters, from the Unity
	// prefab's row + unit_dummy transforms.
	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FVector UeOffsetCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FString TexturePath;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float WidthCm = 60.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float HeightCm = 100.0f;

	// Unity sprite pivot fractions; y near 0 anchors the unit's feet on
	// the ground point.
	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float PivotX = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float PivotY = 0.5f;

	// Idle-frame crops (direction 1), in playback order.
	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	TArray<FString> FrameTexturePaths;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	TArray<FString> FadeFrameTexturePaths;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float IdleUpdateRate = 0.15f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float IdleRandomOffset = 0.016f;

	// MatrixRow.UpdateUnitPositionBarabarianLod close-LOD expansion:
	// [0] = shifted base, [1]/[2] = diagonal clones (UE cm). Empty when
	// the row must not expand (hero row).
	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	TArray<FVector> CloseLodOffsetsCm;
};

USTRUCT(BlueprintType)
struct FRokWorldMapEntityRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	int32 SourceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FString SourceTable;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FString Kind;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FVector2D UnityPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FString DetailTexturePath;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	FString FarTexturePath;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float DetailWorldHeightCm = 2500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float FarWorldHeightCm = 2000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float FarEndScaleMultiplier = 1.18f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float CloseDetailMaxDxf = 1500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float TacticalDetailMaxDxf = 3000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	float FarIconMaxDxf = 5400.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	bool bSelectable = true;

	// Unity BarbarianFormation per-unit placements; empty when the entity
	// keeps its composite card visual.
	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	TArray<FRokWorldMapFormationUnit> FormationUnits;

	// LevelDetailScalerManager m_unit_scale_curve, X = dxf, Y = scale.
	// Shared by every entity; stored per record so actors can evaluate it
	// without a subsystem lookup.
	UPROPERTY(BlueprintReadOnly, Category = "Rok|World Map|Entity")
	TArray<FVector2D> UnitScaleCurve;

	// Runtime curve retains Unity's tangents and constant LOD boundary.
	FRichCurve UnitScaleCurveWithTangents;
};

UCLASS()
class ROK_API URokWorldMapEntitySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Rok|World Map|Entity")
	bool LoadPopulation();

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	bool IsPopulationLoaded() const { return bPopulationLoaded; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	int32 GetEntityCount() const { return Entities.Num(); }

	// Cropped ui_map.png atlas sprites for the Unity world-object
	// nameplates (UI_Tip_WorldObjectRssLevel / UI_Tip_WorldObjectPlayerLevel).
	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	const FString& GetLevelBadgeTexturePath() const
	{
		return LevelBadgeTexturePath;
	}

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	const FString& GetCityNameBgTexturePath() const
	{
		return CityNameBgTexturePath;
	}

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	const FString& GetCityStateIconTexturePath() const
	{
		return CityStateIconTexturePath;
	}

	const TArray<FRokWorldMapEntityRecord>& GetEntities() const
	{
		return Entities;
	}

private:
	UPROPERTY(Transient)
	TArray<FRokWorldMapEntityRecord> Entities;

	FString LevelBadgeTexturePath;
	FString CityNameBgTexturePath;
	FString CityStateIconTexturePath;

	bool bPopulationLoaded = false;

	void LoadPresentation();
};
