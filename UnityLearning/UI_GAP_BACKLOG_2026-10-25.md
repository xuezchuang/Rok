# Unity UI windows not yet reachable from the UE widgets

Generated 2026-10-25 by diffing `ref/client-unity/Assets/**/UI_Win_*.prefab` names against the four core widgets (`RokUnityModalWidget`, `RokUnityFeatureScreenWidget`, `RokMainHudWidget`, `RokWorldMapHudWidget`). A name matching there means the screen is at least referenced; it does not prove visual parity.

Total when generated: 115 of 156 window prefabs. **Still open on 2026-10-31: 97 of 156** (the list at the end). 19 were ported after the list was generated (under "Done" below). The other 40 already had a UE counterpart, `AddSpeed` included.
> **Read the parity audit first:** `UnityLearning/UI_PORTED_PARITY_2026-10-25.md` (2026-10-25).
> The entries below are *data windows*: their numbers are real, but several prefabs are a
> different kind of screen (TheWall/TheTower are status windows, ItemExchange is a quantity
> converter, FuncGuide is an overlay, ChapterDialog is a portrait dialogue). Treat `match` /
> `partial` / `mismatch` in that file as the current status.


Done since this list was generated:
- `UI_Win_EventDate.prefab` -> `URokUnityModalWidget::OpenEventDate` (2026-10-25): activity schedule from `ActivityCalendar.csv` (name via language table, `startTime` + `durationTime`), shared `BuildDefinitionList` layout; capture `ue/E39_event_date.png`; test `Rok.Ui.EventDate.Layout`; open with `-RokOpenModal=event_date`.
- `UI_Win_GuildPurview.prefab` -> `URokUnityModalWidget::OpenGuildPurview` (2026-10-25): rows from `AllianceMemberJurisdiction.csv` with permission names via the language table and the R1..R5 columns as the allowed-rank line; capture `ue/E40_guild_purview.png`; test `Rok.Ui.GuildPurview.Layout`; open with `-RokOpenModal=guild_purview`.
- `UI_Win_ChapterDialog.prefab` -> `URokUnityModalWidget::OpenChapterDialog` (2026-10-25): chapter rows from `TaskChapter.csv` (name + objective, markup/placeholder-stripped via `FRokUiTextTable::SanitizeRichText`); capture `ue/E43_chapter_dialog.png`; test `Rok.Ui.ChapterDialog.Layout`; open with `-RokOpenModal=chapter_dialog`. The dialogue script (`TaskDialog.csv`) is the next slice.
- `UI_Win_GuildFortress.prefab` -> `URokUnityModalWidget::OpenGuildFortress` (2026-10-25): one row per structure name with the most expensive `AllianceBuildingData.csv` row (`AllianceBuildingType.csv` names); capture `ue/E49_guild_fortress_v4.png`; test `Rok.Ui.GuildFortress.Layout`; open with `-RokOpenModal=guild_fortress`. Only two structure names carry costed rows in the table today (副要塞/旗帜); the rest are zero-cost rows.
- `UI_Win_ItemCollection.prefab` -> `URokUnityModalWidget::OpenItemCollection` (2026-10-25): 614 resolvable `Item.csv` names as scroll rows (type/quality detail, subType tag); capture `ue/E50_item_collection.png`; test `Rok.Ui.ItemCollection.Layout`; open with `-RokOpenModal=item_collection`.
- `UI_Win_VipStore.prefab` -> `URokUnityModalWidget::OpenVipStore` (2026-10-25): 33 `VipStore.csv` rows with inline names, VIP level/price/count and the raw `discount` column as the tag; capture `ue/E52_vip_store_final.png`; test `Rok.Ui.VipStore.Layout`; open with `-RokOpenModal=vip_store`.
- `UI_Win_TavernBoxDesc.prefab` -> `URokUnityModalWidget::OpenTavernBoxDesc` (2026-10-25): 22 `TavernProbility.csv` rows as name + probability percentage grouped by tavern tier; capture `ue/E53_tavern_rates.png`; test `Rok.Ui.TavernBoxDesc.Layout`; open with `-RokOpenModal=tavern_rate`.
- `UI_Win_TheWall.prefab` -> `URokUnityModalWidget::OpenTheWall` (2026-10-25): 25 `BuildingCityWall.csv` levels with `wallDurableMax`; capture `ue/E54_the_wall.png`; test `Rok.Ui.TheWall.Layout`; open with `-RokOpenModal=city_wall`. `UI_Win_TheTower` is the natural sibling (`BuildingGuardTower.csv`/`GuardTowerAttr.csv`).
- `UI_Win_TheTower.prefab` -> `URokUnityModalWidget::OpenTheTower` (2026-10-25): 25 `BuildingGuardTower.csv` levels plus the 5 `GuardTowerAttr.csv` HP-section buffs as one list; capture `ue/E55_the_tower.png`; test `Rok.Ui.TheTower.Layout`; open with `-RokOpenModal=guard_tower`.
- `UI_Win_GuildStore.prefab` -> `URokUnityModalWidget::OpenGuildStore` (2026-10-25): 21 `AllianceShopItem.csv` rows joined with `Item.csv` names, showing stock/selling price and group; capture `ue/E56_guild_store.png`; test `Rok.Ui.GuildStore.Layout`; open with `-RokOpenModal=guild_store`.
- `UI_Win_GiftLimitedTime.prefab` -> `URokUnityModalWidget::OpenGiftLimitedTime` (2026-10-25): 18 `RechargeLimitTimeBag.csv` packs with price/unlock level and pack type; uses the new `FRokCsv::ParseRecords` because `des` spans quoted newlines; capture `ue/E59_gift_limited_v2.png`; test `Rok.Ui.GiftLimitedTime.Layout`; open with `-RokOpenModal=gift_limited`.
- `UI_Win_ChargePop.prefab` -> `URokUnityModalWidget::OpenChargePop` (2026-10-25): 40 `RechargeSale.csv` packages (name/price, 限定 · 畅销 tag) with the record reader because the table repeats `l_nameID`; capture `ue/E61_charge_pop_final.png`; test `Rok.Ui.ChargePop.Layout`; open with `-RokOpenModal=charge_pop`.
- `UI_Win_ExpeditionStore.prefab` -> `URokUnityModalWidget::OpenExpeditionStore` (2026-10-25): 786 `ExpeditionShop.csv` rows deduped by (group,item,price) with Item.csv names, price and count; capture `ue/E62_expedition_store.png`; test `Rok.Ui.ExpeditionStore.Layout`; open with `-RokOpenModal=expedition_store`.
- `UI_Win_TavernReward.prefab` -> `URokUnityModalWidget::OpenTavernReward` (2026-10-25): 92 `TavernRank.csv` pool entries (name in `comments`, class + pool in the detail, chest in the tag); capture `ue/E63_tavern_reward.png`; test `Rok.Ui.TavernReward.Layout`; open with `-RokOpenModal=tavern_reward`. Pairs with the probability window.
- `UI_Win_FuncGuide.prefab` -> `URokUnityModalWidget::OpenFuncGuide` (2026-10-25): 77 `Guide.csv` tutorial steps (title from the first `step` column, stage/UI in the detail); capture `ue/E64_func_guide.png`; test `Rok.Ui.FuncGuide.Layout`; open with `-RokOpenModal=func_guide`. `UI_Win_GuideAnim`/`Finger2`/`Finger3` remain runtime overlay work, not list windows.
- `UI_Win_GuildGift.prefab` -> `URokUnityModalWidget::OpenGuildGift` (2026-10-25): 350 `AllianceGiftReward.csv` tiers (chest name + level, gift/key points, reward id) via `AllianceGiftType.csv` names; capture `ue/E65_guild_gift.png`; test `Rok.Ui.GuildGift.Layout`; open with `-RokOpenModal=guild_gift`. Reward ids are not item ids, so they show raw for now.
- `UI_Win_ItemExchange.prefab` -> `URokUnityModalWidget::OpenItemExchange` (2026-10-25): 95 `ItemRewardChoice.csv` selectable entries (inline 中文名, group + count); capture `ue/E66_item_exchange.png`; test `Rok.Ui.ItemExchange.Layout`; open with `-RokOpenModal=item_exchange`.
- `UI_Win_Vip.prefab` -> `URokUnityModalWidget::OpenVipPrivilege` (2026-10-25): 208 positive `VipAtt.csv` privileges across VIP 0-17 (attribute name + granted value, level tag) with the search box; capture `ue/E71_vip_privilege_total.png`; test `Rok.Ui.VipPrivilege.Layout`; open with `-RokOpenModal=vip_privilege`.
- **Heuristic false positive:** `UI_Win_AddSpeed.prefab` is already covered by the feature-screen `URokUnityFeatureScreenWidget::OpenSpeedUp` (`ERokUnityFeatureScreenMode::SpeedUp`, queue lifecycle + pages), reachable with `-RokOpenModal=speed_up` and from the building panel's feature button while a queue is active. A modal duplicate was built on 2026-10-25 and removed the same day; check `Source/Rok/RokUnityFeatureScreenWidget.*` before porting a window whose name matches a feature mode.
- `UI_Win_BattlePassRewards.prefab` -> `URokUnityModalWidget::OpenBattlePassRewards` (2026-10-25): rows from `BattlePassReward.csv` (age 0) with Item.csv -> Language.csv names for both tracks; capture `ue/E37_bp_rewards_final.png`; test `Rok.Ui.BattlePassRewards.Layout`; open with `-RokOpenModal=bp_rewards`.

