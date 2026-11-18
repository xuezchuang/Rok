#include "RokUnityFeatureScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Internationalization/Regex.h"
#include "RokGameNetSubsystem.h"
#include "RokMailWbpBinding.h"
#include "Components/WrapBox.h"
#include "Components/Image.h"
#include "RokUiTextTable.h"
#include "RokUnityConfigSubsystem.h"
#include "RokUiWidgets.h"

namespace
{
using FMail = TSharedPtr<FJsonObject>;
int64 MailNumber(const FMail& Row, const TCHAR* Field)
{
	int64 Value = 0;
	RokSprotoJson::TryGetInt64(Row, Field, Value);
	return Value;
}
FMail MailObject(const FMail& Row, const TCHAR* Field)
{
	const FMail* Value = nullptr;
	return Row && Row->TryGetObjectField(Field, Value) ? *Value : nullptr;
}
FString MailText(URokUnityConfigSubsystem* Config, const FMail& Mail, const TCHAR* Field, const TCHAR* Arguments)
{
	int32 TextId = 0;
	if (Config)
	{
		Config->GetIntFieldByKey(TEXT("Mail"), TEXT("ID"), MailNumber(Mail, TEXT("emailId")), Field, TextId);
	}
	FString Text = FRokUiTextTable::Resolve(TextId);
	// Unity's action-force-return mail supplies a scalar instead of emailContents.
	if (FCString::Strcmp(Field, TEXT("l_mesID")) == 0 && MailNumber(Mail, TEXT("subType")) == 4)
	{
		Text.ReplaceInline(TEXT("{0}"), *LexToString(MailNumber(Mail, TEXT("acitonForceReturn"))));
	}
	const TArray<TSharedPtr<FJsonValue>>* Args = nullptr;
	if (Mail && Mail->TryGetArrayField(Arguments, Args))
	{
		// EmailProxy.OnTextFormat: plain arguments and typed Unity placeholders.
		const FRegexPattern Pattern(TEXT("\\{([A-Za-z0-9]+)_([0-9]+)\\}"));
		FRegexMatcher Matcher(Pattern, Text);
		TMap<FString, FString> Replacements;
		while (Matcher.FindNext())
		{
			const int32 Index = FCString::Atoi(*Matcher.GetCaptureGroup(2));
			if (!Args->IsValidIndex(Index))
			{
				continue;
			}
			FString Value = (*Args)[Index]->AsString();
			const FString Kind = Matcher.GetCaptureGroup(1);
			const TCHAR* Table = Kind == TEXT("item") ? TEXT("Item") : Kind == TEXT("hero") ? TEXT("Hero")
				: Kind == TEXT("monster") ? TEXT("Monster") : Kind == TEXT("activity") ? TEXT("ActivityCalendar")
				: Kind == TEXT("activityKill") ? TEXT("ActivityKillType") : Kind == TEXT("price") ? TEXT("Price")
				: Kind == TEXT("allianceStudy") ? TEXT("AllianceStudy") : nullptr;
			int32 NameId = 0;
			if (Config && Table && Config->GetIntFieldByKey(Table, TEXT("ID"), FCString::Atoi64(*Value),
				Kind == TEXT("monster") ? TEXT("l_nameId") : TEXT("l_nameID"), NameId))
			{
				Value = FRokUiTextTable::Resolve(NameId);
			}
			else if (Kind == TEXT("language"))
			{
				Value = FRokUiTextTable::Resolve(FCString::Atoi(*Value));
			}
			else if (Kind == TEXT("name"))
			{
				FString Guild, Name;
				if (Value.Split(TEXT(","), &Guild, &Name))
				{
					Value = Guild.IsEmpty() ? Name : TEXT("[") + Guild + TEXT("] ") + Name;
				}
			}
			else if (Kind == TEXT("coordinate") || Kind == TEXT("coordinateK"))
			{
				TArray<FString> Parts;
				Value.ParseIntoArray(Parts, TEXT(","), false);
				if (Parts.Num() == 2)
				{
					Value = FString::Printf(TEXT("X:%d Y:%d"), FMath::FloorToInt(FCString::Atod(*Parts[0]) / 600.0),
						FMath::FloorToInt(FCString::Atod(*Parts[1]) / 600.0));
				}
			}
			Replacements.Add(Matcher.GetCaptureGroup(0), Value);
		}
		for (const auto& Pair : Replacements)
		{
			Text.ReplaceInline(*Pair.Key, *Pair.Value);
		}
		for (int32 Index = Args->Num() - 1; Index >= 0; --Index)
		{
			Text.ReplaceInline(*FString::Printf(TEXT("{%d}"), Index), *(*Args)[Index]->AsString());
		}
		if (Text.IsEmpty())
		{
			for (const auto& Value : *Args)
			{
				if (!Text.IsEmpty())
				{
					Text += TEXT("\n");
				}
				Text += Value->AsString();
			}
		}
	}
	return FRokUiTextTable::SanitizeRichText(Text);
}
}

