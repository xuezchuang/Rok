#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "../RokBuildingPanelWidget.h"
#include "../RokMainHudWidget.h"
#include "../RokLanguageSettingsLayout.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokMainHudInteractiveControlsTest,
	"Rok.City.Hud.InteractiveControls",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityManagerSourceStructureTest,
	"Rok.City.Ui.CityManager.SourceStructure",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokCityManagerSourceStructureTest::RunTest(
	const FString& Parameters)
{
	URokUnityFeatureScreenWidget* FeatureWidget =
		NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	TestNotNull(TEXT("City manager widget can be constructed"), FeatureWidget);
	if (!FeatureWidget)
	{
		return false;
	}
	TestTrue(
		TEXT("City manager widget initializes"),
		FeatureWidget->Initialize());
	FeatureWidget->TakeWidget();
	FeatureWidget->OpenCityManager();

	TestNotNull(
		TEXT("Main page has the source vertical scroll"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerListScroll"))));
	for (int32 Index = 0; Index < 6; ++Index)
	{
		TestNotNull(
			*FString::Printf(TEXT("Series title %d exists"), Index),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("CityManagerSeriesTitle_%d"), Index))));
	}
	static const int32 GroupIds[] = {
		101, 201, 301, 401, 402, 403, 501,
		502, 601, 602, 603, 604, 605};
	for (const int32 GroupId : GroupIds)
	{
		TestNotNull(
			*FString::Printf(TEXT("Group %d has a source name"), GroupId),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("CityManagerGroupName_%d"), GroupId))));
		TestNotNull(
			*FString::Printf(TEXT("Group %d has a source icon"), GroupId),
			Cast<UImage>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("CityManagerGroupIcon_%d"), GroupId))));
	}
	TestNotNull(
		TEXT("Relocation remains a dedicated action"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerRelocateButton"))));
	TestNull(
		TEXT("Main page hides the back control"),
		FeatureWidget->GetWidgetFromName(TEXT("CityManagerBack")));
	TestNull(
		TEXT("Old six-card engineering shell is removed"),
		FeatureWidget->GetWidgetFromName(TEXT("CityManagerSeriesCard_0")));

	UButton* WarRoute = Cast<UButton>(FeatureWidget->GetWidgetFromName(
		TEXT("CityManagerGroupButton_201")));
	TestNotNull(TEXT("War frenzy has a detail route"), WarRoute);
	if (WarRoute)
	{
		WarRoute->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
		TestNotNull(
			TEXT("War frenzy detail shows server-safe inactive state"),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				TEXT("CityManagerWarState"))));
		TestNotNull(
			TEXT("War frenzy detail shows the 13-row level table"),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				TEXT("CityManagerWarDuration_12"))));
		UButton* Back = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerBack")));
		TestNotNull(TEXT("Detail page exposes source back control"), Back);
		if (Back)
		{
			Back->OnClicked.Broadcast();
			FeatureWidget->FlushPendingActionForTesting();
			TestNotNull(
				TEXT("Back restores the source main list"),
				Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
					TEXT("CityManagerListScroll"))));
		}
	}

	UButton* ShieldRoute = Cast<UButton>(FeatureWidget->GetWidgetFromName(
		TEXT("CityManagerGroupButton_301")));
	TestNotNull(TEXT("City shield has a config detail route"), ShieldRoute);
	if (ShieldRoute)
	{
		ShieldRoute->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
		TestNotNull(
			TEXT("Config route opens the source detail scroll"),
			Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
				TEXT("CityManagerDetailScroll"))));
		TestNotNull(
			TEXT("Config route keeps the source back control"),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				TEXT("CityManagerBack"))));
	}
	FeatureWidget->CloseScreen();
	TestFalse(TEXT("City manager closes"), FeatureWidget->IsScreenOpen());
	return true;
}

