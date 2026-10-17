#include "RokLeaderboardAuditSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokUnityModalWidget.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 LeaderboardAuditCategories[] = {5, 7, 8};
constexpr int32 LeaderboardAuditCaptureCount = 11;

bool LeaderboardAuditVisible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		if (const auto* Parent = Cast<UUserWidget>(Outer); Parent && (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0)) return false;
	return true;
}
}

bool URokLeaderboardAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokLeaderboardAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokLeaderboardAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }

void URokLeaderboardAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/LeaderboardAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokLeaderboardAuditSubsystem::ScreenshotProcessed);
}

void URokLeaderboardAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}

TStatId URokLeaderboardAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokLeaderboardAuditSubsystem, STATGROUP_Tickables); }

bool URokLeaderboardAuditSubsystem::CheckIdentity()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		|| GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
	{ Finish(TEXT("Wrong project/map; leaderboard audit requires the actual city runtime.")); return false; }
	return true;
}

bool URokLeaderboardAuditSubsystem::Begin()
{
	if (!CheckIdentity()) return false;
	for (const TCHAR* Other : {TEXT("RokUiAnimationAudit"), TEXT("RokWorldEntityAnimationAudit"), TEXT("RokFoodDepositAudit"), TEXT("RokTrainTierAudit"), TEXT("RokPlayerHeadAudit")})
		if (FParse::Param(FCommandLine::Get(), Other))
		{ Finish(TEXT("Run leaderboard audit without another automatic interaction audit.")); return false; }
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokMainHudWidget::StaticClass(), false);
	int32 Count = 0;
	for (auto* Widget : Found)
		if (Widget->GetWorld() == GetWorld() && LeaderboardAuditVisible(Widget) && Widget->GetCachedGeometry().GetLocalSize().X > 1)
		{ Hud = Cast<URokMainHudWidget>(Widget); ++Count; }
	if (Count != 1) { Finish(TEXT("Expected exactly one attached visible city HUD.")); return false; }
	Found.Reset(); Count = 0;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokUnityModalWidget::StaticClass(), false);
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld()) { Modal = Cast<URokUnityModalWidget>(Widget); ++Count; }
	if (Count != 1 || Modal->IsModalOpen())
	{ Finish(TEXT("Expected exactly one closed city modal before the profile button route.")); return false; }
	FString Error;
	if (!RokLeaderboardCatalog::Load(Rows, Error) || Rows.Num() != 9)
	{ Finish(FString::Printf(TEXT("Source leaderboard catalog unavailable: %s"), *Error)); return false; }
	// Leaderboard.csv list=2, sorted by source ID; no display name is invented for 401.
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
		if (Rows[Index].Id != (Index < 8 ? 201 + Index : 401))
		{ Finish(TEXT("Leaderboard source IDs/order differ from the nine source slots.")); return false; }
	if (Rows[5].Name != TEXT("市政厅等级") || Rows[5].Type != 6 || Rows[5].Target != 1
		|| Rows[5].Icon != TEXT("ui_player[img_player_206]") || Rows[5].Columns != TArray<FString>{TEXT("等级"), TEXT("等级:{0}")}
		|| Rows[7].Name != TEXT("远征排行榜") || Rows[7].Type != 208 || Rows[7].Target != 1
		|| Rows[7].Icon != TEXT("ui_player[img_player_208]") || Rows[7].Columns != TArray<FString>{TEXT("任务完成数"), TEXT("任务完成数:{0}")}
		|| !Rows[8].Name.IsEmpty() || Rows[8].HasIcon() || !Rows[8].Columns.IsEmpty() || Rows[8].Type != 118 || Rows[8].Target != 2)
	{ Finish(TEXT("Source 206/208/401 labels, identities or empty configuration differ.")); return false; }
	return ValidateCity();
}

bool URokLeaderboardAuditSubsystem::CheckText(FName Name, const FString& Expected)
{
	auto* Text = Modal.IsValid() ? Cast<UTextBlock>(Modal->GetWidgetFromName(Name)) : nullptr;
	if (!LeaderboardAuditVisible(Text) || Text->GetText().ToString() != Expected)
	{ Finish(FString::Printf(TEXT("Text mismatch %s: expected '%s', actual '%s'."), *Name.ToString(), *Expected,
		Text ? *Text->GetText().ToString() : TEXT("<missing>"))); return false; }
	return true;
}

