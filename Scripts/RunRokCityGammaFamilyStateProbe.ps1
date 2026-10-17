param(
    [switch]$Run,
    [string]$UnityEditor = 'C:\Program Files\Unity\Hub\Editor\2019.4.28f1c1\Editor\Unity.exe'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$python = 'D:\ue\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$verify = Join-Path $PSScriptRoot 'VerifyRokCityGammaFamilyStateProbe.py'
$probe = Join-Path $repo 'Saved\UnityCityGammaFamilyStateProbeV2'
if (!(Test-Path -LiteralPath $python -PathType Leaf)) { throw "UE5.8 Python missing: $python" }
& $python -X utf8 $verify --prepare
if ($LASTEXITCODE -ne 0) { throw "Isolated family-state preparation failed ($LASTEXITCODE)" }
Write-Output "Prepared isolated Unity 2019 family-state probe: $probe"
if (!$Run) { return }

if (!(Test-Path -LiteralPath $UnityEditor -PathType Leaf)) { throw "Unity 2019.4.28f1c1 missing: $UnityEditor" }
$logDir = Join-Path $repo 'Saved\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir ('UnityCityGammaFamilyState_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '.log')
$runs = Join-Path $probe 'OutputRuns'
$existing = @{}
if (Test-Path -LiteralPath $runs) {
    Get-ChildItem -LiteralPath $runs -Directory | ForEach-Object { $existing[$_.FullName] = $true }
}
$arguments = @('-batchmode', '-force-d3d11', '-projectPath', ('"' + $probe + '"'),
    '-executeMethod', 'ExportRokCityGammaFamilyStateProbe.Run', '-logFile', ('"' + $log + '"'))
$process = Start-Process -FilePath $UnityEditor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output "Unity family-state probe PID=$($process.Id) log=$log"
$process.WaitForExit()
$process.Refresh()
if ($process.ExitCode -ne 0) { throw "Unity native family-state export failed (exit $($process.ExitCode)): $log" }
$newRuns = @(Get-ChildItem -LiteralPath $runs -Directory | Where-Object { !$existing.ContainsKey($_.FullName) })
if ($newRuns.Count -ne 1) { throw "Expected exactly one new native run, observed $($newRuns.Count): $log" }
& $python -X utf8 $verify --run $newRuns[0].FullName
if ($LASTEXITCODE -ne 0) { throw "Strict family-state verification failed ($LASTEXITCODE): $($newRuns[0].FullName)" }
Write-Output "Five-family GPU state and mip readback verified: $($newRuns[0].FullName)"
