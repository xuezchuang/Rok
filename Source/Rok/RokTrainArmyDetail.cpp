#include "RokTrainArmyDetail.h"

#include "RokDataPaths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool RokTrainArmyDetail::Load(int32 TroopId, FRokTrainArmyDetail& Detail, FString& Error)
{
	Detail = {}; Error.Reset();
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *(RokDataPaths::Resolve(TEXT("train_army_detail.json"))))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		|| !Root.IsValid() || !Root->TryGetArrayField(TEXT("entries"), Entries))
	{
		Error = TEXT("TrainArmy detail catalog missing or invalid; run BuildRokTrainArmyDetail.py"); return false;
	}
	for (const auto& Value : *Entries)
	{
		const auto Row = Value->AsObject(); int32 Id = 0;
		if (!Row.IsValid() || !Row->TryGetNumberField(TEXT("id"), Id) || Id != TroopId) continue;
		FRokTrainArmyDetail Result; Result.Id = Id;
		const TArray<TSharedPtr<FJsonValue>>* Attributes = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		bool bValid = Row->TryGetStringField(TEXT("name"), Result.Name)
			&& Row->TryGetStringField(TEXT("description"), Result.Description)
			&& Row->TryGetArrayField(TEXT("attributes"), Attributes) && Attributes->Num() == 7
			&& Row->TryGetArrayField(TEXT("skills"), Skills) && Skills->Num() <= 3;
		if (bValid) for (const auto& Item : *Attributes)
		{
			const auto Obj = Item->AsObject(); FRokTrainArmyAttribute Attribute;
			const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("label"), Attribute.Label)
				|| !Obj->TryGetStringField(TEXT("value"), Attribute.Value)
				|| !Obj->TryGetStringField(TEXT("tip"), Attribute.Tip)
				|| !Obj->TryGetNumberField(TEXT("unity_y"), Attribute.UnityY)
				|| !Obj->TryGetBoolField(TEXT("modifier_unknown"), Attribute.bModifierUnknown)
				|| !Obj->TryGetStringField(TEXT("texture"), Attribute.Texture)
				|| !Obj->TryGetArrayField(TEXT("rect"), Rect) || Rect->Num() != 4
				|| !Obj->TryGetArrayField(TEXT("size"), Size) || Size->Num() != 2) { bValid = false; break; }
			const int32 X = (*Rect)[0]->AsNumber(), Y = (*Rect)[1]->AsNumber();
			Attribute.Rect = FIntRect(X, Y, X + int32((*Rect)[2]->AsNumber()), Y + int32((*Rect)[3]->AsNumber()));
			Attribute.AtlasSize = FIntPoint((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber());
			if (Attribute.Rect.Width() <= 0 || Attribute.Rect.Height() <= 0 || X < 0 || Y < 0
				|| Attribute.Rect.Max.X > Attribute.AtlasSize.X || Attribute.Rect.Max.Y > Attribute.AtlasSize.Y) { bValid = false; break; }
			Result.Attributes.Add(MoveTemp(Attribute));
		}
		if (bValid) for (const auto& Item : *Skills)
		{
			const auto Obj = Item->AsObject(); FRokTrainArmyDetailSkill Skill;
			const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
			if (!Obj.IsValid() || !Obj->TryGetNumberField(TEXT("id"), Skill.Id)
				|| !Obj->TryGetStringField(TEXT("name"), Skill.Name) || !Obj->TryGetStringField(TEXT("tip"), Skill.Tip)
				|| !Obj->TryGetStringField(TEXT("texture"), Skill.Texture)
				|| !Obj->TryGetArrayField(TEXT("rect"), Rect) || Rect->Num() != 4
				|| !Obj->TryGetArrayField(TEXT("size"), Size) || Size->Num() != 2) { bValid = false; break; }
			const int32 X = (*Rect)[0]->AsNumber(), Y = (*Rect)[1]->AsNumber();
			Skill.Rect = FIntRect(X, Y, X + int32((*Rect)[2]->AsNumber()), Y + int32((*Rect)[3]->AsNumber()));
			Skill.AtlasSize = FIntPoint((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber());
			if (Skill.Rect.Width() <= 0 || Skill.Rect.Height() <= 0 || X < 0 || Y < 0
				|| Skill.Rect.Max.X > Skill.AtlasSize.X || Skill.Rect.Max.Y > Skill.AtlasSize.Y) { bValid = false; break; }
			Result.Skills.Add(MoveTemp(Skill));
		}
		if (!bValid) { Error = TEXT("TrainArmy detail row invalid"); return false; }
		Detail = MoveTemp(Result); return true;
	}
	Error = FString::Printf(TEXT("TrainArmy detail ID %d missing"), TroopId); return false;
}
