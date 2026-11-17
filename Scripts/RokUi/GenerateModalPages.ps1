# Generates the modal window-frame and mode-page WBPs from the widget specs that
# CaptureSpecs.ps1 wrote (Saved/RokUi/Specs). Create-only: an asset that already
# exists is left alone, so once a WBP is edited in the designer it is the source.
# Runs in a headless editor (no window). Close Rok's editor and game first.
#   pwsh Scripts/RokUi/GenerateModalPages.ps1            # everything below
#   pwsh Scripts/RokUi/GenerateModalPages.ps1 -Only TheWall,TheTower
param(
    [string[]]$Only = @()
)

$ErrorActionPreference = 'Stop'
# pwsh -File passes "a,b" as one string.
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$pages ='/Game/RokUI/Modal/Pages'
$frames = '/Game/RokUI/Modal'
$items = '/Game/RokUI/Modal/Items'

# Window frames shared by several modes: <asset> <spec> <title> <close> <parts>.
$frameCommands = [ordered]@{
    Type1 = "Rok.UiWbp.GenerateFrame $frames/WBP_RokModalWindowType1 player_profile Type1Title Type1CloseButton Type1BodyLeft+Type1BodyRight+Type1HeaderLeft+Type1HeaderRight"
    Type3 = "Rok.UiWbp.GenerateFrame $frames/WBP_RokModalWindowType3 player_power Type3Title Type3CloseButton Type3BodyLeft+Type3BodyRight+Type3HeaderLeft+Type3HeaderRight"
    TypeMid = "Rok.UiWbp.GenerateFrame $frames/WBP_RokModalWindowTypeMid change_name TypeMidTitle TypeMidCloseButton TypeMidFrame"
    CivilizationChange = "Rok.UiWbp.GenerateFrame $frames/WBP_RokModalWindowCivilizationChange civilization_confirmation CivilizationChangeWindowTitle CivilizationChangeCloseButton CivilizationChangeBodyLeft+CivilizationChangeBodyRight+CivilizationChangeHeaderLeft+CivilizationChangeHeaderRight"
    Type2Barracks = @(
        "Rok.UiWbp.GeneratePage $frames/WBP_RokModalWindowType2BarracksBody frame_type2_barracks_body"
        "Rok.UiWbp.GenerateFrame $frames/WBP_RokModalWindowType2BarracksHeader building_upgrade_barracks Type2Title Type2CloseButton Type2HeaderLeft+Type2HeaderRight"
    )
}
$type1 = 'Type1BodyLeft+Type1BodyRight+Type1HeaderLeft+Type1HeaderRight+Type1Title+Type1CloseButton'
$type3 = 'Type3BodyLeft+Type3BodyRight+Type3HeaderLeft+Type3HeaderRight+Type3Title+Type3CloseButton'
$type2 = 'Type2BodyLeft+Type2BodyRight+Type2HeaderLeft+Type2HeaderRight+Type2Title+Type2CloseButton'
$civChange = 'CivilizationChangeBodyLeft+CivilizationChangeBodyRight+CivilizationChangeHeaderLeft+CivilizationChangeHeaderRight+CivilizationChangeWindowTitle+CivilizationChangeCloseButton'

