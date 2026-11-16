#include "RokServerHeroSubsystem.h"

#include "RokGameNetSubsystem.h"
#include "RokSproto.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokUnityConfigSubsystem.h"
#include "RokServerInventorySubsystem.h"
#include "Dom/JsonValue.h"

namespace
{
bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64& Out)
{
	return Object.IsValid() && RokSprotoJson::TryGetInt64(Object, Field, Out);
}

bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
{
	return Object.IsValid() && Object->TryGetStringField(Field, Out);
}

bool ReadRows(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
	TArray<TPair<int32, TSharedPtr<FJsonObject>>>& Out)
{
	if (!Object.IsValid()) return false;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Object->TryGetArrayField(Field, Array))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Row) && Row) Out.Emplace(0, *Row);
		}
		return true;
	}
	const TSharedPtr<FJsonObject>* Map = nullptr;
	if (Object->TryGetObjectField(Field, Map))
	{
		for (const auto& Pair : (*Map)->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			int32 Key = 0;
			if (Pair.Value.IsValid() && Pair.Value->TryGetObject(Row) && Row
				&& LexTryParseString(Key, *Pair.Key)) Out.Emplace(Key, *Row);
		}
		return true;
	}
	return false;
}

void AppendInt(FString& Signature, const TCHAR* Name, bool bKnown, int64 Value)
{
	Signature += FString::Printf(TEXT("|%s=%s"), Name,
		bKnown ? *LexToString(Value) : TEXT("?"));
}

void AppendString(FString& Signature, const TCHAR* Name, const FString& Value)
{
	Signature += FString::Printf(TEXT("|%s:%d:%s"), Name, Value.Len(), *Value);
}

