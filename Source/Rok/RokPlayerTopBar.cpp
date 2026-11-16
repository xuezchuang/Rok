#include "RokPlayerTopBar.h"

#include "Misc/FileHelper.h"
#include "RokCsv.h"
#include "RokDataPaths.h"
#include "RokSproto.h"
#include "RokUiTextTable.h"

namespace
{
struct FTopBarTables
{
	bool bLoaded = false;
	FString Error;
	TArray<TPair<int64, int32>> VipPointAndName; // Vip.csv point, l_nameID in table order
	TArray<TPair<int32, int32>> AgeLevelAndName; // CityAgeSize.csv townLevel, l_nameId
	int32 FoodShowLevel = 1;
	int32 WoodShowLevel = 1;
	int32 StoneShowLevel = 4;
	int32 GoldShowLevel = 10;
	int32 DenarShowStep = 0;
};

bool LoadTable(const TCHAR* Name, TArray<TArray<FString>>& OutRecords, FString& OutError)
{
	FString Text;
	const FString Path = RokDataPaths::Resolve(FString::Printf(TEXT("ConfigTables/%s"), Name));
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("missing %s"), *Path);
		return false;
	}
	FRokCsv::ParseRecords(Text, OutRecords);
	if (OutRecords.Num() < 2)
	{
		OutError = FString::Printf(TEXT("%s has no rows"), Name);
		return false;
	}
	return true;
}

const FTopBarTables& GetTables()
{
	static FTopBarTables Tables;
	if (Tables.bLoaded)
	{
		return Tables;
	}
	Tables.bLoaded = true;

	TArray<TArray<FString>> Rows;
	if (LoadTable(TEXT("Vip.csv"), Rows, Tables.Error))
	{
		const int32 PointColumn = FRokCsv::FindColumn(Rows[0], TEXT("point"));
		const int32 NameColumn = FRokCsv::FindColumn(Rows[0], TEXT("l_nameID"));
		for (int32 Index = 1; PointColumn != INDEX_NONE && NameColumn != INDEX_NONE && Index < Rows.Num(); ++Index)
		{
			if (Rows[Index].IsValidIndex(FMath::Max(PointColumn, NameColumn)) && !Rows[Index][PointColumn].IsEmpty())
			{
				Tables.VipPointAndName.Emplace(FCString::Atoi64(*Rows[Index][PointColumn]), FCString::Atoi(*Rows[Index][NameColumn]));
			}
		}
	}

	Rows.Reset();
	if (LoadTable(TEXT("CityAgeSize.csv"), Rows, Tables.Error))
	{
		const int32 LevelColumn = FRokCsv::FindColumn(Rows[0], TEXT("townLevel"));
		const int32 NameColumn = FRokCsv::FindColumn(Rows[0], TEXT("l_nameId"));
		for (int32 Index = 1; LevelColumn != INDEX_NONE && NameColumn != INDEX_NONE && Index < Rows.Num(); ++Index)
		{
			if (Rows[Index].IsValidIndex(FMath::Max(LevelColumn, NameColumn)) && !Rows[Index][LevelColumn].IsEmpty())
			{
				Tables.AgeLevelAndName.Emplace(FCString::Atoi(*Rows[Index][LevelColumn]), FCString::Atoi(*Rows[Index][NameColumn]));
			}
		}
	}

	// Config.csv is a key/value sheet: key, type, -, group, value, description.
	Rows.Reset();
	if (LoadTable(TEXT("Config.csv"), Rows, Tables.Error))
	{
		for (const TArray<FString>& Row : Rows)
		{
			if (Row.Num() < 5)
			{
				continue;
			}
			const FString& Key = Row[0];
			const int32 Value = FCString::Atoi(*Row[4]);
			if (Key == TEXT("foodShowLev")) Tables.FoodShowLevel = Value;
			else if (Key == TEXT("woodShowLev")) Tables.WoodShowLevel = Value;
			else if (Key == TEXT("stoneShowLev")) Tables.StoneShowLevel = Value;
			else if (Key == TEXT("goldShowLev")) Tables.GoldShowLevel = Value;
			else if (Key == TEXT("denarShowStep")) Tables.DenarShowStep = Value;
		}
	}
	if (Tables.VipPointAndName.IsEmpty() || Tables.AgeLevelAndName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok top bar: config tables incomplete (%s)."), *Tables.Error);
	}
	return Tables;
}

int64 GetInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	int64 Value = 0;
	RokSprotoJson::TryGetInt64(Object, Field, Value);
	return Value;
}

