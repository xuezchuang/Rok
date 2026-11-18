# Rok Unity → UE 迁移盘点（2026-11-16）

基于 git `9a67569` 和当天同步的 P4 `//depot/Rok`。全部离线完成：没有启动 UE 或 Unity，也没有连服务器。

- 生成脚本：`Scripts/BuildRokMigrationInventory.py`。运行命令：`py -3.12 -B -X utf8 Scripts/BuildRokMigrationInventory.py`，约 8 秒。
- 生成数据：`Saved/RokUnityMigration/MigrationInventory/`，包括 `inventory.json` 和 `screens / modules / systems / protocol / config_tables / ue_surfaces.csv`。这些是生成物，要改就重新生成，不要手改。本文的数字都来自这次运行。

## 怎么判定的（先看这里）

| 维度 | Unity 一侧的来源 | 怎样算 UE “已覆盖” |
|---|---|---|
| 界面 | `MVC/UI.cs` 注册的 200 个 `UIInfo`。每个界面由 View 的 `VIEW_NAME`（即 prefab 名）和对应的 Mediator 组成 | **traced**：UE 产品代码里写出了该界面的 prefab、View 或 Mediator 名。**name_match**：只是 UE 的某个功能页、弹窗或 Widget 和它名字相同，或者在脚本的别名表里，代码里没有引用 |
| 协议 | `Content/RokData/Protocol/Protocol.sproto` 里的 237 条消息，和服务端、客户端两份拷贝的消息名和 id 一致 | **wired**：UE 产品代码里以字符串形式用了这个消息名，例如 `TEXT("Build_BuildingInfo")` |
| 配置表 | `Hotfix/Config` 里的 210 个 `XxxDefine` | **source**：C++ 直接读这张表。**scripts_pipeline**：由 `Scripts/` 生成到 `Content/RokData` 后再被读 |
| 服务端 | `ref/server/.../proxy/<Module>.lua` 里的 `response.X` 是客户端请求；服务端 Lua 里用引号写出的消息名算推送 | — |

**几点局限：**
- traced 只说明“有对应关系”，**不等于界面一致**。是否一致仍要按 CLAUDE.md 的验收规则，和 Unity 截图并排对比。
- `*AuditSubsystem` 和 `Tests/` 不算产品代码，单独计。
- UE 常把几个 Unity 界面合并进一个功能页。例如召唤统帅在 Commander 页里完成，而 `UI_IF_CaptainSummon` 显示为 none。所以界面列要和协议列一起看。
- 别名表 `SURFACE_ALIASES` 是按名字推断的，脚本头部写了每条的依据。ActivityCenter、Expedition、GemShop、Help、ArmyStatistics 这五个 UE 功能页还没找到确定对应的 Unity 主界面，没有映射。

## 总览

| 项 | 数量 | UE 已覆盖 | 比例 |
|---|---|---|---|
| Unity 界面（UIInfo，不含 7 个开发工具界面） | 193 | traced 50 + name_match 15 | 34%（其中 26% 有代码引用） |
| 协议消息 | 237（服务端处理的请求 183，推送 47） | wired 47，另有 7 条只在注释里提到 | 20% |
| 配置表 | 210（RokData 里有 209） | 103（C++ 直接读 69，经脚本 34） | 49% |
| Unity Hotfix 代码 | 1946 个文件，约 32 万行非空行（含生成的协议和配置代码） | — | — |

**结论：** 城建主循环和单人养成已经接上服务器，包括建造/升级、科研、训练、医院、统帅、任务、邮件读取、行军/搜索、斥候。**联盟、集结/战争、装备/材料、活动/商店/充值、聊天发送、道具使用这几块基本是空的。** Unity 代码在用、UE 还没接的协议共 181 条，其中约 103 条（57%）属于这几块。

## 按功能模块

`View_Mediator/<模块>`，按 Unity 代码量从大到小，只列主要的。每格的写法：
- 界面：traced / name_match / 总数；
- 协议：wired / 服务端有支持的 / 总数；
- 配置表：已用 / 总数。

