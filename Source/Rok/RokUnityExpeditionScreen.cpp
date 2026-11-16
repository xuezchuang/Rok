#include "RokUnityFeatureScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "ImageUtils.h"
#include "RokCommanderMeshWidget.h"
#include "RokDataPaths.h"
#include "RokGameNetSubsystem.h"
#include "RokServerArmySubsystem.h"
#include "RokSproto.h"
#include "RokTroopPortrait.h"
#include "RokUiTextTable.h"
#include "RokUnityConfigSubsystem.h"
#include "RokUnityEconomyCatalogSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"

namespace
{
constexpr TCHAR Common[] = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
constexpr TCHAR HeroHeads[] = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_head.rok_commander_head");
constexpr TCHAR TroopHeads[] = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_troops_head.rok_fullscreen_troops_head");
constexpr TCHAR TroopUi[] = TEXT("/Game/RokPrototype/UI/TrainArmyDetail/ui_troops.ui_troops");
constexpr TCHAR TaskPanel[] = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.rok_expedition_task_panel");
constexpr TCHAR ResultAtlas[] = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.rok_expedition_result_atlas");
const FIntPoint CommonSize(2048, 1024);
const FIntRect GoldButton(677, 600, 879, 681);
const FIntRect BlueButton(1098, 571, 1300, 652);
const FIntRect DisabledButton(473, 528, 675, 609);
const FLinearColor Ink(0.39f, 0.21f, 0.06f);
const FLinearColor Shade(0.035f, 0.045f, 0.04f, 0.75f);
constexpr int32 StageActionBase = 24000;
constexpr int32 SquadActionBase = 24100;
constexpr int32 HeroActionBase = 24200;
constexpr int32 GatherDeputyAction = 24400;
constexpr int32 GatherRemoveDeputyAction = 24401;
constexpr int32 GatherInfoAction = 24402;

bool GatherSoldierOrder(int32 A, int32 B)
{
	// Unity's gathering draft puts siege units first for their higher load.
	const bool bSiegeA = A / 100 % 100 == 4;
	const bool bSiegeB = B / 100 % 100 == 4;
	return bSiegeA != bSiegeB ? bSiegeA : A < B;
}

int32 ReadInt(const TSharedPtr<FJsonObject>& Object, const FString& Key)
{
	int64 Number = 0;
	return RokSprotoJson::TryGetInt64(Object, Key, Number) && Number >= 0 && Number <= MAX_int32
		? static_cast<int32>(Number) : 0;
}

template <typename Visit>
bool VisitRoleCollection(const TSharedPtr<FJsonObject>& Role, const FString& Field, Visit&& OnRow)
{
	if (!Role) return false;
	const TSharedPtr<FJsonObject>* Keyed = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	const auto VisitValue = [&OnRow](const TSharedPtr<FJsonValue>& Value)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (Value && Value->TryGetObject(Row) && Row && Row->IsValid()) OnRow(*Row);
	};
	if (Role->TryGetObjectField(Field, Keyed) && Keyed && Keyed->IsValid())
	{
		for (const auto& Pair : (*Keyed)->Values) VisitValue(Pair.Value);
		return true;
	}
	// Legacy captured role snapshots preserve the wire collection as an array.
	if (Role->TryGetArrayField(Field, Array) && Array)
	{
		for (const auto& Value : *Array) VisitValue(Value);
		return true;
	}
	return false;
}

TSharedPtr<FJsonObject> GetRole(const UGameInstance* Instance)
{
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	return Net && Net->GetState() == ERokGameNetState::RoleReady ? Net->GetRoleInfo() : nullptr;
}
}

void URokUnityFeatureScreenWidget::RefreshExpeditionAccount()
{
	ExpeditionStageStars.Reset();
	const auto Role = GetRole(GetGameInstance());
	// An empty expeditionInfo is the initial role state in Unity.
	bExpeditionProgressKnown = ReadInt(Role, TEXT("level")) > 0;
	if (!bExpeditionProgressKnown || !Role->HasField(TEXT("expeditionInfo"))) return;
	bExpeditionProgressKnown = VisitRoleCollection(Role, TEXT("expeditionInfo"), [this](const auto& Entry)
	{
		const int32 Id = ReadInt(Entry, TEXT("id"));
		const int32 Stars = ReadInt(Entry, TEXT("star"));
		if (ExpeditionRows.IsValidIndex(Id - 1) && Stars <= 3) ExpeditionStageStars.Add(Id, Stars);
	});
}

bool URokUnityFeatureScreenWidget::CanChallengeExpedition(const FRokExpeditionStagePreviewRow& Stage) const
{
	return Stage.FrontNumber == 0 || (bExpeditionProgressKnown
		&& (ExpeditionStageStars.Contains(Stage.Id) || ExpeditionStageStars.Contains(Stage.FrontNumber)));
}

UImage* URokUnityFeatureScreenWidget::AddExpeditionArt(UCanvasPanel* Canvas, FName Name,
	const FString& Asset, FVector2D Position, FVector2D Size, int32 Layer)
{
	auto& Texture = ExpeditionTextures.FindOrAdd(Asset);
	if (!Texture)
		Texture = FImageUtils::ImportFileAsTexture2D(RokDataPaths::Resolve(TEXT("Expedition/") + Asset));
	auto* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	if (Texture) Image->SetBrushFromTexture(Texture);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Canvas, Image, Position, Size, Layer);
	return Image;
}

void URokUnityFeatureScreenWidget::AddExpeditionHeroHead(UCanvasPanel* Canvas, int32 HeroId,
	FName Name, FVector2D Position, FVector2D Size, int32 Layer)
{
	auto* Catalog = GetGameplayCatalogSubsystem();
	FRokUnityCatalogHero Hero;
	if (!Catalog || !Catalog->GetHero(HeroId, Hero) || Hero.IconWidth <= 0 || Hero.IconHeight <= 0) return;
	const bool bHighRarity = Hero.Rare >= 4;
	const TCHAR* FrameAtlas = bHighRarity ? HeroHeads
		: TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_ui.rok_commander_ui");
	const FIntRect FrameRect = Hero.Rare >= 5 ? FIntRect(593, 1895, 727, 2047)
		: Hero.Rare == 4 ? FIntRect(729, 1895, 863, 2047) : FIntRect(1, 53, 135, 205);
	auto* Frame = MakeAtlasImage(*(Name.ToString() + TEXT("Frame")), FrameAtlas, FrameRect,
		bHighRarity ? FIntPoint(1024, 2048) : FIntPoint(2048, 512), Size);
	if (Hero.Rare == 2) Frame->SetColorAndOpacity(FLinearColor(0.62f, 1.0f, 0.70f));
	else if (Hero.Rare == 3) Frame->SetColorAndOpacity(FLinearColor(0.55f, 0.78f, 1.0f));
	PlaceOnCanvas(Canvas, Frame, Position, Size, Layer);
	PlaceOnCanvas(Canvas, MakeAtlasImage(Name, HeroHeads,
		FIntRect(Hero.IconX, Hero.IconY, Hero.IconX + Hero.IconWidth, Hero.IconY + Hero.IconHeight),
		FIntPoint(Hero.IconAtlasWidth, Hero.IconAtlasHeight), Size * 0.93),
		Position + FVector2D(Size.X * 0.035, 0), Size * 0.93, Layer + 1);
}

void URokUnityFeatureScreenWidget::BuildExpedition()
{
	ExpeditionPreviewRoot = nullptr;
	ExpeditionSideRoot = nullptr;
	ExpeditionCenterRoot = nullptr;
	ExpeditionCenterImg = nullptr;
	ExpeditionCenterFlagMes = nullptr;
	FRokExpeditionStagePreviewRow Stage;
	if (!RokExpeditionStagePreviewData::FindById(ExpeditionRows, SelectedExpeditionStageId, Stage))
	{
		BuildSplitWindowFrame(FVector2D::ZeroVector, FVector2D(1440, 810), FText::FromString(TEXT("远征")), FVector2D(1357, 4));
		auto* Error = MakeScreenText(TEXT("ExpeditionSourceError"), 20, FLinearColor::White, false, ETextJustify::Center);
		Error->SetText(FText::FromString(ExpeditionLoadError));
		PlaceOnCanvas(ModeCanvas, Error, FVector2D(240, 340), FVector2D(960, 60), 10);
		return;
	}
	if (!bExpeditionTaskConfigVisible)
		BuildExpeditionMap(Stage);
	else
	{
		BuildExpeditionTask(Stage);
		if (ExpeditionDraftSlot != INDEX_NONE) BuildArmyComposer();
	}
}

