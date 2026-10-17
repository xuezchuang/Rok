#pragma once

#include "RokUnityGameplayCatalogSubsystem.h"

/** Unity UI_Item_CaptainSkill_SubView config projection. No hero/account level is read. */
struct FRokCommanderSkillReadonlyDetail
{
	FString Title;
	FString Body;
};

namespace RokCommanderSkillReadonlyDetail
{
inline bool Build(const FRokUnityCatalogHeroSkill& Skill, FRokCommanderSkillReadonlyDetail& Out)
{
	Out = FRokCommanderSkillReadonlyDetail();
	if (Skill.Id <= 0 || Skill.LevelEffects.Num() != 4)
	{
		return false;
	}
	const FString Name = !Skill.DisplayNameZhCn.IsEmpty()
		? Skill.DisplayNameZhCn : Skill.DisplayNameEn;
	const FString Type = !Skill.DisplayTypeZhCn.IsEmpty()
		? Skill.DisplayTypeZhCn : Skill.DisplayTypeEn;
	const FString Unlock = !Skill.DisplayOpenZhCn.IsEmpty()
		? Skill.DisplayOpenZhCn : Skill.DisplayOpenEn;
	FString Message = !Skill.DisplayMessageZhCn.IsEmpty()
		? Skill.DisplayMessageZhCn : Skill.DisplayMessageEn;
	if (Name.IsEmpty() || Type.IsEmpty() || Unlock.IsEmpty() || Message.IsEmpty())
	{
		return false;
	}
	TArray<TArray<FString>> ValuesByGroup;
	ValuesByGroup.SetNum(4);
	FString LevelRows;
	for (int32 Group = 0; Group < 4; ++Group)
	{
		const FRokUnityCatalogHeroSkillLevelEffect& Effect = Skill.LevelEffects[Group];
		if (Effect.Data.IsEmpty() && Effect.LocaleId == 0)
		{
			continue;
		}
		const FString Label = !Effect.DisplayTextZhCn.IsEmpty()
			? Effect.DisplayTextZhCn : Effect.DisplayTextEn;
		if (Effect.LocaleId <= 0 || Label.IsEmpty())
		{
			return false;
		}
		Effect.Data.ParseIntoArray(ValuesByGroup[Group], TEXT("|"), false);
		if (ValuesByGroup[Group].Num() != 5
			|| ValuesByGroup[Group].ContainsByPredicate(
				[](const FString& Value) { return Value.IsEmpty(); }))
		{
			return false;
		}
		const TArray<FString>& Values = ValuesByGroup[Group];
		LevelRows += FString::Printf(TEXT("%s  1:%s  2:%s  3:%s  4:%s  5:%s\n"),
			*Label, *Values[0], *Values[1], *Values[2], *Values[3], *Values[4]);
	}
	for (int32 Group = 0; Group < 4; ++Group)
	{
		const FString Placeholder = FString::Printf(TEXT("{%d}"), Group);
		if (Message.Contains(Placeholder, ESearchCase::CaseSensitive))
		{
			if (ValuesByGroup[Group].Num() != 5)
			{
				return false;
			}
			Message.ReplaceInline(*Placeholder, *ValuesByGroup[Group][0], ESearchCase::CaseSensitive);
		}
	}
	if (Message.Contains(TEXT("{")) || Message.Contains(TEXT("}")))
	{
		return false;
	}
	Out.Title = Name;
	const TCHAR* EffectLabel = Skill.Type == 4 && Skill.Open == 5
		? TEXT("觉醒技能配置说明（未验证账户觉醒状态）")
		: TEXT("配置1级效果");
	const FString RageLine = Skill.Anger > 0
		? FString::Printf(TEXT("怒气消耗：%d\n"), Skill.Anger)
		: FString();
	Out.Body = FString::Printf(
		TEXT("配置技能（非账户当前等级）\n类型：%s\n%s%s：%s\n逐级效果（配置）：\n%s解锁条件：%s"),
		*Type, *RageLine, EffectLabel, *Message,
		LevelRows.IsEmpty() ? TEXT("无逐级配置\n") : *LevelRows,
		*Unlock);
	return true;
}
}
