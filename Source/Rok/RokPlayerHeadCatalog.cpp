#include "RokPlayerHeadCatalog.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool RokPlayerHeadCatalog::Load(TArray<FRokPlayerHeadRow>& Rows, FString& Error)
{
	Rows.Reset();
	Error.Reset();
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/player_head_catalog.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		|| !Root.IsValid() || !Root->TryGetArrayField(TEXT("entries"), Entries))
	{
		Error = TEXT("PlayerHead catalog missing or invalid; run BuildRokPlayerHeadCatalog.py");
		return false;
	}
	TSet<int32> Ids;
	for (const auto& Value : *Entries)
	{
		const auto Obj = Value->AsObject();
		FRokPlayerHeadRow Row;
		int32 Get = 0;
		const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetNumberField(TEXT("id"), Row.Id)
			|| !Obj->TryGetNumberField(TEXT("group"), Row.Group)
			|| !Obj->TryGetNumberField(TEXT("get"), Get)
			|| !Obj->TryGetStringField(TEXT("name"), Row.Name)
			|| !Obj->TryGetStringField(TEXT("description"), Row.Description)
			|| !Obj->TryGetStringField(TEXT("tag"), Row.Tag)
			|| !Obj->TryGetStringField(TEXT("icon"), Row.Icon)
			|| !Obj->TryGetStringField(TEXT("texture"), Row.Texture)
			|| !Obj->TryGetBoolField(TEXT("art_available"), Row.bArtAvailable)
			|| !Obj->TryGetArrayField(TEXT("rect"), Rect) || Rect->Num() != 4
			|| !Obj->TryGetArrayField(TEXT("size"), Size) || Size->Num() != 2
			|| Row.Id <= 0 || Ids.Contains(Row.Id) || (Row.Group != 1 && Row.Group != 2))
		{
			Rows.Reset(); Error = TEXT("PlayerHead catalog contains an invalid row"); return false;
		}
		Row.bFree = Get == 1;
		const int32 X = (*Rect)[0]->AsNumber(), Y = (*Rect)[1]->AsNumber();
		Row.Rect = FIntRect(X, Y, X + int32((*Rect)[2]->AsNumber()), Y + int32((*Rect)[3]->AsNumber()));
		Row.AtlasSize = FIntPoint((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber());
		if (Row.bArtAvailable && (Row.Texture.IsEmpty() || Row.Rect.Width() <= 0 || Row.Rect.Height() <= 0
			|| X < 0 || Y < 0 || Row.Rect.Max.X > Row.AtlasSize.X || Row.Rect.Max.Y > Row.AtlasSize.Y))
		{
			Rows.Reset(); Error = TEXT("PlayerHead catalog has invalid atlas bounds"); return false;
		}
		Ids.Add(Row.Id); Rows.Add(MoveTemp(Row));
	}
	if (Rows.IsEmpty()) { Error = TEXT("PlayerHead catalog is empty"); return false; }
	return true;
}