void URokUnityFeatureScreenWidget::BuildExpeditionMap(const FRokExpeditionStagePreviewRow& Stage)
{
	const auto Art = [this](FName Name, const TCHAR* Asset, FVector2D Pos, FVector2D Size, int32 Z)
	{
		return AddExpeditionArt(ModeCanvas, Name, Asset, Pos, Size, Z);
	};
	const auto Label = [this](FName Name, const FText& Text, FVector2D Pos, FVector2D Size, int32 Font,
		const FLinearColor& Color, int32 Z)
	{
		auto* Widget = MakeScreenText(Name, Font, Color, true, ETextJustify::Center);
		Widget->SetText(Text);
		PlaceOnCanvas(ModeCanvas, Widget, Pos, Size, Z);
	};
	const auto Icon = [this, &Art](FName Name, int32 Id, const TCHAR* Asset, FVector2D Pos, FVector2D Size, int32 Z)
	{
		Art(*FString::Printf(TEXT("%sArt"), *Name.ToString()), Asset, Pos, Size, Z);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(Name, Id, nullptr, FIntRect(), FIntPoint(), Size), Pos, Size, Z + 1);
	};

	Art(TEXT("ExpeditionBackdrop"), TEXT("hero_background_14.png"), FVector2D(0, -45), FVector2D(1440, 900), 0);
	Art(TEXT("ExpeditionParchmentLeft"), TEXT("img_bg_battle_frame1.png"), FVector2D(0, 10), FVector2D(720, 794), 1);
	auto* Right = Art(TEXT("ExpeditionParchmentRight"), TEXT("img_bg_battle_frame1.png"),
		FVector2D(720, 10), FVector2D(720, 794), 1);
	Right->SetRenderTransformPivot(FVector2D(0.5));
	Right->SetRenderScale(FVector2D(-1, 1));
	Label(TEXT("ExpeditionTitle"), FText::FromString(TEXT("远征")), FVector2D(520, 55), FVector2D(400, 52), 34, Ink, 5);
	Icon(TEXT("ExpeditionMapBack"), 1122, TEXT("btn_bat_back.png"), FVector2D(74, 46), FVector2D(54), 8);
	Icon(TEXT("ExpeditionRank"), 1125, TEXT("btn_bat_rank.png"), FVector2D(1372, 84), FVector2D(49), 8);
	Icon(TEXT("ExpeditionRules"), 1126, TEXT("btn_bat_rule.png"), FVector2D(1372, 154), FVector2D(49), 8);

	const auto Role = GetRole(GetGameInstance());
	Art(TEXT("ExpeditionMedalIcon"), TEXT("img_com_cur8.png"), FVector2D(1072, 0), FVector2D(54), 6);
	Label(TEXT("ExpeditionMedals"), Role ? FText::AsNumber(ReadInt(Role, TEXT("expeditionCoin"))) : FText::FromString(TEXT("—")),
		FVector2D(1112, 12), FVector2D(105, 34), 19, FLinearColor::White, 6);
	FRokMailRewardRow Gems;
	Gems.Id = 104;
	const TCHAR* Atlas = nullptr;
	FIntRect Rect;
	FIntPoint AtlasSize;
	if (ResolveMailRewardIcon(Gems, Atlas, Rect, AtlasSize))
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("ExpeditionGemIcon"), Atlas, Rect, AtlasSize, FVector2D(53)),
			FVector2D(1232, 0), FVector2D(53), 6);
	Label(TEXT("ExpeditionGems"), Role ? FText::AsNumber(ReadInt(Role, TEXT("denar"))) : FText::FromString(TEXT("—")),
		FVector2D(1278, 12), FVector2D(90, 34), 19, FLinearColor::White, 6);

	const FVector2D ViewSize(1292, 625);
	auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ExpeditionMapScroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, Scroll, FVector2D(74, 109), ViewSize, 2);
	auto* Map = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ExpeditionMapContent"));
	const int32 TileCount = FMath::DivideAndRoundUp(ExpeditionRows.Last().BattleCoordinate.Y + 55, 1024);
	Scroll->AddChild(WrapFixedSize(Map, TEXT("ExpeditionMapExtent"), FVector2D(ViewSize.X, 1024 * TileCount)));
	for (int32 Index = 0; Index < TileCount; ++Index)
		AddExpeditionArt(Map, *FString::Printf(TEXT("ExpeditionMapTile_%d"), Index),
			FString::Printf(TEXT("img_bg_battle_map%d.jpg"), Index % 3 + 1), FVector2D(0, Index * 1024), FVector2D(ViewSize.X, 1024));

	for (const auto& Row : ExpeditionRows)
	{
		const FVector2D Anchor(Row.BattleCoordinate);
		const bool bUnlocked = CanChallengeExpedition(Row);
		const bool bSelected = Row.Id == Stage.Id;
		const FString Suffix = FString::FromInt(Row.Id);
		if (bSelected)
			AddExpeditionArt(Map, TEXT("ExpeditionSelectedFlagGlow"), TEXT("selected_glow.png"),
				Anchor - FVector2D(72, 110), FVector2D(144), 1)->SetColorAndOpacity(FLinearColor(1, 0.65f, 0.15f, 0.8f));
		AddExpeditionArt(Map, *(TEXT("ExpeditionStageFlag_") + Suffix), bUnlocked
			? (Row.Type == 1 ? TEXT("img_bat_flag1.png") : TEXT("img_bat_flag2.png")) : TEXT("img_bat_flag4.png"),
			Anchor - FVector2D(30.5, 71), FVector2D(61, 87), 1);
		AddExpeditionArt(Map, *(TEXT("ExpeditionStageBase_") + Suffix), Row.Type == 1
			? TEXT("img_bat_flagfra.png") : TEXT("img_bat_flagfra2.png"), Anchor + FVector2D(-24.5, 12), FVector2D(49, 40), 2);
		if (bUnlocked)
		{
			for (int32 Star = 0; Star < 3; ++Star)
				AddExpeditionArt(Map, *FString::Printf(TEXT("ExpeditionFlagStar_%d_%d"), Row.Id, Star),
					Star < ExpeditionStageStars.FindRef(Row.Id)
					? TEXT("icon_com_star2.png") : TEXT("icon_com_star1.png"),
					Anchor + FVector2D(-39 + 26 * Star, Star == 1 ? -69 : -64), FVector2D(26), 2);
		}
		auto* Level = MakeScreenText(*(TEXT("ExpeditionStageTitle_") + Suffix), bSelected ? 24 : 21,
			bSelected ? FLinearColor(1, 0.86f, 0.45f) : FLinearColor::White, true, ETextJustify::Center);
		Level->SetText(FText::AsNumber(Row.Level));
		PlaceOnCanvas(Map, Level, Anchor - FVector2D(30, 42), FVector2D(60, 32), 3);
		PlaceOnCanvas(Map, MakeActionButton(*(TEXT("ExpeditionStageButton_") + Suffix), StageActionBase + Row.Id,
			nullptr, FIntRect(), FIntPoint(), FVector2D(90, 102)), Anchor - FVector2D(45, 62), FVector2D(90, 102), 4);
	}
	Scroll->SetScrollOffset(ExpeditionMapOffset);

	Icon(TEXT("ExpeditionShop"), 1124, TEXT("btn_bat_frame.png"), FVector2D(101, 114), FVector2D(99, 100), 8);
	Art(TEXT("ExpeditionShopIcon"), TEXT("btn_bat_shop.png"), FVector2D(113, 129), FVector2D(77, 71), 9);
	Icon(TEXT("ExpeditionChest"), 1127, TEXT("img_bat_box.png"), FVector2D(101, 213), FVector2D(86, 82), 8);

	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("ExpeditionMapSelectedPanel"), Shade),
		FVector2D(165, 617), FVector2D(1150, 132), 6);
	Label(TEXT("ExpeditionSelectedLevel"), FText::FromString(FString::Printf(TEXT("%d/%d"), Stage.Level, ExpeditionRows.Num())),
		FVector2D(181, 630), FVector2D(180, 56), 36, FLinearColor::White, 7);
	const int32 Stars = ExpeditionStageStars.FindRef(Stage.Id);
	for (int32 Star = 0; Star < 3; ++Star)
		Art(*FString::Printf(TEXT("ExpeditionSelectionStar_%d"), Star),
			Star < Stars ? TEXT("icon_com_star2.png") : TEXT("icon_com_star1.png"),
			FVector2D(202 + Star * 52, 678), FVector2D(48), 7);
	const bool bFirst = !ExpeditionStageStars.Contains(Stage.Id);
	Label(TEXT("ExpeditionRewardTitle"), FText::FromString(bFirst ? TEXT("首次完成奖励") : TEXT("关卡奖励")),
		FVector2D(364, 624), FVector2D(200, 28), 19, FLinearColor::White, 7);
	if (bFirst)
	{
		Art(TEXT("ExpeditionFirstRewardSeal"), TEXT("img_bat_first.png"), FVector2D(338, 628), FVector2D(54), 7);
		auto* Stamp = MakeScreenText(TEXT("ExpeditionFirstRewardStamp"), 15, FLinearColor::White, true, ETextJustify::Center);
		Stamp->SetText(FText::FromString(TEXT("首胜")));
		Stamp->SetRenderTransformPivot(FVector2D(0.5));
		Stamp->SetRenderTransformAngle(-33);
		PlaceOnCanvas(ModeCanvas, Stamp, FVector2D(339, 640), FVector2D(52, 30), 8);
	}

	auto* Config = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	const auto* Rewards = Config ? Config->GetTableRows(TEXT("ItemPackage")) : nullptr;
	const auto* Economy = GetEconomyCatalogSubsystem();
	const TArray<FRokUnityEconomyItem> Items = Economy ? Economy->GetItems() : TArray<FRokUnityEconomyItem>();
	int32 RewardSlot = 0;
	if (Rewards)
	{
		TArray<int32> Groups = Stage.RewardGroups;
		if (bFirst) Groups.Insert(Stage.FirstRewardGroup, 0);
		for (const int32 Group : Groups)
		{
			for (const auto& Reward : *Rewards)
			{
				if (ReadInt(Reward, TEXT("group")) != Group || RewardSlot >= 6) continue;
				const int32 Id = ReadInt(Reward, TEXT("typeData"));
				const int32 Type = ReadInt(Reward, TEXT("type"));
				const FVector2D Pos(396 + RewardSlot * 79, 658);
				const FString Name = FString::Printf(TEXT("ExpeditionReward_%d"), RewardSlot++);
				const auto* Item = Items.FindByPredicate([Id](const FRokUnityEconomyItem& Entry) { return Entry.Id == Id; });
				if (Type == 100 && Id == 116)
					Art(*(Name + TEXT("Icon")), TEXT("img_com_cur8.png"), Pos, FVector2D(48), 7);
				else
				{
					const TCHAR* ItemAtlas = nullptr;
					FIntRect ItemRect;
					FIntPoint ItemSize;
					FRokMailRewardRow RewardIcon;
					RewardIcon.Id = Id;
					if (Type == 200 && Item)
					{
						RewardIcon.Kind = ERokMailRewardKind::Item;
						RewardIcon.Icon = Item->Icon;
					}
					if ((Type == 100 || (Type == 200 && Item))
						&& ResolveMailRewardIcon(RewardIcon, ItemAtlas, ItemRect, ItemSize))
					{
						auto* Image = MakeAtlasImage(*(Name + TEXT("Icon")), ItemAtlas, ItemRect, ItemSize, FVector2D(48));
						if (Item) Image->SetToolTipText(FText::FromString(Item->DisplayNameZhCn));
						PlaceOnCanvas(ModeCanvas, Image, Pos, FVector2D(48), 7);
					}
					else if (Item)
						Label(*(Name + TEXT("Name")), FText::FromString(Item->DisplayNameZhCn), Pos,
							FVector2D(76, 48), 12, FLinearColor::White, 8);
				}
				Label(*(Name + TEXT("Count")), FText::AsNumber(ReadInt(Reward, TEXT("number"))),
					Pos + FVector2D(-3, 48), FVector2D(54, 22), 15, FLinearColor::White, 8);
			}
		}
	}
	for (int32 Squad = 0; Squad < 5; ++Squad)
		Art(*FString::Printf(TEXT("ExpeditionCapacitySlot_%d"), Squad),
			Squad < Stage.TroopsNumber ? TEXT("img_bat_queue1.png") : TEXT("img_bat_queue2.png"),
			FVector2D(906 + Squad * 20, 684), FVector2D(16, 40), 7);
	const bool bCanChallenge = CanChallengeExpedition(Stage);
	auto* Challenge = MakeActionButton(TEXT("ExpeditionChallenge"), 1120, Common,
		bCanChallenge ? GoldButton : DisabledButton, CommonSize, FVector2D(202, 75), FText::FromString(TEXT("挑战")), 25, Ink);
	Challenge->SetIsEnabled(bCanChallenge);
	PlaceOnCanvas(ModeCanvas, Challenge, FVector2D(1082, 645), FVector2D(202, 75), 8);
	ExpeditionAnimationPhase = TEXT("Idle");
	ExpeditionAnimationElapsed = 0;
}

