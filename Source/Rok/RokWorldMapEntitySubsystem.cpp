#include "RokWorldMapEntitySubsystem.h"

#include "RokDataPaths.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* PopulationRelativePath =
	TEXT("unity_world_entity_population.json");
const TCHAR* PopulationSchema =
	TEXT("rok.unity_world_entity_population.v1");
const TCHAR* PresentationRelativePath =
	TEXT("unity_world_entity_presentation.json");

TSharedPtr<FJsonObject> JsonObjectField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return nullptr;
	}
	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	return Value && Value->IsValid() ? (*Value)->AsObject() : nullptr;
}

FString JsonStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	return Object.IsValid()
			&& Object->TryGetStringField(FieldName, Value)
		? Value
		: FString();
}

double JsonNumberField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	double Fallback = 0.0)
{
	double Value = Fallback;
	return Object.IsValid()
			&& Object->TryGetNumberField(FieldName, Value)
		? Value
		: Fallback;
}

bool JsonBoolField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool Fallback)
{
	bool Value = Fallback;
	return Object.IsValid()
			&& Object->TryGetBoolField(FieldName, Value)
		? Value
		: Fallback;
}

FString ResolveProjectPath(const FString& RelativePath)
{
	return FPaths::ConvertRelativePathToFull(
		RokDataPaths::ResolveProjectPath(RelativePath));
}
}

void URokWorldMapEntitySubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPopulation();
}