void ReadGear(const TSharedPtr<FJsonObject>& Object, FRokServerHeroGear& Gear)
{
	int64 Value = 0;
#define READ_GEAR(Field, Member) \
	Gear.b##Member##Known = ReadInt(Object, TEXT(Field), Value); \
	if (Gear.b##Member##Known) Gear.Member = Value;
	READ_GEAR("head", Head)
	READ_GEAR("breastPlate", BreastPlate)
	READ_GEAR("weapon", Weapon)
	READ_GEAR("gloves", Gloves)
	READ_GEAR("pants", Pants)
	READ_GEAR("accessories1", Accessories1)
	READ_GEAR("accessories2", Accessories2)
	READ_GEAR("shoes", Shoes)
#undef READ_GEAR
}

void AppendGear(FString& Signature, const FRokServerHeroGear& Gear)
{
	AppendInt(Signature, TEXT("head"), Gear.bHeadKnown, Gear.Head);
	AppendInt(Signature, TEXT("breast"), Gear.bBreastPlateKnown, Gear.BreastPlate);
	AppendInt(Signature, TEXT("weapon"), Gear.bWeaponKnown, Gear.Weapon);
	AppendInt(Signature, TEXT("gloves"), Gear.bGlovesKnown, Gear.Gloves);
	AppendInt(Signature, TEXT("pants"), Gear.bPantsKnown, Gear.Pants);
	AppendInt(Signature, TEXT("accessories1"), Gear.bAccessories1Known, Gear.Accessories1);
	AppendInt(Signature, TEXT("accessories2"), Gear.bAccessories2Known, Gear.Accessories2);
	AppendInt(Signature, TEXT("shoes"), Gear.bShoesKnown, Gear.Shoes);
}

const FRokServerHeroTalentTree* FindTalentTree(const FRokServerHeroEntry& Hero, int32 Page)
{
	return Hero.TalentTrees.FindByPredicate([Page](const auto& Tree) { return Tree.Index == Page; });
}

bool ReadTalentNode(URokUnityConfigSubsystem& Config, int32 Id, FRokHeroTalentNode& Node)
{
	Node.Id = Id;
	return Config.GetIntFieldByKey(TEXT("HeroTalentGainTree"), TEXT("ID"), Id, TEXT("gainTree"), Node.Group)
		&& Config.GetIntFieldByKey(TEXT("HeroTalentGainTree"), TEXT("ID"), Id, TEXT("level"), Node.Level);
}

// Unity HeroProxy.Hero.UpdatePower derives this from the synchronized hero,
// including only the active talent page. Hero_HeroInfo carries no power field.
bool CalculatePower(const FRokServerHeroEntry& Hero, const FRokUnityCatalogHero& Definition,
	const URokUnityGameplayCatalogSubsystem& Catalog, URokUnityConfigSubsystem& Config, int32& OutPower)
{
	if (!Hero.bLevelKnown || Hero.Level <= 0 || !Hero.bSkillsKnown
		|| !Hero.bTalentIndexKnown || !Hero.bTalentTreesKnown) return false;
	int32 LevelScore = 0;
	if (!Config.GetIntFieldByKey(TEXT("HeroLevel"), TEXT("ID"), Definition.Rare * 10000 + Hero.Level - 1,
		TEXT("score"), LevelScore)) return false;
	int32 Power = Definition.Score + LevelScore;
	for (const auto& Skill : Hero.Skills)
	{
		if (!Skill.bLevelKnown) return false;
		if (Skill.Level <= 0) continue;
		int32 Score = 0;
		if (!Config.GetIntFieldByKey(TEXT("HeroSkillEffect"), TEXT("ID"), Skill.SkillId * 1000 + Skill.Level,
			TEXT("score"), Score)) return false;
		Power += Score;
	}
	if (const auto* Tree = FindTalentTree(Hero, Hero.TalentIndex))
	{
		if (!Tree->bTalentIdsKnown) return false;
		TMap<int32, int32> PointsByTree;
		for (int32 Id : Tree->TalentIds)
		{
			FRokHeroTalentNode Node;
			int32 Score = 0;
			if (!ReadTalentNode(Config, Id, Node) || !Config.GetIntFieldByKey(TEXT("HeroTalentGainTree"),
				TEXT("ID"), Id, TEXT("score"), Score)) return false;
			Power += Score;
			++PointsByTree.FindOrAdd(Node.Group);
		}
		if (!PointsByTree.IsEmpty())
		{
			TMap<int32, int32> MasteryPoints;
			for (int32 Id : Definition.TalentIds)
			{
				FRokUnityCatalogHeroTalent Talent;
				if (!Catalog.GetHeroTalent(Id, Talent)) return false;
				MasteryPoints.Add(Talent.MasteryGroupId, PointsByTree.FindRef(Talent.GainTree));
			}
			const auto* Masteries = Config.GetTableRows(TEXT("HeroTalentMastery"));
			if (!Masteries) return false;
			for (const auto& Row : *Masteries)
			{
				int32 Group = 0, Required = 0, Score = 0;
				if (Row && Row->TryGetNumberField(TEXT("group"), Group)
					&& MasteryPoints.Contains(Group)
					&& Row->TryGetNumberField(TEXT("needTalentPoint"), Required)
					&& MasteryPoints.FindRef(Group) >= Required && Row->TryGetNumberField(TEXT("score"), Score))
					Power += Score;
			}
		}
	}
	OutPower = Power;
	return true;
}
}

FRokServerHeroSnapshot URokServerHeroSubsystem::GetSnapshot() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestSnapshot.IsSet()) return TestSnapshot.GetValue();
#endif
	FRokServerHeroSnapshot Empty;
	const UGameInstance* Instance = GetGameInstance();
	const URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net) return Empty;
	if (Net->GetState() == ERokGameNetState::LoggingIn
		|| Net->GetState() == ERokGameNetState::Connected)
	{
		return ProjectSnapshot(nullptr, Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>());
	}
	if (Net->GetState() != ERokGameNetState::RoleReady) return Empty;
	return ProjectSnapshot(Net->GetCollection(TEXT("Hero_HeroInfo"), TEXT("heroInfo")),
		Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>(), Instance->GetSubsystem<URokUnityConfigSubsystem>());
}

