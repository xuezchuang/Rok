param([ValidateSet('Inspect', 'Apply', 'Verify')][string]$Mode = 'Inspect')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$project = Join-Path $root 'Rok.uproject'
if (!(Test-Path -LiteralPath $project -PathType Leaf)) { throw "Missing project: $project" }
$engine = @('D:\ue\UE_5.8', 'C:\Program Files\Epic Games\UE_5.8') | Where-Object {
    Test-Path -LiteralPath (Join-Path $_ 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
} | Select-Object -First 1
if (!$engine) { throw 'The UE 5.8 editor commandlet was not found.' }
$engineVersion = Get-Content -LiteralPath (Join-Path $engine 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if ($engineVersion.MajorVersion -ne 5 -or $engineVersion.MinorVersion -ne 8) { throw "Wrong engine version in $engine" }
$engineModules = Get-Content -LiteralPath (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.modules') -Raw | ConvertFrom-Json
$projectModules = Get-Content -LiteralPath (Join-Path $root 'Binaries/Win64/UnrealEditor.modules') -Raw | ConvertFrom-Json
if ($engineModules.BuildId -cne $projectModules.BuildId) { throw 'The existing Rok DLLs are incompatible with this UE installation.' }
if (Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" | Where-Object {
    $_.CommandLine -like "*$project*"
}) { throw "A Rok editor process is using $project" }

$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'UpdateHudResourceText.py'
$run = Join-Path $root ('Saved/RokUi/Backups/HudResourceText_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
New-Item -Path $run -ItemType Directory | Out-Null

function Invoke-ResourceTextCommandlet([string]$Phase) {
    $previousMode = $env:ROK_HUD_RESOURCE_TEXT_MODE
    try {
        $env:ROK_HUD_RESOURCE_TEXT_MODE = $Phase
        $log = Join-Path $run "$Phase.log"
        $arguments = @(('"' + $project + '"'), '-run=pythonscript', ('-script="' + $script + '"'),
            '-NullRHI', '-Unattended', '-NoSplash', '-NoSound', '-NoP4', '-NoLiveCoding', '-NoCompile',
            '-DisablePlugins=ModelContextProtocol', ('-abslog="' + $log + '"'))
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
        if (!$process.WaitForExit(90000)) {
            Stop-Process -Id $process.Id
            throw "HUD resource text $Phase timed out: $log"
        }
        $receipt = Select-String -LiteralPath $log -Pattern "ROK_HUD_RESOURCE_TEXT mode=$Phase assets=2 pending=[0-2]" | Select-Object -Last 1
        if ($process.ExitCode -ne 0 -or !$receipt) { throw "HUD resource text $Phase failed: $log" }
        Write-Output $receipt.Line
    } finally {
        $env:ROK_HUD_RESOURCE_TEXT_MODE = $previousMode
    }
}

if ($Mode -ne 'Apply') {
    Invoke-ResourceTextCommandlet $Mode
    return
}

Invoke-ResourceTextCommandlet 'Inspect'
$files = @('WBP_RokCityHud.uasset', 'WBP_RokTacticalHud.uasset') | ForEach-Object {
    $asset = Get-Item -LiteralPath (Join-Path $root "Content/RokUI/Hud/$_")
    $backup = Join-Path $run $asset.Name
    Copy-Item -LiteralPath $asset.FullName -Destination $backup
    if ((Get-FileHash -LiteralPath $asset.FullName).Hash -cne (Get-FileHash -LiteralPath $backup).Hash) {
        throw "Backup hash mismatch: $($asset.FullName)"
    }
    [pscustomobject]@{ Path = $asset.FullName; Backup = $backup; ReadOnly = $asset.IsReadOnly }
}
try {
    foreach ($file in $files) { (Get-Item -LiteralPath $file.Path).IsReadOnly = $false }
    Invoke-ResourceTextCommandlet 'Apply'
    Invoke-ResourceTextCommandlet 'Verify'
} catch {
    foreach ($file in $files) { Copy-Item -LiteralPath $file.Backup -Destination $file.Path -Force }
    throw
} finally {
    foreach ($file in $files) { (Get-Item -LiteralPath $file.Path).IsReadOnly = $file.ReadOnly }
}
Write-Output "Saved and independently verified both HUD assets. Backup: $run"