| 模块 | Unity 行数 | 界面 | 协议 | 配置表 | 判断 |
|---|---|---|---|---|---|
| Alliance（联盟） | 19076 | 3/2/37 | **0/39/39** | 20/25 | 最大缺口。只有礼物、权限、商店等只读窗口 |
| Mail（邮件） | 6643 | 1/0/9 | 2/8/8 | 8/9 | 能读，邮件页里也能看战报邮件（`RokBattleReportScreen.cpp`）；写信、回复、删除、收藏、领附件没有，战报详情窗口 `UI_Win_BattleLog` 和部队详情窗口没有对应 |
| Expedition（远征） | 4620 | 2/0/11 | 0/7/7 | 11/13 | 只有商店和任务预览，战斗流程没有 |
| BuildingDesc（建筑详情/升级） | 4139 | 1/1/3 | 1/1/1 | 17/22 | 基本有 |
| MainInterface（主界面 HUD） | 3921 | 1/1/3 | 5/9/9 | 10/12 | 基本有 |
| Equip（装备/材料） | 3834 | 0/0/5 | 0/7/7 | 5/9 | 完全没有 |
| Rss（世界物件/采集） | 3722 | 3/0/7 | 3/3/3 | 7/8 | 主体有，符文和村庄山洞没有 |
| Captain（统帅） | 3498 | 1/0/8 | 2/4/4 | 10/10 | 主页有，升星、升级、装备、天赋改名等子窗口要补 |
| Activity（活动） | 3082 | 1/0/5 | 0/5/5 | 5/12 | 只有活动日历这个只读窗口 |
| Chat（聊天） | 2774 | 1/0/2 | 0/1/1 | 1/5 | 有界面，收发没接 |
| UIWar（战争/增援） | 2529 | 0/0/4 | 0/4/4 | 4/5 | 完全没有 |
| Guide（新手引导） | 2371 | 0/0/1 | 1/2/2 | 3/4 | 没有（FuncGuide 只做了数据窗口） |
| Login（登录/创角） | 2045 | 4/0/7 | 3/6/6 | 7/9 | `-RokNet` 自动登录可用，选服、创角、断线重连界面没有 |
| Tavern（酒馆） | 1915 | 2/0/3 | 0/1/1 | 5/7 | 概率和奖池窗口有，召唤演出（`UI_IF_TavernSummon`）没有 |
| Bag / UseItem / QuickUseResources | 5307 | 2/1/13 | **0/7/7** | 7/8 | 道具和资源能看，**使用、兑换、资源不足补足都没有** |
| Store / UICharge / VIP | 3343 | 4/2/9 | 2/10/10 | 11/11 | 只读展示较多，购买都没接 |
| Monument / MapMarker / UIMoveCity | 3021 | 0/1/5 | 0/6/6 | 3/6 | 纪念碑、收藏夹、迁城都没有 |
| Setting / UIAccount | 1753 | 1/4/8 | 0/1/1 | 2/2 | 设置页多数只是名字对上，账号流程没有 |

完整的 81 行在 `modules.csv`，逐个界面的结果在 `screens.csv`。

## 协议：按模块还没接的消息

格式为 wired / 总数，后面列出 Unity 代码在用但 UE 没接的消息，带 `*` 的是服务端推送。

| 模块 | wired | 没接的（节选） |
|---|---|---|
| Guild | 0/54 | 全部：建盟、申请、搜索、成员管理、帮助、联盟建筑、领土、科技捐献、留言板、礼物、商店，以及 `GuildInfo*` `GuildNotify*` `GuildMemberInfo*` 等 12 条推送 |
| Role | 13/46 | `TrainEnd` `DisbandArmy` `BuyResource` `ModifyName` `SetRoleHead` `ChangeCivilization` `BuyActionForce` `GetFreeDaily` `NoviceGuideStep(Ex)` `GetCityReinforce*`，以及 `RoleNotify*` `EarlyWarningInfo*` `MysteryStore*` |
| Map | 6/26 | `Scouts` `ScoutVillage` `ScoutsBack` `MoveCity` `AddMarker`/`ModifyMarker`/`DeleteMarker` `GetCityDetail`，以及迷雾、领土、圣地相关推送 |
| Build | 5/20 | `EndBuilding` `RemoveBuilding` `DismantleBuilding` `Tavern` `Extinguishing` `Service` `DefendHero`，以及材料和装备链：`ProduceMaterial` `MaterialSynthesis` `MakeEquipment` 等 |
| Activity | 0/13 | 领奖、兑换、排行、转盘，以及 `ScheduleInfo*` `Rank*` |
| Chat | 0/8 | `SendMsg` `SendPrivateMsg`、私聊查询，以及 `PushMsg*` `MarqueeNotify*` |
| Rally | 0/7 | 发起、加入、遣返、解散集结，增援，以及 `RallyBattleInfo*` |
| Shop / Recharge | 0/11 | 商店、VIP 商店、远征商店购买和刷新；成长基金和充值领取 |
| Hero | 7/13 | `AddHeroExp` `HeroStarUp` `ExchangeHeroItem` `HeroWearEquip` `TakeOffEquip` |
| Email | 4/9 | `TakeEnclosure` `DeleteEmail` `CollectEmail` `MsgSendPrivateEmail` `SendGuildEmail` |
| Expedition | 0/5 | `ExpeditionChallenge` `OneKeyAward` `March` `Exit`，以及 `BattleInfo*` |
| Transport / Item / Rank / Monument | 2/12 | 资源援助、`ItemUse` `ItemChangeResource`、排行查询、纪念碑推送 |

另外有 7 条消息在服务端 Lua 里找不到名字：`Role_TechnologyLevelUp` `Map_Fog` `Chat_SendMsg` `Chat_GetSaveChatMsg` `Guild_GetGuildApplys` `Guild_GetGuildHolyLands` `Battle_BattleDamageInfo`。它们可能走聊天服或战斗服，或者名字是拼出来的，接之前要先查服务端。

## 数据层和全局系统

