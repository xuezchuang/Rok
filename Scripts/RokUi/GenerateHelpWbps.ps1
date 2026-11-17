# Create the editable Help screen and its reusable dynamic rows from source-derived specs.
param([switch]$Recreate)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specBuilder = Join-Path $PSScriptRoot 'BuildHelpWbpSpecs.py'
& py -3.12 $specBuilder
if ($LASTEXITCODE -ne 0) { throw 'BuildHelpWbpSpecs.py failed' }

$assets = @(
    'Content/RokUI/Help/WBP_RokHelpScreen.uasset',
    'Content/RokUI/Help/WBP_RokHelpGroupRow.uasset',
    'Content/RokUI/Help/WBP_RokHelpQuestionRow.uasset'
)
$existing = @($assets | ForEach-Object { Join-Path $projectRoot $_ } | Where-Object { Test-Path -LiteralPath $_ })
if ($existing.Count -gt 0) {
    if (-not $Recreate) { throw "Refusing to overwrite existing WBP: $($existing -join ', ')" }
    $assetDir = (Resolve-Path (Join-Path $projectRoot 'Content/RokUI/Help')).Path
    $expectedDir = Join-Path $projectRoot 'Content\RokUI\Help'
    if (-not $assetDir.Equals($expectedDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected Help WBP target directory: $assetDir"
    }
    foreach ($path in $existing) {
        if (-not $path.StartsWith($assetDir + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove WBP outside the verified Help directory: $path"
        }
    }
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $backupDir = Join-Path $projectRoot "Saved\RokUi\Backups\HelpWbpBeforeSlotFix_$stamp"
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    foreach ($path in $existing) {
        Copy-Item -LiteralPath $path -Destination $backupDir
    }
    foreach ($path in $existing) {
        Remove-Item -LiteralPath $path
    }
    Write-Output "Backed up previous generated assets to $backupDir"
}

$commands = @(
    'Rok.UiWbp.GeneratePage /Game/RokUI/Help/WBP_RokHelpScreen help_screen -ParentClass=/Script/Rok.RokHelpWidget -RootName=HelpRoot',
    'Rok.UiWbp.GeneratePage /Game/RokUI/Help/WBP_RokHelpGroupRow help_group_row -ParentClass=/Script/UMG.UserWidget -RootName=HelpGroupRowRoot',
    'Rok.UiWbp.GeneratePage /Game/RokUI/Help/WBP_RokHelpQuestionRow help_question_row -ParentClass=/Script/UMG.UserWidget -RootName=HelpQuestionRowRoot'
) -join ', '

& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_help_wbps -Commands $commands