void URokUnityFeatureScreenWidget::BuildExpeditionTask(const FRokExpeditionStagePreviewRow& Stage)
{
	auto* TerrainShade = MakeSolidBorder(TEXT("ExpeditionStagingShade"), FLinearColor(0.002f, 0.026f, 0.065f, 0.71f));
	PlaceOnCanvas(ModeCanvas, TerrainShade, FVector2D(-400, -180), FVector2D(2240, 1170), 1);
	const auto Label = [this](UCanvasPanel* Root, FName Name, const FText& Text, FVector2D Pos,
		FVector2D Size, int32 Font, FLinearColor Color = FLinearColor::White, ETextJustify::Type Align = ETextJustify::Center)
	{
		auto* TextBlock = MakeScreenText(Name, Font, Color, true, Align);
		TextBlock->SetText(Text);
		PlaceOnCanvas(Root, TextBlock, Pos, Size, 3);
	};
	const auto NewCanvas = [this](FName Name, FVector2D Pos, FVector2D Size, int32 Z)
	{
		auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlaceOnCanvas(ModeCanvas, Root, Pos, Size, Z);
		return Root;
	};
	auto* Side = NewCanvas(TEXT("ExpeditionSideRoot"), FVector2D(1294, -24), FVector2D(146, 875), 6);
	ExpeditionSideRoot = Side;
	PlaceOnCanvas(Side, MakeSolidBorder(TEXT("ExpeditionSideShade"), Shade),
		FVector2D::ZeroVector, FVector2D(146, 875));
	Label(Side, TEXT("ExpeditionTroopCount"), FText::FromString(FString::Printf(TEXT("%d/%d"), ExpeditionTeams.Num(), Stage.TroopsNumber)),
		FVector2D(12, 8), FVector2D(122, 32), 19);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const bool bUnlocked = Index < Stage.TroopsNumber;
		const auto* Team = ExpeditionTeams.Find(Index);
		const FVector2D Pos(16, 86 + Index * 133);
		const FVector2D Size(114, 127);
		AddExpeditionArt(Side, *FString::Printf(TEXT("ExpeditionSquadSlot_%d"), Index),
			bUnlocked ? TEXT("img_bat_trop3.png") : TEXT("img_bat_trop4.png"), Pos, Size, 2);
		if (Team) AddExpeditionHeroHead(Side, Team->HeroId, *FString::Printf(TEXT("ExpeditionSquadHero_%d"), Index),
			Pos + FVector2D(15, 17), FVector2D(84, 95), 3);
		auto* Button = MakeActionButton(*FString::Printf(TEXT("ExpeditionSquadButton_%d"), Index),
			SquadActionBase + Index, nullptr, FIntRect(), FIntPoint(), Size);
		Button->SetIsEnabled(bUnlocked);
		PlaceOnCanvas(Side, Button, Pos, Size, 4);
	}
	const FVector2D BackPos(20, -35), BackSize(57);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("ExpeditionBackButton"), 1122, Common,
		FIntRect(604, 611, 673, 680), CommonSize, BackSize), BackPos, BackSize, 12);
	const FVector2D PreviewPos(1312, 17), PreviewSize(111, 42);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("ExpeditionPreviewButton"), 1121, Common, BlueButton, CommonSize, PreviewSize,
		FText::FromString(TEXT("预览")), 17), PreviewPos, PreviewSize, 12);

	auto* Center = NewCanvas(TEXT("ExpeditionCenterRoot"), FVector2D::ZeroVector, FVector2D(1440, 810), 7);
	ExpeditionCenterRoot = Center;
	Center->SetRenderTransformPivot(FVector2D(0.5));
	auto* Flag = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ExpeditionCenterFlagMes"));
	Flag->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(Center, Flag, FVector2D::ZeroVector, FVector2D(1440, 810));
	ExpeditionCenterFlagMes = Flag;
	PlaceOnCanvas(Flag, MakeAtlasImage(TEXT("ExpeditionTaskPanelImage"), TaskPanel, FIntRect(0, 0, 676, 761),
		FIntPoint(676, 761), FVector2D(676, 725)), FVector2D(382, 65), FVector2D(676, 725));
	auto* Header = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ExpeditionCenterImg"));
	Header->SetVisibility(ESlateVisibility::HitTestInvisible);
	Header->SetRenderTransformPivot(FVector2D(0.5));
	PlaceOnCanvas(Center, Header, FVector2D(286, 84), FVector2D(868, 111), 3);
	ExpeditionCenterImg = Header;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* Half = MakeAtlasImage(Index == 0 ? TEXT("ExpeditionCenterFrameLeft") : TEXT("ExpeditionCenterFrameRight"),
			ResultAtlas, FIntRect(1, 131, 455, 269), FIntPoint(512, 512), FVector2D(434, 128));
		if (Index == 1)
		{
			Half->SetRenderTransformPivot(FVector2D(0.5));
			Half->SetRenderScale(FVector2D(-1, 1));
		}
		PlaceOnCanvas(Header, Half, FVector2D(Index * 434, -8), FVector2D(434, 128));
	}
	Label(Header, TEXT("ExpeditionDetailTitle"), FText::FromString(TEXT("任务目标")),
		FVector2D(207, 25), FVector2D(454, 52), 36);
	Label(Flag, TEXT("ExpeditionSetupHint"), FText::FromString(TEXT("请在右侧配置您的统帅")),
		FVector2D(495, 204), FVector2D(450, 34), 17, FLinearColor(1, 0.67f, 0.22f));
	for (int32 Index = 0; Index < Stage.StarDescriptions.Num(); ++Index)
	{
		const float Y = 248 + Index * 50;
		AddExpeditionArt(Flag, *FString::Printf(TEXT("ExpeditionObjectiveStar_%d"), Index),
			TEXT("icon_com_star2.png"), FVector2D(486, Y), FVector2D(43), 2);
		Label(Flag, *FString::Printf(TEXT("ExpeditionStarCondition_%d"), Index),
			FText::FromString(Stage.StarDescriptions[Index]), FVector2D(542, Y + 7), FVector2D(426, 34), 20,
			FLinearColor::White, ETextJustify::Left);
	}

	// UI_IF_ExpeditionFightTaskMediator reads the enemy commanders through
	// ExpeditionBattle's MonsterTroops references, independently of ownership.
	TArray<int32> EnemyHeroes;
	auto* Config = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	if (Config)
	{
		TArray<FName> EnemyFields{TEXT("bossID"), TEXT("targetCityMonsterID")};
		for (int32 Index = 1; Index <= 5; ++Index)
			EnemyFields.Add(*FString::Printf(TEXT("monster%dID"), Index));
		for (FName Field : EnemyFields)
		{
			int32 Troop = 0;
			int32 Hero = 0;
			if (Config->GetIntFieldByKey(TEXT("ExpeditionBattle"), TEXT("ID"), Stage.BattleId, Field, Troop) && Troop > 0
				&& Config->GetIntFieldByKey(TEXT("MonsterTroops"), TEXT("ID"), Troop, TEXT("heroID1"), Hero) && Hero > 0)
				EnemyHeroes.Add(Hero);
		}
	}
	PlaceOnCanvas(Flag, MakeSolidBorder(TEXT("ExpeditionEnemiesShade"), FLinearColor(0.12f, 0.012f, 0.01f, 0.66f)),
		FVector2D(450, 421), FVector2D(540, 124), 1);
	Label(Flag, TEXT("ExpeditionEnemyTitle"), FText::FromString(TEXT("敌方统帅")),
		FVector2D(470, 427), FVector2D(500, 25), 18);
	for (int32 Index = 0; Index < EnemyHeroes.Num(); ++Index)
		AddExpeditionHeroHead(Flag, EnemyHeroes[Index], *FString::Printf(TEXT("ExpeditionEnemyHero_%d"), Index),
			FVector2D(720 - 44 * EnemyHeroes.Num() + 88 * Index + 8, 452), FVector2D(72, 81), 4);

	auto* Start = MakeActionButton(TEXT("ExpeditionChallenge"), 1120, Common,
		ExpeditionTeams.IsEmpty() ? DisabledButton : GoldButton, CommonSize, FVector2D(200, 70),
		FText::FromString(TEXT("开始")), 29, Ink);
	Start->SetIsEnabled(!ExpeditionTeams.IsEmpty());
	PlaceOnCanvas(Flag, Start, FVector2D(620, 598), FVector2D(200, 70), 5);

	auto* Preview = NewCanvas(TEXT("ExpeditionPreviewRoot"), FVector2D::ZeroVector, FVector2D(1440, 810), 8);
	ExpeditionPreviewRoot = Preview;
	PlaceOnCanvas(Preview, MakeSolidBorder(TEXT("ExpeditionPreviewShade"), Shade), FVector2D(1056, 62), FVector2D(345, 590));
	Label(Preview, TEXT("ExpeditionPreviewTitle"), FText::FromString(TEXT("敌方统帅")),
		FVector2D(1068, 80), FVector2D(321, 46), 23);
	for (int32 Index = 0; Index < EnemyHeroes.Num(); ++Index)
		AddExpeditionHeroHead(Preview, EnemyHeroes[Index], *FString::Printf(TEXT("ExpeditionPreviewEnemy_%d"), Index),
			FVector2D(1100 + 140 * (Index % 2), 139 + 118 * (Index / 2)), FVector2D(92, 104), 3);

	if (ExpeditionAnimationPhase != TEXT("Idle") || bExpeditionPreview)
	{
		ExpeditionAnimationPhase = bExpeditionPreview ? FName(TEXT("PreviewTransition")) : FName(TEXT("NormalTransition"));
		ExpeditionAnimationElapsed = 0;
		AdvanceExpeditionAnimation(0);
	}
	else
	{
		Preview->SetVisibility(ESlateVisibility::Collapsed);
	}
}

