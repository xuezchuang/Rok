#if WITH_DEV_AUTOMATION_TESTS
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokGameNetSubsystem.h"
#include "../RokServerArmySubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokWorldMapEntityActor.h"
#include "../RokWorldMapHudWidget.h"
#include "../RokWorldMapPlayerController.h"

namespace
{
int64 Integer(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field)
{
	int64 Value = 0;
	RokSprotoJson::TryGetInt64(Row, Field, Value);
	return Value;
}

class FRunLiveBarbarianBattle final : public IAutomationLatentCommand
{
public:
	FRunLiveBarbarianBattle(FAutomationTestBase& InTest, FString InOutput, int64 ReopenTarget)
		: Test(InTest), Output(MoveTemp(InOutput)), TargetId(ReopenTarget), Step(ReopenTarget ? 10 : 0) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now > Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Live fight timed out at step %d; dispatch submitted=%d, combat seen=%d, damage seen=%d."),
				Step, Step >= 4, bSawCombat, bSawDamage));
			return true;
		}
		if (!Hud.IsValid())
		{
			for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
				if (It->IsInViewport()) { Hud = *It; break; }
			if (!Hud.IsValid()) return false;
		}
		auto* Instance = Hud->GetGameInstance();
		auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
		auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
		auto* Controller = Cast<ARokWorldMapPlayerController>(Hud->GetOwningPlayer());
		if (!Net || !Army || !Controller) return false;
		if (Net->GetState() == ERokGameNetState::Failed || Net->IsSnapshotReplay())
		{
			Test.AddError(TEXT("Live server session failed: ") + Net->GetLastError());
			return true;
		}
		if (Net->GetState() != ERokGameNetState::RoleReady) return false;
		Screen = Cast<URokUnityFeatureScreenWidget>(Hud->GetWidgetFromName(TEXT("WorldMapUnityFeatureScreenWidget")));
		if (!Screen.IsValid()) return false;

		switch (Step)
		{
		case 10:
			if (Net->GetBattleReportMail().IsEmpty() || !Net->GetRoleInfo()) return false;
			if (!Click(Hud.Get(), TEXT("WorldMapMailButton")) || !Click(Screen.Get(), TEXT("MailTab_1"))) return true;
			++Step;
			return false;
		case 11:
		{
			for (const auto& Pair : Net->GetBattleReportMail())
			{
				const TSharedPtr<FJsonObject>* Report = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Participants = nullptr;
				if (!Pair.Value->TryGetObjectField(TEXT("battleReportExContent"), Report)
					|| !(*Report)->TryGetArrayField(TEXT("objectInfos"), Participants)) continue;
				if (Participants->ContainsByPredicate([this](const auto& Value)
					{ return Value && Value->Type == EJson::Object && Integer(Value->AsObject(), TEXT("objectIndex")) == TargetId; }))
				{
					ReportIndex = Pair.Key;
					break;
				}
			}
			if (!ReportIndex) return false;
			const auto* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("BattleReportTitle")));
			const auto* Detail = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("BattleReportDuration")));
			if (!Detail) return false;
			Test.TestTrue(TEXT("The persisted report shows the victory title"), Title && Title->GetText().ToString() == TEXT("战斗胜利"));
			Test.TestTrue(TEXT("The persisted report uses map coordinates"), Detail->GetText().ToString().Contains(TEXT("X:1148 Y:154")));
			const auto Role = Net->GetRoleInfo();
			const auto ArmyState = Army->GetSnapshot();
			Test.TestTrue(TEXT("Barbarian progression persists after reconnecting"), Integer(Role, TEXT("barbarianLevel")) >= 1);
			Test.AddInfo(FString::Printf(TEXT("Reopened battle report %lld. Barbarian level=%lld, AP=%lld, field armies=%d."),
				ReportIndex, Integer(Role, TEXT("barbarianLevel")), Integer(Role, TEXT("actionForce")), ArmyState.Marches.Num()));
			CaptureAt = Now + 2.0;
			++Step;
			return false;
		}
		case 12:
			if (Now < CaptureAt) return false;
			Capture(TEXT("LiveBattleReport"));
			++Step;
			return false;
		case 0:
		{
			if (!Net->GetRoleInfo() || !Army->GetSnapshot().bAvailable) return false;
			if (!Click(Hud.Get(), TEXT("WorldMapSearchButton"))) return true;
			Screen->OpenResourceSearch(1);
			if (!Click(Screen.Get(), TEXT("ResourceSearchSubmit"))) return true;
			++Step;
			return false;
		}
		case 1:
		{
			const auto* Target = Cast<ARokWorldMapEntityActor>(Controller->GetSelectedWorldMapActor());
			if (!Target || Screen->IsResourceSearchOpen()) return false;
			FRokBarbarianAttackInfo Info;
			FVector2D Point;
			if (!Test.TestTrue(TEXT("The search resolved an attackable live level-1 barbarian"),
				LexTryParseString(TargetId, *Target->GetServerObjectId()) && Army->GetBarbarianAttackInfo(TargetId, Info)
				&& Info.Level == 1 && Info.BlockReason.IsEmpty() && Info.ActionForce >= Info.ActionCost)) return true;
			StartForce = Info.ActionForce;
			StartLevel = Integer(Net->GetRoleInfo(), TEXT("barbarianLevel"));
			for (const auto& Pair : Net->GetBattleReportMail()) PriorReports.Add(Pair.Key);
			if (!Test.TestTrue(TEXT("Map click selects the level-1 enemy"),
				Controller->ProjectWorldLocationToScreen(Target->GetSelectionAnchorWorldPosition(), Point, true)
				&& Controller->SelectWorldMapActorAtScreenPosition(Point))) return true;
			if (auto* Stay = Cast<UCheckBox>(Hud->GetWidgetFromName(TEXT("WorldBarbarianStay")))) Stay->SetIsChecked(false);
			if (!Click(Hud.Get(), TEXT("WorldBarbarianAttack"))) return true;
			++Step;
			return false;
		}
		case 2:
			if (!Click(Hud.Get(), TEXT("WorldGatherSelectNew")) || !Click(Hud.Get(), TEXT("WorldGatherCreateArmy"))) return true;
			++Step;
			return false;
		case 3:
			if (!Test.TestTrue(TEXT("Army composer opened for the enemy"), Screen->IsScreenOpen())) return true;
			++Step; // Only one attempt. Never repeat the live submit after a timeout.
			return !Click(Screen.Get(), TEXT("ExpeditionArmyReady"));
		case 4:
		{
			for (const auto& March : Army->GetSnapshot().Marches)
			{
				if (March.TargetObjectId != TargetId) continue;
				if (!bSawDispatch && March.TargetType == 1)
				{
					bSawDispatch = true;
					Test.AddInfo(FString::Printf(TEXT("Server confirmed attack: target=%lld army=%d soldiers=%lld."), TargetId, March.ArmyIndex, March.Count));
				}
				bSawCombat |= March.Phase == ERokServerMarchPhase::Battling;
			}
			int64 Damage = 0;
			double Age = 0;
			if (Net->GetRecentBattleDamage(TargetId, Damage, Age) && !bSawDamage)
			{
				bSawDamage = true;
				Capture(TEXT("LiveBattle"));
				Test.AddInfo(FString::Printf(TEXT("Live battle damage received: %lld."), Damage));
			}
			for (const auto& Pair : Net->GetBattleReportMail())
			{
				if (PriorReports.Contains(Pair.Key)) continue;
				ReportIndex = Pair.Key;
				break;
			}
			if (!ReportIndex) return false;
			Test.TestTrue(TEXT("An attack army was confirmed by the live server"), bSawDispatch);
			if (!Click(Hud.Get(), TEXT("WorldMapMailButton")) || !Click(Screen.Get(), TEXT("MailTab_1"))) return true;
			++Step;
			return false;
		}
		case 5:
		{
			const auto* Email = Net->GetBattleReportMail().Find(ReportIndex);
			const TSharedPtr<FJsonObject>* Report = nullptr;
			if (!Email || !(*Email)->TryGetObjectField(TEXT("battleReportExContent"), Report)) return false;
			const TArray<TSharedPtr<FJsonValue>>* Armies = nullptr;
			if (!Test.TestTrue(TEXT("The server sent participant details"), (*Report)->TryGetArrayField(TEXT("objectInfos"), Armies))) return true;
			TSharedPtr<FJsonObject> Mine, Enemy;
			for (const auto& Value : *Armies)
			{
				if (!Value || Value->Type != EJson::Object) continue;
				const auto& Row = Value->AsObject();
				if (Integer(Row, TEXT("rid")) == Net->GetRoleId()) Mine = Row;
				if (Integer(Row, TEXT("objectIndex")) == TargetId) Enemy = Row;
			}
			if (!Test.TestTrue(TEXT("Live battle mail belongs to the attacked target and role"), Mine.IsValid() && Enemy.IsValid())) return true;
			Test.TestEqual(TEXT("Our army won"), Integer(*Report, TEXT("winObjectIndex")), Integer(Mine, TEXT("objectIndex")));
			const auto Role = Net->GetRoleInfo();
			const int64 Force = Integer(Role, TEXT("actionForce"));
			const int64 Level = Integer(Role, TEXT("barbarianLevel"));
			Test.TestTrue(TEXT("The server charged action force"), Force < StartForce);
			Test.TestTrue(TEXT("The server persisted barbarian progression"), Level >= FMath::Max<int64>(StartLevel, 1));
			Test.AddInfo(FString::Printf(TEXT("Settlement: template=%lld reportStatus=%lld rounds source=%d own troops=%lld->%lld enemy=%lld->%lld AP=%lld->%lld barbarian level=%lld->%lld exp=%lld combat=%d damage=%d"),
				Integer(*Email, TEXT("emailId")), Integer(*Email, TEXT("reportStatus")), (*Report)->HasField(TEXT("battleReport")),
				Integer(Mine, TEXT("beginArmyCount")), Integer(Mine, TEXT("endArmyCount")),
				Integer(Enemy, TEXT("beginArmyCount")), Integer(Enemy, TEXT("endArmyCount")),
				StartForce, Force, StartLevel, Level, Integer(*Report, TEXT("mainHeroExp")), bSawCombat, bSawDamage));
			if (!Click(Screen.Get(), FName(*FString::Printf(TEXT("BattleReportRow_%lld"), ReportIndex)))) return true;
			Test.TestNotNull(TEXT("The live report is rendered in Mail > Reports"), Screen->GetWidgetFromName(TEXT("BattleReportDuration")));
			Capture(TEXT("LiveBattleReport"));
			++Step;
			return false;
		}
		default:
			return IFileManager::Get().FileSize(*(Output / TEXT("LiveBattleReport.png"))) > 0;
		}
	}

