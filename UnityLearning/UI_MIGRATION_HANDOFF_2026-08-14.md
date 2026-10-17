# Rok Unity → UE UI 迁移交接（2026-08-14）

## 1. 目标与结论

用户目标：**先把所有 UI 迁移好**，以 Unity 项目为可执行参考，在 UE5.8 中还原布局、素材、交互、状态和服务器边界。

当前结论：**任务远未全部完成，不能标记完成。**

- UI 全量目录：1150 个 Unity prefab。
- 已声明运行时映射：44 个界面（43 个唯一 prefab）。
- 仍未实现目录 prefab：1107 个。
- 39 个映射仍含 `preview_shell`。
- 37 个映射仍含 `service_boundary`。
- 22 个映射被静态清单标记为 `real_local_loop`，但这只是源码 token 检查，不代表视觉、交互和服务器闭环全部验收。
- 最近完成的重点切片：资源搜索、邮件空态/入口、城市管理主列表与战争狂热详情。
- 下一个最明确的 P0：**修复任务页的支线任务选择与状态模型**。

覆盖清单：

```text
Saved/RokUnityMigration/unity_ui_runtime_coverage.json
Saved/RokUnityMigration/unity_ui_runtime_coverage_verify.json
```

当前 verifier：`passed=true`，44 mappings、47 entries、1107 unimplemented prefabs。注意 verifier 只证明声明与源码 token 存在，不证明 UI 完成。

## 2. 环境与硬规则

```text
仓库：D:\ueDemo\Rok
工程：D:\ueDemo\Rok\Rok.uproject
引擎：D:\ue\UE_5.8
Unity 实时参考：D:\ueDemo\Rok-unity
仓内 Unity 快照：D:\ueDemo\Rok\ref
```

- 只能使用 UE5.8，不能用 UE5.2 或其他安装保存本项目资产。
- Unity 是行为/视觉参考，UE 中用 C++、UMG、DataTables/DataAssets 重建，不直接搬 MonoBehaviour。
- 服务器权威状态不能伪造成功；未接入时显示真实空态或明确 service boundary。
- fixture/截图参考态必须显式标明，不能称为账号当前状态。
- UI 验收必须至少包含：构建、定向自动化、真实 HUD 入口点击、截图；静态 coverage 不算运行时验收。
- MCP 写操作前必须核对 `unreal.Paths.project_dir()` 与当前 world；本轮没有使用 MCP 写资产。

### 工作树警告

当前工作树非常脏：约 22 个 tracked changed、446 个 untracked。**不要执行 `git reset --hard`、`git clean`、`git checkout --` 或批量删除。** 很多未跟踪文件是现有项目 WIP，不是可以清理的临时文件。

UI 相关当前状态：

```text
 M Source/Rok/RokMainHudWidget.cpp
 M Source/Rok/RokMainHudWidget.h
?? Scripts/BuildRokUnityUiRuntimeCoverage.py
?? Scripts/InvokeRokPhysicalHudClick.ps1
?? Scripts/VerifyRokUnityUiRuntimeCoverage.py
?? Source/Rok/RokUnityFeatureScreenWidget.cpp
?? Source/Rok/RokUnityFeatureScreenWidget.h
?? Source/Rok/RokWorldMapHudWidget.cpp
?? Source/Rok/RokWorldMapHudWidget.h
?? Source/Rok/Tests/RokMainHudInteractionTests.cpp
?? Source/Rok/Tests/RokWorldMapHudTests.cpp
```

## 3. 当前 UI 架构

核心全屏/弹窗界面集中在：

```text
Source/Rok/RokUnityFeatureScreenWidget.h
Source/Rok/RokUnityFeatureScreenWidget.cpp
```

`ERokUnityFeatureScreenMode` 当前包含 Activity、Mail、Tasks、Commander、Items、Alliance、VIP、Chat、地图坐标、书签、资源搜索、远征、宝石商店、建造、工人队列、设置、商店、训练、加速、医院、城市管理、部队一览、充值等模式。

HUD 入口：