int32 URokUnityFeatureScreenWidget::GetSessionMailUnreadCount(int32 Tab) const
{
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	static const int32 Types[] = {1, 2, 3, 99, 4, 5};
	if (!Net || !Config || Tab < 0 || Tab >= UE_ARRAY_COUNT(Types))
	{
		return 0;
	}
	int32 Count = 0;
	for (const auto& Pair : Net->GetSessionMail())
	{
		int32 Type = 0;
		bool bCollected = false;
		Pair.Value->TryGetBoolField(TEXT("isCollect"), bCollected);
		Config->GetIntFieldByKey(TEXT("Mail"), TEXT("ID"), MailNumber(Pair.Value, TEXT("emailId")), TEXT("type"), Type);
		if (MailNumber(Pair.Value, TEXT("status")) == 0 && (Tab == 3 ? bCollected : Type == Types[Tab]))
		{
			++Count;
		}
	}
	return Count;
}

void URokUnityFeatureScreenWidget::SelectSessionMail(int64 EmailIndex)
{
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || !Net->GetSessionMail().Contains(EmailIndex))
	{
		return;
	}
	SelectedSessionMail = EmailIndex;
	Net->RequestMailDetails(EmailIndex);
	Net->MarkMailRead(EmailIndex);
	RebuildModeContent();
}