bool URokLeaderboardAuditSubsystem::CheckIcon(FName Name, const FRokLeaderboardRow& Row)
{
	UWidget* Widget = Modal->GetWidgetFromName(Name);
	if (!Row.HasIcon())
	{
		if (Widget) { Finish(TEXT("Empty source leaderboard 401 acquired a fabricated icon.")); return false; }
		return true;
	}
	auto* Image = Cast<UImage>(Widget);
	if (!LeaderboardAuditVisible(Image) || !Image->GetBrush().GetResourceObject()
		|| Image->GetBrush().GetResourceObject()->GetPathName() != TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.rok_fullscreen_player"))
	{ Finish(TEXT("Leaderboard source icon/atlas is missing from the runtime widget.")); return false; }
	const FBox2f UV = Image->GetBrush().GetUVRegion();
	const FVector2f Min(float(Row.IconRect.Min.X) / 512, 1.0f - float(Row.IconRect.Max.Y) / 1024);
	const FVector2f Max(float(Row.IconRect.Max.X) / 512, 1.0f - float(Row.IconRect.Min.Y) / 1024);
	if (!UV.Min.Equals(Min, 0.00001f) || !UV.Max.Equals(Max, 0.00001f))
	{ Finish(TEXT("Leaderboard icon UVs differ from the selected source row.")); return false; }
	return true;
}

bool URokLeaderboardAuditSubsystem::ValidateCity()
{
	if (!CheckIdentity()) return false;
	if (!Hud.IsValid() || !Modal.IsValid() || Modal->IsModalOpen()
		|| !LeaderboardAuditVisible(Hud.Get()) || LeaderboardAuditVisible(Modal.Get())
		|| !LeaderboardAuditVisible(Hud->GetWidgetFromName(TEXT("PlayerProfileHitButton"))))
	{ Finish(TEXT("Closing the profile did not restore the visible city HUD with no modal.")); return false; }
	return true;
}

bool URokLeaderboardAuditSubsystem::ValidateProfile()
{
	if (!CheckIdentity()) return false;
	if (!Modal.IsValid() || !Modal->IsModalOpen() || !LeaderboardAuditVisible(Modal.Get())
		|| !LeaderboardAuditVisible(Modal->GetWidgetFromName(TEXT("ProfileRankingButton")))
		|| !LeaderboardAuditVisible(Modal->GetWidgetFromName(TEXT("Type1CloseButton")))
		|| Modal->GetWidgetFromName(TEXT("PlayerRankingMainPanel"))
		|| Modal->GetWidgetFromName(TEXT("PlayerRankingDetailPanel")))
	{ Finish(TEXT("Expected the real player profile after the HUD/ranking return route.")); return false; }
	return true;
}

bool URokLeaderboardAuditSubsystem::ValidateRanking(bool bDetail)
{
	if (!CheckIdentity()) return false;
	if (!Modal.IsValid() || !Modal->IsModalOpen() || !LeaderboardAuditVisible(Modal.Get())
		|| !LeaderboardAuditVisible(Modal->GetWidgetFromName(TEXT("Type3CloseButton")))
		|| Modal->GetWidgetFromName(TEXT("ProfileRankingButton")))
	{ Finish(TEXT("Ranking route left the modal or returned to profile too early.")); return false; }
	if (bDetail)
	{
		const FRokLeaderboardRow& Row = Rows[LeaderboardAuditCategories[CategoryOrdinal]];
		if (!LeaderboardAuditVisible(Modal->GetWidgetFromName(TEXT("PlayerRankingDetailPanel")))
			|| Modal->GetWidgetFromName(TEXT("PlayerRankingMainPanel"))
			|| Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_0"))
			|| !CheckText(TEXT("PlayerRankingDetailTitle"), Row.Name)
			|| !CheckIcon(TEXT("PlayerRankingDetailIcon"), Row)
			|| !CheckText(TEXT("PlayerRankingSelfRank"), TEXT("我的排名  —"))
			|| !CheckText(TEXT("PlayerRankingColumn_0"), TEXT("排名"))
			|| !CheckText(TEXT("PlayerRankingColumn_1"), TEXT("名称"))
			|| !CheckText(TEXT("PlayerRankingColumn_2"), Row.Columns.IsEmpty() ? FString() : Row.Columns[0])
			|| !CheckText(TEXT("PlayerRankingBackLabel"), TEXT("‹ 返回"))
			|| !CheckText(TEXT("PlayerRankingEmptyState"), TEXT("尚未同步排行榜数据"))
			|| !CheckText(TEXT("PlayerRankingServerBoundaryState"), TEXT("配置预览 · 榜单服务尚未接入")))
		{ if (!bFinished) Finish(TEXT("Ranking detail retained the overview.")); return false; }
	}
	else
	{
		if (Modal->GetWidgetFromName(TEXT("PlayerRankingDetailTitle"))
			|| Modal->GetWidgetFromName(TEXT("PlayerRankingDetailPanel")))
		{ Finish(TEXT("First detail close did not remove the detail page.")); return false; }
		if (!CheckText(TEXT("PlayerRankingServerBoundaryState"), TEXT("排行榜配置预览 · 榜单尚未同步"))) return false;
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			if (!LeaderboardAuditVisible(Modal->GetWidgetFromName(*FString::Printf(TEXT("PlayerRankingCategoryButton_%d"), Index))))
			{ Finish(TEXT("A source category is missing from the overview.")); return false; }
			if (!CheckText(*FString::Printf(TEXT("PlayerRankingCategoryLabel_%d"), Index), Rows[Index].Name)
				|| !CheckText(*FString::Printf(TEXT("PlayerRankingCategoryState_%d"), Index), TEXT("--"))
				|| !CheckText(*FString::Printf(TEXT("PlayerRankingCategoryOwner_%d"), Index), TEXT("--"))
				|| !CheckIcon(*FString::Printf(TEXT("PlayerRankingCategoryIcon_%d"), Index), Rows[Index])) return false;
		}
		if (Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_9")))
		{ Finish(TEXT("Overview acquired a category outside the source catalog.")); return false; }
	}
	// Current offline source surface has no rank rows: only these preview texts.
	int32 TextCount = 0;
	Modal->WidgetTree->ForEachWidget([&TextCount](UWidget* Widget)
	{
		if (Cast<UTextBlock>(Widget) && Widget->GetName().StartsWith(TEXT("PlayerRanking")) && LeaderboardAuditVisible(Widget)) ++TextCount;
	});
	if (TextCount != (bDetail ? 8 : 28))
	{ Finish(TEXT("Ranking preview contains unexpected or missing visible rank text.")); return false; }
	return true;
}

bool URokLeaderboardAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
	auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!CheckIdentity()) return false;
	if (!Owner || Owner->GetWorld() != GetWorld() || !LeaderboardAuditVisible(Owner) || !LeaderboardAuditVisible(Button)
		|| !Button->GetIsEnabled() || Button->GetCachedGeometry().GetLocalSize().X <= 0 || !Button->OnClicked.IsBound())
	{ Finish(FString::Printf(TEXT("Missing/hidden/disabled/unlaid-out/unbound runtime button: %s."), *Name.ToString())); return false; }
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("button"), Name.ToString());
	Action->SetNumberField(TEXT("frame"), double(Frame)); Action->SetNumberField(TEXT("time_seconds"), Elapsed);
	if (Step == 6)
	{
		const int32 Index = LeaderboardAuditCategories[CategoryOrdinal];
		Action->SetNumberField(TEXT("source_category_index"), Index);
		Action->SetNumberField(TEXT("source_leaderboard_id"), Rows[Index].Id);
	}
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	Button->OnClicked.Broadcast();
	return true;
}

