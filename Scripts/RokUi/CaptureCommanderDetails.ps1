# Capture two read-only commander detail overlays through their actual buttons.
$ErrorActionPreference = 'Stop'
$specRoot = 'D:/ue/Rok/Saved/RokUi/Specs'
$states = @(
    @{Name='commander_skill_0'; Args='-RokCommanderPreviewSkill=0'},
    @{Name='commander_skill_1'; Args='-RokCommanderPreviewSkill=1'},
    @{Name='commander_talent_0'; Args='-RokCommanderPreviewTalent=0'},
    @{Name='commander_biography'; Args='-RokCommanderPreviewBiography'},
    @{Name='commander_sort'; Args='-RokCommanderPreviewSort'}
)
foreach ($state in $states) {
    & "$PSScriptRoot/Capture.ps1" -Name "$($state.Name)_legacy" -Extra "-RokLocalOnly -RokUiLegacy -RokOpenModal=commander -RokCommanderPreviewFixture -RokCommanderHero=1006 $($state.Args) -RokUiExportSpec=$specRoot/$($state.Name).json"
    if (!(Test-Path "$specRoot/$($state.Name).json")) { throw "Missing commander spec: $($state.Name)" }
}
