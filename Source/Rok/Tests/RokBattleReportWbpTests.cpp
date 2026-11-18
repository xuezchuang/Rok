#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUiWidgets.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryWriter.h"
#include "Slate/WidgetRenderer.h"
#include "RokBattleReportWbpFixture.h"

namespace
{
TSharedPtr<FJsonObject> ParseObject(const TCHAR* Source)
{
	TSharedPtr<FJsonObject> Result;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Result);
	return Result;
}

TMap<int64, TSharedPtr<FJsonObject>> MakeMails(int32 Count, bool bWithReport)
{
	TMap<int64, TSharedPtr<FJsonObject>> Result;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int64 Id = 501 + Index;
		FString Json = FString::Printf(
			TEXT("{\"emailId\":10,\"sendTime\":%d,\"status\":0,\"reportSubTile\":[\"\",\"\",\"\",\"\",\"\",\"对手%d\"]"),
			10 - Index, Index + 1);
		if (bWithReport && Index == 0)
		{
			Json = FString::Printf(TEXT(
				"{\"emailId\":10,\"sendTime\":%d,\"status\":0,\"reportSubTile\":[\"\",\"\",\"\",\"\",\"\",\"对手%d\"],"
				"\"battleReportExContent\":{\"battleBeginTime\":100,\"battleEndTime\":145,\"mainHeroExp\":50,\"deputyHeroExp\":25,"
				"\"objectInfos\":["
				"{\"rid\":1,\"objectIndex\":1,\"name\":\"我方\",\"beginArmyCount\":120,\"endArmyCount\":80,\"maxArmyCount\":120,\"soldierDetail\":[{\"battleSoldierHurt\":[{\"minor\":10,\"hardHurt\":20,\"die\":10}]}]},"
				"{\"rid\":2,\"objectIndex\":2,\"name\":\"敌方\",\"beginArmyCount\":90,\"endArmyCount\":20,\"monsterId\":0,\"pos\":{\"x\":100,\"y\":200}}],"
				"\"rewardInfo\":{\"food\":100,\"wood\":20,\"stone\":0,\"gold\":0,\"denar\":0,\"items\":[{\"itemId\":1,\"itemNum\":2}]},"
				"\"battleReport\":[{\"battleDamageHeal\":[{\"attackIndex\":1,\"defenseIndex\":2,\"turn\":1,\"reportUniqueIndex\":1,\"attackArmyCount\":100,\"defenseArmyCount\":80}]}]}}"),
				10 - Index, Index + 1);
		}
		Result.Add(Id, ParseObject(*Json));
	}
	return Result;
}

bool OpenReportTab(URokUnityFeatureScreenWidget& Screen)
{
	Screen.OpenMail(false);
	UButton* Tab = Cast<UButton>(Screen.GetWidgetFromName(TEXT("MailTab_1")));
	if (!Tab) return false;
	Tab->OnClicked.Broadcast();
	Screen.FlushPendingActionForTesting();
	return true;
}

URokUnityFeatureScreenWidget* NewScreen(UWorld* World,
	const TMap<int64, TSharedPtr<FJsonObject>>& Mails, const TSet<int64>& Failed = {})
{
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!Screen || !Screen->Initialize()) return nullptr;
	Screen->TakeWidget();
	RokSetBattleReportFixtureForTest(Screen, Mails, Failed, 1);
	return Screen;
}

