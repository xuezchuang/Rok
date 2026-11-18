#include "RokGameNetSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Compression.h"

namespace
{
constexpr int64 BattleReportSubtype = 2; // EmailSubType.BATTLE_REPORT
constexpr int32 MaxReportBytes = 8 * 1024 * 1024;

TSharedPtr<FJsonObject> MakeEmailIndexRequest(int64 EmailIndex)
{
	auto Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("emailIndexs"), {MakeShared<FJsonValueNumberString>(LexToString(EmailIndex))});
	return Body;
}

bool IsReportStorageUrl(const FString& Url)
{
	int32 Prefix = 0;
	if (Url.StartsWith(TEXT("https://"))) Prefix = 8;
	else if (Url.StartsWith(TEXT("http://"))) Prefix = 7;
	else return false;
	FString Host, Path;
	return Url.Mid(Prefix).Split(TEXT("/"), &Host, &Path)
		&& Host.EndsWith(TEXT(".igg.com")) && !Host.Contains(TEXT("@")) && !Host.Contains(TEXT("\\"));
}

TSharedPtr<FJsonObject> DecodeReport(const FRokSprotoSchema& Schema, const TArray<uint8>& Bytes)
{
	if (Bytes.IsEmpty() || Bytes.Num() > MaxReportBytes) return nullptr;
	const TArray<uint8>* Packed = &Bytes;
	TArray<uint8> Inflated;
	if (Bytes.Num() >= 18 && Bytes[0] == 0x1f && Bytes[1] == 0x8b)
	{
		const uint8* Tail = Bytes.GetData() + Bytes.Num() - 4;
		const uint32 Size = uint32(Tail[0]) | (uint32(Tail[1]) << 8) | (uint32(Tail[2]) << 16) | (uint32(Tail[3]) << 24);
		if (Size == 0 || Size > MaxReportBytes) return nullptr;
		Inflated.SetNumUninitialized(Size);
		if (!FCompression::UncompressMemory(NAME_Gzip, Inflated.GetData(), Size, Bytes.GetData(), Bytes.Num())) return nullptr;
		Packed = &Inflated;
	}
	TArray<uint8> Raw;
	if (!FRokSprotoSchema::Unpack(Packed->GetData(), Packed->Num(), Raw)) return nullptr;
	TSharedPtr<FJsonObject> Report;
	FString Error;
	return Schema.Decode(Schema.FindStruct(TEXT("BattleReportEx")), Raw.GetData(), Raw.Num(), Report, Error) > 0
		&& Report && Report->HasField(TEXT("objectInfos")) ? Report : nullptr;
}
}

void URokGameNetSubsystem::MergeBattleReportMail(const TSharedPtr<FJsonObject>& Email)
{
	int64 Index = 0, Id = 0, Subtype = 0;
	if (!RokSprotoJson::TryGetInt64(Email, TEXT("emailIndex"), Index) || Index <= 0) return;
	if (RokSprotoJson::TryGetInt64(Email, TEXT("emailId"), Id) && Id < 0)
	{
		SessionMail.Remove(Index);
		BattleReportMail.Remove(Index);
		++BattleReportRevision;
		BattleReportRequests.Remove(Index);
		FailedBattleReportDownloads.Remove(Index);
		return;
	}
	auto& InboxRow = SessionMail.FindOrAdd(Index);
	if (!InboxRow)
	{
		InboxRow = MakeShared<FJsonObject>();
	}
	for (const auto& Field : Email->Values)
	{
		InboxRow->SetField(Field.Key, Field.Value);
	}
	++BattleReportRevision;
	const bool bReport = RokSprotoJson::TryGetInt64(Email, TEXT("subType"), Subtype) && Subtype == BattleReportSubtype;
	if (!bReport && !BattleReportMail.Contains(Index)) return;
	auto& Row = BattleReportMail.FindOrAdd(Index);
	if (!Row) Row = MakeShared<FJsonObject>();
	for (const TCHAR* Field : {TEXT("emailId"), TEXT("sendTime"), TEXT("status"), TEXT("reportSubTile"),
		TEXT("reportStatus"), TEXT("battleReportEx"), TEXT("battleReportExContent")})
		if (const auto Value = Email->TryGetField(Field)) Row->SetField(Field, Value);
	++BattleReportRevision;
	FString Url;
	if (!Row->HasField(TEXT("battleReportExContent")) && Email->TryGetStringField(TEXT("battleReportEx"), Url))
		DownloadBattleReport(Index, Url);
}

