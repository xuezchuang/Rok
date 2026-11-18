# WBP 本轮审查清单（2026-11-18）

本轮处理现有游戏入口。下面列出要打开的位置、对应资产和需要对比的状态。界面资产已在 UE 5.8.4 中保存并重新加载。新接入的 C++ 绑定还需要用当前源码构建的客户端回看。

| 从哪打开 | Designer 资产 | 对比和审查 |
| --- | --- | --- |
| 城市、切到世界地图 | `RokUI/Hud/WBP_RokCityHud`、`WBP_RokTacticalHud` | 顶部五种资源数字。检查字号、间距、字形以及石料和金币解锁后的排列。 |
| 城市建筑上方 | `RokUI/Hud/WBP_RokCityStatusIndicator` | 观察医院、空闲工人、免费招募、训练休眠和升级旗帜。检查缩放、遮挡、选中建筑时的显示变化及医院点击入口。 |
| 邮件 → 系统 | `RokUI/Feature/WBP_RokFeature_ServerMailScreen`、`ServerMailRow`、`ServerMailRewardCard`（均带 `WBP_RokFeature_` 前缀） | 检查加载、空列表、未读切换、长正文滚动、无附件和有附件。领取用测试邮件。 |
| 邮件 → 报告 → 选择战报 | `RokUI/Feature/WBP_RokFeature_BattleReportOverview`、`BattleReportInboxRow`、`BattleReportParticipantCard`、`BattleReportLootCard`（同前缀） | 检查空列表、详情获取中、加载失败、已加载。数据齐全时检查多奖励、多参战部队和兵力曲线，缺少事件时检查提示。 |
| 战令奖励、警戒塔、城墙和建筑详情的配置缺失状态 | `RokUI/Modal/Items/WBP_RokModal_StatusMessages` | 可在 Designer 核对标准、宽版、建筑等级和兵营表头四种文本布局。运行时用受控数据触发缺失状态。 |
| 头像 → 设置 → 帮助 → 客服 | `RokUI/Help/WBP_RokHelpCustomerServiceBoundary` | 检查提示位置和文字，确认帮助页仍可滚动和返回。 |

两个已有 HUD 用 `Scripts/RokUi/UpdateHudResourceText.ps1 -Mode Apply` 定向更新。城市提示、邮件战报、Modal 和帮助模板分别由 `BuildCityStatusIndicatorWbpSpec.py`、`GenerateBattleReportWbps.ps1`、`PrepareModalStatusWbpSpecs.py` 准备。新建资产独立重载的记录在 `Saved/RokUi/Logs/verify_city_status_indicator_goal_20261118_final.log` 和 `verify_mail_modal_goal_20261118.log`。

使用现有 Rok DLL 运行了 `Rok.Net.TopBar.Hud` 和 `Rok.Ui.Animation.Hud.TopBarSlices`。两项通过，均为零错误、零警告。报告位于 `Saved/RokUi/Tests/hud_resource_text_goal_20261118/index.json`，覆盖已保存 HUD 的默认类选择、资源数据绑定和布局更新。邮件和建筑提示的新 C++ 绑定仍待运行确认。

首批更新 2 个 HUD 资产并新增 10 个 WBP。`Content/` 由 Perforce 管理。源码、脚本和这些本地资产需要成套交接。城市提示仍使用已有的 `Content/RokData/CityStatusIndicators` 运行时图片。

## 续查补齐的正常状态

复查了城市与世界 HUD、功能页、建筑详情、兵营、斥候、科研和帮助的正常入口。已有主布局继续使用原 WBP。下面是默认路径仍由 C++ 决定布局或样式的部分，本次新增 6 个 WBP。

