param(
    [switch]$Run,
    [string]$UnityEditor = 'C:\Program Files\Unity\Hub\Editor\2019.4.28f1c1\Editor\Unity.exe'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$python = 'D:\ue\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$verifier = Join-Path $PSScriptRoot 'VerifyRokUnityCityGammaMaskedSpritesNative.py'
$probe = Join-Path $repo 'Saved\UnityCityGammaMaskedSpritesProbeV2'
if (!(Test-Path -LiteralPath $python -PathType Leaf)) { throw "UE 5.8 Python missing: $python" }
if (!(Test-Path -LiteralPath $verifier -PathType Leaf)) { throw "Probe preparer missing: $verifier" }
& $python $verifier --prepare
if ($LASTEXITCODE -ne 0) { throw "Source allowlist/preparation failed ($LASTEXITCODE)" }
Write-Output "Prepared isolated Unity 2019 masked-sprite probe: $probe"
if (!$Run) { return }

if (!(Test-Path -LiteralPath $UnityEditor -PathType Leaf)) { throw "Unity 2019.4.28f1c1 missing: $UnityEditor" }
$logDir = Join-Path $repo 'Saved\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir ('UnityCityGammaMaskedSpritesNative_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '.log')
$runs = Join-Path $probe 'OutputRuns'
$existing = @{}
if (Test-Path -LiteralPath $runs) {
    Get-ChildItem -LiteralPath $runs -Directory | ForEach-Object { $existing[$_.FullName] = $true }
}
$arguments = @('-batchmode', '-force-d3d11', '-projectPath', ('"' + $probe + '"'),
    '-executeMethod', 'ExportRokUnityCityGammaMaskedSpritesNative.Run', '-logFile', ('"' + $log + '"'))
$process = Start-Process -FilePath $UnityEditor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output "Unity masked-sprite probe PID=$($process.Id) log=$log"
$process.WaitForExit()
$process.Refresh()
if ($process.ExitCode -ne 0) { throw "Unity native export failed (exit $($process.ExitCode)): $log" }
$newRuns = @(Get-ChildItem -LiteralPath $runs -Directory | Where-Object { !$existing.ContainsKey($_.FullName) })
if ($newRuns.Count -ne 1) { throw "Unity expected exactly one new output run, observed $($newRuns.Count): $log" }
& $python $verifier --run $newRuns[0].FullName
if ($LASTEXITCODE -ne 0) { throw "Native output verification failed ($LASTEXITCODE): $($newRuns[0].FullName)" }
Write-Output "Native masked-sprite export verified: $($newRuns[0].FullName)"
