#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "../RokGameNetSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokWorldMapCameraPawn.h"
#include "../RokWorldMapEntityActor.h"
#include "../RokWorldMapHudWidget.h"
#include "../RokWorldMapPlayerController.h"

namespace
{
	TSharedPtr<FJsonObject> Parse(const TCHAR* Text)
	{
		TSharedPtr<FJsonObject> Result;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Text)), Result);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetWorldMapPushTest,
	"Rok.Net.Push.WorldMapActiveAoi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGameNetWorldMapPushTest::RunTest(const FString&)
{
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>(GEngine));
	FString Error;
	if (!TestTrue(TEXT("Schema loads"), Net->LoadSchema(Error))) return false;
	const FRokSprotoProtocol* Info = Net->Schema.FindProtocol(TEXT("Map_ObjectInfo"));
	const FRokSprotoProtocol* Delete = Net->Schema.FindProtocol(TEXT("Map_ObjectDelete"));
	if (!TestTrue(TEXT("AOI protocols exist"), Info && Delete)) return false;
	Net->MergedState = MakeShared<FJsonObject>();
	Net->ResetWorldMapAoi();
	const uint64 Generation = Net->GetWorldMapAoiGeneration();
	Net->MergePush(*Info, Parse(TEXT(R"({"mapObjectInfo":{"objectId":101,"objectType":1,"cityName":"One","objectPos":{"x":100,"y":200}}})")));
	Net->MergePush(*Info, Parse(TEXT(R"({"mapObjectInfo":{"objectId":202,"objectType":3,"armyName":"Two","objectPos":{"x":300,"y":400}}})")));
	TSharedPtr<FJsonObject> Objects = Net->GetWorldMapObjects();
	if (!TestTrue(TEXT("Two independent objects"), Objects.IsValid() && Objects->Values.Num() == 2)) return false;
	const TSharedPtr<FJsonObject>* One = nullptr;
	const TSharedPtr<FJsonObject>* Two = nullptr;
	if (!TestTrue(TEXT("Both IDs addressable"), Objects->TryGetObjectField(TEXT("101"), One)
		&& Objects->TryGetObjectField(TEXT("202"), Two))) return false;
	Net->MergePush(*Info, Parse(TEXT(R"({"mapObjectInfo":{"objectId":101,"objectPos":{"x":150}}})")));
	int64 Value = 0;
	const TSharedPtr<FJsonObject>* Pos = nullptr;
	TestTrue(TEXT("Sparse push keeps city type"), RokSprotoJson::TryGetInt64(*One, TEXT("objectType"), Value) && Value == 1);
	TestTrue(TEXT("Sparse push keeps city name"), (*One)->HasField(TEXT("cityName")));
	TestTrue(TEXT("Nested position merges"), (*One)->TryGetObjectField(TEXT("objectPos"), Pos)
		&& RokSprotoJson::TryGetInt64(*Pos, TEXT("x"), Value) && Value == 150
		&& RokSprotoJson::TryGetInt64(*Pos, TEXT("y"), Value) && Value == 200);
	TestTrue(TEXT("Other object unchanged"), RokSprotoJson::TryGetInt64(*Two, TEXT("objectType"), Value) && Value == 3);
	TestEqual(TEXT("Revision counts each update"), Net->GetWorldMapAoiRevision(), uint64(3));
	TestTrue(TEXT("GetCollection exposes active AOI"), Net->GetCollection(TEXT("Map_ObjectInfo"), TEXT("mapObjectInfo")) == Objects);

	Net->MergePush(*Delete, Parse(TEXT(R"({"objectId":101})")));
	TestTrue(TEXT("Delete removes only its ID"), !Objects->HasField(TEXT("101")) && Objects->HasField(TEXT("202")));
	Net->MergePush(*Delete, Parse(TEXT(R"({"objectId":101})")));
	TestEqual(TEXT("Unknown duplicate delete does not change revision"), Net->GetWorldMapAoiRevision(), uint64(4));
	Net->MergePush(*Info, Parse(TEXT(R"({"mapObjectInfo":{"objectId":101,"objectType":4,"objectPos":{"x":9,"y":8}}})")));
	if (!TestTrue(TEXT("Re-added ID exists"), Objects->TryGetObjectField(TEXT("101"), One))) return false;
	TestTrue(TEXT("Re-add does not inherit deleted fields"), !(*One)->HasField(TEXT("cityName")));
	Net->MergePush(*Info, Parse(TEXT(R"({"mapObjectInfo":{}})")));
	TestTrue(TEXT("Empty push leaves collection intact"), Objects->Values.Num() == 2);
	TestEqual(TEXT("Revision ignores invalid empty push"), Net->GetWorldMapAoiRevision(), uint64(5));
	Net->ResetWorldMapAoi();
	TestTrue(TEXT("Session reset empties active AOI"), Net->GetWorldMapObjects()->Values.IsEmpty());
	TestEqual(TEXT("Session reset increments generation"), Net->GetWorldMapAoiGeneration(), Generation + 1);
	TestEqual(TEXT("Session reset zeroes revision"), Net->GetWorldMapAoiRevision(), uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetWorldMapReplayTest,
	"Rok.Net.Replay.WorldMapActiveAoi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGameNetWorldMapReplayTest::RunTest(const FString&)
{
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>(GEngine));
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RokNet"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = FPaths::Combine(Directory, TEXT("AoiReplayAutomation.json"));
	FString Error;
	const FString Old = TEXT(R"({"replay_server_time_ms":1700000000000,"state_by_protocol":{"Map_ObjectInfo":{"mapObjectInfo":{"objectId":999,"objectType":1}}}})");
	if (!TestTrue(TEXT("Write legacy fixture"), FFileHelper::SaveStringToFile(Old, *Path))) return false;
	const bool bOldLoaded = Net->StartSnapshotReplay(Path, Error);
	TestTrue(TEXT("Legacy replay loads"), bOldLoaded);
	if (bOldLoaded) TestTrue(TEXT("Last single push is not an AOI snapshot"), Net->GetWorldMapObjects()->Values.IsEmpty());
	const FString Current = TEXT(R"({"replay_server_time_ms":1700000000000,"state_by_protocol":{"Map_ObjectInfo":{"mapObjectInfo":{"101":{"objectId":101,"objectType":1},"202":{"objectId":202,"objectType":3}}}}})");
	TestTrue(TEXT("Write current fixture"), FFileHelper::SaveStringToFile(Current, *Path));
	const bool bCurrentLoaded = Net->StartSnapshotReplay(Path, Error);
	TestTrue(TEXT("Current replay loads"), bCurrentLoaded);
	if (bCurrentLoaded) TestEqual(TEXT("Current keyed AOI restored"), Net->GetWorldMapObjects()->Values.Num(), 2);
	Net->Disconnect(TEXT("test reset"));
	TestTrue(TEXT("Replay disconnect clears AOI"), Net->GetWorldMapObjects()->Values.IsEmpty());
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetViewportRequestTest,
	"Rok.Net.Request.ViewportAoiCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGameNetViewportRequestTest::RunTest(const FString&)
{
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>(GEngine));
	TSharedPtr<FJsonObject> Body;
	TestTrue(TEXT("Grid center is valid"), Net->MakeViewportAoiBody(123, 456, true, Body));
	const TSharedPtr<FJsonObject>* Pos = nullptr;
	int64 Value = 0;
	TestTrue(TEXT("X is scaled to server position units"), Body->TryGetObjectField(TEXT("posInfo"), Pos)
		&& RokSprotoJson::TryGetInt64(*Pos, TEXT("x"), Value) && Value == 12300);
	TestTrue(TEXT("Y is scaled to server position units"), Pos && RokSprotoJson::TryGetInt64(*Pos, TEXT("y"), Value) && Value == 45600);
	bool bPreview = false;
	TestTrue(TEXT("Strategic preview flag retained"), Body->TryGetBoolField(TEXT("isPreview"), bPreview) && bPreview);
	TestFalse(TEXT("Negative center rejected"), Net->MakeViewportAoiBody(-1, 2, false, Body));
	TestFalse(TEXT("Out-of-bound center rejected"), Net->MakeViewportAoiBody(1000001, 2, false, Body));
	TestFalse(TEXT("Offline session cannot request AOI"), Net->RequestViewportAoi(123, 456, false));
	FString Error;
	if (!TestTrue(TEXT("Schema loads for request framing"), Net->LoadSchema(Error))) return false;
	Net->Secret.SetNumZeroed(8);
	TArray<uint8> Frame;
	if (!TestTrue(TEXT("Viewport body builds"), Net->MakeViewportAoiBody(123, 456, false, Body))) return false;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!TestTrue(TEXT("No-response Map_Move frame builds"),
			Net->BuildRequestFrame(TEXT("Map_Move"), Body, Frame, Error))) return false;
	}
	TestEqual(TEXT("No-response moves retain only optional outer ACK sessions"), Net->Rpc->NumPendingResponses(), 3);
	TestEqual(TEXT("Outer tags remain available for error or AOI push replies"), Net->PendingInnerByOuter.Num(), 3);
	TSharedPtr<FJsonObject> Heart = MakeShared<FJsonObject>();
	Heart->SetNumberField(TEXT("clientTime"), 1);
	Heart->SetNumberField(TEXT("serverTime"), 2);
	TestTrue(TEXT("Heartbeat frame builds"), Net->BuildRequestFrame(TEXT("Role_Heart"), Heart, Frame, Error));
	TestEqual(TEXT("Response-bearing heartbeat retains both RPC sessions"), Net->Rpc->NumPendingResponses(), 5);
	TestEqual(TEXT("Heartbeat outer tag retained"), Net->PendingInnerByOuter.Num(), 4);
	Net->ExpireStaleRequests(FPlatformTime::Seconds() + 31.0);
	TestEqual(TEXT("Optional outer ACKs expire from real RPC bookkeeping"), Net->Rpc->NumPendingResponses(), 0);
	TestEqual(TEXT("Expired outer tags are removed together"), Net->PendingInnerByOuter.Num(), 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldMapSearchProtocolTest,
	"Rok.Net.Request.WorldMapSearch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokWorldMapSearchProtocolTest::RunTest(const FString&)
{
	auto* Net = NewObject<URokGameNetSubsystem>(NewObject<UGameInstance>(GEngine));
	FString Error;
	if (!TestTrue(TEXT("Search schema loads"), Net->LoadSchema(Error))) return false;
	Net->Secret.SetNumZeroed(8);
	TArray<uint8> Frame;
	const auto Enemy = Net->MakeWorldSearchBody(0, 2);
	const auto Resource = Net->MakeWorldSearchBody(2, 3);
	int64 Level = 0;
	TestTrue(TEXT("Enemy request carries the chosen level"),
		RokSprotoJson::TryGetInt64(Enemy, TEXT("level"), Level) && Level == 2
		&& Net->BuildRequestFrame(TEXT("Map_SearchBarbarian"), Enemy, Frame, Error));
	TestTrue(TEXT("Resource request uses its separate wire fields"),
		RokSprotoJson::TryGetInt64(Resource, TEXT("resourceLevel"), Level) && Level == 3
		&& Net->BuildRequestFrame(TEXT("Map_SearchResource"), Resource, Frame, Error));
	TestFalse(TEXT("Impossible levels do not produce a request"), Net->MakeWorldSearchBody(4, 7).IsValid());

	const auto Body = Parse(TEXT(R"({"level":2,"barbarians":[
		{"objectId":110,"pos":{"x":15000,"y":13000}},
		{"objectId":120,"pos":{"x":10125,"y":10000}},
		{"objectId":130,"pos":{"x":-1,"y":10}}
	]})"));
	Net->MergedState = Parse(TEXT(R"({"Role_RoleInfo":{"roleInfo":{"pos":{"x":10000,"y":10000}}}})"));
	int32 Calls = 0;
	FRokWorldMapSearchHit Hit;
	Net->PendingWorldSearch = URokGameNetSubsystem::FWorldSearch{0, 2, Net->SprotoSession,
		Net->SprotoSession - 1, 0.0, [this, &Calls, &Hit](FRokWorldMapSearchHit Result, const FString& Failure)
		{
			++Calls;
			Hit = Result;
			TestTrue(TEXT("A valid reply succeeds"), Failure.IsEmpty());
		}};
	FRokSprotoMessage Reply;
	Reply.bRequest = false;
	Reply.bHasSession = true;
	Reply.Session = Net->SprotoSession - 3;
	Reply.Body = Body;
	Net->AcknowledgeWorldSearch(Reply);
	TestEqual(TEXT("Old response is ignored"), Calls, 0);
	Reply.Session = Net->SprotoSession - 1;
	Net->AcknowledgeWorldSearch(Reply);
	TestEqual(TEXT("Exactly one response is delivered"), Calls, 1);
	TestEqual(TEXT("Search picks the closest valid server ID"), Hit.ObjectId, int64(120));
	TestTrue(TEXT("Position is converted without rounding off a fractional coordinate"),
		Hit.UnityPosition.Equals(FVector2D(101.25, 100.0)));
	TestEqual(TEXT("Repeat search cycles to the next target"), Net->TakeWorldSearchHit().ObjectId, int64(110));

	TArray<FRokWorldMapSearchHit> Hits;
	TestTrue(TEXT("An omitted empty list is a valid no-match response"),
		Net->ReadWorldSearchHits(Parse(TEXT(R"({"level":2})")), 0, 2, FVector2D::ZeroVector, Hits) && Hits.IsEmpty());
	TestFalse(TEXT("A different enemy level is not accepted"),
		Net->ReadWorldSearchHits(Body, 0, 3, FVector2D::ZeroVector, Hits));
	return true;
}
namespace
{
class FCheckWorldSearch final : public IAutomationLatentCommand
{
public:
	explicit FCheckWorldSearch(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("World search replay did not find its target. Load the world map with a level-2 search replay."));
			return true;
		}
		for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
		{
			if (!It->IsInViewport()) continue;
			auto* Controller = Cast<ARokWorldMapPlayerController>(It->GetOwningPlayer());
			const auto* Camera = Controller ? Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()) : nullptr;
			auto* Screen = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("WorldMapUnityFeatureScreenWidget")));
			const auto* GameInstance = It->GetGameInstance();
			const auto* Net = GameInstance ? GameInstance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
			if (!Camera || !Screen || !Net || !Net->IsSnapshotReplay()) continue;

			if (bSubmitted)
			{
				const auto* Target = Cast<ARokWorldMapEntityActor>(Controller->GetSelectedWorldMapActor());
				if (!Target || Screen->IsResourceSearchOpen()) return false;
				Test.TestEqual(TEXT("Selected server object ID"), Target->GetServerObjectId(), FString(TEXT("987650002")));
				Test.TestTrue(TEXT("Camera uses returned world coordinates"),
					Camera->GetViewCenterUnity().Equals(FVector2D(6901, 952), 0.01));
				Test.TestNotNull(TEXT("The focused target receives the Unity guide marker"),
					It->GetWidgetFromName(TEXT("WorldSearchResultGuide")));
				return true;
			}
			if (!Screen->IsResourceSearchOpen()) continue;
			auto Click = [this, Screen](const TCHAR* Name)
			{
				auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
				if (!Test.TestNotNull(Name, Button)) return false;
				Button->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				return true;
			};
			auto Text = [Screen](const TCHAR* Name)
			{
				const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
				return Label ? Label->GetText().ToString() : FString();
			};
			Test.TestNotNull(TEXT("Task entry uses the source sword"), Screen->GetWidgetFromName(TEXT("ResourceSearchGuide")));
			Test.TestEqual(TEXT("Task level carried through"), Text(TEXT("ResourceSearchLevelValue")), FString(TEXT("等级:2")));
			Test.TestTrue(TEXT("Locked attack level is explained"), Text(TEXT("ResourceSearchMonsterTip")).Contains(TEXT("等级1")));
			if (!Click(TEXT("ResourceSearchLevelMinus"))) return true;
			Test.TestTrue(TEXT("Lowering the level removes the warning"), Text(TEXT("ResourceSearchMonsterTip")).IsEmpty());
			if (!Click(TEXT("ResourceSearchLevelPlus")) || !Click(TEXT("ResourceSearchType_3"))) return true;
			Test.TestTrue(TEXT("Locked resource explains the required research"),
				Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("采矿")));
			if (!Click(TEXT("ResourceSearchType_1"))) return true;
			const FVector2D Before = Camera->GetViewCenterUnity();
			if (!Click(TEXT("ResourceSearchSubmit"))) return true;
			Test.TestTrue(TEXT("No matches keep the popover and camera in place"),
				Screen->IsResourceSearchOpen() && Camera->GetViewCenterUnity().Equals(Before)
				&& Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("未找到")));
			if (!Click(TEXT("ResourceSearchType_0")) || !Click(TEXT("ResourceSearchSubmit"))) return true;
			bSubmitted = true;
			return false;
		}
		return false;
	}

