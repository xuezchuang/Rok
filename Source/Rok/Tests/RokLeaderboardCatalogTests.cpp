#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokLeaderboardCatalog.h"
#include "../RokUnityModalWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLeaderboardCatalogTest,
	"Rok.Ui.PlayerProfile.LeaderboardCatalog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLeaderboardCatalogTest::RunTest(const FString&)
{
	TArray<FRokLeaderboardRow> Rows; FString Error;
	if (!TestTrue(TEXT("Source catalog loads"), RokLeaderboardCatalog::Load(Rows, Error))) return false;
	TestEqual(TEXT("Nine source bound slots"), Rows.Num(), 9);
	TestEqual(TEXT("City hall source identity"), Rows[5].Id, 206);
	TestEqual(TEXT("City hall source name"), Rows[5].Name, FString(TEXT("市政厅等级")));
	TestEqual(TEXT("Expedition source identity"), Rows[7].Id, 208);
	TestEqual(TEXT("Expedition source name"), Rows[7].Name, FString(TEXT("远征排行榜")));
	TestEqual(TEXT("Empty source entry retained by identity"), Rows[8].Id, 401);
	TestTrue(TEXT("No fabricated name for 401"), Rows[8].Name.IsEmpty());
	TestFalse(TEXT("No fabricated icon for 401"), Rows[8].HasIcon());
	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize();
	const TSharedRef<SWidget> Slate = Modal->TakeWidget();
	Modal->OpenPlayerProfile(); Modal->AdvanceWindowAnimationForTesting(1);
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("ProfileActionHelpButton")))->OnClicked.Broadcast();
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("ProfileRankingButton")))->OnClicked.Broadcast();
	Modal->AdvanceWindowAnimationForTesting(1);
	TestNull(TEXT("Ranking navigation clears action tip"), Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot")));
	auto* State = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerRankingServerBoundaryState")));
	if (!TestNotNull(TEXT("Preview gate is visible"), State)) return false;
	TestTrue(TEXT("Does not impersonate a server response"), State->GetText().ToString().Contains(TEXT("配置预览")));
	TestEqual(TEXT("Source label reaches UI"), CastChecked<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryLabel_5")))->GetText().ToString(), Rows[5].Name);
	TestTrue(TEXT("401 UI name stays empty"), CastChecked<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryLabel_8")))->GetText().IsEmpty());
	TestNull(TEXT("401 UI icon stays absent"), Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryIcon_8")));
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_5")))->OnClicked.Broadcast();
	TestEqual(TEXT("Detail title uses source identity"), CastChecked<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerRankingDetailTitle")))->GetText().ToString(), Rows[5].Name);
	TestNotNull(TEXT("Detail remains unsynchronized"), Modal->GetWidgetFromName(TEXT("PlayerRankingEmptyState")));
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("Type3CloseButton")))->OnClicked.Broadcast();
	TestFalse(TEXT("First close does not play window exit"), Modal->GetWindowAnimationForTesting().IsClosing());
	TestNotNull(TEXT("First close returns to overview"), Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_5")));
	TestNull(TEXT("First close does not leave for profile"), Modal->GetWidgetFromName(TEXT("ProfilePictureButton")));
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_8")))->OnClicked.Broadcast();
	TestTrue(TEXT("Empty source detail stays unnamed"), CastChecked<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerRankingDetailTitle")))->GetText().IsEmpty());
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("PlayerRankingBackButton")))->OnClicked.Broadcast();
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("Type3CloseButton")))->OnClicked.Broadcast();
	TestTrue(TEXT("Overview close starts window exit"), Modal->GetWindowAnimationForTesting().IsClosing());
	Modal->AdvanceWindowAnimationForTesting(1);
	TestNotNull(TEXT("Exit restores profile"), Modal->GetWidgetFromName(TEXT("ProfilePictureButton")));
	return true;
}
#endif
