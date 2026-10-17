#include "RokAllianceTerritoryReadonlyData.h"

namespace RokAllianceTerritoryReadonly
{
const TArray<FGroup>& GetGroups()
{
    static const TArray<FGroup> Values = {
        {0, 732068, 732002},
        {1, 732069, 732092},
        {2, 732003, 732004},
        {3, 732070, 732037},
    };
    return Values;
}

const TArray<FRow>& GetRows()
{
    static const TArray<FRow> Values = {
        {1, 0, 500112, 732050, 732043, TEXT(""), false},
        {2, 0, 732000, 732051, 732042, TEXT(""), false},
        {3, 2, 732003, 732044, 732045, TEXT(""), false},
        {4, 3, 732030, 732055, 0, TEXT("ui_common[img_com_cur1]"), false},
        {5, 3, 732031, 732055, 0, TEXT("ui_common[img_com_cur2]"), false},
        {6, 3, 732032, 732055, 0, TEXT("ui_common[img_com_cur3]"), false},
        {7, 3, 732033, 732055, 0, TEXT("ui_common[img_co]m_cur4"), true},
        {8, 1, 732005, 732040, 732054, TEXT("ui_common[img_com_cur1]"), false},
        {9, 1, 732006, 732040, 732054, TEXT("ui_common[img_com_cur2]"), false},
        {10, 1, 732007, 732040, 732054, TEXT("ui_common[img_com_cur3]"), false},
        {11, 1, 732008, 732040, 732054, TEXT("ui_common[img_co]m_cur4"), true},
        {12, 0, 732000, 732051, 732042, TEXT(""), false},
    };
    return Values;
}

FString GetChineseSourceText(int32 LocalizationId)
{
    switch (LocalizationId)
    {
    case 500112: return FString(TEXT("联盟主要塞"));
    case 732000: return FString(TEXT("联盟副要塞"));
    case 732002: return FString(TEXT("要塞是联盟的核心，建造它能够为联盟带来巨大的好处。"));
    case 732003: return FString(TEXT("联盟旗帜"));
    case 732004: return FString(TEXT("联盟旗帜可以用来扩张领土，帮助我们将世界收入囊中。"));
    case 732005: return FString(TEXT("联盟麦场"));
    case 732006: return FString(TEXT("联盟木料场"));
    case 732007: return FString(TEXT("联盟铁矿场"));
    case 732008: return FString(TEXT("联盟铸币场"));
    case 732030: return FString(TEXT("联盟农田"));
    case 732031: return FString(TEXT("联盟伐木场"));
    case 732032: return FString(TEXT("联盟铁矿床"));
    case 732033: return FString(TEXT("联盟金矿床"));
    case 732037: return FString(TEXT("领土上的联盟资源点能够源源不断地为我们提供资源，给联盟发展和征战提供坚实的后盾。"));
    case 732040: return FString(TEXT("在联盟资源场里进行采集可以快速获得资源，且部队永远不会遭受敌人的攻击。"));
    case 732042: return FString(TEXT("1. 联盟副要塞拥有和联盟主要塞相似的作用，都能够作为联盟领土的起点。\n2. 联盟副要塞也只能修建在无归属的空地上。\n3. 只有成员等级达到4级或以上的成员才能进行建造。\n4. 联盟领土需要与主要塞或副要塞连通才能生效。\n5. 在联盟要塞的战斗中，防御方重伤的部队有一半会直接死亡，而进攻方重伤的部队则会全部直接死亡。"));
    case 732043: return FString(TEXT("1. 修建联盟主要塞能够让联盟获得领土。\n2. 联盟主要塞只能修建在无归属的空地上。\n3. 只有成员等级达到4级或以上的成员才能进行建造。\n4. 联盟领土需要与主要塞或副要塞连通才能生效。"));
    case 732044: return FString(TEXT("联盟旗帜可以用来扩张联盟领土。但是旗帜建造得越多，之后每次旗帜建造耗费的联盟资源也就越多。"));
    case 732045: return FString(TEXT("1. 建造联盟旗帜来扩张联盟领土。\n2. 联盟旗帜必须建造在与现有联盟领土接壤的地方。\n3. 只有成员等级达到4级或以上的成员才能进行建造。\n4. 联盟领土需要与联盟要塞连通才能生效。\n5. 在联盟旗帜的战斗中，防御方重伤的部队有一半会直接死亡，而进攻方重伤的部队则会全部直接死亡。"));
    case 732050: return FString(TEXT("联盟主要塞是所有联盟建筑的基础，也是联盟领土的起点。所有联盟领土都必须与主要塞或副要塞连通才能生效。"));
    case 732051: return FString(TEXT("联盟要塞作领土建筑，也能够修建在其他王国。（失落之地开放后，可修建在失落之地中）"));
    case 732054: return FString(TEXT("联盟资源场拥有海量的资源储备，但需要联盟成员齐心协力共同建造，一旦建造成功，便会带来巨大的收益。同时，它也是一个绝对安全的采集点，敌方部队不能以任何形式攻击正在联盟资源场里的部队。\n1.联盟资源场只能建造在联盟领土上。\n2.所有联盟成员都可参与建造和采集。\n3.一个资源场只允许每位城主派遣一支部队进入采集。\n4.当资源场被采集完后，所有采集部队都会自动返回城市。\n5.资源场在被建造和采集时都无法被攻击。\n6.如果3天内不建造完成，资源场就会消失。\n7.建造好的资源场无论是否被采集完，3天后都会消失。"));
    case 732055: return FString(TEXT("1. 将联盟领土扩张至联盟资源点后就能够占领它。\n2. 联盟资源点能够源源不断地为联盟生产资源。\n3. 联盟资源点将以50%的生产速度为所有成员提供资源，城主能够随时进入联盟领土页面领取。\n4. 联盟资源点的资源产量能够叠加。"));
    case 732068: return FString(TEXT("领土建筑"));
    case 732069: return FString(TEXT("联盟资源场"));
    case 732070: return FString(TEXT("联盟资源点"));
    case 732092: return FString(TEXT("绝对安全的资源采集处，战争中的避风港。"));
    default: return FString();
    }
}

const FRow* FindRow(int32 UnityType)
{
    for (const FRow& Row : GetRows())
    {
        if (Row.UnityType == UnityType) return &Row;
    }
    return nullptr;
}
}