private:
	FAutomationTestBase& Test;
	bool bSubmitted = false;
	double Deadline = FPlatformTime::Seconds() + 45.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldSearchRenderedFlowTest, "Rok.Ui.WorldSearch.ServerReplay",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokWorldSearchRenderedFlowTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckWorldSearch(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldDamageRenderedTest, "Rok.Ui.WorldHud.DamageNumbers",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokWorldDamageRenderedTest::RunTest(const FString&)
{
	class FCheckDamage final : public IAutomationLatentCommand
	{
	public:
		FCheckDamage(FAutomationTestBase& InTest, TFunction<bool(URokGameNetSubsystem&)> InPush)
			: Test(InTest), Push(MoveTemp(InPush)) {}

		bool Update() override
		{
			if (FPlatformTime::Seconds() > Deadline)
			{
				Test.AddError(TEXT("The world damage number replay did not finish."));
				return true;
			}
			if (Hud.IsValid())
			{
				const auto* Army = Cast<UTextBlock>(RokUiFind::Find(Hud, TEXT("WorldAttackDamage_0")));
				const auto* Target = Cast<UTextBlock>(RokUiFind::Find(Hud, TEXT("WorldAttackDamage_1")));
				if (!Army || !Target) return false;
				if (bCaptured)
				{
					return !Army->IsVisible() && !Target->IsVisible() && IFileManager::Get().FileSize(*Screenshot) > 0;
				}
				if (!Army->IsVisible() || !Target->IsVisible()) return false;
				Test.TestTrue(TEXT("The army damage uses the server event amount"), Army->GetText().ToString().Contains(FText::AsNumber(1234).ToString()));
				Test.TestTrue(TEXT("The target damage uses the server event amount"), Target->GetText().ToString().Contains(FText::AsNumber(5678).ToString()));
				Test.TestTrue(TEXT("Fresh damage is painted before fading"), Army->GetRenderOpacity() > 0 && Target->GetRenderOpacity() > 0);
				const FString CaptureDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Tests"));
				IFileManager::Get().MakeDirectory(*CaptureDir, true);
				Screenshot = CaptureDir / FString::Printf(TEXT("hud_world_damage_%s.png"),
					URokUiSettings::IsLegacyForced() ? TEXT("legacy") : TEXT("wbp"));
				IFileManager::Get().Delete(*Screenshot);
				FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
				bCaptured = true;
				return false;
			}
			for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport() || !It->GetGameInstance()) continue;
				auto* Net = It->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
				const auto* Battle = RokUiFind::Find(*It, TEXT("WorldAttackMarchMarker_0"));
				if (!Net || !Net->IsSnapshotReplay() || !Battle || !Battle->IsVisible()) continue;
				if (!Test.TestTrue(TEXT("The battle damage protocol is available in replay"), Push(*Net))) return true;
				Hud = *It;
				return false;
			}
			return false;
		}

	private:
		FAutomationTestBase& Test;
		TFunction<bool(URokGameNetSubsystem&)> Push;
		TWeakObjectPtr<URokWorldMapHudWidget> Hud;
		FString Screenshot;
		double Deadline = FPlatformTime::Seconds() + 18.0;
		bool bCaptured = false;
	};
	const auto Push = [this](URokGameNetSubsystem& Net)
	{
		FString Error;
		if (!Net.LoadSchema(Error))
		{
			AddError(Error);
			return false;
		}
		const FRokSprotoProtocol* Protocol = Net.Schema.FindProtocol(TEXT("Battle_BattleDamageInfo"));
		if (!Protocol) return false;
		Net.MergePush(*Protocol, Parse(TEXT(R"({"battleDamageInfo":[{"objectIndex":987650200,"damage":1234},{"objectIndex":987650001,"damage":5678}]})")));
		return true;
	};
	ADD_LATENT_AUTOMATION_COMMAND(FCheckDamage(*this, Push));
	return true;
}
#endif
