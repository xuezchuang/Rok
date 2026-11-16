#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/Base64.h"
#include "../RokGameNetSubsystem.h"
#include "../RokServerArmySubsystem.h"
#include "../RokSkynetCrypt.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResourceGatherNetTest, "Rok.Net.Gather.CreateArmy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokResourceGatherNetTest::RunTest(const FString&)
{
	constexpr int64 TargetId = 9007199254741001LL;
	const auto Pos = MakeShared<FJsonObject>();
	Pos->SetNumberField(TEXT("x"), 114900);
	Pos->SetNumberField(TEXT("y"), 15900);
	const auto Target = MakeShared<FJsonObject>();
	Target->SetField(TEXT("objectId"), MakeShared<FJsonValueNumberString>(LexToString(TargetId)));
	Target->SetNumberField(TEXT("objectType"), 6);
	Target->SetObjectField(TEXT("objectPos"), Pos);
	FRokServerArmySnapshot Army;
	auto& Stack = Army.CitySoldiers.AddDefaulted_GetRef();
	Stack.Id = 10101;
	Stack.FamilyType = 1;
	Stack.Tier = 1;
	Stack.Count = 600;
	TArray<FRokServerHeroEntry> Heroes;
	Heroes.AddDefaulted_GetRef().HeroId = 101;
	Heroes.AddDefaulted_GetRef().HeroId = 102;
	FString Error;
	auto MakeOrder = [&](int32 Deputy, int32 Count)
	{
		return URokServerArmySubsystem::MakeGatherRequest(TargetId, Target, Army, Heroes, 101, Deputy,
			{{10101, Count}}, false, Error);
	};
	TestFalse(TEXT("Unavailable soldiers are rejected"), MakeOrder(0, 601).IsValid());
	TestFalse(TEXT("Deputy is unavailable before three stars"), MakeOrder(102, 500).IsValid());
	Target->SetNumberField(TEXT("collectRid"), 12);
	TestFalse(TEXT("An occupied resource cannot be gathered"), MakeOrder(0, 500).IsValid());
	Target->SetNumberField(TEXT("collectRid"), 0);
	const auto Order = MakeOrder(0, 500);
	if (!TestTrue(TEXT("Live target, owned commander and valid troops form a request"), Order.IsValid())) return false;

	auto* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>());
	Net->Secret.Init(1, 8);
	Net->MergedState = MakeShared<FJsonObject>();
	if (!TestTrue(TEXT("Schema loads"), Net->LoadSchema(Error))) return false;
	const auto* CreateArmy = Net->Schema.FindProtocol(TEXT("Role_CreateArmy"));
	const auto* Gate = Net->Schema.FindProtocol(TEXT("GateMessage"));
	const auto* ArmyPush = Net->Schema.FindProtocol(TEXT("Army_ArmyList"));
	if (!CreateArmy || !Gate || !ArmyPush) return false;

	auto Start = [&]()
	{
		TArray<uint8> Frame;
		if (!TestTrue(TEXT("Gather request encodes"), Net->BuildRequestFrame(CreateArmy->Name, Order, Frame, Error)))
			return false;
		Net->PendingArmyDispatch = URokGameNetSubsystem::FArmyDispatchWrite{Net->SprotoSession, Net->SprotoSession - 1,
			TargetId, 101, 0, 12.0, nullptr};
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
	auto Receive = [&](int64 Session, const TSharedPtr<FJsonObject>& Message)
	{
		FJsonObject Body;
		Body.SetArrayField(TEXT("content"), {MakeShared<FJsonValueObject>(Message)});
		TArray<uint8> Encoded, Frame;
		if (!TestTrue(TEXT("Gate response encodes"), EncodeResponse(Session, Gate->ResponseType, Body, Encoded))) return;
		RokSkynetCrypt::DesEncode(Net->Secret, Encoded.GetData(), Encoded.Num(), Frame);
		Frame.AddZeroed(5);
		Net->HandleFrame(Frame);
	};

	TArray<uint8> Packed;
	FRokSprotoRpc Client(Net->Schema);
	FRokSprotoRpc Server(Net->Schema);
	FRokSprotoMessage Decoded;
	if (!TestTrue(TEXT("Server decodes Role_CreateArmy"), Client.EncodeRequest(CreateArmy->Tag, Order.Get(),
		true, 1, Packed, Error) && Server.Dispatch(Packed.GetData(), Packed.Num(), Decoded, Error))) return false;
	const auto Arg = Decoded.Body->GetObjectField(TEXT("targetArg"));
	int64 Value = 0;
	TestTrue(TEXT("Gather command carries the precise live object ID"),
		RokSprotoJson::TryGetInt64(Arg, TEXT("targetObjectIndex"), Value) && Value == TargetId);
	TestTrue(TEXT("Gather command uses server coordinates"), RokSprotoJson::TryGetInt64(
		Arg->GetObjectField(TEXT("pos")), TEXT("x"), Value) && Value == 114900);
	TestTrue(TEXT("Gather command uses collection target type"),
		RokSprotoJson::TryGetInt64(Decoded.Body, TEXT("targetType"), Value) && Value == 4);
	TestTrue(TEXT("Gather command includes selected troop count"),
		RokSprotoJson::TryGetInt64(Decoded.Body->GetArrayField(TEXT("soldiers"))[0]->AsObject(), TEXT("num"), Value) && Value == 500);

	int32 Calls = 0, ConfirmedArmy = 0;
	FString Result;
	auto OnResult = [&](int32 Index, const FString& Failure) { ++Calls; ConfirmedArmy = Index; Result = Failure; };
	if (!Start()) return false;
	Net->PendingArmyDispatch->OnResult = OnResult;
	FJsonObject Ack;
	Ack.SetNumberField(TEXT("armyIndex"), 27);
	if (!EncodeResponse(Net->PendingArmyDispatch->InnerSession, CreateArmy->ResponseType, Ack, Packed)) return false;
	const auto AckMessage = MakeShared<FJsonObject>();
	AckMessage->SetStringField(TEXT("networkMessage"), FBase64::Encode(Packed));
	Receive(Net->PendingArmyDispatch->OuterSession, AckMessage);
	Net->CheckArmyDispatch(1);
	TestEqual(TEXT("ACK waits for the authoritative army list"), Calls, 0);
	const auto Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("armyIndex"), 27);
	Row->SetNumberField(TEXT("mainHeroId"), 101);
	Row->SetObjectField(TEXT("targetArg"), Arg);
	const auto Push = MakeShared<FJsonObject>();
	Push->SetArrayField(TEXT("armyInfo"), {MakeShared<FJsonValueObject>(Row)});
	Net->MergePush(*ArmyPush, Push);
	Net->CheckArmyDispatch(2);
	TestEqual(TEXT("Matching army push completes exactly once"), Calls, 1);
	TestEqual(TEXT("UI receives the server's army index"), ConfirmedArmy, 27);
	TestTrue(TEXT("Successful dispatch has no error"), Result.IsEmpty());

	if (!Start()) return false;
	Net->PendingArmyDispatch->OnResult = OnResult;
	const auto Rejection = MakeShared<FJsonObject>();
	Rejection->SetNumberField(TEXT("errorCode"), 1036);
	const auto ErrorMessage = MakeShared<FJsonObject>();
	ErrorMessage->SetObjectField(TEXT("error"), Rejection);
	Receive(Net->PendingArmyDispatch->OuterSession, ErrorMessage);
	TestTrue(TEXT("Server rejection exposes the missing research"), Calls == 2 && Result.Contains(TEXT("科技")));
	TestFalse(TEXT("Definitive rejection allows another request"), Net->bArmyDispatchNeedsReconnect);

	if (!Start()) return false;
	Net->PendingArmyDispatch->OnResult = OnResult;
	Net->CheckArmyDispatch(12);
	TestTrue(TEXT("Uncertain timeout prevents blind retries"), Calls == 3 && Net->bArmyDispatchNeedsReconnect);
	Net->CheckArmyDispatch(13);
	TestEqual(TEXT("Timeout also completes exactly once"), Calls, 3);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResourceRecallNetTest, "Rok.Net.Gather.ActiveAndRecall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokResourceRecallNetTest::RunTest(const FString&)
{
	const auto Point = MakeShared<FJsonObject>();
	Point->SetNumberField(TEXT("objectId"), 500);
	Point->SetNumberField(TEXT("objectType"), 6);
	Point->SetNumberField(TEXT("collectRid"), 12);
	Point->SetNumberField(TEXT("armyIndex"), 3);
	Point->SetNumberField(TEXT("resourceAmount"), 378000);
	Point->SetNumberField(TEXT("collectTime"), 100);
	Point->SetNumberField(TEXT("collectSpeed"), 350000);
	const auto Army = MakeShared<FJsonObject>();
	Army->SetNumberField(TEXT("armyIndex"), 3);
	Army->SetNumberField(TEXT("mainHeroId"), 2007);
	Army->SetNumberField(TEXT("status"), 128);
	Army->SetNumberField(TEXT("targetType"), 4);
	const auto Target = MakeShared<FJsonObject>();
	Target->SetNumberField(TEXT("targetObjectIndex"), 500);
	Army->SetObjectField(TEXT("targetArg"), Target);
	const auto Collect = MakeShared<FJsonObject>();
	Collect->SetNumberField(TEXT("endTime"), 1063);
	Army->SetObjectField(TEXT("collectResource"), Collect);

	const auto State = URokServerArmySubsystem::ProjectGatherPoint(Point, Army, 12, 138, 33680);
	TestTrue(TEXT("Own collecting army can return"), State.bOwned && State.bCanRecall);
	TestEqual(TEXT("Server clock and speed update gathered amount"), State.Gathered, int64(1330));
	TestEqual(TEXT("Remaining wood"), State.Remaining, int64(376670));
	TestEqual(TEXT("Source troop load within server timer precision"), State.Goal, int64(33680));
	const auto Other = URokServerArmySubsystem::ProjectGatherPoint(Point, Army, 21, 138);
	TestFalse(TEXT("Another player's point is never recallable"), Other.bOwned || Other.bCanRecall);
	const auto Segment = MakeShared<FJsonObject>();
	Segment->SetNumberField(TEXT("collectTime"), 20);
	Segment->SetNumberField(TEXT("collectSpeed"), 100000);
	Point->SetArrayField(TEXT("collectSpeeds"), {MakeShared<FJsonValueObject>(Segment)});
	TestEqual(TEXT("Old rate contributes before current rate"), URokServerArmySubsystem::ProjectGatherPoint(
		Point, Army, 12, 138).Gathered, int64(830));
	Target->SetNumberField(TEXT("targetObjectIndex"), 501);
	TestFalse(TEXT("An army assigned to a different point cannot be recalled here"),
		URokServerArmySubsystem::ProjectGatherPoint(Point, Army, 12, 138).bCanRecall);

	auto* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>());
	Net->Secret.Init(1, 8);
	Net->MergedState = MakeShared<FJsonObject>();
	FString Error;
	const auto Recall = URokGameNetSubsystem::MakeGatherRecallBody(3);
	TArray<uint8> Frame;
	if (!TestTrue(TEXT("Recall encodes into a gate frame"),
		Net->BuildRequestFrame(TEXT("Map_March"), Recall, Frame, Error))) return false;
	const auto* March = Net->Schema.FindProtocol(TEXT("Map_March"));
	if (!TestNotNull(TEXT("March protocol"), March)) return false;
	TestEqual(TEXT("Map_March has no inner acknowledgment"), March->ResponseType, INDEX_NONE);
	FRokSprotoRpc Client(Net->Schema);
	FRokSprotoRpc Server(Net->Schema);
	TArray<uint8> Packed;
	FRokSprotoMessage Decoded;
	if (!TestTrue(TEXT("Server decodes Map_March"), Client.EncodeRequest(March->Tag, Recall.Get(),
		true, 1, Packed, Error, false) && Server.Dispatch(Packed.GetData(), Packed.Num(), Decoded, Error))) return false;
	int64 Value = 0;
	TestTrue(TEXT("Recall carries the active army index"), RokSprotoJson::TryGetInt64(
		Decoded.Body, TEXT("armyIndex"), Value) && Value == 3);
	TestTrue(TEXT("Recall sets the server return target"), RokSprotoJson::TryGetInt64(
		Decoded.Body, TEXT("targetType"), Value) && Value == 5);

	const auto ArmyList = MakeShared<FJsonObject>();
	const auto Armies = MakeShared<FJsonObject>();
	Armies->SetObjectField(TEXT("3"), Army);
	ArmyList->SetObjectField(TEXT("armyInfo"), Armies);
	Net->MergedState->SetObjectField(TEXT("Army_ArmyList"), ArmyList);
	int32 Confirmations = 0;
	Net->PendingGatherRecall = URokGameNetSubsystem::FGatherRecall{Net->SprotoSession, 3, 12,
		[&Confirmations](const FString& Failure) { if (Failure.IsEmpty()) ++Confirmations; }};
	Net->CheckGatherRecall(1);
	TestTrue(TEXT("Gathering state cannot acknowledge recall"), Net->PendingGatherRecall.IsSet());
	Army->SetNumberField(TEXT("targetType"), 5);
	Net->CheckGatherRecall(2);
	TestEqual(TEXT("Authoritative return confirms once"), Confirmations, 1);
	TestFalse(TEXT("Confirmed recall releases pending request"), Net->PendingGatherRecall.IsSet());
	Net->PendingGatherRecall = URokGameNetSubsystem::FGatherRecall{Net->SprotoSession, 3, 12, nullptr};
	Army->SetNumberField(TEXT("targetType"), 4);
	Net->CheckGatherRecall(12);
	TestTrue(TEXT("A timeout requires reconnect before retry"), Net->bGatherRecallNeedsReconnect);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBarbarianAttackNetTest, "Rok.Net.Attack.BarbarianDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokBarbarianAttackNetTest::RunTest(const FString&)
{
	constexpr int64 TargetId = 9007199254741001LL;
	const auto Target = MakeShared<FJsonObject>();
	Target->SetField(TEXT("objectId"), MakeShared<FJsonValueNumberString>(LexToString(TargetId)));
	Target->SetNumberField(TEXT("objectType"), 2);
	const auto Position = MakeShared<FJsonObject>();
	Position->SetNumberField(TEXT("x"), 690100);
	Position->SetNumberField(TEXT("y"), 95200);
	Target->SetObjectField(TEXT("objectPos"), Position);
	FRokServerArmySnapshot Army;
	auto& Stack = Army.CitySoldiers.AddDefaulted_GetRef();
	Stack.Id = 10101;
	Stack.FamilyType = 1;
	Stack.Tier = 1;
	Stack.Count = 500;
	TArray<FRokServerHeroEntry> Heroes;
	Heroes.AddDefaulted_GetRef().HeroId = 4000;
	FString Error;
	const auto Create = URokServerArmySubsystem::MakeGatherRequest(TargetId, Target, Army, Heroes, 4000, 0,
		{{10101, 300}}, true, Error, true);
	const auto Retarget = URokServerArmySubsystem::MakeExistingAttackRequest(TargetId, Target, 27, false, Error);
	if (!TestTrue(TEXT("Both dispatch requests are constructed"), Create && Retarget)) return false;
	auto* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("Schema loads"), Net->LoadSchema(Error))) return false;
	Net->Secret.Init(1, 8);
	Net->MergedState = MakeShared<FJsonObject>();
	const auto* ArmyPush = Net->Schema.FindProtocol(TEXT("Army_ArmyList"));
	if (!ArmyPush) return false;

	const auto Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("armyIndex"), 27);
	Row->SetNumberField(TEXT("mainHeroId"), 4000);
	const auto Push = MakeShared<FJsonObject>();
	Push->SetArrayField(TEXT("armyInfo"), {MakeShared<FJsonValueObject>(Row)});
	for (bool bNew : {true, false})
	{
		const auto* Protocol = Net->Schema.FindProtocol(bNew ? TEXT("Role_CreateArmy") : TEXT("Map_March"));
		const auto& Body = bNew ? Create : Retarget;
		TArray<uint8> Frame;
		if (!TestTrue(TEXT("Attack request encodes"), Protocol && Net->BuildRequestFrame(Protocol->Name, Body, Frame, Error))) return false;
		TArray<uint8> Packed;
		FRokSprotoRpc Client(Net->Schema), Server(Net->Schema);
		FRokSprotoMessage Decoded;
		if (!TestTrue(TEXT("Server decodes the attack request"), Client.EncodeRequest(Protocol->Tag, Body.Get(),
			true, 1, Packed, Error) && Server.Dispatch(Packed.GetData(), Packed.Num(), Decoded, Error))) return false;
		const auto Arg = Decoded.Body->GetObjectField(TEXT("targetArg"));
		int64 Value = 0;
		bool bStation = false;
		TestTrue(TEXT("Attack target uses the exact live object ID and raw position"),
			RokSprotoJson::TryGetInt64(Arg, TEXT("targetObjectIndex"), Value) && Value == TargetId
			&& RokSprotoJson::TryGetInt64(Arg->GetObjectField(TEXT("pos")), TEXT("x"), Value) && Value == 690100);
		TestTrue(TEXT("Attack and the station choice are on the wire"),
			RokSprotoJson::TryGetInt64(Decoded.Body, TEXT("targetType"), Value) && Value == 1
			&& Decoded.Body->TryGetBoolField(TEXT("isSituStation"), bStation) && bStation == bNew);
		TestTrue(TEXT("The request uses the selected army or soldier count"), bNew
			? RokSprotoJson::TryGetInt64(Decoded.Body->GetArrayField(TEXT("soldiers"))[0]->AsObject(), TEXT("num"), Value) && Value == 300
			: RokSprotoJson::TryGetInt64(Decoded.Body, TEXT("armyIndex"), Value) && Value == 27);

		int32 Confirmed = 0;
		Net->PendingArmyDispatch = URokGameNetSubsystem::FArmyDispatchWrite{Net->SprotoSession,
			bNew ? Net->SprotoSession - 1 : 0, TargetId, 4000, bNew ? 0 : 27, 20,
			[&Confirmed](int32 Index, const FString& Failure) { if (Failure.IsEmpty()) Confirmed = Index; }, 1};
		if (bNew)
		{
			FRokSprotoMessage Ack;
			Ack.bHasSession = true;
			Ack.Session = Net->SprotoSession - 1;
			Ack.Body = MakeShared<FJsonObject>();
			Ack.Body->SetNumberField(TEXT("armyIndex"), 27);
			Net->AcknowledgeArmyDispatch(Ack);
		}
		Row->SetNumberField(TEXT("targetType"), 4);
		Row->SetObjectField(TEXT("targetArg"), Arg);
		Net->MergePush(*ArmyPush, Push);
		Net->CheckArmyDispatch(1);
		TestEqual(TEXT("A non-attack update cannot confirm the order"), Confirmed, 0);
		Row->SetNumberField(TEXT("targetType"), 1);
		Row->SetNumberField(TEXT("status"), 2);
		Net->MergePush(*ArmyPush, Push);
		Net->CheckArmyDispatch(2);
		TestEqual(TEXT("The matching attack push confirms the selected army"), Confirmed, 27);
		if (Net->PendingArmyDispatch.IsSet()) { Net->PendingArmyDispatch.Reset(); return false; }
	}
	FRokServerMarch March;
	March.TargetType = 1;
	March.Phase = ERokServerMarchPhase::Marching;
	March.bTimeKnown = true;
	March.StartTime = 100;
	March.ArrivalTime = 110;
	March.RemainingSeconds = 4;
	March.AttackPath = {{0, 0}, {2, 0}, {2, 8}};
	FVector2D Marker;
	TestTrue(TEXT("Map marker follows the server path and elapsed time"),
		March.TryGetAttackMarchPosition(Marker) && Marker.Equals(FVector2D(2, 4)));
	int32 NextPoint = INDEX_NONE;
	TestTrue(TEXT("The remaining route starts in the current segment and follows the server clock"),
		March.TryGetAttackMarchPosition(Marker, &NextPoint, 1.0)
		&& Marker.Equals(FVector2D(2, 5)) && NextPoint == 2);
	TestFalse(TEXT("Arrival clears the route even before the next snapshot"),
		March.TryGetAttackMarchPosition(Marker, &NextPoint, 4.0));
	March.Phase = ERokServerMarchPhase::Battling;
	TestFalse(TEXT("A stationary battle no longer displays a marching marker"), March.TryGetAttackMarchPosition(Marker));
	return true;
}
#endif