| 从哪打开 | Designer 资产 | 对比和审查 |
| --- | --- | --- |
| 世界地图 → 行军列表 | `RokUI/Hud/WBP_RokWorldMarchEntry` | 检查点击整行能否选中原部队。分别看行军、采集、返回，以及缺统帅头像的条目。已有 `WBP_RokWorldMarchRow` 继续提供图层。 |
| 城市 → 远征 → 选关 → 部队编成 | `RokUI/Feature/WBP_RokExpeditionListLayout`、`WBP_RokExpeditionCommanderDisplay` | 检查关卡标记偏移、奖励排列、敌方阵容、星级目标、统帅与士兵行高。切换已知统帅后检查模型是否按对应数据显示。 |
| 城市 → 个人头像 → 资料 | `RokUI/Modal/Items/WBP_RokModal_ProfileDataPresentation` | 在账号有文明和行动力数据时检查图标和进度条。当前本地初始化数据不保证带这两个字段。 |
| 日常任务 → 奖励 | `RokUI/Feature/WBP_RokFeature_TaskRewardTextOnly` | 找有图标和无图标的奖励各一条。无图标项检查名称与数量是否完整。 |
| 通用设置、章节任务对白、工人队列雇佣 | `RokUI/Feature/WBP_RokFeature_FeatureTextStates` | 检查设置推荐文字的高亮、章节对白的强调文字、余额充足与不足时的价格颜色。 |

6 个新增资产均通过独立重载检查，设计树和生成树与规格中的控件类型及层级相符。世界行军另外检查了可点击按钮与已有行军图层模板的组合。日志见 `Saved/RokUi/Logs/verify_wbp_exp_profile_20261118.log`、`verify_wbp_world_march_20261118.log`、`verify_world_march_entry_composition_20261118.log` 和 `verify_wbp_feature_states_20261118.log`。本轮未编译 C++，这些新增绑定的运行效果待回看。


## 续查补齐的缺数据提示

| 从哪打开 | Designer 资产 | 触发条件 |
| --- | --- | --- |
| 个人头像 → 更换头像、个人排行榜 | `RokUI/Modal/Items/WBP_RokModal_UnavailableStatusMessages` | 头像目录或排行榜目录读取失败。两个提示有独立布局。 |
| 兵营 → 升级 | `RokUI/Modal/Items/WBP_RokModal_UnavailableStatusMessages` | 升级属性无法加载。两处显示分支共用模板，沿用已有属性区位置和层级。 |
| 增益 → 配置参考 | `RokUI/Modal/Items/WBP_RokModal_UnavailableStatusMessages` | 静态增益目录无可用定义。 |
| 个人头像 → 设置 → 帮助 | `RokUI/Help/WBP_RokHelpLoadError` | 帮助目录、必要控件或字形初始化失败时，沿用现有错误文案。 |
| 训练 → 选兵种、详情 | `RokUI/Feature/WBP_RokFeature_TrainArmyCatalogUnavailable`、`TrainArmyDetailUnavailable`、`TrainArmyModelUnavailable`（同前缀） | 分别检查兵种目录、详情配置、模型加载失败。详情保留返回操作，模型失败保留可用的静态头像。 |

复查确认联盟排行榜、官职、礼物、商店和科技的配置缺失，日常任务目录缺失、章节对白空态、斥候目录缺失、活动日历配置缺失以及统帅和建筑外观预览不可用已有 WBP 入口。源码中仍可见对应的原生备用控件。

本次新增 5 个 WBP。独立重载检查通过，记录见 `Saved/RokUi/Logs/verify_wbp_unavailable_modal_help_20261118.log` 和 `Saved/RokUi/Logs/verify_train_unavailable_20261118.log`。使用已有 UE 5.8.4 和 Rok DLL 保存并检查资产，未编译 C++。新接入的代码和实际缺数据时的画面待人工对比。


## 训练费用、状态配色和嵌套控件树

- 训练费用的位置和锚点在 `RokUI/Feature/WBP_RokFeature_TrainResourceCostLayout`。按可见资源数选择 `TrainCostLayout_1_Resources` 至 `TrainCostLayout_4_Resources`，编辑对应 `TrainCostSlot_*` 的位置和锚点。费用格尺寸和内容样式仍在训练参数页。默认锚点让整排费用相对容器居中。
- 加速购买价格与章节分组标签的四种颜色在 `RokUI/Feature/WBP_RokFeature_FeatureTextColors`。打开玩家建筑加速的购买行，以及章节任务的分组按钮检查状态切换。
- `Rok.UiWbp.DumpModalTree <modal key> <dump name>` 导出弹窗内容树。配合 `EditorCmd.ps1 -Wbp` 选择 WBP 路径，输出在 `Saved/RokUi/Trees`。`TreeDiff.py` 默认按解析后的旧控件名比较完整层级。`--logical` 可略去可复用 WBP 包装层和画布根节点，该模式省略这些层的几何和可见性比较。