void URokLeaderboardAuditSubsystem::Enter(int32 NextStep) { Step = NextStep; StepStarted = Elapsed; }

bool URokLeaderboardAuditSubsystem::ValidateCapture()
{
	if (Step == 1 || Step == 13) return ValidateCity();
	if (Step == 3 || Step == 11) return ValidateProfile();
	if (Step == 5 || Step == 9) return ValidateRanking(false);
	if (Step == 7) return ValidateRanking(true);
	Finish(TEXT("Unexpected leaderboard capture stage.")); return false;
}

TSharedRef<FJsonObject> URokLeaderboardAuditSubsystem::State() const
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("frame"), double(Frame)); Result->SetNumberField(TEXT("time_seconds"), Elapsed);
	Result->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	Result->SetNumberField(TEXT("step"), Step);
	if (Modal.IsValid())
	{
		auto Texts = MakeShared<FJsonObject>();
		Modal->WidgetTree->ForEachWidget([&Texts](UWidget* Widget)
		{
			if (auto* Text = Cast<UTextBlock>(Widget); Text && Text->GetName().StartsWith(TEXT("PlayerRanking")) && LeaderboardAuditVisible(Text))
				Texts->SetStringField(Text->GetName(), Text->GetText().ToString());
		});
		Result->SetObjectField(TEXT("visible_ranking_texts"), Texts);
		Result->SetBoolField(TEXT("profile_present"), Modal->GetWidgetFromName(TEXT("ProfileRankingButton")) != nullptr);
		Result->SetBoolField(TEXT("detail_present"), Modal->GetWidgetFromName(TEXT("PlayerRankingDetailTitle")) != nullptr);
		Result->SetBoolField(TEXT("detail_icon_present"), Modal->GetWidgetFromName(TEXT("PlayerRankingDetailIcon")) != nullptr);
		Result->SetBoolField(TEXT("category_401_icon_present"), Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryIcon_8")) != nullptr);
	}
	if (Step == 7)
	{
		const int32 Index = LeaderboardAuditCategories[CategoryOrdinal];
		Result->SetNumberField(TEXT("clicked_source_category_index"), Index);
		Result->SetNumberField(TEXT("expected_source_leaderboard_id"), Rows[Index].Id);
	}
	return Result;
}

