#if WITH_DEV_AUTOMATION_TESTS

#include "../RokGameNetSubsystem.h"
#include "../RokServerArmySubsystem.h"
#include "../RokServerHeroSubsystem.h"
#include "../RokSproto.h"
#include "../RokUiSpec.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityConfigSubsystem.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"
#include "../RokWorldMapEntityActor.h"
#include "../RokWorldMapHudWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

namespace
{
constexpr int64 GatherTargetId = 9007199254741001LL;
constexpr int32 InfantryId = 10101;
constexpr int32 SiegeId = 10401;

class FCheckGatherArmy final : public IAutomationLatentCommand
{
public:
	FCheckGatherArmy(FAutomationTestBase& InTest, bool bInBarbarian,
		TFunction<TFunction<void()>(UGameInstance*, const FRokWorldMapEntityRecord&)> InInstallFixture)
		: Test(InTest), bBarbarian(bInBarbarian), InstallFixture(MoveTemp(InInstallFixture))
	{
		FParse::Value(FCommandLine::Get(), TEXT("RokGatherArmyEvidence="), Output);
	}

	~FCheckGatherArmy()
	{
		if (RestoreNet) RestoreNet();
		if (PlayerArmy.IsValid()) PlayerArmy->ClearSnapshotForTesting();
		if (PlayerHeroes.IsValid()) PlayerHeroes->ClearSnapshotForTesting();
		if (Target.IsValid()) Target->Destroy();
	}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(FString::Printf(TEXT("World gather army timed out at step %d"), Step));
			return true;
		}
		if (!Screenshot.IsEmpty() && IFileManager::Get().FileSize(*Screenshot) <= 0) return false;
		if (Step == 0) return OpenFromWorld();
		if (!Test.TestNotNull(TEXT("Actual World feature screen"), Screen.Get())) return true;
		if (DispatchCreate.IsValid())
		{
			const auto* Shade = RokUiFind::Find(WorldHud, TEXT("WorldGatherQueueShade"));
			const auto* Slot = Shade ? Cast<UCanvasPanelSlot>(Shade->Slot) : nullptr;
			const auto* Design = Cast<USizeBox>(RokUiFind::Find(WorldHud, TEXT("WorldMapExpandedDesignSize")));
			if (!Test.TestTrue(TEXT("Dispatch layout exposes shade and viewport size"), Slot && Design)) return true;
			if (Slot->GetSize().Y == 7.0) return false;
			Test.TestTrue(TEXT("Dispatch resizes the shade height and preserves its Designer width"),
				Slot->GetSize().Equals(FVector2D(201, FMath::Max(100.0f, Design->GetHeightOverride() - 128.0f)), 0.01));
			auto* Create = DispatchCreate.Get();
			DispatchCreate.Reset();
			Create->OnClicked.Broadcast();
			if (bBarbarian)
			{
				const auto* Tip = Cast<UTextBlock>(RokUiFind::Find(WorldHud, TEXT("WorldGatherCreateTip")));
				if (!Test.TestTrue(TEXT("Insufficient action force keeps the dispatch panel and reports the original error"),
					!Screen->IsScreenOpen() && Create->IsVisible() && Tip && Tip->GetText().ToString().Contains(TEXT("行动力不足")))) return true;
				if (!Output.IsEmpty())
				{
					IFileManager::Get().MakeDirectory(*Output, true);
					Screenshot = Output / TEXT("low_action_force.png");
					FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
				}
				AttackRetry = Create;
			}
			return false;
		}
		if (AttackRetry.IsValid())
		{
			const auto Role = FixtureNet.IsValid() ? FixtureNet->GetRoleInfo() : nullptr;
			if (!Test.TestTrue(TEXT("Retry uses the same local role and dispatch button"), Role && AttackRetry->IsVisible())) return true;
			Role->SetNumberField(TEXT("actionForce"), ExpectedActionCost + 1);
			AttackRetry->OnClicked.Broadcast();
			AttackRetry.Reset();
			return false;
		}
		if (Screen->GetWindowAnimationForTesting().IsAnimating()) return false;