FRokServerHeroSnapshot URokServerHeroSubsystem::ProjectSnapshot(
	const TSharedPtr<FJsonObject>& HeroInfo, const URokUnityGameplayCatalogSubsystem* Catalog,
	URokUnityConfigSubsystem* ConfigTables)
{
	FRokServerHeroSnapshot Result;
	Result.bAvailable = true;
	Result.bHeroesKnown = HeroInfo.IsValid();
	Result.bRowsComplete = Result.bHeroesKnown;
	Result.bLoaded = Result.bHeroesKnown && Catalog && Catalog->IsCatalogLoaded();
	Result.Signature = FString::Printf(TEXT("hero:%d:%d"), Result.bHeroesKnown, Result.bLoaded);
	if (!Result.bHeroesKnown) return Result;

	// Hero_HeroInfo.heroInfo is keyed by heroId. The Unity proxy creates an owned
	// hero only for rows received here; config-only heroes are not owned.
	for (const auto& Pair : HeroInfo->Values)
	{
		int32 Id = 0;
		const TSharedPtr<FJsonObject>* RowPtr = nullptr;
		if (!LexTryParseString(Id, *Pair.Key) || Id <= 0 || !Pair.Value.IsValid()
			|| !Pair.Value->TryGetObject(RowPtr) || !RowPtr || !RowPtr->IsValid())
		{
			Result.bRowsComplete = false;
			continue;
		}
		const TSharedPtr<FJsonObject>& Row = *RowPtr;
		FRokServerHeroEntry Hero;
		Hero.HeroId = Id;
		FRokUnityCatalogHero Config;
		if (Catalog && Catalog->GetHero(Id, Config))
		{
			Hero.bCatalogKnown = true;
			Hero.Name = Config.DisplayNameZhCn;
			Hero.Title = Config.DisplayTitleZhCn;
			Hero.Icon = Config.Icon;
			Hero.IconAtlas = Config.IconAtlas;
			Hero.IconSprite = Config.IconSprite;
			Hero.Rare = Config.Rare;
		}
		int64 Value = 0;
		Hero.bLevelKnown = ReadInt(Row, TEXT("level"), Value);
		if (Hero.bLevelKnown) Hero.Level = int32(Value);
		Hero.bStarKnown = ReadInt(Row, TEXT("star"), Value);
		if (Hero.bStarKnown) Hero.Star = int32(Value);
		Hero.bExpKnown = ReadInt(Row, TEXT("exp"), Value);
		if (Hero.bExpKnown) Hero.Exp = Value;
		Hero.bStarExpKnown = ReadInt(Row, TEXT("starExp"), Value);
		if (Hero.bStarExpKnown) Hero.StarExp = Value;
		Hero.bTalentIndexKnown = ReadInt(Row, TEXT("talentIndex"), Value);
		if (Hero.bTalentIndexKnown) Hero.TalentIndex = int32(Value);
		ReadGear(Row, Hero.Gear);

		TArray<TPair<int32, TSharedPtr<FJsonObject>>> SkillRows;
		Hero.bSkillsKnown = ReadRows(Row, TEXT("skills"), SkillRows);
		for (const auto& SkillRow : SkillRows)
		{
			int64 SkillId = 0;
			if (!ReadInt(SkillRow.Value, TEXT("skillId"), SkillId) || SkillId <= 0) continue;
			FRokServerHeroSkill Skill;
			Skill.SkillId = int32(SkillId);
			Skill.bLevelKnown = ReadInt(SkillRow.Value, TEXT("skillLevel"), Value);
			if (Skill.bLevelKnown) Skill.Level = int32(Value);
			Hero.Skills.Add(Skill);
		}
		Hero.Skills.Sort([](const auto& A, const auto& B) { return A.SkillId < B.SkillId; });

		TArray<TPair<int32, TSharedPtr<FJsonObject>>> TalentRows;
		Hero.bTalentTreesKnown = ReadRows(Row, TEXT("talentTrees"), TalentRows);
		for (const auto& TalentRow : TalentRows)
		{
			int64 Index = 0;
			if (!ReadInt(TalentRow.Value, TEXT("index"), Index)) Index = TalentRow.Key;
			if (Index <= 0) continue;
			FRokServerHeroTalentTree Tree;
			Tree.Index = int32(Index);
			Tree.bNameKnown = ReadString(TalentRow.Value, TEXT("name"), Tree.Name);
			const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
			Tree.bTalentIdsKnown = TalentRow.Value->TryGetArrayField(TEXT("talentTree"), Ids);
			if (Tree.bTalentIdsKnown)
			{
				for (const auto& IdValue : *Ids)
				{
					int64 TalentId = 0;
					double Number = 0.0;
					if (IdValue.IsValid() && IdValue->TryGetNumber(Number))
						Tree.TalentIds.Add(int32(Number));
				}
			}
			Hero.TalentTrees.Add(MoveTemp(Tree));
		}
		Hero.TalentTrees.Sort([](const auto& A, const auto& B) { return A.Index < B.Index; });
		if (Hero.bCatalogKnown && ConfigTables)
			Hero.bPowerKnown = CalculatePower(Hero, Config, *Catalog, *ConfigTables, Hero.Power);
		Result.Heroes.Add(MoveTemp(Hero));
	}
	Result.Heroes.Sort([](const auto& A, const auto& B) { return A.HeroId < B.HeroId; });
	Result.Signature += FString::Printf(TEXT("|complete:%d"), Result.bRowsComplete);
	for (const FRokServerHeroEntry& Hero : Result.Heroes)
	{
		AppendInt(Result.Signature, TEXT("id"), true, Hero.HeroId);
		AppendInt(Result.Signature, TEXT("catalog"), true, Hero.bCatalogKnown);
		AppendString(Result.Signature, TEXT("name"), Hero.Name);
		AppendString(Result.Signature, TEXT("title"), Hero.Title);
		AppendString(Result.Signature, TEXT("icon"), Hero.Icon);
		AppendString(Result.Signature, TEXT("atlas"), Hero.IconAtlas);
		AppendString(Result.Signature, TEXT("sprite"), Hero.IconSprite);
		AppendInt(Result.Signature, TEXT("rare"), Hero.bCatalogKnown, Hero.Rare);
		AppendInt(Result.Signature, TEXT("level"), Hero.bLevelKnown, Hero.Level);
		AppendInt(Result.Signature, TEXT("power"), Hero.bPowerKnown, Hero.Power);
		AppendInt(Result.Signature, TEXT("star"), Hero.bStarKnown, Hero.Star);
		AppendInt(Result.Signature, TEXT("exp"), Hero.bExpKnown, Hero.Exp);
		AppendInt(Result.Signature, TEXT("starExp"), Hero.bStarExpKnown, Hero.StarExp);
		AppendInt(Result.Signature, TEXT("talentIndex"), Hero.bTalentIndexKnown, Hero.TalentIndex);
		AppendInt(Result.Signature, TEXT("skillsKnown"), true, Hero.bSkillsKnown);
		for (const auto& Skill : Hero.Skills)
		{
			AppendInt(Result.Signature, TEXT("skill"), true, Skill.SkillId);
			AppendInt(Result.Signature, TEXT("skillLevel"), Skill.bLevelKnown, Skill.Level);
		}
		AppendInt(Result.Signature, TEXT("treesKnown"), true, Hero.bTalentTreesKnown);
		for (const auto& Tree : Hero.TalentTrees)
		{
			AppendInt(Result.Signature, TEXT("tree"), true, Tree.Index);
			AppendInt(Result.Signature, TEXT("treeNameKnown"), true, Tree.bNameKnown);
			AppendString(Result.Signature, TEXT("treeName"), Tree.Name);
			AppendInt(Result.Signature, TEXT("talentIdsKnown"), true, Tree.bTalentIdsKnown);
			for (int32 TalentId : Tree.TalentIds) AppendInt(Result.Signature, TEXT("talent"), true, TalentId);
		}
		AppendGear(Result.Signature, Hero.Gear);
	}
	return Result;
}

