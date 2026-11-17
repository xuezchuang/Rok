# One off-screen game capture (no window): -RenderOffscreen renders UI and 3D,
# Rok's -RokAutoScreenshot writes the PNG, -RokScreenshotQuit ends the process.
# Example:
#   pwsh Scripts/RokUi/Capture.ps1 -Name civilization_fixed -Extra "-RokOpenModal=civilization -RokModalPreviewSample=0.75"
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Extra = '',
    [int]$ResX = 1456,
    [int]$ResY = 910,
    [string]$Map = '/Game/Maps/L_RokUnityGameAppCity',
    [double]$Delay = 6,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = 'D:\ue\Rok\Rok.uproject'
$outDir = 'D:\ue\Rok\Saved\RokUi\Captures'
New-Item -ItemType Directory -Force $outDir | Out-Null
$png = Join-Path $outDir "$Name.png"
$log = Join-Path $outDir "$Name.log"
Remove-Item $png, $log -ErrorAction SilentlyContinue

$arguments = @(
    $project, $Map, '-game', '-RenderOffscreen', "-ResX=$ResX", "-ResY=$ResY", '-ForceRes',
    '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding', '-Unattended',
    '-DisablePlugins=ModelContextProtocol',
    '-RokAutoScreenshot', "-RokScreenshotDelay=$Delay", "-RokScreenshotPath=$png", '-RokScreenshotQuit',
    "-abslog=$log"
) + @($Extra -split ' ' | Where-Object { $_ })

$process = Start-Process -FilePath $engine -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "capture $Name timed out after $TimeoutSeconds s (process $($process.Id) stopped)"
}
if (-not (Test-Path $png) -or (Get-Item $png).Length -lt 10000) {
    throw "capture $Name produced no usable PNG; see $log"
}
Write-Output "captured $png"