```text
主城：Source/Rok/RokMainHudWidget.h/.cpp
世界：Source/Rok/RokWorldMapHudWidget.h/.cpp
主城测试：Source/Rok/Tests/RokMainHudInteractionTests.cpp
世界测试：Source/Rok/Tests/RokWorldMapHudTests.cpp
```

验收工具：

```text
Scripts/BuildRokUnityUiRuntimeCoverage.py
Scripts/VerifyRokUnityUiRuntimeCoverage.py
Scripts/InvokeRokPhysicalHudClick.ps1
```

`InvokeRokPhysicalHudClick.ps1` 已支持可选第二次点击，用于验证“HUD 入口 → 页面内部按钮”的连续物理路线。

## 4. 最近已经落地并有证据的 UI

### 4.1 资源搜索 `resource_search`

Unity 来源：

```text
ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UISearchRes/UI_IF_SearchRes.prefab
```

当前 UE：

- 世界地图底部 5 个类型按钮。
- 选中类型弹出面板、等级滑杆、加减按钮、搜索按钮。
- Barbarian 与资源等级分别保存。
- 全屏透明触摸层负责关闭。
- 搜索提交仍是服务器边界，不移动镜头、不伪造搜索结果。

证据：

```text
Saved/RokUiAcceptance/ResourceSearch_World_PhysicalRoute_20260807_1440x810.png
Saved/Logs/RokResourceSearchWorldPhysicalRoute_20260807.log
Saved/Logs/RokResourceSearchWorldAutomation_20260807.log
```

已知未清项：`Rok.City.Hud.FeatureScreenEntries` 中仍有 4 个 ResourceSearch 断言与当前实现不一致（等级上限、类型切换状态、touch-close），见：

```text
Saved/Logs/RokCityManagerAutomation_20260812.log
```

### 4.2 邮件 `mail`

Unity 来源：

```text
ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/UI_Win_Mail.prefab
```

当前 UE：

- `OpenMail(false)` 是生产空邮箱：6 页签、写信、关闭、右侧空列表、左侧“当前暂无邮件”，不生成假邮件/红点/领取按钮。
- `OpenMail(true)` 是明确标记的截图 fixture，用于有邮件视觉参考。
- 主城和世界 HUD 均有真实入口。
- 邮件服务器状态、收取、删除、回复、领取仍未接入；动作只进入 service boundary，不改变状态。

证据：

```text
Saved/RokUiAcceptance/Mail_World_PhysicalRoute_20260812_1440x810.png
Saved/Logs/RokMailWorldPhysicalRoute_20260812.log
Saved/Logs/RokMailCityAutomation_20260807.log
Saved/Logs/RokMailWorldAutomation_20260807.log
```

### 4.3 城市管理 `city_manager`

Unity 来源：

```text
ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UICityManager/UI_Win_CityManager.prefab
ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UICityManager/UI_Win_CityManagerMediator.cs
ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/CityBuffProxy.cs
```

当前 UE 已重做：

- 不再是错误的 6 张分类卡。
- 主列表为 Unity 的 6 个标题行 + 13 个功能行，共 2180 高滚动内容。
- 顺序：迁城、战争狂热、城市防御、战斗增益、军事策略、资源经济。
- 迁城行只有“迁城”按钮；战争狂热进入专属详情；其余 11 项进入通用增益道具详情。
- 战争狂热详情显示未激活状态与 13 行等级时长。
- 返回键回主列表，关闭键关闭整个窗口。
- 库存、钻石、激活 Buff、迁城条件仍是服务器权威；未伪造购买、使用或迁城成功。

配置运行时清单已扩成 10 表/9885 行：

```text
Saved/RokUnityMigration/ConfigTables/config_tables_manifest.json
BattleBuff, BuildingLevelData, BuildingModelData, BuildingResourcesProduce,
BuildingTypeConfig, CityBuff, CityBuffGroup, CityBuffSeries, InitBuilding, Language
```

证据：

```text
Saved/RokUiAcceptance/CityManager_PhysicalRouteRetry_20260812_1440x810.png
Saved/RokUiAcceptance/CityManager_WarDetail_PhysicalRoute_20260812_1440x810.png
Saved/Logs/RokCityManagerPhysicalRouteRetry_20260812.log
Saved/Logs/RokCityManagerWarDetailPhysicalRoute_20260812.log
Saved/Logs/RokCityManagerSourceStructureRetry_20260812.log
```

