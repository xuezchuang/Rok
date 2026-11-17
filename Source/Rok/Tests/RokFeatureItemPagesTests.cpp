#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "../RokAllianceFlagLocalPreviewData.h"
#include "../RokUiWidgets.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

namespace
{
bool ClickFeatureButton(URokUnityFeatureScreenWidget& Screen, FName Name)
{
	UButton* Button = Cast<UButton>(Screen.GetWidgetFromName(Name));
	if (!Button)
	{
		return false;
	}
	Button->OnClicked.Broadcast();
	Screen.FlushPendingActionForTesting();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMapMarkerItemPageTest, "Rok.Ui.FeatureItems.MapMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokMapMarkerItemPageTest::RunTest(const FString&)
{
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	if (!TestTrue(TEXT("Map marker host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenMapMarkers();
	Screen->AdvanceWindowAnimationForTesting(1.0f);

	TestNotNull(TEXT("All markers includes the first row"), Screen->GetWidgetFromName(TEXT("MapMarkerName_0")));
	TestNotNull(TEXT("All markers includes the last row"), Screen->GetWidgetFromName(TEXT("MapMarkerName_3")));
	if (!TestTrue(TEXT("Third row remains clickable"), ClickFeatureButton(*Screen, TEXT("MapMarkerRowHit_2"))))
	{
		return false;
	}
	const UBorder* Selection = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("MapMarkerRowShade_2")));
	TestTrue(TEXT("Third row uses the selected style"),
		Selection && Selection->GetBrushColor().Equals(FLinearColor(0.18f, 0.28f, 0.3f, 1.0f), 0.001f));
	if (!TestTrue(TEXT("Alliance tab remains clickable"), ClickFeatureButton(*Screen, TEXT("MapMarkerPage_4"))))
	{
		return false;
	}
	TestNull(TEXT("Alliance filter removes other rows"), Screen->GetWidgetFromName(TEXT("MapMarkerName_0")));
	const UTextBlock* AllianceMarker = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("MapMarkerName_3")));
	if (TestNotNull(TEXT("Alliance filter retains the alliance row"), AllianceMarker))
	{
		TestEqual(TEXT("The row retains its source label"), AllianceMarker->GetText().ToString(), FString(TEXT("联盟要塞")));
		const UWidget* ModeCanvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
		TestTrue(TEXT("The filtered marker reflows to the first row"),
			RokUiFind::CanvasPosition(AllianceMarker, ModeCanvas).Equals(FVector2D(272, 209), 0.001f));
	}
	TestTrue(TEXT("Add retains its boundary action"), ClickFeatureButton(*Screen, TEXT("MapMarkerAdd")));
	const UTextBlock* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
	TestTrue(TEXT("Offline mode still explains the storage requirement"),
		Feedback && Feedback->GetText().ToString().Contains(TEXT("服务器同步")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGemShopItemPageTest, "Rok.Ui.FeatureItems.GemShop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGemShopItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokGemShopItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Gem shop host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenGemShop();
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const UTextBlock* Amount = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("GemShopPackageAmount_0")));
	if (TestNotNull(TEXT("The catalogue opens its first card"), Amount))
	{
		TestTrue(TEXT("The first card is populated from RechargeGemMall"), Amount->GetText().ToString().Contains(TEXT("200")));
	}
	TestNotNull(TEXT("The first card exposes its purchase label"), Screen->GetWidgetFromName(TEXT("GemShopBuy_0Label")));
	TestTrue(TEXT("The first card retains its boundary action"), ClickFeatureButton(*Screen, TEXT("GemShopBuy_0")));
	const UTextBlock* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
	TestTrue(TEXT("The purchase boundary explains server order verification"),
		Feedback && Feedback->GetText().ToString().Contains(TEXT("服务器确认")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokChargeItemPageTest, "Rok.Ui.FeatureItems.Charge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokChargeItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokChargeItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Charge host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenCharge(107);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestNotNull(TEXT("Gem tab opens the source catalogue"), Screen->GetWidgetFromName(TEXT("ChargeGemPackageAmount_0")));
	TestNull(TEXT("Gem tab omits the activity boundary"), Screen->GetWidgetFromName(TEXT("ChargeSelectedPageServiceBoundary")));
	if (!TestTrue(TEXT("The activity tab accepts the original click"), ClickFeatureButton(*Screen, TEXT("ChargePage_101"))))
	{
		return false;
	}
	TestNull(TEXT("Activity tab omits gem cards"), Screen->GetWidgetFromName(TEXT("ChargeGemPackageAmount_0")));
	const UTextBlock* ActivityText = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ChargeSelectedPageServiceBoundary")));
	TestTrue(TEXT("Activity tab shows the server state boundary"),
		ActivityText && ActivityText->GetText().ToString().Contains(TEXT("服务器状态")));
	const UButton* ActivityAction = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ChargePageBoundaryAction")));
	TestTrue(TEXT("The activity button is visible and hit-testable"),
		ActivityAction && ActivityAction->GetVisibility() == ESlateVisibility::Visible);
	TestTrue(TEXT("The activity button retains its click route"), ClickFeatureButton(*Screen, TEXT("ChargePageBoundaryAction")));
	TestTrue(TEXT("Returning to the gem tab uses the same navigation"), ClickFeatureButton(*Screen, TEXT("ChargePage_107")));
	TestNotNull(TEXT("Returning to the gem tab repopulates the first card"), Screen->GetWidgetFromName(TEXT("ChargeGemPackageAmount_0")));
	TestNull(TEXT("Returning removes the activity boundary"), Screen->GetWidgetFromName(TEXT("ChargeSelectedPageServiceBoundary")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceOfficeItemPageTest, "Rok.Ui.FeatureItems.AllianceOffice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokAllianceOfficeItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokAllianceOfficeItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Alliance host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenAlliance(false);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestNotNull(TEXT("Welcome retains the create entry"), Screen->GetWidgetFromName(TEXT("AllianceWelcomeCreate")));
	TestNotNull(TEXT("Welcome retains the technology entry"), Screen->GetWidgetFromName(TEXT("AllianceTechnologyReferenceOpen")));
	if (!TestTrue(TEXT("Welcome opens the office page via its original action"),
		ClickFeatureButton(*Screen, TEXT("AllianceOfficeReferenceOpen"))))
	{
		return false;
	}
	TestNull(TEXT("Loaded catalogue shows no error"), Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceUnavailable")));
	const UTextBlock* Name = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceName_1001")));
	TestTrue(TEXT("First row retains its catalogue id"), Name && Name->GetText().ToString().StartsWith(TEXT("#1001 · ")));
	TestNotNull(TEXT("Fourth row retains its distinct art socket"), Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceIcon_4001")));
	const UBorder* Initial = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceRow_1001")));
	if (!TestNotNull(TEXT("Initial selected row exists"), Initial))
	{
		return false;
	}
	const FLinearColor SelectionColor = Initial->GetBrushColor();
	TestTrue(TEXT("Third row selection stays on the reference page"),
		ClickFeatureButton(*Screen, TEXT("AllianceOfficeReferenceHit_3001")));
	const UBorder* Selected = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceRow_3001")));
	TestTrue(TEXT("Third row receives the selected style"), Selected && Selected->GetBrushColor().Equals(SelectionColor));
	TestTrue(TEXT("Office returns to the original welcome page"), ClickFeatureButton(*Screen, TEXT("AllianceOfficeReferenceBack")));
	TestNull(TEXT("Office rows do not survive the return"), Screen->GetWidgetFromName(TEXT("AllianceOfficeReferenceName_1001")));
	TestTrue(TEXT("The welcome page still opens the join screen"), ClickFeatureButton(*Screen, TEXT("AllianceWelcomeJoin")));
	TestNotNull(TEXT("Join continues to expose its actual search input"), Screen->GetWidgetFromName(TEXT("AllianceSearchInput")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTaskDailyCatalogItemPageTest, "Rok.Ui.FeatureItems.TaskDailyCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTaskDailyCatalogItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTaskDailyCatalogItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestTrue(TEXT("The normal gameplay catalogue loads"), Catalog && Catalog->LoadCatalog()))
	{
		return false;
	}
	const TArray<int32> DailyIds = Catalog->GetDailyTaskIds();
	if (!TestTrue(TEXT("The catalogue provides daily tasks"), !DailyIds.IsEmpty()))
	{
		return false;
	}
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Task host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenTasks(false, 1);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	if (!TestTrue(TEXT("Main tasks can open the daily tab"), ClickFeatureButton(*Screen, TEXT("TaskPage_2"))))
	{
		return false;
	}
	const UScrollBox* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskDailyCatalogScroll")));
	if (!TestNotNull(TEXT("Offline daily opens the scrollable catalogue"), Scroll))
	{
		return false;
	}
	TestEqual(TEXT("Every source row has one scroll child alongside the header"), Scroll->GetChildrenCount(), DailyIds.Num() + 1);
	for (int32 TaskId : {DailyIds[0], DailyIds.Last()})
	{
		const FName TitleName(*FString::Printf(TEXT("TaskDailyCatalogTitle_%d"), TaskId));
		const auto* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TitleName));
		TestTrue(TEXT("Boundary rows carry their own source id"),
			Title && Title->GetText().ToString().StartsWith(FString::Printf(TEXT("#%d · "), TaskId)));
	}
	TestNull(TEXT("Loaded offline catalogue has no unavailable message"), Screen->GetWidgetFromName(TEXT("TaskPageEmpty")));
	TestTrue(TEXT("The original tab button returns to main tasks"), ClickFeatureButton(*Screen, TEXT("TaskPage_1")));
	TestNotNull(TEXT("The main task scroll is restored"), Screen->GetWidgetFromName(TEXT("TaskMainScroll")));
	TestNull(TEXT("Daily rows are retired after returning"), Screen->GetWidgetFromName(TEXT("TaskDailyCatalogScroll")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTaskMainLocalItemPageTest, "Rok.Ui.FeatureItems.TaskMainLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTaskMainLocalItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTaskMainLocalItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = CreateWidget<URokUnityFeatureScreenWidget>(Instance);
	if (!TestNotNull(TEXT("The task screen creates a normal widget"), Screen)) return false;
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();

	Screen->OpenTasks(false, 1);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const auto* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	const auto* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskMainScroll")));
	if (!TestTrue(TEXT("The main task opens inside the mode"), RokUiFind::IsInside(Scroll, Mode))) return false;
	TestEqual(TEXT("The expected main presentation is mounted"), Scroll->GetParent() == Mode, URokUiSettings::IsLegacyForced());
	TestEqual(TEXT("The local active task ID stays source-owned"), Screen->GetVisibleTaskIdForAudit(0), 200900);
	TestNotNull(TEXT("Side account state displays the offline boundary"), Screen->GetWidgetFromName(TEXT("TaskSideNotSynced")));
	TestNull(TEXT("The production entry has no animation fixture"), Screen->GetWidgetFromName(TEXT("TaskCompletedAnimationPreview")));
	TestTrue(TEXT("The main page still opens daily through the real tab"), ClickFeatureButton(*Screen, TEXT("TaskPage_2")));
	TestNotNull(TEXT("The daily tab opens its own scroll"), Screen->GetWidgetFromName(TEXT("TaskDailyCatalogScroll")));

	Screen->OpenTasks(true, 1);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const auto* ReferenceScroll = Screen->GetWidgetFromName(TEXT("TaskMainScroll"));
	const auto* Row = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("TaskRow_0")));
	TestTrue(TEXT("The fixture still animates the main task row"), RokUiFind::IsInside(Row, ReferenceScroll));
	const auto* SideTitle = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskSideHeader")));
	TestTrue(TEXT("The reference entry labels its source boundary"),
		SideTitle && SideTitle->GetText().ToString().Contains(TEXT("本地引导参考态")));
	if (!TestTrue(TEXT("The original fixture animation action remains clickable"),
		ClickFeatureButton(*Screen, TEXT("TaskCompletedAnimationPreview")))) return false;
	TestTrue(TEXT("The action starts the original completion tween"), Screen->GetTaskCompletedAnimationElapsedForAudit() >= 0.0f);
	TestEqual(TEXT("Starting a tween does not apply a reward"), Screen->GetTaskCompletedAnimationFixtureCallbacksForAudit(), 0);
	Screen->CloseScreen();
	TestTrue(TEXT("Closing cancels the same active tween"), Screen->GetTaskCompletedAnimationElapsedForAudit() < 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTaskChapterCatalogItemPageTest, "Rok.Ui.FeatureItems.TaskChapterCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTaskChapterCatalogItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTaskChapterCatalogItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = CreateWidget<URokUnityFeatureScreenWidget>(Instance);
	if (!TestNotNull(TEXT("Task chapter host initializes"), Screen))
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	Screen->OpenTasks(false, 1);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	if (!TestTrue(TEXT("Main tasks can open the chapter tab"), ClickFeatureButton(*Screen, TEXT("TaskPage_0"))))
	{
		return false;
	}

	const auto* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	const auto* ChapterRoot = Screen->GetWidgetFromName(TEXT("TaskChapterRoot"));
	TestTrue(TEXT("The chapter is inside the active mode"), RokUiFind::IsInside(ChapterRoot, Mode));
	TestEqual(TEXT("The expected chapter presentation is mounted"), ChapterRoot && ChapterRoot->GetParent() == Mode,
		URokUiSettings::IsLegacyForced());
	const auto Text = [Screen](FName Name)
	{
		const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};
	TestEqual(TEXT("Chapter shows its source title"), Text(TEXT("TaskChapterGroupTitle")), FString(TEXT("第一章")));
	TestTrue(TEXT("The aggregate icon preserves the legacy address"),
		RokUiFind::IsInside(Screen->GetWidgetFromName(TEXT("TaskChapterAggregateReward_1_0Icon")), Mode));
	TestEqual(TEXT("The aggregate quantity matches the source"), Text(TEXT("TaskChapterAggregateReward_1_0Quantity")), FString(TEXT("50")));
	TestEqual(TEXT("The task quantity keeps comma grouping"), Text(TEXT("TaskChapterTaskReward_101001_0Quantity")), FString(TEXT("2,000")));

	const TArray<int32> TaskCounts = {3, 4, 6, 6, 7, 7, 6};
	for (int32 GroupId = 1; GroupId <= TaskCounts.Num(); ++GroupId)
	{
		const FName ButtonName(*FString::Printf(TEXT("TaskChapterGroupButton_%d"), GroupId));
		if (!TestTrue(TEXT("The chapter group button remains clickable"), ClickFeatureButton(*Screen, ButtonName)))
		{
			return false;
		}
		TestEqual(TEXT("The user-selected group remains active"), Screen->GetSelectedTaskChapterGroupIdForAudit(), GroupId);
		const auto* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskChapterScroll")));
		if (!TestTrue(TEXT("The chapter viewport is mounted"), Scroll && RokUiFind::IsInside(Scroll, Mode)))
		{
			return false;
		}
		TestEqual(TEXT("Every configured task has a scroll row"), Scroll->GetChildrenCount(), TaskCounts[GroupId - 1]);
		const TArray<int32>& TaskIds = Screen->GetRenderedTaskChapterTaskIdsForAudit();
		TestEqual(TEXT("All task IDs are retained"), TaskIds.Num(), TaskCounts[GroupId - 1]);
		for (int32 TaskId : TaskIds)
		{
			const FName RowName(*FString::Printf(TEXT("TaskChapterTaskRow_%d"), TaskId));
			TestTrue(TEXT("The source row alias resolves inside the active scroll"), RokUiFind::IsInside(Screen->GetWidgetFromName(RowName), Scroll));
			const FName ClaimName(*FString::Printf(TEXT("TaskChapterTaskClaim_%d"), TaskId));
			TestNull(TEXT("The local chapter has no account reward action"), Screen->GetWidgetFromName(ClaimName));
		}
		TestTrue(TEXT("Per-task package IDs remain available for auditing"), !Screen->GetRenderedTaskChapterTaskRewardPackageIdsForAudit().IsEmpty());
		TestEqual(TEXT("Only group seven has missing chapter metadata"), Screen->IsSelectedTaskChapterMetadataKnownForAudit(), GroupId != 7);
		TestEqual(TEXT("Only group seven omits aggregate reward IDs"), Screen->GetRenderedTaskChapterAggregateRewardPackageIdsForAudit().IsEmpty(), GroupId == 7);
	}
	const UWidget* Missing = Screen->GetWidgetFromName(TEXT("TaskChapterAggregateRewardUnavailable"));
	TestTrue(TEXT("The missing aggregate state is visible"), Missing && Missing->IsVisible());
	TestNull(TEXT("Group seven does not fabricate a reward"), Screen->GetWidgetFromName(TEXT("TaskChapterAggregateReward_7_0")));
	TestTrue(TEXT("The chapter window returns to the main task tab"), ClickFeatureButton(*Screen, TEXT("TaskPage_1")));
	TestNotNull(TEXT("Main task presentation is restored"), Screen->GetWidgetFromName(TEXT("TaskMainScroll")));
	TestNull(TEXT("No chapter row survives navigation"), Screen->GetWidgetFromName(TEXT("TaskChapterRoot")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMailReferenceItemPageTest, "Rok.Ui.FeatureItems.MailReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokMailReferenceItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokMailReferenceItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = CreateWidget<URokUnityFeatureScreenWidget>(Instance);
	if (!TestNotNull(TEXT("The reference mail screen initializes"), Screen)) return false;
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();

	Screen->OpenMail(true);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const auto* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	const auto* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("MailListScroll")));
	if (!TestTrue(TEXT("The reference list is mounted"), RokUiFind::IsInside(List, Mode))) return false;
	TestEqual(TEXT("The reference list uses the expected UI path"), List->GetParent() == Mode, URokUiSettings::IsLegacyForced());
	TestEqual(TEXT("The source reference displays six rows"), List->GetChildrenCount(), 6);
	const UWidget* Mesh = Screen->GetWidgetFromName(TEXT("MailAttachmentMesh_0"));
	if (!TestNotNull(TEXT("The source gift mesh is mounted"), Mesh)) return false;
	const auto* Host = Screen->GetWidgetFromName(TEXT("MailAttachmentPulse_0"));
	TestTrue(TEXT("The source mesh remains inside the pulse"), RokUiFind::IsInside(Mesh, Host));
	TestEqual(TEXT("The reference attachment retains its source reward"), Screen->GetRenderedMailRewardRowsForAudit().Num(), 1);

	if (!TestTrue(TEXT("The alternate source row remains actionable"), ClickFeatureButton(*Screen, TEXT("MailRowHit_1")))) return false;
	TestNotNull(TEXT("The selected reference row explains its missing body"), Screen->GetWidgetFromName(TEXT("MailDetailUnavailable")));
	TestNull(TEXT("No claim is shown for the missing source attachment"), Screen->GetWidgetFromName(TEXT("MailClaim")));
	TestTrue(TEXT("Reference row selection preserves the mesh instance"), Screen->GetWidgetFromName(TEXT("MailAttachmentMesh_0")) == Mesh);
	if (!TestTrue(TEXT("The alliance tab remains actionable"), ClickFeatureButton(*Screen, TEXT("MailTab_2")))) return false;
	TestNotNull(TEXT("The alternate category displays its empty state"), Screen->GetWidgetFromName(TEXT("MailEmptyState")));
	TestNull(TEXT("No source body persists in an empty category"), Screen->GetWidgetFromName(TEXT("MailDetailScroll")));
	if (!TestTrue(TEXT("Return through the System tab"), ClickFeatureButton(*Screen, TEXT("MailTab_0")))) return false;
	TestTrue(TEXT("The same attachment instance returns"), Screen->GetWidgetFromName(TEXT("MailAttachmentMesh_0")) == Mesh);

	FRokMailRewardPayload Mixed, Empty;
	FString Error;
	if (!TestTrue(TEXT("Both local reward payloads load"),
		RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::MixedAudit, Mixed, Error)
		&& RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::EmptyAlternate, Empty, Error))) return false;
	const TMap<int32, FRokMailRewardPayload> Payloads = {{0, Mixed}, {1, Empty}};
	Screen->OpenMailReadOnlyRewardFixture(Payloads);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestEqual(TEXT("The mixed mail displays seven normalized rewards"), Screen->GetRenderedMailRewardRowsForAudit().Num(), 7);
	TestNotNull(TEXT("The second row renders its final item"), Screen->GetWidgetFromName(TEXT("MailRewardFixtureIcon_6")));
	TestNull(TEXT("Currency rewards retain their non-interactive state"), Screen->GetWidgetFromName(TEXT("MailRewardFixtureHit_0")));
	if (!TestTrue(TEXT("The final reward uses the same tooltip action"), ClickFeatureButton(*Screen, TEXT("MailRewardFixtureHit_6")))) return false;
	TestEqual(TEXT("The tooltip follows the selected reward"), Screen->GetActiveMailRewardTooltipIndexForAudit(), 6);
	TestNotNull(TEXT("The tooltip body keeps its pointer hit region"), Screen->GetWidgetFromName(TEXT("MailRewardTooltipBodyHit")));
	TestNotNull(TEXT("The tooltip arrow keeps its pointer hit region"), Screen->GetWidgetFromName(TEXT("MailRewardTooltipArrowHit")));
	const auto* DetailSize = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("MailDetailContentSize")));
	TestTrue(TEXT("Multiple reward rows expand the original scroll content"), DetailSize && DetailSize->GetHeightOverride() > 590.0f);
	const FString Fingerprint = Screen->GetMailRewardFixtureFingerprintForAudit();
	if (!TestTrue(TEXT("The claim button keeps its original offline action"), ClickFeatureButton(*Screen, TEXT("MailClaim")))) return false;
	TestEqual(TEXT("The read-only claim reaches the existing boundary"), Screen->GetLastMailBoundaryActionIdForAudit(), 240);
	TestEqual(TEXT("No reward payload changes"), Screen->GetMailRewardFixtureFingerprintForAudit(), Fingerprint);
	if (!TestTrue(TEXT("The row with empty reward data is selectable"), ClickFeatureButton(*Screen, TEXT("MailRowHit_1")))) return false;
	const auto* EmptyGrid = Screen->GetWidgetFromName(TEXT("MailRewardFixtureGrid"));
	TestTrue(TEXT("An empty fixture has a zero-height grid"), EmptyGrid
		&& Cast<UCanvasPanelSlot>(EmptyGrid->Slot) && Cast<UCanvasPanelSlot>(EmptyGrid->Slot)->GetSize().Y == 0.0);
	TestEqual(TEXT("No synthetic rewards are added"), Screen->GetRenderedMailRewardRowsForAudit().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMailLocalItemPageTest, "Rok.Ui.FeatureItems.MailLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokMailLocalItemPageTest::RunTest(const FString&)
{
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	if (!TestTrue(TEXT("Mail host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenMailLocalUnreadFixture();
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestNotNull(TEXT("Local unread preview is explicitly identified"), Screen->GetWidgetFromName(TEXT("MailLocalUnreadNotice")));
	TestNotNull(TEXT("Unread report tab displays its badge"), Screen->GetWidgetFromName(TEXT("MailLocalTabUnreadCount_1")));
	TestTrue(TEXT("Selecting the second system row works"), ClickFeatureButton(*Screen, TEXT("MailRowHit_1")));
	const auto* Detail = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("MailLocalDetailTitle")));
	TestTrue(TEXT("Detail follows the selected system row"), Detail && Detail->GetText().ToString() == TEXT("系统通知示例 B"));

	TestTrue(TEXT("The local report tab stays inside the fixture"), ClickFeatureButton(*Screen, TEXT("MailTab_1")));
	TestNull(TEXT("Reading the local report clears the tab badge"), Screen->GetWidgetFromName(TEXT("MailLocalTabUnreadCount_1")));
	TestNotNull(TEXT("The report fixture renders its own row"), Screen->GetWidgetFromName(TEXT("MailRowHit_2")));
	TestNull(TEXT("The reference fixture does not use online report rows"), Screen->GetWidgetFromName(TEXT("BattleReportEmpty")));
	TestTrue(TEXT("The sent tab is reachable"), ClickFeatureButton(*Screen, TEXT("MailTab_4")));
	TestNotNull(TEXT("An empty local tab displays the empty state"), Screen->GetWidgetFromName(TEXT("MailEmptyState")));
	TestNull(TEXT("An empty local tab does not render detail"), Screen->GetWidgetFromName(TEXT("MailLocalDetailTitle")));

	Screen->OpenMail(false);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestNotNull(TEXT("Ordinary offline mailbox opens its empty state"), Screen->GetWidgetFromName(TEXT("MailEmptyState")));
	TestNull(TEXT("Ordinary mail omits the fixture banner"), Screen->GetWidgetFromName(TEXT("MailLocalUnreadNotice")));
	TestTrue(TEXT("The ordinary report tab retains its button"), ClickFeatureButton(*Screen, TEXT("MailTab_1")));
	TestNotNull(TEXT("Ordinary reports still enter the dedicated renderer"), Screen->GetWidgetFromName(TEXT("BattleReportEmpty")));
	TestTrue(TEXT("Returning from reports still works"), ClickFeatureButton(*Screen, TEXT("MailTab_0")));
	TestNotNull(TEXT("Returning restores ordinary mail"), Screen->GetWidgetFromName(TEXT("MailEmptyState")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceJoinItemPageTest, "Rok.Ui.FeatureItems.AllianceJoin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokAllianceJoinItemPageTest::RunTest(const FString&)
{
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	if (!TestTrue(TEXT("Alliance host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenAlliance(false);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestTrue(TEXT("Join opens from the original welcome action"), ClickFeatureButton(*Screen, TEXT("AllianceWelcomeJoin")));
	TestNotNull(TEXT("Ordinary join shows the server state boundary"), Screen->GetWidgetFromName(TEXT("AllianceJoinServiceState")));
	TestNull(TEXT("Ordinary join does not fabricate rows"), Screen->GetWidgetFromName(TEXT("AllianceJoinRowHit_0")));
	TestNull(TEXT("Ordinary join omits the fixture label"), Screen->GetWidgetFromName(TEXT("AllianceJoinFixtureTag")));

	const FString OriginalCommandLine(FCommandLine::Get());
	FCommandLine::Append(TEXT(" -RokAlliancePreviewFixture"));
	ON_SCOPE_EXIT { FCommandLine::Set(*OriginalCommandLine); };
	Screen->OpenAlliance(false);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestTrue(TEXT("The same join button opens the explicit fixture"), ClickFeatureButton(*Screen, TEXT("AllianceWelcomeJoin")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestNotNull(TEXT("Fixture is visibly labeled"), Screen->GetWidgetFromName(TEXT("AllianceJoinFixtureTag")));
	TestNotNull(TEXT("Unfiltered fixture contains all five rows"), Screen->GetWidgetFromName(TEXT("AllianceJoinRowHit_4")));

	auto Search = [Screen](const TCHAR* Query)
	{
		auto* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("AllianceSearchInput")));
		if (!Input)
		{
			return false;
		}
		Input->SetText(FText::FromString(Query));
		return ClickFeatureButton(*Screen, TEXT("AllianceSearch"));
	};
	TestTrue(TEXT("A short query runs the fixture search action"), Search(TEXT("RO")));
	TestNull(TEXT("A short query does not fabricate a selected row"), Screen->GetWidgetFromName(TEXT("AllianceJoinConfirm")));
	const auto* Empty = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceJoinFixtureEmpty")));
	TestTrue(TEXT("A short query shows the original minimum length hint"),
		Empty && Empty->GetText().ToString().Contains(TEXT("至少输入3个字符")));
	TestTrue(TEXT("A valid query runs the same search action"), Search(TEXT("  rok  ")));
	TestNull(TEXT("Filtering removes unmatched rows"), Screen->GetWidgetFromName(TEXT("AllianceJoinRowHit_0")));
	TestNotNull(TEXT("Filtering retains the matched row"), Screen->GetWidgetFromName(TEXT("AllianceJoinRowHit_1")));
	const auto* Name = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceJoinDetailName")));
	TestTrue(TEXT("Filtered selection uses the matched row identity"), Name && Name->GetText().ToString().StartsWith(TEXT("[ROK]")));
	const auto* Confirm = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceJoinConfirmLabel")));
	TestTrue(TEXT("The selected row keeps its application action text"),
		Confirm && Confirm->GetText().ToString() == TEXT("申请加入"));
	TestTrue(TEXT("Back uses the existing close animation route"), ClickFeatureButton(*Screen, TEXT("AllianceJoinBack")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestNotNull(TEXT("Back restores the welcome screen"), Screen->GetWidgetFromName(TEXT("AllianceWelcomeJoin")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceCreateItemPageTest, "Rok.Ui.FeatureItems.AllianceCreate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokAllianceCreateItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokAllianceCreateItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Alliance host initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	Screen->OpenAlliance(false);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestTrue(TEXT("Welcome opens the original creation route"), ClickFeatureButton(*Screen, TEXT("AllianceWelcomeCreate")));
	const auto* Cost = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceCreateCost")));
	TestTrue(TEXT("Create form displays the configured cost"), Cost && Cost->GetText().ToString().StartsWith(TEXT("创建消耗：")));
	TestTrue(TEXT("Create retains required field validation"), ClickFeatureButton(*Screen, TEXT("AllianceCreateConfirm")));
	const auto* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
	TestTrue(TEXT("An empty form explains the required fields"),
		Feedback && Feedback->GetText().ToString() == TEXT("请输入联盟名称和简称。"));

	const FName Inputs[] = {TEXT("AllianceCreateNameInput"), TEXT("AllianceCreateTagInput"), TEXT("AllianceCreateNoticeInput")};
	const TCHAR* Drafts[] = {TEXT("本地预览联盟"), TEXT("ROK"), TEXT("旗帜页面返回后保留公告")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Inputs); ++Index)
	{
		auto* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(Inputs[Index]));
		if (!TestNotNull(TEXT("The draft field is editable"), Input))
		{
			return false;
		}
		TestFalse(TEXT("The field retains its hint"), Input->GetHintText().IsEmpty());
		Input->SetText(FText::FromString(Drafts[Index]));
	}
	TestTrue(TEXT("The flag choice uses the existing editor"), ClickFeatureButton(*Screen, TEXT("AllianceFlagEdit")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestTrue(TEXT("Cancel returns from the flag editor"), ClickFeatureButton(*Screen, TEXT("AllianceFlagEditorCancel")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Inputs); ++Index)
	{
		const auto* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(Inputs[Index]));
		TestTrue(TEXT("Returning from flags restores draft data"),
			Input && Input->GetText().ToString() == Drafts[Index]);
	}
	TestNotNull(TEXT("Returning restores the rendered flag preview"), Screen->GetWidgetFromName(TEXT("AllianceCreateFlagEmblem")));
	TestTrue(TEXT("Back retains the welcome route"), ClickFeatureButton(*Screen, TEXT("AllianceCreateBack")));
	TestNotNull(TEXT("Welcome is shown after closing the form"), Screen->GetWidgetFromName(TEXT("AllianceWelcomeCreate")));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceFlagEditorItemPageTest, "Rok.Ui.FeatureItems.AllianceFlagEditor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokAllianceFlagEditorItemPageTest::RunTest(const FString&)
{
	using namespace RokAllianceFlagLocalPreview;
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	if (!TestTrue(TEXT("Alliance host initializes"), Screen->Initialize()))
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Screen->CloseScreen(); };

	const auto FlagMatches = [Screen](FName Name, const FIntRect& Rect, const FLinearColor& Tint)
	{
		const UImage* Image = Cast<UImage>(Screen->GetWidgetFromName(Name));
		if (!Image || !Image->GetBrush().GetResourceObject())
		{
			return false;
		}
		const FBox2f Region = static_cast<FBox2f>(Image->GetBrush().GetUVRegion());
		const FVector2f Min(Rect.Min.X / 1024.0f, (2048 - Rect.Max.Y) / 2048.0f);
		const FVector2f Max(Rect.Max.X / 1024.0f, (2048 - Rect.Min.Y) / 2048.0f);
		return Region.Min.Equals(Min) && Region.Max.Equals(Max) && Image->GetColorAndOpacity().Equals(Tint);
	};
	const auto DraftMatches = [Screen](const FDraft& Draft)
	{
		const auto* Identity = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceFlagEditorSourceIds")));
		const FString Expected = FString::Printf(TEXT("配置 ID  %d / %d / %d / %d"),
			Draft.ClothId, Draft.ClothColorId, Draft.EmblemId, Draft.EmblemColorId);
		return Identity && Identity->GetText().ToString() == Expected;
	};

	Screen->OpenAlliance(false);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	if (!TestTrue(TEXT("Welcome opens creation"), ClickFeatureButton(*Screen, TEXT("AllianceWelcomeCreate"))))
	{
		return false;
	}
	const FText Notice = FText::FromString(TEXT("旗帜编辑保留表单公告"));
	auto* NoticeInput = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("AllianceCreateNoticeInput")));
	if (!TestNotNull(TEXT("The creation notice draft is editable"), NoticeInput))
	{
		return false;
	}
	NoticeInput->SetText(Notice);

	if (!TestTrue(TEXT("Creation opens its flag editor"), ClickFeatureButton(*Screen, TEXT("AllianceFlagEdit"))))
	{
		return false;
	}
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const FDraft Defaults;
	FDraft Draft = Defaults;
	TestTrue(TEXT("Editor displays the original default IDs"), DraftMatches(Defaults));
	TestTrue(TEXT("A cloth can be tried before cancelling"), ClickFeatureButton(*Screen, TEXT("AllianceFlagItemHit_101")));
	TestTrue(TEXT("The editor cancel action runs"), ClickFeatureButton(*Screen, TEXT("AllianceFlagEditorCancel")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const FEntry* DefaultCloth = Find(1, Defaults.ClothId);
	const FEntry* DefaultClothColor = Find(2, Defaults.ClothColorId);
	TestTrue(TEXT("Cancel restores the creation flag atlas UV and tint"), DefaultCloth && DefaultClothColor
		&& FlagMatches(TEXT("AllianceCreateFlagCloth"), DefaultCloth->RealityRect, LinearColor(*DefaultClothColor)));

	if (!TestTrue(TEXT("Creation reopens the same flag editor"), ClickFeatureButton(*Screen, TEXT("AllianceFlagEdit"))))
	{
		return false;
	}
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestTrue(TEXT("Cancel does not change the draft when reopened"), DraftMatches(Defaults));

	int32 Checked = 0;
	for (int32 Type = 1; Type <= 4; ++Type)
	{
		const FString Tab = FString::Printf(TEXT("AllianceFlagTab_%d"), Type);
		if (!TestTrue(Tab, ClickFeatureButton(*Screen, *Tab)))
		{
			return false;
		}
		for (const FEntry& Entry : Rows())
		{
			if (Entry.Type != Type) continue;
			const FString HitName = FString::Printf(TEXT("AllianceFlagItemHit_%d"), Entry.Id);
			if (!TestTrue(HitName, ClickFeatureButton(*Screen, *HitName)))
			{
				return false;
			}
			Draft.SetId(Type, Entry.Id);
			TestTrue(FString::Printf(TEXT("Flag option %d updates its correct draft field"), Entry.Id), DraftMatches(Draft));
			if (Entry.bHasColor)
			{
				const auto* Swatch = Cast<UBorder>(Screen->GetWidgetFromName(
					*FString::Printf(TEXT("AllianceFlagItemSwatch_%d"), Entry.Id)));
				TestTrue(FString::Printf(TEXT("Flag option %d uses the source swatch"), Entry.Id),
					Swatch && Swatch->GetBrushColor().Equals(LinearColor(Entry)));
			}
			else
			{
				TestTrue(FString::Printf(TEXT("Flag option %d uses the source sprite"), Entry.Id),
					FlagMatches(*FString::Printf(TEXT("AllianceFlagItemSprite_%d"), Entry.Id), Entry.ShowRect, FLinearColor::White));
			}
			++Checked;
		}
	}
	TestEqual(TEXT("All four tabs expose all 63 source options"), Checked, 63);
	TestTrue(TEXT("Confirm commits the editor draft to the creation form"), ClickFeatureButton(*Screen, TEXT("AllianceFlagEditorConfirm")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const auto* Cloth = Find(1, Draft.ClothId);
	const auto* ClothColor = Find(2, Draft.ClothColorId);
	const auto* Emblem = Find(3, Draft.EmblemId);
	const auto* EmblemColor = Find(4, Draft.EmblemColorId);
	TestTrue(TEXT("Creation uses the confirmed cloth UV and tint"), Cloth && ClothColor
		&& FlagMatches(TEXT("AllianceCreateFlagCloth"), Cloth->RealityRect, LinearColor(*ClothColor)));
	TestTrue(TEXT("Creation uses the confirmed emblem UV and tint"), Emblem && EmblemColor
		&& FlagMatches(TEXT("AllianceCreateFlagEmblem"), Emblem->RealityRect, LinearColor(*EmblemColor)));
	NoticeInput = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("AllianceCreateNoticeInput")));
	TestTrue(TEXT("The local round-trip preserves other fields"), NoticeInput && NoticeInput->GetText().EqualTo(Notice));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResourceSearchItemPageTest, "Rok.Ui.FeatureItems.ResourceSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokResourceSearchItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokResourceSearchItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Search screen initializes"), Screen->Initialize()))
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Screen->CloseScreen(); };

	const auto GetSlider = [Screen] { return Cast<USlider>(Screen->GetWidgetFromName(TEXT("ResourceSearchSlider"))); };
	const auto ReadText = [Screen](FName Name)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Text ? Text->GetText().ToString() : FString();
	};
	const auto PopupLayout = [Screen]
	{
		const UWidget* Popup = Screen->GetWidgetFromName(TEXT("ResourceSearchPopupRoot"));
		return Popup ? Cast<UCanvasPanelSlot>(Popup->Slot) : nullptr;
	};

	Screen->OpenResourceSearch(5);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	if (!TestNotNull(TEXT("Guide search slider"), GetSlider()) || !TestNotNull(TEXT("Search popup layout"), PopupLayout()))
	{
		return false;
	}
	const double InitialPopupX = PopupLayout()->GetPosition().X;
	TestEqual(TEXT("Barbarian slider has the source maximum"), GetSlider()->GetMaxValue(), 25.0f);
	TestEqual(TEXT("The task opens its requested level"), GetSlider()->GetValue(), 5.0f);
	TestNotNull(TEXT("The task hand guide is present"), Screen->GetWidgetFromName(TEXT("ResourceSearchGuide")));

	if (!TestTrue(TEXT("The food type is clickable"), ClickFeatureButton(*Screen, TEXT("ResourceSearchType_1")))
		|| !TestNotNull(TEXT("Food search slider"), GetSlider()) || !TestNotNull(TEXT("Food popup layout"), PopupLayout()))
	{
		return false;
	}
	TestTrue(TEXT("Popup tracks the selected dock item"), FMath::IsNearlyEqual(PopupLayout()->GetPosition().X - InitialPopupX, 220.0));
	TestEqual(TEXT("Resources have a separate maximum"), GetSlider()->GetMaxValue(), 6.0f);
	TestNull(TEXT("Selecting a non-task resource hides the hand guide"), Screen->GetWidgetFromName(TEXT("ResourceSearchGuide")));
	TestTrue(TEXT("Food tooltip is bound"), ReadText(TEXT("ResourceSearchDescription")).Contains(TEXT("食物")));
	GetSlider()->OnValueChanged.Broadcast(3.6f);
	TestEqual(TEXT("Slider changes round to a level"), ReadText(TEXT("ResourceSearchLevelValue")), FString(TEXT("等级:4")));
	const auto* Fill = Cast<UProgressBar>(Screen->GetWidgetFromName(TEXT("ResourceSearchSliderFill")));
	TestTrue(TEXT("Fill reflects the snapped level"), Fill && FMath::IsNearlyEqual(Fill->GetPercent(), 0.6f));
	TestTrue(TEXT("Plus advances resource level"), ClickFeatureButton(*Screen, TEXT("ResourceSearchLevelPlus")));
	TestEqual(TEXT("Plus moves the native slider"), GetSlider()->GetValue(), 5.0f);
	TestTrue(TEXT("Minus reverses the level"), ClickFeatureButton(*Screen, TEXT("ResourceSearchLevelMinus")));
	TestEqual(TEXT("Minus moves the native slider"), GetSlider()->GetValue(), 4.0f);

	const auto* LockedIcon = Cast<UImage>(Screen->GetWidgetFromName(TEXT("ResourceSearchTypeIcon_4")));
	TestTrue(TEXT("Unsynced gold uses the existing grayscale material"),
		LockedIcon && Cast<UMaterialInstanceDynamic>(LockedIcon->GetBrush().GetResourceObject()) != nullptr);
	TestTrue(TEXT("Locked gold retains its explanatory interaction"), ClickFeatureButton(*Screen, TEXT("ResourceSearchType_4")));
	TestTrue(TEXT("Locked resource shows its research requirement"), ReadText(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("研究")));
	TestEqual(TEXT("Locked selection preserves the active food level"), GetSlider()->GetValue(), 4.0f);
	TestTrue(TEXT("The original search action remains bound"), ClickFeatureButton(*Screen, TEXT("ResourceSearchSubmit")));
	TestTrue(TEXT("City submission shows the existing world boundary"),
		ReadText(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("世界地图")));

	TestTrue(TEXT("Barbarian category is reachable again"), ClickFeatureButton(*Screen, TEXT("ResourceSearchType_0")));
	TestEqual(TEXT("Returning preserves the separate barbarian level"), GetSlider()->GetValue(), 5.0f);
	TestTrue(TEXT("The transparent mask dispatches the close action"), ClickFeatureButton(*Screen, TEXT("ResourceSearchTouchCloseMask")));
	Screen->AdvanceWindowAnimationForTesting(1.0f / 60.0f);
	TestFalse(TEXT("The original close animation releases the screen"), Screen->IsScreenOpen());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokActivityCenterItemPageTest, "Rok.Ui.FeatureItems.ActivityCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokActivityCenterItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokActivityCenterItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Activity host initializes"), Screen->Initialize()))
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Screen->CloseScreen(); };

	const auto Text = [Screen](FName Name)
	{
		const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};
	Screen->OpenActivityCenter();
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const FString Initial = Text(TEXT("ActivityCountdown"));
	TestFalse(TEXT("The original activity opens with a countdown"), Initial.IsEmpty());
	Screen->AdvanceActivityCountdownForTesting(1.0f);
	const FString Updated = Text(TEXT("ActivityCountdown"));
	TestTrue(TEXT("The ordinary countdown changes with elapsed time"), !Updated.IsEmpty() && Updated != Initial);
	TestTrue(TEXT("The alternate Lords War entry uses its original button"), ClickFeatureButton(*Screen, TEXT("ActivityMenuHit_2")));
	TestEqual(TEXT("Rebuilding the activity keeps the current countdown"), Text(TEXT("ActivityCountdown")), Updated);
	TestTrue(TEXT("History retains its boundary action"), ClickFeatureButton(*Screen, TEXT("ActivityHistoryButton")));
	TestTrue(TEXT("History still explains the service boundary"), Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("历史排行")));
	TestTrue(TEXT("The unavailable activity retains its menu button"), ClickFeatureButton(*Screen, TEXT("ActivityMenuHit_3")));
	TestTrue(TEXT("Unavailable menu remains on the current card"), Text(TEXT("ActivityTitle")).Contains(TEXT("城主之战")));
	TestTrue(TEXT("Unavailable menu retains its explanatory feedback"), Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("尚未迁入")));

	if (!TestTrue(TEXT("The calendar opens from its original menu button"), ClickFeatureButton(*Screen, TEXT("ActivityMenuHit_0"))))
	{
		return false;
	}
	const auto* Calendar = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("ActivityCalendarReferenceContent")));
	TestTrue(TEXT("The calendar draws all 89 resolved definitions"), Calendar && Calendar->GetChildrenCount() == 89);
	TestNull(TEXT("The city battle countdown does not overlay the calendar"), Screen->GetWidgetFromName(TEXT("ActivityCountdown")));
	TestTrue(TEXT("A catalog item with details selects through its original action"), ClickFeatureButton(*Screen, TEXT("ActivityCalendarReferenceHit_40201")));
	TestEqual(TEXT("Selected calendar title follows the catalog"), Text(TEXT("ActivityCalendarSelectedTitle")), FString(TEXT("笑傲诸城")));
	TestFalse(TEXT("An available description is displayed"), Text(TEXT("ActivityCalendarSelectedSketch")).IsEmpty());
	TestTrue(TEXT("The item without an icon remains selectable"), ClickFeatureButton(*Screen, TEXT("ActivityCalendarReferenceHit_100001")));
	TestEqual(TEXT("Selection updates to another catalog item"), Text(TEXT("ActivityCalendarSelectedTitle")), FString(TEXT("英豪盛宴")));
	TestNotNull(TEXT("A missing catalog icon uses the neutral art"), Screen->GetWidgetFromName(TEXT("ActivityCalendarSelectedNeutralIcon")));
	TestNull(TEXT("The neutral entry omits the missing icon"), Screen->GetWidgetFromName(TEXT("ActivityCalendarSelectedIcon")));
	TestNull(TEXT("The neutral entry omits an absent description"), Screen->GetWidgetFromName(TEXT("ActivityCalendarSelectedSketch")));

	TestTrue(TEXT("The original close button is bound"), ClickFeatureButton(*Screen, TEXT("ActivityClose")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestFalse(TEXT("The close animation releases the activity"), Screen->IsScreenOpen());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityManagerItemPageTest, "Rok.Ui.FeatureItems.CityManager",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityManagerItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCityManagerItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("City manager initializes"), Screen->Initialize()))
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Screen->CloseScreen(); };
	const auto ReadText = [Screen](FName Name)
	{
		const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};

	Screen->OpenCityManager();
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestTrue(TEXT("Relocation retains its dedicated action"), ClickFeatureButton(*Screen, TEXT("CityManagerRelocateButton")));
	TestTrue(TEXT("Relocation explains its server boundary"), ReadText(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("未关闭页面或移动城市")));
	TestNotNull(TEXT("Relocation leaves the main list open"), Screen->GetWidgetFromName(TEXT("CityManagerListScroll")));

	TestTrue(TEXT("Shield opens from its catalog group"), ClickFeatureButton(*Screen, TEXT("CityManagerGroupButton_301")));
	TestEqual(TEXT("Shield uses the group title"), ReadText(TEXT("CityManagerDetailTitle")), FString(TEXT("城市护盾")));
	const FString ShieldItem = ReadText(TEXT("CityManagerDetailItemName_0"));
	TestFalse(TEXT("The real local catalog provides a shield item"), ShieldItem.IsEmpty());
	TestTrue(TEXT("The item retains its original action"), ClickFeatureButton(*Screen, TEXT("CityManagerDetailAction_0")));
	TestTrue(TEXT("Item action explains inventory authority"), ReadText(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("未购买、消耗或激活")));

	TestTrue(TEXT("Back restores the source catalog"), ClickFeatureButton(*Screen, TEXT("CityManagerBack")));
	TestTrue(TEXT("Gathering opens from its distinct group"), ClickFeatureButton(*Screen, TEXT("CityManagerGroupButton_601")));
	TestEqual(TEXT("Gathering uses its distinct title"), ReadText(TEXT("CityManagerDetailTitle")), FString(TEXT("采集速度")));
	const FString GatherItem = ReadText(TEXT("CityManagerDetailItemName_0"));
	TestTrue(TEXT("Switching groups replaces the detail rows"), !GatherItem.IsEmpty() && GatherItem != ShieldItem);
	TestNull(TEXT("The detail page keeps the main list closed"), Screen->GetWidgetFromName(TEXT("CityManagerListScroll")));
	TestTrue(TEXT("The original close remains bound"), ClickFeatureButton(*Screen, TEXT("FeatureFrameClose")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestFalse(TEXT("Closing releases the city manager"), Screen->IsScreenOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHospitalItemPageTest, "Rok.Ui.FeatureItems.Hospital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHospitalItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokHospitalItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Hospital initializes"), Screen->Initialize()))
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Screen->CloseScreen(); };
	const FString OriginalCommandLine(FCommandLine::Get());
	ON_SCOPE_EXIT { FCommandLine::Set(*OriginalCommandLine); };
	const auto Open = [&](int32 Preview)
	{
		FCommandLine::Set(*(OriginalCommandLine + FString::Printf(TEXT(" -RokLocalOnly -RokHospitalPreviewState=%d"), Preview)));
		Screen->OpenHospital(nullptr);
		Screen->AdvanceWindowAnimationForTesting(1.0f);
	};
	const auto ReadText = [Screen](FName Name)
	{
		const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};
	const auto Enabled = [Screen](FName Name)
	{
		const auto* Widget = Screen->GetWidgetFromName(Name);
		return Widget && Widget->GetIsEnabled();
	};
	const auto Visible = [Screen](FName Name)
	{
		const auto* Widget = Screen->GetWidgetFromName(Name);
		return Widget && Widget->IsVisible();
	};

	Open(0);
	TestEqual(TEXT("No role remains unsynced"), ReadText(TEXT("HospitalWoundedState")), FString(TEXT("伤员信息尚未同步")));
	TestNull(TEXT("Unsynced hospital has no soldier rows"), Screen->GetWidgetFromName(TEXT("HospitalTroopValue_10101")));
	TestTrue(TEXT("Capacity help opens via the original button"), ClickFeatureButton(*Screen, TEXT("HospitalCapacityInfo")));
	TestFalse(TEXT("Help hides the hospital content"), Visible(TEXT("HospitalMainContent")));
	TestTrue(TEXT("Help back stays functional"), ClickFeatureButton(*Screen, TEXT("HospitalExplanationBackButton")));
	TestTrue(TEXT("Help back restores hospital content"), Visible(TEXT("HospitalMainContent")));

	Open(1);
	TestEqual(TEXT("Known empty is distinct from unsynced"), ReadText(TEXT("HospitalWoundedState")), FString(TEXT("没有受伤单位")));
	const auto* EmptyList = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("HospitalTreatmentList")));
	TestTrue(TEXT("Empty state has no treatment rows"), !EmptyList || EmptyList->GetChildrenCount() == 0);

	Open(2);
	const auto* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("HospitalTreatmentList")));
	TestTrue(TEXT("Distinct wounded soldiers have independent rows"), List && List->GetChildrenCount() == 2);
	auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("HospitalSlider_10101")));
	auto* Quantity = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("HospitalTroopValue_10101")));
	if (!TestNotNull(TEXT("Selected troop retains its slider"), Slider)
		|| !TestNotNull(TEXT("Selected troop retains its quantity editor"), Quantity))
	{
		return false;
	}
	TestEqual(TEXT("Wounded quantity initially uses the selected maximum"), Quantity->GetText().ToString(), FString(TEXT("4")));
	TestEqual(TEXT("Live selection reads the source food cost"), ReadText(TEXT("HospitalCostValue_0")), FString(TEXT("288")));
	Slider->OnValueChanged.Broadcast(0.5f);
	TestEqual(TEXT("Slider updates the editable quantity"), Quantity->GetText().ToString(), FString(TEXT("2")));
	TestNotEqual(TEXT("Slider reprices selected troops"), ReadText(TEXT("HospitalCostValue_0")), FString(TEXT("288")));
	Quantity->OnTextCommitted.Broadcast(FText::FromString(TEXT("4")), ETextCommit::OnEnter);
	TestEqual(TEXT("Quantity edits update the slider"), Slider->GetValue(), 1.0f);
	TestEqual(TEXT("Quantity edit restores full cost"), ReadText(TEXT("HospitalCostValue_0")), FString(TEXT("288")));
	TestTrue(TEXT("Instant treatment can open its quote without issuing a request"),
		Enabled(TEXT("HospitalHealNowButton")) && ClickFeatureButton(*Screen, TEXT("HospitalHealNowButton")));
	TestEqual(TEXT("Gem price reaches the confirmation"), ReadText(TEXT("HospitalConfirmationText")), FString(TEXT("消耗 1 宝石立即治疗？")));
	TestTrue(TEXT("Cancel closes confirmation"), ClickFeatureButton(*Screen, TEXT("HospitalInstantCancel")));
	TestNull(TEXT("Cancelled dialog is released"), Screen->GetWidgetFromName(TEXT("HospitalInstantConfirm")));

	Open(3);
	const auto* QueueValue = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("HospitalTroopValue_10101")));
	TestTrue(TEXT("Treatment locks the quantity editor"), QueueValue && QueueValue->GetIsReadOnly());
	TestFalse(TEXT("Treatment locks the slider"), Enabled(TEXT("HospitalSlider_10101")));
	TestFalse(TEXT("Active treatment cannot be collected"), Enabled(TEXT("HospitalHealButton")));
	TestEqual(TEXT("Queue clock drives remaining duration"), ReadText(TEXT("HospitalTreatmentCost")), FString(TEXT("00:04:40")));

	Open(4);
	TestEqual(TEXT("Finished queue switches to collect"), ReadText(TEXT("HospitalHealLabel")), FString(TEXT("领取")));
	TestTrue(TEXT("Ready treatment enables the same action"), Enabled(TEXT("HospitalHealButton")));
	TestFalse(TEXT("Ready treatment hides its instant action"), Visible(TEXT("HospitalHealNowButton")));
	TestTrue(TEXT("Original close action is bound"), ClickFeatureButton(*Screen, TEXT("FeatureFrameClose")));
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestFalse(TEXT("Closing releases the hospital"), Screen->IsScreenOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokSpeedUpItemPageTest, "Rok.Ui.FeatureItems.SpeedUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokSpeedUpItemPageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokSpeedUpItemPageTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Speed-up screen initializes"), Screen->Initialize()))
	{
		return false;
	}
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Screen->CloseScreen(); };
	const auto ReadText = [Screen](FName Name)
	{
		const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};

	Screen->OpenSpeedUp();
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	TestEqual(TEXT("Catalog-only route shows the local preview state"),
		ReadText(TEXT("SpeedUpQueueProgressText")), FString(TEXT("无活动队列")));
	const FString FirstDetails = ReadText(TEXT("SpeedUpItemDetails_0"));
	TestFalse(TEXT("Catalog row uses a real configurable item"), FirstDetails.IsEmpty());
	TestTrue(TEXT("Next page uses the original button"), ClickFeatureButton(*Screen, TEXT("SpeedUpNextPage")));
	TestNotEqual(TEXT("Next page replaces the first catalog item"), ReadText(TEXT("SpeedUpItemDetails_0")), FirstDetails);
	TestTrue(TEXT("Previous route is bound"), ClickFeatureButton(*Screen, TEXT("SpeedUpPreviousPage")));
	TestEqual(TEXT("Previous restores the catalog page"), ReadText(TEXT("SpeedUpItemDetails_0")), FirstDetails);
	TestTrue(TEXT("Selecting a local catalog item uses the read-only action"), ClickFeatureButton(*Screen, TEXT("SpeedUpItemAction_1")));
	TestTrue(TEXT("Local catalog leaves the timer intact"),
		ReadText(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("未改变倒计时")));

	const FString OriginalCommandLine(FCommandLine::Get());
	ON_SCOPE_EXIT { FCommandLine::Set(*OriginalCommandLine); };
	for (int32 Preview = 1; Preview <= 3; ++Preview)
	{
		FCommandLine::Set(*(OriginalCommandLine + FString::Printf(TEXT(" -RokLocalOnly -RokSpeedUpServerPreview=%d"), Preview)));
		Screen->OpenWorkerQueue();
		Screen->AdvanceWindowAnimationForTesting(1.0f);
		Screen->FlushPendingActionForTesting();
		Screen->FlushPendingActionForTesting();
		TestTrue(TEXT("Worker button opens the bound building's speed-up view"),
			ReadText(TEXT("SpeedUpQueueDescription")).Contains(TEXT("市政厅")));
		TestEqual(TEXT("Worker countdown drives the server format"),
			ReadText(TEXT("SpeedUpQueueProgressText")), FString(TEXT("00:01:30")));
		TestEqual(TEXT("Owned variant has its original inventory text"),
			ReadText(TEXT("SpeedUpItemCount_0")), FString(TEXT("拥有数量:2")));
		TestFalse(TEXT("Buy variant displays a gem quote"), ReadText(TEXT("SpeedUpBuyPrice_1")).IsEmpty());
		if (Preview >= 2)
		{
			TestTrue(TEXT("The original item action routes to the matching confirmation"),
				ReadText(TEXT("BuildingSpeedUpConfirmText")).Contains(Preview == 2 ? TEXT("宝石") : TEXT("已超出")));
			TestTrue(TEXT("Confirmation can return without changing the queue"),
				ClickFeatureButton(*Screen, TEXT("BuildingSpeedUpConfirmCancel")));
			TestNull(TEXT("Cancelling removes the confirmation"), Screen->GetWidgetFromName(TEXT("BuildingSpeedUpConfirmText")));
		}
		TestTrue(TEXT("Server page close is bound"), ClickFeatureButton(*Screen, TEXT("FeatureFrameClose")));
		TestNotNull(TEXT("Closing returns to the originating worker list"), Screen->GetWidgetFromName(TEXT("WorkerQueuePrimaryAction")));
	}
	return true;
}

#endif