bool URokWorldMapEntitySubsystem::LoadPopulation()
{
	Entities.Reset();
	bPopulationLoaded = false;

	const FString PopulationPath = FPaths::ConvertRelativePathToFull(RokDataPaths::Resolve(PopulationRelativePath));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *PopulationPath))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world entity population not found: %s"),
			*PopulationPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject)
		|| !RootObject.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok world entity population is invalid JSON: %s"),
			*PopulationPath);
		return false;
	}

	const FString Schema = JsonStringField(RootObject, TEXT("schema"));
	if (Schema != PopulationSchema)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok world entity population schema mismatch: expected=%s actual=%s."),
			PopulationSchema,
			*Schema);
		return false;
	}

	const TSharedPtr<FJsonObject> LodObject =
		JsonObjectField(RootObject, TEXT("lod_contract"));
	const float CloseDetailMaxDxf = static_cast<float>(
		JsonNumberField(
			LodObject,
			TEXT("close_detail_max_dxf"),
			1500.0));
	const float TacticalDetailMaxDxf = static_cast<float>(
		JsonNumberField(
			LodObject,
			TEXT("tactical_detail_max_dxf"),
			3000.0));
	const float FarIconMaxDxf = static_cast<float>(
		JsonNumberField(
			LodObject,
			TEXT("far_icon_max_dxf"),
			5400.0));
	if (!(CloseDetailMaxDxf > 0.0f
		&& TacticalDetailMaxDxf > CloseDetailMaxDxf
		&& FarIconMaxDxf > TacticalDetailMaxDxf))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok world entity population has invalid dxf thresholds."));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EntityValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("entities"), EntityValues)
		|| !EntityValues)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok world entity population lacks entities."));
		return false;
	}

	Entities.Reserve(EntityValues->Num());
	for (const TSharedPtr<FJsonValue>& Value : *EntityValues)
	{
		const TSharedPtr<FJsonObject> EntityObject =
			Value.IsValid() ? Value->AsObject() : nullptr;
		const TSharedPtr<FJsonObject> PositionObject =
			JsonObjectField(EntityObject, TEXT("unity_position"));
		const TSharedPtr<FJsonObject> DetailObject =
			JsonObjectField(EntityObject, TEXT("detail_visual"));
		const TSharedPtr<FJsonObject> FarObject =
			JsonObjectField(EntityObject, TEXT("far_visual"));
		if (!EntityObject.IsValid()
			|| !PositionObject.IsValid()
			|| !DetailObject.IsValid()
			|| !FarObject.IsValid())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Rok world entity population has an incomplete entity record."));
			Entities.Reset();
			return false;
		}

		FRokWorldMapEntityRecord Record;
		Record.SourceId = FMath::RoundToInt(
			JsonNumberField(EntityObject, TEXT("source_id")));
		Record.SourceTable = JsonStringField(
			EntityObject,
			TEXT("source_table"));
		Record.Kind = JsonStringField(EntityObject, TEXT("kind"));
		Record.DisplayName = JsonStringField(
			EntityObject,
			TEXT("display_name"));
		Record.Level = FMath::RoundToInt(
			JsonNumberField(EntityObject, TEXT("level")));
		Record.UnityPosition = FVector2D(
			JsonNumberField(PositionObject, TEXT("x")),
			JsonNumberField(PositionObject, TEXT("y")));
		Record.DetailTexturePath = ResolveProjectPath(
			JsonStringField(DetailObject, TEXT("path")));
		Record.FarTexturePath = ResolveProjectPath(
			JsonStringField(FarObject, TEXT("path")));
		Record.DetailWorldHeightCm = static_cast<float>(
			JsonNumberField(
				DetailObject,
				TEXT("world_height_cm"),
				2500.0));
		Record.FarWorldHeightCm = static_cast<float>(
			JsonNumberField(
				FarObject,
				TEXT("world_height_cm"),
				2000.0));
		Record.FarEndScaleMultiplier = static_cast<float>(JsonNumberField(
			FarObject, TEXT("end_scale_multiplier"), 1.18));
		Record.CloseDetailMaxDxf = CloseDetailMaxDxf;
		Record.TacticalDetailMaxDxf = TacticalDetailMaxDxf;
		Record.FarIconMaxDxf = FarIconMaxDxf;
		Record.bSelectable = JsonBoolField(
			EntityObject,
			TEXT("selectable"),
			true);

		const bool bValid =
			Record.SourceId > 0
			&& !Record.SourceTable.IsEmpty()
			&& !Record.Kind.IsEmpty()
			&& !Record.UnityPosition.ContainsNaN()
			&& Record.DetailWorldHeightCm > 0.0f
			&& Record.FarWorldHeightCm > 0.0f
			&& FPaths::FileExists(Record.DetailTexturePath)
			&& FPaths::FileExists(Record.FarTexturePath);
		if (!bValid)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Rok world entity population rejected source_id=%d kind=%s."),
				Record.SourceId,
				*Record.Kind);
			Entities.Reset();
			return false;
		}
		Entities.Add(MoveTemp(Record));
	}

	bPopulationLoaded = !Entities.IsEmpty();
	if (bPopulationLoaded)
	{
		LoadPresentation();
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Loaded Rok Unity world entity population: entities=%d close_dxf=%.1f tactical_dxf=%.1f far_dxf=%.1f."),
			Entities.Num(),
			CloseDetailMaxDxf,
			TacticalDetailMaxDxf,
			FarIconMaxDxf);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok Unity world entity population contains no entities."));
	}
	return bPopulationLoaded;
}