- **Proxy（数据层）：** Unity 共 44 个，UE 代码提到 17 个。几个大的 Proxy UE 没有对应：ChatProxy（3439 行）、ActivityProxy（1923）、RallyTroopsProxy（1509）、EmailProxy、PlayerAttributeProxy、WorldMapObjectProxy、MapMarkerProxy、RechargeProxy、StoreProxy。AllianceProxy 在 UE 里被提到，但它的 30 条消息一条都没接。
- **GlobalMediator（全局逻辑）：** Unity 共 38 个，UE 提到 7 个，包括 CityGlobalMediator、PVPGlobalMediator、GlobalViewLevelMediator、WorldMgrMediator、TaskScriptGlobalMediator 等。没有对应的有：TroopHUDMediator（2154 行，部队头顶 HUD）、GlobalFilmMediator（剧情演出）、GlobalCityMenuHudMediator、ExpeditionMediator、FogSystemMediator（迷雾）、GlobalAllianceHelpMediator、ServerPushMediator（17 条推送，UE 接了 2 条）。
- **配置表：** 105 张 UE 完全没用。被 Unity 用得多的有 `MapMarkerType` `MapItemType` `Equip` `HeroTalentType` `SkillStatus` `ActivityDaysType` `ActivityKillType` `AllianceLanguageSet` `AttrInfo` `ChatEmoji` `ActivityInfernal` `AllianceMember` `BuildingFreight` `ChatChannel` `CityBuffGroup` `SkillBattle` `EquipAtt` `EquipCompose` `GuideEx` `JumpType` 等，完整清单在 `config_tables.csv`。`AllianceSign` 和 `AllianceStudy` 只被 audit 代码读过。

## 建议的迁移顺序

排序依据：玩家多早会碰到，能带动多少后续功能，以及工作量。每项先列不启动 UE 能做的部分，后面写需要 UE 或服务器验证的部分。

1. **道具使用和资源补足**（Bag/UseItem/QuickUseResources/RewardGet；`Item_ItemUse` `Item_ItemChangeResource` `Role_BuyResource`，约 15 个界面）。量小，但加速、资源不足、领奖弹窗都依赖它，打通后很多已有界面才算完整。
   - 离线：协议结构、`URokServerInventorySubsystem` 的写操作和单元测试、从 prefab 导出布局。
   - 要 UE 和服务器：实际使用一次道具，并截图对比。
2. **联盟第一批**（Guild 54 条消息、37 个界面、约 1.9 万行）。这是最大的缺口，也是 RoK 社交的核心。建议分批：
   - 加入/创建/搜索 → 主页和成员 → 帮助 → 科技捐献 → 联盟建筑和领土 → 留言板、礼物、商店。
   - 离线：AllianceProxy 对应的 `URokServerAllianceSubsystem`、13 条推送的解码和合并、只读窗口的布局导出。
   - 要服务器：账号需要有联盟，或者先建一个；**建盟和入盟都是写操作，要你先同意**。
3. **世界地图 PvP**（Rally 7 条、UIWar 4 个界面、斥候 `Map_Scouts*`、增援、迁城、收藏夹）。依赖第 2 项的联盟关系，还有 TroopHUDMediator 和迷雾。
4. **装备和材料**（Equip 5 个界面，`Build_ProduceMaterial` `MaterialSynthesis` `MakeEquipment` 等，`Equip`/`EquipAtt`/`EquipCompose` 表），以及统帅的穿脱装备和升星。
5. **活动、商店、充值**（Activity 13、Shop 7、Recharge 4 条消息）。现在多是只读展示，需要补购买和领奖，**这些都是写操作**。
6. **聊天和邮件的写操作**（`Chat_*` 8 条、`Email_*` 5 条），以及 ServerPushMediator 剩下的 15 条推送。
7. **登录和新手**（选服、创角、断线重连、新手引导 Guide、剧情演出 GlobalFilm）。现在开发用 `-RokNet` 自动登录就够了，可以放到最后。

**不启动 UE 能推进的：**
- 上面每项的协议层：sproto 的结构和编解码、Subsystem 的状态合并、离线单元测试的代码。
- 从 prefab 导出界面布局 JSON。
- 配置表接入。
- 编译检查：用 `Build.bat`，不开编辑器窗口，需要你同意。

**做不了离线验证的：** 运行时行为、截图对比、自动化测试，以及任何要连服务器的验收。

## 和以前记录的出入

- 2026-10-31 的 `UI_GAP_BACKLOG` 写的是“156 个 `UI_Win_*` 里 97 个没有对应”（见 `git show 0f59fce:UnityLearning/UI_GAP_BACKLOG_2026-10-25.md`）。这次的口径是 `UI.cs` 注册的全部 200 个界面，包括 `UI_IF_*` 和 `UI_Pop_*`，所以数字不能直接比。
- CLAUDE.md 的“Status on 2026-11-04”还没写进 `0f59fce`（11-16）新加的内容：服务器军队、建造队列、英雄、背包、任务、时钟这些 Subsystem，以及大约 20 个测试文件。它们在本盘点里体现在 Build、Hero、Task、Technology、Role 几类协议的 wired 数里。