运行时 marker：

```text
Rok city HUD click: city_manager.
Rok city manager: page=main series=6 groups=13 authority=config_only.
Rok city manager: open group=201 page=2.
```

2026-08-14 重跑：

```text
Saved/Logs/RokHandoffCityInteractive_20260814.log
Rok.City.Hud.InteractiveControls = Success
```

素材缺口：`CityBuff 60401/60402` 引用 `ui_mainmenu[icon_mmu_604]`，源 atlas 没有该 rect；存在疑似拼写 `icon_mmu__604`，但不能静默替换。

### 4.4 部队一览 `army_show`

Unity 来源：

```text
ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_troops/UIArmyShow/UI_Win_ArmyShow.prefab
```

当前代码已经删除 3 条假队列、汇总数字和“待服务器”占位文案：

- 标题为“部队一览”。
- 保留规则条、规则按钮、空的真实列表 viewport。
- 服务器 troop count=0 时列表为空，和 Unity 一致。
- 规则页可进入/返回。
- 真实部队行仍需服务器 troop 数据。

证据：

```text
Saved/RokUiAcceptance/ArmyShowUnityEmpty_World_1440x810.png
Saved/RokUiAcceptance/ArmyShowRules_World_PhysicalRoute_Retry3_20260807_1440x810.png
Saved/Logs/RokArmyShowWorldPhysicalRouteRetry3_20260807.log
```

覆盖清单的中文 display name 仍写“行军队列”，运行时标题已经是“部队一览”；可后续修正清单文案。

## 5. 当前最重要的错误：任务页支线列表

实现位置：

```text
Source/Rok/RokUnityFeatureScreenWidget.cpp:8145
Source/Rok/RokUnityGameplayCatalogSubsystem.h:554
Source/Rok/RokUnityGameplayCatalogSubsystem.cpp:660
Scripts/BuildRokUnityGameplayCatalog.py
```

当前错误行为：

- `BuildTasks()` 对 `GetSideTaskIds()` 的 2343 条配置按 ID 排序。
- 只按任务名去重，然后取前 8 条。
- 当前实际会显示：

```text
301001 庄园之主
301024 森林征服者
301048 开山裂地
301072 摇钱树
301096 坚不可摧
301120 保持警戒
301143 战场精英
301166 万马奔腾
```

这不是 Unity 的“当前支线任务集合”。

Unity 正确规则：

```text
SortId = group1 * 10000 + order
Finished 全部排在前面
Unfinished 每个 group1 只取第一个满足 showLevelReq/showBuildingReq 的任务
Unfinished 再按 taskSideShowLimit=5 截断
Received 不显示
```

权威源码：

```text
ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/TaskProxy.cs:315-321
ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/TaskProxy.cs:1163-1204
ref/client-unity/Assets/Scripts/Hotfix/Config/TaskSideConfig.cs
```

本地 fresh/bootstrap 预览在没有账号支线状态时，应明确标为 local fixture，候选前 5 条应为：

```text
301001 庄园之主
303001 文明之地
304001 以吾之名
305001 无畏之心
306001 村庄之友
```

推荐修复顺序：

1. `BuildRokUnityGameplayCatalog.py::make_task()` 增加 `group1/order/showLevelReq/showBuildingReq`。
2. `FRokUnityCatalogTask` 与 JSON parser 增加对应字段。
3. 增加明确的支线状态模型：`TaskId/PageType/SourceAuthority/TaskState/Type/P1/P2/Require/Intent`。
4. production 无服务器 side state 时，不要称为账号当前支线；显示空/未同步，或只在显式 local-bootstrap fixture 下显示上述 5 条。
5. `Finished` 才能 claim；`Received` 隐藏；unknown/config-only 只能 go/inspect，且 state 不变。
6. 当前 `TaskAction` 用“row0 是否主线完成”推断 claim，应改成每行显式状态。
7. `task_icon_1011` 当前会错误回退到 `task_icon_1003`，需要补 icon rect。
8. 奖励明细不要继续硬编码；当前 gameplay catalog 只有 reward group ID，需接 ItemPackageShow/经济目录。

