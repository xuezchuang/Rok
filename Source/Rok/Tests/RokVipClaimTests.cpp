#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "../RokGameNetSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokVipClaimConfirmationTest, "Rok.Net.Vip.ClaimConfirmation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokVipClaimConfirmationTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(GameInstance);
	FString Error;
	if (!TestTrue(TEXT("Game protocol schema loads"), Net->LoadSchema(Error))) return false;
	const FRokSprotoProtocol* RoleProtocol = Net->Schema.FindProtocol(TEXT("Role_RoleInfo"));
	if (!TestNotNull(TEXT("Role push is available"), RoleProtocol)) return false;

	Net->MergedState = MakeShared<FJsonObject>();
	Net->SetState(ERokGameNetState::RoleReady);
	Net->Secret.Init(0x42, 8);
	for (const TCHAR* Name : {TEXT("Role_GetVipPoint"), TEXT("Role_GetVipFreeBox")})
	{
		TArray<uint8> Frame;
		TestTrue(Name, Net->BuildRequestFrame(Name, nullptr, Frame, Error) && !Frame.IsEmpty());
	}

	auto PushFlag = [Net, RoleProtocol](const TCHAR* Field, bool bValue)
	{
		const auto Role = MakeShared<FJsonObject>();
		Role->SetBoolField(Field, bValue);
		const auto Push = MakeShared<FJsonObject>();
		Push->SetObjectField(TEXT("roleInfo"), Role);
		Net->MergePush(*RoleProtocol, Push);
	};

	int32 Completed = 0;
	FString Result;
	auto OnResult = [&Completed, &Result](const FString& Failure)
	{
		++Completed;
		Result = Failure;
	};
	Net->PendingVipClaim = URokGameNetSubsystem::FVipClaim{false, 7, 30.0, OnResult};
	PushFlag(TEXT("vipExpFlag"), false);
	PushFlag(TEXT("vipFreeBox"), true);
	Net->CheckVipClaim(10.0);
	TestTrue(TEXT("Unrelated claim flag cannot confirm daily points"), Net->IsVipClaimPending());
	TestEqual(TEXT("No completion before server confirmation"), Completed, 0);
	PushFlag(TEXT("vipExpFlag"), true);
	Net->CheckVipClaim(11.0);
	TestFalse(TEXT("The daily flag clears its pending request"), Net->IsVipClaimPending());
	TestEqual(TEXT("The daily flag completes the request once"), Completed, 1);
	TestTrue(TEXT("The confirmed request succeeds"), Result.IsEmpty());

	PushFlag(TEXT("vipFreeBox"), false);
	Net->PendingVipClaim = URokGameNetSubsystem::FVipClaim{true, 8, 30.0, OnResult};
	Net->CheckVipClaim(30.0);
	TestTrue(TEXT("Timeout requires resynchronization before another claim"), Net->bVipClaimNeedsReconnect);
	TestFalse(TEXT("Timed-out claim is no longer pending"), Net->IsVipClaimPending());
	TestEqual(TEXT("Timeout completes the callback once"), Completed, 2);
	TestFalse(TEXT("Timeout is reported as uncertain, not as success"), Result.IsEmpty());
	bool bFreeBoxClaimed = true;
	TestTrue(TEXT("Timeout never flips the authoritative flag"),
		Net->GetRoleInfo()->TryGetBoolField(TEXT("vipFreeBox"), bFreeBoxClaimed) && !bFreeBoxClaimed);

	Net->bSnapshotReplay = true;
	TestFalse(TEXT("Snapshot replay cannot send a claim"), Net->RequestVipClaim(true, OnResult, Error));
	TestTrue(TEXT("Replay explains why the request is blocked"), Error.Contains(TEXT("快照回放")));
	return true;
}

#endif
