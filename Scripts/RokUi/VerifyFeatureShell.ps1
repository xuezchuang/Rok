# Feature shell generation, visual comparisons, and rendered virtual Slate input.
param(
    [Parameter(Mandatory = $true)][ValidateSet('Generate', 'Capture', 'Input')][string]$Action,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Run = 'feature_shell',
    [switch]$WorldInput
)

$ErrorActionPreference = 'Stop'
$projectDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$saved = Join-Path $projectDir 'Saved/RokUi'
$captures = Join-Path $saved 'Captures'
$specDir = Join-Path $saved 'Specs'
$local = '-RokLocalOnly -RokNoNet'
$shellAsset = '/Game/RokUI/Feature/WBP_RokFeature_FeatureShell'
$city = '/Game/Maps/L_RokUnityGameAppCity'
$world = '/Game/Maps/L_RokUnityWorldMapPrototype'
$captureScript = Join-Path $PSScriptRoot 'Capture.ps1'
$sizes = @(@{ X = 1456; Y = 910 }, @{ X = 1920; Y = 810 })

New-Item -ItemType Directory -Force $specDir | Out-Null

function Read-ShellCapture([string]$Name, [string]$Map, [string]$Entry, [hashtable]$Size, [bool]$Legacy) {
    $spec = Join-Path $specDir "$Name.json"
    $mode = if ($Legacy) { '-RokUiLegacy' } else { '' }
    $export = ''
    if ($Map -eq $city) {
        Remove-Item -LiteralPath $spec -ErrorAction SilentlyContinue
        $export = "-RokUiExportFeatureShell -RokUiExportSpec=$spec"
    }
    & $captureScript -Name $Name -Map $Map -ResX $Size.X -ResY $Size.Y -Delay 8 `
        -Extra "$local $mode -RokOpenModal=$Entry $export" | Write-Host
    $log = Join-Path $captures "$Name.log"
    if ($Map -eq $world) {
        if (!(Select-String -LiteralPath $log -Pattern "Rok world-map HUD click: $Entry\." -Quiet)) {
            throw "The actual world $Entry action did not run. See $log"
        }
        return
    }
    if (!(Test-Path -LiteralPath $spec) -or !(Select-String -LiteralPath $log -Pattern 'Rok UI spec: wrote' -Quiet)) {
        throw "The actual $Entry route wrote no shell specification. See $log"
    }
    $result = Get-Content -LiteralPath $spec -Raw -Encoding utf8 | ConvertFrom-Json -AsHashtable
    $expectedRoot = if ($Legacy) { 'RokUnityFeatureScreenRoot' } else { 'PageRoot' }
    if ($result.label -cne 'FeatureShell' -or $result.root -cne $expectedRoot) {
        throw "$Name mounted $($result.root) instead of $expectedRoot. See $log"
    }
    $result
}

function Assert-Node([array]$Children, [string]$Name, [string]$Type) {
    $found = @($Children | Where-Object { $_.name -ceq $Name -and $_.class -ceq "/Script/UMG.$Type" })
    if ($found.Count -ne 1) { throw "Expected one $Type named $Name" }
    $found[0]
}

if ($Action -eq 'Generate') {
    $spec = Read-ShellCapture "${Run}_source" $city 'settings' $sizes[0] $true
    $null = Assert-Node $spec.nodes 'FeatureScreenDimmer' 'Border'
    $scene = Assert-Node $spec.nodes 'CommanderSceneBackground' 'Image'
    $null = Assert-Node $spec.nodes 'FeatureScreenInputBlocker' 'Button'
    $scale = Assert-Node $spec.nodes 'FeatureUnityExpandScaleBox' 'ScaleBox'
    $design = Assert-Node $scale.children 'FeatureExpandedDesignSize' 'SizeBox'
    $canvas = Assert-Node $design.children 'FeatureExpandedDesignCanvas' 'CanvasPanel'
    $modeCanvas = Assert-Node $canvas.children 'FeatureModeCanvas' 'CanvasPanel'
    $chrome = Assert-Node $canvas.children 'CommanderChrome' 'CanvasPanel'
    $modeCanvas.children = @()
    $chrome.children = @()
    $null = $modeCanvas.props.Remove('RenderTransform')
    $null = $modeCanvas.props.Remove('RenderOpacity')
    $null = $scene.props.Remove('Brush')
    $scene.props.Visibility = 'Collapsed'
    $spec.notes = @()
    $spec | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath (Join-Path $specDir 'feature_FeatureShellPage.json') -Encoding utf8
    $generated = & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name "${Run}_generate" `
        -Commands "Rok.UiWbp.GeneratePage $shellAsset feature_FeatureShellPage" -TimeoutSeconds 180
    $generated
    if (!($generated -match 'exit=0 ') -or !($generated -match [regex]::Escape("generated $shellAsset widgets=8 ")) -or
        !(Test-Path -LiteralPath (Join-Path $projectDir 'Content/RokUI/Feature/WBP_RokFeature_FeatureShell.uasset'))) {
        throw 'Feature shell generation did not save all eight widgets.'
    }
    return
}

