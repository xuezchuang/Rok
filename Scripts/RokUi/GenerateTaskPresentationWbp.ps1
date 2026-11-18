# Create the task style template without regenerating existing rows or pages.
param([switch]$SpecsOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& py -3.12 (Join-Path $PSScriptRoot 'BuildTaskPresentationWbpSpec.py')
if ($LASTEXITCODE -ne 0) { throw 'Task presentation spec validation failed.' }
if ($SpecsOnly) { return }
$path = Join-Path $projectRoot 'Content/RokUI/Styles/WBP_RokTaskPresentation.uasset'
if (Test-Path -LiteralPath $path) { throw "Create-only generation: asset already exists: $path" }
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_task_presentation -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Styles/WBP_RokTaskPresentation task_presentation -ParentClass=/Script/Rok.RokUiPageWidget -RootName=TaskPresentationRoot'
if (!(Test-Path -LiteralPath $path)) { throw "Task presentation asset missing: $path" }