TMap<int32, int32> URokUnityFeatureScreenWidget::GetExpeditionAvailableSoldiers(int32 TeamSlot) const
{
	TMap<int32, int32> Result;
	const auto Role = GetRole(GetGameInstance());
	VisitRoleCollection(Role, TEXT("historySoldiers"), [&Result](const auto& Soldier)
	{
		const int32 Id = ReadInt(Soldier, TEXT("id"));
		if (Id > 0) Result.Add(Id, ReadInt(Soldier, TEXT("num")));
	});
	for (const auto& [OtherSlot, Team] : ExpeditionTeams)
	{
		if (OtherSlot == TeamSlot) continue;
		for (const auto& [Id, Count] : Team.Soldiers)
			if (auto* Available = Result.Find(Id)) *Available = FMath::Max(0, *Available - Count);
	}
	return Result;
}

int32 URokUnityFeatureScreenWidget::GetExpeditionDraftCapacity() const
{
	auto* Instance = GetGameInstance();
	const auto* HeroSubsystem = Instance ? Instance->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	if (!HeroSubsystem || !Config) return 0;
	const auto Snapshot = HeroSubsystem->GetSnapshot();
	const auto* Hero = Snapshot.Heroes.FindByPredicate([this](const auto& Row) { return Row.HeroId == ExpeditionDraft.HeroId; });
	if (!Hero || !Hero->bLevelKnown || Hero->Level <= 0 || Hero->Rare <= 0) return 0;
	int32 CityCapacity = 0, HeroCapacity = 0;
	if (!Config->GetIntFieldByKey(TEXT("BuildingTownCenter"), TEXT("ID"), ReadInt(GetRole(Instance), TEXT("level")),
			TEXT("troopsCapacity"), CityCapacity)
		|| !Config->GetIntFieldByKey(TEXT("HeroLevel"), TEXT("ID"), Hero->Rare * 10000 + Hero->Level - 1,
			TEXT("soldiers"), HeroCapacity))
		return 0;
	return CityCapacity + HeroCapacity;
}

void URokUnityFeatureScreenWidget::OpenExpeditionArmySlot(int32 TeamSlot)
{
	ExpeditionDraftSlot = TeamSlot;
	ExpeditionDraft = ExpeditionTeams.FindRef(TeamSlot);
	const auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	const auto Heroes = Server ? Server->GetSnapshot() : FRokServerHeroSnapshot();
	ExpeditionVisibleHeroChoices.Reset();
	for (const auto& Hero : Heroes.Heroes)
	{
		bool bInOtherTeam = false;
		for (const auto& [OtherSlot, Team] : ExpeditionTeams)
			bInOtherTeam |= OtherSlot != TeamSlot && Team.HeroId == Hero.HeroId;
		if (!bInOtherTeam) ExpeditionVisibleHeroChoices.Add(Hero.HeroId);
	}
	if (!ExpeditionVisibleHeroChoices.Contains(ExpeditionDraft.HeroId))
		ExpeditionDraft.HeroId = ExpeditionVisibleHeroChoices.IsEmpty() ? 0 : ExpeditionVisibleHeroChoices[0];
	bExpeditionChoosingHero = false;
	if (!ExpeditionTeams.Contains(TeamSlot))
	{
		int32 Remaining = GetExpeditionDraftCapacity();
		const auto Available = GetExpeditionAvailableSoldiers(TeamSlot);
		TArray<int32> Ids;
		Available.GetKeys(Ids);
		Ids.Sort();
		for (int32 Id : Ids)
		{
			const int32 Count = FMath::Min(Remaining, Available.FindRef(Id));
			if (Count > 0) ExpeditionDraft.Soldiers.Add(Id, Count);
			Remaining -= Count;
		}
	}
	RebuildModeContent();
}

