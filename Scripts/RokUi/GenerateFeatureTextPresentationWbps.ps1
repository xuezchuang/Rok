# Create only the missing FeatureScreen presentation-state templates.
param([switch]$SpecsOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& py -3.12 (Join-Path $PSScriptRoot 'BuildFeatureTextPresentationSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'Feature text presentation spec validation failed.' }
if ($SpecsOnly) { return }
$pages = @(
    @('FeatureTextStates', 'feature_text_states', 'FeatureTextStatesRoot'),
    @('TaskRewardTextOnly', 'task_reward_text_only', 'TaskRewardTextOnlyRoot')
)
foreach ($page in $pages) {
    $path = Join-Path $projectRoot "Content/RokUI/Feature/WBP_RokFeature_$($page[0]).uasset"
    if (Test-Path -LiteralPath $path) { throw "Create-only generation: asset already exists: $path" }
}
$commands = @($pages | ForEach-Object {
    "Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_$($_[0]) $($_[1]) -ParentClass=/Script/Rok.RokUiPageWidget -RootName=$($_[2])"
})
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_text_presentation -Commands ($commands -join ', ')
foreach ($page in $pages) {
    $path = Join-Path $projectRoot "Content/RokUI/Feature/WBP_RokFeature_$($page[0]).uasset"
    if (!(Test-Path -LiteralPath $path)) { throw "Feature text presentation asset missing: $path" }
}
