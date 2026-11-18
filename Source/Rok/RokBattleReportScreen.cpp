#include "RokUnityFeatureScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Containers/SortedMap.h"
#include "EngineUtils.h"
#include "RokGameNetSubsystem.h"
#include "RokMailWbpBinding.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "RokUiTextTable.h"
#include "RokUnityConfigSubsystem.h"
#include "RokWorldMapConfigActor.h"
#include "RokUiWidgets.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/RokBattleReportWbpFixture.h"
#endif

namespace
{
constexpr TCHAR MailAtlas[] = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_mail.rok_fullscreen_mail");
const FIntPoint AtlasSize(2048, 1024);
const FIntRect ReportIcon(1930, 650, 2007, 725);
const FIntRect ReportPoster(1, 650, 822, 773);
const FLinearColor Gold(0.91f, 0.79f, 0.52f);
const FLinearColor Secondary(0.74f, 0.75f, 0.70f);
const FLinearColor Slate(0.047f, 0.055f, 0.057f, 0.96f);
const FLinearColor Stripe(0.078f, 0.088f, 0.085f);

using FRow = TSharedPtr<FJsonObject>;

struct FBattleReportFixture
{
	TMap<int64, FRow> Mails;
	TSet<int64> FailedDownloads;
	int64 RoleId = 1;
};
TMap<const URokUnityFeatureScreenWidget*, FBattleReportFixture> BattleReportFixtures;

bool IsLegacyBattleReportPresentation()
{
	return URokUiSettings::IsLegacyForced();
}

FRow Object(const FRow& Row, const TCHAR* Field)
{
	const FRow* Found = nullptr;
	return Row && Row->TryGetObjectField(Field, Found) ? *Found : nullptr;
}

const TArray<TSharedPtr<FJsonValue>>& Array(const FRow& Row, const TCHAR* Field)
{
	static const TArray<TSharedPtr<FJsonValue>> Empty;
	const TArray<TSharedPtr<FJsonValue>>* Found = nullptr;
	return Row && Row->TryGetArrayField(Field, Found) ? *Found : Empty;
}

int64 Number(const FRow& Row, const TCHAR* Field)
{
	int64 Value = 0;
	RokSprotoJson::TryGetInt64(Row, Field, Value);
	return Value;
}

FString Count(int64 Value) { return FText::AsNumber(Value).ToString(); }

FString ConfigText(URokUnityConfigSubsystem* Config, const TCHAR* Table, int64 Id, const TCHAR* Field)
{
	int32 LanguageId = 0;
	return Config && Id > 0 && Id <= MAX_int32 && Config->GetIntFieldByKey(Table, TEXT("ID"), Id, Field, LanguageId)
		? FRokUiTextTable::SanitizeRichText(FRokUiTextTable::Resolve(LanguageId)) : FString();
}

FString EnemyLabel(URokUnityConfigSubsystem* Config, int64 MonsterId)
{
	int32 Level = 0, Type = 0;
	if (Config && MonsterId > 0 && MonsterId <= MAX_int32
		&& Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("type"), Type) && Type == 1
		&& Config->GetIntFieldByKey(TEXT("Monster"), TEXT("ID"), MonsterId, TEXT("level"), Level))
		return FString::Printf(TEXT("等级%d乱军"), Level);
	return ConfigText(Config, TEXT("Monster"), MonsterId, TEXT("l_nameId"));
}

FString ReportSubject(URokUnityConfigSubsystem* Config, const FRow& Mail)
{
	// BattleReport.getEmailIdAndArg: field 5 is the defender's static ID.
	const auto& Subtitle = Array(Mail, TEXT("reportSubTile"));
	int64 MonsterId = 0;
	if (Subtitle.IsValidIndex(4)) LexTryParseString(MonsterId, *Subtitle[4]->AsString());
	const FString Enemy = EnemyLabel(Config, MonsterId);
	if (!Enemy.IsEmpty()) return Enemy;
	return Subtitle.IsValidIndex(5) && !Subtitle[5]->AsString().IsEmpty() ? Subtitle[5]->AsString() : TEXT("交战记录");
}

FString ReportTitle(URokUnityConfigSubsystem* Config, const FRow& Mail)
{
	// The Mail table uses l_nameID, whereas Monster uses l_nameId. Preserve the JSON spelling.
	const auto* Rows = Config ? Config->GetTableRows(TEXT("Mail")) : nullptr;
	const int64 MailId = Number(Mail, TEXT("emailId"));
	if (Rows && MailId > 0)
	{
		const auto* Template = Rows->FindByPredicate([MailId](const FRow& Row) { return Number(Row, TEXT("ID")) == MailId; });
		if (Template)
		{
			const FString Title = FRokUiTextTable::Resolve(Number(*Template, TEXT("l_nameID")));
			if (!Title.IsEmpty()) return Title;
		}
	}
	return TEXT("战斗报告");
}

struct FCasualties
{
	int64 Minor = 0, Severe = 0, Dead = 0;
	bool bKnown = false;
};

FCasualties Casualties(const FRow& Army)
{
	FCasualties Out;
	for (const auto& Enemy : Array(Army, TEXT("soldierDetail")))
	{
		if (!Enemy || Enemy->Type != EJson::Object) continue;
		for (const auto& Soldier : Array(Enemy->AsObject(), TEXT("battleSoldierHurt")))
		{
			if (!Soldier || Soldier->Type != EJson::Object) continue;
			const auto& Row = Soldier->AsObject();
			Out.bKnown = true;
			Out.Minor += Number(Row, TEXT("minor"));
			Out.Severe += Number(Row, TEXT("hardHurt"));
			Out.Dead += Number(Row, TEXT("die"));
		}
	}
	return Out;
}
}

