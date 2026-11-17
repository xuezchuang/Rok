# Compare commander details with a still background and fixed 0.75s mesh phase.
param([switch]$Legacy)
$ErrorActionPreference = 'Stop'
$states = @(
    @{Name='commander_skill_0'; Args='-RokCommanderPreviewSkill=0'},
    @{Name='commander_skill_1'; Args='-RokCommanderPreviewSkill=1'},
    @{Name='commander_talent_0'; Args='-RokCommanderPreviewTalent=0'}
)
$mode = if ($Legacy) { 'legacy' } else { 'wbp' }
$legacyFlag = if ($Legacy) { '-RokUiLegacy' } else { '' }
foreach ($state in $states) {
    & "$PSScriptRoot/Capture.ps1" -Name "$($state.Name)_static_$mode" -Extra "-RokLocalOnly $legacyFlag -RokOpenModal=commander -RokCommanderPreviewFixture -RokCommanderHero=1006 -RokCommanderStaticCapture $($state.Args)"
}