bool URokUnityFeatureScreenWidget::OpenGatherArmy(int64 TargetId, double Distance, double ResourceRadius,
	FString& Error, TOptional<bool> AttackStationAfterBattle)
{
	auto* Instance = GetGameInstance();
	auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	FRokServerArmySnapshot Snapshot;
	TArray<FRokServerHeroEntry> Heroes;
	FRokBarbarianAttackInfo Attack;
	if (!Army || !Net)
	{
		Error = TEXT("正在同步部队资料。");
		return false;
	}
	if (AttackStationAfterBattle.IsSet())
	{
		if (!Army->GetBarbarianArmyOptions(TargetId, Snapshot, Heroes, Error)
			|| !Army->GetBarbarianAttackInfo(TargetId, Attack)) return false;
	}
	else if (!Army->GetGatherOptions(TargetId, Snapshot, Heroes, Error)) return false;

	++GatherArmyComposerSerial;
	GatherArmyAttackStation = AttackStationAfterBattle;
	GatherArmyActionCost = Attack.ActionCost;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::GatherArmy;
	GatherArmyTargetId = TargetId;
	GatherArmyAoiGeneration = Net->GetWorldMapAoiGeneration();
	GatherArmyDistance = Distance;
	GatherArmyResourceRadius = ResourceRadius;
	GatherArmyDeputy = 0;
	bGatherPickingDeputy = false;
	bGatherSubmitting = false;
	bExpeditionChoosingHero = false;
	ExpeditionDraft = {};
	ExpeditionVisibleHeroChoices.Reset();
	for (const auto& Hero : Heroes) ExpeditionVisibleHeroChoices.Add(Hero.HeroId);
	ExpeditionDraft.HeroId = ExpeditionVisibleHeroChoices[0];
	GatherArmyAvailable.Reset();
	for (const auto& Soldier : Snapshot.CitySoldiers)
		GatherArmyAvailable.Add(Soldier.Id, static_cast<int32>(FMath::Min<int64>(Soldier.Count, MAX_int32)));
	RefillGatherArmy();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

void URokUnityFeatureScreenWidget::RefillGatherArmy()
{
	ExpeditionDraft.Soldiers.Reset();
	TArray<int32> Ids;
	GatherArmyAvailable.GetKeys(Ids);
	Ids.Sort(GatherSoldierOrder);
	int32 Remaining = GetExpeditionDraftCapacity();
	for (int32 Id : Ids)
	{
		const int32 Count = FMath::Min(Remaining, GatherArmyAvailable.FindRef(Id));
		if (Count > 0) ExpeditionDraft.Soldiers.Add(Id, Count);
		Remaining -= Count;
	}
}

bool URokUnityFeatureScreenWidget::HandleGatherArmyAction(int32 ActionId)
{
	if (bGatherSubmitting) return true;
	if (bExpeditionChoosingHero)
	{
		if (ActionId == 1131 || ActionId == 1)
		{
			bExpeditionChoosingHero = false;
			RebuildModeContent();
		}
		else if (ActionId == GatherRemoveDeputyAction && bGatherPickingDeputy)
		{
			GatherArmyDeputy = 0;
			bExpeditionChoosingHero = false;
			RebuildModeContent();
		}
		else if (ExpeditionVisibleHeroChoices.IsValidIndex(ActionId - HeroActionBase))
		{
			const int32 Id = ExpeditionVisibleHeroChoices[ActionId - HeroActionBase];
			if (bGatherPickingDeputy && Id != ExpeditionDraft.HeroId) GatherArmyDeputy = Id;
			else if (!bGatherPickingDeputy && Id != GatherArmyDeputy)
			{
				ExpeditionDraft.HeroId = Id;
				GatherArmyDeputy = 0;
			}
			bExpeditionChoosingHero = false;
			RebuildModeContent();
		}
		return true;
	}
	switch (ActionId)
	{
	case 1:
	case 1131:
		CloseScreen();
		return true;
	case 1132:
	case GatherDeputyAction:
		bGatherPickingDeputy = ActionId == GatherDeputyAction;
		bExpeditionChoosingHero = true;
		RebuildModeContent();
		return true;
	case 1133:
		ExpeditionDraft.Soldiers.Reset();
		RebuildModeContent();
		return true;
	case GatherInfoAction:
		ShowBoundaryFeedback(FText::FromString(TEXT("行军时间按兵种基础速度估算，实际以服务器为准。")));
		return true;
	case 1134:
		break;
	default:
		return false;
	}

	auto* Instance = GetGameInstance();
	auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	TMap<int32, int64> Soldiers;
	for (const auto& Pair : ExpeditionDraft.Soldiers)
		if (Pair.Value > 0) Soldiers.Add(Pair.Key, Pair.Value);
	FString Error(TEXT("地图已重新同步，请重新选择目标。"));
	const auto OnResult = [WeakScreen = TWeakObjectPtr<URokUnityFeatureScreenWidget>(this),
		Serial = GatherArmyComposerSerial](int32 Index, const FString& Failure)
	{
		if (auto* Screen = WeakScreen.Get(); Screen && Screen->GatherArmyComposerSerial == Serial)
			Screen->FinishGatherArmy(Index, Failure);
	};
	const bool bSent = Army && Net && Net->GetWorldMapAoiGeneration() == GatherArmyAoiGeneration
		&& (GatherArmyAttackStation.IsSet()
			? Army->RequestBarbarianAttack(GatherArmyTargetId, ExpeditionDraft.HeroId, GatherArmyDeputy, Soldiers,
				GatherArmyAttackStation.GetValue(), OnResult, Error)
			: Army->RequestGather(GatherArmyTargetId, ExpeditionDraft.HeroId, GatherArmyDeputy, Soldiers, OnResult, Error));
	if (!bSent)
	{
		ShowBoundaryFeedback(FText::FromString(Error));
		return true;
	}
	bGatherSubmitting = true;
	if (auto* Panel = GetWidgetFromName(TEXT("ExpeditionArmyRoot"))) Panel->SetIsEnabled(false);
	ShowBoundaryFeedback(FText::FromString(TEXT("正在等待服务器确认出征…")));
	return true;
}

void URokUnityFeatureScreenWidget::FinishGatherArmy(int32 ArmyIndex, const FString& Error)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::GatherArmy) return;
	bGatherSubmitting = false;
	if (ArmyIndex > 0 && Error.IsEmpty())
	{
		CloseScreen();
		return;
	}
	if (auto* Panel = GetWidgetFromName(TEXT("ExpeditionArmyRoot"))) Panel->SetIsEnabled(true);
	ShowBoundaryFeedback(FText::FromString(Error));
	UpdateExpeditionArmySummary();
}