void URokLeaderboardAuditSubsystem::Capture(const FString& Name)
{
	if (!ValidateCapture()) return;
	if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Another screenshot request is active.")); return; }
	PendingPath = Directory / (Name + TEXT(".png"));
	PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("name"), Name); PendingCapture->SetStringField(TEXT("path"), PendingPath);
	PendingCapture->SetObjectField(TEXT("state_at_request"), State());
	ScreenshotStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}

void URokLeaderboardAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !PendingCapture.IsValid() || !ValidateCapture()) return;
	TArray<uint8> File;
	const uint8 PngSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(File, *PendingPath) || File.Num() < 24 || FMemory::Memcmp(File.GetData(), PngSignature, 8) != 0)
	{ Finish(TEXT("Processed leaderboard screenshot is missing, empty, or not PNG.")); return; }
	const auto ReadBigEndian = [&File](int32 Offset) { return (uint32(File[Offset]) << 24) | (uint32(File[Offset + 1]) << 16) | (uint32(File[Offset + 2]) << 8) | uint32(File[Offset + 3]); };
	const uint32 Width = ReadBigEndian(16), Height = ReadBigEndian(20);
	if (Width < 2 || Height < 2) { Finish(TEXT("Leaderboard screenshot has invalid dimensions.")); return; }
	PendingCapture->SetNumberField(TEXT("file_bytes"), File.Num());
	PendingCapture->SetNumberField(TEXT("width"), Width); PendingCapture->SetNumberField(TEXT("height"), Height);
	PendingCapture->SetObjectField(TEXT("state_at_processed"), State());
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture)); PendingCapture.Reset();
	if (!Save(false)) { Finish(TEXT("Leaderboard progress manifest write failed.")); return; }
	if (Step == 9 && CategoryOrdinal + 1 < UE_ARRAY_COUNT(LeaderboardAuditCategories)) { ++CategoryOrdinal; Enter(6); }
	else Enter(Step + 1);
}

void URokLeaderboardAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += FMath::Max(0.0f, DeltaTime); ++Frame;
	if (Elapsed > 90) { Finish(TEXT("Leaderboard runtime audit timed out.")); return; }
	if (PendingCapture.IsValid()) { if (Elapsed - ScreenshotStarted > 10) Finish(TEXT("Leaderboard screenshot processing timed out.")); return; }
	if (Elapsed - StepStarted < (Step == 0 ? 3.0 : 0.75)) return;
	switch (Step)
	{
	case 0: if (Begin()) Enter(1); break;
	case 1: Capture(TEXT("01_city_before")); break;
	case 2: if (Click(Hud.Get(), TEXT("PlayerProfileHitButton"))) Enter(3); break;
	case 3: Capture(TEXT("02_profile_open")); break;
	case 4: if (Click(Modal.Get(), TEXT("ProfileRankingButton"))) Enter(5); break;
	case 5: Capture(TEXT("03_overview_open")); break;
	case 6: if (Click(Modal.Get(), *FString::Printf(TEXT("PlayerRankingCategoryButton_%d"), LeaderboardAuditCategories[CategoryOrdinal]))) Enter(7); break;
	case 7: Capture(FString::Printf(TEXT("%02d_detail_%d"), 4 + CategoryOrdinal * 2, Rows[LeaderboardAuditCategories[CategoryOrdinal]].Id)); break;
	case 8:
		if (Click(Modal.Get(), TEXT("Type3CloseButton")))
		{
			if (!Modal->IsModalOpen() || !Modal->GetWidgetFromName(TEXT("PlayerRankingCategoryButton_0"))
				|| Modal->GetWidgetFromName(TEXT("PlayerRankingDetailTitle")))
			{ Finish(TEXT("First detail close must immediately retain the modal and return to overview.")); return; }
			Enter(9);
		}
		break;
	case 9: Capture(FString::Printf(TEXT("%02d_overview_after_%d"), 5 + CategoryOrdinal * 2, Rows[LeaderboardAuditCategories[CategoryOrdinal]].Id)); break;
	case 10: if (Click(Modal.Get(), TEXT("Type3CloseButton"))) Enter(11); break;
	case 11: Capture(TEXT("10_profile_return")); break;
	case 12: if (Click(Modal.Get(), TEXT("Type1CloseButton"))) Enter(13); break;
	case 13: Capture(TEXT("11_city_return")); break;
	case 14: Finish(); break;
	default: Finish(TEXT("Unexpected leaderboard audit step.")); break;
	}
}

bool URokLeaderboardAuditSubsystem::Save(bool bComplete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), bComplete);
	Root->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty() && Captures.Num() == LeaderboardAuditCaptureCount);
	Root->SetNumberField(TEXT("audit_version"), 1); Root->SetNumberField(TEXT("expected_capture_count"), LeaderboardAuditCaptureCount);
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Root->SetStringField(TEXT("catalog"), TEXT("Saved/RokUnityMigration/leaderboard_catalog.json"));
	Root->SetStringField(TEXT("source"), TEXT("UI_Win_RankingMediator.cs DataProcess(list=2, ascending ID), onClose(detail->overview); Leaderboard.csv"));
	Root->SetStringField(TEXT("input_method"), TEXT("Existing visible enabled UMG OnClicked delegates and real NativeTick. No TestingAdvance; physical pointer hit testing is separate."));
	Root->SetStringField(TEXT("authority_boundary"), TEXT("Configuration preview only. No rank request or synthetic rank rows; personal rank remains unknown. Source IDs are validated catalog bindings for clicked category indices, not server result IDs."));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetArrayField(TEXT("actions"), Actions); Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json;
	return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))
		&& FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokLeaderboardAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	else if (Captures.Num() != LeaderboardAuditCaptureCount) Failures.Add(TEXT("Expected exactly eleven processed leaderboard screenshots."));
	bFinished = true;
	if (!Save(true)) Failures.Add(TEXT("Final leaderboard manifest write failed."));
	const FString Errors = FString::Join(Failures, TEXT(" | "));
	if (Failures.IsEmpty()) { UE_LOG(LogTemp, Display, TEXT("Rok Leaderboard audit PASS: %s"), *Directory); }
	else { UE_LOG(LogTemp, Error, TEXT("Rok Leaderboard audit FAIL: %s; %s"), *Errors, *Directory); }
	FPlatformMisc::RequestExit(false);
}
