#pragma once

#include "CoreMinimal.h"

// Explicit synthetic interaction fixture. No account mail, service request, or reward payload.
struct FRokMailLocalUnreadRow
{
	int32 Id = INDEX_NONE;
	int32 Tab = INDEX_NONE;
	FString Title;
	FString Summary;
	bool bRead = false;
};

struct FRokMailLocalUnreadState
{
	TArray<FRokMailLocalUnreadRow> Rows;

	void SeedOnce()
	{
		if (!Rows.IsEmpty()) return;
		Rows = {
			{1001, 0, TEXT("系统通知示例 A"), TEXT("本地未读状态演示"), false},
			{1002, 0, TEXT("系统通知示例 B"), TEXT("本地已读状态演示"), true},
			{2001, 1, TEXT("报告示例"), TEXT("本地未读状态演示"), false},
			{3001, 2, TEXT("联盟示例"), TEXT("本地未读状态演示"), false},
			{4001, 3, TEXT("个人邮件示例"), TEXT("本地未读状态演示"), false},
			{6001, 5, TEXT("收藏示例"), TEXT("本地未读状态演示"), false}
		};
	}

	const FRokMailLocalUnreadRow* Find(int32 Index) const { return Rows.IsValidIndex(Index) ? &Rows[Index] : nullptr; }
	int32 FirstIndexForTab(int32 Tab) const
	{
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			if (Rows[Index].Tab == Tab) return Index;
		}
		return INDEX_NONE;
	}
	int32 SelectFirstAndMarkReadForTab(int32 Tab)
	{
		const int32 Index = FirstIndexForTab(Tab);
		if (Index != INDEX_NONE) MarkRead(Index, Tab);
		return Index;
	}
	int32 UnreadCount(int32 Tab) const
	{
		int32 Count = 0;
		for (const FRokMailLocalUnreadRow& Row : Rows)
		{
			if (Row.Tab == Tab && !Row.bRead) ++Count;
		}
		return Count;
	}
	bool MarkRead(int32 Index, int32 Tab)
	{
		if (!Rows.IsValidIndex(Index) || Rows[Index].Tab != Tab || Rows[Index].bRead) return false;
		Rows[Index].bRead = true;
		return true;
	}
};