#if WITH_DEV_AUTOMATION_TESTS
void RokSetBattleReportFixtureForTest(
	URokUnityFeatureScreenWidget* Screen,
	const TMap<int64, TSharedPtr<FJsonObject>>& Mails,
	const TSet<int64>& FailedDownloads,
	int64 RoleId)
{
	if (!Screen) return;
	FBattleReportFixture Fixture;
	Fixture.Mails = Mails;
	Fixture.FailedDownloads = FailedDownloads;
	Fixture.RoleId = RoleId;
	BattleReportFixtures.Add(Screen, MoveTemp(Fixture));
}

void RokClearBattleReportFixtureForTest(URokUnityFeatureScreenWidget* Screen)
{
	BattleReportFixtures.Remove(Screen);
}
#endif

void URokUnityFeatureScreenWidget::RefreshBattleReportMail()
{
	if (bLocalMailUnreadFixtureState || bReferenceMailCaptureState || bReadOnlyMailRewardFixtureState) return;
	if (BattleReportFixtures.Contains(this)) return;
	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (Net && (ShownBattleReportRevision != Net->GetBattleReportRevision()
		|| bShownMailInboxLoading != Net->IsMailInboxLoading()))
	{
		RebuildModeContent();
	}
}

void URokUnityFeatureScreenWidget::SelectBattleReport(int64 EmailIndex)
{
	if (const FBattleReportFixture* Fixture = BattleReportFixtures.Find(this))
	{
		if (!Fixture->Mails.Contains(EmailIndex)) return;
		SelectedBattleReport = EmailIndex;
		RebuildModeContent();
		return;
	}
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || !Net->GetBattleReportMail().Contains(EmailIndex)) return;
	SelectedBattleReport = EmailIndex;
	Net->RequestBattleReport(EmailIndex);
	Net->MarkBattleReportRead(EmailIndex);
	RebuildModeContent();
}