bool FRokMainHudInteractiveControlsTest::RunTest(
	const FString& Parameters)
{
	URokMainHudWidget* HudWidget =
		NewObject<URokMainHudWidget>(GetTransientPackage());
	TestNotNull(TEXT("Main HUD can be constructed"), HudWidget);
	if (!HudWidget)
	{
		return false;
	}

	TestTrue(
		TEXT("Main HUD initializes its runtime WidgetTree"),
		HudWidget->Initialize());
	HudWidget->TakeWidget();

	static const FName RequiredButtonNames[] = {
		TEXT("PlayerProfileHitButton"),
		TEXT("PlayerArrowButton"),
		TEXT("TaskEntryButton"),
		TEXT("PositionSearchButton"),
		TEXT("FavoriteButton"),
		TEXT("ChatButton"),
		TEXT("CityWorldButton"),
		TEXT("FoodResourceHitButton"),
		TEXT("WoodResourceHitButton"),
		TEXT("GemResourceHitButton"),
		TEXT("BuildCityButton"),
		TEXT("ArmyQueueButton"),
		TEXT("CampaignFeatureButton"),
		TEXT("AllianceFeatureButton"),
		TEXT("MoreFeatureButton"),
		TEXT("MailFeatureHitButton"),
	};
	for (const FName& ButtonName : RequiredButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real UButton"),
				*ButtonName.ToString()),
			Cast<UButton>(HudWidget->GetWidgetFromName(ButtonName)));
	}

	URokBuildingPanelWidget* BuildingPanelWidget =
		Cast<URokBuildingPanelWidget>(
			HudWidget->GetWidgetFromName(TEXT("BuildingPanelWidget")));
	TestNotNull(
		TEXT("Main HUD owns the building panel"),
		BuildingPanelWidget);
	if (BuildingPanelWidget)
	{
		TestNotNull(
			TEXT("Building feature action is a real UButton"),
			Cast<UButton>(
				BuildingPanelWidget->GetWidgetFromName(
					TEXT("BuildingFeatureButton"))));
	}

	URokUnityFeatureScreenWidget* FeatureWidget =
		Cast<URokUnityFeatureScreenWidget>(
			HudWidget->GetWidgetFromName(
				TEXT("UnityFeatureScreenWidget")));
	TestNotNull(
		TEXT("Main HUD owns the shared feature screen"),
		FeatureWidget);
	if (FeatureWidget)
	{
		UButton* MailEntry = Cast<UButton>(
			HudWidget->GetWidgetFromName(TEXT("MailFeatureHitButton")));
		TestNotNull(TEXT("Main HUD exposes the physical mail entry"), MailEntry);
		if (MailEntry)
		{
			MailEntry->OnClicked.Broadcast();
			TestTrue(
				TEXT("Main HUD mail entry opens the shared feature screen"),
				FeatureWidget->IsScreenOpen());
			TestNotNull(
				TEXT("Production mail entry opens the Unity scroll list"),
				Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
					TEXT("MailListScroll"))));
			TestNotNull(
				TEXT("Production mail entry preserves the Unity compose control"),
				Cast<UButton>(FeatureWidget->GetWidgetFromName(
					TEXT("MailCompose"))));
			TestNotNull(
				TEXT("Production mail entry shows the Unity zero-row state"),
				Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
					TEXT("MailEmptyState"))));
			TestNull(
				TEXT("Production mail entry does not fabricate a first row"),
				FeatureWidget->GetWidgetFromName(TEXT("MailRow_0")));
			FeatureWidget->CloseScreen();
		}

		URokUnityModalWidget* ModalWidget =
			Cast<URokUnityModalWidget>(
				HudWidget->GetWidgetFromName(
					TEXT("UnityModalWidget")));
		TestNotNull(
			TEXT("Main HUD owns the shared modal screen"),
			ModalWidget);
		UButton* ProfileEntry = Cast<UButton>(
			HudWidget->GetWidgetFromName(
				TEXT("PlayerProfileHitButton")));
		if (ModalWidget && ProfileEntry)
		{
			// Broadcast verifies the synchronous UButton delegate route and
			// rebuilt widget state. It is not a physical Slate hit-test
			// acceptance.
			ProfileEntry->OnClicked.Broadcast();
			TestTrue(
				TEXT("Player-profile entry opens the shared modal"),
				ModalWidget->IsModalOpen());

			static const FName ProfileButtonNames[] = {
				TEXT("ProfilePictureButton"),
				TEXT("ProfileModifyButton"),
				TEXT("ProfileRenameButton"),
				TEXT("ProfileCivilizationButton"),
				TEXT("ProfileKillsHelpButton"),
				TEXT("ProfileActionHelpButton"),
				TEXT("ActionAddButton"),
				TEXT("ProfileArmyButton"),
				TEXT("ProfileMoreButton"),
				TEXT("ProfileRankingButton"),
				TEXT("ProfileSettingsButton"),
			};
			for (const FName& ButtonName : ProfileButtonNames)
			{
				TestNotNull(
					*FString::Printf(
						TEXT("%s is a real player-profile UButton"),
						*ButtonName.ToString()),
					Cast<UButton>(
						ModalWidget->GetWidgetFromName(ButtonName)));
			}

			static const FName ProfileTextNames[] = {
				TEXT("RoleId"),
				TEXT("ProfilePlayerName"),
				TEXT("CivilizationName"),
				TEXT("AllianceValue"),
				TEXT("PowerValue"),
				TEXT("KillsValue"),
				TEXT("ActionValue"),
			};
			static const TCHAR* LegacyPreviewValues[] = {
				TEXT("snow"),
				TEXT("18942891"),
				TEXT("7357"),
				TEXT("1000/1000"),
			};
			for (const FName& TextName : ProfileTextNames)
			{
				UTextBlock* ProfileText = Cast<UTextBlock>(
					ModalWidget->GetWidgetFromName(TextName));
				TestNotNull(
					*FString::Printf(
						TEXT("%s is a real player-profile UTextBlock"),
						*TextName.ToString()),
					ProfileText);
				if (!ProfileText)
				{
					continue;
				}

				const FString DisplayedText =
					ProfileText->GetText().ToString();
				TestFalse(
					*FString::Printf(
						TEXT("%s exposes a non-empty honest profile value"),
						*TextName.ToString()),
					DisplayedText.IsEmpty());
				FString CanonicalDisplayedText = DisplayedText;
				CanonicalDisplayedText.ReplaceInline(TEXT(","), TEXT(""));
				CanonicalDisplayedText.ReplaceInline(TEXT("，"), TEXT(""));
				for (const TCHAR* LegacyValue : LegacyPreviewValues)
				{
					TestFalse(
						*FString::Printf(
							TEXT("%s no longer exposes legacy preview value %s"),
							*TextName.ToString(),
							LegacyValue),
						CanonicalDisplayedText.Contains(LegacyValue));
				}
			}

			static const FName ProfileBoundaryButtonNames[] = {
				TEXT("ProfileKillsHelpButton"),
				TEXT("ProfileActionHelpButton"),
				TEXT("ActionAddButton"),
			};
			for (const FName& ButtonName : ProfileBoundaryButtonNames)
			{
				ProfileEntry->OnClicked.Broadcast();
				UButton* BoundaryButton = Cast<UButton>(
					ModalWidget->GetWidgetFromName(ButtonName));
				UTextBlock* NavigationFeedback = Cast<UTextBlock>(
					ModalWidget->GetWidgetFromName(
						TEXT("ProfileNavigationFeedback")));
				TestNotNull(
					*FString::Printf(
						TEXT("%s has a navigation-feedback text target"),
						*ButtonName.ToString()),
					NavigationFeedback);
				if (!BoundaryButton || !NavigationFeedback)
				{
					continue;
				}

				BoundaryButton->OnClicked.Broadcast();
				NavigationFeedback = Cast<UTextBlock>(
					ModalWidget->GetWidgetFromName(
						TEXT("ProfileNavigationFeedback")));
				TestNotNull(
					*FString::Printf(
						TEXT("%s keeps navigation feedback available"),
						*ButtonName.ToString()),
					NavigationFeedback);
				if (NavigationFeedback)
				{
					if (ButtonName == FName(TEXT("ProfileKillsHelpButton"))
						|| ButtonName == FName(TEXT("ProfileActionHelpButton")))
					{
						const bool bKillsHelp = ButtonName == FName(TEXT("ProfileKillsHelpButton"));
						const FName TipRootName = bKillsHelp
							? TEXT("PlayerKillTipRoot")
							: TEXT("PlayerActionTipRoot");
						const FName ClickAwayName = bKillsHelp
							? TEXT("PlayerKillTipClickAway")
							: TEXT("PlayerActionTipClickAway");
						TestNotNull(
							*FString::Printf(TEXT("%s opens its real tooltip"), *ButtonName.ToString()),
							ModalWidget->GetWidgetFromName(TipRootName));
						TestTrue(
							*FString::Printf(TEXT("%s does not use generic navigation feedback"), *ButtonName.ToString()),
							NavigationFeedback->GetVisibility() == ESlateVisibility::Collapsed);
						if (UButton* ClickAway = Cast<UButton>(ModalWidget->GetWidgetFromName(ClickAwayName)))
						{
							ClickAway->OnClicked.Broadcast();
							TestNull(
								*FString::Printf(TEXT("%s tooltip closes through its click-away"), *ButtonName.ToString()),
								ModalWidget->GetWidgetFromName(TipRootName));
							TestTrue(
								TEXT("Closing a profile tooltip preserves the profile modal"),
								ModalWidget->IsModalOpen());
						}
						else
						{
							AddError(FString::Printf(TEXT("%s tooltip has a click-away control"), *ButtonName.ToString()));
						}
				}
				else
				{
					TestTrue(
						TEXT("Action-point add explains the local no-mutation boundary"),
						NavigationFeedback->GetVisibility() == ESlateVisibility::HitTestInvisible
							&& NavigationFeedback->GetText().ToString().Contains(TEXT("不会扣除或增加")));
				}
				}
			}

			ProfileEntry->OnClicked.Broadcast();
			UButton* CivilizationRoute = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("ProfileCivilizationButton")));
			TestNotNull(TEXT("Profile exposes the civilization-selection route"), CivilizationRoute);
			if (CivilizationRoute)
			{
				CivilizationRoute->OnClicked.Broadcast();
				for (const int32 CivilizationId : {101, 201, 501})
				{
					TestNotNull(
						*FString::Printf(TEXT("Civilization %d is selectable from Unity config"), CivilizationId),
						Cast<UButton>(ModalWidget->GetWidgetFromName(
							*FString::Printf(TEXT("CivilizationChoiceButton_%d"), CivilizationId))));
				}
				for (const int32 CivilizationId : {301, 401, 601, 801})
				{
					TestNotNull(
						*FString::Printf(TEXT("Civilization %d keeps its Unity lock state"), CivilizationId),
						Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
							*FString::Printf(TEXT("CivilizationLockedState_%d"), CivilizationId))));
				}
				UButton* ContinueButton = Cast<UButton>(
					ModalWidget->GetWidgetFromName(TEXT("CivilizationContinueButton")));
				TestNotNull(TEXT("Civilization selection exposes the source change button"), ContinueButton);
				if (ContinueButton)
				{
					TestTrue(TEXT("An unlocked civilization can enter confirmation"), ContinueButton->GetIsEnabled());
					ContinueButton->OnClicked.Broadcast();
					TestNotNull(
						TEXT("Civilization change opens the source-shaped explanation scroll"),
						Cast<UScrollBox>(ModalWidget->GetWidgetFromName(TEXT("CivilizationConfirmationScroll"))));
					UButton* SubmitButton = Cast<UButton>(
						ModalWidget->GetWidgetFromName(TEXT("CivilizationSubmitButton")));
					TestNotNull(TEXT("Civilization confirmation exposes submit"), SubmitButton);
					if (SubmitButton)
					{
						TestFalse(
							TEXT("Civilization submit stays disabled without Role_ChangeCivilization"),
							SubmitButton->GetIsEnabled());
					}
					UButton* ConfirmationClose = Cast<UButton>(
						ModalWidget->GetWidgetFromName(TEXT("CivilizationChangeCloseButton")));
					if (ConfirmationClose)
					{
						ConfirmationClose->OnClicked.Broadcast();
						TestNotNull(
							TEXT("Closing civilization confirmation returns to selection"),
							Cast<UButton>(ModalWidget->GetWidgetFromName(TEXT("CivilizationChoiceButton_101"))));
					}
				}
			}

			ProfileEntry->OnClicked.Broadcast();
			UButton* HeadRoute = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("ProfilePictureButton")));
			TestNotNull(TEXT("Profile exposes a real player-head route"), HeadRoute);
			if (HeadRoute)
			{
				HeadRoute->OnClicked.Broadcast();
				TestNotNull(
					TEXT("Player-head route opens the Unity four-column catalog"),
					Cast<UButton>(ModalWidget->GetWidgetFromName(TEXT("PlayerHeadChoiceButton_0"))));
				UButton* HeadConfirm = Cast<UButton>(
					ModalWidget->GetWidgetFromName(TEXT("PlayerHeadConfirmButton")));
				TestNotNull(TEXT("Player-head page exposes confirm"), HeadConfirm);
				if (HeadConfirm)
				{
					TestFalse(
						TEXT("Player-head confirm stays disabled without authoritative ownership"),
						HeadConfirm->GetIsEnabled());
				}
				UButton* FramePage = Cast<UButton>(
					ModalWidget->GetWidgetFromName(TEXT("PlayerHeadPageButton_1")));
				TestNotNull(TEXT("Player-head page exposes the frame tab"), FramePage);
				if (FramePage)
				{
					FramePage->OnClicked.Broadcast();
					TestNotNull(
						TEXT("Frame tab rebuilds the frame catalog"),
						Cast<UButton>(ModalWidget->GetWidgetFromName(TEXT("PlayerHeadChoiceButton_1"))));
				}
				UButton* CloseSubscreen = Cast<UButton>(
					ModalWidget->GetWidgetFromName(TEXT("Type1CloseButton")));
				if (CloseSubscreen)
				{
					CloseSubscreen->OnClicked.Broadcast();
					ModalWidget->AdvanceWindowAnimationForTesting(0.25f);
					TestNotNull(
						TEXT("Closing player-head page returns to player profile"),
						Cast<UButton>(ModalWidget->GetWidgetFromName(TEXT("ProfileMoreButton"))));
				}
			}

			ProfileEntry->OnClicked.Broadcast();
			UButton* PowerRoute = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("ProfileMoreButton")));
			TestNotNull(TEXT("Profile exposes a real power-details route"), PowerRoute);
			if (PowerRoute)
			{
				PowerRoute->OnClicked.Broadcast();
				TestNotNull(
					TEXT("Power-details route opens the source-shaped scroll view"),
					Cast<UScrollBox>(ModalWidget->GetWidgetFromName(TEXT("PlayerPowerScroll"))));
				TestNotNull(
					TEXT("Power-details page exposes its server boundary"),
					Cast<UTextBlock>(ModalWidget->GetWidgetFromName(TEXT("PlayerPowerServerBoundaryState"))));
				UButton* CloseSubscreen = Cast<UButton>(
					ModalWidget->GetWidgetFromName(TEXT("Type3CloseButton")));
				if (CloseSubscreen)
				{
					CloseSubscreen->OnClicked.Broadcast();
					ModalWidget->AdvanceWindowAnimationForTesting(0.25f);
				}
			}

			ProfileEntry->OnClicked.Broadcast();
			UButton* RankingRoute = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("ProfileRankingButton")));
			TestNotNull(TEXT("Profile exposes a real ranking route"), RankingRoute);
			if (RankingRoute)
			{
				RankingRoute->OnClicked.Broadcast();
				UButton* FirstCategory = Cast<UButton>(
					ModalWidget->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_0")));
				TestNotNull(TEXT("Ranking page exposes source categories"), FirstCategory);
				if (FirstCategory)
				{
					FirstCategory->OnClicked.Broadcast();
					TestNotNull(
						TEXT("Ranking category opens the honest unsynced detail page"),
						Cast<UTextBlock>(ModalWidget->GetWidgetFromName(TEXT("PlayerRankingEmptyState"))));
					UButton* RankingBack = Cast<UButton>(
						ModalWidget->GetWidgetFromName(TEXT("PlayerRankingBackButton")));
					TestNotNull(TEXT("Ranking detail exposes a local back route"), RankingBack);
					if (RankingBack)
					{
						RankingBack->OnClicked.Broadcast();
						TestNotNull(
							TEXT("Ranking back restores the category grid"),
							Cast<UButton>(ModalWidget->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_0"))));
					}
				}
			}

			ProfileEntry->OnClicked.Broadcast();
			UButton* ArmyRoute = Cast<UButton>(
				ModalWidget->GetWidgetFromName(
					TEXT("ProfileArmyButton")));
			TestNotNull(
				TEXT("Profile exposes a real army route"),
				ArmyRoute);
			if (ArmyRoute)
			{
				ArmyRoute->OnClicked.Broadcast();
				TestNotNull(
					TEXT("Profile army route opens army statistics"),
					Cast<UButton>(
						FeatureWidget->GetWidgetFromName(
							TEXT("ArmyStatisticsPage_0"))));
				FeatureWidget->CloseScreen();
			}

			ProfileEntry->OnClicked.Broadcast();
			UButton* SettingsRoute = Cast<UButton>(
				ModalWidget->GetWidgetFromName(
					TEXT("ProfileSettingsButton")));
			TestNotNull(
				TEXT("Profile exposes a real settings route"),
				SettingsRoute);
			if (SettingsRoute)
			{
				SettingsRoute->OnClicked.Broadcast();
				TestNotNull(
					TEXT("Profile settings route opens the Unity settings hub"),
					Cast<UButton>(
						FeatureWidget->GetWidgetFromName(
							TEXT("SettingsHubGeneralButton"))));
				FeatureWidget->CloseScreen();
			}
		}

		struct FEntryExpectation
		{
			const TCHAR* HudButton;
			const TCHAR* ScreenAction;
			bool bTargetIsButton = true;
		};
		static const FEntryExpectation Entries[] = {
			{TEXT("PlayerArrowButton"), TEXT("CityManagerListScroll"), false},
			{TEXT("ArmyQueueButton"), TEXT("ArmyShowRulesButton")},
			{TEXT("ChatButton"), TEXT("ChatSendButton")},
			{TEXT("TaskEntryButton"), TEXT("TaskPage_1")},
			{TEXT("PositionSearchButton"), TEXT("WorldPositionGo")},
			{TEXT("FavoriteButton"), TEXT("MapMarkerGo")},
			{TEXT("BuildCityButton"), TEXT("BuildCityCategory_0")},
			{TEXT("CampaignFeatureButton"), TEXT("ExpeditionChallenge")},
			{TEXT("GemResourceHitButton"), TEXT("ChargePage_107")},
		};
		for (const FEntryExpectation& Entry : Entries)
		{
			UButton* EntryButton = Cast<UButton>(
				HudWidget->GetWidgetFromName(Entry.HudButton));
			if (!EntryButton)
			{
				continue;
			}
			EntryButton->OnClicked.Broadcast();
			TestTrue(
				*FString::Printf(
					TEXT("%s opens a feature screen"),
					Entry.HudButton),
				FeatureWidget->IsScreenOpen());
			UWidget* TargetWidget = FeatureWidget->GetWidgetFromName(
				Entry.ScreenAction);
			TestNotNull(
				*FString::Printf(
					TEXT("%s opens its source-mapped target"),
					Entry.HudButton),
				Entry.bTargetIsButton
					? static_cast<UWidget*>(Cast<UButton>(TargetWidget))
					: static_cast<UWidget*>(Cast<UScrollBox>(TargetWidget)));
			FeatureWidget->CloseScreen();
		}
	}

	UButton* MoreButton = Cast<UButton>(
		HudWidget->GetWidgetFromName(TEXT("MoreFeatureButton")));
	UCanvasPanel* CampaignRoot = Cast<UCanvasPanel>(
		HudWidget->GetWidgetFromName(TEXT("CampaignFeature")));
	if (MoreButton && CampaignRoot)
	{
		MoreButton->OnClicked.Broadcast();
		TestEqual(TEXT("Closing menu stays visible but cannot receive clicks"),
			CampaignRoot->GetVisibility(), ESlateVisibility::HitTestInvisible);
		HudWidget->AdvanceHudAnimations(0.125f);
		TestTrue(TEXT("Menu is halfway through source horizontal slide"),
			FMath::IsNearlyEqual(CampaignRoot->GetRenderTransform().Translation.X, 220.25f, 0.01f));
		HudWidget->AdvanceHudAnimations(0.125f);
		TestEqual(
			TEXT("More collapses the feature bar"),
			CampaignRoot->GetVisibility(),
			ESlateVisibility::Collapsed);
		MoreButton->OnClicked.Broadcast();
		HudWidget->AdvanceHudAnimations(0.25f);
		TestEqual(
			TEXT("More expands the feature bar"),
			CampaignRoot->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokPlayerProfileChangeNameTest,
	"Rok.City.Hud.ProfileChangeName",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokPlayerProfileChangeNameTest::RunTest(
	const FString& Parameters)
{
	TArray<FString> SyntheticBannedWords;
	SyntheticBannedWords.Add(TEXT("blocked"));
	const auto Validate = [&SyntheticBannedWords](
		const FString& Candidate,
		const FString& CurrentName = FString(),
		bool bHasCurrentName = false,
		bool bBannedWordsLoaded = true)
	{
		return URokUnityModalWidget::ValidateChangeNameCandidate(
			Candidate,
			CurrentName,
			bHasCurrentName,
			SyntheticBannedWords,
			bBannedWordsLoaded);
	};

	TestTrue(
		TEXT("Empty rename candidate is rejected"),
		Validate(TEXT("")) == ERokChangeNameValidationResult::Empty);
	TestTrue(
		TEXT("Two-character rename candidate is rejected"),
		Validate(TEXT("ab")) == ERokChangeNameValidationResult::TooShort);
	TestTrue(
		TEXT("Three-character rename candidate passes local format validation"),
		Validate(TEXT("abc")) == ERokChangeNameValidationResult::Valid);
	TestTrue(
		TEXT("Fifteen-character rename candidate passes local format validation"),
		Validate(FString::ChrN(15, TEXT('a')))
			== ERokChangeNameValidationResult::Valid);
	TestTrue(
		TEXT("Sixteen-character rename candidate is rejected"),
		Validate(FString::ChrN(16, TEXT('a')))
			== ERokChangeNameValidationResult::TooLong);
	TestTrue(
		TEXT("Pure numeric rename candidate is rejected"),
		Validate(TEXT("+123"))
			== ERokChangeNameValidationResult::OnlyDigits);
	TestTrue(
		TEXT("Current name cannot be submitted unchanged"),
		Validate(TEXT("Governor"), TEXT("Governor"), true)
			== ERokChangeNameValidationResult::SameAsCurrent);
	TestTrue(
		TEXT("Banned-word substring is case insensitive"),
		Validate(TEXT("MyBLOCKEDName"))
			== ERokChangeNameValidationResult::BannedWord);
	TestTrue(
		TEXT("Missing banned-word data fails closed"),
		Validate(TEXT("Governor"), FString(), false, false)
			== ERokChangeNameValidationResult::BannedWordDataUnavailable);

	URokUnityModalWidget* ModalWidget =
		NewObject<URokUnityModalWidget>(GetTransientPackage());
	TestNotNull(TEXT("Change-name modal can be constructed"), ModalWidget);
	if (!ModalWidget)
	{
		return false;
	}
	TestTrue(
		TEXT("Change-name modal initializes its runtime WidgetTree"),
		ModalWidget->Initialize());
	ModalWidget->TakeWidget();
	ModalWidget->OpenPlayerProfile();

	UTextBlock* ProfileNameBefore = Cast<UTextBlock>(
		ModalWidget->GetWidgetFromName(TEXT("ProfilePlayerName")));
	const FString NameBefore = ProfileNameBefore
		? ProfileNameBefore->GetText().ToString()
		: FString();
	UButton* RenameButton = Cast<UButton>(
		ModalWidget->GetWidgetFromName(TEXT("ProfileRenameButton")));
	TestNotNull(TEXT("Profile exposes the change-name route"), RenameButton);
	if (!RenameButton)
	{
		return false;
	}
	RenameButton->OnClicked.Broadcast();

	UEditableTextBox* ChangeNameInput = Cast<UEditableTextBox>(
		ModalWidget->GetWidgetFromName(TEXT("ChangeNameInput")));
	UButton* ConfirmButton = Cast<UButton>(
		ModalWidget->GetWidgetFromName(TEXT("ChangeNameConfirmButton")));
	UButton* CloseButton = Cast<UButton>(
		ModalWidget->GetWidgetFromName(TEXT("TypeMidCloseButton")));
	UTextBlock* ValidationFeedback = Cast<UTextBlock>(
		ModalWidget->GetWidgetFromName(
			TEXT("ChangeNameValidationFeedback")));
	UTextBlock* ServiceBoundary = Cast<UTextBlock>(
		ModalWidget->GetWidgetFromName(
			TEXT("ChangeNameServiceBoundaryState")));
	TestNotNull(TEXT("Change-name route opens an editable input"), ChangeNameInput);
	TestNotNull(TEXT("Change-name route has a confirm button"), ConfirmButton);
	TestNotNull(TEXT("Change-name route has a close button"), CloseButton);
	TestNotNull(TEXT("Change-name route has validation feedback"), ValidationFeedback);
	TestNotNull(TEXT("Change-name route exposes its service boundary"), ServiceBoundary);
	if (!ChangeNameInput || !ConfirmButton || !CloseButton || !ValidationFeedback)
	{
		return false;
	}

	TestFalse(
		TEXT("Confirm stays disabled without Role_ModifyName and authoritative inventory"),
		ConfirmButton->GetIsEnabled());
	ChangeNameInput->OnTextChanged.Broadcast(FText::FromString(
		TEXT("\u0634\u0631\u0645\u0648\u0637\u0629")));
	TestTrue(
		TEXT("Widget preserves Unity's index-zero blocked-word sentinel behavior"),
		ValidationFeedback->GetText().ToString().Contains(TEXT("服务器")));
	ChangeNameInput->OnTextChanged.Broadcast(
		FText::FromString(TEXT("AdminHero")));
	TestTrue(
		TEXT("Widget uses the Unity blocked-word table"),
		ValidationFeedback->GetText().ToString().Contains(TEXT("不可使用")));

	ChangeNameInput->OnTextChanged.Broadcast(
		FText::FromString(TEXT("NewGovernor")));
	TestTrue(
		TEXT("A locally valid name explains that server validation remains"),
		ValidationFeedback->GetText().ToString().Contains(TEXT("服务器")));
	TestFalse(
		TEXT("Locally valid input still cannot bypass the unavailable service"),
		ConfirmButton->GetIsEnabled());

	FString EmojiBoundary = FString::ChrN(14, TEXT('a'));
	EmojiBoundary.AppendChar(static_cast<TCHAR>(0xD83D));
	EmojiBoundary.AppendChar(static_cast<TCHAR>(0xDE00));
	EmojiBoundary.AppendChar(TEXT('b'));
	ChangeNameInput->OnTextChanged.Broadcast(
		FText::FromString(EmojiBoundary));
	TestEqual(
		TEXT("Fifteen-unit cap never leaves a split UTF-16 surrogate pair"),
		ChangeNameInput->GetText().ToString(),
		FString::ChrN(14, TEXT('a')));
	FString ZeroWidthCandidate(TEXT("New"));
	ZeroWidthCandidate.AppendChar(static_cast<TCHAR>(0x200B));
	ZeroWidthCandidate.Append(TEXT("Governor"));
	ChangeNameInput->OnTextChanged.Broadcast(
		FText::FromString(ZeroWidthCandidate));
	TestEqual(
		TEXT("Unity input filter removes zero-width characters in its configured range"),
		ChangeNameInput->GetText().ToString(),
		FString(TEXT("NewGovernor")));
	ConfirmButton->OnClicked.Broadcast();
	TestTrue(
		TEXT("Direct delegate broadcast is double-guarded at the service boundary"),
		ValidationFeedback->GetText().ToString().Contains(TEXT("没有发送请求")));

	CloseButton->OnClicked.Broadcast();
	ModalWidget->AdvanceWindowAnimationForTesting(0.25f);
	UTextBlock* ProfileNameAfter = Cast<UTextBlock>(
		ModalWidget->GetWidgetFromName(TEXT("ProfilePlayerName")));
	TestNotNull(TEXT("Close returns to the player-profile modal"), ProfileNameAfter);
	if (ProfileNameAfter)
	{
		TestEqual(
			TEXT("Blocked submit does not mutate the displayed profile name"),
			ProfileNameAfter->GetText().ToString(),
			NameBefore);
	}

	RenameButton = Cast<UButton>(
		ModalWidget->GetWidgetFromName(TEXT("ProfileRenameButton")));
	if (RenameButton)
	{
		RenameButton->OnClicked.Broadcast();
		ChangeNameInput = Cast<UEditableTextBox>(
			ModalWidget->GetWidgetFromName(TEXT("ChangeNameInput")));
		ValidationFeedback = Cast<UTextBlock>(
			ModalWidget->GetWidgetFromName(
				TEXT("ChangeNameValidationFeedback")));
		TestTrue(
			TEXT("Reopening change-name resets the draft"),
			ChangeNameInput && ChangeNameInput->GetText().IsEmpty());
		TestTrue(
			TEXT("Reopening change-name resets validation feedback"),
			ValidationFeedback
				&& ValidationFeedback->GetText().ToString().Contains(TEXT("请输入")));
		UButton* DismissMask = Cast<UButton>(
			ModalWidget->GetWidgetFromName(TEXT("ModalDismissMask")));
		TestNotNull(
			TEXT("Change-name modal retains its full-screen dismiss target"),
			DismissMask);
		if (DismissMask)
		{
			DismissMask->OnClicked.Broadcast();
			ModalWidget->AdvanceWindowAnimationForTesting(0.25f);
			TestNotNull(
				TEXT("Dismissing change-name returns to profile like its close button"),
				Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
					TEXT("ProfilePlayerName"))));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokMainHudFeatureScreenEntryTest,
	"Rok.City.Hud.FeatureScreenEntries",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokMainHudFeatureScreenEntryTest::RunTest(
	const FString& Parameters)
{
	// Subsystem-backed screens (gameplay catalog, runtime bootstrap state) need
	// a real GameInstance; the standalone game instance owns a dummy world for
	// this widget test, matching the runtime subsystem path. The engine must
	// be the outer because UGameInstance::GetEngine() casts its outer.
	UGameInstance* FeatureGameInstance = NewObject<UGameInstance>(GEngine);
	FeatureGameInstance->InitializeStandalone(
		TEXT("RokFeatureScreenEntryTest"));
	URokUnityFeatureScreenWidget* FeatureWidget =
		NewObject<URokUnityFeatureScreenWidget>(FeatureGameInstance->GetWorld());
	TestNotNull(TEXT("Feature screen can be constructed"), FeatureWidget);
	if (!FeatureWidget)
	{
		return false;
	}

	TestTrue(
		TEXT("Feature screen initializes its runtime WidgetTree"),
		FeatureWidget->Initialize());
	FeatureWidget->TakeWidget();
	URokUnityModalWidget* ModalWidget =
		NewObject<URokUnityModalWidget>(GetTransientPackage());
	TestNotNull(TEXT("Modal screen can be constructed"), ModalWidget);
	if (!ModalWidget)
	{
		return false;
	}
	TestTrue(
		TEXT("Modal screen initializes its runtime WidgetTree"),
		ModalWidget->Initialize());
	ModalWidget->TakeWidget();

	// Resource quick-use inventory: production stays honest (no server item
	// inventory → no fabricated owned counts); reference rows are explicit.
	ModalWidget->OpenResourceInventory(0);
	TestNotNull(
		TEXT("Production resource inventory shows the honest empty state"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("ResourceInventoryEmptyState"))));
	TestNotNull(
		TEXT("Production resource inventory keeps the service line"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("ResourceInventoryServiceState"))));
	TestNull(
		TEXT("Production resource inventory fabricates no owned rows"),
		ModalWidget->GetWidgetFromName(TEXT("ResourceRow_0")));
	ModalWidget->OpenResourceInventory(0, true);
	TestNotNull(
		TEXT("Reference resource inventory renders a row"),
		Cast<UCanvasPanel>(ModalWidget->GetWidgetFromName(
			TEXT("ResourceRow_0"))));
	TestNotNull(
		TEXT("Reference resource inventory is labeled as a local fixture"),
		ModalWidget->GetWidgetFromName(
			TEXT("ResourceInventoryReferenceTag")));

	FeatureWidget->OpenActivityCenter();
	for (int32 Tab = 0; Tab < 5; ++Tab)
	{
		TestNotNull(
			*FString::Printf(TEXT("Activity menu tab %d is a real button"), Tab),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("ActivityMenuHit_%d"), Tab))));
	}
	TestNotNull(
		TEXT("Activity preview card exposes the local preview label"),
		FeatureWidget->GetWidgetFromName(TEXT("ActivityPreviewTag")));
	TestNotNull(
		TEXT("Activity lords-war card has the countdown line"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("ActivityCountdown"))));
	if (UButton* CalendarTab = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("ActivityMenuHit_0"))))
	{
		CalendarTab->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
	}
	if (const UTextBlock* BoundaryFeedback = Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(TEXT("FeatureBoundaryFeedback"))))
	{
		TestTrue(
			TEXT("Unmigrated activity tab keeps the boundary feedback"),
			BoundaryFeedback->GetText().ToString().Contains(
				TEXT("该活动详情尚未迁入本地预览")));
	}

	FeatureWidget->OpenChat();
	TestTrue(TEXT("Chat screen opens"), FeatureWidget->IsScreenOpen());
	TestNotNull(
		TEXT("Chat screen has a real send button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("ChatSendButton"))));
	TestNotNull(
		TEXT("Chat screen has an editable message field"),
		Cast<UEditableTextBox>(
			FeatureWidget->GetWidgetFromName(
				TEXT("ChatInputTextBox"))));
	TestNotNull(
		TEXT("Chat screen has a real emoji toggle"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("ChatEmojiButton"))));
	TestNotNull(
		TEXT("Chat screen uses the source side close button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("FeatureFrameClose"))));
	for (int32 Channel = 0; Channel < 3; ++Channel)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("Chat channel %d is a real button"),
				Channel),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("ChatChannel_%d"), Channel))));
	}
	const UTextBlock* WorldChannelLabel = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("ChatChannelLabel_0")));
	if (TestNotNull(
		TEXT("Chat world channel label exists"),
		WorldChannelLabel))
	{
		TestTrue(
			TEXT("Chat world channel uses the Unity name 王国 (750011)"),
			WorldChannelLabel->GetText().ToString() == TEXT("王国"));
	}
	TestNotNull(
		TEXT("Default chat screen owns an honest empty message state"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("ChatMessagesEmptyState"))));
	TestNotNull(
		TEXT("Default chat screen owns an honest empty contact state"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("ChatContactsEmptyState"))));
	TestNull(
		TEXT("Default chat screen does not fabricate server messages"),
		FeatureWidget->GetWidgetFromName(TEXT("ChatMessage_0")));
	TestNull(
		TEXT("Default chat screen does not fabricate private contacts"),
		FeatureWidget->GetWidgetFromName(TEXT("ChatContact_0")));

	FeatureWidget->OpenChat(true);
	TestNotNull(
		TEXT("Explicit chat fixture can still render a reference message"),
		Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(TEXT("ChatMessage_0"))));
	TestNotNull(
		TEXT("Explicit chat fixture can still render a reference contact"),
		Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(TEXT("ChatContact_0"))));
	TestNotNull(
		TEXT("Explicit chat fixture is labeled as a local fixture"),
		FeatureWidget->GetWidgetFromName(TEXT("ChatFixtureTag")));

	FeatureWidget->OpenVip();
	UTextBlock* VipCurrentLevel = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipCurrentLevelText")));
	UTextBlock* VipTodayPoints = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipTodayPoints")));
	UTextBlock* VipLoginDays = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipLoginDays")));
	UTextBlock* VipProgress = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipProgressText")));
	TestTrue(
		TEXT("Default VIP screen does not fabricate an account level"),
		VipCurrentLevel
			&& VipCurrentLevel->GetText().ToString() == TEXT("--"));
	TestTrue(
		TEXT("Default VIP screen marks daily points as unsynchronized"),
		VipTodayPoints
			&& VipTodayPoints->GetText().ToString().Contains(TEXT("未同步")));
	TestTrue(
		TEXT("Default VIP screen marks login days as unsynchronized"),
		VipLoginDays
			&& VipLoginDays->GetText().ToString().Contains(TEXT("未同步")));
	TestTrue(
		TEXT("Default VIP screen marks progress as unsynchronized"),
		VipProgress
			&& VipProgress->GetText().ToString().Contains(TEXT("未同步")));
	TestNull(
		TEXT("Default VIP screen does not fabricate a daily-point red point"),
		FeatureWidget->GetWidgetFromName(TEXT("VipDailyPointRedPoint")));
	TestNull(
		TEXT("Default VIP screen does not fabricate a free-chest red point"),
		FeatureWidget->GetWidgetFromName(TEXT("VipFreeChestRedPoint")));
	UButton* VipClaimFreeChest = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipClaimFreeChest")));
	UButton* VipBuySpecialChest = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipBuySpecialChest")));
	UButton* VipFreeChestHit = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipFreeChestHit")));
	UButton* VipSpecialChestHit = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipSpecialChestHit")));
	TestTrue(
		TEXT("Default VIP free-chest action waits for a role snapshot"),
		VipClaimFreeChest && !VipClaimFreeChest->GetIsEnabled());
	TestTrue(
		TEXT("Default VIP purchase action waits for a role snapshot"),
		VipBuySpecialChest && !VipBuySpecialChest->GetIsEnabled());
	TestTrue(
		TEXT("Default VIP free-chest image waits for a role snapshot"),
		VipFreeChestHit && !VipFreeChestHit->GetIsEnabled());
	TestTrue(
		TEXT("Default VIP special-chest image waits for a role snapshot"),
		VipSpecialChestHit && !VipSpecialChestHit->GetIsEnabled());

	FeatureWidget->OpenVip(true);
	VipTodayPoints = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipTodayPoints")));
	VipLoginDays = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipLoginDays")));
	VipProgress = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipProgressText")));
	TestTrue(
		TEXT("Explicit VIP fixture uses Unity first-login daily points"),
		VipTodayPoints
			&& VipTodayPoints->GetText().ToString().Contains(TEXT("40")));
	TestTrue(
		TEXT("Explicit VIP fixture uses Unity first-login streak"),
		VipLoginDays
			&& VipLoginDays->GetText().ToString().Contains(TEXT("1")));
	TestTrue(
		TEXT("Explicit VIP fixture uses Unity first-login progress"),
		VipProgress
			&& VipProgress->GetText().ToString() == TEXT("0/200"));
	TestNotNull(
		TEXT("Explicit VIP fixture owns the unclaimed daily-point red point"),
		FeatureWidget->GetWidgetFromName(TEXT("VipDailyPointRedPoint")));
	TestNotNull(
		TEXT("Explicit VIP fixture owns the unclaimed free-chest red point"),
		FeatureWidget->GetWidgetFromName(TEXT("VipFreeChestRedPoint")));
	VipFreeChestHit = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipFreeChestHit")));
	VipSpecialChestHit = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipSpecialChestHit")));
	TestTrue(
		TEXT("Explicit VIP fixture enables the free-chest image hit area"),
		VipFreeChestHit && VipFreeChestHit->GetIsEnabled());
	TestTrue(
		TEXT("Explicit VIP fixture enables the special-chest image hit area"),
		VipSpecialChestHit && VipSpecialChestHit->GetIsEnabled());
	TestNotNull(
		TEXT("Explicit VIP fixture is labeled as a local fixture"),
		FeatureWidget->GetWidgetFromName(TEXT("VipFixtureTag")));

	// Level paging mirrors the Unity arrow buttons: next shows VIP 1, back
	// returns to VIP 0.
	if (UButton* VipNext = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("VipNextPage"))))
	{
		VipNext->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
		const UTextBlock* VipBenefitTitle = Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(TEXT("VipBenefitTitle")));
		TestTrue(
			TEXT("VIP next arrow advances the preview level to 1"),
			VipBenefitTitle
				&& VipBenefitTitle->GetText().ToString().Contains(TEXT("VIP 1")));
		if (UButton* VipPrevious = Cast<UButton>(FeatureWidget->GetWidgetFromName(
				TEXT("VipPreviousPage"))))
		{
			VipPrevious->OnClicked.Broadcast();
			FeatureWidget->FlushPendingActionForTesting();
			VipBenefitTitle = Cast<UTextBlock>(
				FeatureWidget->GetWidgetFromName(TEXT("VipBenefitTitle")));
			TestTrue(
				TEXT("VIP back arrow returns the preview level to 0"),
				VipBenefitTitle
					&& VipBenefitTitle->GetText().ToString().Contains(TEXT("VIP 0")));
		}
	}
	else
	{
		AddError(TEXT("VIP screen exposes the next-level arrow"));
	}

	FeatureWidget->OpenGemShop();
	const UTextBlock* GemShopBalanceText = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("GemShopBalance")));
	if (TestNotNull(TEXT("Gem shop balance line exists"), GemShopBalanceText))
	{
		TestTrue(
			TEXT("Gem shop does not fabricate a gem balance"),
			GemShopBalanceText->GetText().ToString().Contains(TEXT("未同步")));
	}
	const UTextBlock* FirstGemPackage = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("GemShopPackageAmount_0")));
	if (TestNotNull(TEXT("Gem shop renders catalog gem packages"), FirstGemPackage))
	{
		TestTrue(
			TEXT("Gem shop first package uses Unity RechargeGemMall 200 gems"),
			FirstGemPackage->GetText().ToString().Contains(TEXT("200")));
	}
	const UTextBlock* FirstGemBuyLabel = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("GemShopBuy_0Label")));
	if (TestNotNull(TEXT("Gem shop buy button label exists"), FirstGemBuyLabel))
	{
		TestTrue(
			TEXT("Gem shop buy button shows no fabricated cash price"),
			!FirstGemBuyLabel->GetText().ToString().Contains(TEXT("¥")));
	}
	TestNotNull(
		TEXT("Gem shop keeps the payment boundary note"),
		FeatureWidget->GetWidgetFromName(TEXT("GemShopBoundaryNote")));

	FeatureWidget->OpenWorldPositionSearch();
	TestTrue(
		TEXT("World-position search screen stays open"),
		FeatureWidget->IsScreenOpen());
	TestNotNull(
		TEXT("World-position search has a real go button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("WorldPositionGo"))));
	TestNotNull(
		TEXT("World-position search has an editable X field"),
		Cast<UEditableTextBox>(
			FeatureWidget->GetWidgetFromName(
				TEXT("WorldPositionXInput"))));
	TestNotNull(
		TEXT("World-position search has an editable Y field"),
		Cast<UEditableTextBox>(
			FeatureWidget->GetWidgetFromName(
				TEXT("WorldPositionYInput"))));

	FeatureWidget->OpenMapMarkers();
	TestTrue(
		TEXT("Map-marker screen stays open"),
		FeatureWidget->IsScreenOpen());
	TestNotNull(
		TEXT("Map-marker screen has a real go button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("MapMarkerGo"))));

	FeatureWidget->OpenResourceSearch();
	TestNotNull(
		TEXT("Resource search uses the Unity bottom dock"),
		FeatureWidget->GetWidgetFromName(
			TEXT("ResourceSearchDockBackground")));
	TestNotNull(
		TEXT("Resource search uses the Unity moving popup"),
		FeatureWidget->GetWidgetFromName(TEXT("ResourceSearchPopup")));
	TestNotNull(
		TEXT("Resource search exposes the Unity touch-close mask"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("ResourceSearchTouchCloseMask"))));
	TestNotNull(
		TEXT("Resource search uses the Unity map-icon frame"),
		FeatureWidget->GetWidgetFromName(
			TEXT("ResourceSearchTypeFrame_0")));
	TestNotNull(
		TEXT("Resource search uses the Unity barbarian icon"),
		FeatureWidget->GetWidgetFromName(
			TEXT("ResourceSearchTypeIcon_0")));
	TestNotNull(
		TEXT("Resource search has an integer level slider"),
		Cast<USlider>(FeatureWidget->GetWidgetFromName(
			TEXT("ResourceSearchSlider"))));
	TestNotNull(
		TEXT("Resource search has a real submit button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(
				TEXT("ResourceSearchSubmit"))));
	TestNull(
		TEXT("Resource search no longer uses the generic framed modal close"),
		FeatureWidget->GetWidgetFromName(TEXT("FeatureFrameClose")));
	TestNull(
		TEXT("Resource search no longer exposes the engineering source note"),
		FeatureWidget->GetWidgetFromName(TEXT("ResourceSearchSourceNote")));
	if (UWidget* Dimmer = FeatureWidget->GetWidgetFromName(
		TEXT("FeatureScreenDimmer")))
	{
		TestEqual(
			TEXT("Resource search leaves the world map visible"),
			Dimmer->GetVisibility(),
			ESlateVisibility::Collapsed);
	}

	UButton* FoodSearchType = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("ResourceSearchType_1")));
	TestNotNull(TEXT("Resource search has a real farmland type button"), FoodSearchType);
	if (FoodSearchType)
	{
		FoodSearchType->OnClicked.Broadcast();
		// Indexed button actions defer to the next widget tick; flush the
		// pending action explicitly because there is no tick loop in this test.
		FeatureWidget->FlushPendingActionForTesting();
		USlider* ResourceSlider = Cast<USlider>(
			FeatureWidget->GetWidgetFromName(TEXT("ResourceSearchSlider")));
		TestTrue(
			TEXT("Gather-resource search uses the Unity configured level cap 6"),
			ResourceSlider && FMath::IsNearlyEqual(ResourceSlider->GetMaxValue(), 6.0f));
		for (int32 ClickIndex = 0; ClickIndex < 8; ++ClickIndex)
		{
			if (UButton* Plus = Cast<UButton>(FeatureWidget->GetWidgetFromName(
				TEXT("ResourceSearchLevelPlus"))))
			{
				Plus->OnClicked.Broadcast();
				FeatureWidget->FlushPendingActionForTesting();
			}
		}
		UTextBlock* ResourceLevel = Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(TEXT("ResourceSearchLevelValue")));
		TestTrue(
			TEXT("Gather-resource plus control clamps at level 6"),
			ResourceLevel && ResourceLevel->GetText().ToString().Contains(TEXT("6")));

		if (UButton* BarbarianType = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("ResourceSearchType_0"))))
		{
			BarbarianType->OnClicked.Broadcast();
			FeatureWidget->FlushPendingActionForTesting();
		}
		USlider* BarbarianSlider = Cast<USlider>(
			FeatureWidget->GetWidgetFromName(TEXT("ResourceSearchSlider")));
		TestTrue(
			TEXT("Barbarian search keeps its separate local maximum"),
			BarbarianSlider && FMath::IsNearlyEqual(BarbarianSlider->GetMaxValue(), 25.0f));
		if (UButton* FoodTypeAgain = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("ResourceSearchType_1"))))
		{
			FoodTypeAgain->OnClicked.Broadcast();
			FeatureWidget->FlushPendingActionForTesting();
		}
		UTextBlock* PreservedResourceLevel = Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(TEXT("ResourceSearchLevelValue")));
		TestTrue(
			TEXT("Switching types preserves the shared resource level"),
			PreservedResourceLevel
				&& PreservedResourceLevel->GetText().ToString().Contains(TEXT("6")));
	}
	if (UButton* TouchClose = Cast<UButton>(FeatureWidget->GetWidgetFromName(
		TEXT("ResourceSearchTouchCloseMask"))))
	{
		TouchClose->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
		FeatureWidget->AdvanceWindowAnimationForTesting(1.0f / 60.0f);
		TestFalse(
			TEXT("Resource-search touch mask closes the interface"),
			FeatureWidget->IsScreenOpen());
	}

	FeatureWidget->OpenExpedition();
	TestNotNull(
		TEXT("Expedition has a real challenge button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(
				TEXT("ExpeditionChallenge"))));

	FeatureWidget->OpenCharge(107);
	static const int32 ChargePageTypes[] = {
		101,
		102,
		103,
		104,
		107,
		105,
		106,
	};
	for (const int32 PageType : ChargePageTypes)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("Charge page %d has a real navigation button"),
				PageType),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("ChargePage_%d"), PageType))));
	}
	TestNotNull(
		TEXT("Charge screen exposes the payment/server boundary"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("ChargeServiceBoundaryState"))));
	TestNull(
		TEXT("Opening charge page 107 does not show a non-gem page boundary"),
		FeatureWidget->GetWidgetFromName(
			TEXT("ChargeSelectedPageServiceBoundary")));

	FeatureWidget->OpenBuildCity();
	UTextBlock* BuildCardName = Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(TEXT("BuildCityCardName_2")));
	UTextBlock* CountTitle = Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(TEXT("BuildCityCardCountTitle_2")));
	UTextBlock* DurationTitle = Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(TEXT("BuildCityCardDurationTitle_2")));
	if (TestNotNull(TEXT("Source constructed label"), CountTitle) && TestNotNull(TEXT("Source construction-time label"), DurationTitle))
	{
		TestEqual(TEXT("Language 180304"), CountTitle->GetText().ToString(), FString(TEXT("已建造")));
		TestEqual(TEXT("Language 180303"), DurationTitle->GetText().ToString(), FString(TEXT("建造时间")));
	}
	UTextBlock* BuildCardDescription = Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(TEXT("BuildCityCardDescription_2")));
	if (TestNotNull(TEXT("Farm title from runtime catalog"), BuildCardName)
		&& TestNotNull(TEXT("Farm description from runtime catalog"), BuildCardDescription))
	{
		TestEqual(TEXT("Unity 24px title uses 18pt"), BuildCardName->GetFont().Size, 18.0f);
		TestEqual(TEXT("Unity 18px body keeps fractional 13.5pt"), BuildCardDescription->GetFont().Size, 13.5f);
		TestEqual(TEXT("No invented title shadow"), BuildCardName->GetShadowOffset(), FVector2D::ZeroVector);
		TestEqual(TEXT("No invented description shadow"), BuildCardDescription->GetShadowOffset(), FVector2D::ZeroVector);
	}
	static const FName BuildCityButtonNames[] = {
		TEXT("BuildCityCategory_0"),
		TEXT("BuildCityCategory_1"),
		TEXT("BuildCityCategory_2"),
	};
	for (const FName& ButtonName : BuildCityButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real BuildCity button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	TestNotNull(
		TEXT("BuildCity exposes the Unity horizontal building list"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("BuildCityBuildingScroll"))));
	TestNotNull(
		TEXT("BuildCity exposes the Unity mask close route"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("BuildCityMaskClose"))));
	TestNotNull(
		TEXT("BuildCity uses the Unity bottom drawer background"),
		FeatureWidget->GetWidgetFromName(
			TEXT("BuildCityDrawerBackground")));
	TestNotNull(
		TEXT("BuildCity uses the Unity right-side decoration"),
		FeatureWidget->GetWidgetFromName(
			TEXT("BuildCitySideDecoration")));
	TestNull(
		TEXT("BuildCity no longer exposes the non-Unity global place button"),
		FeatureWidget->GetWidgetFromName(TEXT("BuildCityPlaceButton")));
	TestNull(
		TEXT("BuildCity no longer uses the standard modal close button"),
		FeatureWidget->GetWidgetFromName(TEXT("FeatureFrameClose")));
	// Indexed actions rebuild on NativeTick after the UButton callback unwinds.
	// This transient widget test has no real frame pump; the decoration category
	// and type-51 route are covered by the physical BuildCity acceptance run.

	FeatureWidget->OpenWorkerQueue();
	static const FName WorkerQueueButtonNames[] = {
		TEXT("WorkerQueuePrimaryAction"),
		TEXT("WorkerQueueSecondQueueUnlockOrExtend"),
	};
	for (const FName& ButtonName : WorkerQueueButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real worker-queue button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	TestNull(
		TEXT("Worker queue does not show a simultaneous renew action"),
		FeatureWidget->GetWidgetFromName(
			TEXT("WorkerQueueSecondQueueRenew")));
	TestNotNull(
		TEXT("Worker queue exposes a real primary queue progress bar"),
		Cast<UProgressBar>(FeatureWidget->GetWidgetFromName(
			TEXT("WorkerQueuePrimaryProgress"))));
	TestNotNull(
		TEXT("Worker queue exposes primary queue countdown/progress text"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("WorkerQueuePrimaryProgressText"))));
	TestNotNull(
		TEXT("Worker queue exposes the second queue server-state note"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("WorkerQueueSecondaryNote"))));

	FeatureWidget->OpenHospital();
	static const FName HospitalButtonNames[] = {
		TEXT("HospitalCapacityInfo"),
		TEXT("HospitalHealButton"),
		TEXT("HospitalHealNowButton"),
		TEXT("HospitalMaxButton"),
	};
	for (const FName& ButtonName : HospitalButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real hospital button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	TestNotNull(
		TEXT("Hospital exposes catalog capacity text"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("HospitalCapacityValue"))));
	TestNotNull(
		TEXT("Hospital exposes honest wounded-state boundary"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("HospitalWoundedState"))));

	FeatureWidget->OpenCityManager();
	TestNotNull(
		TEXT("City manager exposes the Unity vertical list"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerListScroll"))));
	for (int32 SeriesIndex = 0; SeriesIndex < 6; ++SeriesIndex)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("City-manager series %d has a source title"),
				SeriesIndex),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(
					TEXT("CityManagerSeriesTitle_%d"),
					SeriesIndex))));
	}
	TestNotNull(
		TEXT("City manager exposes the source relocate action"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerRelocateButton"))));
	TestNotNull(
		TEXT("City manager exposes the war-frenzy group route"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerGroupButton_201"))));
	TestNotNull(
		TEXT("City manager exposes the final economy group"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerGroupName_605"))));
	TestNull(
		TEXT("City manager removes the old engineering boundary copy"),
		FeatureWidget->GetWidgetFromName(
			TEXT("CityManagerServiceBoundaryState")));

	FeatureWidget->OpenArmyShow();
	TestNotNull(
		TEXT("ArmyShow exposes the Unity rules entry"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("ArmyShowRulesButton"))));
	TestNotNull(
		TEXT("ArmyShow exposes the Unity vertical army list"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("ArmyShowListScroll"))));
	TestNull(
		TEXT("ArmyShow does not fabricate an active army row"),
		FeatureWidget->GetWidgetFromName(TEXT("ArmyShowRow_0")));
	TestNull(
		TEXT("ArmyShow removes the old fixed placeholder queue"),
		FeatureWidget->GetWidgetFromName(TEXT("ArmyShowQueueShade_0")));
	TestNull(
		TEXT("ArmyShow removes the non-Unity sync control"),
		FeatureWidget->GetWidgetFromName(
			TEXT("ArmyShowSyncBoundaryButton")));

	FeatureWidget->OpenMail(false);
	for (int32 TabIndex = 0; TabIndex < 6; ++TabIndex)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("Mail tab %d is a real button"),
				TabIndex),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("MailTab_%d"), TabIndex))));
	}
	TestNotNull(
		TEXT("Production mail preserves the Unity compose entry"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(TEXT("MailCompose"))));
	TestNotNull(
		TEXT("Production mail uses a real empty scroll list"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("MailListScroll"))));
	TestNull(
		TEXT("Production mail hides attachment actions until the service is ready"),
		FeatureWidget->GetWidgetFromName(TEXT("MailFooterHit_0")));
	TestNotNull(
		TEXT("Production mail shows the Unity zero-row message"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("MailEmptyState"))));
	TestNull(
		TEXT("Production mail removes the non-Unity engineering boundary copy"),
		FeatureWidget->GetWidgetFromName(TEXT("MailServiceBoundaryState")));
	TestNull(
		TEXT("Production mail does not fabricate a first message"),
		FeatureWidget->GetWidgetFromName(TEXT("MailRow_0")));
	TestNull(
		TEXT("Production mail does not fabricate an unread red point"),
		FeatureWidget->GetWidgetFromName(
			TEXT("MailSystemTabRedPoint")));

	FeatureWidget->OpenMail(true);
	TestNotNull(
		TEXT("Mail reference mode keeps a scrollable summary list"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("MailListScroll"))));
	TestNotNull(
		TEXT("Mail reference mode keeps the screenshot compose button"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("MailCompose"))));
	TestNotNull(
		TEXT("Mail reference mode keeps the screenshot footer actions"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("MailFooterHit_0"))));
	TestNotNull(
		TEXT("Mail reference mode keeps a selectable screenshot row"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("MailRowHit_0"))));
	TestNotNull(
		TEXT("Mail reference mode is explicitly labelled"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("MailReferencePreviewNotice"))));

	ModalWidget->OpenWarWarning(false);
	TestTrue(TEXT("War-warning modal opens"), ModalWidget->IsModalOpen());
	TestNotNull(
		TEXT("Production war warning exposes the server-state boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("WarWarningServiceBoundaryState"))));
	TestNull(
		TEXT("Production war warning does not fabricate a first warning"),
		ModalWidget->GetWidgetFromName(TEXT("WarWarningRow_0")));

	ModalWidget->OpenWarWarning(true);
	TestNotNull(
		TEXT("War-warning reference mode is explicitly labelled"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("WarWarningReferencePreviewNotice"))));
	TestNotNull(
		TEXT("War-warning reference mode keeps a screenshot row"),
		ModalWidget->GetWidgetFromName(TEXT("WarWarningRow_0")));

	ModalWidget->OpenWarWarning(false);
	TestNotNull(
		TEXT("Reopened production war warning restores the server boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("WarWarningServiceBoundaryState"))));
	TestNull(
		TEXT("Reopened production war warning removes the reference row"),
		ModalWidget->GetWidgetFromName(TEXT("WarWarningRow_0")));

	ModalWidget->OpenBuffList(false);
	TestNotNull(
		TEXT("Production buff list exposes the server-state boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("BuffListServiceBoundaryState"))));
	TestNull(
		TEXT("Production buff list does not fabricate active buffs"),
		ModalWidget->GetWidgetFromName(TEXT("BuffListReferenceTag_0")));
	ModalWidget->OpenBuffList(true);
	TestNotNull(
		TEXT("Buff-list reference mode is explicitly labelled"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("BuffListReferencePreviewNotice"))));
	TestNotNull(
		TEXT("Buff-list reference mode keeps a reference row"),
		ModalWidget->GetWidgetFromName(TEXT("BuffListReferenceTag_0")));
	ModalWidget->OpenBuffList(false);
	TestNull(
		TEXT("Reopened production buff list removes reference rows"),
		ModalWidget->GetWidgetFromName(TEXT("BuffListReferenceTag_0")));

	ModalWidget->OpenPlayerHeadSelection();
	TestNotNull(
		TEXT("Player head selection exposes the head grid"),
		ModalWidget->GetWidgetFromName(TEXT("PlayerHeadGridPanel")));
	TestNotNull(
		TEXT("Player head selection has a real confirm button"),
		Cast<UButton>(ModalWidget->GetWidgetFromName(
			TEXT("PlayerHeadConfirmButton"))));
	TestNotNull(
		TEXT("Player head selection keeps the server-state boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("PlayerHeadServerBoundaryState"))));

	ModalWidget->OpenBuffList(false);
	TestTrue(TEXT("Buff-list modal opens"), ModalWidget->IsModalOpen());
	TestNotNull(
		TEXT("Production buff list exposes the server-state boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("BuffListServiceBoundaryState"))));
	TestNull(
		TEXT("Production buff list does not fabricate a first buff"),
		ModalWidget->GetWidgetFromName(TEXT("BuffListRow_0")));

	ModalWidget->OpenBuffList(true);
	TestNotNull(
		TEXT("Buff-list reference mode is explicitly labelled"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("BuffListReferencePreviewNotice"))));
	TestNotNull(
		TEXT("Buff-list reference mode keeps a screenshot row"),
		ModalWidget->GetWidgetFromName(TEXT("BuffListRow_0")));

	ModalWidget->OpenBuffList(false);
	TestNotNull(
		TEXT("Reopened production buff list restores the server boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("BuffListServiceBoundaryState"))));
	TestNull(
		TEXT("Reopened production buff list removes the reference row"),
		ModalWidget->GetWidgetFromName(TEXT("BuffListRow_0")));
	ModalWidget->CloseModal();

	FeatureWidget->OpenSettingsHub();
	static const FName SettingsHubButtonNames[] = {
		TEXT("SettingsHubCharacterButton"),
		TEXT("SettingsHubNotificationButton"),
		TEXT("SettingsHubGeneralButton"),
		TEXT("SettingsHubLanguageButton"),
		TEXT("SettingsHubCommunityButton"),
		TEXT("SettingsHubHelpButton"),
		TEXT("SettingsHubAccountButton")};
	for (const FName& ButtonName : SettingsHubButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real settings-hub button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	UButton* CharacterSettings = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("SettingsHubCharacterButton")));
	UButton* CommunitySettings = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("SettingsHubCommunityButton")));
	TestTrue(
		TEXT("Settings hub keeps character management behind its server gate"),
		CharacterSettings && !CharacterSettings->GetIsEnabled());
	TestTrue(
		TEXT("Settings hub keeps community behind its platform gate"),
		CommunitySettings && !CommunitySettings->GetIsEnabled());

	FeatureWidget->OpenGeneralSettings();
	TestNotNull(
		TEXT("General settings uses the Unity vertical scroll layout"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("GeneralSettingsScroll"))));
	USlider* GraphicsSlider = Cast<USlider>(FeatureWidget->GetWidgetFromName(
		TEXT("GeneralSettingsGraphicsSlider")));
	USlider* FrameRateSlider = Cast<USlider>(FeatureWidget->GetWidgetFromName(
		TEXT("GeneralSettingsFrameRateSlider")));
	USlider* MusicSlider = Cast<USlider>(FeatureWidget->GetWidgetFromName(
		TEXT("GeneralSettingsMusicSlider")));
	USlider* SfxSlider = Cast<USlider>(FeatureWidget->GetWidgetFromName(
		TEXT("GeneralSettingsSfxSlider")));
	TestNotNull(TEXT("General settings has a graphics slider"), GraphicsSlider);
	TestNotNull(TEXT("General settings has a frame-rate slider"), FrameRateSlider);
	TestTrue(
		TEXT("Music slider has a runtime audio route"),
		MusicSlider && MusicSlider->GetIsEnabled());
	TestTrue(
		TEXT("SFX slider has a runtime audio route"),
		SfxSlider && SfxSlider->GetIsEnabled());

	FeatureWidget->OpenNotificationSettings();
	TestNotNull(
		TEXT("Notification settings exposes the server-shaped scroll list"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("NotificationSettingsScroll"))));
	UButton* FirstNotification = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("NotificationSetting_1")));
	TestTrue(
		TEXT("Unknown notification state cannot be changed optimistically"),
		FirstNotification && !FirstNotification->GetIsEnabled());

	FeatureWidget->OpenLanguageSettings();
	UButton* LanguageConfirm = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("LanguageConfirmButton")));
	if (!TestNotNull(TEXT("Language settings has a real confirm button"), LanguageConfirm))
	{
		return false;
	}
	TestEqual(
		TEXT("Language screen starts from the explicitly applied Chinese preview"),
		FeatureWidget->GetLanguageDraftIdForAudit(),
		RokLanguageSettingsLayout::AppliedLanguageId);
	TestTrue(
		TEXT("Confirm is enabled only while the draft matches the applied language"),
		LanguageConfirm && LanguageConfirm->GetIsEnabled());
	if (UButton* ArabicOption = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("LanguageOptionArabicButton"))))
	{
		ArabicOption->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
		TestEqual(
			TEXT("Selecting Arabic changes only the draft identity"),
			FeatureWidget->GetLanguageDraftIdForAudit(),
			RokLanguageSettingsLayout::SourceId(1));
		TestFalse(
			TEXT("Unsupported locale draft cannot be confirmed or applied"),
			LanguageConfirm->GetIsEnabled());
		if (UButton* ChineseOption = Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("LanguageOptionChineseButton"))))
		{
			ChineseOption->OnClicked.Broadcast();
			FeatureWidget->FlushPendingActionForTesting();
			TestEqual(
				TEXT("Returning to Chinese restores the unchanged-language close state"),
				FeatureWidget->GetLanguageDraftIdForAudit(),
				RokLanguageSettingsLayout::AppliedLanguageId);
			TestTrue(TEXT("Unchanged-language confirm is available to leave settings"),
				LanguageConfirm->GetIsEnabled());
		}
		else
		{
			AddError(TEXT("Language settings exposes the Chinese source option"));
		}
	}
	else
	{
		AddError(TEXT("Language settings exposes the Arabic draft option"));
	}

	FeatureWidget->OpenAccountSettings();
	UButton* AccountChange = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("AccountChangeButton")));
	TestTrue(
		TEXT("Account switching waits for the IGG SDK"),
		AccountChange && !AccountChange->GetIsEnabled());

	FeatureWidget->OpenArmyStatistics();
	for (int32 Page = 0; Page < 3; ++Page)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("Army statistics page %d has a real tab button"),
				Page),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(
					*FString::Printf(
						TEXT("ArmyStatisticsPage_%d"),
						Page))));
	}

	FeatureWidget->OpenStore();
	static const FName StoreButtonNames[] = {
		TEXT("StoreCategory_0"),
		TEXT("StoreCategory_1"),
		TEXT("StoreCategory_2"),
		TEXT("StoreCategory_3"),
		TEXT("StorePreviousPage"),
		TEXT("StoreNextPage"),
		TEXT("StoreQuantityMinus"),
		TEXT("StoreQuantityPlus"),
		TEXT("StoreQuantityMax"),
		TEXT("StoreBuyButton"),
	};
	for (const FName& ButtonName : StoreButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real store button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}

	FeatureWidget->OpenItems();
	static const FName ItemsButtonNames[] = {
		TEXT("ItemsCategory_0"),
		TEXT("ItemsCategory_1"),
		TEXT("ItemsCategory_2"),
		TEXT("ItemsCategory_3"),
		TEXT("ItemsCategory_4"),
	};
	for (const FName& ButtonName : ItemsButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real items button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	UTextBlock* ItemsEmptyState = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("ItemsInventoryEmptyState")));
	TestNotNull(TEXT("Items default state renders the Unity empty inventory label"), ItemsEmptyState);
	if (ItemsEmptyState)
	{
		TestEqual(
			TEXT("Items empty inventory wording matches UI_Win_Bag"),
			ItemsEmptyState->GetText().ToString(),
			FString(TEXT("您的道具库里真是空空如也，快获取一些道具来充实它吧！")));
	}
	TestNull(
		TEXT("Items default state does not fabricate an inventory list"),
		FeatureWidget->GetWidgetFromName(TEXT("ItemsInventoryScroll")));
	TestNull(
		TEXT("Items default state hides item details"),
		FeatureWidget->GetWidgetFromName(TEXT("ItemsDetailPanel")));
	TestNull(
		TEXT("Items no longer exposes Unity-incompatible previous-page control"),
		FeatureWidget->GetWidgetFromName(TEXT("ItemsPreviousPage")));
	TestNull(
		TEXT("Items no longer exposes Unity-incompatible next-page control"),
		FeatureWidget->GetWidgetFromName(TEXT("ItemsNextPage")));

	FeatureWidget->OpenTrainArmy(nullptr, 1);
	static const FName TrainArmyButtonNames[] = {
		TEXT("TrainArmyTier_0"),
		TEXT("TrainArmyTier_1"),
		TEXT("TrainArmyTier_2"),
		TEXT("TrainArmyTier_3"),
		TEXT("TrainArmyTier_4"),
		TEXT("TrainArmyMinus"),
		TEXT("TrainArmyPlus"),
		TEXT("TrainArmyMax"),
		TEXT("TrainArmyStart"),
	};
	for (const FName& ButtonName : TrainArmyButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real train-army button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	TestNotNull(
		TEXT("Train-army context is rendered"),
		Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(
				TEXT("TrainArmyBuildingContext"))));
	TestNotNull(
		TEXT("Train-army quantity is rendered"),
		Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(
				TEXT("TrainArmyQuantity"))));

	FeatureWidget->OpenSpeedUp(nullptr, 20102);
	static const FName SpeedUpButtonNames[] = {
		TEXT("SpeedUpPreviousPage"),
		TEXT("SpeedUpNextPage"),
		TEXT("SpeedUpItemAction_0"),
	};
	for (const FName& ButtonName : SpeedUpButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real speed-up button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	TestNotNull(
		TEXT("Speed-up queue progress is a real UProgressBar"),
		Cast<UProgressBar>(
			FeatureWidget->GetWidgetFromName(
				TEXT("SpeedUpQueueProgress"))));
	TestNotNull(
		TEXT("Speed-up queue description is rendered"),
		Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(
				TEXT("SpeedUpQueueDescription"))));
	TestNotNull(
		TEXT("Speed-up page state is rendered"),
		Cast<UTextBlock>(
			FeatureWidget->GetWidgetFromName(
				TEXT("SpeedUpPage"))));
	const bool bHasSpeedUpCatalogRow =
		FeatureWidget->GetWidgetFromName(
			TEXT("SpeedUpItemName_0")) != nullptr;
	const bool bHasSpeedUpEmptyState =
		FeatureWidget->GetWidgetFromName(
			TEXT("SpeedUpEmptyCatalog")) != nullptr;
	TestTrue(
		TEXT("Speed-up screen exposes either catalog content or an honest empty state"),
		bHasSpeedUpCatalogRow != bHasSpeedUpEmptyState);

	FeatureWidget->OpenCommander();
	static const FName CommanderButtonNames[] = {
		TEXT("CommanderSortAction"),
		TEXT("CommanderHeadAction_0"),
		TEXT("CommanderHeadAction_8"),
		TEXT("CommanderSummonAction"),
	};
	for (const FName& ButtonName : CommanderButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real commander button"),
				*ButtonName.ToString()),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(ButtonName)));
	}
	TestNotNull(
		TEXT("Commander roster is a continuous vertical scroll"),
		Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
			TEXT("CommanderRosterScroll"))));
	TestNotNull(
		TEXT("Default unowned commander exposes the summon state"),
		Cast<UBorder>(FeatureWidget->GetWidgetFromName(
			TEXT("CommanderSummonPanel"))));
	TestNull(
		TEXT("Commander pagination is removed"),
		FeatureWidget->GetWidgetFromName(TEXT("CommanderPreviousPage")));
	TestNull(
		TEXT("Unowned commander does not expose skill upgrade"),
		FeatureWidget->GetWidgetFromName(TEXT("CommanderSkillUpgrade")));
	TestNull(
		TEXT("Inactive Unity commander background is not rendered"),
		FeatureWidget->GetWidgetFromName(TEXT("CommanderBackground")));

	FeatureWidget->OpenAlliance(false);
	TestNotNull(
		TEXT("Alliance welcome has a real join button"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceWelcomeJoin"))));
	TestNotNull(
		TEXT("Alliance welcome has a real create button"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceWelcomeCreate"))));
	TestNull(
		TEXT("Default alliance screen does not fabricate joined-state data"),
		FeatureWidget->GetWidgetFromName(TEXT("AllianceJoinedName")));

	// Production join list stays honest: Unity source search hint (730025),
	// Unity empty-list text (730024), an explicit service line, and no
	// fabricated alliance rows.
	if (UButton* WelcomeJoin = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceWelcomeJoin"))))
	{
		WelcomeJoin->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
	}
	if (UEditableTextBox* AllianceSearchInput = Cast<UEditableTextBox>(
			FeatureWidget->GetWidgetFromName(TEXT("AllianceSearchInput"))))
	{
		TestTrue(
			TEXT("Alliance search hint uses the Unity source text"),
			AllianceSearchInput->GetHintText().ToString().Contains(
				TEXT("点击此处输入想要搜索的联盟")));
	}
	else
	{
		AddError(TEXT("Production alliance join list shows the search input"));
	}
	const UTextBlock* JoinEmptyState = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("AllianceJoinEmptyState")));
	if (TestNotNull(
		TEXT("Production alliance join list shows the Unity empty state"),
		JoinEmptyState))
	{
		TestTrue(
			TEXT("Alliance empty state uses Unity text 730024"),
			JoinEmptyState->GetText().ToString().Contains(
				TEXT("当前王国未找到合适的联盟")));
	}
	TestNotNull(
		TEXT("Production alliance join list keeps the service line"),
		FeatureWidget->GetWidgetFromName(TEXT("AllianceJoinServiceState")));
	TestNull(
		TEXT("Production alliance join list fabricates no alliance rows"),
		FeatureWidget->GetWidgetFromName(TEXT("AllianceJoinRowHit_0")));
	if (UButton* JoinBack = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceJoinBack"))))
	{
		JoinBack->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
	}
	TestNotNull(
		TEXT("Alliance back returns to the welcome page"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceWelcomeJoin"))));

	FeatureWidget->OpenAlliance(true);
	for (int32 Page = 0; Page < 3; ++Page)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("Alliance fixture page %d has a real tab button"),
				Page),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("AlliancePageHit_%d"), Page))));
	}
	TestNotNull(
		TEXT("Alliance joined preview is labeled as a local fixture"),
		FeatureWidget->GetWidgetFromName(TEXT("AllianceJoinedFixtureTag")));
	// Fixture loop: settings page -> quit preview -> welcome -> join list.
	if (UButton* AllianceSettingsTab = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AlliancePageHit_2"))))
	{
		AllianceSettingsTab->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
	}
	if (UButton* AllianceQuitPreview = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceSettingHit_5"))))
	{
		AllianceQuitPreview->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
	}
	TestNotNull(
		TEXT("Alliance fixture quit returns to the welcome page"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceWelcomeJoin"))));
	if (UButton* FixtureWelcomeJoin = Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceWelcomeJoin"))))
	{
		FixtureWelcomeJoin->OnClicked.Broadcast();
		FeatureWidget->FlushPendingActionForTesting();
	}
	TestNotNull(
		TEXT("Alliance fixture join list keeps a real row button"),
		Cast<UButton>(FeatureWidget->GetWidgetFromName(
			TEXT("AllianceJoinRowHit_0"))));
	TestNotNull(
		TEXT("Alliance fixture join list is labeled as a local fixture"),
		FeatureWidget->GetWidgetFromName(TEXT("AllianceJoinFixtureTag")));

	FeatureWidget->OpenTasks(true);
	TestNotNull(
		TEXT("Task screen has a real close button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("FeatureFrameClose"))));
	UScrollBox* TaskMainScroll = Cast<UScrollBox>(
		FeatureWidget->GetWidgetFromName(TEXT("TaskMainScroll")));
	TestNotNull(
		TEXT("Task main page uses the Unity-style vertical scroll list"),
		TaskMainScroll);
	TestNotNull(
		TEXT("Task main page has a 30px main-task tag"),
		Cast<UCanvasPanel>(
			FeatureWidget->GetWidgetFromName(TEXT("TaskMainTag"))));
	TestNotNull(
		TEXT("Task main page has a 30px side-task tag"),
		Cast<UCanvasPanel>(
			FeatureWidget->GetWidgetFromName(TEXT("TaskSideTag"))));
	for (int32 Page = 0; Page < 3; ++Page)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("Task page %d has a real tab button"),
				Page),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(
					*FString::Printf(TEXT("TaskPage_%d"), Page))));
	}
	for (int32 Row = 0; Row < 5; ++Row)
	{
		TestNotNull(
			*FString::Printf(TEXT("Task row %d has a real canvas"), Row),
			Cast<UCanvasPanel>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("TaskRow_%d"), Row))));
		TestNotNull(
			*FString::Printf(TEXT("Task row %d has a real title"), Row),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("TaskTitle_%d"), Row))));
		TestNotNull(
			*FString::Printf(TEXT("Task row %d has a real description"), Row),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("TaskDescription_%d"), Row))));
		TestNotNull(
			*FString::Printf(TEXT("Task row %d has a reward label"), Row),
			Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("TaskRewardLabel_%d"), Row))));
		TestNotNull(
			*FString::Printf(TEXT("Task row %d has a real action button"), Row),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("TaskAction_%d"), Row))));
	}
	TestNull(
		TEXT("Task normal content does not expose the old engineering-state banner"),
		FeatureWidget->GetWidgetFromName(TEXT("TaskCatalogStateBoundary")));
	if (TaskMainScroll)
	{
		TaskMainScroll->ScrollWidgetIntoView(
			FeatureWidget->GetWidgetFromName(TEXT("TaskRow_4")),
			true,
			EDescendantScrollDestination::IntoView);
	}

	// Fixture side rows must come from the Unity TaskProxy.GetTaskSideList
	// rule: first eligible unfinished task per group1 in SortId order, limit
	// taskSideShowLimit=5, group2 blocked by showLevelReq at bootstrap.
	const UTextBlock* FixtureSideFirst = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("TaskTitle_1")));
	if (TestNotNull(
		TEXT("Task fixture has the first side candidate title"),
		FixtureSideFirst))
	{
		TestEqual(
			TEXT("Task fixture first side candidate is Unity 301001 庄园之主"),
			FixtureSideFirst->GetText().ToString(),
			FString(TEXT("庄园之主")));
	}
	const UTextBlock* FixtureSideLast = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("TaskTitle_5")));
	if (TestNotNull(
		TEXT("Task fixture has the fifth side candidate title"),
		FixtureSideLast))
	{
		TestEqual(
			TEXT("Task fixture fifth side candidate is Unity 306001 村庄之友"),
			FixtureSideLast->GetText().ToString(),
			FString(TEXT("村庄之友")));
	}

	FeatureWidget->CloseScreen();
	TestFalse(TEXT("Feature screen closes"), FeatureWidget->IsScreenOpen());

	// Production has no server side-task state; the side section must stay on
	// the honest not-synced row instead of config-driven task rows.
	FeatureWidget->OpenTasks(false);
	TestNotNull(
		TEXT("Task production side section shows the not-synced state"),
		FeatureWidget->GetWidgetFromName(TEXT("TaskSideNotSynced")));
	TestNull(
		TEXT("Task production does not fabricate side task rows"),
		FeatureWidget->GetWidgetFromName(TEXT("TaskRow_1")));
	FeatureWidget->CloseScreen();
	TestFalse(TEXT("Task production screen closes"), FeatureWidget->IsScreenOpen());

	if (UWorld* FeatureWorld = FeatureGameInstance->GetWorld())
	{
		FeatureGameInstance->Shutdown();
		if (GEngine)
		{
			GEngine->DestroyWorldContext(FeatureWorld);
		}
	}
	return true;
}

#endif
