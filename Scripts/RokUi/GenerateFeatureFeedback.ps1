param([switch]$ReuseSource, [switch]$PrepareOnly)

$ErrorActionPreference = 'Stop'
$specDir = 'D:\ue\Rok\Saved\RokUi\Specs'
$source = Join-Path $specDir 'feature_feedback_source.json'
$asset = '/Game/RokUI/Feature/WBP_RokFeature_FeatureFeedback'
New-Item -ItemType Directory -Force $specDir | Out-Null
if (!$ReuseSource) {
    Remove-Item -LiteralPath $source -ErrorAction SilentlyContinue
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name 'feature_feedback_source' -Delay 6 `
        -Extra "-RokUiLegacy -RokLocalOnly -RokNoNet -RokOpenModal=commander -RokCommanderPreviewFixture -RokUiExportSpec=$source"
}

$raw = Get-Content -LiteralPath $source -Raw -Encoding utf8 | ConvertFrom-Json -AsHashtable
$labels = @($raw.nodes | Where-Object name -CEQ 'FeatureBoundaryFeedback')
if ($labels.Count -ne 1 -or $labels[0].class -ne '/Script/UMG.TextBlock') {
    throw 'The legacy commander spec does not contain the common feedback text.'
}
$label = $labels[0]
function New-FeedbackSlot($X, $Y, $Width, $Height) {
    return @{
        class = '/Script/UMG.CanvasPanelSlot'
        props = @{
            LayoutData = "(Offsets=(Left=$X,Top=$Y,Right=$Width,Bottom=$Height))"
        }
    }
}
$label.slot = New-FeedbackSlot 360 778 720 28
$label.props.Visibility = 'Collapsed'
$layouts = [ordered]@{
    ChatLayout = @(350, 692, 645, 28)
    ChatEmojiLayout = @(350, 432, 645, 28)
    CommanderLayout = @(360, 728, 720, 28)
    ExpeditionLayout = @(360, 752, 720, 28)
    WorldPositionSearchLayout = @(560, 385, 320, 40)
    ResourceSearchLayout = @(420, 332, 600, 40)
}
$nodes = @($label)
foreach ($entry in $layouts.GetEnumerator()) {
    $geometry = $entry.Value
    $nodes += @{
        name = $entry.Key
        class = '/Script/UMG.CanvasPanel'
        props = @{ Visibility = 'Collapsed' }
        slot = New-FeedbackSlot @geometry
        children = @()
    }
}
$spec = @{ version = $raw.version; label = 'FeatureFeedback'; root = 'PageRoot'; nodes = $nodes; notes = @() }
$spec | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $specDir 'feature_FeatureFeedbackPage.json') -Encoding utf8
if ($PrepareOnly) { return }
$report = & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name 'gen_feature_feedback' `
    -Commands "Rok.UiWbp.GeneratePage $asset feature_FeatureFeedbackPage"
$report
if (!($report -match '^exit=0 ') -or !($report -match [regex]::Escape("generated $asset widgets=7 "))) {
    throw 'The editor did not save all seven feedback widgets.'
}
