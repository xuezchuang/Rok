#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// What the Unity main interface shows in its top bar for a server RoleInfo:
// UI_Item_PlayerPowerInfo_SubView (power, VIP, head), UI_Item_PlayerResources_SubView
// (resource values and which cells are shown), MainInterfaceMediator.OnAgeChange (age)
// and OnWorldViewChange (coordinates), and the city nameplate (name, town hall level).
struct FRokPlayerTopBar
{
	FString Name;
	int32 Level = 0; // RoleInfo.level, the town hall level
	int64 Power = 0;
	int64 Food = 0;
	int64 Wood = 0;
	int64 Stone = 0;
	int64 Gold = 0;
	int64 Denar = 0;
	// Config foodShowLev/woodShowLev/stoneShowLev/goldShowLev against RoleInfo.level;
	// gems follow denarShowStep, which is 0 (always shown) in the shipped table.
	bool bShowFood = false;
	bool bShowWood = false;
	bool bShowStone = false;
	bool bShowGold = false;
	bool bShowGem = false;
	int64 VipPoints = 0;
	int32 VipNameId = 0;
	FString VipText;
	// RefreshVIPRedDot: !vipFreeBox || !vipExpFlag (daily box or daily points unclaimed).
	bool bVipRedPoint = false;
	int32 HeadId = 0;
	int32 HeadFrameId = 0;
	// Tile coordinates of the city: server units / 100 -> Unity world, floor, / 6.
	int32 PosX = 0;
	int32 PosY = 0;
	int32 AgeNameId = 0;
	FString AgeText;
};

namespace RokPlayerTopBar
{
	// Fills Out from Role_RoleInfo.roleInfo. Tables come from Content/RokData/ConfigTables.
	ROK_API bool FromRoleInfo(const TSharedPtr<FJsonObject>& RoleInfo, FRokPlayerTopBar& Out, FString& OutError);

	// PlayerProxy.GetCurVipInfo: the first Vip row whose point exceeds the role's points,
	// else the last row. Returns its l_nameID (0 when the table is missing).
	ROK_API int32 FindVipNameId(int64 VipPoints);

	// OnAgeChange: l_nameId of the last CityAgeSize row with townLevel <= Level.
	ROK_API int32 FindAgeNameId(int32 Level);

	// ClientUtils.FormatComma ("N0"): 7357 -> "7,357".
	ROK_API FString FormatComma(int64 Value);

	// MainInterface position box. The server number comes from PlayerProxy.m_gameNode,
	// which Unity initialises to 1 and never sets.
	ROK_API FString FormatCoordinate(int32 X, int32 Y);
}