bool URokGameNetSubsystem::RequestBattleReport(int64 EmailIndex)
{
	const auto* Row = BattleReportMail.Find(EmailIndex);
	if (!Row || State != ERokGameNetState::RoleReady || !Worker) return false;
	if ((*Row)->HasField(TEXT("battleReportExContent"))) return true;
	const double Now = FPlatformTime::Seconds();
	if (const auto* Last = BattleReportRequests.Find(EmailIndex); Last && Now - *Last < 12.0) return true;
	if (!SendRequest(TEXT("Email_GetEmailInfo"), MakeEmailIndexRequest(EmailIndex))) return false;
	BattleReportRequests.Add(EmailIndex, Now);
	return true;
}

void URokGameNetSubsystem::MarkBattleReportRead(int64 EmailIndex)
{
	const auto* Row = BattleReportMail.Find(EmailIndex);
	int64 Status = 1;
	if (State == ERokGameNetState::RoleReady && Worker && Row
		&& RokSprotoJson::TryGetInt64(*Row, TEXT("status"), Status) && Status == 0)
		SendRequest(TEXT("Email_UpdateEmailStatus"), MakeEmailIndexRequest(EmailIndex));
}

void URokGameNetSubsystem::DownloadBattleReport(int64 EmailIndex, const FString& Url)
{
	if (!IsReportStorageUrl(Url))
	{
		FailedBattleReportDownloads.Add(EmailIndex);
		return;
	}
	const auto Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(10.0f);
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, EmailIndex, Url, Session = BattleReportSession](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
		{
			const auto* Row = BattleReportMail.Find(EmailIndex);
			FString CurrentUrl;
			if (Session != BattleReportSession || !Row || (*Row)->HasField(TEXT("battleReportExContent"))
				|| !(*Row)->TryGetStringField(TEXT("battleReportEx"), CurrentUrl) || CurrentUrl != Url) return;
			const auto Report = bSuccess && Response && EHttpResponseCodes::IsOk(Response->GetResponseCode())
				? DecodeReport(Schema, Response->GetContent()) : nullptr;
			if (Report)
			{
				(*Row)->SetObjectField(TEXT("battleReportExContent"), Report);
				FailedBattleReportDownloads.Remove(EmailIndex);
			}
			else FailedBattleReportDownloads.Add(EmailIndex);
			++BattleReportRevision;
		});
	if (!Request->ProcessRequest()) FailedBattleReportDownloads.Add(EmailIndex);
}

bool URokGameNetSubsystem::GetRecentBattleDamage(int64 ObjectId, int64& Damage, double& Age) const
{
	const auto* Event = RecentBattleDamage.Find(ObjectId);
	if (!Event) return false;
	Age = FPlatformTime::Seconds() - Event->At;
	Damage = Event->Amount;
	return Age < 1.4;
}

bool URokGameNetSubsystem::IsMailInboxLoading() const
{
	if (State == ERokGameNetState::LoggingIn || State == ERokGameNetState::Connected)
	{
		return true;
	}
	const FRokSprotoProtocol* Protocol = Schema.FindProtocol(TEXT("Email_GetEmails"));
	return State == ERokGameNetState::RoleReady && Protocol && PendingInnerByOuter.FindKey(Protocol->Tag);
}

// Email.lua:GetEmailInfo/UpdateEmailStatus/TakeEnclosure are server-authoritative.
// Do not optimistically mark read or claimed: Email_EmailList pushes confirm both.
bool URokGameNetSubsystem::RequestMailDetails(int64 EmailIndex)
{
	return SessionMail.Contains(EmailIndex) && State == ERokGameNetState::RoleReady && Worker
		&& SendRequest(TEXT("Email_GetEmailInfo"), MakeEmailIndexRequest(EmailIndex));
}

void URokGameNetSubsystem::MarkMailRead(int64 EmailIndex)
{
	const auto* Mail = SessionMail.Find(EmailIndex);
	int64 Status = 1;
	if (Mail && RokSprotoJson::TryGetInt64(*Mail, TEXT("status"), Status) && Status == 0
		&& State == ERokGameNetState::RoleReady && Worker)
	{
		SendRequest(TEXT("Email_UpdateEmailStatus"), MakeEmailIndexRequest(EmailIndex));
	}
}

bool URokGameNetSubsystem::TakeMailEnclosure(int64 EmailIndex)
{
	const auto* Mail = SessionMail.Find(EmailIndex);
	bool bTaken = false;
	if (!Mail || State != ERokGameNetState::RoleReady || !Worker)
	{
		return false;
	}
	(*Mail)->TryGetBoolField(TEXT("takeEnclosure"), bTaken);
	if (bTaken || !(*Mail)->HasTypedField<EJson::Object>(TEXT("rewards")))
	{
		return false;
	}
	auto Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("type"), 1);
	Body->SetField(TEXT("data"), MakeShared<FJsonValueNumberString>(LexToString(EmailIndex)));
	return SendRequest(TEXT("Email_TakeEnclosure"), Body);
}
