# After-conversion check for page WBPs (gates G2, G6, G7): for each entry
# "name|game arguments" (the same entries as CaptureSpecs.ps1), one off-screen
# capture (Saved/RokUi/Captures/<name>_page.png) compared with the baseline that
# CaptureSpecs.ps1 took from the C++ build (<name>_base.png).
# The capture log must show which WBPs drew it ("Rok UI: drawing with WBP": a page,
# or only a window frame for modes whose body is still C++) and no page binding
# errors ("Rok UI: page ... has no ..."). Captures run one at a time.
# Writes Saved/RokUi/Captures/<Set>_summary.csv.
# Example:
#   pwsh Scripts/RokUi/CompareBase.ps1 -Set pages -Entries "resources|-RokOpenModal=resources"
param(
    [Parameter(Mandatory = $true)][string]$Set,
    [Parameter(Mandatory = $true)][string[]]$Entries,
    [double]$Delay = 6
)

$ErrorActionPreference = 'Stop'
$capture = Join-Path $PSScriptRoot 'Capture.ps1'
$diff = Join-Path $PSScriptRoot 'PixelDiff.py'
$outDir = 'D:\ue\Rok\Saved\RokUi\Captures'
# pwsh -File passes "a,b" as one string.
$Entries = @($Entries | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
# The HUD runtime-info line shows the wall clock (see CapturePairs.ps1). 1456x910 only.
$ignoreArgs = @('--ignore', '1100,80,1455,106')
$rows = @()
foreach ($entry in $Entries) {
    $name, $gameArgs = $entry -split '\|', 2
    $base = Join-Path $outDir "${name}_base.png"
    $page = "${name}_page"
    $row = [ordered]@{ name = $name; result = ''; pixels = ''; wbps = ''; errors = ''; note = '' }
    try {
        if (-not (Test-Path $base)) { throw "no baseline $base (run CaptureSpecs.ps1 first)" }
        & $capture -Name $page -Delay $Delay -Extra $gameArgs | Out-Null
        $log = Join-Path $outDir "$page.log"
        $used = @(Select-String -Path $log -Pattern 'Rok UI: drawing with WBP (\S+)' |
            ForEach-Object { ($_.Matches[0].Groups[1].Value -split '\.')[-1] })
        $errors = @(Select-String -Path $log -Pattern 'Rok UI: (page .* has no .*|WBP .* did not load.*|no page WBP.*)' |
            ForEach-Object { $_.Matches[0].Groups[1].Value })
        $row.wbps = ($used -join ' ')
        $row.errors = ($errors -join '; ')
        $output = & py -3.12 $diff $base (Join-Path $outDir "$page.png") --out (Join-Path $outDir "${name}_page_diff.png") @ignoreArgs
        $code = $LASTEXITCODE
        $row.pixels = (($output | Select-Object -First 1) -replace '^pixels over tolerance \d+: ', '')
        $row.note = (($output | Select-Object -Skip 1 | Where-Object { $_ -notlike 'heatmap:*' }) -join '; ')
        if ($errors.Count) { $row.result = 'PAGE_ERRORS' }
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
    Write-Output ("{0,-30} {1,-14} {2} {3}" -f $name, $row.result, $row.pixels, $row.errors)
}
$summary = Join-Path $outDir "${Set}_summary.csv"
$rows | Export-Csv -Path $summary -Encoding utf8
Write-Output "summary: $summary"