void URokUnityFeatureScreenWidget::BuildSessionMail(UScrollBox* List)
{
	auto* Instance = GetGameInstance();
	auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	ShownBattleReportRevision = Net ? Net->GetBattleReportRevision() : 0;
	bShownMailInboxLoading = Net && Net->IsMailInboxLoading();
	// Unity's UI order differs from server EmailType: favourites is UI tab 3 / type 99.
	static const int32 Types[] = {1, 2, 3, 99, 4, 5};
	TArray<int64> Ids;
	if (Net)
	{
		for (const auto& Pair : Net->GetSessionMail())
		{
			int32 Type = 0;
			if (Config)
			{
				Config->GetIntFieldByKey(TEXT("Mail"), TEXT("ID"), MailNumber(Pair.Value, TEXT("emailId")), TEXT("type"), Type);
			}
			bool bCollected = false;
			Pair.Value->TryGetBoolField(TEXT("isCollect"), bCollected);
			if (SelectedMailTab >= 0 && SelectedMailTab < UE_ARRAY_COUNT(Types)
				&& (SelectedMailTab == 3 ? bCollected : Type == Types[SelectedMailTab]))
			{
				Ids.Add(Pair.Key);
			}
		}
		Ids.Sort([Net](int64 A, int64 B)
		{
			const auto& Mail = Net->GetSessionMail();
			const int64 TimeA = MailNumber(Mail.FindChecked(A), TEXT("sendTime"));
			const int64 TimeB = MailNumber(Mail.FindChecked(B), TEXT("sendTime"));
			return TimeA == TimeB ? A > B : TimeA > TimeB;
		});
	}
	if (!Ids.Contains(SelectedSessionMail))
	{
		SelectedSessionMail = Ids.IsEmpty() ? 0 : Ids[0];
		if (SelectedSessionMail)
		{
			Net->RequestMailDetails(SelectedSessionMail);
			Net->MarkMailRead(SelectedSessionMail);
		}
	}
	if (TryBuildSessionMailWbp(List, Ids))
	{
		return;
	}
	auto Label = [this](UCanvasPanel* Parent, FName Name, const FString& Text, FVector2D Position, FVector2D Size, int32 Font)
	{
		auto* Block = MakeScreenText(Name, Font, FLinearColor::White);
		Block->SetText(FText::FromString(Text));
		Block->SetAutoWrapText(true);
		PlaceOnCanvas(Parent, Block, Position, Size, 2);
	};
	for (const int64 Id : Ids)
	{
		const auto& Mail = Net->GetSessionMail().FindChecked(Id);
		auto* Button = MakeActionButton(*FString::Printf(TEXT("ServerMailRow_%lld"), Id), INDEX_NONE,
			nullptr, FIntRect(), FIntPoint(1, 1), {470, 108});
		Button->OnIndexedClicked.AddWeakLambda(this, [this, Id](int32) { SelectSessionMail(Id); });
		auto* Canvas = CastChecked<UCanvasPanel>(Button->GetContent());
		PlaceOnCanvas(Canvas, MakeSolidBorder(NAME_None, Id == SelectedSessionMail
			? FLinearColor(0.30f, 0.30f, 0.24f) : FLinearColor(0.11f, 0.11f, 0.10f)), {5, 4}, {458, 99});
		Label(Canvas, NAME_None, MailText(Config, Mail, TEXT("l_nameID"), TEXT("titleContents")), {20, 14}, {423, 32}, 23);
		Label(Canvas, NAME_None, MailText(Config, Mail, TEXT("l_subheadingID"), TEXT("subTitleContents")), {20, 51}, {422, 45}, 18);
		if (MailNumber(Mail, TEXT("status")) == 0)
		{
			Label(Canvas, NAME_None, TEXT("●"), {432, 10}, {24, 24}, 17);
		}
		List->AddChild(WrapFixedSize(Button, NAME_None, {470, 108}));
	}
	if (Ids.IsEmpty())
	{
		Label(ModeCanvas, TEXT("ServerMailEmpty"), bShownMailInboxLoading ? TEXT("正在获取邮件…") : TEXT("当前暂无邮件。"),
			{575, 205}, {760, 50}, 23);
		return;
	}
	const auto& Mail = Net->GetSessionMail().FindChecked(SelectedSessionMail);
	auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Scroll->SetAllowOverscroll(false);
	PlaceOnCanvas(ModeCanvas, Scroll, {552, 132}, {820, 582}, 6);
	auto* Content = WidgetTree->ConstructWidget<UCanvasPanel>();
	Label(Content, TEXT("ServerMailTitle"), MailText(Config, Mail, TEXT("l_nameID"), TEXT("titleContents")), {20, 12}, {760, 40}, 27);
	Label(Content, TEXT("ServerMailSubtitle"), MailText(Config, Mail, TEXT("l_subheadingID"), TEXT("subTitleContents")), {20, 58}, {760, 50}, 21);
	const int64 Time = MailNumber(Mail, TEXT("sendTime"));
	if (Time > 0 && Time < 253402300799LL)
	{
		Label(Content, TEXT("ServerMailTime"), (FDateTime::FromUnixTimestamp(Time) + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%Y/%m/%d %H:%M")), {20, 108}, {760, 25}, 17);
	}
	FString Body = MailText(Config, Mail, TEXT("l_mesID"), TEXT("emailContents"));
	if (Body.IsEmpty())
	{
		Body = TEXT("邮件正文暂不可用。");
	}
	const float BodyHeight = FMath::Max(160.0f, (Body.Len() / 38 + 1) * 30.0f);
	Label(Content, TEXT("ServerMailBody"), Body, {20, 155}, {760, BodyHeight}, 21);
	float Y = 175 + BodyHeight;
	const FString Sender = MailText(Config, Mail, TEXT("l_senderID"), TEXT("senderContents"));
	if (!Sender.IsEmpty())
	{
		Label(Content, TEXT("ServerMailSender"), Sender, {24, Y}, {750, 35}, 19);
		Y += 40;
	}
	const auto Rewards = MailObject(Mail, TEXT("rewards"));
	const auto Payload = RokMailWbp::ReadRewards(Rewards);
	RenderedMailRewardRows = RokMailRewardDetails::Normalize(Payload, GetEconomyCatalogSubsystem(), GetGameplayCatalogSubsystem());
	for (const auto& Reward : RenderedMailRewardRows)
	{
		Label(Content, NAME_None, Reward.DisplayName + TEXT(" ×") + Reward.QuantityText, {24, Y}, {750, 34}, 21);
		Y += 38;
	}
	bool bTaken = false;
	Mail->TryGetBoolField(TEXT("takeEnclosure"), bTaken);
	if (!RenderedMailRewardRows.IsEmpty())
	{
		auto* Claim = MakeActionButton(TEXT("ServerMailClaim"), INDEX_NONE, nullptr, FIntRect(), FIntPoint(1, 1), {220, 48});
		Claim->SetIsEnabled(!bTaken);
		Claim->OnIndexedClicked.AddWeakLambda(this, [this, Id = SelectedSessionMail](int32)
		{
			if (auto* Service = GetGameInstance()->GetSubsystem<URokGameNetSubsystem>())
			{
				Service->TakeMailEnclosure(Id);
			}
		});
		PlaceOnCanvas(Content, Claim, {288, Y + 16}, {220, 48}, 3);
		Label(Content, NAME_None, bTaken ? TEXT("已领取") : TEXT("领取"), {351, Y + 24}, {170, 35}, 23);
		Y += 85;
	}
	Scroll->AddChild(WrapFixedSize(Content, NAME_None, {805, FMath::Max(Y + 20, 580.0f)}));
}

void URokUnityFeatureScreenWidget::BindLiveMailRewardCard(URokUiPageWidget& Page, const FRokMailRewardRow& Reward, const TCHAR* Prefix)
{
	RokMailWbp::SetText(Page, *FString(Prefix).Append(TEXT("Name")), Reward.DisplayName);
	RokMailWbp::SetText(Page, *FString(Prefix).Append(TEXT("Quantity")), Reward.QuantityText);
	const TCHAR* Atlas = nullptr;
	FIntRect Rect;
	FIntPoint AtlasSize;
	auto* Icon = Cast<UImage>(RokMailWbp::Find(Page, *FString(Prefix).Append(TEXT("Icon"))));
	if (Icon)
	{
		const bool bResolved = ResolveMailRewardIcon(Reward, Atlas, Rect, AtlasSize);
		UTexture2D* Texture = bResolved ? LoadUiTexture(Atlas) : nullptr;
		Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture)
		{
			Icon->SetBrush(MakeAtlasBrush(Texture, Rect, AtlasSize, Icon->GetBrush().GetImageSize()));
		}
	}
}

bool URokUnityFeatureScreenWidget::TryBuildSessionMailWbp(UScrollBox* List, const TArray<int64>& Ids)
{
	if (URokUiSettings::IsLegacyForced())
	{
		return false;
	}
	UClass* ScreenClass = ResolveFeaturePageClass(TEXT("ServerMailScreen"));
	UClass* RowClass = ResolveFeaturePageClass(TEXT("ServerMailRow"));
	UClass* RewardClass = ResolveFeaturePageClass(TEXT("ServerMailRewardCard"));
	if (!ScreenClass || !RowClass || !RewardClass)
	{
		return false;
	}
	auto* Page = CreateWidget<URokUiPageWidget>(this, ScreenClass);
	auto* RowPrototype = CreateWidget<URokUiPageWidget>(this, RowClass);
	auto* RewardPrototype = CreateWidget<URokUiPageWidget>(this, RewardClass);
	auto* Scroll = Page ? Cast<UScrollBox>(RokMailWbp::Find(*Page, TEXT("ServerMailScroll"))) : nullptr;
	auto* RewardHost = Page ? Cast<UWrapBox>(RokMailWbp::Find(*Page, TEXT("ServerMailRewardCards"))) : nullptr;
	auto* Claim = Page ? Cast<URokUnityIndexedButton>(RokMailWbp::Find(*Page, TEXT("ServerMailClaim"))) : nullptr;
	const FVector2D RowSize = RowPrototype ? RokMailWbp::Size(*RowPrototype, TEXT("ServerMailRowLayout")) : FVector2D::ZeroVector;
	const FVector2D RewardSize = RewardPrototype ? RokMailWbp::Size(*RewardPrototype, TEXT("ServerMailRewardLayout")) : FVector2D::ZeroVector;
	if (!Page || !Scroll || !RewardHost || !Claim || RowSize.X <= 0 || RowSize.Y <= 0 || RewardSize.X <= 0 || RewardSize.Y <= 0)
	{
		return false;
	}
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	if (!Net && !Ids.IsEmpty())
	{
		return false;
	}
	Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, Page, {547, 128}, {830, 590}, 6);
	if (bShownMailInboxLoading && Ids.IsEmpty())
	{
		RokMailWbp::SetText(*Page, TEXT("ServerMailEmpty"), TEXT("正在获取邮件…"));
	}
	RokMailWbp::Show(*Page, TEXT("ServerMailEmpty"), Ids.IsEmpty());
	Scroll->SetVisibility(Ids.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	for (const int64 Id : Ids)
	{
		const auto& Mail = Net->GetSessionMail().FindChecked(Id);
		auto* Row = CreateWidget<URokUiPageWidget>(this, RowClass);
		auto* Hit = Row ? Cast<URokUnityIndexedButton>(RokMailWbp::Find(*Row, TEXT("ServerMailRowHitButton"))) : nullptr;
		if (!Row || !Hit)
		{
			continue;
		}
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RokMailWbp::SetText(*Row, TEXT("ServerMailRowTitle"), MailText(Config, Mail, TEXT("l_nameID"), TEXT("titleContents")));
		RokMailWbp::SetText(*Row, TEXT("ServerMailRowSubject"), MailText(Config, Mail, TEXT("l_subheadingID"), TEXT("subTitleContents")));
		RokMailWbp::Show(*Row, TEXT("ServerMailRowUnread"), MailNumber(Mail, TEXT("status")) == 0);
		RokMailWbp::Show(*Row, TEXT("ServerMailRowSelection"), Id == SelectedSessionMail);
		Hit->ConfigureAction(INDEX_NONE);
		Hit->OnIndexedClicked.AddWeakLambda(this, [this, Id](int32) { SelectSessionMail(Id); });
		List->AddChild(WrapFixedSize(Row, NAME_None, RowSize));
	}
	if (Ids.IsEmpty())
	{
		RenderedMailRewardRows.Reset();
		return true;
	}
	const auto& Mail = Net->GetSessionMail().FindChecked(SelectedSessionMail);
	RokMailWbp::SetText(*Page, TEXT("ServerMailTitle"), MailText(Config, Mail, TEXT("l_nameID"), TEXT("titleContents")));
	RokMailWbp::SetText(*Page, TEXT("ServerMailSubtitle"), MailText(Config, Mail, TEXT("l_subheadingID"), TEXT("subTitleContents")));
	RokMailWbp::SetText(*Page, TEXT("ServerMailTime"), RokMailWbp::LocalTime(MailNumber(Mail, TEXT("sendTime"))));
	const FString Body = MailText(Config, Mail, TEXT("l_mesID"), TEXT("emailContents"));
	RokMailWbp::SetText(*Page, TEXT("ServerMailBody"), Body.IsEmpty() ? TEXT("邮件正文暂不可用。") : Body);
	const FString Sender = MailText(Config, Mail, TEXT("l_senderID"), TEXT("senderContents"));
	RokMailWbp::SetText(*Page, TEXT("ServerMailSender"), Sender);
	RokMailWbp::Show(*Page, TEXT("ServerMailSender"), !Sender.IsEmpty());
	const auto Rewards = MailObject(Mail, TEXT("rewards"));
	const auto Payload = RokMailWbp::ReadRewards(Rewards);
	RenderedMailRewardRows = RokMailRewardDetails::Normalize(Payload, GetEconomyCatalogSubsystem(), GetGameplayCatalogSubsystem());
	RewardHost->ClearChildren();
	for (const auto& Reward : RenderedMailRewardRows)
	{
		auto* Card = CreateWidget<URokUiPageWidget>(this, RewardClass);
		if (Card)
		{
			Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			BindLiveMailRewardCard(*Card, Reward, TEXT("ServerMailReward"));
			RewardHost->AddChild(WrapFixedSize(Card, NAME_None, RewardSize));
		}
	}
	bool bOtherRewards = false;
	for (const TCHAR* Field : {TEXT("actionForce"), TEXT("expeditionCoin"), TEXT("guildPoint"), TEXT("vip"), TEXT("leaguePoints"), TEXT("activityActivePoint")})
	{
		bOtherRewards |= MailNumber(Rewards, Field) > 0;
	}
	for (const TCHAR* Field : {TEXT("guildGifts"), TEXT("heros")})
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		bOtherRewards |= Rewards && Rewards->TryGetArrayField(Field, Values) && !Values->IsEmpty();
	}
	const bool bHasRewards = !RenderedMailRewardRows.IsEmpty() || bOtherRewards;
	RokMailWbp::Show(*Page, TEXT("ServerMailRewardHeading"), bHasRewards);
	RokMailWbp::Show(*Page, TEXT("ServerMailRewardCards"), !RenderedMailRewardRows.IsEmpty());
	RokMailWbp::Show(*Page, TEXT("ServerMailRewardUnavailable"), bOtherRewards);
	RokMailWbp::Show(*Page, TEXT("ServerMailClaimSectionSize"), bHasRewards);
	bool bTaken = false;
	Mail->TryGetBoolField(TEXT("takeEnclosure"), bTaken);
	Claim->SetIsEnabled(bHasRewards && !bTaken);
	Claim->ConfigureAction(INDEX_NONE);
	RokMailWbp::SetText(*Page, TEXT("ServerMailClaimLabel"), bTaken ? TEXT("已领取") : TEXT("领取附件"));
	Claim->OnIndexedClicked.AddWeakLambda(this, [this, Id = SelectedSessionMail](int32)
	{
		if (auto* Service = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr)
		{
			Service->TakeMailEnclosure(Id);
		}
	});
	return true;
}