void CaptureBattleReportState(FAutomationTestBase& Test, const TSharedRef<SWidget>& SlateRoot, const TCHAR* State)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokBattleReportCapture"))) return;

	const bool bLegacy = URokUiSettings::IsLegacyForced();
	const FString Mode = bLegacy ? TEXT("legacy") : TEXT("wbp");
	const FString CaptureRoot = FPaths::ProjectSavedDir() / TEXT("RokUi/Captures/BattleReport") / Mode;
	IFileManager::Get().MakeDirectory(*CaptureRoot, true);
	const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString Path = CaptureRoot / FString::Printf(TEXT("%s_%s.png"), State, *Stamp);

	FWidgetRenderer Renderer(true);
	UTextureRenderTarget2D* RenderTarget = Renderer.DrawWidget(SlateRoot, FVector2D(1440, 810));
	FlushRenderingCommands();
	TArray<uint8> Png;
	FMemoryWriter Writer(Png);
	const bool bSaved = RenderTarget
		&& FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, Writer)
		&& FFileHelper::SaveArrayToFile(Png, *Path);
	Test.TestTrue(FString::Printf(TEXT("%s %s fixture renders to PNG"), *Mode, State), bSaved);
	if (bSaved) Test.AddInfo(FString::Printf(TEXT("Battle report capture: %s"), *Path));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBattleReportWbpTest, "Rok.Ui.BattleReport.LocalFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokBattleReportWbpTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokBattleReportWbpTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT
	{
		Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
	};

	{
		auto* Screen = NewScreen(World, {});
		if (!TestNotNull(TEXT("Empty fixture screen initializes"), Screen) || !TestTrue(TEXT("Empty report tab opens"), OpenReportTab(*Screen))) return false;
		const TSharedRef<SWidget> SlateRoot = Screen->TakeWidget();
		if (URokUiSettings::IsLegacyForced())
			TestNotNull(TEXT("Legacy empty detail panel is present"), RokUiFind::Find(Screen, TEXT("BattleReportDetailPanel")));
		else
			TestNotNull(TEXT("Generated battle-report WBP page is mounted"), RokUiFind::Find(Screen, TEXT("BattleReportDetailCanvas")));
		const auto* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("MailListScroll")));
		const auto* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
		if (!TestTrue(TEXT("The battle reports use the shared mail list"), RokUiFind::IsInside(List, Mode))) return false;
		TestEqual(TEXT("The report list uses the expected mail presentation"), List->GetParent() == Mode,
			URokUiSettings::IsLegacyForced());
		TestNull(TEXT("The ordinary empty panel is absent from reports"), Screen->GetWidgetFromName(TEXT("MailEmptyState")));
		TestNotNull(TEXT("Empty message is rendered"), RokUiFind::Find(Screen, TEXT("BattleReportEmpty")));
		CaptureBattleReportState(*this, SlateRoot, TEXT("empty"));
		RokClearBattleReportFixtureForTest(Screen);
	}

	{
		auto* Screen = NewScreen(World, MakeMails(1, false));
		if (!TestNotNull(TEXT("Loading fixture screen initializes"), Screen) || !TestTrue(TEXT("Loading report tab opens"), OpenReportTab(*Screen))) return false;
		const TSharedRef<SWidget> SlateRoot = Screen->TakeWidget();
		const auto* Pending = Cast<UTextBlock>(RokUiFind::Find(Screen, TEXT("BattleReportPending")));
		TestTrue(TEXT("Missing report content shows loading state"), Pending && Pending->GetText().ToString().Contains(TEXT("正在获取")));
		CaptureBattleReportState(*this, SlateRoot, TEXT("loading"));
		RokClearBattleReportFixtureForTest(Screen);
	}

	{
		TSet<int64> Failed; Failed.Add(501);
		auto* Screen = NewScreen(World, MakeMails(1, false), Failed);
		if (!TestNotNull(TEXT("Failure fixture screen initializes"), Screen) || !TestTrue(TEXT("Failure report tab opens"), OpenReportTab(*Screen))) return false;
		const TSharedRef<SWidget> SlateRoot = Screen->TakeWidget();
		const auto* Pending = Cast<UTextBlock>(RokUiFind::Find(Screen, TEXT("BattleReportPending")));
		TestTrue(TEXT("Failed download is distinguished from loading"), Pending && Pending->GetText().ToString().Contains(TEXT("下载失败")));
		CaptureBattleReportState(*this, SlateRoot, TEXT("failed"));
		RokClearBattleReportFixtureForTest(Screen);
	}

	{
		auto* Screen = NewScreen(World, MakeMails(3, true));
		if (!TestNotNull(TEXT("Report detail fixture screen initializes"), Screen) || !TestTrue(TEXT("Detail report tab opens"), OpenReportTab(*Screen))) return false;
		const TSharedRef<SWidget> SlateRoot = Screen->TakeWidget();
		auto* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("MailListScroll")));
		TestTrue(TEXT("Three report rows render dynamically"), List && List->GetChildrenCount() == 3);
		if (!URokUiSettings::IsLegacyForced())
		{
			auto* FirstRowBox = Cast<USizeBox>(List ? List->GetChildAt(0) : nullptr);
			auto* FirstRow = FirstRowBox ? Cast<URokUiPageWidget>(FirstRowBox->GetContent()) : nullptr;
			TestTrue(TEXT("Battle-report list row is the generated WBP class"), FirstRow
				&& FirstRow->GetClass()->GetPathName().Contains(TEXT("WBP_RokFeature_BattleReportInboxRow")));
			TestNotNull(TEXT("List row's inner indexed click target is mounted"), FirstRow
				? RokUiFind::Find(FirstRow, TEXT("BattleReportRowHitButton")) : nullptr);
		}
		TestNotNull(TEXT("Battle report title renders"), RokUiFind::Find(Screen, TEXT("BattleReportTitle")));
		TestNotNull(TEXT("Army comparison renders"), RokUiFind::Find(Screen, TEXT("BattleReportDuration")));
		TestNotNull(TEXT("Hero experience renders"), RokUiFind::Find(Screen, TEXT("BattleReportExperience")));
		const auto* Casualties = Cast<UTextBlock>(RokUiFind::Find(Screen, TEXT("BattleReportCasualties")));
		TestTrue(TEXT("The overview preserves source casualty counts"), Casualties
			&& Casualties->GetText().ToString().Contains(TEXT("10")) && Casualties->GetText().ToString().Contains(TEXT("20")));
		const auto* Player = Cast<UTextBlock>(RokUiFind::Find(Screen, TEXT("BattleReportPlayer")));
		TestTrue(TEXT("The overview uses the current role's battle object"), Player && Player->GetText().ToString() == TEXT("我方"));
		TestNotNull(TEXT("The strength chart draws the actual report event"), RokUiFind::Find(Screen, TEXT("BattleReportCurveSegment_0")));
		TestNotNull(TEXT("The source chart start time is shown"), RokUiFind::Find(Screen, TEXT("BattleReportStartTime")));
		if (!URokUiSettings::IsLegacyForced())
		{
			TestNotNull(TEXT("The authored WBP detail canvas remains mounted"), RokUiFind::Find(Screen, TEXT("BattleReportDetailCanvas")));
			const auto* OldHeading = RokUiFind::Find(Screen, TEXT("BattleReportArmyHeading"));
			TestTrue(TEXT("The old comparison table is absent from the overview"), !OldHeading || OldHeading->GetVisibility() == ESlateVisibility::Collapsed);
		}
		CaptureBattleReportState(*this, SlateRoot, TEXT("detail"));
		if (auto* Selected = Cast<URokUnityIndexedButton>(RokUiFind::Find(Screen, TEXT("BattleReportRow_502"))))
			Selected->OnClicked.Broadcast();
		else if (auto* Size = Cast<USizeBox>(List ? List->GetChildAt(1) : nullptr))
		{
			if (auto* SelectedPage = Cast<URokUiPageWidget>(Size->GetContent()))
			{
				if (auto* Hit = Cast<URokUnityIndexedButton>(RokUiFind::Find(SelectedPage, TEXT("BattleReportRowHitButton"))))
					Hit->OnClicked.Broadcast();
				else TestTrue(TEXT("The second WBP row contains its indexed click target"), false);
			}
			else TestTrue(TEXT("The second WBP row provides a selectable action"), false);
		}
		else TestTrue(TEXT("The list provides a selectable report row"), false);
		const auto* RetriedPending = Cast<UTextBlock>(RokUiFind::Find(Screen, TEXT("BattleReportPending")));
		TestTrue(TEXT("Selecting the second row refreshes its loading detail"), RetriedPending
			&& RetriedPending->GetText().ToString().Contains(TEXT("正在获取")));
		auto* SystemTab = Cast<UButton>(Screen->GetWidgetFromName(TEXT("MailTab_0")));
		if (!TestNotNull(TEXT("Reports can return through the existing System tab"), SystemTab)) return false;
		SystemTab->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		TestNotNull(TEXT("Returning to System restores the ordinary empty state"), Screen->GetWidgetFromName(TEXT("MailEmptyState")));
		TestNull(TEXT("Returning to System removes the report detail"), Screen->GetWidgetFromName(TEXT("BattleReportDetailScroll")));
		RokClearBattleReportFixtureForTest(Screen);
	}
	return true;
}

#endif
