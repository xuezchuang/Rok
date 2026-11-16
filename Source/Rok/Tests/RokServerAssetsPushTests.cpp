#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Serialization/JsonSerializer.h"
#include "../RokGameNetSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetPlayerAssetsPushTest,
	"Rok.Net.Push.PlayerAssetsSparseUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGameNetPlayerAssetsPushTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	URokGameNetSubsystem* Net = NewObject<URokGameNetSubsystem>(Instance);
	FString Error;
	if (!TestTrue(TEXT("Protocol schema loads"), Net->LoadSchema(Error))) return false;
	const FRokSprotoProtocol* Heroes = Net->Schema.FindProtocol(TEXT("Hero_HeroInfo"));
	const FRokSprotoProtocol* Items = Net->Schema.FindProtocol(TEXT("Item_ItemInfo"));
	if (!TestTrue(TEXT("Asset push protocols exist"), Heroes && Items)) return false;
	Net->MergedState = MakeShared<FJsonObject>();
	const auto Json = [](const TCHAR* Text)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Text)), Object);
		return Object;
	};
	Net->MergePush(*Heroes, Json(TEXT(R"({"heroInfo":[
		{"heroId":4000,"star":1,"level":1,"exp":0,"skills":[{"skillId":400001,"skillLevel":1}],
		 "talentTrees":[{"index":1,"name":"page one","talentTree":[1001]},{"index":2,"name":"page two","talentTree":[]}],"talentIndex":1,"weapon":0},
		{"heroId":2007,"star":1,"level":1,"exp":0,"skills":[{"skillId":200701,"skillLevel":1}]}]})")));
	Net->MergePush(*Heroes, Json(TEXT(R"({"heroInfo":[{"heroId":4000,"level":3,"exp":12,
		"skills":[{"skillId":400001,"skillLevel":2},{"skillId":400002,"skillLevel":1}],
		"talentTrees":[{"index":1,"talentTree":[]}],"weapon":21}]})")));
	const TSharedPtr<FJsonObject> Roster = Net->GetCollection(TEXT("Hero_HeroInfo"), TEXT("heroInfo"));
	if (!TestTrue(TEXT("Both heroes survive a partial push"), Roster.IsValid() && Roster->Values.Num() == 2)) return false;
	const TSharedPtr<FJsonObject>* Hero = nullptr;
	if (!TestTrue(TEXT("Updated hero exists"), Roster->TryGetObjectField(TEXT("4000"), Hero))) return false;
	const TSharedPtr<FJsonObject> UpdatedHero = *Hero;
	int64 Number = 0;
	TestTrue(TEXT("Changed hero level applies"), RokSprotoJson::TryGetInt64(UpdatedHero, TEXT("level"), Number) && Number == 3);
	TestTrue(TEXT("Omitted hero star stays known"), RokSprotoJson::TryGetInt64(UpdatedHero, TEXT("star"), Number) && Number == 1);
	TestTrue(TEXT("Equipment index applies"), RokSprotoJson::TryGetInt64(UpdatedHero, TEXT("weapon"), Number) && Number == 21);
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	TestTrue(TEXT("Skills use whole-list source update"), UpdatedHero->TryGetArrayField(TEXT("skills"), Rows) && Rows->Num() == 2);
	if (Rows && Rows->Num() == 2)
		TestTrue(TEXT("Skill level update retained"), RokSprotoJson::TryGetInt64((*Rows)[0]->AsObject(), TEXT("skillLevel"), Number) && Number == 2);
	Rows = nullptr;
	if (!TestTrue(TEXT("Talent pages merge by index"), UpdatedHero->TryGetArrayField(TEXT("talentTrees"), Rows) && Rows->Num() == 2)) return false;
	const TSharedPtr<FJsonObject> Page = (*Rows)[0]->AsObject();
	FString Name;
	TestTrue(TEXT("Page name omitted from delta survives"), Page->TryGetStringField(TEXT("name"), Name) && Name == TEXT("page one"));
	const TArray<TSharedPtr<FJsonValue>>* Talents = nullptr;
	TestTrue(TEXT("An empty unkeyed talent list clears learned talents"), Page->TryGetArrayField(TEXT("talentTree"), Talents) && Talents->IsEmpty());
	Net->MergePush(*Heroes, Json(TEXT(R"({"heroInfo":[{"heroId":4000,"skills":[],"talentTrees":[]}]})")));
	Rows = nullptr;
	TestTrue(TEXT("Empty skills replaces the skill list"), UpdatedHero->TryGetArrayField(TEXT("skills"), Rows) && Rows->IsEmpty());
	Rows = nullptr;
	TestTrue(TEXT("Empty keyed talent patch retains existing pages, matching Entity.updateEntity"),
		UpdatedHero->TryGetArrayField(TEXT("talentTrees"), Rows) && Rows->Num() == 2);

	Net->MergePush(*Items, Json(TEXT(R"({"itemInfo":[
		{"itemIndex":1,"itemId":201010010,"overlay":32,"heroId":0},
		{"itemIndex":2,"itemId":201010010,"overlay":10,"heroId":0},
		{"itemIndex":21,"itemId":301110010,"overlay":1,"heroId":0}]})")));
	Net->MergePush(*Items, Json(TEXT(R"({"itemInfo":[{"itemIndex":1,"overlay":0},{"itemIndex":21,"heroId":4000}]})")));
	const TSharedPtr<FJsonObject> Bag = Net->GetCollection(TEXT("Item_ItemInfo"), TEXT("itemInfo"));
	if (!TestTrue(TEXT("Item index identity keeps duplicate item IDs separate"), Bag.IsValid() && Bag->Values.Num() == 3)) return false;
	const TSharedPtr<FJsonObject>* Item = nullptr;
	if (!TestTrue(TEXT("Zero overlay tombstone survives the merge"), Bag->TryGetObjectField(TEXT("1"), Item))) return false;
	TestTrue(TEXT("Zero overlay applied"), RokSprotoJson::TryGetInt64(*Item, TEXT("overlay"), Number) && Number == 0);
	TestTrue(TEXT("Omitted config ID preserved for deleted stack"), RokSprotoJson::TryGetInt64(*Item, TEXT("itemId"), Number) && Number == 201010010);
	if (!TestTrue(TEXT("Independent duplicate stack survives"), Bag->TryGetObjectField(TEXT("2"), Item))) return false;
	TestTrue(TEXT("Untouched duplicate count stays ten"), RokSprotoJson::TryGetInt64(*Item, TEXT("overlay"), Number) && Number == 10);
	if (!TestTrue(TEXT("Equipped stack survives"), Bag->TryGetObjectField(TEXT("21"), Item))) return false;
	TestTrue(TEXT("Equip update keeps known item quantity"), RokSprotoJson::TryGetInt64(*Item, TEXT("overlay"), Number) && Number == 1);
	TestTrue(TEXT("Equip owner update applies"), RokSprotoJson::TryGetInt64(*Item, TEXT("heroId"), Number) && Number == 4000);
	const auto* Mail = Net->Schema.FindProtocol(TEXT("Email_EmailList"));
	if (!TestNotNull(TEXT("Mail list push protocol"), Mail)) return false;
	Net->MergePush(*Mail, Json(TEXT(R"({"emailInfo":[{"emailIndex":1,"emailId":100,"status":0,"content":"private"},
		{"emailIndex":2,"emailId":100,"status":0},{"emailIndex":3,"emailId":101,"status":1}]})")));
	TestEqual(TEXT("Counts only unread messages"), Net->GetUnreadMailCount(), 2);
	const auto Inbox = Net->GetCollection(TEXT("Email_EmailList"), TEXT("emailInfo"));
	const TSharedPtr<FJsonObject>* FirstEmail = nullptr;
	if (!TestTrue(TEXT("Mail metadata retained by index"), Inbox && Inbox->TryGetObjectField(TEXT("1"), FirstEmail))) return false;
	TestFalse(TEXT("Mail bodies are not retained for notification counts"), (*FirstEmail)->HasField(TEXT("content")));
	Net->MergePush(*Mail, Json(TEXT(R"({"emailInfo":[{"emailIndex":1,"status":1},{"emailIndex":2,"emailId":-1}]})")));
	TestEqual(TEXT("Sparse read and deleted updates clear their badges"), Net->GetUnreadMailCount(), 0);
	Net->Disconnect(TEXT("test reset"));
	return true;
}
#endif