bool URokServerHeroSubsystem::GetItemCount(int32 ItemId, int64& Count) const
{
	Count = 0;
	const auto* Inventory = GetGameInstance()->GetSubsystem<URokServerInventorySubsystem>();
	const auto Snapshot = Inventory ? Inventory->GetSnapshot() : FRokServerInventorySnapshot();
	if (!Snapshot.bItemsKnown || !Snapshot.bRowsComplete) return false;
	for (const auto& Stack : Snapshot.Stacks)
	{
		if (Stack.ItemId == ItemId) Count += Stack.Overlay;
	}
	return true;
}

int32 URokServerHeroSubsystem::GetNotificationCount() const
{
	const auto Snapshot = GetSnapshot();
	const auto* Catalog = GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	auto* Config = GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>();
	if (!Snapshot.bAvailable || !Snapshot.bRowsComplete || !Catalog || !Config) return 0;

	const auto* InventoryService = GetGameInstance()->GetSubsystem<URokServerInventorySubsystem>();
	const auto Inventory = InventoryService ? InventoryService->GetSnapshot() : FRokServerInventorySnapshot();
	const FString Signature = Snapshot.Signature + TEXT("|") + Inventory.Signature;
	if (Signature == LastNotificationSignature) return CachedNotificationCount;
	TMap<int32, int64> ItemAmounts;
	if (Inventory.bItemsKnown && Inventory.bRowsComplete)
	{
		for (const auto& Stack : Inventory.Stacks)
		{
			if (Stack.ItemId > 0 && Stack.ItemId <= MAX_int32 && Stack.Overlay > 0)
			{
				int64& Amount = ItemAmounts.FindOrAdd(static_cast<int32>(Stack.ItemId));
				Amount += FMath::Min(Stack.Overlay, MAX_int64 - Amount);
			}
		}
	}
	TSet<int32> Owned;
	int32 Count = 0;
	for (const auto& Hero : Snapshot.Heroes)
	{
		Owned.Add(Hero.HeroId);
		FRokUnityCatalogHero Definition;
		if (!Catalog->GetHero(Hero.HeroId, Definition) || Definition.ListDisplay == 1) continue;
		FRokHeroCommand Command;
		Command.Type = ERokHeroCommand::UpgradeSkill;
		Command.HeroId = Hero.HeroId;
		FRokHeroCommandCost Cost;
		FString Failure;
		if (GetCommandCost(Command, Cost, Failure)) ++Count;

		int32 LevelLimit = 0;
		if (Hero.bLevelKnown && Hero.bStarKnown && Hero.Star < Definition.MaxStar
			&& Config->GetIntFieldByKey(TEXT("HeroStar"), TEXT("ID"), Hero.Star, TEXT("starLimit"), LevelLimit)
			&& Hero.Level == LevelLimit) ++Count;

		TArray<FRokHeroTalentNode> Nodes;
		int32 Points = 0;
		if (Hero.bTalentIndexKnown && GetTalentPage(Hero, Hero.TalentIndex, Nodes, Points) && Points > 0) ++Count;
	}

	for (int32 Id : Catalog->GetHeroIds())
	{
		if (Owned.Contains(Id)) continue;
		FRokUnityCatalogHero Definition;
		if (Catalog->GetHero(Id, Definition) && Definition.ListDisplay != 1
			&& Definition.GetItem > 0 && Definition.GetItemNum > 0
			&& ItemAmounts.FindRef(Definition.GetItem) >= Definition.GetItemNum) ++Count;
	}
	LastNotificationSignature = Signature;
	CachedNotificationCount = Count;
	return Count;
}