void URokUnityFeatureScreenWidget::BuildArmyComposer()
{
	const bool bGather = ActiveMode == ERokUnityFeatureScreenMode::GatherArmy;
	auto* CommonTexture = bGather ? LoadUiTexture(Common) : nullptr;
	if (!bGather)
	{
		auto* Blocker = MakeSolidBorder(TEXT("ExpeditionArmyBackdrop"), FLinearColor(0, 0, 0, 0.76f));
		Blocker->SetVisibility(ESlateVisibility::Visible);
		PlaceOnCanvas(ModeCanvas, Blocker, FVector2D(-400, -180), FVector2D(2240, 1170), 20);
	}
	auto* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ExpeditionArmyRoot"));
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, Panel, FVector2D::ZeroVector, FVector2D(1440, 810), 21);
	PlaceOnCanvas(Panel, MakeSolidBorder(TEXT("ExpeditionArmyFrameBacking"), FLinearColor(0.15f, 0.15f, 0.15f)),
		FVector2D(169, 84), FVector2D(1097, 678));
	BuildSplitWindowFrame(FVector2D(142, 37), FVector2D(1156, 751),
		FText::FromString(bExpeditionChoosingHero ? TEXT("选择统帅") : TEXT("创建部队")), FVector2D(1220, 41), Panel, 1131);
	const auto Label = [this](UCanvasPanel* Root, FName Name, const FString& Text,
		FVector2D Pos, FVector2D Size, int32 Font = 18, ETextJustify::Type Align = ETextJustify::Left)
	{
		auto* Widget = MakeScreenText(Name, Font, FLinearColor::White, true, Align);
		Widget->SetText(FText::FromString(Text));
		PlaceOnCanvas(Root, Widget, Pos, Size, 7);
		return Widget;
	};
	auto* Catalog = GetGameplayCatalogSubsystem();
	if (bExpeditionChoosingHero)
	{
		auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ExpeditionHeroChoiceScroll"));
		PlaceOnCanvas(Panel, Scroll, FVector2D(205, 130), FVector2D(1028, 584), 6);
		if (bGather && bGatherPickingDeputy)
		{
			Scroll->AddChild(MakeActionButton(TEXT("GatherRemoveDeputy"), GatherRemoveDeputyAction, Common,
				BlueButton, CommonSize, {720, 65}, FText::FromString(TEXT("不携带副将")), 21));
		}
		for (int32 Index = 0; Index < ExpeditionVisibleHeroChoices.Num(); ++Index)
		{
			const int32 HeroId = ExpeditionVisibleHeroChoices[Index];
			if (bGather && (bGatherPickingDeputy ? HeroId == ExpeditionDraft.HeroId : HeroId == GatherArmyDeputy)) continue;
			FRokUnityCatalogHero Hero;
			if (!Catalog || !Catalog->GetHero(HeroId, Hero)) continue;
			auto* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("ExpeditionHeroChoice_%d"), Index));
			Scroll->AddChild(WrapFixedSize(Row, *FString::Printf(TEXT("ExpeditionHeroChoiceSize_%d"), Index), FVector2D(1000, 96)));
			AddExpeditionHeroHead(Row, HeroId, *FString::Printf(TEXT("ExpeditionHeroChoiceIcon_%d"), Index),
				FVector2D(18, 3), FVector2D(74, 84), 1);
			PlaceOnCanvas(Row, MakeActionButton(*FString::Printf(TEXT("ExpeditionHeroChoiceButton_%d"), Index), HeroActionBase + Index,
				Common, BlueButton, CommonSize, FVector2D(720, 65), FText::FromString(Hero.DisplayNameZhCn), 21),
				FVector2D(105, 13), FVector2D(720, 65), 2);
		}
		return;
	}

	FRokUnityCatalogHero Hero;
	const bool bHasHero = Catalog && Catalog->GetHero(ExpeditionDraft.HeroId, Hero);
	PlaceOnCanvas(Panel, MakeSolidBorder(TEXT("ExpeditionArmyPortraitShade"), FLinearColor(0.09f, 0.09f, 0.09f, 0.68f)),
		FVector2D(178, 91), FVector2D(438, 661), 2);
	if (bGather)
	{
		// UI_Win_CreateArmy/pl_SingleCaptain: pl_tro_1002 and its platform.
		PlaceOnCanvas(Panel, MakeAtlasImage(TEXT("GatherPortraitScene"), TroopUi, FIntRect(1, 923, 416, 1390),
			FIntPoint(512, 2048), {415, 467}), {192, 93}, {415, 467}, 3);
		PlaceOnCanvas(Panel, MakeAtlasImage(TEXT("GatherPortraitPlatform"), TroopUi, FIntRect(1, 870, 379, 921),
			FIntPoint(512, 2048), {378, 51}), {210, 540}, {378, 51}, 3);
	}
	else
	{
		AddExpeditionArt(Panel, TEXT("ExpeditionArmyPortraitScene"), TEXT("hero_background_14.png"),
			FVector2D(194, 144), FVector2D(404, 415), 3)->SetRenderOpacity(0.35f);
	}
	if (bHasHero)
	{
		auto* Portrait = WidgetTree->ConstructWidget<URokCommanderMeshWidget>(URokCommanderMeshWidget::StaticClass(), TEXT("ExpeditionArmyPortrait"));
		if (Portrait->LoadHero(Hero.Id))
			PlaceOnCanvas(Panel, Portrait, bGather ? FVector2D(113, 25) : FVector2D(151, 112),
				bGather ? FVector2D(576, 638) : FVector2D(495, 546), 4);
		Label(Panel, TEXT("ExpeditionArmyCommanderName"), Hero.DisplayNameZhCn,
			FVector2D(205, 142), FVector2D(320, 40), 26);
	}
	else
	{
		Label(Panel, TEXT("ExpeditionArmyCommanderName"), TEXT("统帅数据尚未同步"),
			FVector2D(202, 144), FVector2D(391, 46), 23, ETextJustify::Center);
	}
	auto* Swap = bGather
		? MakeActionButton(TEXT("ExpeditionArmySwitchHero"), 1132, TroopUi, FIntRect(172, 684, 232, 740),
			FIntPoint(512, 2048), {60, 56})
		: MakeActionButton(TEXT("ExpeditionArmySwitchHero"), 1132, Common, BlueButton, CommonSize, FVector2D(149, 51),
			FText::FromString(TEXT("更换统帅")), 18);
	Swap->SetIsEnabled(!ExpeditionVisibleHeroChoices.IsEmpty());
	Swap->SetToolTipText(FText::FromString(TEXT("更换统帅")));
	PlaceOnCanvas(Panel, Swap, bGather ? FVector2D(163, 493) : FVector2D(198, 588),
		bGather ? FVector2D(60, 56) : FVector2D(149, 51), 8);

	const auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	const auto Snapshot = Server ? Server->GetSnapshot() : FRokServerHeroSnapshot();
	const auto* Owned = Snapshot.Heroes.FindByPredicate([&Hero](const auto& Entry) { return Entry.HeroId == Hero.Id; });
	const auto* Deputy = bGather && GatherArmyDeputy > 0
		? Snapshot.Heroes.FindByPredicate([this](const auto& Entry) { return Entry.HeroId == GatherArmyDeputy; }) : nullptr;
	if (bHasHero && Owned)
	{
		PlaceOnCanvas(Panel, MakeSolidBorder(TEXT("ExpeditionArmySkillsShade"), Shade),
			FVector2D(189, 650), FVector2D(412, 82), 5);
		for (int32 Index = 0; Index < Hero.SkillIds.Num() && Index < 5; ++Index)
		{
			FRokUnityCatalogHeroSkill Skill;
			if (!Catalog->GetHeroSkill(Hero.SkillIds[Index], Skill) || Skill.IconWidth <= 0 || Skill.IconHeight <= 0) continue;
			const bool bFirstAtlas = Skill.IconAtlas == TEXT("hero_skill");
			const TCHAR* Atlas = bFirstAtlas
				? TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_skill.rok_commander_skill")
				: TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_skill2.rok_commander_skill2");
			const FIntRect IconRect(Skill.IconX, Skill.IconY, Skill.IconX + Skill.IconWidth, Skill.IconY + Skill.IconHeight);
			const FIntPoint IconSize(Skill.IconAtlasWidth, Skill.IconAtlasHeight);
			auto* Image = MakeAtlasImage(*FString::Printf(TEXT("ExpeditionArmySkill_%d"), Index),
				Atlas, IconRect, IconSize, FVector2D(58));
			const auto* Progress = Owned->Skills.FindByPredicate([&Skill](const auto& Entry) { return Entry.SkillId == Skill.Id; });
			if (!Progress || Progress->Level <= 0)
			{
				if (auto* Gray = LoadCommanderPreviewTexture(bFirstAtlas
					? TEXT("Textures/hero_skill_gray.png") : TEXT("Textures/hero_skill2_gray.png")))
					Image->SetBrush(MakeAtlasBrush(Gray, IconRect, IconSize, FVector2D(58)));
			}
			PlaceOnCanvas(Panel, Image, FVector2D(197 + 68 * Index, 662), FVector2D(58), 7);
			if (bGather && Progress && Progress->Level > 0)
				Label(Panel, *FString::Printf(TEXT("GatherSkillLevel_%d"), Index), LexToString(Progress->Level),
					{201.0f + 68.0f * Index, 697}, {32, 24}, 15);
		}
	}
	if (bGather)
	{
		auto* DeputyButton = MakeActionButton(TEXT("GatherDeputy"), GatherDeputyAction, TroopUi,
			FIntRect(172, 640, 210, 682), FIntPoint(512, 2048), {42, 46});
		const bool bDeputyUnlocked = Owned && Owned->bStarKnown && Owned->Star >= 3;
		DeputyButton->SetToolTipText(FText::FromString(bDeputyUnlocked ? TEXT("选择副将") : TEXT("主将达到三星解锁副将")));
		DeputyButton->SetIsEnabled(bDeputyUnlocked);
		PlaceOnCanvas(Panel, DeputyButton, {556, 672}, {42, 46}, 8);
		if (Deputy)
			AddExpeditionHeroHead(Panel, Deputy->HeroId, TEXT("GatherDeputyHead"), {550, 660}, {53, 62}, 9);
		PlaceOnCanvas(Panel, MakeSolidBorder(TEXT("GatherArmyAttributesShade"), Shade), {189, 610}, {412, 32}, 5);
		Label(Panel, TEXT("GatherArmyAttributes"), GatherArmyAttackStation.IsSet()
			? FString::Printf(TEXT("进攻乱军   ·   行动力 %d"), GatherArmyActionCost)
			: FString(TEXT("行军速度   ·   采集负载")), {202, 613}, {315, 28}, 16);
		PlaceOnCanvas(Panel, MakeActionButton(TEXT("GatherArmyAttributesMore"), GatherInfoAction, Common, GoldButton,
			CommonSize, {84, 30}, FText::FromString(TEXT("更多")), 17, Ink), {527, 610}, {84, 30}, 8);
	}

	auto* Config = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	ExpeditionDraftCapacity = GetExpeditionDraftCapacity();
	const auto Available = bGather ? GatherArmyAvailable : GetExpeditionAvailableSoldiers(ExpeditionDraftSlot);
	Available.GetKeys(ExpeditionDraftSoldierIds);
	if (bGather) ExpeditionDraftSoldierIds.Sort(GatherSoldierOrder);
	else ExpeditionDraftSoldierIds.Sort();
	Label(Panel, TEXT("ExpeditionArmyHeroPower"), Owned && Owned->bPowerKnown && (!Deputy || Deputy->bPowerKnown)
		? FString::Printf(TEXT("统帅战力：%s"), *FText::AsNumber(Owned->Power + (Deputy ? Deputy->Power : 0)).ToString()) : FString(TEXT("统帅战力：—")),
		FVector2D(644, 112), FVector2D(310, 29));
	Label(Panel, TEXT("ExpeditionArmyTotalPower"), TEXT(""), FVector2D(973, 112), FVector2D(239, 29), 18, ETextJustify::Right);
	auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ExpeditionArmyTroops"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(Panel, Scroll, FVector2D(642, 144), FVector2D(580, 407), 5);
	int32 Remaining = ExpeditionDraftCapacity;
	for (int32 Id : ExpeditionDraftSoldierIds)
	{
		const int32 Max = Available.FindRef(Id);
		const int32 Selected = FMath::Clamp(ExpeditionDraft.Soldiers.FindRef(Id), 0, FMath::Min(Max, Remaining));
		Remaining -= Selected;
		ExpeditionDraft.Soldiers.Add(Id, Selected);
		FRokUnityCatalogTroop Troop;
		if (!Catalog || !Catalog->GetTroop(Id, Troop)) continue;
		int32 NameId = 0;
		if (Config) Config->GetIntFieldByKey(TEXT("Arms"), TEXT("ID"), Id, TEXT("l_armsID"), NameId);
		auto* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ExpeditionSoldier_%d"), Id));
		Scroll->AddChild(WrapFixedSize(Row, *FString::Printf(TEXT("ExpeditionSoldierSize_%d"), Id), FVector2D(580, 101)));
		PlaceOnCanvas(Row, MakeSolidBorder(*FString::Printf(TEXT("ExpeditionSoldierShade_%d"), Id), Shade),
			FVector2D(0, 1), FVector2D(575, 98));
		FIntRect Portrait;
		if (RokTroopPortrait::Resolve(Troop.Icon, Portrait))
			PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ExpeditionSoldierPortrait_%d"), Id), TroopHeads,
				Portrait, FIntPoint(1024, 1024), FVector2D(70)), FVector2D(13, 12), FVector2D(70), 2);
		auto* Name = Label(Row, *FString::Printf(TEXT("ExpeditionSoldierName_%d"), Id), FRokUiTextTable::Resolve(NameId),
			FVector2D(92, 16), FVector2D(388, 28));
		if (bGather)
		{
			Name->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.61f, 0.39f)));
			PlaceOnCanvas(Row, MakeSolidBorder(*FString::Printf(TEXT("GatherSoldierCountBg_%d"), Id),
				FLinearColor(0.012f, 0.014f, 0.014f, 0.57f)), {478, 49}, {78, 37}, 3);
		}
		auto* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), *FString::Printf(TEXT("ExpeditionSoldierSlider_%d"), Id));
		Slider->SetMaxValue(FMath::Max(1, Max));
		Slider->SetStepSize(1.0f);
		Slider->SetSliderBarColor(FLinearColor::Transparent);
		auto* Progress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),
			*FString::Printf(TEXT("ExpeditionSoldierFill_%d"), Id));
		if (CommonTexture)
		{
			// UI_Item_ArmyCount uses pb_com_1000_1, _4 and _5.
			FProgressBarStyle Style = Progress->GetWidgetStyle();
			Style.SetBackgroundImage(MakeAtlasBrush(CommonTexture, FIntRect(401, 112, 421, 135), CommonSize, {20, 23}));
			Style.SetFillImage(MakeAtlasBrush(CommonTexture, FIntRect(384, 59, 396, 78), CommonSize, {12, 19}));
			Progress->SetWidgetStyle(Style);
			Progress->SetFillColorAndOpacity(FLinearColor::White);
			FSliderStyle SliderStyle = Slider->GetWidgetStyle();
			const FSlateBrush Thumb = MakeAtlasBrush(CommonTexture, FIntRect(679, 474, 699, 514), CommonSize, {16, 36});
			SliderStyle.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb);
			Slider->SetWidgetStyle(SliderStyle);
		}
		else Progress->SetFillColorAndOpacity(FLinearColor(0.1f, 0.64f, 0.28f));
		Progress->SetPercent(Max > 0 ? static_cast<float>(Selected) / Max : 0);
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Row, Progress, FVector2D(93, 62), FVector2D(384, 16), 3);
		Slider->SetIsEnabled(Max > 0 && ExpeditionDraftCapacity > 0);
		Slider->SetValue(Selected);
		Slider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleExpeditionArmySliderChanged);
		PlaceOnCanvas(Row, Slider, FVector2D(92, 50), FVector2D(385, 37), 8);
		Label(Row, *FString::Printf(TEXT("ExpeditionSoldierQuantity_%d"), Id), FText::AsNumber(Selected).ToString(),
			bGather ? FVector2D(478, 54) : FVector2D(486, 54), FVector2D(77, 28), 17,
			bGather ? ETextJustify::Center : ETextJustify::Right);
	}
	if (ExpeditionDraftSoldierIds.IsEmpty())
		Label(Panel, TEXT("ExpeditionNoAvailableSoldiers"), TEXT("暂无可编入远征的部队"),
			FVector2D(651, 248), FVector2D(550, 38), 19, ETextJustify::Center);

	PlaceOnCanvas(Panel, MakeSolidBorder(TEXT("ExpeditionArmySummaryShade"), Shade), FVector2D(642, 565), FVector2D(580, 56), 4);
	Label(Panel, TEXT("ExpeditionArmyLoad"), TEXT(""), FVector2D(664, 582), FVector2D(260, 34), 17);
	Label(Panel, TEXT("ExpeditionArmyCount"), TEXT(""), FVector2D(909, 582),
		bGather ? FVector2D(250, 34) : FVector2D(290, 34), 17, ETextJustify::Right);
	PlaceOnCanvas(Panel, MakeActionButton(TEXT("ExpeditionArmyClear"), 1133, Common, BlueButton, CommonSize, FVector2D(267, 75),
		FText::FromString(TEXT("清空")), 30, FLinearColor(0.02f, 0.11f, 0.19f)), FVector2D(644, 660), FVector2D(267, 75), 8);
	PlaceOnCanvas(Panel, MakeActionButton(TEXT("ExpeditionArmyReady"), 1134, Common, GoldButton, CommonSize, FVector2D(267, 75),
		bGather ? FText::GetEmpty() : FText::FromString(TEXT("准备")), 30, Ink), FVector2D(940, 660), FVector2D(267, 75), 8);
	if (bGather)
	{
		auto* March = MakeScreenText(TEXT("GatherMarchLabel"), 28, FLinearColor(0.06f, 0.045f, 0.019f),
			true, ETextJustify::Center);
		March->SetText(FText::FromString(GatherArmyAttackStation.IsSet() ? TEXT("出征") : TEXT("行军")));
		PlaceOnCanvas(Panel, March, {942, 671}, {264, 32}, 9);
		PlaceOnCanvas(Panel, MakeAtlasImage(TEXT("GatherMarchTimeIcon"), Common, FIntRect(1229, 372, 1244, 392),
			CommonSize, {16, 22}), {1028, 703}, {16, 22}, 9);
		auto* Time = MakeScreenText(TEXT("GatherMarchTime"), 17, FLinearColor::White, true);
		PlaceOnCanvas(Panel, Time, {1050, 703}, {115, 25}, 9);
		PlaceOnCanvas(Panel, MakeActionButton(TEXT("GatherArmyHelp"), GatherInfoAction, Common, FIntRect(559, 200, 609, 248),
			CommonSize, {44, 44}), {1175, 570}, {44, 44}, 8);
		FeedbackText = MakeScreenText(TEXT("GatherArmyFeedback"), 17, FLinearColor(1.0f, 0.84f, 0.4f), true, ETextJustify::Center);
		PlaceOnCanvas(Panel, FeedbackText, {642, 625}, {580, 29}, 9);
	}
	UpdateExpeditionArmySummary();
}