主线说明：bootstrap 当前 active task 是 `200900`，只显示它；active chain 后续项只是 lookahead，不能并列当作当前任务。

## 6. 其他未完成范围

- 邮件：没有真实邮箱 subsystem/service/model，生产态只能空邮箱。
- 城市管理：没有 `cityBuff/expiredTime`、背包、货币、服务器时钟与 `Role_AddBuff` 回包。
- 部队一览：没有服务器 troop 行数据。
- 资源搜索：提交不执行服务器查询或镜头定位；广义 FeatureScreen 测试仍有 4 个断言要修。
- 任务：支线筛选和状态权威错误，是下一步 P0。
- 其余 44 映射中大量仍是视觉壳或 service boundary；不要根据 coverage 的 `unimplemented=false` 宣称完成。
- 全目录仍有 1107 个 prefab 未映射实现，必须继续按屏幕/交互闭环分批迁移。
- `Source/Rok/RokUnityFeatureScreenWidget.cpp` 已约 411 KB；不要为了“整洁”做无关大重构，先按 UI 切片完成行为与验收。

## 7. 当前覆盖状态（静态声明，不是完成证明）

符号：`P=preview shell`，`L=real local loop`，`S=service boundary`。

| ID | 中文 | P | L | S |
|---|---|:---:|:---:|:---:|
| city_hud | 主城 HUD |  | ✓ |  |
| world_hud | 世界地图 HUD |  | ✓ |  |
| server_list | 王国列表 | ✓ |  | ✓ |
| player_profile | 玩家资料 | ✓ | ✓ | ✓ |
| player_head_selection | 头像与头像框 | ✓ |  | ✓ |
| player_power_details | 玩家战力详情 | ✓ | ✓ | ✓ |
| player_ranking | 玩家排行榜 | ✓ |  | ✓ |
| player_civilization | 文明选择与预览 | ✓ | ✓ | ✓ |
| player_civilization_flag_item | 文明选择条目 | ✓ | ✓ | ✓ |
| player_civilization_confirmation | 文明更换确认 | ✓ |  | ✓ |
| settings_hub | 设置中心 | ✓ |  |  |
| city_manager | 城市管理 | ✓ |  | ✓ |
| army_show | 部队一览 | ✓ |  | ✓ |
| general_settings | 通用设置 | ✓ | ✓ |  |
| notification_settings | 通知设置 | ✓ |  | ✓ |
| language_settings | 语言设置 | ✓ |  | ✓ |
| account_settings | 账号管理 | ✓ |  | ✓ |
| army_statistics | 部队统计 | ✓ | ✓ | ✓ |
| store | 普通商店 | ✓ | ✓ | ✓ |
| train_army | 训练部队 | ✓ | ✓ | ✓ |
| speed_up | 队列加速 | ✓ | ✓ | ✓ |
| resources | 资源快捷使用 | ✓ |  | ✓ |
| building_info | 建筑信息 |  | ✓ |  |
| building_upgrade | 建筑升级 | ✓ | ✓ | ✓ |
| building_menu | 建筑快捷菜单 |  | ✓ |  |
| build_city | 建造城市 | ✓ |  | ✓ |
| worker_queue | 工人队列 | ✓ | ✓ | ✓ |
| hospital | 医院 | ✓ | ✓ | ✓ |
| activity | 活动中心 | ✓ |  | ✓ |
| mail | 邮件 | ✓ |  | ✓ |
| tasks | 任务 | ✓ | ✓* | ✓ |
| commander | 统帅 | ✓ | ✓ | ✓ |
| items | 道具背包 | ✓ | ✓ | ✓ |
| alliance | 联盟 | ✓ |  | ✓ |
| vip | VIP | ✓ |  | ✓ |
| chat | 聊天 | ✓ |  | ✓ |
| world_position | 世界坐标定位 |  | ✓ |  |
| bookmarks | 地图书签 | ✓ | ✓ | ✓ |
| resource_search | 资源搜索 | ✓ |  | ✓ |
| expedition | 远征 | ✓ |  | ✓ |
| war_warning | 战争预警 | ✓ |  | ✓ |
| charge | 充值中心 | ✓ | ✓ | ✓ |
| buff_list | 增益列表 | ✓ |  | ✓ |
| gem_shop | 宝石商店 | ✓ |  | ✓ |

