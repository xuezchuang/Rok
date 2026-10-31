# Ported windows vs Unity prefab structure (2026-10-25)

Each row compares the Unity prefab's control skeleton (`m_Name` entries inside
`ref/client-unity/Assets/**/UI_Win_*.prefab`) with what the UE modal actually
renders. "Data" means the numbers/strings come from the real config tables;
"structure" means the widget actually has the prefab's controls.

| Prefab | Unity skeleton (evidence) | UE window today | Verdict |
|---|---|---|---|
| `UI_Win_TavernBoxDesc` | `lbl_lbl1`, `sv_list`, `lbl_lbl2` | scroll list of name + % | **match** |
| `UI_Win_TavernReward` | `c_list_view`, `sv_list_view`, `lbl_title`, `lbl_desc` | scroll list of pool entries | **match** |
| `UI_Win_BattlePassRewards` | `sv_list`, `c_list`, `lbl_title` | scroll list of rewards | **match** |
| `UI_Win_ItemCollection` | `pl_items` (grid), `lbl_tips` | 3-column card grid with the real item icons (`.tpsheet` rects + decoded atlas), name/type/quality/id, search box | **partial**: layout and icons match; no category tabs or rarity frames yet |
| `UI_Win_GuildPurview` | `sv_list`, `Ricon1..Ricon5` (rank icons) | text list of permissions | **partial**: no rank icon row |
| `UI_Win_GuildGift` | `sv_list_uncommon`, `pb_giftBar`, `btn_key`, `ck_rara` | tier text list | **partial**: no progress bar / key button / rarity filter |
| `UI_Win_GuildStore` | `sv_list`, `ipt_num`, `btn_substract`, `btn_add`, `btn_info1/2` | price text list | **partial**: no quantity stepper, no detail buttons |
| `UI_Win_ExpeditionStore` | item cards (`pl_item`, `img_frame`, `lbl_refreshNum`, `pl_refresh`) | text list | **partial**: no item cards/refresh timer |
| `UI_Win_EventDate` | `sv_content`, `pl_date`, `sv_list` | text list | **partial**: no date rail |
| `UI_Win_ChargePop` | `btn_close` only (content built at runtime) | text list | **partial**: prefab gives no skeleton to match |
| `UI_Win_GiftLimitedTime` | `pl_firstcharge`, `pl_item`, `img_picture`, `lbl_title/desc` | text list | **partial**: no art/picture area |
| `UI_Win_VipStore` | `sv_list`, `pb_vipBar`, `lbl_level`, `lbl_need` | text list | **partial**: no VIP progress bar |
| `UI_Win_Vip` | `sv_listVip`, `lbl_vipPoint`, `lbl_vipDay`, arrows, box timer | privilege text list | **partial**: no point/day bars, no level paging |
| `UI_Win_TheWall` | five skill plates (`img_skill1bg1..5`), `btn_hero_add2`, two scroll views | durability status window: bar, level/durability line, durability curve text, and an explicit note that the hero/skill plates have no config table | **partial**: no hero/skill slots because no table backs them (stated in the window) |
| `UI_Win_TheTower` | `pb_bar`, `lbl_state`, `img_Fill`, `lbl_tiptext` | status window: full HP bar (`TheTowerHpBar`), state line (level/attack/HP), tip text with the five HP-section rules | **partial**: structure matches, but the bar is a fixed full-duration preview (no live HP) |
| `UI_Win_ItemExchange` | `btn_add`, `btn_substract`, `ipt_num`, `lbl_changeNum`, arrows | entry list + selection panel + quantity stepper (`ItemExchangeMinus`/`ItemExchangePlus`/`ItemExchangeCount`) | **partial**: has the stepper controls, but no exchange rates or currency cost (server-side; stated in the window) |
| `UI_Win_FuncGuide` | `img_fullMask`, `img_anim_arrow`, `btn_jump`, `lbl_desc` | tutorial step list | **mismatch**: the prefab is the guidance overlay, not a list |
| `UI_Win_ChapterDialog` | `spine_hero_left`, `spine_hero_right`, `btn_noTextButton` | dialogue screen: left/right portrait plates, wrapped line text, `继续`, speaker/voice footer (`TaskDialog.csv`, group id) — chapter list only for group 0 | **partial**: structure now matches, but no portrait art or voice playback (models/voices labelled, not rendered/played) |
| `UI_Win_GuildFortress` | `rect` only | structure list | **unknown**: the prefab's content is built at runtime |
| `UI_Gallery` (dev) | — | 19-button grid | n/a (developer tool) |

## What this means (updated 2026-10-31)

Tally of the 19 ported windows: **3 match, 14 partial, 1 mismatch, 1 unknown**.

1. The **data path** for every ported window is real (tables → language table →
   rows); none of it is invented.
2. The first audit found seven structurally wrong screens. Six were rebuilt on
   2026-10-25 and are `partial` now:
   - `ChapterDialog`: portrait dialogue;
   - `TheTower` / `TheWall`: status windows;
   - `ItemExchange`: quantity stepper;
   - `ItemCollection`: icon grid;
   - `Vip`: still a privilege list; its missing level pager is noted in the row.
   `FuncGuide` is the only **mismatch** left: Unity's prefab is the guidance
   overlay, and UE still shows a step list.
3. The backlog's "Done" entries still mean **"data window landed"**, not
   "screen ported". Most `partial` rows lack the prefab's bars, steppers, icon
   rows or cards. None of these windows has a Unity reference capture yet.

## Suggested order for the next slices (2026-10-31)

The first list (ChapterDialog → TheTower/TheWall → ItemCollection grid →
ItemExchange stepper) is done. Next:

1. Fill in the cheap missing controls of `partial` windows:
   - `GuildPurview`: rank icon row;
   - `GuildGift`: progress bar;
   - `VipStore` / `Vip`: VIP bar and level paging;
   - `GuildStore`: stepper (reuse the ItemExchange one);
   - `EventDate`: date rail.
2. Item cards for `ExpeditionStore`, and category tabs / rarity frames for
   `ItemCollection` (icons already resolve through `FRokSpriteAtlas`).
3. `FuncGuide` as the overlay (`img_fullMask`, `img_anim_arrow`, plus
   `GuideAnim`/`Finger2/3`). This is runtime overlay work, not a list.
4. `ChapterDialog` portraits and voice. Both need assets: the
   `spine_hero_left/right` art and the voice clips.