void URokWorldMapEntitySubsystem::LoadPresentation()
{
	LevelBadgeTexturePath.Reset();
	CityNameBgTexturePath.Reset();
	CityStateIconTexturePath.Reset();
	for (FRokWorldMapEntityRecord& Record : Entities)
	{
		Record.FormationUnits.Reset();
	}

	const FString PresentationPath = FPaths::ConvertRelativePathToFull(RokDataPaths::Resolve(PresentationRelativePath));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *PresentationPath))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world entity presentation not found: %s"),
			*PresentationPath);
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject)
		|| !RootObject.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world entity presentation is invalid JSON: %s"),
			*PresentationPath);
		return;
	}

	struct FUnitVisual
	{
		FString TexturePath;
		float WidthCm = 60.0f;
		float HeightCm = 100.0f;
		float PivotX = 0.5f;
		float PivotY = 0.5f;
		TArray<FString> FrameTexturePaths;
		TArray<FString> FadeFrameTexturePaths;
		float IdleUpdateRate = 0.15f;
		float IdleRandomOffset = 0.016f;
	};
	TMap<FString, FUnitVisual> UnitVisuals;
	const TSharedPtr<FJsonObject> UnitLibrary =
		JsonObjectField(RootObject, TEXT("unit_library"));
	if (UnitLibrary.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair :
			UnitLibrary->Values)
		{
			const TSharedPtr<FJsonObject> UnitObject =
				Pair.Value.IsValid()
					? Pair.Value->AsObject()
					: nullptr;
			if (!UnitObject.IsValid())
			{
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* FrameCropValues =
				nullptr;
			if (!UnitObject->TryGetArrayField(
					TEXT("frame_crops"),
					FrameCropValues)
				|| !FrameCropValues
				|| FrameCropValues->Num() != 8)
			{
				continue;
			}
			FUnitVisual Visual;
			Visual.IdleUpdateRate = static_cast<float>(JsonNumberField(UnitObject, TEXT("idle_update_rate"), 0.15));
			Visual.IdleRandomOffset = static_cast<float>(JsonNumberField(UnitObject, TEXT("idle_random_offset"), 0.016));
			for (const TSharedPtr<FJsonValue>& FrameCropValue :
				*FrameCropValues)
			{
				const TSharedPtr<FJsonObject> FrameCropObject =
					FrameCropValue.IsValid()
						? FrameCropValue->AsObject()
						: nullptr;
				if (FrameCropObject.IsValid())
				{
					Visual.FrameTexturePaths.Add(ResolveProjectPath(
						JsonStringField(FrameCropObject, TEXT("path"))));
					Visual.FadeFrameTexturePaths.Add(ResolveProjectPath(
						JsonStringField(FrameCropObject, TEXT("fade_path"))));
				}
			}
			if (Visual.FrameTexturePaths.Num() != 8)
			{
				continue;
			}
			const TSharedPtr<FJsonObject> FirstCropObject =
				(*FrameCropValues)[0].IsValid()
					? (*FrameCropValues)[0]->AsObject()
					: nullptr;
			Visual.TexturePath = ResolveProjectPath(
				JsonStringField(FirstCropObject, TEXT("path")));
			const TSharedPtr<FJsonObject> SizeObject =
				JsonObjectField(FirstCropObject, TEXT("world_size_cm"));
			const TSharedPtr<FJsonObject> PivotObject =
				JsonObjectField(FirstCropObject, TEXT("pivot"));
			if (SizeObject.IsValid())
			{
				Visual.WidthCm = static_cast<float>(JsonNumberField(
					SizeObject,
					TEXT("width"),
					60.0));
				Visual.HeightCm = static_cast<float>(JsonNumberField(
					SizeObject,
					TEXT("height"),
					100.0));
			}
			if (PivotObject.IsValid())
			{
				Visual.PivotX = static_cast<float>(JsonNumberField(
					PivotObject,
					TEXT("x"),
					0.5));
				Visual.PivotY = static_cast<float>(JsonNumberField(
					PivotObject,
					TEXT("y"),
					0.5));
			}
			if (FPaths::FileExists(Visual.TexturePath))
			{
				UnitVisuals.Add(Pair.Key, MoveTemp(Visual));
			}
		}
	}

	TArray<FVector2D> UnitScaleCurve;
	FRichCurve UnitScaleCurveWithTangents;
	const TSharedPtr<FJsonObject> ScaleCurveObject =
		JsonObjectField(RootObject, TEXT("unit_scale_curve"));
	if (ScaleCurveObject.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* KeyValues = nullptr;
		if (ScaleCurveObject->TryGetArrayField(TEXT("keys"), KeyValues)
			&& KeyValues)
		{
			for (const TSharedPtr<FJsonValue>& KeyValue : *KeyValues)
			{
				const TSharedPtr<FJsonObject> KeyObject =
					KeyValue.IsValid() ? KeyValue->AsObject() : nullptr;
				if (!KeyObject.IsValid())
				{
					continue;
				}
				if (!KeyObject->HasTypedField<EJson::Number>(TEXT("in_slope"))
					|| !KeyObject->HasTypedField<EJson::Number>(TEXT("out_slope"))
					|| !KeyObject->HasTypedField<EJson::String>(TEXT("interpolation")))
				{
					UE_LOG(LogTemp, Error, TEXT("Rok unit scale tangents missing; regenerate BuildRokUnityWorldEntityPresentation.py."));
					return;
				}
				UnitScaleCurve.Add(FVector2D(
					JsonNumberField(KeyObject, TEXT("dxf")),
					JsonNumberField(KeyObject, TEXT("scale"))));
				const FKeyHandle Handle = UnitScaleCurveWithTangents.AddKey(
					UnitScaleCurve.Last().X, UnitScaleCurve.Last().Y);
				FRichCurveKey& Key = UnitScaleCurveWithTangents.GetKey(Handle);
				Key.InterpMode = JsonStringField(KeyObject, TEXT("interpolation")) == TEXT("constant")
					? RCIM_Constant : RCIM_Cubic;
				Key.TangentMode = RCTM_User;
				Key.ArriveTangent = JsonNumberField(KeyObject, TEXT("in_slope"));
				Key.LeaveTangent = JsonNumberField(KeyObject, TEXT("out_slope"));
			}
		}
	}
	if (UnitScaleCurve.Num() < 2)
	{
		UnitScaleCurve.Reset();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world entity presentation lacks a usable unit scale curve."));
	}

	const TSharedPtr<FJsonObject> Formations =
		JsonObjectField(RootObject, TEXT("formations"));
	int32 FormationUnitsBuilt = 0;
	if (Formations.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair :
			Formations->Values)
		{
			const TSharedPtr<FJsonObject> FormationObject =
				Pair.Value.IsValid()
					? Pair.Value->AsObject()
					: nullptr;
			const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
			if (!FormationObject.IsValid()
				|| !FormationObject->TryGetArrayField(
					TEXT("rows"),
					RowValues)
				|| !RowValues)
			{
				continue;
			}
			for (FRokWorldMapEntityRecord& Record : Entities)
			{
				if (Record.Kind != TEXT("barbarian")
					|| FString::FromInt(Record.Level) != Pair.Key)
				{
					continue;
				}
				Record.FormationUnits.Reset();
				for (const TSharedPtr<FJsonValue>& RowValue : *RowValues)
				{
					const TSharedPtr<FJsonObject> RowObject =
						RowValue.IsValid()
							? RowValue->AsObject()
							: nullptr;
					const TArray<TSharedPtr<FJsonValue>>* UnitValues =
						nullptr;
					if (!RowObject.IsValid()
						|| !RowObject->TryGetArrayField(
							TEXT("units"),
							UnitValues)
						|| !UnitValues)
					{
						continue;
					}
					for (const TSharedPtr<FJsonValue>& UnitValue :
						*UnitValues)
					{
						const TSharedPtr<FJsonObject> UnitObject =
							UnitValue.IsValid()
								? UnitValue->AsObject()
								: nullptr;
						const TArray<TSharedPtr<FJsonValue>>*
							OffsetValues = nullptr;
						if (!UnitObject.IsValid()
							|| !UnitObject->TryGetArrayField(
								TEXT("ue_offset_cm"),
								OffsetValues)
							|| !OffsetValues
							|| OffsetValues->Num() != 3)
						{
							continue;
						}
						const FUnitVisual* Visual = UnitVisuals.Find(
							JsonStringField(
								UnitObject,
								TEXT("unit_prefab")));
						if (!Visual)
						{
							continue;
						}
						FRokWorldMapFormationUnit Unit;
						Unit.UnitPrefab = JsonStringField(
							UnitObject,
							TEXT("unit_prefab"));
						Unit.UeOffsetCm = FVector(
							(*OffsetValues)[0]->AsNumber(),
							(*OffsetValues)[1]->AsNumber(),
							(*OffsetValues)[2]->AsNumber());
						Unit.TexturePath = Visual->TexturePath;
						Unit.WidthCm = Visual->WidthCm;
						Unit.HeightCm = Visual->HeightCm;
						Unit.PivotX = Visual->PivotX;
						Unit.PivotY = Visual->PivotY;
						Unit.FrameTexturePaths = Visual->FrameTexturePaths;
						Unit.FadeFrameTexturePaths = Visual->FadeFrameTexturePaths;
						Unit.IdleUpdateRate = Visual->IdleUpdateRate;
						Unit.IdleRandomOffset = Visual->IdleRandomOffset;
						Record.UnitScaleCurve = UnitScaleCurve;
						Record.UnitScaleCurveWithTangents = UnitScaleCurveWithTangents;
						const TArray<TSharedPtr<FJsonValue>>*
							CloseOffsets = nullptr;
						if (UnitObject->TryGetArrayField(
								TEXT("close_lod_offsets_cm"),
								CloseOffsets)
							&& CloseOffsets
							&& CloseOffsets->Num() == 3)
						{
							bool bValidOffsets = true;
							for (const TSharedPtr<FJsonValue>& OffsetValue :
								*CloseOffsets)
							{
								const TArray<TSharedPtr<FJsonValue>>*
									Components =
										OffsetValue.IsValid()
											? &OffsetValue->AsArray()
											: nullptr;
								if (!Components
									|| Components->Num() != 3)
								{
									bValidOffsets = false;
									break;
								}
								Unit.CloseLodOffsetsCm.Add(FVector(
									(*Components)[0]->AsNumber(),
									(*Components)[1]->AsNumber(),
									(*Components)[2]->AsNumber()));
							}
							if (!bValidOffsets)
							{
								Unit.CloseLodOffsetsCm.Reset();
							}
						}
						Record.FormationUnits.Add(MoveTemp(Unit));
					}
				}
				FormationUnitsBuilt += Record.FormationUnits.Num();
			}
		}
	}

	const TSharedPtr<FJsonObject> Badges =
		JsonObjectField(RootObject, TEXT("badge_sprites"));
	if (Badges.IsValid())
	{
		const TSharedPtr<FJsonObject> LevelBadge =
			JsonObjectField(Badges, TEXT("pl_map_levelBg"));
		const TSharedPtr<FJsonObject> NameBadge =
			JsonObjectField(Badges, TEXT("pl_map_buildingNameBg"));
		const TSharedPtr<FJsonObject> StateIcon =
			JsonObjectField(Badges, TEXT("img_map_state_back"));
		const TSharedPtr<FJsonObject> LevelOutput =
			JsonObjectField(LevelBadge, TEXT("output"));
		const TSharedPtr<FJsonObject> NameOutput =
			JsonObjectField(NameBadge, TEXT("output"));
		const TSharedPtr<FJsonObject> StateOutput =
			JsonObjectField(StateIcon, TEXT("output"));
		if (LevelOutput.IsValid())
		{
			LevelBadgeTexturePath = ResolveProjectPath(
				JsonStringField(LevelOutput, TEXT("path")));
		}
		if (NameOutput.IsValid())
		{
			CityNameBgTexturePath = ResolveProjectPath(
				JsonStringField(NameOutput, TEXT("path")));
		}
		if (StateOutput.IsValid())
		{
			CityStateIconTexturePath = ResolveProjectPath(
				JsonStringField(StateOutput, TEXT("path")));
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Loaded Rok Unity world entity presentation: formation_units=%d unit_types=%d level_badge=%s."),
		FormationUnitsBuilt,
		UnitVisuals.Num(),
		LevelBadgeTexturePath.IsEmpty() ? TEXT("missing") : TEXT("ok"));
}
