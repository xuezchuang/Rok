#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "../RokGameNetSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerResearchWriteTest, "Rok.Net.Research.WriteLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerResearchWriteTest::RunTest(const FString&)
{
    auto* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>());
    Net->MergedState = MakeShared<FJsonObject>();
    FString Error;
    if (!TestTrue(TEXT("Schema loads"), Net->LoadSchema(Error))) return false;
    const auto* RolePush = Net->Schema.FindProtocol(TEXT("Role_RoleInfo"));
    const auto* Start = Net->Schema.FindProtocol(TEXT("Technology_ResearchTechnology"));
    const auto* Award = Net->Schema.FindProtocol(TEXT("Technology_AwardTechnology"));
    const auto* Stop = Net->Schema.FindProtocol(TEXT("Technology_StopTechnology"));
    if (!TestNotNull(TEXT("Role push"), RolePush) || !TestNotNull(TEXT("Research"), Start) ||
        !TestNotNull(TEXT("Award"), Award) || !TestNotNull(TEXT("Stop"), Stop)) return false;
    for (const FRokSprotoProtocol* Protocol : {Start, Award, Stop})
    {
        FRokSprotoRpc Client(Net->Schema), Server(Net->Schema);
        auto Body = MakeShared<FJsonObject>();
        if (Protocol == Start)
        {
            Body->SetNumberField(TEXT("technologyType"), 101);
            Body->SetBoolField(TEXT("immediately"), false);
        }
        TArray<uint8> Packed;
        FRokSprotoMessage Decoded;
        if (!TestTrue(TEXT("Research request round-trips through the real sproto schema"),
            Client.EncodeRequest(Protocol->Tag, &Body.Get(), true, 5, Packed, Error,
                Protocol->ResponseType != INDEX_NONE) &&
            Server.Dispatch(Packed.GetData(), Packed.Num(), Decoded, Error))) return false;
        if (Protocol == Start)
        {
            int64 Type = 0;
            bool bInstant = true;
            TestTrue(TEXT("Start encodes selected technology"), RokSprotoJson::TryGetInt64(Decoded.Body, TEXT("technologyType"), Type) && Type == 101);
            TestTrue(TEXT("Start never uses instant gems"), Decoded.Body->TryGetBoolField(TEXT("immediately"), bInstant) && !bInstant);
        }
        else TestEqual(TEXT("Award and stop take no arguments"), Decoded.Body->Values.Num(), 0);
    }
    auto Role = MakeShared<FJsonObject>();
    auto Queue = MakeShared<FJsonObject>();
    auto Technology = MakeShared<FJsonObject>();
    Technology->SetNumberField(TEXT("technologyType"), 101);
    Technology->SetNumberField(TEXT("level"), 0);
    Role->SetArrayField(TEXT("technologies"), {MakeShared<FJsonValueObject>(Technology)});
    Role->SetObjectField(TEXT("technologyQueue"), Queue);
    auto Push = [&]() {
        auto Body = MakeShared<FJsonObject>(); Body->SetObjectField(TEXT("roleInfo"), Role);
        Net->MergePush(*RolePush, Body);
    };
    auto UpdateQueue = [&](int32 Type, int32 Finish) {
        Queue->SetNumberField(TEXT("technologyType"), Type);
        Queue->SetNumberField(TEXT("finishTime"), Finish);
        Push();
    };
    int32 Calls = 0;
    FString Result;
    auto Begin = [&](ERokResearchCommand Command) {
        URokGameNetSubsystem::FResearchWrite Write;
        Write.Command = Command; Write.StudyType = 101; Write.PreviousLevel = 0;
        Write.InnerSession = 11; Write.OuterSession = 12; Write.Deadline = 100;
        Write.OnResult = [&](const FString& Failure) { ++Calls; Result = Failure; };
        Net->PendingResearchWrite = MoveTemp(Write);
    };
    auto Ack = [&]() {
        FRokSprotoMessage Message;
        Message.bRequest = false; Message.bHasSession = true; Message.Session = 11;
        Message.Body = MakeShared<FJsonObject>(); Message.Body->SetBoolField(TEXT("result"), true);
        Net->AcknowledgeResearchWrite(Message);
    };
    UpdateQueue(0, -1);
    Begin(ERokResearchCommand::Start);
    Ack(); Net->CheckResearchWrite(1);
    TestEqual(TEXT("Start acknowledgement alone waits for push"), Calls, 0);
    UpdateQueue(102, 1000); Net->CheckResearchWrite(2);
    TestEqual(TEXT("Another study does not confirm"), Calls, 0);
    UpdateQueue(101, 1000); Net->CheckResearchWrite(3);
    TestTrue(TEXT("Matching queue confirms start"), Calls == 1 && Result.IsEmpty());
    Begin(ERokResearchCommand::Stop);
    UpdateQueue(0, -1); Net->CheckResearchWrite(4);
    TestEqual(TEXT("Stop waits for acknowledgement"), Calls, 1);
    Ack(); Net->CheckResearchWrite(5);
    TestTrue(TEXT("Stop confirmed by acknowledgement and idle queue"), Calls == 2 && Result.IsEmpty());
    UpdateQueue(101, -1);
    Begin(ERokResearchCommand::Award);
    UpdateQueue(0, -1); Net->CheckResearchWrite(6);
    TestEqual(TEXT("Idle queue does not prove award"), Calls, 2);
    Technology->SetNumberField(TEXT("level"), 1); Push(); Net->CheckResearchWrite(7);
    TestTrue(TEXT("Award requires level push"), Calls == 3 && Result.IsEmpty());
    Technology->SetNumberField(TEXT("level"), 0);
    Queue->Values.Reset();
    Net->MergedState = MakeShared<FJsonObject>();
    Push(); // A fresh account may encode its idle queue as an empty struct.
    Begin(ERokResearchCommand::Instant);
    Ack(); Net->CheckResearchWrite(8);
    TestEqual(TEXT("Instant acknowledgement waits for level"), Calls, 3);
    Technology->SetNumberField(TEXT("level"), 1); Push(); Net->CheckResearchWrite(9);
    TestTrue(TEXT("Instant level push confirms operation"), Calls == 4 && Result.IsEmpty());
    UpdateQueue(0, -1);
    Begin(ERokResearchCommand::Start);
    Net->CheckResearchWrite(100);
    TestTrue(TEXT("Unconfirmed write blocks retry"), Calls == 5 && Net->bResearchWriteNeedsReconnect && Result.Contains(TEXT("重新连接")));
    return true;
}
#endif