		switch (Step)
		{
		case 1:
		{
			if (!IsArtReady(TEXT("ExpeditionSoldierPortrait_10401"))) return false;
			auto* Troops = Cast<UScrollBox>(Find(TEXT("ExpeditionArmyTroops")));
			if (!Test.TestNotNull(TEXT("Create army opens the troop list through the World HUD"), Troops)) return true;
			const auto* Page = Troops->GetTypedOuter<URokUiPageWidget>();
			Test.TestTrue(TEXT("Expected main layout selected"), URokUiSettings::IsLegacyForced()
				? !Page : Page && Page->GetClass()->GetName() == TEXT("WBP_RokFeature_GatherArmy_C"));
			const auto* Slider = Find(TEXT("ExpeditionSoldierSlider_10401"));
			const auto* Row = Slider ? Slider->GetTypedOuter<URokUiPageWidget>() : nullptr;
			Test.TestTrue(TEXT("Gather soldier row uses its own layout"), Slider && (URokUiSettings::IsLegacyForced()
				? !Row : Row && Row->GetClass()->GetName() == TEXT("WBP_RokFeature_GatherArmySoldierRow_C")));
			const TCHAR* ExpectedVerb = bBarbarian ? TEXT("出征") : TEXT("行军");
			const FString ExpectedAttributes = bBarbarian
				? FString::Printf(TEXT("进攻乱军   ·   行动力 %d"), ExpectedActionCost) : TEXT("行军速度   ·   采集负载");
			Test.TestEqual(TEXT("Selected entry supplies the original action verb"), Text(TEXT("GatherMarchLabel")), FString(ExpectedVerb));
			Test.TestEqual(TEXT("Composer uses the expected source attributes"), Text(TEXT("GatherArmyAttributes")), ExpectedAttributes);
			Test.TestTrue(TEXT("March uses preview quantities and travel time"),
				Text(TEXT("ExpeditionArmyCount")).Contains(TEXT("/")) && Text(TEXT("GatherMarchTime")) != TEXT("—"));
			if (!Capture(TEXT("main"))) return true;
			break;
		}
		case 2:
			if (!Click(TEXT("GatherDeputy"))) return true;
			break;
		case 3:
		{
			if (!IsArtReady(TEXT("ExpeditionHeroChoiceIcon_1"))) return false;
			Test.TestNull(TEXT("Current main is excluded from deputy choices"), Find(TEXT("ExpeditionHeroChoiceButton_0")));
			const auto* Button = Find(TEXT("ExpeditionHeroChoiceButton_1"));
			const auto* Row = Button ? Button->GetTypedOuter<URokUiPageWidget>() : nullptr;
			Test.TestTrue(TEXT("Editable choice retains action index"), Button && (URokUiSettings::IsLegacyForced()
				? !Row : Row && Row->GetClass()->GetName() == TEXT("WBP_RokFeature_GatherArmyHeroChoiceRow_C")));
			if (!Capture(TEXT("deputy_picker"))) return true;
			break;
		}
		case 4:
			if (!Click(TEXT("ExpeditionHeroChoiceButton_1"))) return true;
			break;
		case 5:
			if (!IsArtReady(TEXT("GatherDeputyHead"))) return false;
			Test.TestNotNull(TEXT("Selected deputy badge"), Find(TEXT("GatherDeputyHead")));
			if (!Capture(TEXT("deputy_selected"))) return true;
			break;
		case 6:
			if (!Click(TEXT("ExpeditionArmySwitchHero"))) return true;
			break;
		case 7:
			Test.TestNull(TEXT("Current deputy is excluded from main choices"), Find(TEXT("ExpeditionHeroChoiceButton_1")));
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			break;
		case 8:
			if (!Click(TEXT("GatherDeputy"))) return true;
			break;
		case 9:
		{
			const auto* Scroll = Cast<UScrollBox>(Find(TEXT("ExpeditionHeroChoiceScroll")));
			auto* None = Scroll ? Cast<UButton>(Scroll->GetChildAt(0)) : nullptr;
			if (!Test.TestTrue(TEXT("First deputy choice is the real remove action"),
				None && None->GetName().StartsWith(TEXT("GatherRemoveDeputy")) && None->GetIsEnabled())) return true;
			Test.AddInfo(FString::Printf(TEXT("Direct deputy choice button in chooser: %s"), *None->GetName()));
			None->OnClicked.Broadcast();
			Screen->FlushPendingActionForTesting();
			const auto* Badge = Find(TEXT("GatherDeputyHead"));
			Test.TestTrue(TEXT("Removing the deputy returns to composer without a deputy badge"),
				Find(TEXT("ExpeditionArmyTroops")) && (!Badge || !Badge->IsVisible()));
			break;
		}
		case 10:
			if (!Click(TEXT("GatherArmyHelp"))) return true;
			Test.TestTrue(TEXT("March help is handled by the original action"), Text(TEXT("GatherArmyFeedback")).Contains(TEXT("基础速度")));
			if (!Click(TEXT("ExpeditionArmyClear"))) return true;
			break;
		case 11:
		{
			auto* Slider = Cast<USlider>(Find(TEXT("ExpeditionSoldierSlider_10401")));
			auto* Fill = Cast<UProgressBar>(Find(TEXT("ExpeditionSoldierFill_10401")));
			const auto* Ready = Find(TEXT("ExpeditionArmyReady"));
			if (!Test.TestTrue(TEXT("Clearing counts disables dispatch"), Slider && Fill && Ready
				&& Slider->GetValue() == 0 && Fill->GetPercent() == 0 && !Ready->GetIsEnabled())) return true;
			Slider->SetValue(25);
			Slider->OnValueChanged.Broadcast(25);
			Test.TestEqual(TEXT("Slider updates the displayed troop count"), Text(TEXT("ExpeditionSoldierQuantity_10401")), FString(TEXT("25")));
			Test.TestTrue(TEXT("Progress and draft capacity update after slider input"), Fill->GetPercent() > 0 && Ready->GetIsEnabled());
			// Close from the real frame button. The dispatch action is never fired.
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			Test.TestFalse(TEXT("Gather composer closes"), Screen->IsScreenOpen());
			return true;
		}
		default:
			return true;
		}
		++Step;
		return false;
	}

