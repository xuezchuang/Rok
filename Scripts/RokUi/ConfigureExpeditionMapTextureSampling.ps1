param([int]$TimeoutSeconds = 600)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$expectedProject = 'D:\ue\Rok\Rok.uproject'
$project = Join-Path $projectRoot 'Rok.uproject'
if (-not $project.Equals($expectedProject, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected Unreal project target: $project"
}

$assetDir = (Resolve-Path (Join-Path $projectRoot 'Content/RokPrototype/UI/ExpeditionSource/Art')).Path
$expectedAssetDir = Join-Path $projectRoot 'Content\RokPrototype\UI\ExpeditionSource\Art'
if (-not $assetDir.Equals($expectedAssetDir, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected Expedition asset directory: $assetDir"
}

$assetNames = @('img_bg_battle_map1', 'img_bg_battle_map2', 'img_bg_battle_map3', 'selected_glow', 'img_bat_first')
$packages = @()
foreach ($name in $assetNames) {
    $package = @(Get-ChildItem -LiteralPath $assetDir -File | Where-Object {
        $_.BaseName -eq $name -and $_.Extension -in @('.uasset', '.uexp', '.ubulk', '.uptnl')
    })
    if (-not ($package | Where-Object Extension -eq '.uasset')) {
        throw "Expected generated Expedition map asset is missing: $name.uasset"
    }
    foreach ($file in $package) {
        if (-not $file.FullName.StartsWith($assetDir + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to back up an asset outside the verified directory: $($file.FullName)"
        }
    }
    $packages += $package
}

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$backupDir = Join-Path $projectRoot "Saved\RokUi\Backups\ExpeditionMapSamplingBeforeExperiment_$stamp"
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
$manifest = foreach ($file in $packages) {
    $destination = Join-Path $backupDir $file.Name
    Copy-Item -LiteralPath $file.FullName -Destination $destination
    $sourceHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    $backupHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    if ($sourceHash -ne $backupHash) { throw "Backup hash mismatch for $($file.FullName)" }
    [pscustomobject]@{
        asset = $file.Name
        source = $file.FullName
        backup = $destination
        length = $file.Length
        source_sha256 = $sourceHash
        backup_sha256 = $backupHash
    }
}
$manifest | Export-Csv -LiteralPath (Join-Path $backupDir 'manifest.csv') -NoTypeInformation -Encoding utf8
Write-Output "Backed up Expedition texture packages to $backupDir"

$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $engine)) { throw "UE 5.8.4 commandlet is missing: $engine" }
$activeRokProcesses = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') -and $_.CommandLine -like '*D:\ue\Rok\Rok.uproject*'
})
if ($activeRokProcesses.Count -gt 0) {
    throw "Rok Unreal process is already active; refusing to overlap the asset operation."
}

$script = Join-Path $PSScriptRoot 'ConfigureExpeditionMapTextureSampling.py'
$logDir = Join-Path $projectRoot 'Saved\RokUi\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir 'configure_expedition_map_texture_sampling.log'
Remove-Item -LiteralPath $log -ErrorAction SilentlyContinue
$arguments = @(
    $project, '-NullRHI', '-Unattended', '-NoSplash', '-NoSound', '-NoP4', '-NoLiveCoding',
    '-DisablePlugins=ModelContextProtocol', "-ExecutePythonScript=$script", "-abslog=$log"
)
$process = Start-Process -FilePath $engine -ArgumentList $arguments -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "Map texture sampling configuration timed out (process $($process.Id) stopped). Backup: $backupDir"
}
Select-String -Path $log -Pattern 'Expedition texture configured|Error:|Traceback' | ForEach-Object { $_.Line }
if ($process.ExitCode -ne 0) {
    throw "Map texture sampling configuration failed: exit=$($process.ExitCode), backup=$backupDir, log=$log"
}
Write-Output "exit=$($process.ExitCode) backup=$backupDir log=$log"