两个新资产使用已有 UE 5.8.4 二进制生成并独立重载。设计树与生成树的 10 个排位锚点、默认间距和 4 个颜色值通过检查。树差异脚本的 9 项回归测试通过。新 C++ 绑定及导出命令的运行验证待当前源码构建后进行。


## 任务、训练、建筑、活动和交互样式

| 入口 | Designer 位置 | 本轮迁移 |
| --- | --- | --- |
| 城市任务栏、任务页 | `RokUI/Hud/WBP_RokHudTaskRow`、`RokUI/Styles/WBP_RokTaskPresentation` | HUD 文字框和最大字号由任务行提供，动态缩字仍由原逻辑计算。样式模板提供主支线颜色、服务器任务行字号与省略规则，以及普通与 `99+` 角标。 |
| 兵营训练 | `RokUI/Feature/WBP_RokFeature_TrainArmyActionPresentation` | `TrainStartLabelReady` 和 `TrainStartLabelOther` 提供按钮文字的 Y 坐标，`TrainTierUnlocked` 和 `TrainTierLocked` 提供兵阶标签颜色。 |
| 建筑详情、资源建筑详情、普通升级、兵营升级 | `RokUI/Modal/Pages/WBP_RokModal_BuildingPreviewLayout`、现有兵营页面 | 四组 `PreviewSlot` 和 `PreviewCenter` 负责预览框和适配中心。兵营级别和表头控件提供字体尺寸、文字区域、对齐、偏移和颜色的可编辑属性。 |
| 活动中心、活动日历 | `RokUI/Feature/WBP_RokFeature_ActivityListPresentation` | `CalendarRowBounds` 提供行尺寸，`CalendarRowGap` 的高度提供行间距。菜单和日历标题各有普通与选中颜色。 |
| 世界筛选、城市放置 | `RokUI/Hud/WBP_RokButtonFeedbackStyles` | 筛选按钮、确认按钮、取消按钮各自提供普通、悬停、按下和禁用的颜色。有效性判断和过渡时间沿用原代码。 |
| 帮助列表 | `RokUI/Help/WBP_RokHelpGroupEntry`、`WBP_RokHelpQuestionEntry` | 两个行模板沿用原有图层，`Extent` 提供行尺寸，`Action` 提供点击区域。分类展开、滚动位置和详情仍由原逻辑处理。 |

新增 7 个资产，原有 WBP 文件继续使用。两个帮助行生成时提示内嵌了已有子 WBP。独立重载通过，14 棵设计树和生成树的关键尺寸、锚点、颜色和字号也通过检查。双批日志为 `Saved/RokUi/Logs/verify_train_activity_presentation_goal_20261118.log`、`verify_task_building_controls_goal_20261118.log`，实际属性检查见 `verify_presentation_values_goal_20261118.log`。

建筑布局生成脚本读取 `Saved/RokUi/Specs/` 下四份原生预览规格。其他新模板从仓库中的 Python 规格脚本生成。本轮未编译 C++；实际入口、兵营新属性和画面对比待后续确认。


## HUD 折叠、出征选队和列表排位

