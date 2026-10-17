param(
    [Parameter(Mandatory = $true)]
    [string]$Modes,

    [string]$DateTag = '20260815'
)

$editor = 'D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = 'D:\ueDemo\Rok\Rok.uproject'
$shotDir = 'D:\ueDemo\Rok\Saved\RokUiAcceptance'
$logDir = 'D:\ueDemo\Rok\Saved\Logs'

foreach ($mode in ($Modes -split ',')) {
    $mode = $mode.Trim()
    if (-not $mode) { continue }
    $log = Join-Path $logDir ("RokShot_{0}_{1}.log" -f $mode, $DateTag)
    $shot = Join-Path $shotDir ("Screen_{0}_{1}_1440x810.png" -f $mode, $DateTag)
    Remove-Item $log, $shot -Force -ErrorAction SilentlyContinue
    $args = @(
        $project,
        '-game', '-windowed', '-ForceRes', '-ResX=1440', '-ResY=810',
        '-WinX=0', '-WinY=0', '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding',
        '-DisablePlugins=McpAutomationBridge,ModelContextProtocol',
        "-RokOpenModal=$mode",
        '-RokAutoScreenshot', '-RokScreenshotDelay=6',
        "-RokScreenshotPath=$shot",
        '-RokScreenshotQuit', '-seconds=31',
        "-abslog=$log"
    )
    $p = Start-Process -FilePath $editor -ArgumentList $args -PassThru
    if (-not $p.WaitForExit(100000)) {
        Stop-Process -Id $p.Id -Force
    }
    $hasShot = Test-Path $shot
    $opened = Select-String -Path $log -Pattern 'Rok modal UI: opened' -Quiet
    Write-Output ("{0} shot={1} opened={2}" -f $mode, $hasShot, $opened)
}
