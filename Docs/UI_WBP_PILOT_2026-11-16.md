# UI 转 WBP：试点结果（2026-11-16）

把 C++ 现场拼出来的 UI 改成 Widget Blueprint（WBP）的第一批。全程没有打开编辑器或游戏窗口：WBP 由无界面的编辑器（`UnrealEditor-Cmd -NullRHI`）生成，截图用 `-game -RenderOffscreen`（不建窗口）。计划见 `C:\Users\13436\.claude\plans\golden-seeking-crescent.md`。

## 结论

弹窗（`URokUnityModalWidget`）的两类公共部件已经由 WBP 绘制：

- **Type2 窗口框**：25 个弹窗模式共用。
- **定义列表**：15 个模式共用。

通过的验收门：

- 25 个模式的 WBP 截图和 C++ 截图逐像素一致。时钟区域除外，原因见下。
- `Rok.*` 自动化测试两种模式结果完全相同。

C++ 只负责填数据和行为（标题、文字、图标、搜索、关闭）。布局、底图和字号都在 WBP 里，可以在 UMG Designer 中修改。

## 生成的资产（`/Game/RokUI/Modal/`，共 7 个，都还没进 P4）

| WBP | 父类（C++） | 来源（从哪个模式的 C++ 树复制） | 用在 |
|---|---|---|---|
| `WBP_RokModalWindowType2` | `URokModalWindowFrame` | guild_purview 的 `Type2*` 部件 | 除兵营升级外的所有 Type2 窗口（25 个模式） |
| `WBP_RokDefinitionList` | `URokDefinitionListWidget` | guild_purview | 单列、无搜索的列表 |
| `WBP_RokDefinitionListSearch` | 同上 | vip_privilege | 单列、带搜索 |
| `WBP_RokDefinitionGridSearch` | 同上 | item_collection | 三列网格、带搜索 |
| `WBP_RokDefinitionRow` | `URokDefinitionRowWidget` | guild_purview 第一行 | 无图标的行 |
| `WBP_RokDefinitionRowIcon` | 同上 | guild_store 第一个带图标的行 | 带图标的行 |
| `WBP_RokDefinitionCardIcon` | 同上 | item_collection 第一张卡 | 网格里带图标的卡 |

`Config/DefaultGame.ini` 的 `[/Script/Rok.RokUiSettings]` 决定每个部件用哪个 WBP：

- 某一项留空，该部件就继续用 C++ 拼，所以可以逐个屏幕切换。
- 命令行 `-RokUiLegacy` 强制全部改回 C++，前后对比就靠它。
- `/Game/RokUI` 已加入 `DirectoriesToAlwaysCook`。

## 怎么做的

- **生成器**：编辑器模块 `RokUiTools`，提供控制台命令 `Rok.UiWbp.GenerateModalPiece <模式> <frame|list|row|row_icon|card_icon> <资产路径>`。
  - 它先用 C++ 打开该模式，复制部件的树（类、名字、槽位、属性）到新 WBP，然后编译、保存。
  - 只新建，不覆盖：WBP 一旦存在就以 WBP 为准。
  - 嵌入的九宫格底图会自动加上锚点，在 Designer 里改大小时会正确拉伸。
- **字体**：WBP 不能序列化 C++ 里临时拼的 Arial+雅黑复合字体。所以文字用 `URokUiTextBlock`，它有一个 `FontRole` 属性，运行时换回同一个复合字体，像素不变。Designer 里预览用 Roboto，字号正确。
- **名字兼容**：
  - 部件里的控件改用短名，例如 `Title`、`RowTitle`。
  - WBP 里的 `LegacyNames` 记录旧的平铺名，例如 `{Prefix}RowTitle{Suffix}`。
  - `RokUiFind::Find(宿主, 旧名)` 会进入嵌套的 WBP 查找，所以原来按 `GuildPurviewRowTitle_0` 查控件的测试和审计代码不用改逻辑。
  - 本次把 41 个测试文件和 27 个审计子系统里查弹窗控件的 `GetWidgetFromName` 换成了 `RokUiFind::Find`。

## 验收结果

