#include "RokLeaderboardCatalog.h"

#include "RokDataPaths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool RokLeaderboardCatalog::Load(TArray<FRokLeaderboardRow>& Rows, FString& Error)
{
	Rows.Reset(); Error.Reset();
	FString Json; TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *(RokDataPaths::Resolve(TEXT("leaderboard_catalog.json"))))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("entries"), Entries) || Entries->Num() != 9)
	{ Error = TEXT("Leaderboard catalog missing/invalid; run BuildRokLeaderboardCatalog.py"); return false; }
	int32 PreviousId = 0;
	for (const auto& Entry : *Entries)
	{
		const auto Obj = Entry->AsObject(); FRokLeaderboardRow Row; int32 List = 0;
		const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetNumberField(TEXT("id"), Row.Id) || Row.Id <= PreviousId
			|| !Obj->TryGetNumberField(TEXT("list"), List) || List != 2
			|| !Obj->TryGetNumberField(TEXT("target"), Row.Target) || !Obj->TryGetNumberField(TEXT("type"), Row.Type)
			|| !Obj->TryGetNumberField(TEXT("show_limit"), Row.ShowLimit)
			|| !Obj->TryGetStringField(TEXT("name"), Row.Name) || !Obj->TryGetStringField(TEXT("icon"), Row.Icon)
			|| !Obj->TryGetArrayField(TEXT("rect"), Rect) || !Obj->TryGetArrayField(TEXT("columns"), Columns)
			|| (Columns->Num() != 0 && Columns->Num() != 2) || Rect->Num() != (Row.HasIcon() ? 4 : 0))
		{ Rows.Reset(); Error = TEXT("Leaderboard catalog contains invalid source bindings"); return false; }
		if (Row.HasIcon())
		{
			const int32 X = (*Rect)[0]->AsNumber(), Y = (*Rect)[1]->AsNumber();
			const int32 W = (*Rect)[2]->AsNumber(), H = (*Rect)[3]->AsNumber();
			if (!Row.Icon.StartsWith(TEXT("ui_player[")) || W <= 0 || H <= 0 || X < 0 || Y < 0 || X + W > 512 || Y + H > 1024)
			{ Rows.Reset(); Error = TEXT("Leaderboard icon is outside the source atlas"); return false; }
			Row.IconRect = FIntRect(X, Y, X + W, Y + H);
		}
		for (const auto& Column : *Columns) Row.Columns.Add(Column->AsString());
		PreviousId = Row.Id; Rows.Add(MoveTemp(Row));
	}
	return true;
}
