param([Parameter(Mandatory=$true)][string]$Version,[Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$EngineRoot='')
$ErrorActionPreference='Stop'
if(Get-Process UnrealEditor -ErrorAction SilentlyContinue){throw 'Save and close Unreal Editor before exporting assets.'}
. (Join-Path $PSScriptRoot 'PythonRuntime.ps1')
& (Resolve-TeamPython $EngineRoot) (Join-Path $PSScriptRoot 'assets.py') export --root (Split-Path $PSScriptRoot -Parent) --version $Version --output $OutputDirectory
if($LASTEXITCODE -ne 0){throw 'Asset export failed. Do not share an incomplete pack.'}