private:
	bool OpenFromWorld()
	{
		for (TObjectIterator<URokWorldMapHudWidget> Hud; Hud; ++Hud)
		{
			UWorld* World = Hud->GetWorld();
			if (!Hud->IsInViewport() || !World || World->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")) continue;
			auto* Instance = World->GetGameInstance();
			FixtureNet = Instance->GetSubsystem<URokGameNetSubsystem>();
			auto* Entities = Instance->GetSubsystem<URokWorldMapEntitySubsystem>();
			if (!Entities || !Entities->IsPopulationLoaded()) return false;
			const auto* Source = Entities->GetEntities().FindByPredicate([this](const auto& Row)
			{
				return Row.Kind == (bBarbarian ? TEXT("barbarian") : TEXT("food"));
			});
			if (!Test.TestNotNull(TEXT("Source world-map entity"), Source)) return true;
			RestoreNet = InstallFixture(Instance, *Source);
			if (!RestoreNet) return true;
			PlayerArmy = Instance->GetSubsystem<URokServerArmySubsystem>();
			PlayerHeroes = Instance->GetSubsystem<URokServerHeroSubsystem>();
			auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
			const auto* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
			if (!Test.TestTrue(TEXT("Offline fixture uses existing runtime catalogs"), PlayerArmy.IsValid() && PlayerHeroes.IsValid()
				&& Catalog && Runtime)) return true;
			FRokServerArmySnapshot Army;
			Army.bAvailable = Army.bLoaded = Army.bCityKnown = Army.bDispatchUsedKnown = Army.bDispatchCapacityKnown = true;
			Army.DispatchSlotsMax = 2;
			Army.CityCount = 600;
			for (int32 Id : {InfantryId, SiegeId})
			{
				auto& Stack = Army.CitySoldiers.AddDefaulted_GetRef();
				Stack.Id = Id;
				Stack.Count = 300;
			}
			PlayerArmy->SetSnapshotForTesting(Army);
			FRokServerHeroSnapshot Heroes;
			Heroes.bAvailable = Heroes.bLoaded = Heroes.bHeroesKnown = Heroes.bRowsComplete = true;
			for (const auto& Candidate : Runtime->GetStarterHeroes())
			{
				FRokUnityCatalogHero Hero;
				if (!Catalog->GetHero(Candidate.HeroId, Hero) || Hero.IconWidth <= 0 || Hero.SkillIds.IsEmpty()) continue;
				auto& Owned = Heroes.Heroes.AddDefaulted_GetRef();
				Owned.HeroId = Hero.Id;
				Owned.Rare = Hero.Rare;
				Owned.Level = 1;
				Owned.Star = 3;
				Owned.Power = Hero.Score;
				Owned.bCatalogKnown = Owned.bLevelKnown = Owned.bStarKnown = Owned.bPowerKnown = true;
				for (int32 SkillId : Hero.SkillIds)
				{
					auto& Skill = Owned.Skills.AddDefaulted_GetRef();
					Skill.SkillId = SkillId;
					Skill.Level = 1;
					Skill.bLevelKnown = true;
				}
				if (Heroes.Heroes.Num() == 2) break;
			}
			if (!Test.TestEqual(TEXT("Two distinct offline commander records"), Heroes.Heroes.Num(), 2)) return true;
			PlayerHeroes->SetSnapshotForTesting(Heroes);
			Target = World->SpawnActor<ARokWorldMapEntityActor>();
			auto Resource = *Source;
			Resource.ServerObjectId = GatherTargetId;
			Resource.ServerObjectType = bBarbarian ? 2 : 4;
			Resource.ResourceAmount = 18000;
			Resource.ResourceCapacity = 20000;
			Resource.CollectorRoleId = 0;
			if (!Test.TestTrue(TEXT("World fixture preserves source entity art"), Target.IsValid() && Target->Configure(Resource, 200.0f))) return true;
			Hud->ShowWorldObjectInfo(Target.Get());
			const FName OpenAction(bBarbarian ? TEXT("WorldBarbarianAttack") : TEXT("WorldObjectInfoGatherButton"));
			auto* Open = Cast<UButton>(RokUiFind::Find(*Hud, OpenAction));
			if (!Test.TestTrue(TEXT("Original selected world object action enabled"), Open && Open->GetIsEnabled())) return true;
			if (bBarbarian)
			{
				FRokBarbarianAttackInfo Attack;
				if (!Test.TestTrue(TEXT("Real Monster catalog supplies a positive cost and zero current action force"),
					PlayerArmy->GetBarbarianAttackInfo(GatherTargetId, Attack) && Attack.ActionCost > 0
					&& Attack.ActionForce == 0 && Attack.BlockReason.IsEmpty())) return true;
				ExpectedActionCost = Attack.ActionCost;
				auto* Stay = Cast<UCheckBox>(RokUiFind::Find(*Hud, TEXT("WorldBarbarianStay")));
				if (!Test.TestTrue(TEXT("Real barbarian station option is available"), Stay && Stay->IsVisible())) return true;
				Stay->SetIsChecked(true);
			}
			Open->OnClicked.Broadcast();
			auto* Create = Cast<UButton>(RokUiFind::Find(*Hud, TEXT("WorldGatherCreateArmy")));
			if (!Test.TestTrue(TEXT("Original dispatch panel opens with create army enabled"), Create && Create->GetIsEnabled() && Create->IsVisible())) return true;
			Screen = Cast<URokUnityFeatureScreenWidget>(RokUiFind::Find(*Hud, TEXT("WorldMapUnityFeatureScreenWidget")));
			auto* Shade = RokUiFind::Find(*Hud, TEXT("WorldGatherQueueShade"));
			auto* ShadeSlot = Shade ? Cast<UCanvasPanelSlot>(Shade->Slot) : nullptr;
			if (!Test.TestNotNull(TEXT("Dispatch panel queue shade"), ShadeSlot)) return true;
			ShadeSlot->SetSize(FVector2D(201, 7));
			WorldHud = *Hud;
			DispatchCreate = Create;
			++Step;
			return false;
		}
		return false;
	}

	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }

	FString Text(FName Name) const
	{
		const auto* Label = Cast<UTextBlock>(Find(Name));
		return Label ? Label->GetText().ToString() : FString();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Find(Name));
		if (!Test.TestTrue(FString::Printf(TEXT("Original action %s is available"), *Name.ToString()),
			Button && Button->IsVisible() && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	bool Capture(const TCHAR* State)
	{
		if (!bBarbarian && URokUiSettings::IsLegacyForced() && FParse::Param(FCommandLine::Get(), TEXT("RokGatherArmyExport")))
		{
			const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Specs"));
			IFileManager::Get().MakeDirectory(*Directory, true);
			const auto* Mode = Cast<UCanvasPanel>(Find(TEXT("FeatureModeCanvas")));
			FString Error;
			if (!Test.TestTrue(TEXT("Export from native gathered layout"), Mode && RokUiSpec::ExportChildren(Mode,
				Directory / FString::Printf(TEXT("gather_army_reference_%s.json"), State), State, Error)))
			{
				if (!Error.IsEmpty()) Test.AddError(Error);
				return false;
			}
		}
		if (!Output.IsEmpty())
		{
			IFileManager::Get().MakeDirectory(*Output, true);
			Screenshot = Output / FString::Printf(TEXT("%s.png"), State);
			FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
		}
		return true;
	}

	bool IsArtReady(FName Name)
	{
		const auto* Image = Cast<UImage>(Find(Name));
		const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
		const bool bReady = Texture && !Texture->IsCompiling() && Texture->GetResource()
			&& Texture->GetResource()->TextureRHI.IsValid() && Image->GetCachedGeometry().GetLocalSize().X > 0;
		StableFrames = bReady ? StableFrames + 1 : 0;
		if (StableFrames < 3) return false;
		StableFrames = 0;
		return true;
	}

	FAutomationTestBase& Test;
	const bool bBarbarian;
	TFunction<TFunction<void()>(UGameInstance*, const FRokWorldMapEntityRecord&)> InstallFixture;
	TFunction<void()> RestoreNet;
	TWeakObjectPtr<URokGameNetSubsystem> FixtureNet;
	TWeakObjectPtr<URokServerArmySubsystem> PlayerArmy;
	TWeakObjectPtr<URokServerHeroSubsystem> PlayerHeroes;
	TWeakObjectPtr<ARokWorldMapEntityActor> Target;
	TWeakObjectPtr<URokWorldMapHudWidget> WorldHud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<UButton> DispatchCreate;
	TWeakObjectPtr<UButton> AttackRetry;
	FString Output;
	FString Screenshot;
	const double Deadline = FPlatformTime::Seconds() + 70.0;
	int32 Step = 0;
	int32 StableFrames = 0;
	int32 ExpectedActionCost = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGatherArmyRenderedTest, "Rok.Ui.World.GatherArmyRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRokGatherArmyRenderedTest::RunTest(const FString&)
{
	if (!TestTrue(TEXT("Fixture uses the offline offscreen world map"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokNoNet")) && FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"))))
	{
		return false;
	}
	const bool bBarbarian = FParse::Param(FCommandLine::Get(), TEXT("RokGatherArmyBarbarian"));
	auto Install = [this, bBarbarian](UGameInstance* Instance, const FRokWorldMapEntityRecord& Source) -> TFunction<void()>
	{
		auto* Net = Instance->GetSubsystem<URokGameNetSubsystem>();
		if (!TestTrue(TEXT("Offline fixture has no transport or pending dispatch"), Net && !Net->Worker && !Net->PendingArmyDispatch.IsSet())) return {};
		int64 MonsterId = 0;
		if (bBarbarian)
		{
			auto* Config = Instance->GetSubsystem<URokUnityConfigSubsystem>();
			const auto* Monsters = Config ? Config->GetTableRows(TEXT("Monster")) : nullptr;
			if (!TestNotNull(TEXT("Actual Monster catalog loaded"), Monsters)) return {};
			for (const auto& Row : *Monsters)
			{
				int64 Type = 0, Level = 0;
				if (RokSprotoJson::TryGetInt64(Row, TEXT("type"), Type) && Type == 1
					&& RokSprotoJson::TryGetInt64(Row, TEXT("level"), Level) && Level == Source.Level
					&& RokSprotoJson::TryGetInt64(Row, TEXT("ID"), MonsterId) && MonsterId > 0)
				{
					break;
				}
			}
			if (!TestTrue(TEXT("AOI monsterId comes from Monster at the template level"), MonsterId > 0)) return {};
		}
		const auto PreviousState = Net->State;
		const auto PreviousMap = Net->WorldMapObjects;
		const auto PreviousRole = Net->MergedState;
		const auto Role = MakeShared<FJsonObject>();
		Role->SetNumberField(TEXT("level"), 5);
		if (bBarbarian)
		{
			Role->SetNumberField(TEXT("actionForce"), 0);
			Role->SetNumberField(TEXT("barbarianLevel"), Source.Level);
			Role->SetBoolField(TEXT("situStation"), false);
		}
		const auto RoleMessage = MakeShared<FJsonObject>();
		RoleMessage->SetObjectField(TEXT("roleInfo"), Role);
		Net->MergedState = MakeShared<FJsonObject>();
		Net->MergedState->SetObjectField(TEXT("Role_RoleInfo"), RoleMessage);
		const auto Resource = MakeShared<FJsonObject>();
		Resource->SetField(TEXT("objectId"), MakeShared<FJsonValueNumberString>(LexToString(GatherTargetId)));
		Resource->SetNumberField(TEXT("objectType"), bBarbarian ? 2 : 4);
		if (bBarbarian) Resource->SetNumberField(TEXT("monsterId"), MonsterId);
		Net->WorldMapObjects = MakeShared<FJsonObject>();
		Net->WorldMapObjects->SetObjectField(LexToString(GatherTargetId), Resource);
		const auto Session = Net->SprotoSession;
		Net->State = ERokGameNetState::RoleReady;
		return [this, WeakNet = TWeakObjectPtr<URokGameNetSubsystem>(Net), PreviousState, PreviousMap, PreviousRole, Session]()
		{
			if (auto* FixtureNet = WeakNet.Get())
			{
				TestTrue(TEXT("No army request was sent"), !FixtureNet->Worker && !FixtureNet->PendingArmyDispatch.IsSet()
					&& Session == FixtureNet->SprotoSession);
				FixtureNet->State = PreviousState;
				FixtureNet->WorldMapObjects = PreviousMap;
				FixtureNet->MergedState = PreviousRole;
			}
		};
	};
	ADD_LATENT_AUTOMATION_COMMAND(FCheckGatherArmy(*this, bBarbarian, MoveTemp(Install)));
	return true;
}

#endif