private:
	bool Click(UUserWidget* Widget, FName Name)
	{
		auto* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
		if (!Test.TestTrue(Name.ToString(), Button && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		if (Widget == Screen.Get()) Screen->FlushPendingActionForTesting();
		return true;
	}

	void Capture(const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(Output / (FString(Name) + TEXT(".png")), true, false);
	}

	FAutomationTestBase& Test;
	FString Output;
	TWeakObjectPtr<URokWorldMapHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TSet<int64> PriorReports;
	const double Deadline = FPlatformTime::Seconds() + 230;
	double CaptureAt = 0.0;
	int64 TargetId = 0, ReportIndex = 0, StartForce = 0, StartLevel = 0;
	int32 Step = 0;
	bool bSawDispatch = false, bSawCombat = false, bSawDamage = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLiveBarbarianBattleTest, "Rok.Live.Barbarian.LevelOneBattle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokLiveBarbarianBattleTest::RunTest(const FString&)
{
	FString Output;
	int64 ReopenTarget = 0;
	const bool bReopen = FParse::Value(FCommandLine::Get(), TEXT("RokReopenLiveBarbarian="), Output)
		&& FParse::Value(FCommandLine::Get(), TEXT("RokReopenLiveBarbarianTarget="), ReopenTarget) && ReopenTarget > 0;
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokNet"))
		|| (!bReopen && !FParse::Value(FCommandLine::Get(), TEXT("RokVerifyOneLiveBarbarian="), Output)) || Output.IsEmpty())
	{
		AddInfo(TEXT("Live write skipped. Explicit -RokNet and -RokVerifyOneLiveBarbarian=<output> required."));
		return true;
	}
	IFileManager::Get().MakeDirectory(*Output, true);
	ADD_LATENT_AUTOMATION_COMMAND(FRunLiveBarbarianBattle(*this, Output, bReopen ? ReopenTarget : 0));
	return true;
}
#endif
