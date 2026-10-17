#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "../RokDataPaths.h"
#include "../RokTaskRewardPreviewData.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTaskRewardPreviewTest,
	"Rok.Ui.TaskDetail.SourceRewardPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTaskRewardPreviewTest::RunTest(const FString&)
{
	TArray<FRokTaskRewardPreviewRow> Rows;
	FString Error;
	if (!TestTrue(TEXT("Compiled source reward group loads"), RokTaskRewardPreviewData::LoadGroup(200900, Rows, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("Three contiguous reward entries"), Rows.Num(), 3)) return false;
	const int32 Ids[] = {100, 101, 104};
	const int64 Amounts[] = {2000, 2000, 50};
	const int32 SourceRowIds[] = {200900002, 200900003, 200900001};
	const int32 LocaleIds[] = {124012, 124011, 124034};
	const TCHAR* Icons[] = {TEXT("ui_common[img_com_cur1]"), TEXT("ui_common[img_com_cur2]"), TEXT("ui_common[img_com_cur5]")};
	const TCHAR* Names[] = {TEXT("食物"), TEXT("木材"), TEXT("钻石")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestEqual(TEXT("Currency ID order"), Rows[Index].CurrencyId, Ids[Index]);
		TestEqual(TEXT("Source quantity"), Rows[Index].Quantity, Amounts[Index]);
		TestEqual(TEXT("Source ItemPackageShow identity"), Rows[Index].PackageRowId, SourceRowIds[Index]);
		TestEqual(TEXT("Currency locale identity"), Rows[Index].LocaleId, LocaleIds[Index]);
		TestEqual(TEXT("Currency source icon"), Rows[Index].IconKey, FString(Icons[Index]));
		TestEqual(TEXT("Currency localized name"), Rows[Index].DisplayName, FString(Names[Index]));
		TestEqual(TEXT("Source icon width"), Rows[Index].IconRect.Width(), 80);
		TestEqual(TEXT("Source icon height"), Rows[Index].IconRect.Height(), 80);
	}
	TArray<uint8> SourceBytes;
	const FString CatalogPath = RokDataPaths::Resolve(TEXT("task_reward_preview.json"));
	if (!TestTrue(TEXT("Read generated catalog without modifying it"),
		FFileHelper::LoadFileToArray(SourceBytes, *CatalogPath))) return false;
	if (!TestTrue(TEXT("Exact source bytes pass pinned validation"),
		RokTaskRewardPreviewData::LoadGroupBytesForTesting(200900, SourceBytes, Rows, Error))) return false;
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(SourceBytes.GetData()), SourceBytes.Num());
	const FString OriginalJson(Converted.Length(), Converted.Get());
	auto RejectChangedSource = [this, &OriginalJson, &Rows, &Error](const TCHAR* Label,
		const TCHAR* Before, const TCHAR* After)
	{
		FString Changed = OriginalJson;
		const int32 Changes = Changed.ReplaceInline(Before, After, ESearchCase::CaseSensitive);
		if (!TestEqual(FString::Printf(TEXT("%s source edit applies once"), Label), Changes, 1)) return false;
		const FTCHARToUTF8 Encoded(*Changed);
		TArray<uint8> ChangedBytes;
		ChangedBytes.Append(reinterpret_cast<const uint8*>(Encoded.Get()), Encoded.Length());
		const bool bAccepted = RokTaskRewardPreviewData::LoadGroupBytesForTesting(200900, ChangedBytes, Rows, Error);
		TestFalse(FString::Printf(TEXT("%s modified source rejected"), Label), bAccepted);
		TestEqual(FString::Printf(TEXT("%s modified source returns no rows"), Label), Rows.Num(), 0);
		TestTrue(FString::Printf(TEXT("%s modified source has explicit integrity error"), Label),
			Error.Contains(TEXT("identity")));
		return !bAccepted && Rows.IsEmpty();
	};
	if (!RejectChangedSource(TEXT("Quantity"), TEXT("\"quantity\": 50"), TEXT("\"quantity\": 99"))
		|| !RejectChangedSource(TEXT("Package row ID"), TEXT("\"package_row_id\": 200900002"),
			TEXT("\"package_row_id\": 200900099"))
		|| !RejectChangedSource(TEXT("Sprite UV rectangle"), TEXT("        865,"),
			TEXT("        866,"))) return false;
	TestFalse(TEXT("Unknown group has no invented preview"), RokTaskRewardPreviewData::LoadGroup(200901, Rows, Error));
	TestEqual(TEXT("Failed lookup clears rows"), Rows.Num(), 0);
	TestTrue(TEXT("Unknown group returns an explicit reason"), !Error.IsEmpty());

	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTaskRewardPreviewFixture"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Isolated widget world"), World)) return false;
	URokUnityFeatureScreenWidget* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestNotNull(TEXT("Task detail screen"), Screen)) return false;
	Screen->Initialize();
	Screen->TakeWidget();
	Screen->OpenTasks();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FString Prefix = FString::Printf(TEXT("TaskRewardPreview_0_%d"), Index);
		UWidget* Cell = Screen->GetWidgetFromName(*Prefix);
		UImage* Icon = Cast<UImage>(Screen->GetWidgetFromName(*(Prefix + TEXT("Icon"))));
		UTextBlock* Count = Cast<UTextBlock>(Screen->GetWidgetFromName(*(Prefix + TEXT("Count"))));
		if (!TestNotNull(TEXT("Source reward cell"), Cell)
			|| !TestNotNull(TEXT("Source reward icon"), Icon)
			|| !TestNotNull(TEXT("Source reward quantity"), Count)) return false;
		TestTrue(TEXT("Reward tooltip identifies preview boundary"), Cell->GetToolTipText().ToString().Contains(TEXT("服务器状态待同步")));
		TestTrue(TEXT("Reward icon has a loaded brush"), Icon->GetBrush().GetResourceObject() != nullptr);
		TestTrue(TEXT("Reward quantity is visible text"), !Count->GetText().IsEmpty());
	}
	TestNull(TEXT("Former engineering group label removed"), Screen->GetWidgetFromName(TEXT("TaskRewardGroup_0")));
	TestNull(TEXT("Known group is not unavailable"), Screen->GetWidgetFromName(TEXT("TaskRewardUnavailable_0")));
	UButton* Action = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
	UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskAction_0Label")));
	if (!TestNotNull(TEXT("Ordinary task action"), Action)
		|| !TestNotNull(TEXT("Ordinary task action label"), Label)) return false;
	TestTrue(TEXT("Incomplete task retains action"), Action->GetIsEnabled());
	TestEqual(TEXT("Preview does not turn Go into Claim"), Label->GetText().ToString(), FString(TEXT("前往")));
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
