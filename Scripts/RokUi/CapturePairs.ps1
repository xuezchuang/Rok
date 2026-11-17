# Before/after captures for the WBP conversion (gates G2 and G6): for each key,
# one off-screen capture with -RokUiLegacy (C++ build) and one without (WBPs from
# URokUiSettings), then a pixel diff. The WBP capture log must show which WBPs
# drew the screen ("Rok UI: drawing with WBP"); a pair where none did is reported
# as NO_WBP, since identical pixels would then prove nothing.
# Captures run one at a time. Writes Saved/RokUi/Captures/<Set>_summary.csv.
# Example:
#   pwsh Scripts/RokUi/CapturePairs.ps1 -Set modal_lists -Flag RokOpenModal -Keys guild_purview,vip_store
param(
    [Parameter(Mandatory = $true)][string]$Set,
    [Parameter(Mandatory = $true)][string]$Flag,
    [Parameter(Mandatory = $true)][string[]]$Keys,
    [string]$Extra = '',
    [double]$Delay = 6
)

$ErrorActionPreference = 'Stop'
$capture = Join-Path $PSScriptRoot 'Capture.ps1'
$diff = Join-Path $PSScriptRoot 'PixelDiff.py'
$outDir = 'D:\ue\Rok\Saved\RokUi\Captures'
# pwsh -File passes "a,b,c" as one string.
$Keys = @($Keys | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
# The HUD runtime-info line (RokMainHudWidget "AST mm/dd HH:MM  lod:--") shows the wall
# clock, so a minute turning between the two captures differs there. 1456x910 only.
$ignoreArgs = @('--ignore', '1100,80,1455,106')
$rows = @()
foreach ($key in $Keys) {
    $legacy = "${key}_legacy"
    $wbp = "${key}_wbp"
    $row = [ordered]@{ key = $key; result = ''; pixels = ''; wbps = ''; note = '' }
    try {
        & $capture -Name $legacy -Delay $Delay -Extra "-$Flag=$key -RokUiLegacy $Extra" | Out-Null
        & $capture -Name $wbp -Delay $Delay -Extra "-$Flag=$key $Extra" | Out-Null
        $used = @(Select-String -Path (Join-Path $outDir "$wbp.log") -Pattern 'Rok UI: drawing with WBP (\S+)' |
            ForEach-Object { ($_.Matches[0].Groups[1].Value -split '\.')[-1] })
        $failed = @(Select-String -Path (Join-Path $outDir "$wbp.log") -Pattern 'Rok UI: WBP (\S+) did not load')
        $row.wbps = ($used -join ' ')
        $output = & py -3.12 $diff (Join-Path $outDir "$legacy.png") (Join-Path $outDir "$wbp.png") --out (Join-Path $outDir "${key}_diff.png") @ignoreArgs
        $code = $LASTEXITCODE
        $row.pixels = (($output | Select-Object -First 1) -replace '^pixels over tolerance \d+: ', '')
        $row.note = (($output | Select-Object -Skip 1 | Where-Object { $_ -notlike 'heatmap:*' }) -join '; ')
        if ($failed.Count) { $row.result = 'WBP_LOAD_FAILED' }
        elseif (-not $used.Count) { $row.result = 'NO_WBP' }
        elseif ($code -eq 0) { $row.result = 'IDENTICAL' }
        elseif ($code -eq 1) { $row.result = 'DIFFERENT' }
        else { $row.result = 'NOT_COMPARABLE' }
    }
    catch {
        $row.result = 'ERROR'
        $row.note = $_.Exception.Message
    }
    $rows += [pscustomobject]$row
    Write-Output ("{0,-18} {1,-16} {2}" -f $key, $row.result, $row.pixels)
}
$summary = Join-Path $outDir "${Set}_summary.csv"
$rows | Export-Csv -Path $summary -Encoding utf8
Write-Output "summary: $summary"
