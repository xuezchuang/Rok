# Runs console commands in a headless editor (UnrealEditor-Cmd -NullRHI: no window),
# then quits. -Wbp uses configured WBP pieces; the default forces legacy UI.
# Used for the WBP generator commands (Rok.UiWbp.*); -RokUiLegacy makes
# the source UI the C++ build.
# Example:
#   pwsh Scripts/RokUi/EditorCmd.ps1 -Name gen_guild_purview `
#     -Commands "Rok.UiWbp.GenerateModalPage guild_purview /Game/RokUI/_Spike/WBP_SpikeGuildPurview"
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Commands,
    [int]$TimeoutSeconds = 900,
    [switch]$Wbp
)

$ErrorActionPreference = 'Stop'
$engine = 'D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $engine -PathType Leaf)) {
    $engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
}
$project = 'D:\ue\Rok\Rok.uproject'
$outDir = 'D:\ue\Rok\Saved\RokUi\Logs'
New-Item -ItemType Directory -Force $outDir | Out-Null
$log = Join-Path $outDir "$Name.log"
Remove-Item $log -ErrorAction SilentlyContinue

# -ExecCmds splits on commas; the value is quoted because commands contain spaces.
# The editor ignores "Quit" (game only); QUIT_EDITOR closes it.
$arguments = @(
    $project, '-NullRHI', '-Unattended', '-NoSplash', '-NoSound', '-NoP4', '-NoLiveCoding', '-NoCompile',
    "-ExecCmds=""$Commands, QUIT_EDITOR""", "-abslog=$log"
)
if (!$Wbp) { $arguments += '-RokUiLegacy' }
$process = Start-Process -FilePath $engine -ArgumentList $arguments -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "editor command $Name timed out after $TimeoutSeconds s (process $($process.Id) stopped)"
}
Select-String -Path $log -Pattern 'RokUiWbp|Error:' | ForEach-Object { $_.Line }
Write-Output "exit=$($process.ExitCode) log=$log"
