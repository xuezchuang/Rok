# Build the editable row and reward cell dimensions without rewriting the visual pages.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null

function New-LayoutBox([string]$Name, [int]$X, [int]$Y, [int]$Width, [int]$Height) {
    @{
        name = $Name
        class = '/Script/UMG.CanvasPanel'
        slot = @{
            class = '/Script/UMG.CanvasPanelSlot'
            props = @{ LayoutData = "(Offsets=(Left=$X,Top=$Y,Right=$Width,Bottom=$Height))" }
        }
    }
}

function Write-LayoutSpec([string]$Name, [string]$Root, [object[]]$Nodes) {
    @{ version = 1; label = $Name; root = $Root; nodes = $Nodes } | ConvertTo-Json -Depth 12 |
        Set-Content -Encoding UTF8 (Join-Path $specDir "$Name.json")
}

Write-LayoutSpec 'hud_list_layout' 'HudListLayout' @(
    (New-LayoutBox 'TaskRow' 0 0 196 80)
    (New-LayoutBox 'TaskRowGap' 0 80 196 4)
    (New-LayoutBox 'WorldMarchRow' 220 0 102 86)
    (New-LayoutBox 'WorldMarchViewport' 346 0 102 258)
)
Write-LayoutSpec 'world_barbarian_reward_grid' 'WorldBarbarianRewardGrid' @(
    (New-LayoutBox 'RewardCell_0' 0 0 74 74)
    (New-LayoutBox 'RewardCell_1' 79 0 74 74)
    (New-LayoutBox 'RewardCell_2' 158 0 74 74)
    (New-LayoutBox 'RewardCell_3' 0 78 74 74)
    (New-LayoutBox 'RewardCell_4' 79 78 74 74)
    (New-LayoutBox 'RewardCell_5' 158 78 74 74)
)

if (!$SpecsOnly) {
    $hud = '/Game/RokUI/Hud'
    $commands = @(
        "Rok.UiWbp.GeneratePage $hud/WBP_RokHudListLayout hud_list_layout"
        "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldBarbarianRewardGrid world_barbarian_reward_grid"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_hud_list_layout -Commands ($commands -join ', ')
}
