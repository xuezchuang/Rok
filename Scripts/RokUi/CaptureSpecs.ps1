# Baseline captures plus widget specs for the WBP conversion. For each entry
# "name|game arguments", one off-screen capture (Saved/RokUi/Captures/<name>_base.png)
# that also writes the open modal's C++-built content as a widget spec
# (Saved/RokUi/Specs/<name>.json, see RokUiSpec.h). Rok.UiWbp.GeneratePage turns
# specs into a page WBP; the PNG is the "before" image the converted page must match.
# Captures run one at a time.
# Example:
#   pwsh Scripts/RokUi/CaptureSpecs.ps1 -Entries "resources|-RokOpenModal=resources","wood|-RokOpenModal=wood"
param(
    [Parameter(Mandatory = $true)][string[]]$Entries,
    [double]$Delay = 6
)

$ErrorActionPreference = 'Stop'
$capture = Join-Path $PSScriptRoot 'Capture.ps1'
$specDir = 'D:/ue/Rok/Saved/RokUi/Specs'
$captureDir = 'D:\ue\Rok\Saved\RokUi\Captures'
New-Item -ItemType Directory -Force $specDir | Out-Null
# pwsh -File passes "a,b" as one string.
$Entries = @($Entries | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
foreach ($entry in $Entries) {
    $name, $gameArgs = $entry -split '\|', 2
    $spec = "$specDir/$name.json"
    Remove-Item $spec -ErrorAction SilentlyContinue
    try {
        & $capture -Name "${name}_base" -Delay $Delay -Extra "$gameArgs -RokUiExportSpec=$spec" | Out-Null
        $log = Join-Path $captureDir "${name}_base.log"
        $written = Select-String -Path $log -Pattern 'Rok UI spec: wrote' -Quiet
        $result = if ($written -and (Test-Path $spec)) { 'ok' } else { 'NO_SPEC' }
    }
    catch {
        $result = "ERROR $($_.Exception.Message)"
    }
    Write-Output ("{0,-32} {1}" -f $name, $result)
}