| 入口 | Designer 资产 | 可编辑内容 |
| --- | --- | --- |
| 城市 HUD → 收起功能栏、任务栏 | `RokUI/Hud/WBP_RokHudFoldLayout` | `CollapsedTaskBackground` 的尺寸、`CollapsedTaskClip` 的位置和尺寸、`CollapsedTaskArrow` 的位置和旋转。功能栏三处标记提供背景宽度、箭头旋转和按钮横移。展开端仍由现有 HUD 页面提供。 |
| 世界地图 → 出征 → 已有部队 | `RokUI/Hud/WBP_RokWorldAttackArmyChoiceStyles` | `WorldAttackArmyPlainStyle`、`WorldAttackArmySelectedStyle` 各自的普通、悬停和按下画刷。 |
| 医院 → 选择治疗数量 | `RokUI/Feature/WBP_RokFeature_HospitalResourceCostLayout` | 按费用资源数量选一至四项排位，编辑每个图标和数值标记的位置、锚点与对齐。 |
| 章节任务 → 章节总奖励、单任务奖励 | `RokUI/Feature/WBP_RokFeature_TaskChapterRewardLayouts` | 服务器章节与配置参考页各有总奖励和单任务两组。编辑 `Cell` 的尺寸、锚点和对齐，移动 `Next` 调整重复间距。 |
| 邮件 → 系统 | `RokUI/Feature/WBP_RokFeature_ServerMailLayout` | `ServerMailHost` 的区块尺寸、锚点、位置和层级。原详情 WBP 填满该宿主。 |
| 城市或世界地图 → 部队一览 | `RokUI/Feature/WBP_RokFeature_ArmyShowListLayout` | `ArmyShowWaitingExtent`、`ArmyShowEmptyExtent`、`ArmyShowMarchExtent` 的尺寸分别用于等同步、列表空和行军项。 |

前四项和后两项按顺序分批生成。6 个新资产都通过独立重载，以及 12 棵设计树和生成树的默认值检查，涵盖折叠端点、出征画刷图集区域、医院四组排位、章节奖励步距、邮件宿主和部队行尺寸。日志位于 `Saved/RokUi/Logs/verify_hud_world_hospital_chapter_goal_20261118.log`、`verify_mail_army_list_layout_goal_20261118.log` 和 `verify_fold_and_list_values_goal_20261118_serialized.log`。

未编译 C++ 或提交。新绑定的实际运行及画面对比待人工确认。

## 建筑快捷菜单、行军状态和重复行布局

| 入口 | Designer 资产 | 可编辑内容 |
| --- | --- | --- |
| 城市 → 选中建筑 | `RokUI/Building/WBP_RokBuildingMenuLayouts` | `TwoButtons`、`BuilderHut`、`Decoration` 各组标记相对现有菜单根节点排位。三按钮布局在 `WBP_RokBuildingPanel`。屏幕投影把同一垂直偏移应用到各按钮。 |
| 世界行军、采集编队、出征和统帅头像 | `RokUI/Hud/WBP_RokWorldArmyStateStyles` | 五种 `ArmyFrame` 的图集、裁切区域和颜色，以及七种 `ArmyStatus` 图标。头像、军队状态及交互由实时数据确定。 |
| 统帅 → 技能 | `RokUI/Feature/WBP_RokFeature_CommanderSkillBar` | `CommanderSkillHost` 提供整排位置，`CommanderSkillCell` 提供格子排位与大小，移动 `CommanderSkillNext` 调整步距。技能实例仍用 `WBP_RokFeature_CommanderSkillItem`。 |
| 每日、主线和支线任务 | `RokUI/Feature/WBP_RokFeature_TaskListLayouts` | 每日服务器任务行以及本地、服务器主线与支线行各自的外层占位。每日配置参考行使用已有行资产的尺寸。 |
| 资源建筑和兵营 → 等级表 | `RokUI/Modal/Pages/WBP_RokModal_BuildingLevelTableLayouts` | `ResourceLevelCell`、`BarracksLevelCell` 提供首行位置和尺寸，两组 `Next` 标记给出行步距。滚动内容高度覆盖末行。 |

对应生成规格在 `Scripts/RokUi/BuildBuildingMenuLayoutsSpec.py`、`BuildWorldArmyStateStylesSpec.py`、`BuildCommanderSkillBarSpec.py`、`BuildTaskListLayoutSpec.py` 和 `BuildBuildingLevelTableLayoutsSpec.py`。默认值取自原有正常入口，包括建筑投影后的按钮位置、行军图集区域和列表占位。

5 个新资产均通过独立重载。检查了 10 棵设计与生成控件树、各槽位几何、图集资源、裁切区域及颜色。日志为 `Saved/RokUi/Logs/verify_army_skill_table_layouts_goal.log`。未编译 C++，实际入口和画面由人工审查。

## 聊天展开、列表排位、世界详情与资料提示

