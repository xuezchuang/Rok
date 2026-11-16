#include "RokWorldMapEntitySubsystem.h"

#include "RokDataPaths.h"
#include "RokSproto.h"

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
const TCHAR* ServerBarbarianPresentationRelativePath =
	TEXT("server_barbarian_presentation.json");

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

TArray<TSharedPtr<FJsonValue>> ConfigRows(const TCHAR* Name)
{
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const FString Path = RokDataPaths::Resolve(
		FString::Printf(TEXT("ConfigTables/%s.json"), Name));
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		|| !Root.IsValid() || !Root->TryGetArrayField(TEXT("rows"), Rows)
		|| !Rows)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok server entity config unavailable: %s"), *Path);
		return {};
	}
	return *Rows;
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
	ResourceSources.Reset();
	MonsterSources.Reset();
	FixedPointSources.Reset();
	AllianceSources.Reset();
	StrongHoldDataSources.Reset();
	StrongHoldTypeSources.Reset();
	ServerVisualTemplates.Reset();

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
		LoadServerProjection(RootObject);
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

void URokWorldMapEntitySubsystem::LoadServerProjection(
	const TSharedPtr<FJsonObject>& PopulationRoot)
{
	ResourceSources.Reset();
	MonsterSources.Reset();
	FixedPointSources.Reset();
	AllianceSources.Reset();
	StrongHoldDataSources.Reset();
	StrongHoldTypeSources.Reset();
	ServerVisualTemplates.Reset();

	// The static population is a visual sample. Its SourceId is a point-row
	// ID, not the live object's resourceId / monsterId.
	const TSharedPtr<FJsonObject> Library =
		JsonObjectField(PopulationRoot, TEXT("visual_library"));
	for (const FRokWorldMapEntityRecord& Entity : Entities)
	{
		const FString Key = Entity.Kind == TEXT("barbarian")
			? FString::Printf(TEXT("barbarian:%d"), Entity.Level)
			: Entity.Kind;
		if (ServerVisualTemplates.Contains(Key))
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Visual =
			JsonObjectField(Library, *Entity.Kind);
		const FString DetailPath = ResolveProjectPath(JsonStringField(
			JsonObjectField(JsonObjectField(Visual, TEXT("detail")), TEXT("output")),
			TEXT("path")));
		const FString FarPath = ResolveProjectPath(JsonStringField(
			JsonObjectField(JsonObjectField(Visual, TEXT("far")), TEXT("output")),
			TEXT("path")));
		if (!Visual.IsValid() || !FPaths::FileExists(DetailPath)
			|| !FPaths::FileExists(FarPath))
		{
			continue;
		}
		FRokWorldMapEntityRecord Template = Entity;
		Template.DetailTexturePath = DetailPath;
		Template.FarTexturePath = FarPath;
		ServerVisualTemplates.Add(Key, MoveTemp(Template));
	}

	auto LoadTable = [this](const TCHAR* Filename,
		TMap<int32, FServerSourceRow>& Destination, bool bFixedPoint)
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		const FString Path = RokDataPaths::Resolve(
			FString::Printf(TEXT("ConfigTables/%s.json"), Filename));
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!FFileHelper::LoadFileToString(Json, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
			|| !Root.IsValid() || !Root->TryGetArrayField(TEXT("rows"), Rows)
			|| !Rows)
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok server entity source table unavailable: %s"), *Path);
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Rows)
		{
			const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
			int64 Id = 0, Type = 0, Level = 0, X = 0, Y = 0;
			if (!RokSprotoJson::TryGetInt64(Row, TEXT("ID"), Id)
				|| Id <= 0 || Id > MAX_int32
				|| !RokSprotoJson::TryGetInt64(Row, TEXT("type"), Type)
				|| Type <= 0 || Type > MAX_int32)
			{
				continue;
			}
			FServerSourceRow Source;
			Source.Type = static_cast<int32>(Type);
			if (bFixedPoint)
			{
				if (!RokSprotoJson::TryGetInt64(Row, TEXT("posX"), X)
					|| !RokSprotoJson::TryGetInt64(Row, TEXT("posY"), Y))
				{
					continue;
				}
				Source.Position = FVector2D(static_cast<double>(X), static_cast<double>(Y));
			}
			else
			{
				if (!RokSprotoJson::TryGetInt64(Row, TEXT("level"), Level)
					|| Level < 0 || Level > MAX_int32)
				{
					continue;
				}
				Source.Level = static_cast<int32>(Level);
				Source.ModelId = JsonStringField(Row, TEXT("modelId"));
				Source.ModelIdLod = JsonStringField(Row, TEXT("modelIdLod"));
				Source.ModelIdLod2 = JsonStringField(Row, TEXT("modelIdLod2"));
				Source.MapIconRes = JsonStringField(Row, TEXT("mapIconRes"));
				Source.MapIcon = JsonStringField(Row, TEXT("mapIcon"));
				Source.Name = JsonStringField(Row, TEXT("name"));
				RokSprotoJson::TryGetInt64(Row, TEXT("resAmount"), Source.ResourceCapacity);
				Row->TryGetNumberField(TEXT("radius"), Source.Radius);
			}
			Destination.Add(static_cast<int32>(Id), MoveTemp(Source));
		}
	};
	LoadTable(TEXT("ResourceGatherType"), ResourceSources, false);
	LoadTable(TEXT("Monster"), MonsterSources, false);
	LoadTable(TEXT("MapFixPoint"), FixedPointSources, true);
	for (const TSharedPtr<FJsonValue>& Value : ConfigRows(TEXT("AllianceBuildingType")))
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		int64 Type = 0;
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("type"), Type)
			|| Type <= 0 || Type > MAX_int32) continue;
		FServerSourceRow Source;
		Source.Type = static_cast<int32>(Type);
		Source.ModelId = JsonStringField(Row, TEXT("modelId"));
		Source.MapIcon = JsonStringField(Row, TEXT("iconImg"));
		AllianceSources.Add(Source.Type, MoveTemp(Source));
	}
	for (const TSharedPtr<FJsonValue>& Value : ConfigRows(TEXT("StrongHoldData")))
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		int64 Id = 0, Type = 0;
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("ID"), Id)
			|| !RokSprotoJson::TryGetInt64(Row, TEXT("type"), Type)
			|| Id <= 0 || Id > MAX_int32 || Type <= 0 || Type > MAX_int32) continue;
		FServerSourceRow Source;
		Source.Type = static_cast<int32>(Type);
		Source.Name = JsonStringField(Row, TEXT("name"));
		double SourceYaw = 0;
		if (Row->TryGetNumberField(TEXT("posTo"), SourceYaw) && FMath::IsFinite(SourceYaw))
			Source.YawDegrees = static_cast<float>(SourceYaw);
		StrongHoldDataSources.Add(static_cast<int32>(Id), MoveTemp(Source));
	}
	for (const TSharedPtr<FJsonValue>& Value : ConfigRows(TEXT("StrongHoldType")))
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		int64 Id = 0, Group = 0, Level = 0;
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("ID"), Id)
			|| !RokSprotoJson::TryGetInt64(Row, TEXT("group"), Group)
			|| !RokSprotoJson::TryGetInt64(Row, TEXT("level"), Level)
			|| Id <= 0 || Id > MAX_int32 || Group <= 0 || Group > MAX_int32
			|| Level <= 0 || Level > MAX_int32) continue;
		FServerSourceRow Source;
		Source.Group = static_cast<int32>(Group);
		Source.Level = static_cast<int32>(Level);
		Source.Name = JsonStringField(Row, TEXT("name"));
		Source.Name2 = JsonStringField(Row, TEXT("name2"));
		Source.ModelId = JsonStringField(Row, TEXT("modelId"));
		Source.MapIcon = JsonStringField(Row, TEXT("iconImg"));
		StrongHoldTypeSources.Add(static_cast<int32>(Id), MoveTemp(Source));
	}
	LoadServerIconVisuals();
}