bool URokServerHeroSubsystem::GetTalentPage(const FRokServerHeroEntry& Hero, int32 Page,
	TArray<FRokHeroTalentNode>& Nodes, int32& Points) const
{
	Nodes.Reset();
	Points = 0;
	auto* Instance = GetGameInstance();
	auto* Config = Instance->GetSubsystem<URokUnityConfigSubsystem>();
	const auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	FRokUnityCatalogHero Definition;
	if (!Config || !Catalog || !Catalog->GetHero(Hero.HeroId, Definition) || !Hero.bLevelKnown
		|| !Hero.bStarKnown || !Hero.bTalentTreesKnown || Page < 1 || Page > 3) return false;
	const auto* Levels = Config->GetTableRows(TEXT("HeroLevel"));
	const auto* Stars = Config->GetTableRows(TEXT("HeroStar"));
	const auto* Talents = Config->GetTableRows(TEXT("HeroTalentGainTree"));
	if (!Levels || !Stars || !Talents) return false;
	for (const auto& Row : *Levels)
	{
		int32 Rare = 0, Level = 0, Granted = 0;
		if (Row && Row->TryGetNumberField(TEXT("rareGroup"), Rare) && Rare == Definition.Rare
			&& Row->TryGetNumberField(TEXT("lv"), Level) && Level <= Hero.Level
			&& Row->TryGetNumberField(TEXT("starEffectData"), Granted)) Points += FMath::Max(0, Granted);
	}
	for (const auto& Row : *Stars)
	{
		int32 Star = 0, Granted = 0;
		if (Row && Row->TryGetNumberField(TEXT("ID"), Star) && Star <= Hero.Star
			&& Row->TryGetNumberField(TEXT("starEffectData"), Granted)) Points += FMath::Max(0, Granted);
	}
	if (const auto* Tree = FindTalentTree(Hero, Page))
	{
		if (!Tree->bTalentIdsKnown) return false;
		Points = FMath::Max(0, Points - Tree->TalentIds.Num());
	}
	TSet<int32> Groups;
	for (int32 Id : Definition.TalentIds)
	{
		FRokUnityCatalogHeroTalent Talent;
		if (Catalog->GetHeroTalent(Id, Talent)) Groups.Add(Talent.GainTree);
	}
	for (const auto& Row : *Talents)
	{
		FRokHeroTalentNode Node;
		if (Row && Row->TryGetNumberField(TEXT("gainTree"), Node.Group) && Groups.Contains(Node.Group)
			&& Row->TryGetNumberField(TEXT("ID"), Node.Id)
			&& Row->TryGetNumberField(TEXT("level"), Node.Level))
		{
			Row->TryGetNumberField(TEXT("nameID"), Node.NameId);
			Row->TryGetNumberField(TEXT("descID"), Node.DescriptionId);
			Nodes.Add(Node);
		}
	}
	Nodes.Sort([](const auto& Left, const auto& Right)
	{
		return Left.Group == Right.Group ? Left.Level < Right.Level : Left.Group < Right.Group;
	});
	return !Nodes.IsEmpty();
}

