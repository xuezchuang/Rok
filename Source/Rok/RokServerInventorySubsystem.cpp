#include "RokServerInventorySubsystem.h"

#include "RokGameNetSubsystem.h"
#include "RokSproto.h"
#include "RokUnityEconomyCatalogSubsystem.h"
#include "Misc/Crc.h"

namespace
{
bool ReadInt(const TSharedPtr<FJsonObject>& Row, const TCHAR* Name, int64& Value)
{
	return Row.IsValid() && RokSprotoJson::TryGetInt64(Row, Name, Value);
}

bool AppendStack(const TSharedPtr<FJsonObject>& Row, const FString& MapKey,
	const URokUnityEconomyCatalogSubsystem* Catalog, TArray<FRokServerInventoryStack>& Out)
{
	int64 Index = 0, Id = 0, Overlay = 0;
	if (!ReadInt(Row, TEXT("overlay"), Overlay)) return false;
	// BagProxy removes these rows from its visible bags even when the push omits itemId.
	if (Overlay <= 0) return true;
	if (!ReadInt(Row, TEXT("itemIndex"), Index)) LexTryParseString(Index, *MapKey);
	if (Index <= 0 || !ReadInt(Row, TEXT("itemId"), Id) || Id <= 0) return false;

	FRokServerInventoryStack& Stack = Out.AddDefaulted_GetRef();
	Stack.ItemIndex = Index;
	Stack.ItemId = Id;
	Stack.Overlay = Overlay;
	ReadInt(Row, TEXT("heroId"), Stack.HeroId);
	FRokUnityEconomyItem Item;
	if (Catalog && Id <= MAX_int32 && Catalog->FindItemById(int32(Id), Item))
	{
		Stack.bCatalogKnown = true;
		Stack.Name = Item.DisplayNameZhCn;
		Stack.Icon = Item.Icon;
		Stack.Quality = Item.Quality;
		Stack.SubType = Item.SubType;
		Stack.BagType = Item.Type;
		Stack.TypeGroup = Item.TypeGroup;
		Stack.Rank = Item.Rank;
	}
	return true;
}
}

FRokServerInventorySnapshot URokServerInventorySubsystem::GetSnapshot() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestSnapshot.IsSet()) return TestSnapshot.GetValue();
#endif
	FRokServerInventorySnapshot Empty;
	const UGameInstance* Instance = GetGameInstance();
	const URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net) return Empty;
	const ERokGameNetState State = Net->GetState();
	if (State != ERokGameNetState::LoggingIn && State != ERokGameNetState::Connected
		&& State != ERokGameNetState::RoleReady) return Empty;
	FRokServerInventorySnapshot Result = ProjectSnapshot(
		State == ERokGameNetState::RoleReady
			? Net->GetCollection(TEXT("Item_ItemInfo"), TEXT("itemInfo")) : nullptr,
		Instance->GetSubsystem<URokUnityEconomyCatalogSubsystem>());
	Result.bAvailable = true;
	return Result;
}

FRokServerInventorySnapshot URokServerInventorySubsystem::ProjectSnapshot(
	const TSharedPtr<FJsonObject>& ItemInfo,
	const URokUnityEconomyCatalogSubsystem* Catalog)
{
	FRokServerInventorySnapshot Result;
	Result.bAvailable = true;
	Result.bLoaded = Catalog && Catalog->IsCatalogLoaded();
	Result.bItemsKnown = ItemInfo.IsValid();
	Result.bRowsComplete = Result.bItemsKnown;
	if (ItemInfo.IsValid())
	{
		for (const auto& Pair : ItemInfo->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if (Pair.Value.IsValid() && Pair.Value->TryGetObject(Row) && Row)
			{
				const bool bRowComplete = AppendStack(*Row, FString(*Pair.Key), Catalog, Result.Stacks);
				Result.bRowsComplete &= bRowComplete;
			}
			else Result.bRowsComplete = false;
		}
	}
	Result.Stacks.Sort([](const FRokServerInventoryStack& A, const FRokServerInventoryStack& B)
	{
		if (A.ItemIndex != B.ItemIndex) return A.ItemIndex < B.ItemIndex;
		return A.ItemId < B.ItemId;
	});
	FString Source = FString::Printf(TEXT("%d,%d,%d,%d"), Result.bAvailable, Result.bLoaded,
		Result.bItemsKnown, Result.bRowsComplete);
	for (const FRokServerInventoryStack& Stack : Result.Stacks)
	{
		Source += FString::Printf(TEXT("|%lld,%lld,%lld,%lld,%d,%d,%d,%d,%d,%d;%s;%s"),
			Stack.ItemIndex, Stack.ItemId, Stack.Overlay, Stack.HeroId,
			Stack.bCatalogKnown, Stack.Quality, Stack.SubType, Stack.BagType,
			Stack.TypeGroup, Stack.Rank, *Stack.Name, *Stack.Icon);
	}
	Result.Signature = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*Source));
	return Result;
}