void URokWorldMapEntitySubsystem::LoadServerIconVisuals()
{
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const FString Path = RokDataPaths::Resolve(TEXT("server_world_visuals.json"));
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		|| !Root.IsValid()
		|| JsonStringField(Root, TEXT("schema")) != TEXT("rok.server_world_visuals.v1")
		|| !Root->TryGetArrayField(TEXT("entries"), Entries) || !Entries)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok server icon-only visuals unavailable: %s"), *Path);
		return;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Entries)
	{
		const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
		int64 Type = 0, Id = 0, Group = 0;
		if (!RokSprotoJson::TryGetInt64(Entry, TEXT("object_type"), Type)
			|| !RokSprotoJson::TryGetInt64(Entry, TEXT("config_id"), Id)
			|| !RokSprotoJson::TryGetInt64(Entry, TEXT("source_group"), Group)
			|| Id < 0 || Id > MAX_int32 || Group < 0 || Group > MAX_int32
			|| (Id == 0 && Group == 0)) continue;
		const FServerSourceRow* Source = nullptr;
		if (Type == 7) Source = ResourceSources.Find(static_cast<int32>(Id));
		else if (Type >= 16 && Type <= 19) Source = AllianceSources.Find(static_cast<int32>(Id));
		else if (Type == 25 || Type == 26)
		{
			for (const TPair<int32, FServerSourceRow>& Pair : StrongHoldTypeSources)
			{
				if (Pair.Value.Group == Group) { Source = &Pair.Value; break; }
			}
		}
		else Source = MonsterSources.Find(static_cast<int32>(Id));
		const FString Icon = Type == 7 && Source ? Source->MapIconRes
			: Source ? Source->MapIcon : FString();
		const FString Kind = JsonStringField(Entry, TEXT("kind"));
		const FString BaselineKey = Type == 7 ? TEXT("stone") : TEXT("barbarian:14");
		const FRokWorldMapEntityRecord* Baseline = ServerVisualTemplates.Find(BaselineKey);
		const FString IconPath = ResolveProjectPath(JsonStringField(Entry, TEXT("path")));
		const double DetailHeight = JsonNumberField(Entry, TEXT("detail_world_height_cm"));
		const double FarHeight = JsonNumberField(Entry, TEXT("far_world_height_cm"));
		if (!Source || !Baseline || !FPaths::FileExists(IconPath)
			|| Source->ModelId != JsonStringField(Entry, TEXT("model_id"))
			|| Icon != JsonStringField(Entry, TEXT("map_icon"))
			|| JsonStringField(Entry, TEXT("presentation"))
				!= TEXT("source_map_icon_at_all_lods_no_close_model")
			|| !(DetailHeight > 0.0 && FarHeight > 0.0)
			|| (Type == 2 && (Source->Type != 1 || Kind != TEXT("barbarian")))
			|| (Type == 28 && (Source->Type != 2 || Kind != TEXT("barbarian_citadel")))
			|| (Type == 7 && (Source->Type != 4 || Kind != TEXT("gold")))
			|| (Type >= 16 && Type <= 19
				&& (Source->Type != Type - 12
					|| Kind != (Type == 16 ? TEXT("guild_food")
						: Type == 17 ? TEXT("guild_wood")
						: Type == 18 ? TEXT("guild_stone") : TEXT("guild_gold"))))
			|| (Type == 25 && (Group != 10 || Kind != TEXT("checkpoint")))
			|| (Type == 26 && ((Group != 1 && Group != 2) || Kind != TEXT("holy_land")))
			|| (Type != 2 && Type != 7 && (Type < 16 || Type > 19)
				&& Type != 25 && Type != 26 && Type != 28))
		{
			continue;
		}
		FRokWorldMapEntityRecord Visual = *Baseline;
		Visual.Kind = Kind;
		Visual.DetailTexturePath = IconPath;
		Visual.FarTexturePath = IconPath;
		Visual.DetailWorldHeightCm = static_cast<float>(DetailHeight);
		Visual.FarWorldHeightCm = static_cast<float>(FarHeight);
		Visual.FormationUnits.Reset();
		Visual.UnitScaleCurve.Reset();
		Visual.UnitScaleCurveWithTangents.Reset();
		Visual.bIconOnlyVisual = true;
		ServerVisualTemplates.Add(FString::Printf(TEXT("server:%lld:%lld"), Type,
			Group > 0 ? Group : Id),
			MoveTemp(Visual));
	}
}