void URokUnityFeatureScreenWidget::UpdateExpeditionArmySummary()
{
	int64 Count = 0, Load = 0, Power = 0;
	float Speed = MAX_flt;
	const bool bGather = ActiveMode == ERokUnityFeatureScreenMode::GatherArmy;
	auto* Catalog = GetGameplayCatalogSubsystem();
	for (const auto& [Id, Selected] : ExpeditionDraft.Soldiers)
	{
		FRokUnityCatalogTroop Troop;
		if (!Catalog || !Catalog->GetTroop(Id, Troop)) continue;
		Count += Selected;
		Load += static_cast<int64>(Selected) * Troop.Capacity;
		Power += static_cast<int64>(Selected) * Troop.Power;
		if (Selected > 0 && Troop.Speed > 0) Speed = FMath::Min(Speed, Troop.Speed);
	}
	const auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	const auto Heroes = Server ? Server->GetSnapshot() : FRokServerHeroSnapshot();
	const auto* Hero = Heroes.Heroes.FindByPredicate([this](const auto& Entry) { return Entry.HeroId == ExpeditionDraft.HeroId; });
	const auto Set = [this](FName Name, const FString& Text)
	{
		if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(Name))) Label->SetText(FText::FromString(Text));
	};
	Set(TEXT("ExpeditionArmyLoad"), FString::Printf(TEXT("负载：%s"), *FText::AsNumber(Load).ToString()));
	Set(TEXT("ExpeditionArmyCount"), FString::Printf(TEXT("部队数量：%s/%s"),
		*FText::AsNumber(Count).ToString(), *FText::AsNumber(ExpeditionDraftCapacity).ToString()));
	const auto* Deputy = bGather && GatherArmyDeputy > 0
		? Heroes.Heroes.FindByPredicate([this](const auto& Row) { return Row.HeroId == GatherArmyDeputy; }) : nullptr;
	Set(TEXT("ExpeditionArmyTotalPower"), Hero && Hero->bPowerKnown && (!Deputy || Deputy->bPowerKnown)
		? FString::Printf(TEXT("总战力：%s"), *FText::AsNumber(Power + Hero->Power + (Deputy ? Deputy->Power : 0)).ToString())
		: FString(TEXT("总战力：—")));
	if (bGather)
	{
		// Unity CalDistance subtracts city and node radii before estimating travel time.
		constexpr double CityRadius = 5.0; // Config.cityRadius
		const int32 Seconds = Speed < MAX_flt && GatherArmyDistance > 0
			? FMath::CeilToInt(FMath::Max(0.0, GatherArmyDistance - CityRadius - GatherArmyResourceRadius) * 100.0 / Speed) : 0;
		Set(TEXT("GatherMarchTime"), Speed < MAX_flt && GatherArmyDistance > 0
			? FString::Printf(TEXT("%02d:%02d:%02d"), Seconds / 3600, Seconds / 60 % 60, Seconds % 60) : TEXT("—"));
	}
	if (auto* Ready = GetWidgetFromName(TEXT("ExpeditionArmyReady")))
		Ready->SetIsEnabled(Hero && Count > 0 && Count <= ExpeditionDraftCapacity && !(bGather && bGatherSubmitting));
}

