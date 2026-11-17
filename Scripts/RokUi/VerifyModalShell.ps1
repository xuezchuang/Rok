# Capture the idle native shell from the real HUD, generate the WBP, then replay input.
param(
    [Parameter(Mandatory = $true)][ValidateSet('Export', 'Generate', 'Input')][string]$Action,
    [ValidateSet('Legacy', 'Wbp')][string]$Mode = 'Wbp',
    [ValidateSet('City', 'World')][string]$Map = 'City',
    [ValidateRange(640, 3840)][int]$Width = 1440,
    [ValidateRange(480, 2160)][int]$Height = 810,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Run = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

$ErrorActionPreference = 'Stop'
$projectDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$saved = Join-Path $projectDir 'Saved/RokUi'
$spec = Join-Path $saved 'Specs/modal_ModalShell.json'
$asset = '/Game/RokUI/Modal/WBP_RokModalShell'
$package = Join-Path $projectDir 'Content/RokUI/Modal/WBP_RokModalShell.uasset'

if ($Action -eq 'Generate') {
    if (Test-Path -LiteralPath $package -PathType Leaf) {
        Write-Output "Existing asset preserved: $package"
        return
    }
    if (!(Test-Path -LiteralPath $spec -PathType Leaf)) {
        throw "Export the native shell first. Missing $spec"
    }
    $source = Get-Content -LiteralPath $spec -Raw -Encoding utf8 | ConvertFrom-Json
    if ($source.label -cne 'ModalShell' -or $source.root -cne 'RokUnityModalRoot') {
        throw "Unexpected shell source: $spec"
    }
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name 'gen_modal_shell' `
        -Commands "Rok.UiWbp.GeneratePage $asset modal_ModalShell -ParentClass=/Script/Rok.RokUnityModalWidget -RootName=RokUnityModalRoot"
    if (!(Test-Path -LiteralPath $package -PathType Leaf)) {
        throw "No shell package was generated: $package"
    }
    return
}

if ($Action -eq 'Export') {
    $Mode = 'Legacy'
    $Map = 'City'
    New-Item -ItemType Directory -Force (Split-Path $spec -Parent) | Out-Null
}

$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$scene = if ($Map -eq 'World') { '/Game/Maps/L_RokUnityWorldMapPrototype' } else { '/Game/Maps/L_RokUnityGameAppCity' }
$testName = 'Rok.Ui.ModalShell.RenderedInput'
$output = Join-Path $saved "ModalShellAudit/$Run-$Map-$Mode-${Width}x$Height"
if (Test-Path -LiteralPath $output) {
    throw "$output exists. Use a different -Run."
}
New-Item -ItemType Directory -Force $output | Out-Null
$log = Join-Path $output 'run.log'
$screenshot = Join-Path $output 'Profile.png'
$ueArgs = @(
    (Join-Path $projectDir 'Rok.uproject'), $scene, '-game', '-RenderOffscreen', "-ResX=$Width", "-ResY=$Height", '-ForceRes',
    '-UseFixedTimeStep', '-FPS=60',
    '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding', '-Unattended', '-DisablePlugins=ModelContextProtocol', '-RokLocalOnly', '-RokNoNet',
    "-ExecCmds=""Automation RunTests $testName""", '-TestExit="Automation Test Queue Empty"',
    "-ReportExportPath=""$output""", "-RokModalShellScreenshot=""$screenshot""", "-abslog=""$log"""
)
if ($Action -eq 'Export') { $ueArgs += "-RokModalShellSpec=""$spec""" }
if ($Mode -eq 'Legacy') { $ueArgs += '-RokUiLegacy' }
if ($Map -eq 'World') { $ueArgs += '-RokModalShellWorld' }

$process = Start-Process -FilePath $engine -ArgumentList $ueArgs -WorkingDirectory $projectDir -WindowStyle Hidden -PassThru
Write-Output "PID=$($process.Id) report=$output"
if (!$process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id -Force
    throw "Rendered input timed out. See $log"
}
$index = Join-Path $output 'index.json'
if (!(Test-Path -LiteralPath $index -PathType Leaf)) {
    throw "Missing rendered-input report. See $log"
}
$report = Get-Content -LiteralPath $index -Raw -Encoding utf8 | ConvertFrom-Json
$result = @($report.tests | Where-Object { $_.fullTestPath -ceq $testName })
if ($result.Count -ne 1 -or $result[0].state -ne 'Success' -or $process.ExitCode -ne 0) {
    throw "Rendered input state '$($result.state)' exit=$($process.ExitCode). See $log"
}
if ($Action -eq 'Export' -and !(Test-Path -LiteralPath $spec -PathType Leaf)) {
    throw "Missing shell specification $spec"
}
Write-Output "Success warnings=$($result[0].warnings) errors=$($result[0].errors) report=$index screenshot=$screenshot"
