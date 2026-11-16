#include "RokUnityFeatureScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Containers/SortedMap.h"
#include "EngineUtils.h"
#include "RokGameNetSubsystem.h"
#include "RokUiTextTable.h"
#include "RokUnityConfigSubsystem.h"
#include "RokWorldMapConfigActor.h"

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

void URokUnityFeatureScreenWidget::RefreshBattleReportMail()
{
	if (bLocalMailUnreadFixtureState || bReferenceMailCaptureState || bReadOnlyMailRewardFixtureState) return;
	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (Net && ShownBattleReportRevision != Net->GetBattleReportRevision()) RebuildModeContent();
}

void URokUnityFeatureScreenWidget::SelectBattleReport(int64 EmailIndex)
{
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
	ShownBattleReportRevision = Net ? Net->GetBattleReportRevision() : 0;
	TArray<int64> Ids;
	if (Net)
	{
		Net->GetBattleReportMail().GenerateKeyArray(Ids);
		Ids.Sort([Net](int64 A, int64 B)
		{
			const int64 TimeA = Number(Net->GetBattleReportMail().FindChecked(A), TEXT("sendTime"));
			const int64 TimeB = Number(Net->GetBattleReportMail().FindChecked(B), TEXT("sendTime"));
			return TimeA == TimeB ? A > B : TimeA > TimeB;
		});
	}
	if (!Ids.Contains(SelectedBattleReport)) SelectedBattleReport = Ids.IsEmpty() ? 0 : Ids[0];
	auto Label = [this](UCanvasPanel* Parent, FName Name, const FString& Text, FVector2D Pos, FVector2D Size,
		int32 Font, FLinearColor Color = FLinearColor::White, bool bBold = false)
	{
		auto* Block = MakeScreenText(Name, Font, Color, bBold);
		Block->SetText(FText::FromString(Text));
		PlaceOnCanvas(Parent, Block, Pos, Size, 2);
	};
	for (const int64 Id : Ids)
	{
		const auto& Mail = Net->GetBattleReportMail().FindChecked(Id);
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
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("BattleReportDetailPanel"), Slate), {547, 128}, {830, 590}, 3);
	if (!SelectedBattleReport)
	{
		auto* Empty = MakeScreenText(TEXT("BattleReportEmpty"), 24, Secondary);
		Empty->SetText(FText::FromString(TEXT("暂无战斗报告")));
		PlaceOnCanvas(ModeCanvas, Empty, {604, 215}, {690, 44}, 5);
		return;
	}
	const auto& Mail = Net->GetBattleReportMail().FindChecked(SelectedBattleReport);
	const auto Report = Object(Mail, TEXT("battleReportExContent"));
	if (!Report) Net->RequestBattleReport(SelectedBattleReport);

	auto* Detail = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BattleReportDetailScroll"));
	Detail->SetAllowOverscroll(false);
	PlaceOnCanvas(ModeCanvas, Detail, {552, 132}, {820, 582}, 5);
	auto* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
	PlaceOnCanvas(Content, MakeAtlasImage(NAME_None, MailAtlas, ReportPoster, AtlasSize, {803, 118}), {2, 1}, {803, 118});
	FString EnemyName = ReportSubject(Config, Mail);
	Label(Content, TEXT("BattleReportTitle"), ReportTitle(Config, Mail), {22, 12}, {520, 38}, 29, Gold, true);
	Label(Content, TEXT("BattleReportOpponent"), EnemyName, {23, 60}, {600, 28}, 22);
	const int64 SendTime = Number(Mail, TEXT("sendTime"));
	if (SendTime > 0 && SendTime < 253402300799LL)
		Label(Content, NAME_None, FDateTime::FromUnixTimestamp(SendTime).ToString(TEXT("%Y/%m/%d %H:%M UTC")),
			{577, 87}, {224, 26}, 15, Secondary);
	if (!Report)
	{
		Label(Content, TEXT("BattleReportPending"), Net->HasBattleReportDownloadFailed(SelectedBattleReport)
			? TEXT("战报下载失败，点选左侧邮件重试。") : TEXT("正在获取战斗详情…"), {24, 154}, {750, 44}, 23, Secondary);
		Detail->AddChild(WrapFixedSize(Content, NAME_None, {805, 245}));
		return;
	}

	FRow Self, Enemy;
	for (const auto& Value : Array(Report, TEXT("objectInfos")))
	{
		if (!Value || Value->Type != EJson::Object) continue;
		const auto& Row = Value->AsObject();
		if (Number(Row, TEXT("rid")) == Net->GetRoleId() && Net->GetRoleId() > 0) Self = Row;
		else if (!Enemy) Enemy = Row;
	}
	const int64 SelfIndex = Number(Self, TEXT("objectIndex"));
	const int64 EnemyIndex = Number(Enemy, TEXT("objectIndex"));
	const FString MonsterName = EnemyLabel(Config, Number(Enemy, TEXT("monsterId")));
	if (!MonsterName.IsEmpty()) EnemyName = MonsterName;
	FString SelfName;
	if (Self) Self->TryGetStringField(TEXT("name"), SelfName);
	if (SelfName.IsEmpty()) SelfName = TEXT("我方");
	const int64 Duration = FMath::Max<int64>(0, Number(Report, TEXT("battleEndTime")) - Number(Report, TEXT("battleBeginTime")));
	const auto Position = Object(Enemy, TEXT("pos"));
	FRokWorldMapProjection Projection;
	if (UWorld* World = GetWorld())
	{
		if (TActorIterator<ARokWorldMapConfigActor> It(World); It) Projection = It->Projection;
	}
	const FVector2D DisplayPosition = Projection.UnityToDisplay(FVector2D(Number(Position, TEXT("x")), Number(Position, TEXT("y"))) / 100.0);
	Label(Content, TEXT("BattleReportDuration"), Position
		? FString::Printf(TEXT("战斗时长 %lld 秒   ·   X:%.0f Y:%.0f"), Duration, DisplayPosition.X, DisplayPosition.Y)
		: FString::Printf(TEXT("战斗时长 %lld 秒"), Duration), {23, 131}, {750, 30}, 18, Secondary);

	float Y = 170;
	Label(Content, NAME_None, TEXT("部队战况"), {23, Y}, {750, 34}, 22, Gold, true);
	Y += 42;
	const TCHAR* Columns[] = {TEXT("部队"), TEXT("参战"), TEXT("剩余"), TEXT("轻伤"), TEXT("重伤"), TEXT("阵亡")};
	const float X[] = {24, 281, 389, 497, 605, 713};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Columns); ++Index)
		Label(Content, NAME_None, Columns[Index], {X[Index], Y}, {100, 27}, 17, Secondary);
	Y += 32;
	auto Side = [&](const FRow& Army, const FString& Name)
	{
		PlaceOnCanvas(Content, MakeSolidBorder(NAME_None, Stripe), {12, Y}, {784, 41});
		const auto Hurt = Casualties(Army);
		const FString Cells[] = {Name, Army ? Count(Number(Army, TEXT("beginArmyCount"))) : TEXT("—"),
			Army ? Count(Number(Army, TEXT("endArmyCount"))) : TEXT("—"), Hurt.bKnown ? Count(Hurt.Minor) : TEXT("—"),
			Hurt.bKnown ? Count(Hurt.Severe) : TEXT("—"), Hurt.bKnown ? Count(Hurt.Dead) : TEXT("—")};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cells); ++Index)
			Label(Content, NAME_None, Cells[Index], {X[Index], Y + 7}, {Index == 0 ? 245.0 : 102.0, 27}, 19);
		Y += 45;
	};
	Side(Self, SelfName);
	Side(Enemy, EnemyName);
	Y += 14;
	Label(Content, NAME_None, TEXT("战斗奖励"), {23, Y}, {750, 32}, 22, Gold, true);
	Y += 36;
	const auto Rewards = Object(Report, TEXT("rewardInfo"));
	FRokMailRewardPayload Payload;
	Payload.Food = Number(Rewards, TEXT("food"));
	Payload.Wood = Number(Rewards, TEXT("wood"));
	Payload.Stone = Number(Rewards, TEXT("stone"));
	Payload.Gold = Number(Rewards, TEXT("gold"));
	Payload.Denar = Number(Rewards, TEXT("denar"));
	for (const auto& Value : Array(Rewards, TEXT("items")))
		if (Value && Value->Type == EJson::Object)
			Payload.Items.Add({Number(Value->AsObject(), TEXT("itemId")), Number(Value->AsObject(), TEXT("itemNum"))});
	const auto Loot = RokMailRewardDetails::Normalize(Payload, GetEconomyCatalogSubsystem(), GetGameplayCatalogSubsystem());
	const int64 Exp = Number(Report, TEXT("mainHeroExp"));
	const int64 DeputyExp = Number(Self, TEXT("deputyHeroId")) > 0 ? Number(Report, TEXT("deputyHeroExp")) : 0;
	if (Loot.IsEmpty() && Exp == 0 && DeputyExp == 0)
	{
		Label(Content, NAME_None, TEXT("无"), {24, Y}, {750, 25}, 19, Secondary);
		Y += 29;
	}
	for (int32 Index = 0; Index < Loot.Num(); ++Index)
	{
		const auto& Item = Loot[Index];
		const float Left = 22 + (Index % 3) * 260;
		const float Top = Y + (Index / 3) * 48;
		const TCHAR* Texture = nullptr;
		FIntRect Rect;
		FIntPoint TextureSize;
		if (ResolveMailRewardIcon(Item, Texture, Rect, TextureSize))
			PlaceOnCanvas(Content, MakeAtlasImage(NAME_None, Texture, Rect, TextureSize, {40, 40}), {Left, Top}, {40, 40}, 1);
		Label(Content, NAME_None, Item.DisplayName + TEXT(" ×") + Count(Item.Quantity), {Left + 44, Top + 8}, {210, 30}, 17);
	}
	Y += FMath::DivideAndRoundUp(Loot.Num(), 3) * 48;
	if (Exp > 0 || DeputyExp > 0)
	{
		FString Experience = TEXT("主将经验 +") + Count(Exp);
		if (DeputyExp > 0) Experience += TEXT("     副将经验 +") + Count(DeputyExp);
		Label(Content, TEXT("BattleReportExperience"), Experience, {24, Y}, {750, 29}, 19);
		Y += 36;
	}

	Y += 12;
	Label(Content, NAME_None, TEXT("战斗回合"), {23, Y}, {750, 34}, 22, Gold, true);
	Y += 39;
	struct FRound { int64 SelfCount = 0, EnemyCount = 0; };
	TSortedMap<int64, FRound> Rounds;
	for (const auto& Target : Array(Report, TEXT("battleReport")))
	{
		if (!Target || Target->Type != EJson::Object) continue;
		for (const auto& Value : Array(Target->AsObject(), TEXT("battleDamageHeal")))
		{
			if (!Value || Value->Type != EJson::Object) continue;
			const auto& Entry = Value->AsObject();
			const int64 Attacker = Number(Entry, TEXT("attackIndex"));
			const int64 Defender = Number(Entry, TEXT("defenseIndex"));
			if (!SelfIndex || !EnemyIndex || !((Attacker == SelfIndex && Defender == EnemyIndex)
				|| (Attacker == EnemyIndex && Defender == SelfIndex))) continue;
			auto& Round = Rounds.FindOrAdd(Number(Entry, TEXT("turn")));
			const bool bSelf = Attacker == SelfIndex;
			(bSelf ? Round.SelfCount : Round.EnemyCount) = Number(Entry, TEXT("attackArmyCount"));
			(bSelf ? Round.EnemyCount : Round.SelfCount) = Number(Entry, TEXT("defenseArmyCount"));
		}
	}
	if (Rounds.IsEmpty())
	{
		Label(Content, NAME_None, TEXT("本场战报没有回合记录。"), {24, Y}, {750, 29}, 18, Secondary);
		Y += 32;
	}
	for (const auto& Pair : Rounds)
	{
		const auto& Round = Pair.Value;
		PlaceOnCanvas(Content, MakeSolidBorder(NAME_None, Stripe), {12, Y}, {784, 33});
		const FString Summary = FString::Printf(TEXT("第 %lld 回合     我方剩余 %s     敌方剩余 %s"), Pair.Key,
			*Count(Round.SelfCount), *Count(Round.EnemyCount));
		Label(Content, NAME_None, Summary, {24, Y + 4}, {760, 28}, 17);
		Y += 36;
	}
	Detail->AddChild(WrapFixedSize(Content, NAME_None, {805, Y + 18}));
}