void URokUnityFeatureScreenWidget::HandleExpeditionArmySliderChanged(float)
{
	if ((ActiveMode != ERokUnityFeatureScreenMode::Expedition || ExpeditionDraftSlot == INDEX_NONE)
		&& ActiveMode != ERokUnityFeatureScreenMode::GatherArmy) return;
	if (bGatherSubmitting && ActiveMode == ERokUnityFeatureScreenMode::GatherArmy) return;
	int32 Remaining = ExpeditionDraftCapacity;
	for (const auto& Pair : ExpeditionDraft.Soldiers) Remaining -= Pair.Value;
	for (const int32 Id : ExpeditionDraftSoldierIds)
	{
		const auto Key = [Id](const TCHAR* Prefix) { return FName(*FString::Printf(TEXT("%s%d"), Prefix, Id)); };
		auto* Slider = Cast<USlider>(GetWidgetFromName(Key(TEXT("ExpeditionSoldierSlider_"))));
		if (!Slider) continue;
		int32& Count = ExpeditionDraft.Soldiers.FindOrAdd(Id);
		const int32 Requested = FMath::RoundToInt(Slider->GetValue());
		if (Requested == Count) continue;
		Count = FMath::Clamp(Requested, 0, Count + FMath::Max(0, Remaining));
		if (auto* Quantity = Cast<UTextBlock>(GetWidgetFromName(Key(TEXT("ExpeditionSoldierQuantity_")))))
			Quantity->SetText(FText::AsNumber(Count));
		if (auto* Progress = Cast<UProgressBar>(GetWidgetFromName(Key(TEXT("ExpeditionSoldierFill_")))))
			Progress->SetPercent(static_cast<float>(Count) / Slider->GetMaxValue());
		if (Count != Requested) Slider->SetValue(Count);
		break;
	}
	UpdateExpeditionArmySummary();
}

bool URokUnityFeatureScreenWidget::HandleExpeditionAction(int32 ActionId)
{
	FRokExpeditionStagePreviewRow Stage;
	if (!RokExpeditionStagePreviewData::FindById(ExpeditionRows, SelectedExpeditionStageId, Stage))
		return false;

	if (ActionId > StageActionBase && ActionId <= StageActionBase + ExpeditionRows.Num())
	{
		if (!bExpeditionTaskConfigVisible)
		{
			if (const auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ExpeditionMapScroll"))))
				ExpeditionMapOffset = Scroll->GetScrollOffset();
			SelectedExpeditionStageId = ActionId - StageActionBase;
			RebuildModeContent();
		}
		return true;
	}
	if (ActionId >= SquadActionBase && ActionId < SquadActionBase + 5)
	{
		const int32 TeamSlot = ActionId - SquadActionBase;
		if (bExpeditionTaskConfigVisible && !bExpeditionPreview && ExpeditionDraftSlot == INDEX_NONE && TeamSlot < Stage.TroopsNumber)
			OpenExpeditionArmySlot(TeamSlot);
		return true;
	}
	if (bExpeditionChoosingHero && ExpeditionVisibleHeroChoices.IsValidIndex(ActionId - HeroActionBase))
	{
		ExpeditionDraft.HeroId = ExpeditionVisibleHeroChoices[ActionId - HeroActionBase];
		bExpeditionChoosingHero = false;
		RebuildModeContent();
		return true;
	}

	switch (ActionId)
	{
	case 1120:
		if (!bExpeditionTaskConfigVisible)
		{
			RefreshExpeditionAccount();
			if (!CanChallengeExpedition(Stage)) return true;
			if (const auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ExpeditionMapScroll"))))
				ExpeditionMapOffset = Scroll->GetScrollOffset();
			bExpeditionTaskConfigVisible = true;
			ExpeditionAnimationPhase = NAME_None;
			RebuildModeContent();
		}
		else if (!bExpeditionPreview && !ExpeditionTeams.IsEmpty())
		{
			ExpeditionLastBoundaryActionId = ActionId;
			ShowBoundaryFeedback(FText::FromString(TEXT("编队已就绪，远征战斗场景尚未接入。")));
		}
		return true;
	case 1121:
		if (bExpeditionTaskConfigVisible && !bExpeditionPreview && ExpeditionDraftSlot == INDEX_NONE)
		{
			bExpeditionPreview = true;
			ExpeditionAnimationPhase = TEXT("PreviewTransition");
			ExpeditionAnimationElapsed = 0;
			AdvanceExpeditionAnimation(0);
		}
		return true;
	case 1122:
		if (!bExpeditionTaskConfigVisible) RequestCloseScreen();
		else if (bExpeditionPreview)
		{
			bExpeditionPreview = false;
			ExpeditionAnimationPhase = TEXT("NormalTransition");
			ExpeditionAnimationElapsed = 0;
			AdvanceExpeditionAnimation(0);
		}
		else if (ExpeditionDraftSlot == INDEX_NONE)
		{
			bExpeditionTaskConfigVisible = false;
			ExpeditionTeams.Reset();
			RebuildModeContent();
		}
		return true;
	case 1124:
		if (OnExpeditionStoreRequested.IsBound())
		{
			CloseScreen();
			OnExpeditionStoreRequested.Execute();
		}
		return true;
	case 1125:
		ShowBoundaryFeedback(FText::FromString(TEXT("远征排行榜尚未同步。")));
		return true;
	case 1126:
		ShowBoundaryFeedback(FText::FromString(TEXT("完成关卡以解锁后续关卡，达成任务目标可获得星级奖励。")));
		return true;
	case 1127:
		ShowBoundaryFeedback(FText::FromString(TEXT("远征奖励领取尚未接入。")));
		return true;
	case 1131:
		if (bExpeditionChoosingHero) bExpeditionChoosingHero = false;
		else ExpeditionDraftSlot = INDEX_NONE;
		RebuildModeContent();
		return true;
	case 1132:
		if (ExpeditionDraftSlot != INDEX_NONE && !ExpeditionVisibleHeroChoices.IsEmpty())
		{
			bExpeditionChoosingHero = true;
			RebuildModeContent();
		}
		return true;
	case 1133:
		if (ExpeditionDraftSlot != INDEX_NONE)
		{
			ExpeditionDraft.Soldiers.Reset();
			RebuildModeContent();
		}
		return true;
	case 1134:
		if (ExpeditionDraftSlot != INDEX_NONE)
		{
			const auto Available = GetExpeditionAvailableSoldiers(ExpeditionDraftSlot);
			int64 Count = 0;
			for (const auto& [Id, Selected] : ExpeditionDraft.Soldiers)
			{
				if (Selected < 0 || Selected > Available.FindRef(Id)) return true;
				Count += Selected;
			}
			if (Count > 0 && Count <= GetExpeditionDraftCapacity()
				&& ExpeditionVisibleHeroChoices.Contains(ExpeditionDraft.HeroId))
			{
				ExpeditionTeams.Add(ExpeditionDraftSlot, ExpeditionDraft);
				ExpeditionDraftSlot = INDEX_NONE;
				RebuildModeContent();
			}
		}
		return true;
	default:
		return false;
	}
}
