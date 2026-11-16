#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/Base64.h"
#include "../RokGameNetSubsystem.h"
#include "../RokSkynetCrypt.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerTrainingWriteTest, "Rok.Net.Training.WriteLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerTrainingWriteTest::RunTest(const FString&)
{
	auto* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>());
	Net->Secret.Init(1, 8);
	Net->MergedState = MakeShared<FJsonObject>();
	FString Error;
	if (!TestTrue(TEXT("Server schema loads"), Net->LoadSchema(Error))) return false;
	const auto* Train = Net->Schema.FindProtocol(TEXT("Role_TrainArmy"));
	const auto* Award = Net->Schema.FindProtocol(TEXT("Role_AwardArmy"));
	const auto* RolePush = Net->Schema.FindProtocol(TEXT("Role_RoleInfo"));
	const auto* Gate = Net->Schema.FindProtocol(TEXT("GateMessage"));
	if (!Train || !Award || !RolePush || !Gate) return false;

	auto Write = URokGameNetSubsystem::FTrainingWrite();
	Write.BuildingIndex = 30;
	Write.ArmyType = 3;
	Write.Tier = 1;
	Write.Count = 20;
	Write.TroopId = 10301;
	int32 Calls = 0;
	FString Result;
	Write.OnResult = [&Calls, &Result](const FString& Failure) { ++Calls; Result = Failure; };

	auto Start = [&]()
	{
		const auto* Protocol = Write.bReceive ? Award : Train;
		const auto Body = URokGameNetSubsystem::MakeTrainingBody(Write);
		TArray<uint8> Frame, Packed;
		FRokSprotoRpc Client(Net->Schema);
		FRokSprotoRpc Server(Net->Schema);
		FRokSprotoMessage Request;
		if (!TestTrue(TEXT("Request round-trips through the live schema"),
			Net->BuildRequestFrame(Protocol->Name, Body, Frame, Error)
			&& Client.EncodeRequest(Protocol->Tag, Body.Get(), true, 1, Packed, Error)
			&& Server.Dispatch(Packed.GetData(), Packed.Num(), Request, Error))) return false;
		int64 Value = 0;
		TestTrue(TEXT("Correct army family"), RokSprotoJson::TryGetInt64(Request.Body, TEXT("type"), Value) && Value == 3);
		if (Write.bReceive)
		{
			TestEqual(TEXT("Award only includes family"), Request.Body->Values.Num(), 1);
		}
		else
		{
			TestTrue(TEXT("Train uses the real building"), RokSprotoJson::TryGetInt64(Request.Body, TEXT("buildingIndex"), Value) && Value == 30);
			TestTrue(TEXT("Tier one"), RokSprotoJson::TryGetInt64(Request.Body, TEXT("level"), Value) && Value == 1);
			TestTrue(TEXT("Requested count"), RokSprotoJson::TryGetInt64(Request.Body, TEXT("trainNum"), Value) && Value == 20);
			TestTrue(TEXT("Ordinary training"), RokSprotoJson::TryGetInt64(Request.Body, TEXT("isUpdate"), Value) && Value == 0);
			bool bImmediately = false;
			Request.Body->TryGetBoolField(TEXT("immediately"), bImmediately);
			TestEqual(TEXT("Only confirmed instant training spends gems"), bImmediately, Write.bImmediately);
		}
		Write.InnerSession = Net->SprotoSession - 1;
		Write.OuterSession = Net->SprotoSession;
		Write.Deadline = 12;
		Net->PendingTrainingWrite = Write;
		return true;
	};
	auto PackResponse = [&](int64 Session, int32 Type, const FJsonObject& Body, TArray<uint8>& Packed)
	{
		FJsonObject Header;
		Header.SetNumberField(TEXT("session"), Session);
		TArray<uint8> Bytes, Payload;
		if (!Net->Schema.Encode(Net->Schema.FindStruct(TEXT("package")), Header, Bytes, Error)
			|| !Net->Schema.Encode(Type, Body, Payload, Error)) return false;
		Bytes.Append(Payload);
		FRokSprotoSchema::Pack(Bytes.GetData(), Bytes.Num(), Packed);
		return true;
	};
	auto Receive = [&](const TSharedPtr<FJsonObject>& Message)
	{
		FJsonObject Body;
		Body.SetArrayField(TEXT("content"), {MakeShared<FJsonValueObject>(Message)});
		TArray<uint8> Packed, Frame;
		if (!TestTrue(TEXT("Gate response encodes"), PackResponse(Write.OuterSession, Gate->ResponseType, Body, Packed))) return;
		RokSkynetCrypt::DesEncode(Net->Secret, Packed.GetData(), Packed.Num(), Frame);
		Frame.AddZeroed(5);
		Net->HandleFrame(Frame);
	};
	auto ReceiveAck = [&](const FJsonObject& Response)
	{
		TArray<uint8> Packed;
		const int32 ResponseType = (Write.bReceive ? Award : Train)->ResponseType;
		if (!TestTrue(TEXT("Inner response encodes"), PackResponse(Write.InnerSession, ResponseType, Response, Packed))) return;
		auto Message = MakeShared<FJsonObject>();
		Message->SetStringField(TEXT("networkMessage"), FBase64::Encode(Packed));
		Receive(Message);
	};
	auto PushRole = [&](const TSharedPtr<FJsonObject>& Role)
	{
		auto Push = MakeShared<FJsonObject>();
		Push->SetObjectField(TEXT("roleInfo"), Role);
		Net->MergePush(*RolePush, Push);
	};

	if (!Start()) return false;
	ReceiveAck(FJsonObject());
	Net->CheckTrainingWrite(1);
	TestTrue(TEXT("Empty train acknowledgement waits for queue"), Net->IsTrainingWritePending() && Calls == 0);
	auto Role = MakeShared<FJsonObject>();
	Role->SetNumberField(TEXT("wood"), 175700);
	auto Queue = MakeShared<FJsonObject>();
	Queue->SetNumberField(TEXT("queueIndex"), 3);
	Queue->SetNumberField(TEXT("buildingIndex"), 30);
	Queue->SetNumberField(TEXT("armyType"), 3);
	Queue->SetNumberField(TEXT("newArmyLevel"), 1);
	Queue->SetNumberField(TEXT("armyNum"), 19);
	Queue->SetNumberField(TEXT("finishTime"), 2000);
	Role->SetArrayField(TEXT("armyQueue"), {MakeShared<FJsonValueObject>(Queue)});
	PushRole(Role);
	Net->CheckTrainingWrite(2);
	TestEqual(TEXT("Different queue count does not confirm"), Calls, 0);
	Queue->SetNumberField(TEXT("armyNum"), 20);
	PushRole(Role);
	Net->CheckTrainingWrite(3);
	TestTrue(TEXT("Matching train push confirms once"), Calls == 1 && Result.IsEmpty() && !Net->IsTrainingWritePending());
	int64 Wood = 0;
	TestTrue(TEXT("Inventory stays server-owned"),
		RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("wood"), Wood) && Wood == 175700);

	Write.bReceive = true;
	Write.PreviousTroops = 100;
	if (!Start()) return false;
	const auto AwardRow = MakeShared<FJsonObject>();
	AwardRow->SetNumberField(TEXT("id"), 301); // response uses family*100+tier, role inventory uses Arms.ID
	AwardRow->SetNumberField(TEXT("num"), 20);
	FJsonObject Ack;
	Ack.SetArrayField(TEXT("soldiers"), {MakeShared<FJsonValueObject>(AwardRow)});
	Queue->SetNumberField(TEXT("finishTime"), -1);
	PushRole(Role);
	ReceiveAck(Ack);
	Net->CheckTrainingWrite(4);
	TestEqual(TEXT("An idle queue alone cannot confirm receipt"), Calls, 1);
	const auto Soldier = MakeShared<FJsonObject>();
	Soldier->SetNumberField(TEXT("id"), 10301);
	Soldier->SetNumberField(TEXT("type"), 3);
	Soldier->SetNumberField(TEXT("level"), 1);
	Soldier->SetNumberField(TEXT("num"), 120);
	Role->SetArrayField(TEXT("soldiers"), {MakeShared<FJsonValueObject>(Soldier)});
	PushRole(Role);
	Net->CheckTrainingWrite(5);
	TestTrue(TEXT("Roster push confirms collected troops"), Calls == 2 && Result.IsEmpty());

	Write.bReceive = false;
	Write.bImmediately = true;
	Write.PreviousTroops = 120;
	if (!Start()) return false;
	AwardRow->SetNumberField(TEXT("id"), 10301);
	ReceiveAck(Ack);
	Net->CheckTrainingWrite(6);
	TestEqual(TEXT("Instant training ACK still waits for roster push"), Calls, 2);
	Soldier->SetNumberField(TEXT("num"), 140);
	Role->SetNumberField(TEXT("denar"), 234);
	PushRole(Role);
	Net->CheckTrainingWrite(7);
	TestTrue(TEXT("Instant training confirms after the authoritative roster update"), Calls == 3 && Result.IsEmpty());

	Write.bImmediately = false;
	if (!Start()) return false;
	auto Rejection = MakeShared<FJsonObject>();
	Rejection->SetNumberField(TEXT("errorCode"), 1011);
	auto Message = MakeShared<FJsonObject>();
	Message->SetObjectField(TEXT("error"), Rejection);
	AddExpectedError(TEXT("Role_TrainArmy returned error 1011"), EAutomationExpectedErrorFlags::Contains, 1);
	Receive(Message);
	TestTrue(TEXT("Known rejection reports wood shortage and releases write"),
		Calls == 4 && Result.Contains(TEXT("木材不足")) && !Net->bTrainingWriteNeedsReconnect);
	if (!Start()) return false;
	Net->CheckTrainingWrite(12);
	TestTrue(TEXT("Unknown outcome blocks retry until reconnect"),
		Calls == 5 && Net->bTrainingWriteNeedsReconnect && Result.Contains(TEXT("重新连接")));
	return true;
}
#endif