# Mode pages: <asset> <spec per state...> [-Exclude=frame widgets or C++-filled panels].
# The first spec is the default state; widgets only later specs add start collapsed.
$pageCommands = [ordered]@{
    ResourceInventory = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_ResourceInventory resources resources_wood resources_stone resources_gold resources_reference"
    PlayerPowerDetails = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_PlayerPowerDetails player_power -Exclude=$type3"
    PlayerProfile = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_PlayerProfile player_profile -Exclude=$type1"
    BuildingPreviewPane = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_BuildingPreviewPane building_preview_pane"
    # The WBP page owns the preview pane layout; C++ supplies model-specific art and glow.
    BuildingInfo = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_BuildingInfo building_info -Exclude=BuildingPreviewPane"
    BuildingUpgrade = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_BuildingUpgrade page_building_upgrade -Exclude=BuildingPreviewPane"
    BarracksUpgrade = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_BarracksUpgrade page_barracks_upgrade -Exclude=$type2+BuildingPreviewPane -SkipChildren=BarracksLevelRows"
    BarracksUpgradeSource = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_BarracksUpgradeSource page_barracks_upgrade_source -Exclude=$type2+BuildingPreviewPane -SkipChildren=BarracksLevelRows"
    ChangeName = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_ChangeName change_name -Exclude=TypeMidFrame+TypeMidTitle+TypeMidCloseButton"
    WarWarning = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_WarWarning war_warning war_warning_reference"
    BuffList = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_BuffList buff_list buff_reference"
    ChapterDialogScreen = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_ChapterDialogScreen chapter_dialog_20050"
    UiGallery = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_UiGallery ui_gallery"
    GainEffect = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_GainEffect gain_effect gain_effect_military"
    TheTower = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_TheTower guard_tower"
    TheWall = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_TheWall city_wall"
    # Filter chips over the item collection's definition grid (the grid is its own WBP).
    ItemCollectionFilters = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_ItemCollectionFilters list_item_collection"
    CivilizationConfirmation = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_CivilizationConfirmation civilization_confirmation -Exclude=$civChange"
    BattlePassRewards = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_BattlePassRewards bp_rewards"
    # The entry rows are item WBPs (below) that C++ places in the scroll content.
    ItemExchange = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_ItemExchange item_exchange -SkipChildren=ItemExchangeScrollContent"
    # Overview (category tiles are items) and one category's detail.
    PlayerRankingOverview = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_PlayerRankingOverview player_ranking -Exclude=$type3 -SkipChildren=PlayerRankingMainPanel"
    PlayerRankingDetail = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_PlayerRankingDetail player_ranking_detail -Exclude=$type3"
    # Heads tab, frames tab (missing art) and a tagged frame; the tabs are renamed
    # per state so the page has a selected and a dark look of each (see $specSteps).
    PlayerHeadSelection = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_PlayerHeadSelection page_player_head page_player_head_frames page_player_head_frames_tag -Exclude=$type1 -SkipChildren=PlayerHeadGrid"
    # Description and level pages share one page; C++ slides between them. The
    # building art pane is added by C++; level rows are items.
    ResourceBuildingInfo = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_ResourceBuildingInfo resource_building_info -Exclude=BuildingPreviewPane -SkipChildren=BuildingLevelRows"
    # Country list flags are items; the animated hero mesh is added by C++.
    CivilizationSelection = "Rok.UiWbp.GeneratePage $pages/WBP_RokModal_CivilizationSelection civilization -Exclude=CivilizationAnimatedHero -SkipChildren=CivilizationListRoot"
}

# Items: one row, tile or cell that C++ places N times. -Root takes a row canvas's
# children; item specs cut by CutItemSpec.py are flat. -Prefix/-Suffix/-Index strip
# the mode prefix, data id and list index; the old flat names stay findable.
$itemCommands = [ordered]@{
    ItemExchangeRow = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_ItemExchangeRow item_exchange -Root=ItemExchangeRow_0 -Prefix=ItemExchange -Suffix=_0"
    PlayerRankingTile = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_PlayerRankingTile item_player_ranking_tile -Prefix=PlayerRanking -Suffix=_0"
    # Names carry the row id (PlayerHeadCell_1000) and the list index (PlayerHeadChoiceButton_0).
    PlayerHeadCell = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_PlayerHeadCell item_player_head_cell -Prefix=PlayerHead -Suffix=_1000 -Index=_0"
    BuildingLevelRow = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_BuildingLevelRow item_building_level_row -Prefix=BuildingLevel -Suffix=_1"
    BarracksLevelRow = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_BarracksLevelRow item_barracks_level_row -Prefix=BarracksLevel -Suffix=_1"
    # A country flag in the civilization list, and the selected one (risen, with its frame and pulse).
    CivilizationFlag = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_CivilizationFlag item_civilization_flag -Prefix=Civilization -Suffix=_301"
    CivilizationFlagSelected = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_CivilizationFlagSelected item_civilization_flag_selected -Prefix=Civilization -Suffix=_101"
    # Profile tooltips: the click-away layer with its bubble, mounted over the profile page.
    PlayerKillTip = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_PlayerKillTip player_kill_tip -Root=PlayerKillTipLayer"
    PlayerActionTip = "Rok.UiWbp.GeneratePage $items/WBP_RokModalItem_PlayerActionTip player_action_tip -Root=PlayerActionTipLayer"
}

# Spec preparation run before the editor: CutItemSpec.py (an item's widgets laid
# flat in a shared canvas, grouped at the item origin) and RenameSpec.py (one
# widget with a different layout per state, named per state).
$headTabs = 'PlayerHeadTabBackground_{0}=PlayerHeadTabBackground{1}_{0}', 'PlayerHeadTabIcon_{0}=PlayerHeadTabIcon{1}_{0}'
function TabRenames([string]$selected0, [string]$selected1) {
    foreach ($pattern in $headTabs) { $pattern -f 0, $selected0; $pattern -f 1, $selected1 }
}
if ($Only.Count -eq 0 -or @($Only | Where-Object { $_ -in 'BuildingPreviewPane', 'BuildingUpgrade', 'BarracksUpgrade', 'BarracksUpgradeSource' }).Count) {
    & py -3.12 (Join-Path $PSScriptRoot 'PrepareBuildingUpgradeSpecs.py')
    if ($LASTEXITCODE -ne 0) { throw 'PrepareBuildingUpgradeSpecs.py failed' }
}

