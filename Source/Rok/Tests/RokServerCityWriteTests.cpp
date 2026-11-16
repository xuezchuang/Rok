#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/Base64.h"
#include "../RokGameNetSubsystem.h"
#include "../RokServerTaskSubsystem.h"
#include "../RokSkynetCrypt.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetCityWriteTest, "Rok.Net.City.WriteConfirmation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGameNetCityWriteTest::RunTest(const FString&)
{
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>(GEngine));
	Net->Secret.Init(1, 8);
	Net->MergedState = MakeShared<FJsonObject>();
	FString Error;
	if (!TestTrue(TEXT("Load city write schema"), Net->LoadSchema(Error))) return false;
	const FRokSprotoProtocol* Gate = Net->Schema.FindProtocol(TEXT("GateMessage"));
	const FRokSprotoProtocol* Update = Net->Schema.FindProtocol(TEXT("Build_BuildingInfo"));
	if (!TestNotNull(TEXT("Gate protocol"), Gate) || !TestNotNull(TEXT("Building push"), Update)) return false;

	int32 Calls = 0;
	FString Result;
	auto Start = [&](const TCHAR* Name, int32 Index, int32 Type, bool bInstant)
	{
		const auto Body = MakeShared<FJsonObject>();
		if (Type > 0)
		{
			const auto Pos = MakeShared<FJsonObject>();
			Pos->SetNumberField(TEXT("x"), -3);
			Pos->SetNumberField(TEXT("y"), 2);
			Body->SetObjectField(TEXT("pos"), Pos);
			Body->SetNumberField(TEXT("type"), Type);
		}
		else
		{
			Body->SetNumberField(TEXT("buildingIndex"), Index);
			Body->SetBoolField(TEXT("immediately"), bInstant);
		}
		TArray<uint8> Frame;
		if (!TestTrue(TEXT("Request encodes"), Net->BuildRequestFrame(Name, Body, Frame, Error))) return false;
		Net->CityWrite.Emplace();
		auto& Write = Net->CityWrite.GetValue();
		Write.OuterSession = Net->SprotoSession;
		Write.InnerSession = Net->SprotoSession - 1;
		Write.Deadline = 30.0;
		Write.Index = Index;
		Write.Type = Type;
		Write.Grid = FIntPoint(-3, 2);
		Write.TargetLevel = 2;
		Write.bImmediately = bInstant;
		Write.OnResult = [&](const FString& Failure) { ++Calls; Result = Failure; };
		return true;
	};

	auto EncodeResponse = [&](int64 Session, int32 Type, const FJsonObject& Body, TArray<uint8>& Out)
	{
		FJsonObject Header;
		Header.SetNumberField(TEXT("session"), Session);
		TArray<uint8> Bytes, Payload;
		if (!Net->Schema.Encode(Net->Schema.FindStruct(TEXT("package")), Header, Bytes, Error)
			|| !Net->Schema.Encode(Type, Body, Payload, Error)) return false;
		Bytes.Append(Payload);
		FRokSprotoSchema::Pack(Bytes.GetData(), Bytes.Num(), Out);
		return true;
	};
	auto Receive = [&](int64 Session, const TArray<TSharedPtr<FJsonValue>>& Contents)
	{
		FJsonObject Body;
		Body.SetArrayField(TEXT("content"), Contents);
		TArray<uint8> Encoded, Frame;
		if (!TestTrue(TEXT("Gate response encodes"), EncodeResponse(Session, Gate->ResponseType, Body, Encoded))) return;
		RokSkynetCrypt::DesEncode(Net->Secret, Encoded.GetData(), Encoded.Num(), Frame);
		Frame.AddZeroed(5); // Four game session bytes and the uncompressed flag.
		Net->HandleFrame(Frame);
	};
	auto Ack = [&](const TCHAR* Name, int32 Index, bool bInstant)
	{
		FJsonObject AckBody;
		AckBody.SetNumberField(TEXT("buildingIndex"), Index);
		if (FCString::Strcmp(Name, TEXT("Build_UpGradeBuilding")) == 0)
			AckBody.SetBoolField(TEXT("immediately"), bInstant);
		const auto* Protocol = Net->Schema.FindProtocol(Name);
		TArray<uint8> Encoded;
		if (!Protocol || !TestTrue(TEXT("Inner ACK encodes"),
			EncodeResponse(Net->CityWrite->InnerSession, Protocol->ResponseType, AckBody, Encoded))) return;
		const auto Message = MakeShared<FJsonObject>();
		Message->SetStringField(TEXT("networkMessage"), FBase64::Encode(Encoded));
		Receive(Net->CityWrite->OuterSession, {MakeShared<FJsonValueObject>(Message)});
	};
	auto PushBuilding = [&](int32 Index, int32 Type, int32 Level, int64 Finish)
	{
		const auto Pos = MakeShared<FJsonObject>();
		Pos->SetNumberField(TEXT("x"), -3);
		Pos->SetNumberField(TEXT("y"), 2);
		const auto Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("buildingIndex"), Index);
		Row->SetNumberField(TEXT("type"), Type);
		Row->SetNumberField(TEXT("level"), Level);
		Row->SetNumberField(TEXT("finishTime"), Finish);
		Row->SetObjectField(TEXT("pos"), Pos);
		const auto Body = MakeShared<FJsonObject>();
		Body->SetArrayField(TEXT("buildingInfo"), {MakeShared<FJsonValueObject>(Row)});
		Net->MergePush(*Update, Body);
	};

	const TCHAR* Create = TEXT("Build_CreateBuilding");
	const TCHAR* Upgrade = TEXT("Build_UpGradeBuilding");
	if (!Start(Create, 0, 10, false)) return false;
	Ack(Create, 27, false);
	Net->CheckCityWrite(1.0);
	TestEqual(TEXT("Create ACK alone does not complete"), Calls, 0);
	PushBuilding(27, 10, 0, 100);
	Net->CheckCityWrite(2.0);
	TestEqual(TEXT("New level-zero building confirms creation"), Calls, 1);
	TestTrue(TEXT("Creation succeeds"), Result.IsEmpty());

	if (!Start(Upgrade, 6, 0, false)) return false;
	PushBuilding(6, 9, 1, 100);
	Net->CheckCityWrite(3.0);
	TestEqual(TEXT("Push alone is not an ACK"), Calls, 1);
	Ack(Upgrade, 6, false);
	Net->CheckCityWrite(4.0);
	TestEqual(TEXT("Timed upgrade confirms with a running server timer"), Calls, 2);
	TestTrue(TEXT("Timed upgrade succeeds"), Result.IsEmpty());

	if (!Start(Upgrade, 6, 0, true)) return false;
	Ack(Upgrade, 6, true);
	Net->CheckCityWrite(5.0);
	TestEqual(TEXT("Existing timer cannot confirm instant upgrade"), Calls, 2);
	PushBuilding(6, 9, 2, -1);
	Net->CheckCityWrite(6.0);
	TestEqual(TEXT("New server level confirms instant upgrade"), Calls, 3);
	TestTrue(TEXT("Instant upgrade succeeds"), Result.IsEmpty());

	if (!Start(Create, 0, 2, false)) return false;
	const auto Rejection = MakeShared<FJsonObject>();
	Rejection->SetNumberField(TEXT("errorCode"), 6001);
	const auto Message = MakeShared<FJsonObject>();
	Message->SetObjectField(TEXT("error"), Rejection);
	Receive(Net->CityWrite->OuterSession, {MakeShared<FJsonValueObject>(Message)});
	TestEqual(TEXT("Gate rejection resolves its command"), Calls, 4);
	TestTrue(TEXT("Rejected placement exposes failure"), !Result.IsEmpty());

	if (!Start(Create, 0, 2, false)) return false;
	Net->CheckCityWrite(30.0);
	TestEqual(TEXT("Timeout resolves the pending UI"), Calls, 5);
	TestTrue(TEXT("Uncertain write requires reconnect before retry"), Net->bCityWriteNeedsReconnect);

	Net->bCityWriteNeedsReconnect = false;
	if (!Start(Create, 0, 2, false)) return false;
	Rejection->SetNumberField(TEXT("errorCode"), 500);
	Receive(Net->CityWrite->OuterSession, {MakeShared<FJsonValueObject>(Message)});
	TestEqual(TEXT("Server exception resolves the pending UI"), Calls, 6);
	TestTrue(TEXT("Server exception cannot be retried blindly"), Net->bCityWriteNeedsReconnect);

	const auto* RolePush = Net->Schema.FindProtocol(TEXT("Role_RoleInfo"));
	const auto* TaskFinish = Net->Schema.FindProtocol(TEXT("Task_TaskFinish"));
	if (!TestNotNull(TEXT("Role push protocol"), RolePush) || !TestNotNull(TEXT("Task claim protocol"), TaskFinish)) return false;
	const auto SideClaim = MakeShared<FJsonObject>();
	SideClaim->SetNumberField(TEXT("taskId"), 309001);
	TArray<uint8> RequestFrame, AckFrame;
	if (!TestTrue(TEXT("Side claim request encodes"), Net->BuildRequestFrame(TaskFinish->Name, SideClaim, RequestFrame, Error))) return false;
	Net->PendingTaskClaim = URokGameNetSubsystem::FTaskClaim{ERokTaskClaim::Side, 309001,
		Net->SprotoSession, Net->SprotoSession - 1, 30.0, false,
		[&](const FString& Failure) { ++Calls; Result = Failure; }};
	if (!TestTrue(TEXT("Task ACK encodes"), EncodeResponse(Net->PendingTaskClaim->InnerSession,
		TaskFinish->ResponseType, *SideClaim, AckFrame))) return false;
	const auto AckMessage = MakeShared<FJsonObject>();
	AckMessage->SetStringField(TEXT("networkMessage"), FBase64::Encode(AckFrame));
	Receive(Net->PendingTaskClaim->OuterSession, {MakeShared<FJsonValueObject>(AckMessage)});
	Net->CheckTaskClaim(1.0);
	TestEqual(TEXT("Task ACK alone does not complete claim"), Calls, 6);
	const auto UpdatedRole = MakeShared<FJsonObject>();
	UpdatedRole->SetArrayField(TEXT("finishSideTasks"), {MakeShared<FJsonValueObject>(SideClaim)});
	const auto PushRole = MakeShared<FJsonObject>();
	PushRole->SetObjectField(TEXT("roleInfo"), UpdatedRole);
	Net->MergePush(*RolePush, PushRole);
	Net->CheckTaskClaim(2.0);
	TestEqual(TEXT("Side claim completes when the server marks it received"), Calls, 7);
	TestTrue(TEXT("Side claim succeeds"), Result.IsEmpty());

	using EQueueAction = URokGameNetSubsystem::FCityWrite::EQueueAction;
	const auto PushQueue = [&](int32 QueueIndex, int32 Building, int64 Finish, int64 Expiry)
	{
		const auto Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("queueIndex"), QueueIndex);
		Row->SetNumberField(TEXT("buildingIndex"), Building);
		Row->SetNumberField(TEXT("finishTime"), Finish);
		Row->SetNumberField(TEXT("expiredTime"), Expiry);
		const auto Role = MakeShared<FJsonObject>();
		Role->SetArrayField(TEXT("buildQueue"), {MakeShared<FJsonValueObject>(Row)});
		const auto Push = MakeShared<FJsonObject>();
		Push->SetObjectField(TEXT("roleInfo"), Role);
		Net->MergePush(*RolePush, Push);
	};
	const auto StartQueueWrite = [&](const TCHAR* Name, EQueueAction Action, int32 QueueIndex, int32 Building,
		int64 PreviousTime, const TSharedPtr<FJsonObject>& Body)
	{
		TArray<uint8> Frame;
		if (!TestTrue(Name, Net->BuildRequestFrame(Name, Body, Frame, Error))) return false;
		Net->CityWrite.Emplace();
		auto& Write = Net->CityWrite.GetValue();
		Write.QueueAction = Action;
		Write.QueueIndex = QueueIndex;
		Write.Index = Building;
		Write.PreviousQueueTime = PreviousTime;
		Write.OuterSession = Net->SprotoSession;
		Write.InnerSession = Net->SprotoSession - 1;
		Write.Deadline = 30;
		Write.OnResult = [&](const FString& Failure) { ++Calls; Result = Failure; };
		return true;
	};
	const auto QueueAck = [&](const TCHAR* Name, const FJsonObject& Body)
	{
		const auto* Protocol = Net->Schema.FindProtocol(Name);
		TArray<uint8> Encoded;
		if (!Protocol || !TestTrue(TEXT("Queue ACK encodes"),
			EncodeResponse(Net->CityWrite->InnerSession, Protocol->ResponseType, Body, Encoded))) return;
		const auto Inner = MakeShared<FJsonObject>();
		Inner->SetStringField(TEXT("networkMessage"), FBase64::Encode(Encoded));
		Receive(Net->CityWrite->OuterSession, {MakeShared<FJsonValueObject>(Inner)});
	};
	const TCHAR* Hire = TEXT("Build_UnlockBuildQueue");
	const auto HireItem = MakeShared<FJsonObject>();
	HireItem->SetNumberField(TEXT("itemId"), URokGameNetSubsystem::WorkerHireItemId);
	FJsonObject HireResult;
	HireResult.SetBoolField(TEXT("result"), true);
	if (!StartQueueWrite(Hire, EQueueAction::Hire, 2, 0, 0, HireItem)) return false;
	QueueAck(Hire, HireResult);
	Net->CheckCityWrite(1);
	TestEqual(TEXT("Hire ACK waits for second-queue push"), Calls, 7);
	PushQueue(2, 0, -1, 172800);
	Net->CheckCityWrite(2);
	TestEqual(TEXT("Item hire confirmed by new lease"), Calls, 8);
	TestTrue(TEXT("Item hire succeeds"), Result.IsEmpty());
	if (!StartQueueWrite(Hire, EQueueAction::Hire, 2, 0, 172800, MakeShared<FJsonObject>())) return false;
	PushQueue(2, 0, -1, 345600);
	Net->CheckCityWrite(3);
	TestEqual(TEXT("Renewal push still requires ACK"), Calls, 8);
	QueueAck(Hire, HireResult);
	Net->CheckCityWrite(4);
	TestEqual(TEXT("Gem renewal confirmed by extended lease"), Calls, 9);
	TestTrue(TEXT("Gem renewal succeeds"), Result.IsEmpty());

	const TCHAR* SpeedUp = TEXT("Role_SpeedUp");
	for (int32 QueueIndex : {1, 2})
	{
		const int32 Building = 25 + QueueIndex;
		const auto Body = MakeShared<FJsonObject>();
		Body->SetNumberField(TEXT("queueIndex"), QueueIndex);
		Body->SetNumberField(TEXT("type"), 20102);
		Body->SetNumberField(TEXT("itemId"), 201010010);
		Body->SetNumberField(TEXT("itemNum"), 1);
		Body->SetBoolField(TEXT("costDenar"), QueueIndex == 2);
		PushQueue(QueueIndex, Building, 900, QueueIndex == 1 ? -1 : 345600);
		if (!StartQueueWrite(SpeedUp, EQueueAction::SpeedUp, QueueIndex, Building, 900, Body)) return false;
		FJsonObject SpeedResult;
		SpeedResult.SetNumberField(TEXT("finishTime"), QueueIndex == 1 ? 840 : 0);
		QueueAck(SpeedUp, SpeedResult);
		Net->CheckCityWrite(5);
		TestEqual(TEXT("Speed-up ACK alone does not alter the timer"), Calls, 8 + QueueIndex);
		PushQueue(QueueIndex, QueueIndex == 1 ? Building : 0, QueueIndex == 1 ? 840 : -1,
			QueueIndex == 1 ? -1 : 345600);
		Net->CheckCityWrite(6);
		TestEqual(TEXT("Speed-up confirms the exact queue timer change or completion"), Calls, 9 + QueueIndex);
		TestTrue(TEXT("Speed-up succeeds"), Result.IsEmpty());
	}

	Net->bSnapshotReplay = true;
	TestFalse(TEXT("Snapshot playback rejects live city writes"), Net->CanRequestCityWrite(Error));
	TestFalse(TEXT("Snapshot playback rejects live reward claims"),
		Net->RequestTaskClaim(ERokTaskClaim::Side, 309002, {}, Error));
	return true;
}
#endif