- `UI_Win_AccountMgr.prefab`
- `UI_Win_AddRes.prefab`
- `UI_Win_Agreement.prefab`
- `UI_Win_AgreementDetial.prefab`
- `UI_Win_ArmyBuff.prefab`
- `UI_Win_AssitRes.prefab`
- `UI_Win_BagGiftOpen.prefab`
- `UI_Win_BattleLog.prefab`
- `UI_Win_BattlePassBuy.prefab`
- `UI_Win_BattlePassRule.prefab`
- `UI_Win_BookAdd.prefab`
- `UI_Win_CaptainItemSource.prefab`
- `UI_Win_CaptainLevelUp.prefab`
- `UI_Win_ChargeFirst.prefab`
- `UI_Win_ChoseChat.prefab`
- `UI_Win_ChoseContact.prefab`
- `UI_Win_CreateArmy.prefab`
- `UI_Win_EmailEnclosure.prefab`
- `UI_Win_EmojiSet.prefab`
- `UI_Win_EquipQuick.prefab`
- `UI_Win_EventTypeRankReward.prefab`
- `UI_Win_ExChangeActionPoint.prefab`
- `UI_Win_ExpeditionRule.prefab`
- `UI_Win_Finger2.prefab`
- `UI_Win_Finger3.prefab`
- `UI_Win_GameTool.prefab`
- `UI_Win_GameToolViewSetting.prefab`
- `UI_Win_GemShort.prefab`
- `UI_Win_GroupMember.prefab`
- `UI_Win_GuideAnim.prefab`
- `UI_Win_GuildApplication.prefab`
- `UI_Win_GuildAppoint.prefab`
- `UI_Win_GuildBuild.prefab`
- `UI_Win_GuildBuildDesc.prefab`
- `UI_Win_GuildChangeName.prefab`
- `UI_Win_GuildCollection.prefab`
- `UI_Win_GuildDepot.prefab`
- `UI_Win_GuildDissolution.prefab`
- `UI_Win_GuildFlagSetting.prefab`
- `UI_Win_GuildHelp.prefab`
- `UI_Win_GuildHoly.prefab`
- `UI_Win_GuildInvite.prefab`
- `UI_Win_GuildInviteMsg.prefab`
- `UI_Win_GuildJoin.prefab`
- `UI_Win_GuildLanguage.prefab`
- `UI_Win_GuildMain.prefab`
- `UI_Win_GuildMember.prefab`
- `UI_Win_GuildMemRemove.prefab`
- `UI_Win_GuildMemUpgrade.prefab`
- `UI_Win_GuildRanking.prefab`
- `UI_Win_GuildResearch.prefab`
- `UI_Win_GuildResearchRewardView.prefab`
- `UI_Win_GuildResearchUpate.prefab`
- `UI_Win_GuildSetting.prefab`
- `UI_Win_GuildTerrtroy.prefab`
- `UI_Win_GuildView.prefab`
- `UI_Win_GuildWelcome.prefab`
- `UI_Win_GuildWelcomeEdit.prefab`
- `UI_Win_ListViewTest.prefab`
- `UI_Win_LoginMain.prefab`
- `UI_Win_LoginType.prefab`
- `UI_Win_MailContact.prefab`
- `UI_Win_MailShare.prefab`
- `UI_Win_MainTain.prefab`
- `UI_Win_MonumentAllianceShow.prefab`
- `UI_Win_MysteryStore.prefab`
- `UI_Win_MysteryStoreGone.prefab`
- `UI_Win_NewRoleActivity.prefab`
- `UI_Win_NormalStore.prefab`
- `UI_Win_NPCDialog.prefab`
- `UI_Win_OpenFogShow.prefab`
- `UI_Win_OtherPlayerData.prefab`
- `UI_Win_PlayerChangeSure.prefab`
- `UI_Win_PlayerManage.prefab`
- `UI_Win_PlayerNewChar.prefab`
- `UI_Win_QuickUseItem.prefab`
- `UI_Win_QuickUseResources.prefab`
- `UI_Win_Reinforcements.prefab`
- `UI_Win_ReplyAMail.prefab`
- `UI_Win_ResearchUpate.prefab`
- `UI_Win_ResShort.prefab`
- `UI_Win_ResShortSpecial.prefab`
- `UI_Win_RewardGet.prefab`
- `UI_Win_RewardGetWin.prefab`
- `UI_Win_SearchFriend.prefab`
- `UI_Win_SettingNotice.prefab`
- `UI_Win_StrongerPlayerRank.prefab`
- `UI_Win_Suggest.prefab`
- `UI_Win_TalentChangeAlert.prefab`
- `UI_Win_TalentChangeName.prefab`
- `UI_Win_TestUICompoment.prefab`
- `UI_Win_TrainDissolve.prefab`
- `UI_Win_UseItem.prefab`
- `UI_Win_WarDetial.prefab`
- `UI_Win_WebView.prefab`
- `UI_Win_WorldObjectInfoBuildInfo.prefab`
- `UI_Win_WriteAMail.prefab`