$specSteps = @{
    Type2Barracks = @(
        , @('CutItemSpec.py', 'frame_type2_barracks_body', '--source', 'building_upgrade_barracks', '--origin', '0,0',
            'Type2BodyLeft', 'Type2BodyRight')
    )
    PlayerRankingTile = @(
        , @('CutItemSpec.py', 'item_player_ranking_tile', '--source', 'player_ranking', '--origin', '0,66',
            'PlayerRankingCategoryTile_0', 'PlayerRankingCategoryIcon_0', 'PlayerRankingCategoryLabelRegion_0',
            'PlayerRankingCategoryStateRegion_0', 'PlayerRankingCategoryOwnerRegion_0', 'PlayerRankingCategoryButton_0')
    )
    PlayerHeadSelection = @(
        @(@('RenameSpec.py', 'player_head', 'page_player_head') + (TabRenames 'Selected' 'Dark')),
        @(@('RenameSpec.py', 'player_head_frames', 'page_player_head_frames') + (TabRenames 'Dark' 'Selected')),
        @(@('RenameSpec.py', 'player_head_frames_tag', 'page_player_head_frames_tag') + (TabRenames 'Dark' 'Selected'))
    )
    # Cell 0 of the heads tab, its frame image from frame 0 and the missing-art
    # line from frame 1 (cell origins 26.8 and 171.8).
    PlayerHeadCell = @(
        , @('CutItemSpec.py', 'item_player_head_cell',
            '--source', 'player_head', '--origin', '26.8,0',
            'PlayerHeadCell_1000', 'PlayerHeadChoiceImage_1000', 'PlayerHeadAccess_1000', 'PlayerHeadChoiceButton_0',
            '--source', 'player_head_frames', '--origin', '26.8,0', 'PlayerHeadChoiceFrame_2000=PlayerHeadChoiceFrame_1000',
            '--source', 'player_head_frames', '--origin', '171.8,0', 'PlayerHeadMissingArt_2001=PlayerHeadMissingArt_1000')
    )
    # Level 1 of the farm table; the current-level highlight (drawn under the row)
    # from the barracks table, where level 1 is the known level.
    BuildingLevelRow = @(
        , @('CutItemSpec.py', 'item_building_level_row',
            '--source', 'building_upgrade_barracks_levels', '--origin', '0,0', 'BarracksLevelCurrentRow=BuildingLevelCurrentRow',
            '--source', 'resource_building_levels', '--origin', '0,0',
            'BuildingLevelLine_1', 'BuildingLevelCell_1_0', 'BuildingLevelCell_1_1', 'BuildingLevelCell_1_2', 'BuildingLevelCell_1_3')
    )
    BarracksLevelRow = @(
        , @('CutItemSpec.py', 'item_barracks_level_row',
            '--source', 'building_upgrade_barracks_levels', '--origin', '0,0',
            'BarracksLevelCurrentRow', 'BarracksLevelLine_1', 'BarracksLevelCellRegion_1_0',
            'BarracksLevelCellRegion_1_1', 'BarracksLevelCellRegion_1_2', 'BarracksLevelCellRegion_1_3')
    )
    # Rome (locked) with Egypt's hit button; Arabia as the selected flag. Origins
    # are the flag (unselected) and the frame (selected).
    CivilizationFlag = @(
        , @('CutItemSpec.py', 'item_civilization_flag',
            '--source', 'civilization', '--origin', '658.5,662.85',
            'CivilizationFlag_301', 'CivilizationChoiceName_301', 'CivilizationLockedShade_301', 'CivilizationLockedState_301',
            '--source', 'civilization', '--origin', '1056.5,662.85', 'CivilizationChoiceButton_201=CivilizationChoiceButton_301')
    )
    CivilizationFlagSelected = @(
        , @('CutItemSpec.py', 'item_civilization_flag_selected',
            '--source', 'civilization', '--origin', '1236.5,616.85',
            'CivilizationSelectedFrame_101', 'CivilizationSelectionPulse', 'CivilizationFlag_101',
            'CivilizationChoiceName_101', 'CivilizationChoiceButton_101')
    )
}
foreach ($key in $specSteps.Keys) {
    if ($Only.Count -eq 0 -or $Only -contains $key) {
        foreach ($step in $specSteps[$key]) {
            $script, $stepArgs = $step
            & py -3.12 (Join-Path $PSScriptRoot $script) @stepArgs
            if ($LASTEXITCODE -ne 0) { throw "$script failed for $key" }
        }
    }
}
$selected = @()
foreach ($table in @($frameCommands, $pageCommands, $itemCommands)) {
    foreach ($key in $table.Keys) {
        if ($Only.Count -eq 0 -or $Only -contains $key) {
            $selected += $table[$key]
        }
    }
}
if ($selected.Count -eq 0) {
    throw "nothing selected; keys: $($frameCommands.Keys + $pageCommands.Keys + $itemCommands.Keys -join ', ')"
}
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_modal_pages -Commands ($selected -join ', ')