void URokUnityFeatureScreenWidget::BuildBattleReportMail(UScrollBox* List)
{
	auto* Instance = GetGameInstance();
	auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	const TMap<int64, FRow>* MailMap = Net ? &Net->GetBattleReportMail() : nullptr;
	const FBattleReportFixture* Fixture = BattleReportFixtures.Find(this);
	if (Fixture) MailMap = &Fixture->Mails;
	ShownBattleReportRevision = Net && !Fixture ? Net->GetBattleReportRevision() : 0;
	bShownMailInboxLoading = Net && !Fixture && Net->IsMailInboxLoading();
	TArray<int64> Ids;
	if (MailMap)
	{
		MailMap->GenerateKeyArray(Ids);
		Ids.Sort([MailMap](int64 A, int64 B)
		{
			const int64 TimeA = Number(MailMap->FindChecked(A), TEXT("sendTime"));
			const int64 TimeB = Number(MailMap->FindChecked(B), TEXT("sendTime"));
			return TimeA == TimeB ? A > B : TimeA > TimeB;
		});
	}
	if (!Ids.Contains(SelectedBattleReport))
	{
		SelectedBattleReport = Ids.IsEmpty() ? 0 : Ids[0];
		if (SelectedBattleReport && Net && !Fixture)
		{
			Net->MarkBattleReportRead(SelectedBattleReport);
		}
	}
	static UClass* ReportPageClass = nullptr;
	static UClass* ReportRowClass = nullptr;
	if (!IsLegacyBattleReportPresentation())
	{
		if (!ReportPageClass) ReportPageClass = LoadClass<URokUiPageWidget>(nullptr,
			TEXT("/Game/RokUI/Feature/WBP_RokFeature_BattleReportOverview.WBP_RokFeature_BattleReportOverview_C"));
		if (!ReportRowClass) ReportRowClass = LoadClass<URokUiPageWidget>(nullptr,
			TEXT("/Game/RokUI/Feature/WBP_RokFeature_BattleReportInboxRow.WBP_RokFeature_BattleReportInboxRow_C"));

	}
	const bool bUseWbp = ReportPageClass && ReportRowClass;
	URokUiPageWidget* ReportPage = nullptr;
	UCanvasPanel* DetailCanvas = nullptr;
	UScrollBox* Detail = nullptr;
	USizeBox* DetailSize = nullptr;
	UTextBlock* EmptyText = nullptr;
	if (bUseWbp)
	{
		ReportPage = CreateWidget<URokUiPageWidget>(this, ReportPageClass);
		if (ReportPage)
		{
			ReportPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UCanvasPanelSlot* PageSlot = ModeCanvas->AddChildToCanvas(ReportPage);
			PageSlot->SetPosition({547, 128});
			PageSlot->SetSize({830, 590});
			PageSlot->SetZOrder(6);
			Detail = ReportPage->Get<UScrollBox>(TEXT("BattleReportDetailScroll"));
			DetailSize = ReportPage->Get<USizeBox>(TEXT("BattleReportDetailSize"));
			DetailCanvas = ReportPage->Get<UCanvasPanel>(TEXT("BattleReportDetailCanvas"));
			EmptyText = ReportPage->Get<UTextBlock>(TEXT("BattleReportEmpty"));
		}
	}
	bool bMountedWbp = ReportPage && Detail && DetailSize && DetailCanvas && EmptyText;
	if (ReportPage && !bMountedWbp)
	{
		ReportPage->RemoveFromParent();
		ReportPage = nullptr;
	}
	auto Label = [this, ReportPage, bMountedWbp](UCanvasPanel* Parent, FName Name, const FString& Text, FVector2D Pos, FVector2D Size,
		int32 Font, FLinearColor Color = FLinearColor::White, bool bBold = false)
	{
		if (bMountedWbp && !Name.IsNone())
		{
			if (auto* Block = Cast<UTextBlock>(RokUiFind::Find(ReportPage, Name)))
			{
				Block->SetText(FText::FromString(Text));
				Block->SetVisibility(ESlateVisibility::HitTestInvisible);
				return;
			}
		}
		auto* Block = MakeScreenText(Name, Font, Color, bBold);
		Block->SetText(FText::FromString(Text));
		PlaceOnCanvas(Parent, Block, Pos, Size, 2);
	};
	auto SetWbpVisibility = [ReportPage](FName Name, ESlateVisibility TargetVisibility)
	{
		if (UWidget* Widget = RokUiFind::Find(ReportPage, Name)) Widget->SetVisibility(TargetVisibility);
	};
	for (const int64 Id : Ids)
	{
		const auto& Mail = MailMap->FindChecked(Id);
		if (bMountedWbp)
		{
			auto* Row = CreateWidget<URokUiPageWidget>(this, ReportRowClass);
			if (!Row) continue;
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			auto* HitButton = Cast<URokUnityIndexedButton>(RokUiFind::Find(Row, TEXT("BattleReportRowHitButton")));
			if (!HitButton) continue;
			HitButton->ConfigureAction(INDEX_NONE);
			HitButton->OnIndexedClicked.AddWeakLambda(this, [this, Id](int32) { SelectBattleReport(Id); });
			const bool bAuthoredSelection = RokMailWbp::Find(*Row, TEXT("BattleReportRowSelection")) != nullptr;
			if (bAuthoredSelection)
			{
				RokMailWbp::Show(*Row, TEXT("BattleReportRowSelection"), Id == SelectedBattleReport);
			}
			if (auto* Plate = bAuthoredSelection ? nullptr : Cast<UBorder>(RokUiFind::Find(Row, TEXT("BattleReportRowPlate"))))
				Plate->SetBrushColor(Id == SelectedBattleReport ? FLinearColor(0.19f, 0.22f, 0.20f) : Slate);
			if (auto* Title = Cast<UTextBlock>(RokUiFind::Find(Row, TEXT("BattleReportRowTitle"))))
			{
				Title->SetText(FText::FromString(ReportTitle(Config, Mail)));
				if (!bAuthoredSelection)
				{
					Title->SetColorAndOpacity(Number(Mail, TEXT("status")) == 0 ? Gold : FLinearColor::White);
				}
			}
			if (auto* Subject = Cast<UTextBlock>(RokUiFind::Find(Row, TEXT("BattleReportRowSubject"))))
				Subject->SetText(FText::FromString(ReportSubject(Config, Mail)));
			if (auto* Unread = RokUiFind::Find(Row, TEXT("BattleReportRowUnread")))
				Unread->SetVisibility(Number(Mail, TEXT("status")) == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			const FVector2D AuthoredSize = RokMailWbp::Size(*Row, TEXT("BattleReportRowLayout"));
			List->AddChild(WrapFixedSize(Row, *FString::Printf(TEXT("BattleReportRowSize_%lld"), Id),
				AuthoredSize.X > 0 && AuthoredSize.Y > 0 ? AuthoredSize : FVector2D(470, 100)));
			continue;
		}
		const FName Name(*FString::Printf(TEXT("BattleReportRow_%lld"), Id));
		auto* Button = MakeActionButton(Name, INDEX_NONE, nullptr, FIntRect(), FIntPoint(1, 1), {470, 100});
		Button->OnIndexedClicked.Clear();
		Button->OnIndexedClicked.AddWeakLambda(this, [this, Id](int32) { SelectBattleReport(Id); });
		auto* Row = CastChecked<UCanvasPanel>(Button->GetContent());
		PlaceOnCanvas(Row, MakeSolidBorder(NAME_None, Id == SelectedBattleReport
			? FLinearColor(0.19f, 0.22f, 0.20f) : Slate), {6, 4}, {460, 90});
		PlaceOnCanvas(Row, MakeAtlasImage(NAME_None, MailAtlas, ReportIcon, AtlasSize, {68, 66}), {18, 16}, {68, 66}, 1);
		Label(Row, NAME_None, ReportTitle(Config, Mail), {101, 14}, {340, 32}, 23,
			Number(Mail, TEXT("status")) == 0 ? Gold : FLinearColor::White, true);
		Label(Row, NAME_None, ReportSubject(Config, Mail), {101, 54}, {300, 24}, 17, Secondary);
		if (Number(Mail, TEXT("status")) == 0)
			Label(Row, NAME_None, TEXT("●"), {426, 20}, {28, 22}, 18, FLinearColor(0.92f, 0.19f, 0.12f));
		List->AddChild(WrapFixedSize(Button, NAME_None, {470, 100}));
	}
	auto BuildNativeDetailPane = [&]()
	{
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("BattleReportDetailPanel"), Slate), {547, 128}, {830, 590}, 3);
		Detail = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BattleReportDetailScroll"));
		Detail->SetAllowOverscroll(false);
		PlaceOnCanvas(ModeCanvas, Detail, {552, 132}, {820, 582}, 5);
		DetailCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	};
	if (!bMountedWbp)
	{
		BuildNativeDetailPane();
	}
	if (!SelectedBattleReport)
	{
		const FText EmptyLabel = bShownMailInboxLoading
			? NSLOCTEXT("RokMail", "InboxLoading", "正在获取邮件…")
			: NSLOCTEXT("RokMail", "NoBattleReports", "暂无战斗报告");
		if (bMountedWbp)
		{
			EmptyText->SetText(EmptyLabel);
			EmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
			Detail->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			auto* Empty = MakeScreenText(TEXT("BattleReportEmpty"), 24, Secondary);
			Empty->SetText(EmptyLabel);
			PlaceOnCanvas(ModeCanvas, Empty, {604, 215}, {690, 44}, 5);
		}
		return;
	}
	const auto& Mail = MailMap->FindChecked(SelectedBattleReport);
	const auto Report = Object(Mail, TEXT("battleReportExContent"));
	const bool bDownloadFailed = Fixture ? Fixture->FailedDownloads.Contains(SelectedBattleReport)
		: Net && Net->HasBattleReportDownloadFailed(SelectedBattleReport);
	if (!Report && !Fixture && Net) Net->RequestBattleReport(SelectedBattleReport);
	if (bMountedWbp)
	{
		EmptyText->SetVisibility(ESlateVisibility::Collapsed);
		Detail->SetVisibility(ESlateVisibility::Visible);
		SetWbpVisibility(TEXT("BattleReportPending"), ESlateVisibility::Collapsed);
	}
	else if (!Report)
	{
		// Legacy content is constructed below from the same data rows.
	}
	if (bMountedWbp && TryBuildBattleReportOverview(*ReportPage, Mail, Report,
		Fixture ? Fixture->RoleId : (Net ? Net->GetRoleId() : 0), bDownloadFailed))
	{
		return;
	}
	if (bMountedWbp)
	{
		// A partial/older asset cannot remain above the native fallback.
		ReportPage->RemoveFromParent();
		ReportPage = nullptr;
		bMountedWbp = false;
		BuildNativeDetailPane();
	}
	// Native/previous-schema fallback when the editable overview is unavailable.
	// Keep the page shell/list, replace the old summary-table presentation with
	// Unity UI_Item_MailFightReport's title -> rewards -> army -> strength curve.
	auto* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
	auto Text = [this, Content](FName Name, const FString& Value, FVector2D Pos, FVector2D Size, int32 Font,
		FLinearColor Color = FLinearColor::White)
	{
		auto* Block = MakeScreenText(Name, Font, Color);
		Block->SetText(FText::FromString(Value));
		PlaceOnCanvas(Content, Block, Pos, Size, 2);
	};
	PlaceOnCanvas(Content, MakeAtlasImage(NAME_None, MailAtlas, ReportPoster, AtlasSize, {803, 118}), {2, 1}, {803, 118});
	Text(TEXT("BattleReportTitle"), ReportTitle(Config, Mail), {16, 10}, {765, 38}, 27);
	Text(TEXT("BattleReportOpponent"), TEXT("与") + ReportSubject(Config, Mail) + TEXT("发生了战斗"), {16, 55}, {765, 29}, 21);
	const int64 SendTime = Number(Mail, TEXT("sendTime"));
	if (SendTime > 0 && SendTime < 253402300799LL)
	{
		Text(TEXT("BattleReportTime"), (FDateTime::FromUnixTimestamp(SendTime) + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%Y/%m/%d %H:%M")), {16, 86}, {765, 25}, 18);
	}
	if (!Report)
	{
		Text(TEXT("BattleReportPending"), bDownloadFailed ? TEXT("战报下载失败，点选左侧邮件重试。")
			: TEXT("正在获取战斗详情…"), {24, 154}, {750, 44}, 23, Secondary);
		if (bMountedWbp)
		{
			DetailSize->SetHeightOverride(245.0f);
		}
		else
		{
			Detail->AddChild(WrapFixedSize(Content, NAME_None, {805, 245}));
		}
		return;
	}
	FRow Self;
	TArray<FRow> Opponents;
	const int64 RoleId = Fixture ? Fixture->RoleId : (Net ? Net->GetRoleId() : 0);
	for (const auto& Value : Array(Report, TEXT("objectInfos")))
	{
		if (!Value || Value->Type != EJson::Object)
		{
			continue;
		}
		const auto& Row = Value->AsObject();
		if (RoleId > 0 && Number(Row, TEXT("rid")) == RoleId && !Self)
		{
			Self = Row;
		}
		else
		{
			Opponents.Add(Row);
		}
	}
	const auto Rewards = Object(Report, TEXT("rewardInfo"));
	const auto Payload = RokMailWbp::ReadRewards(Rewards);
	const auto Loot = RokMailRewardDetails::Normalize(Payload, GetEconomyCatalogSubsystem(), GetGameplayCatalogSubsystem());
	Text(TEXT("BattleReportRewardHeading"), TEXT("击杀奖励"), {16, 157}, {110, 30}, 20);
	float Y = 128;
	for (int32 Index = 0; Index < Loot.Num(); ++Index)
	{
		const auto& Reward = Loot[Index];
		const float Left = 124 + (Index % 7) * 93;
		const float Top = 128 + (Index / 7) * 95;
		const TCHAR* Texture = nullptr;
		FIntRect Rect;
		FIntPoint TextureSize;
		if (ResolveMailRewardIcon(Reward, Texture, Rect, TextureSize))
		{
			PlaceOnCanvas(Content, MakeAtlasImage(NAME_None, Texture, Rect, TextureSize, {63, 63}), {Left, Top}, {63, 63}, 1);
		}
		Text(NAME_None, Count(Reward.Quantity), {Left, Top + 46}, {68, 24}, 16);
		Text(NAME_None, Reward.DisplayName, {Left, Top + 67}, {92, 23}, 13, Secondary);
	}
	Y += FMath::Max(1, FMath::DivideAndRoundUp(Loot.Num(), 7)) * 95;
	if (Loot.IsEmpty())
	{
		Text(NAME_None, TEXT("无"), {124, 157}, {650, 28}, 19, Secondary);
	}
	if (!Self)
	{
		Text(TEXT("BattleReportMissingSelf"), TEXT("战报中没有当前角色的部队信息。"), {20, Y}, {760, 40}, 22, Secondary);
		Y += 60;
	}
	else
	{
		FString Name, Guild;
		Self->TryGetStringField(TEXT("name"), Name);
		Self->TryGetStringField(TEXT("guildName"), Guild);
		if (!Guild.IsEmpty())
		{
			Name = TEXT("[") + Guild + TEXT("] ") + Name;
		}
		PlaceOnCanvas(Content, MakeSolidBorder(NAME_None, FLinearColor(0.39f, 0.38f, 0.32f)), {3, Y}, {796, 63});
		Text(TEXT("BattleReportPlayer"), Name, {24, Y + 15}, {480, 35}, 24);
		Text(TEXT("BattleReportArmyTotal"), TEXT("部队总数:") + Count(Number(Self, TEXT("beginArmyCount"))), {531, Y + 5}, {255, 27}, 18);
		Text(TEXT("BattleReportArmyRemaining"), TEXT("剩余:") + Count(Number(Self, TEXT("endArmyCount"))), {531, Y + 31}, {255, 27}, 18);
		Y += 76;
		const FString Hero = ConfigText(Config, TEXT("Hero"), Number(Self, TEXT("mainHeroId")), TEXT("l_nameID"));
		Text(TEXT("BattleReportHero"), Hero.IsEmpty() ? TEXT("驻防部队")
			: FString::Printf(TEXT("等级%lld %s"), Number(Self, TEXT("mainHeroLevel")), *Hero), {61, Y}, {720, 35}, 20);
		Y += 42;
		// Unity flattens all target records and sorts by reportUniqueIndex, not turn.
		// Multiple events in one turn, healing and reinforcement must remain visible.
		TArray<FRow> Events;
		for (const auto& Target : Array(Report, TEXT("battleReport")))
		{
			if (!Target || Target->Type != EJson::Object)
			{
				continue;
			}
			for (const auto& Value : Array(Target->AsObject(), TEXT("battleDamageHeal")))
			{
				if (Value && Value->Type == EJson::Object)
				{
					Events.Add(Value->AsObject());
				}
			}
		}
		Events.StableSort([](const FRow& A, const FRow& B) { return Number(A, TEXT("reportUniqueIndex")) < Number(B, TEXT("reportUniqueIndex")); });
		const int64 SelfIndex = Number(Self, TEXT("objectIndex"));
		const int64 MaxArmy = FMath::Max<int64>(1, Number(Self, TEXT("maxArmyCount")));
		const FVector2D Origin(70, Y), Extent(693, 130);
		for (int32 Tick = 0; Tick <= 3; ++Tick)
		{
			Text(NAME_None, Count(FMath::RoundToInt64(static_cast<double>(MaxArmy) * (3 - Tick) / 3)),
				{7, Y + Tick * Extent.Y / 3 - 10}, {61, 26}, 16, Secondary);
		}
		PlaceOnCanvas(Content, MakeSolidBorder(NAME_None, Stripe), {Origin.X, Origin.Y + Extent.Y}, {Extent.X, 2});
		if (Events.IsEmpty())
		{
			Text(TEXT("BattleReportNoCurve"), TEXT("本场战报没有兵力变化记录。"), {115, Y + 48}, {630, 30}, 19, Secondary);
		}
		else
		{
			auto Point = [&](double Fraction, int64 Army)
			{
				return Origin + FVector2D(Fraction * Extent.X, (1.0 - FMath::Clamp(static_cast<double>(Army) / MaxArmy, 0.0, 1.0)) * Extent.Y);
			};
			FVector2D Previous = Point(0, Number(Self, TEXT("beginArmyCount")));
			for (int32 Index = 0; Index < Events.Num(); ++Index)
			{
				const auto& Event = Events[Index];
				const bool bAttacking = Number(Event, TEXT("attackIndex")) == SelfIndex;
				if (!bAttacking && Number(Event, TEXT("defenseIndex")) != SelfIndex)
				{
					continue;
				}
				const FVector2D Next = Point(static_cast<double>(Index + 1) / Events.Num(),
					Number(Event, bAttacking ? TEXT("attackArmyCount") : TEXT("defenseArmyCount")));
				const FVector2D Delta = Next - Previous;
				auto* Segment = MakeSolidBorder(*FString::Printf(TEXT("BattleReportCurveSegment_%d"), Index), FLinearColor(0.12f, 0.68f, 0.89f));
				Segment->SetRenderTransformPivot(FVector2D(0, 0.5));
				Segment->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
				PlaceOnCanvas(Content, Segment, Previous - FVector2D(0, 2), {Delta.Size(), 4}, 1);
				Previous = Next;
			}
		}
		Y += 143;
		const int64 Begin = Number(Report, TEXT("battleBeginTime")), End = Number(Report, TEXT("battleEndTime"));
		if (Begin > 0 && End >= Begin && End < 253402300799LL)
		{
			Text(TEXT("BattleReportStartTime"), (FDateTime::FromUnixTimestamp(Begin) + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%H:%M:%S")), {22, Y}, {350, 27}, 18);
			Text(TEXT("BattleReportEndTime"), (FDateTime::FromUnixTimestamp(End) + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%H:%M:%S")), {652, Y}, {140, 27}, 18);
		}
		Y += 36;
		for (const auto& Opponent : Opponents)
		{
			FString OpponentName = EnemyLabel(Config, Number(Opponent, TEXT("monsterId")));
			if (OpponentName.IsEmpty())
			{
				Opponent->TryGetStringField(TEXT("name"), OpponentName);
			}
			PlaceOnCanvas(Content, MakeSolidBorder(NAME_None, FLinearColor(0.48f, 0.34f, 0.12f)), {60, Y}, {703, 32});
			Text(NAME_None, OpponentName, {75, Y + 3}, {680, 28}, 19);
			Y += 40;
			Text(NAME_None, TEXT("参战 ") + Count(Number(Opponent, TEXT("beginArmyCount"))) + TEXT("    剩余 ")
				+ Count(Number(Opponent, TEXT("endArmyCount"))), {75, Y}, {680, 28}, 18, Secondary);
			Y += 34;
			const auto EnemyHurt = Casualties(Opponent);
			if (EnemyHurt.bKnown)
			{
				Text(NAME_None, FString::Printf(TEXT("轻伤 %s    重伤 %s    阵亡 %s"), *Count(EnemyHurt.Minor),
					*Count(EnemyHurt.Severe), *Count(EnemyHurt.Dead)), {75, Y}, {680, 28}, 18, Secondary);
				Y += 34;
			}
		}
		const auto Hurt = Casualties(Self);
		if (Hurt.bKnown)
		{
			Text(TEXT("BattleReportCasualties"), FString::Printf(TEXT("轻伤 %s    重伤 %s    阵亡 %s"),
				*Count(Hurt.Minor), *Count(Hurt.Severe), *Count(Hurt.Dead)), {24, Y}, {750, 32}, 19, Secondary);
			Y += 40;
		}
	}
	const int64 Exp = Number(Report, TEXT("mainHeroExp"));
	const int64 DeputyExp = Number(Report, TEXT("deputyHeroExp"));
	if (Exp > 0 || DeputyExp > 0)
	{
		Text(TEXT("BattleReportExperience"), FString::Printf(TEXT("主将经验 +%s    副将经验 +%s"), *Count(Exp), *Count(DeputyExp)),
			{24, Y}, {750, 30}, 19, Secondary);
		Y += 38;
	}
	const int64 BeginTime = Number(Report, TEXT("battleBeginTime")), EndTime = Number(Report, TEXT("battleEndTime"));
	if (BeginTime > 0 && EndTime >= BeginTime)
	{
		Text(TEXT("BattleReportDuration"), FString::Printf(TEXT("战斗时长 %lld 秒"), EndTime - BeginTime), {24, Y}, {750, 30}, 18, Secondary);
		Y += 38;
	}
	FRokWorldMapProjection Projection;
	if (UWorld* World = GetWorld())
	{
		if (TActorIterator<ARokWorldMapConfigActor> It(World); It)
		{
			Projection = It->Projection;
		}
	}
	for (const auto& Opponent : Opponents)
	{
		if (const auto Position = Object(Opponent, TEXT("pos")))
		{
			const FVector2D Display = Projection.UnityToDisplay(FVector2D(Number(Position, TEXT("x")), Number(Position, TEXT("y"))) / 100.0);
			Text(NAME_None, FString::Printf(TEXT("X:%.0f Y:%.0f"), Display.X, Display.Y), {24, Y}, {750, 30}, 18, Secondary);
			Y += 38;
		}
	}
	if (bMountedWbp)
	{
		DetailSize->SetHeightOverride(Y + 18.0f);
	}
	else
	{
		Detail->AddChild(WrapFixedSize(Content, NAME_None, {805, Y + 18}));
	}
}

bool URokUnityFeatureScreenWidget::TryBuildBattleReportOverview(URokUiPageWidget& Page,
	const TSharedPtr<FJsonObject>& Mail, const TSharedPtr<FJsonObject>& Report, int64 RoleId, bool bDownloadFailed)
{
	auto* Body = Cast<UVerticalBox>(RokMailWbp::Find(Page, TEXT("BattleReportOverviewBody")));
	auto* RewardHost = Cast<UWrapBox>(RokMailWbp::Find(Page, TEXT("BattleReportRewardCards")));
	auto* OpponentHost = Cast<UVerticalBox>(RokMailWbp::Find(Page, TEXT("BattleReportOpponentRows")));
	auto* Plot = Cast<UCanvasPanel>(RokMailWbp::Find(Page, TEXT("BattleReportCurvePlot")));
	auto* LineTemplate = Cast<UImage>(RokMailWbp::Find(Page, TEXT("BattleReportCurveLineTemplate")));
	auto* PointTemplate = Cast<UImage>(RokMailWbp::Find(Page, TEXT("BattleReportCurvePointTemplate")));
	UClass* ArmyClass = LoadClass<URokUiPageWidget>(nullptr,
		TEXT("/Game/RokUI/Feature/WBP_RokFeature_BattleReportParticipantCard.WBP_RokFeature_BattleReportParticipantCard_C"));
	UClass* RewardClass = LoadClass<URokUiPageWidget>(nullptr,
		TEXT("/Game/RokUI/Feature/WBP_RokFeature_BattleReportLootCard.WBP_RokFeature_BattleReportLootCard_C"));
	if (!Body || !RewardHost || !OpponentHost || !Plot || !LineTemplate || !PointTemplate || !ArmyClass || !RewardClass)
	{
		return false;
	}
	auto* ArmyPrototype = CreateWidget<URokUiPageWidget>(this, ArmyClass);
	auto* RewardPrototype = CreateWidget<URokUiPageWidget>(this, RewardClass);
	const FVector2D ArmySize = ArmyPrototype ? RokMailWbp::Size(*ArmyPrototype, TEXT("BattleReportArmyLayout")) : FVector2D::ZeroVector;
	const FVector2D RewardSize = RewardPrototype ? RokMailWbp::Size(*RewardPrototype, TEXT("BattleReportRewardLayout")) : FVector2D::ZeroVector;
	const FVector2D PlotSize = RokMailWbp::Size(Page, TEXT("BattleReportCurvePlot"));
	const FVector2D LineSize = RokMailWbp::Size(Page, TEXT("BattleReportCurveLineTemplate"));
	const FVector2D PointSize = RokMailWbp::Size(Page, TEXT("BattleReportCurvePointTemplate"));
	if (ArmySize.X <= 0 || ArmySize.Y <= 0 || RewardSize.X <= 0 || RewardSize.Y <= 0 || PlotSize.X <= 0 || PlotSize.Y <= 0 || LineSize.Y <= 0)
	{
		return false;
	}
	auto* Config = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	RokMailWbp::SetText(Page, TEXT("BattleReportTitle"), ReportTitle(Config, Mail));
	RokMailWbp::SetText(Page, TEXT("BattleReportOpponent"), TEXT("与") + ReportSubject(Config, Mail) + TEXT("发生了战斗"));
	RokMailWbp::SetText(Page, TEXT("BattleReportTime"), RokMailWbp::LocalTime(Number(Mail, TEXT("sendTime"))));
	RokMailWbp::SetText(Page, TEXT("BattleReportPending"), bDownloadFailed ? TEXT("战报获取失败，请重新选择邮件重试。") : TEXT("正在获取战斗详情…"));
	RokMailWbp::Show(Page, TEXT("BattleReportPending"), !Report);
	for (const TCHAR* Section : {TEXT("BattleReportRewardsSection"), TEXT("BattleReportPlayerSectionSize"),
		TEXT("BattleReportCurveSectionSize"), TEXT("BattleReportMetadata"), TEXT("BattleReportOpponentHeading"), TEXT("BattleReportOpponentRows")})
	{
		RokMailWbp::Show(Page, Section, Report.IsValid());
	}
	if (!Report)
	{
		return true;
	}
	FRow Self;
	TArray<FRow> Opponents;
	for (const auto& Value : Array(Report, TEXT("objectInfos")))
	{
		if (!Value || Value->Type != EJson::Object)
		{
			continue;
		}
		const FRow Row = Value->AsObject();
		if (RoleId > 0 && Number(Row, TEXT("rid")) == RoleId && !Self)
		{
			Self = Row;
		}
		else
		{
			Opponents.Add(Row);
		}
	}
	const auto Rewards = Object(Report, TEXT("rewardInfo"));
	const auto Payload = RokMailWbp::ReadRewards(Rewards);
	const auto Loot = RokMailRewardDetails::Normalize(Payload, GetEconomyCatalogSubsystem(), GetGameplayCatalogSubsystem());
	RewardHost->ClearChildren();
	for (const auto& Reward : Loot)
	{
		if (auto* Card = CreateWidget<URokUiPageWidget>(this, RewardClass))
		{
			Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			BindLiveMailRewardCard(*Card, Reward, TEXT("BattleReportReward"));
			RewardHost->AddChild(WrapFixedSize(Card, NAME_None, RewardSize));
		}
	}
	RokMailWbp::Show(Page, TEXT("BattleReportEmptyRewards"), Loot.IsEmpty());
	RokMailWbp::Show(Page, TEXT("BattleReportRewardCards"), !Loot.IsEmpty());
	RokMailWbp::Show(Page, TEXT("BattleReportPlayerSectionSize"), Self.IsValid());
	RokMailWbp::Show(Page, TEXT("BattleReportCurveSectionSize"), Self.IsValid());
	RokMailWbp::Show(Page, TEXT("BattleReportMissingSelf"), !Self);
	RokMailWbp::Show(Page, TEXT("BattleReportCasualties"), Self.IsValid());
	Plot->ClearChildren();
	if (Self)
	{
		FString Name, Guild;
		Self->TryGetStringField(TEXT("name"), Name);
		Self->TryGetStringField(TEXT("guildName"), Guild);
		RokMailWbp::SetText(Page, TEXT("BattleReportPlayer"), Guild.IsEmpty() ? Name : TEXT("[") + Guild + TEXT("] ") + Name);
		RokMailWbp::SetText(Page, TEXT("BattleReportArmyTotal"), TEXT("参战部队 ") + Count(Number(Self, TEXT("beginArmyCount"))));
		RokMailWbp::SetText(Page, TEXT("BattleReportArmyRemaining"), TEXT("剩余部队 ") + Count(Number(Self, TEXT("endArmyCount"))));
		const FString Hero = ConfigText(Config, TEXT("Hero"), Number(Self, TEXT("mainHeroId")), TEXT("l_nameID"));
		RokMailWbp::SetText(Page, TEXT("BattleReportHero"), Hero.IsEmpty() ? TEXT("驻防部队")
			: FString::Printf(TEXT("等级%lld %s"), Number(Self, TEXT("mainHeroLevel")), *Hero));
		const auto Hurt = Casualties(Self);
		RokMailWbp::SetText(Page, TEXT("BattleReportCasualties"), Hurt.bKnown
			? FString::Printf(TEXT("我方轻伤 %s    重伤 %s    阵亡 %s"), *Count(Hurt.Minor), *Count(Hurt.Severe), *Count(Hurt.Dead))
			: TEXT("我方伤亡信息暂不可用。"));
		TArray<FRow> Events;
		for (const auto& Target : Array(Report, TEXT("battleReport")))
		{
			if (!Target || Target->Type != EJson::Object)
			{
				continue;
			}
			for (const auto& Value : Array(Target->AsObject(), TEXT("battleDamageHeal")))
			{
				if (Value && Value->Type == EJson::Object)
				{
					Events.Add(Value->AsObject());
				}
			}
		}
		Events.StableSort([](const FRow& A, const FRow& B) { return Number(A, TEXT("reportUniqueIndex")) < Number(B, TEXT("reportUniqueIndex")); });
		const int64 SelfIndex = Number(Self, TEXT("objectIndex"));
		const int64 ReportMaxArmy = Number(Self, TEXT("maxArmyCount"));
		const int64 MaxArmy = FMath::Max<int64>(1, ReportMaxArmy);
		for (int32 Tick = 0; Tick <= 3; ++Tick)
		{
			RokMailWbp::SetText(Page, *FString::Printf(TEXT("BattleReportCurveAxis_%d"), Tick),
				ReportMaxArmy > 0 ? Count(FMath::RoundToInt64(static_cast<double>(MaxArmy) * (3 - Tick) / 3)) : TEXT("—"));
		}
		auto Point = [PlotSize, MaxArmy](double Fraction, int64 Count)
		{
			return FVector2D(Fraction * PlotSize.X, (1.0 - FMath::Clamp(static_cast<double>(Count) / MaxArmy, 0.0, 1.0)) * PlotSize.Y);
		};
		FVector2D Previous = Point(0, Number(Self, TEXT("beginArmyCount")));
		bool bHasCurve = false;
		for (int32 Index = 0; Index < Events.Num() && ReportMaxArmy > 0; ++Index)
		{
			const auto& Event = Events[Index];
			const bool bAttacking = Number(Event, TEXT("attackIndex")) == SelfIndex;
			if (!bAttacking && Number(Event, TEXT("defenseIndex")) != SelfIndex)
			{
				continue;
			}
			const FVector2D Next = Point(static_cast<double>(Index + 1) / Events.Num(), Number(Event,
				bAttacking ? TEXT("attackArmyCount") : TEXT("defenseArmyCount")));
			const FVector2D Delta = Next - Previous;
			auto* Segment = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("BattleReportCurveSegment_%d"), Index));
			Segment->SetBrush(LineTemplate->GetBrush());
			Segment->SetVisibility(ESlateVisibility::HitTestInvisible);
			Segment->SetRenderTransformPivot(FVector2D(0, 0.5));
			Segment->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
			UCanvasPanelSlot* Slot = Plot->AddChildToCanvas(Segment);
			Slot->SetPosition(Previous - FVector2D(0, LineSize.Y / 2));
			Slot->SetSize({Delta.Size(), LineSize.Y});
			Previous = Next;
			bHasCurve = true;
		}
		if (bHasCurve)
		{
			for (const FVector2D Endpoint : {Point(0, Number(Self, TEXT("beginArmyCount"))), Previous})
			{
				auto* Dot = WidgetTree->ConstructWidget<UImage>();
				Dot->SetBrush(PointTemplate->GetBrush());
				Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
				auto* Slot = Plot->AddChildToCanvas(Dot);
				Slot->SetPosition(Endpoint - PointSize / 2);
				Slot->SetSize(PointSize);
			}
		}
		RokMailWbp::Show(Page, TEXT("BattleReportNoCurve"), !bHasCurve);
		RokMailWbp::SetText(Page, TEXT("BattleReportStartTime"), RokMailWbp::LocalTime(Number(Report, TEXT("battleBeginTime")), TEXT("%H:%M:%S")));
		RokMailWbp::SetText(Page, TEXT("BattleReportEndTime"), RokMailWbp::LocalTime(Number(Report, TEXT("battleEndTime")), TEXT("%H:%M:%S")));
	}
	const int64 MainExp = Number(Report, TEXT("mainHeroExp")), DeputyExp = Number(Report, TEXT("deputyHeroExp"));
	RokMailWbp::Show(Page, TEXT("BattleReportExperience"), MainExp > 0 || DeputyExp > 0);
	RokMailWbp::SetText(Page, TEXT("BattleReportExperience"), FString::Printf(TEXT("主将经验 +%s    副将经验 +%s"), *Count(MainExp), *Count(DeputyExp)));
	const int64 Begin = Number(Report, TEXT("battleBeginTime")), End = Number(Report, TEXT("battleEndTime"));
	RokMailWbp::Show(Page, TEXT("BattleReportDuration"), Begin > 0 && End >= Begin);
	RokMailWbp::SetText(Page, TEXT("BattleReportDuration"), FString::Printf(TEXT("战斗时长 %lld 秒"), FMath::Max<int64>(0, End - Begin)));
	OpponentHost->ClearChildren();
	for (const auto& Opponent : Opponents)
	{
		auto* Row = CreateWidget<URokUiPageWidget>(this, ArmyClass);
		if (!Row)
		{
			continue;
		}
		FString Name = EnemyLabel(Config, Number(Opponent, TEXT("monsterId")));
		if (Name.IsEmpty())
		{
			Opponent->TryGetStringField(TEXT("name"), Name);
		}
		RokMailWbp::SetText(*Row, TEXT("BattleReportArmyName"), Name);
		RokMailWbp::SetText(*Row, TEXT("BattleReportArmyCounts"), TEXT("参战 ") + Count(Number(Opponent, TEXT("beginArmyCount")))
			+ TEXT("    剩余 ") + Count(Number(Opponent, TEXT("endArmyCount"))));
		const auto Hurt = Casualties(Opponent);
		RokMailWbp::SetText(*Row, TEXT("BattleReportArmyCasualties"), Hurt.bKnown
			? FString::Printf(TEXT("轻伤 %s    重伤 %s    阵亡 %s"), *Count(Hurt.Minor), *Count(Hurt.Severe), *Count(Hurt.Dead))
			: TEXT("伤亡信息暂不可用。"));
		if (const auto Position = Object(Opponent, TEXT("pos")))
		{
			FRokWorldMapProjection Projection;
			if (UWorld* World = GetWorld())
			{
				if (TActorIterator<ARokWorldMapConfigActor> It(World); It)
				{
					Projection = It->Projection;
				}
			}
			const FVector2D Display = Projection.UnityToDisplay(FVector2D(Number(Position, TEXT("x")), Number(Position, TEXT("y"))) / 100.0);
			RokMailWbp::SetText(*Row, TEXT("BattleReportArmyPosition"), FString::Printf(TEXT("X:%.0f Y:%.0f"), Display.X, Display.Y));
		}
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		OpponentHost->AddChild(WrapFixedSize(Row, NAME_None, ArmySize));
	}
	RokMailWbp::Show(Page, TEXT("BattleReportOpponentHeading"), !Opponents.IsEmpty());
	return true;
}