| 入口 | Designer 资产 | 可编辑内容 |
| --- | --- | --- |
| 聊天 → 展开表情 | `RokUI/Feature/WBP_RokFeature_Chat`、`WBP_RokFeature_FeatureFeedback` | `ChatEmojiPanel` 的高度带动消息区、输入栏和空态排位。`ChatEmojiLayout` 提供展开后反馈的位置和尺寸。 |
| 城市管理 → 增益详情 | `RokUI/Feature/WBP_RokFeature_CityManagerDetailLayout` | `Extent`、`Stride`、`Minimum` 标记提供行尺寸、步距和最小滚动内容高度，内容按实际行数覆盖末行。 |
| 部队统计 → 全部、城内、城外 | `RokUI/Feature/WBP_RokFeature_ArmyStatisticsListLayout` | 普通行、等待行和空行分别有尺寸标记，普通行有奇偶两种底色。 |
| 世界 → 资源点、乱军详情 | `RokUI/Hud/WBP_RokWorldObjectStyles` | 四类资源的详情和进度图标、五种奖励品质框与两种头像画刷提供图集、裁切区域、尺寸和画刷颜色。详情与奖励格的槽位仍在现有 WBP。 |
| 头像 → 行动力说明、改名；市政厅 → 增益效果 | `RokUI/Modal/Items/WBP_RokModal_ContextStates`、`WBP_RokModal_GainEffectUnavailable` | 行动力富文本的正文与剩余时间样式、改名校验和增益标签的状态色。经济配置缺失时显示独立提示模板。 |

新增资产的生成规格在 `Scripts/RokUi/BuildCityManagerAndArmyStatisticsLayoutSpecs.py`、`BuildWorldObjectStylesSpec.py` 和 `BuildModalContextStatesSpecs.py`。聊天读取已有 WBP 槽位。

5 个新资产和聊天使用的 2 个现有资产均通过独立重载。核对了 14 棵设计与生成控件树、排位、图集区域和文字样式。日志位于 `Saved/RokUi/Logs/verify_chat_emoji_layout_goal.log`、`verify_rows_and_world_goal.log` 与 `verify_modal_context_states_goal.log`。未编译 C++，实际入口和画面对比由人工审查。

## 远征状态画刷、商店与角色选项排位

| 入口 | Designer 资产 | 可编辑内容 |
| --- | --- | --- |
| 远征 → 选关、编队 | `RokUI/Feature/WBP_RokExpeditionStateBrushes` | 12 个画刷分别用于关卡旗帜、底座、选中光、空星与满星，以及可用和锁定的容量格、编队格。地图和编队页保留原本的槽位、图像尺寸和颜色。 |
| 远征 → 商店 | `RokUI/Modal/Pages/WBP_RokModal_ExpeditionStoreListLayout` | `RowExtent` 提供行尺寸，`FirstRow` 和 `NextRow` 提供排位和纵向步距，`ScrollPadding` 的尺寸控制右侧与底部留白。搜索结果按实际行数计算滚动范围。 |
| 资料 → 头像、头像框或文明 | `RokUI/Modal/Pages/WBP_RokModal_ProfileChoiceLayouts` | `PlayerHeadCell_0` 至 `PlayerHeadCell_3` 给出四列起点和格尺寸，`PlayerHeadNextRow` 给出纵向步距。文明旗帜有普通与选中槽位，`CivilizationFlagNext` 给出从右向左的重复步距。 |
| 统帅 → 天赋 | `RokUI/Feature/WBP_RokFeature_CommanderTalentRowLayout` | `RowExtent` 提供节点行的外层占位，初始为 `402×40`。天赋节点、标签和操作继续使用现有行 WBP。 |

对应生成规格在 `Scripts/RokUi/BuildExpeditionStateBrushesSpec.py`、`BuildExpeditionStoreListLayoutSpec.py`、`BuildProfileChoiceLayoutsSpec.py` 和 `BuildCommanderTalentRowLayoutSpec.py`。

4 个新资产均通过独立重载，核对了 8 棵设计与生成控件树的槽位及纹理引用。记录见 `Saved/RokUi/Logs/verify_expedition_brushes_and_store_layout_goal.log` 和 `verify_profile_and_talent_layouts_goal.log`。未编译 C++，实际入口与画面对比由人工审查。
