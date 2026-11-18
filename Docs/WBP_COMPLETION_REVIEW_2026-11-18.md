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