double URokWorldMapEntitySubsystem::GetResourceRadius(int32 SourceId) const
{
	const auto* Source = ResourceSources.Find(SourceId);
	return Source ? Source->Radius : 0;
}

bool URokWorldMapEntitySubsystem::ResolveServerEntity(
	const TSharedPtr<FJsonObject>& Row, FRokWorldMapEntityRecord& Out) const
{
	Out = FRokWorldMapEntityRecord();
	int64 ObjectId = 0, ObjectType = 0, SourceId = 0;
	const TSharedPtr<FJsonObject> Pos = JsonObjectField(Row, TEXT("objectPos"));
	int64 X = 0, Y = 0;
	if (!RokSprotoJson::TryGetInt64(Row, TEXT("objectId"), ObjectId)
		|| ObjectId <= 0
		|| !RokSprotoJson::TryGetInt64(Row, TEXT("objectType"), ObjectType)
		|| ObjectType <= 0 || ObjectType > MAX_int32
		|| !RokSprotoJson::TryGetInt64(Pos, TEXT("x"), X)
		|| !RokSprotoJson::TryGetInt64(Pos, TEXT("y"), Y))
	{
		return false;
	}

	FString Kind, SourceTable, VisualKey;
	const FServerSourceRow* Source = nullptr;
	if (ObjectType >= 4 && ObjectType <= 7)
	{
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("resourceId"), SourceId)
			|| SourceId <= 0 || SourceId > MAX_int32) return false;
		Source = ResourceSources.Find(static_cast<int32>(SourceId));
		SourceTable = TEXT("ResourceGatherType");
		const int32 ConfigType = ObjectType == 5 ? 1 : ObjectType == 6 ? 2
			: ObjectType == 4 ? 3 : 4;
		const FServerSourceRow* VisualBaseline = ResourceSources.Find(ConfigType * 10000 + 3);
		if (!Source || !VisualBaseline || Source->Type != ConfigType
			|| Source->Level < 1
			|| Source->ModelId != VisualBaseline->ModelId
			|| Source->ModelIdLod != VisualBaseline->ModelIdLod
			|| Source->ModelIdLod2 != VisualBaseline->ModelIdLod2
			|| Source->MapIconRes != VisualBaseline->MapIconRes) return false;
		Kind = ConfigType == 1 ? TEXT("food") : ConfigType == 2 ? TEXT("wood")
			: ConfigType == 3 ? TEXT("stone") : TEXT("gold");
		const FString ExpectedModel = ConfigType == 1 ? TEXT("build_Rssfood_Big")
			: ConfigType == 2 ? TEXT("build_Rsswood_Big")
			: ConfigType == 3 ? TEXT("build_Rssstone_Big") : TEXT("build_Rssgold_Big");
		VisualKey = ObjectType == 7
			? FString::Printf(TEXT("server:%lld:%lld"), ObjectType, SourceId)
			: Kind;
		if (Source->ModelId != ExpectedModel) return false;
	}
	else if (ObjectType == 2)
	{
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("monsterId"), SourceId)
			|| SourceId <= 0 || SourceId > MAX_int32) return false;
		Source = MonsterSources.Find(static_cast<int32>(SourceId));
		if (!Source || Source->Type != 1
			|| !Source->ModelId.StartsWith(TEXT("BarbarianFormation_")))
		{
			return false;
		}
		Kind = TEXT("barbarian");
		SourceTable = TEXT("Monster");
		const FString IconKey = FString::Printf(TEXT("server:%lld:%lld"), ObjectType, SourceId);
		VisualKey = ServerBarbarianFormations.Contains(Source->ModelId)
			? Source->ModelId
			: ServerVisualTemplates.Contains(IconKey) ? IconKey
			: FString::Printf(TEXT("barbarian:%d"), Source->Level);
	}
	else if (ObjectType == 28)
	{
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("monsterId"), SourceId)
			|| SourceId <= 0 || SourceId > MAX_int32) return false;
		Source = MonsterSources.Find(static_cast<int32>(SourceId));
		if (!Source || Source->Type != 2
			|| Source->ModelId != TEXT("build_BarbarianCitadel_Big")) return false;
		Kind = TEXT("barbarian_citadel");
		SourceTable = TEXT("Monster");
		VisualKey = FString::Printf(TEXT("server:%lld:%lld"), ObjectType, SourceId);
	}
	else if (ObjectType >= 16 && ObjectType <= 19)
	{
		// AllianceProxy.typeMap[16..19] = AllianceBuildingType 4..7.
		SourceId = ObjectType - 12;
		Source = AllianceSources.Find(static_cast<int32>(SourceId));
		const FString ExpectedModel = FString::Printf(TEXT("build_alliance_%lld"), SourceId + 999);
		const FString ExpectedIcon = FString::Printf(TEXT("map_icon[map_icon_aliCur%lld]"), SourceId - 3);
		if (!Source || Source->ModelId != ExpectedModel || Source->MapIcon != ExpectedIcon) return false;
		Kind = ObjectType == 16 ? TEXT("guild_food") : ObjectType == 17 ? TEXT("guild_wood")
			: ObjectType == 18 ? TEXT("guild_stone") : TEXT("guild_gold");
		SourceTable = TEXT("AllianceBuildingType");
		VisualKey = FString::Printf(TEXT("server:%lld:%lld"), ObjectType, SourceId);
	}
	else if (ObjectType == 25 || ObjectType == 26)
	{
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("strongHoldId"), SourceId)
			|| SourceId <= 0 || SourceId > MAX_int32) return false;
		const FServerSourceRow* StrongHold = StrongHoldDataSources.Find(static_cast<int32>(SourceId));
		if (!StrongHold) return false;
		Source = StrongHoldTypeSources.Find(StrongHold->Type);
		if (!Source) return false;
		const bool bCheckpoint = ObjectType == 25 && Source->Group == 10
			&& Source->ModelId == TEXT("build_Checkpoint_Big")
			&& Source->MapIcon == TEXT("map_icon[map_icon_build1]");
		const bool bHolyLand1 = ObjectType == 26 && Source->Group == 1
			&& Source->ModelId == TEXT("build_HolyLand1_Big")
			&& Source->MapIcon == TEXT("map_icon[map_icon_build2]");
		const bool bHolyLand2 = ObjectType == 26 && Source->Group == 2
			&& Source->ModelId == TEXT("build_HolyLand2_Big")
			&& Source->MapIcon == TEXT("map_icon[map_icon_build3]");
		if (!bCheckpoint && !bHolyLand1 && !bHolyLand2) return false;
		Kind = bCheckpoint ? TEXT("checkpoint") : TEXT("holy_land");
		SourceTable = TEXT("StrongHoldData");
		VisualKey = FString::Printf(TEXT("server:%lld:%d"), ObjectType, Source->Group);
	}
	else if (ObjectType == 10)
	{
		if (!RokSprotoJson::TryGetInt64(Row, TEXT("resourcePointId"), SourceId)
			|| SourceId <= 0 || SourceId > MAX_int32) return false;
		const FServerSourceRow* Fixed = FixedPointSources.Find(static_cast<int32>(SourceId));
		if (!Fixed || Fixed->Type != 70001) return false;
		Source = ResourceSources.Find(Fixed->Type);
		if (!Source || Source->ModelId != TEXT("build_RssVillage_Big")) return false;
		Kind = TEXT("village");
		SourceTable = TEXT("MapFixPoint");
		VisualKey = Kind;
	}
	else
	{
		return false;
	}
	const FRokWorldMapEntityRecord* Template = ObjectType == 2
		? ServerBarbarianFormations.Find(VisualKey) : nullptr;
	if (!Template) Template = ServerVisualTemplates.Find(VisualKey);
	if (!Template || (ObjectType == 2 && Template->FormationUnits.IsEmpty()
		&& !Template->bIconOnlyVisual)) return false;
	int64 Amount = -1;
	if (ObjectType >= 4 && ObjectType <= 7 && Row->HasField(TEXT("resourceAmount"))
		&& (!RokSprotoJson::TryGetInt64(Row, TEXT("resourceAmount"), Amount)
			|| Amount < 0)) return false;
	Out = *Template;
	Out.ServerObjectId = ObjectId;
	Out.ServerObjectType = static_cast<int32>(ObjectType);
	Out.SourceId = static_cast<int32>(SourceId);
	Out.SourceTable = SourceTable;
	Out.SourceModelId = Source->ModelId;
	if (ObjectType == 25)
	{
		if (const FServerSourceRow* StrongHold = StrongHoldDataSources.Find(static_cast<int32>(SourceId)))
			Out.SourceYawDegrees = StrongHold->YawDegrees;
	}
	Out.Kind = Kind;
	if (ObjectType >= 16 && ObjectType <= 19)
	{
		Out.DisplayName = ObjectType == 16 ? TEXT("联盟农田") : ObjectType == 17 ? TEXT("联盟伐木场")
			: ObjectType == 18 ? TEXT("联盟石矿床") : TEXT("联盟金矿床");
	}
	else if (ObjectType == 25 || ObjectType == 26)
	{
		const FServerSourceRow* StrongHold = StrongHoldDataSources.Find(static_cast<int32>(SourceId));
		Out.DisplayName = StrongHold && !StrongHold->Name.IsEmpty()
			? StrongHold->Name : Source->Name;
	}
	else Out.DisplayName = Source->Name.IsEmpty() ? Template->DisplayName : Source->Name;
	Out.Level = ObjectType == 10 || (ObjectType >= 16 && ObjectType <= 19) ? 0 : Source->Level;
	Out.UnityPosition = FVector2D(static_cast<double>(X) / 100.0,
		static_cast<double>(Y) / 100.0);
	if (ObjectType == 10)
	{
		// WorldMgrMediator.OnLoadMapObj anchors village/cave at MapFixPoint.
		Out.UnityPosition = FixedPointSources.FindChecked(static_cast<int32>(SourceId)).Position;
	}
	if (ObjectType >= 4 && ObjectType <= 7)
	{
		Out.ResourceAmount = Amount;
		Out.ResourceCapacity = Source->ResourceCapacity;
		RokSprotoJson::TryGetInt64(Row, TEXT("collectRid"), Out.CollectorRoleId);
		// MapObjectMgr.resourceUpdate writes the collector's name to MapObjectInfo.cityName.
		FString Name = JsonStringField(Row, TEXT("cityName"));
		if (Name.IsEmpty()) Name = JsonStringField(Row, TEXT("name"));
		const FString CollectorAlliance = JsonStringField(Row, TEXT("guildAbbName"));
		Out.CollectorName = Out.CollectorRoleId > 0 && !CollectorAlliance.IsEmpty() && !Name.IsEmpty()
			? FString::Printf(TEXT("[%s]%s"), *CollectorAlliance, *Name) : Name;
		Out.ResourceAlliance = JsonStringField(Row, TEXT("resourceGuildAbbName"));
	}
	return true;
}