`tasks` 的 `L` 目前包含错误支线逻辑，修复前不要当成完成。

## 8. 已知测试状态

通过：

```text
UE5.8 RokEditor Win64 Development build（2026-08-14，Result: Succeeded）
Rok.City.Hud.InteractiveControls（2026-08-14，Success）
Rok.City.Ui.CityManager.SourceStructure（2026-08-12，Success）
Rok.WorldMap.Hud.InteractiveControls（资源搜索/邮件阶段通过）
UI runtime coverage verifier（44 mappings / 47 entries / 1107 unimplemented，passed）
```

未全绿：

```text
Rok.City.Hud.FeatureScreenEntries
```

`Saved/Logs/RokCityManagerAutomation_20260812.log` 中有 4 个 ResourceSearch 断言失败。旧日志中的 `PlayerArrowButton` 失败已在后续修改后修复，2026-08-14 的 `Rok.City.Hud.InteractiveControls` 已通过。

## 9. 可直接复制的命令

构建：

```powershell
& 'D:\ue\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  RokEditor Win64 Development 'D:\ueDemo\Rok\Rok.uproject' `
  -WaitMutex -NoHotReload
```

覆盖生成/验证：

```powershell
$python = 'D:\ue\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
& $python Scripts\BuildRokUnityUiRuntimeCoverage.py
& $python Scripts\VerifyRokUnityUiRuntimeCoverage.py
```

定向 Automation 示例：

```powershell
& 'D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\ueDemo\Rok\Rok.uproject' `
  -Unattended -NoSplash -NullRHI -NoSound -NoP4 -NoLiveCoding `
  '-DisablePlugins=McpAutomationBridge,ModelContextProtocol' `
  '-ExecCmds=Automation RunTests Rok.City.Ui.CityManager.SourceStructure; Quit' `
  '-TestExit=Automation Test Queue Empty' `
  '-abslog=D:\ueDemo\Rok\Saved\Logs\RokCityManagerSourceStructure_Handoff.log'
```

城市管理双击路线（主城入口 → 战争狂热）：

```powershell
& .\Scripts\InvokeRokPhysicalHudClick.ps1 `
  -Map '/Game/Maps/L_RokUnityGameAppCity' `
  -ClientX 270 -ClientY 71 `
  -ExpectedLogMarker 'Rok city HUD click: city_manager.' `
  -ReadyLogMarker 'Rok UI: Main HUD added to viewport.' `
  -SecondClientX 700 -SecondClientY 460 `
  -ExpectedSecondLogMarker 'Rok city manager: open group=201 page=2.' `
  -LogPath 'Saved\Logs\RokCityManagerWarDetailPhysicalRoute_Handoff.log' `
  -ScreenshotPath 'Saved\RokUiAcceptance\CityManager_WarDetail_Handoff_1440x810.png' `
  -ScreenshotDelaySeconds 5
```

## 10. 建议接手顺序

1. 先修任务页支线 schema、筛选、状态与奖励/图标，不要继续保留“ID 前 8 条”。
2. 修复并重跑 `Rok.City.Hud.FeatureScreenEntries` 的 ResourceSearch 断言。
3. 每完成一个 UI 切片：Unity prefab/C# 审计 → UE 最小实现 → 构建 → 定向测试 → 真实 HUD 点击 → 截图。
4. 然后按 coverage 中 `preview_shell=true` 且用户可直接进入的高频界面继续：任务、联盟、VIP、聊天、活动、统帅、背包。
5. 服务器数据未接入时保持诚实空态/边界；不要生成假账号邮件、假部队、假任务完成或假 Buff。
6. 只有所有显式 UI 需求、全部映射/未实现目录与运行时验收都完成后，才能把“所有 UI 迁移”标记完成。

