#include "RokBuildingLevelDetails.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool RokBuildingLevelDetails::Load(int32 Type, TArray<FString>& Headers, TArray<FRokBuildingLevelDetailsRow>& Rows, FString& Error, FString* Name, FString* Description)
{
	Headers.Reset(); Rows.Reset(); Error.Reset();
	if (Name) Name->Reset();
	if (Description) Description->Reset();
	FString Json; TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Tables = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/building_level_details.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("tables"), Tables))
	{ Error = TEXT("Building level details missing/invalid: run BuildRokBuildingLevelDetails.py"); return false; }
	for (const auto& Value : *Tables)
	{
		const auto Table = Value->AsObject(); int32 TableType = 0;
		if (!Table.IsValid() || !Table->TryGetNumberField(TEXT("type"), TableType) || TableType != Type) continue;
		const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!Table->TryGetArrayField(TEXT("headers"), Columns) || Columns->Num() != 4
			|| !Table->TryGetArrayField(TEXT("rows"), Entries) || Entries->Num() != 25) break;
		for (const auto& Column : *Columns) Headers.Add(Column->AsString());
		for (const auto& Entry : *Entries)
		{
			const auto Obj = Entry->AsObject(); FRokBuildingLevelDetailsRow Row;
			if (!Obj.IsValid() || !Obj->TryGetNumberField(TEXT("level"), Row.Level)
				|| !Obj->TryGetNumberField(TEXT("produce_speed"), Row.ProduceSpeed)
				|| !Obj->TryGetNumberField(TEXT("gather_max"), Row.GatherMax)
				|| !Obj->TryGetNumberField(TEXT("power"), Row.Power) || Row.Level != Rows.Num() + 1) break;
			Rows.Add(Row);
		}
		FString TableName, TableDescription;
		if (Rows.Num() == 25 && Table->TryGetStringField(TEXT("name"), TableName) && !TableName.IsEmpty()
			&& Table->TryGetStringField(TEXT("description"), TableDescription) && !TableDescription.IsEmpty())
		{
			if (Name) *Name = TableName;
			if (Description) *Description = TableDescription;
			return true;
		}
		break;
	}
	Headers.Reset(); Rows.Reset(); Error = FString::Printf(TEXT("Building level details invalid/unsupported type %d"), Type); return false;
}

bool RokBuildingLevelDetails::LoadBarracks(TArray<FString>& Headers, TArray<FRokBarracksLevelDetailsRow>& Rows, FString& Error, FString* Name, FString* Description)
{
	Headers.Reset(); Rows.Reset(); Error.Reset();
	if (Name) Name->Reset();
	if (Description) Description->Reset();
	FString Json; TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Tables = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/building_level_details.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("tables"), Tables))
	{ Error = TEXT("Building level details missing/invalid: run BuildRokBuildingLevelDetails.py"); return false; }
	for (const auto& Value : *Tables)
	{
		const auto Table = Value->AsObject(); int32 TableType = 0;
		if (!Table.IsValid() || !Table->TryGetNumberField(TEXT("type"), TableType) || TableType != 9) continue;
		const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		FString TableName, TableDescription;
		if (!Table->TryGetArrayField(TEXT("headers"), Columns) || Columns->Num() != 4
			|| !Table->TryGetArrayField(TEXT("rows"), Entries) || Entries->Num() != 25
			|| !Table->TryGetStringField(TEXT("name"), TableName) || TableName.IsEmpty()
			|| !Table->TryGetStringField(TEXT("description"), TableDescription) || TableDescription.IsEmpty()) break;
		for (const auto& Column : *Columns)
		{
			FString Header;
			if (!Column.IsValid() || !Column->TryGetString(Header) || Header.IsEmpty()) break;
			Headers.Add(Header);
		}
		if (Headers.Num() != 4) break;
		for (const auto& Entry : *Entries)
		{
			const auto Obj = Entry->AsObject(); FRokBarracksLevelDetailsRow Row;
			if (!Obj.IsValid() || !Obj->TryGetNumberField(TEXT("level"), Row.Level)
				|| !Obj->TryGetNumberField(TEXT("infantry_train_number"), Row.InfantryTrainNumber)
				|| !Obj->TryGetNumberField(TEXT("infantry_attack_multi"), Row.InfantryAttackMulti)
				|| !Obj->TryGetStringField(TEXT("infantry_attack_text"), Row.InfantryAttackText)
				|| !Obj->TryGetNumberField(TEXT("power"), Row.Power)
				|| Row.Level != Rows.Num() + 1 || Row.InfantryTrainNumber < 0 || Row.InfantryAttackMulti < 0 || Row.Power < 0
				|| (Row.InfantryAttackMulti == 0) != Row.InfantryAttackText.IsEmpty()) break;
			Rows.Add(Row);
		}
		if (Rows.Num() == 25)
		{
			if (Name) *Name = TableName;
			if (Description) *Description = TableDescription;
			return true;
		}
		break;
	}
	Headers.Reset(); Rows.Reset(); Error = TEXT("Building level details invalid/unsupported barracks type 9"); return false;
}