void URokWorldMapEntitySubsystem::LoadPresentation()
{
	ServerBarbarianFormations.Reset();
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
	// The server supplement carries actual Monster models beyond the static
	// world sample. Its unit rows use the same schema and parser below.
	TSharedPtr<FJsonObject> Supplement;
	FString SupplementText;
	const FString SupplementPath = RokDataPaths::Resolve(ServerBarbarianPresentationRelativePath);
	if (FFileHelper::LoadFileToString(SupplementText, *SupplementPath)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SupplementText), Supplement)
		&& Supplement.IsValid()
		&& JsonStringField(Supplement, TEXT("schema")) == TEXT("rok.server_barbarian_presentation.v1"))
	{
		const TSharedPtr<FJsonObject> BaseUnits = JsonObjectField(RootObject, TEXT("unit_library"));
		const TSharedPtr<FJsonObject> ExtraUnits = JsonObjectField(Supplement, TEXT("unit_library"));
		const TSharedPtr<FJsonObject> BaseFormations = JsonObjectField(RootObject, TEXT("formations"));
		const TSharedPtr<FJsonObject> ExtraFormations = JsonObjectField(Supplement, TEXT("formations"));
		if (BaseUnits.IsValid() && ExtraUnits.IsValid() && BaseFormations.IsValid()
			&& ExtraFormations.IsValid())
		{
			for (const auto& Pair : ExtraUnits->Values)
			{
				if (!BaseUnits->HasField(Pair.Key)) BaseUnits->SetField(Pair.Key, Pair.Value);
			}
			for (const auto& Pair : ExtraFormations->Values) BaseFormations->SetField(Pair.Key, Pair.Value);
		}
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
			if (Visual.FadeFrameTexturePaths.Num() != 8)
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
			bool bAllFramesExist = true;
			for (const FString& FramePath : Visual.FrameTexturePaths)
			{
				bAllFramesExist &= FPaths::FileExists(FramePath);
			}
			for (const FString& FramePath : Visual.FadeFrameTexturePaths)
			{
				bAllFramesExist &= FPaths::FileExists(FramePath);
			}
			if (bAllFramesExist)
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
			TArray<FRokWorldMapEntityRecord*> Targets;
			if (Pair.Key.StartsWith(TEXT("BarbarianFormation_")))
			{
				const FRokWorldMapEntityRecord* Sample = Entities.FindByPredicate(
					[](const FRokWorldMapEntityRecord& Entity) { return Entity.Kind == TEXT("barbarian"); });
				if (Sample)
				{
					ServerBarbarianFormations.Add(Pair.Key, *Sample);
					Targets.Add(ServerBarbarianFormations.Find(Pair.Key));
				}
			}
			else for (FRokWorldMapEntityRecord& Entity : Entities)
			{
				if (Entity.Kind == TEXT("barbarian")
					&& FString::FromInt(Entity.Level) == Pair.Key) Targets.Add(&Entity);
			}
			for (FRokWorldMapEntityRecord* Target : Targets)
			{
				FRokWorldMapEntityRecord& Record = *Target;
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
				if (Pair.Key.StartsWith(TEXT("BarbarianFormation_"))
					&& Record.FormationUnits.Num() != static_cast<int32>(JsonNumberField(FormationObject, TEXT("unit_count"))))
				{
					ServerBarbarianFormations.Remove(Pair.Key);
				}
				else FormationUnitsBuilt += Record.FormationUnits.Num();
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