// PosHelper.ServerUnitToClientPos (/100), then OnWorldViewChange: FloorToInt(x) / 6.
int32 ServerUnitToTile(int64 ServerUnit)
{
	const int32 World = FMath::FloorToInt32(static_cast<double>(ServerUnit) / 100.0);
	return World / 6;
}
}

namespace RokPlayerTopBar
{
int32 FindVipNameId(int64 VipPoints)
{
	const FTopBarTables& Tables = GetTables();
	for (const TPair<int64, int32>& Row : Tables.VipPointAndName)
	{
		if (Row.Key > VipPoints)
		{
			return Row.Value;
		}
	}
	return Tables.VipPointAndName.IsEmpty() ? 0 : Tables.VipPointAndName.Last().Value;
}

int32 FindAgeNameId(int32 Level)
{
	int32 NameId = 0;
	for (const TPair<int32, int32>& Row : GetTables().AgeLevelAndName)
	{
		if (Level >= Row.Key)
		{
			NameId = Row.Value;
		}
	}
	return NameId;
}

FString FormatComma(int64 Value)
{
	const bool bNegative = Value < 0;
	FString Digits = FString::Printf(TEXT("%llu"), bNegative ? 0ull - static_cast<uint64>(Value) : static_cast<uint64>(Value));
	for (int32 Insert = Digits.Len() - 3; Insert > 0; Insert -= 3)
	{
		Digits.InsertAt(Insert, TEXT(','));
	}
	return bNegative ? TEXT("-") + Digits : Digits;
}

FString FormatCoordinate(int32 X, int32 Y)
{
	return FString::Printf(TEXT("#1    X:%d Y:%d"), X, Y);
}

bool FromRoleInfo(const TSharedPtr<FJsonObject>& RoleInfo, FRokPlayerTopBar& Out, FString& OutError)
{
	Out = FRokPlayerTopBar();
	if (!RoleInfo.IsValid() || !RoleInfo->HasField(TEXT("level")))
	{
		OutError = TEXT("roleInfo has no level");
		return false;
	}
	const FTopBarTables& Tables = GetTables();

	RoleInfo->TryGetStringField(TEXT("name"), Out.Name);
	Out.Level = static_cast<int32>(GetInt(RoleInfo, TEXT("level")));
	Out.Power = GetInt(RoleInfo, TEXT("combatPower"));
	Out.Food = GetInt(RoleInfo, TEXT("food"));
	Out.Wood = GetInt(RoleInfo, TEXT("wood"));
	Out.Stone = GetInt(RoleInfo, TEXT("stone"));
	Out.Gold = GetInt(RoleInfo, TEXT("gold"));
	Out.Denar = GetInt(RoleInfo, TEXT("denar"));
	Out.bShowFood = Out.Level >= Tables.FoodShowLevel;
	Out.bShowWood = Out.Level >= Tables.WoodShowLevel;
	Out.bShowStone = Out.Level >= Tables.StoneShowLevel;
	Out.bShowGold = Out.Level >= Tables.GoldShowLevel;
	// The shipped denarShowStep is 0, which Unity always shows. A non-zero step needs
	// GuideProxy.IsCompletedByStage, which is not ported, so the gem cell stays hidden.
	Out.bShowGem = Tables.DenarShowStep == 0;

	Out.VipPoints = GetInt(RoleInfo, TEXT("vip"));
	Out.VipNameId = FindVipNameId(Out.VipPoints);
	Out.VipText = Out.VipNameId > 0 ? FRokUiTextTable::Resolve(Out.VipNameId) : FString();
	bool bVipFreeBox = false;
	bool bVipExpFlag = false;
	RoleInfo->TryGetBoolField(TEXT("vipFreeBox"), bVipFreeBox);
	RoleInfo->TryGetBoolField(TEXT("vipExpFlag"), bVipExpFlag);
	Out.bVipRedPoint = !bVipFreeBox || !bVipExpFlag;
	Out.HeadId = static_cast<int32>(GetInt(RoleInfo, TEXT("headId")));
	Out.HeadFrameId = static_cast<int32>(GetInt(RoleInfo, TEXT("headFrameID")));

	const TSharedPtr<FJsonObject>* Pos = nullptr;
	if (RoleInfo->TryGetObjectField(TEXT("pos"), Pos) && Pos)
	{
		Out.PosX = ServerUnitToTile(GetInt(*Pos, TEXT("x")));
		Out.PosY = ServerUnitToTile(GetInt(*Pos, TEXT("y")));
	}
	Out.AgeNameId = FindAgeNameId(Out.Level);
	Out.AgeText = Out.AgeNameId > 0 ? FRokUiTextTable::Resolve(Out.AgeNameId) : FString();
	return true;
}
}