if ($Action -eq 'Capture') {
    foreach ($size in $sizes) {
        foreach ($route in @(@{ Map = $city; Entry = 'settings' }, @{ Map = $world; Entry = 'resource_search' })) {
            $name = "$Run-$($route.Entry)-$($size.X)x$($size.Y)"
            $null = Read-ShellCapture "$name-legacy" $route.Map $route.Entry $size $true
            $null = Read-ShellCapture "$name-wbp" $route.Map $route.Entry $size $false
            Write-Output $name
            & py -3.12 (Join-Path $PSScriptRoot 'PixelDiff.py') `
                (Join-Path $captures "$name-legacy.png") (Join-Path $captures "$name-wbp.png") `
                --out (Join-Path $captures "$name-diff.png")
            if ($LASTEXITCODE -notin 0, 1) { throw "Pixel comparison failed for $name" }
        }
    }
    return
}

$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = Join-Path $projectDir 'Rok.uproject'
$testName = 'Rok.Ui.FeatureShell.RenderedInput'
$map = if ($WorldInput) { $world } else { $city }
$failed = @()
foreach ($size in $sizes) {
    foreach ($mode in 'legacy', 'wbp') {
        $outDir = Join-Path $saved "Tests/$Run-$mode-$($size.X)x$($size.Y)"
        if (Test-Path -LiteralPath $outDir) { throw "$outDir exists. Use a different -Run." }
        New-Item -ItemType Directory -Force $outDir | Out-Null
        $log = Join-Path $outDir 'run.log'
        $ueArgs = @(
            $project, $map, '-game', '-RenderOffscreen', "-ResX=$($size.X)", "-ResY=$($size.Y)", '-ForceRes',
            '-NoSound', '-NoSplash', '-NoP4', '-NoLiveCoding', '-Unattended',
            '-DisablePlugins=ModelContextProtocol', '-RokLocalOnly', '-RokNoNet',
            "-ExecCmds=""Automation RunTests $testName""", '-TestExit="Automation Test Queue Empty"',
            "-ReportExportPath=$outDir", "-abslog=$log"
        )
        if ($mode -eq 'legacy') { $ueArgs += '-RokUiLegacy' }
        if ($WorldInput) { $ueArgs += '-RokFeatureShellWorldInput' }
        $process = Start-Process -FilePath $engine -ArgumentList $ueArgs -WindowStyle Hidden -PassThru
        if (!$process.WaitForExit(180000)) {
            Stop-Process -Id $process.Id -Force
            $failed += $log
            Write-Warning "Input test timed out. See $log"
            continue
        }
        $index = Join-Path $outDir 'index.json'
        $result = @()
        if (Test-Path -LiteralPath $index) {
            $report = (Get-Content -LiteralPath $index -Raw -Encoding utf8).Trim([char]0, [char]0xFEFF) | ConvertFrom-Json
            $result = @($report.tests | Where-Object { $_.fullTestPath -ceq $testName })
        }
        if ($result.Count -ne 1 -or $result[0].state -ne 'Success' -or $process.ExitCode -ne 0) {
            $failed += $log
            Write-Warning "Input test state '$($result.state)' exit=$($process.ExitCode). See $log"
        }
        else {
            Write-Output "Success $mode $($size.X)x$($size.Y) report=$index"
        }
    }
}
if ($failed.Count) { throw "Rendered input failed. See $($failed -join ', ')" }
