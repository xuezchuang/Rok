param(
    [Parameter(Mandatory = $true)]
    [string]$Map,

    [Parameter(Mandatory = $true)]
    [int]$ClientX,

    [Parameter(Mandatory = $true)]
    [int]$ClientY,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedLogMarker,

    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [Parameter(Mandatory = $true)]
    [string]$ScreenshotPath,

    [string]$ReadyLogMarker = 'HUD added to viewport with GameAndUI focus',
    [int]$SecondClientX = -1,
    [int]$SecondClientY = -1,
    [string]$ExpectedSecondLogMarker = '',
    [int]$ScreenshotDelaySeconds = 15,
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
$editor = 'D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\Rok.uproject')).Path
$resolvedLog = [System.IO.Path]::GetFullPath($LogPath)
$resolvedScreenshot = [System.IO.Path]::GetFullPath($ScreenshotPath)

New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($resolvedLog)) | Out-Null
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($resolvedScreenshot)) | Out-Null
Remove-Item -LiteralPath $resolvedLog, $resolvedScreenshot -Force -ErrorAction SilentlyContinue

$arguments = @(
    $project,
    $Map,
    '-game',
    '-windowed',
    '-ForceRes',
    '-ResX=1440',
    '-ResY=810',
    '-WinX=0',
    '-WinY=0',
    '-NoSound',
    '-NoSplash',
    '-NoP4',
    '-NoLiveCoding',
    '-DisablePlugins=McpAutomationBridge,ModelContextProtocol',
    '-RokAutoScreenshot',
    "-RokScreenshotDelay=$ScreenshotDelaySeconds",
    "-RokScreenshotPath=$resolvedScreenshot",
    '-RokScreenshotQuit',
    "-seconds=$($ScreenshotDelaySeconds + 25)",
    "-abslog=$resolvedLog"
)

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class RokPhysicalHudClick
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct Point { public int X, Y; }

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr window, ref Point point);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);
}
'@

$process = Start-Process -FilePath $editor -ArgumentList $arguments -PassThru
try {
    $readyDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $ready = $false
    while ([DateTime]::UtcNow -lt $readyDeadline -and -not $process.HasExited) {
        $process.Refresh()
        if ($process.MainWindowHandle -ne [IntPtr]::Zero -and (Test-Path -LiteralPath $resolvedLog)) {
            $logText = Get-Content -LiteralPath $resolvedLog -Raw -ErrorAction SilentlyContinue
            if ($logText.Contains($ReadyLogMarker)) {
                $ready = $true
                break
            }
        }
        Start-Sleep -Milliseconds 250
    }
    if (-not $ready) {
        throw "Rok HUD/window did not become ready for process $($process.Id)."
    }

    Start-Sleep -Milliseconds 1500
    $process.Refresh()
    $window = $process.MainWindowHandle
    $rect = New-Object RokPhysicalHudClick+Rect
    if (-not [RokPhysicalHudClick]::GetClientRect($window, [ref]$rect)) {
        throw 'GetClientRect failed.'
    }
    $clientWidth = $rect.Right - $rect.Left
    $clientHeight = $rect.Bottom - $rect.Top
    if ($clientWidth -ne 1440 -or $clientHeight -ne 810) {
        throw "Unexpected Rok client size ${clientWidth}x${clientHeight}."
    }
    if ($ClientX -lt 0 -or $ClientX -ge $clientWidth -or $ClientY -lt 0 -or $ClientY -ge $clientHeight) {
        throw "Requested click ($ClientX,$ClientY) is outside the Rok client."
    }

    $origin = New-Object RokPhysicalHudClick+Point
    $origin.X = 0
    $origin.Y = 0
    if (-not [RokPhysicalHudClick]::ClientToScreen($window, [ref]$origin)) {
        throw 'ClientToScreen failed.'
    }

    [RokPhysicalHudClick]::SetForegroundWindow($window) | Out-Null
    [RokPhysicalHudClick]::BringWindowToTop($window) | Out-Null
    Start-Sleep -Milliseconds 400
    [RokPhysicalHudClick]::SetCursorPos($origin.X + $ClientX, $origin.Y + $ClientY) | Out-Null
    [RokPhysicalHudClick]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 90
    [RokPhysicalHudClick]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)

    $clickDeadline = [DateTime]::UtcNow.AddSeconds(8)
    $clicked = $false
    while ([DateTime]::UtcNow -lt $clickDeadline -and -not $process.HasExited) {
        $logText = Get-Content -LiteralPath $resolvedLog -Raw -ErrorAction SilentlyContinue
        if ($logText.Contains($ExpectedLogMarker)) {
            $clicked = $true
            break
        }
        Start-Sleep -Milliseconds 200
    }
    if (-not $clicked) {
        throw "Physical click marker was not observed: $ExpectedLogMarker"
    }

    $secondClicked = $false
    if ($SecondClientX -ge 0 -or $SecondClientY -ge 0 -or -not [string]::IsNullOrWhiteSpace($ExpectedSecondLogMarker)) {
        if ($SecondClientX -lt 0 -or $SecondClientX -ge $clientWidth -or $SecondClientY -lt 0 -or $SecondClientY -ge $clientHeight) {
            throw "Requested second click ($SecondClientX,$SecondClientY) is outside the Rok client."
        }
        if ([string]::IsNullOrWhiteSpace($ExpectedSecondLogMarker)) {
            throw 'ExpectedSecondLogMarker is required when a second click is requested.'
        }

        [RokPhysicalHudClick]::SetForegroundWindow($window) | Out-Null
        [RokPhysicalHudClick]::BringWindowToTop($window) | Out-Null
        Start-Sleep -Milliseconds 400
        [RokPhysicalHudClick]::SetCursorPos($origin.X + $SecondClientX, $origin.Y + $SecondClientY) | Out-Null
        [RokPhysicalHudClick]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 90
        [RokPhysicalHudClick]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)

        $secondClickDeadline = [DateTime]::UtcNow.AddSeconds(8)
        while ([DateTime]::UtcNow -lt $secondClickDeadline -and -not $process.HasExited) {
            $logText = Get-Content -LiteralPath $resolvedLog -Raw -ErrorAction SilentlyContinue
            if ($logText.Contains($ExpectedSecondLogMarker)) {
                $secondClicked = $true
                break
            }
            Start-Sleep -Milliseconds 200
        }
        if (-not $secondClicked) {
            throw "Second physical click marker was not observed: $ExpectedSecondLogMarker"
        }
    }

    if (-not $process.WaitForExit(($ScreenshotDelaySeconds + 25) * 1000)) {
        throw 'Rok standalone did not quit after its screenshot deadline.'
    }
    if (-not (Test-Path -LiteralPath $resolvedScreenshot)) {
        throw "Screenshot was not created: $resolvedScreenshot"
    }
    $screenshot = Get-Item -LiteralPath $resolvedScreenshot
    Write-Output "PID=$($process.Id) client=${clientWidth}x${clientHeight} click=($ClientX,$ClientY) marker=1 second_marker=$([int]$secondClicked) screenshot=$($screenshot.FullName) bytes=$($screenshot.Length) exit=$($process.ExitCode)"
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
}
