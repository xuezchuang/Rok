#include "RokGameNetSubsystem.h"
#include "Dom/JsonValue.h"

namespace
{
const TCHAR* SearchProtocol(int32 Type)
{
	return Type == 0 ? TEXT("Map_SearchBarbarian") : TEXT("Map_SearchResource");
}

bool ReadSearchPosition(const TSharedPtr<FJsonObject>& Row, FVector2D& Position)
{
	const TSharedPtr<FJsonObject>* Pos = nullptr;
	int64 X = 0, Y = 0;
	// Sproto PosInfo uses hundredths of the Unity world grid.
	if (!Row || !Row->TryGetObjectField(TEXT("pos"), Pos)
		|| !RokSprotoJson::TryGetInt64(*Pos, TEXT("x"), X) || !RokSprotoJson::TryGetInt64(*Pos, TEXT("y"), Y)
		|| X < 0 || Y < 0 || X > 100000000 || Y > 100000000) return false;
	Position = FVector2D(X, Y) / 100.0;
	return true;
}
}

TSharedPtr<FJsonObject> URokGameNetSubsystem::MakeWorldSearchBody(int32 Type, int32 Level)
{
	if (Type < 0 || Type > 4 || Level < 1 || Level > (Type == 0 ? 25 : 6)) return nullptr;
	auto Body = MakeShared<FJsonObject>();
	if (Type == 0) Body->SetNumberField(TEXT("level"), Level);
	else
	{
		Body->SetNumberField(TEXT("resourceType"), Type);
		Body->SetNumberField(TEXT("resourceLevel"), Level);
	}
	return Body;
}

bool URokGameNetSubsystem::ReadWorldSearchHits(const TSharedPtr<FJsonObject>& Body,
	int32 Type, int32 Level, FVector2D Origin, TArray<FRokWorldMapSearchHit>& OutHits)
{
	OutHits.Reset();
	int64 Returned = 0;
	if (!RokSprotoJson::TryGetInt64(Body, Type == 0 ? TEXT("level") : TEXT("resourceType"), Returned)
		|| Returned != (Type == 0 ? Level : Type)) return false;
	const TCHAR* Field = Type == 0 ? TEXT("barbarians") : TEXT("resources");
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Body->TryGetArrayField(Field, Rows)) return !Body->HasField(Field);
	TSet<int64> Seen;
	for (const auto& Value : *Rows)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		FRokWorldMapSearchHit Hit;
		int64 ResourceLevel = 0;
		if (!Value || !Value->TryGetObject(Row)
			|| !RokSprotoJson::TryGetInt64(*Row, TEXT("objectId"), Hit.ObjectId) || Hit.ObjectId <= 0
			|| Seen.Contains(Hit.ObjectId) || !ReadSearchPosition(*Row, Hit.UnityPosition)
			|| (Type != 0 && (!RokSprotoJson::TryGetInt64(*Row, TEXT("resourceLevel"), ResourceLevel)
				|| ResourceLevel < Level))) continue;
		Seen.Add(Hit.ObjectId);
		OutHits.Add(Hit);
	}
	OutHits.StableSort([Origin](const FRokWorldMapSearchHit& A, const FRokWorldMapSearchHit& B)
	{
		return FVector2D::DistSquared(A.UnityPosition, Origin) < FVector2D::DistSquared(B.UnityPosition, Origin);
	});
	return Rows->IsEmpty() || !OutHits.IsEmpty();
}

bool URokGameNetSubsystem::RequestWorldMapSearch(int32 Type, int32 Level,
	FRokWorldMapSearchResult OnResult, FString& OutError)
{
	OutError.Reset();
	const auto Body = MakeWorldSearchBody(Type, Level);
	if (!Body) OutError = TEXT("搜索类型或等级无效。");
	else if (State != ERokGameNetState::RoleReady || (!Worker.IsValid() && !bSnapshotReplay))
		OutError = TEXT("连接服务器后可搜索附近目标。");
	else if (PendingWorldSearch.IsSet()) OutError = TEXT("正在搜索，请稍候。");
	if (!OutError.IsEmpty()) return false;

	const double Now = FPlatformTime::Seconds();
	if (Type == CachedWorldSearchType && Level == CachedWorldSearchLevel && Now < WorldSearchCacheUntil)
	{
		if (OnResult) OnResult(TakeWorldSearchHit(), FString());
		return true;
	}
	const FString Protocol(SearchProtocol(Type));
	if (bSnapshotReplay)
	{
		const TSharedPtr<FJsonObject>* Result = nullptr;
		FVector2D Origin = FVector2D::ZeroVector;
		ReadSearchPosition(GetRoleInfo(), Origin);
		TArray<FRokWorldMapSearchHit> Hits;
		if (!MergedState || !MergedState->TryGetObjectField(Protocol + TEXT(".response"), Result)
			|| !ReadWorldSearchHits(*Result, Type, Level, Origin, Hits))
		{
			OutError = TEXT("服务器快照中没有本次搜索的结果。");
			return false;
		}
		CachedWorldSearchType = Type;
		CachedWorldSearchLevel = Level;
		WorldSearchCacheUntil = Now + 4.0;
		NextWorldSearchHit = 0;
		WorldSearchHits = MoveTemp(Hits);
		if (OnResult) OnResult(TakeWorldSearchHit(), FString());
		return true;
	}
	if (!SendRequest(Protocol, Body))
	{
		OutError = TEXT("搜索请求发送失败。");
		return false;
	}
	PendingWorldSearch = FWorldSearch{Type, Level, SprotoSession, SprotoSession - 1, Now + 10.0, MoveTemp(OnResult)};
	return true;
}

void URokGameNetSubsystem::AcknowledgeWorldSearch(const FRokSprotoMessage& Message)
{
	if (!PendingWorldSearch.IsSet() || Message.bRequest || !Message.bHasSession
		|| Message.Session != PendingWorldSearch->InnerSession) return;
	const auto& Search = *PendingWorldSearch;
	FVector2D Origin = FVector2D::ZeroVector;
	ReadSearchPosition(GetRoleInfo(), Origin);
	TArray<FRokWorldMapSearchHit> Hits;
	if (!ReadWorldSearchHits(Message.Body, Search.Type, Search.Level, Origin, Hits))
	{
		FinishWorldSearch(TEXT("服务器返回了无效的搜索结果。"));
		return;
	}
	CachedWorldSearchType = Search.Type;
	CachedWorldSearchLevel = Search.Level;
	NextWorldSearchHit = 0;
	WorldSearchCacheUntil = FPlatformTime::Seconds() + 4.0;
	WorldSearchHits = MoveTemp(Hits);
	if (MergedState)
		MergedState->SetObjectField(FString(SearchProtocol(Search.Type)) + TEXT(".response"), Message.Body);
	FinishWorldSearch(FString());
}

FRokWorldMapSearchHit URokGameNetSubsystem::TakeWorldSearchHit()
{
	if (WorldSearchHits.IsEmpty()) return {};
	const auto Hit = WorldSearchHits[NextWorldSearchHit];
	NextWorldSearchHit = (NextWorldSearchHit + 1) % WorldSearchHits.Num();
	return Hit;
}

void URokGameNetSubsystem::FinishWorldSearch(const FString& Error)
{
	auto Callback = MoveTemp(PendingWorldSearch->OnResult);
	PendingWorldSearch.Reset();
	if (Callback) Callback(Error.IsEmpty() ? TakeWorldSearchHit() : FRokWorldMapSearchHit{}, Error);
}