bool URokServerHeroSubsystem::GetCommandCost(const FRokHeroCommand& Request, FRokHeroCommandCost& Cost, FString& Error) const
{
	Cost = {};
	Error.Reset();
	const auto Snapshot = GetSnapshot();
	const auto* Hero = Snapshot.Heroes.FindByPredicate(
		[&Request](const auto& Row) { return Row.HeroId == Request.HeroId; });
	auto* Instance = GetGameInstance();
	const auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	auto* Config = Instance->GetSubsystem<URokUnityConfigSubsystem>();
	FRokUnityCatalogHero Definition;
	auto Fail = [&Error](const TCHAR* Text) { Error = Text; return false; };
	if (!Snapshot.bAvailable || !Snapshot.bRowsComplete || !Config || !Catalog
		|| !Catalog->GetHero(Request.HeroId, Definition)) return Fail(TEXT("统帅数据尚未同步。"));

	if (Request.Type == ERokHeroCommand::Summon)
	{
		if (Hero) return Fail(TEXT("已拥有该统帅。"));
		Cost.ItemId = Definition.GetItem;
		Cost.ItemCount = Definition.GetItemNum;
	}
	else if (!Hero)
	{
		return Fail(TEXT("请先招募该统帅。"));
	}
	else if (Request.Type == ERokHeroCommand::UpgradeSkill)
	{
		if (!Hero->bSkillsKnown || Hero->Skills.IsEmpty()
			|| Hero->Skills.ContainsByPredicate([](const auto& Row) { return !Row.bLevelKnown; }))
			return Fail(TEXT("技能等级尚未同步。"));
		int32 Upgrades = 0;
		bool bHasUpgradeableSkill = false;
		for (const auto& Skill : Hero->Skills)
		{
			if (Skill.Level <= 0) continue;
			Upgrades += Skill.Level - 1;
			int32 Score = 0;
			bHasUpgradeableSkill |= Skill.Level < 5 && Config->GetIntFieldByKey(TEXT("HeroSkillEffect"), TEXT("ID"),
				Skill.SkillId * 1000 + Skill.Level + 1, TEXT("score"), Score);
		}
		if (!bHasUpgradeableSkill) return Fail(TEXT("当前已解锁的技能均已达到最高等级。"));
		Cost.ItemId = Definition.GetItem;
		if (!Config->GetIntFieldByKey(TEXT("HeroSkillLevel"), TEXT("ID"), 100 + Upgrades,
			*FString::Printf(TEXT("costItem%d"), Definition.Rare), Cost.ItemCount))
			return Fail(TEXT("技能升级费用尚未同步。"));
	}
	else
	{
		if (Request.Page < 1 || Request.Page > 3 || !Hero->bTalentIndexKnown || !Hero->bTalentTreesKnown)
			return Fail(TEXT("天赋方案尚未同步。"));
		const auto* Tree = FindTalentTree(*Hero, Request.Page);
		if (Request.Type == ERokHeroCommand::LearnTalent)
		{
			TArray<FRokHeroTalentNode> Nodes;
			int32 Points = 0;
			if (!GetTalentPage(*Hero, Request.Page, Nodes, Points)) return Fail(TEXT("天赋配置尚未同步。"));
			if (Points <= 0) return Fail(TEXT("没有可用的天赋点。"));
			const auto* Node = Nodes.FindByPredicate([&Request](const auto& Row) { return Row.Id == Request.TalentId; });
			if (!Node) return Fail(TEXT("该统帅无法学习此天赋。"));
			bool bHasPrerequisite = Node->Level == 1;
			if (Tree)
			{
				for (int32 Id : Tree->TalentIds)
				{
					FRokHeroTalentNode Learned;
					if (!ReadTalentNode(*Config, Id, Learned)) return Fail(TEXT("天赋配置尚未同步。"));
					if (Learned.Level == Node->Level) return Fail(TEXT("该方案已学习同层天赋。"));
					bHasPrerequisite |= Learned.Level + 1 == Node->Level;
				}
			}
			if (!bHasPrerequisite) return Fail(TEXT("请先学习上一层天赋。"));
			return true;
		}
		if (Request.Type == ERokHeroCommand::RenameTalent)
		{
			const int32 Codepoints = StringCast<UTF32CHAR>(*Request.Name).Length();
			if (Codepoints < 1 || Codepoints > 15) return Fail(TEXT("方案名称需要 1 至 15 个字符。"));
			if (Tree && Tree->bNameKnown && Tree->Name == Request.Name) return Fail(TEXT("方案名称相同。"));
			return true;
		}
		if (Request.Type == ERokHeroCommand::SwitchTalent && Hero->TalentIndex == Request.Page)
			return Fail(TEXT("该方案已启用。"));
		if (Request.Type == ERokHeroCommand::ResetTalent)
		{
			if (!Tree || !Tree->bTalentIdsKnown || Tree->TalentIds.IsEmpty())
				return Fail(TEXT("该方案没有可重置的天赋。"));
			if (Hero->TalentIndex != Request.Page) return true;
		}

		const auto* Rows = Config->GetTableRows(TEXT("Config"));
		if (Rows)
		{
			for (const auto& Row : *Rows)
			{
				FString Key, Value;
				if (Row && Row->TryGetStringField(TEXT("Scouts"), Key) && Key == TEXT("talentResetItemID")
					&& Row->TryGetStringField(TEXT("Column5"), Value))
				{
					LexTryParseString(Cost.ItemId, *Value);
					break;
				}
			}
		}
		if (Cost.ItemId <= 0) return Fail(TEXT("天赋重置道具配置尚未同步。"));
		if (Request.bUseGems)
		{
			if (!Config->GetIntFieldByKey(TEXT("Item"), TEXT("ID"), Cost.ItemId, TEXT("shortcutPrice"), Cost.Gems)
				|| Cost.Gems <= 0) return Fail(TEXT("宝石费用尚未同步。"));
			const auto* Net = Instance->GetSubsystem<URokGameNetSubsystem>();
			int64 Gems = 0;
			if (!Net || !ReadInt(Net->GetRoleInfo(), TEXT("denar"), Gems)) return Fail(TEXT("宝石余额尚未同步。"));
			return Gems >= Cost.Gems || Fail(TEXT("宝石不足。"));
		}
		Cost.ItemCount = 1;
	}
	if (Cost.ItemId <= 0 || Cost.ItemCount <= 0) return Fail(TEXT("统帅道具费用尚未同步。"));
	if (!GetItemCount(Cost.ItemId, Cost.AvailableItems)) return Fail(TEXT("道具数量尚未同步。"));
	return Cost.AvailableItems >= Cost.ItemCount || Fail(TEXT("所需道具不足。"));
}