| 门 | 结果 | 证据（都在 `Saved/RokUi/`，本地不提交） |
|---|---|---|
| G1 编译 | 7 个 WBP 编译无错，BindWidget 全部匹配 | `Logs/gen_modal_pieces.log`、`Logs/gen_modal_rows_icon.log` |
| G2 像素 | 25 个模式在时钟区域以外 0 像素不同（容差 2） | `Captures/<模式>_{legacy,wbp,diff}.png`，`Captures/modal_*_summary.csv` |
| G2 是否真的用了 WBP | 每个 WBP 截图日志都有 `Rok UI: drawing with WBP …`，7 个 WBP 都出现过。C++ 截图日志里没有这一行 | `Captures/<模式>_wbp.log` |
| G4 测试 | `Rok.*` 共 273 个：C++ 模式和 WBP 模式都是 256 过、17 败。每个测试的状态、第一条错误、警告数都相同。两次运行的警告/错误行集合也相同 | `Tests/rok_legacy/`，`Tests/rok_wbp/`（`results.csv`） |
| G6 真实入口 | 都走 `-RokOpenModal=<模式>`（与 HUD 打开弹窗同一路径），不是只在测试里构造 | 同 G2 |
| G7 无警告 | WBP 截图和测试日志里没有与新资产相关的警告。WBP 模式的警告/错误行与 C++ 模式逐条相同 | 同上 |

**比对时排除的部分：**

- HUD 右上角的运行信息行（`AST 月/日 时:分 lod:--`）显示的是当前时间。两次截图之间分钟数变了，这一行就会不同。所以对比时忽略矩形 `1100,80`–`1455,106`，忽略掉的像素数仍然会打印出来。
- building_upgrade 左侧的建筑预览在动。同为 C++ 的两次截图之间就差 8493 像素，而 C++ 和 WBP 之间只差 25 像素（最大差值 3），在它自身的波动范围内。

**17 个失败都是本机原有的，和 UI 无关：**

- 测试里写死了另一台机器的路径 `D:/ueDemo/Rok/`。
- 缺 Unity 采样证据。
- 发光贴图的标志位校验没通过。
- `-NullRHI` 下没有着色器预载，粒子计时也有误差。

两种模式下的失败完全一样。

## 顺带修掉的问题

- **`FRokSpriteAtlas` 的贴图缓存会留下悬空指针（`HEAD` 里就有）。**
  - 列表图标所用的图集贴图是运行时临时生成的。缓存用普通 `TMap` 保存，GC 看不见它。
  - 弹窗关闭后发生一次 GC，再打开带图标的列表时就会用到已经释放的贴图，引发崩溃。
  - 现在缓存改为弱引用，贴图被释放后会重新解码，与 `RokWorldMapEntityActor` 的做法一致。
- **无界面编辑器不响应 `Quit`（它只对游戏有效）**，`-ExecCmds` 跑完后会一直空转。现在脚本改用 `QUIT_EDITOR`；跑测试用 `-TestExit`。

## 还没做

- **删除 C++ 布局代码（G5）。** 现在 C++ 拼法还保留着，作为 `-RokUiLegacy` 的对照路径。等你确认方向后，再删掉列表和窗口框的 C++ 布局。
- **树结构对比（G3，`TreeDiff.py`）。** 树导出还不会进入嵌套的 WBP，这次只用了像素和日志。
- **计划里的功能页试点（`LanguageSettings`）。** 没有开始。
- **各弹窗自己的正文**（资源、建筑升级、增益效果、图鉴……）。这些仍由 C++ 拼在 WBP 窗口框里面。这类布局不规则，需要整页 WBP 加按名绑定（计划第 1 步的 `FRokUiBuildScope`），工作量明显更大。
- **兵营升级的 Type2 变体**还用 C++。
- **Designer 里的编辑体验。** 需要你自己打开编辑器看：例如打开 `WBP_RokDefinitionRow` 改一个颜色，保存后用下面的截图命令重拍。

## 重跑

先关闭 Rok 的编辑器和游戏进程，然后：

```powershell
# 构建（TDJ 编辑器开着时必须加 -NoHotReloadFromIDE）
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RokEditor Win64 Development "D:\ue\Rok\Rok.uproject" -waitmutex -NoHotReloadFromIDE
# 生成一个部件（目标资产已存在时会拒绝）
pwsh Scripts/RokUi/EditorCmd.ps1 -Name gen -Commands "Rok.UiWbp.GenerateModalPiece guild_purview row /Game/RokUI/Modal/WBP_RokDefinitionRow"
# C++ 与 WBP 成对截图并比对
pwsh Scripts/RokUi/CapturePairs.ps1 -Set modal_lists -Flag RokOpenModal -Keys guild_purview,vip_store,item_collection
# 测试：先 C++ 模式，再 WBP 模式
pwsh Scripts/RokUi/RunTests.ps1 -Name rok_legacy -Filter Rok. -Legacy
pwsh Scripts/RokUi/RunTests.ps1 -Name rok_wbp -Filter Rok.
```

每对截图约 50 秒，`Rok.*` 每轮连同编辑器启动约 2–3 分钟。
