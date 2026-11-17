$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$expectedProject = 'D:\ue\Rok\Rok.uproject'
$project = Join-Path $projectRoot 'Rok.uproject'
if (-not $project.Equals($expectedProject, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected Unreal project target: $project"
}
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'ImportExpeditionWbpTextures.py'
$logDir = Join-Path $projectRoot 'Saved\RokUi\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir 'import_expedition_wbp_textures.log'
Remove-Item -LiteralPath $log -ErrorAction SilentlyContinue
$arguments = @(
    $project, '-NullRHI', '-Unattended', '-NoSplash', '-NoSound', '-NoP4', '-NoLiveCoding',
    "-ExecutePythonScript=$script", "-abslog=$log"
)
$process = Start-Process -FilePath $engine -ArgumentList $arguments -PassThru -NoNewWindow
if (-not $process.WaitForExit(600000)) {
    Stop-Process -Id $process.Id -Force
    throw "Expedition texture import timed out (process $($process.Id) stopped)"
}
Select-String -Path $log -Pattern 'Imported/verified|Error:|Traceback' | ForEach-Object { $_.Line }
if ($process.ExitCode -ne 0) { throw "Expedition texture import failed: exit=$($process.ExitCode), log=$log" }
Write-Output "exit=$($process.ExitCode) log=$log"