bool URokServerHeroSubsystem::SubmitCommand(const FRokHeroCommand& Request,
	TFunction<void(const FString&)> OnResult, FString& Error)
{
	FRokHeroCommandCost Cost;
	if (!GetCommandCost(Request, Cost, Error)) return false;
	auto* Net = GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
	if (Net) return Net->RequestHeroCommand(Request, MoveTemp(OnResult), Error);
	Error = TEXT("服务器连接尚未就绪。");
	return false;
}

bool URokServerHeroSubsystem::IsCommandApplied(const FRokHeroCommand& Request,
	const TSharedPtr<FJsonObject>& HeroInfo, int32 SkillId, int32 SkillLevel)
{
	const auto Snapshot = ProjectSnapshot(HeroInfo, nullptr);
	const auto* Hero = Snapshot.Heroes.FindByPredicate(
		[&Request](const auto& Row) { return Row.HeroId == Request.HeroId; });
	if (!Hero) return false;
	const auto* Tree = FindTalentTree(*Hero, Request.Page);
	switch (Request.Type)
	{
	case ERokHeroCommand::Summon:
		return Hero->bLevelKnown && Hero->Level > 0;
	case ERokHeroCommand::UpgradeSkill:
		return SkillId > 0 && SkillLevel > 0 && Hero->Skills.ContainsByPredicate([SkillId, SkillLevel](const auto& Row)
		{
			return Row.SkillId == SkillId && Row.bLevelKnown && Row.Level >= SkillLevel;
		});
	case ERokHeroCommand::LearnTalent:
		return Tree && Tree->bTalentIdsKnown && Tree->TalentIds.Contains(Request.TalentId);
	case ERokHeroCommand::SwitchTalent:
		return Hero->bTalentIndexKnown && Hero->TalentIndex == Request.Page;
	case ERokHeroCommand::ResetTalent:
		return Tree && Tree->bTalentIdsKnown && Tree->TalentIds.IsEmpty();
	case ERokHeroCommand::RenameTalent:
		return Tree && Tree->bNameKnown && Tree->Name == Request.Name;
	}
	return false;
}
