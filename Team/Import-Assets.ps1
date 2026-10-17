param([Parameter(Mandatory=$true)][string]$PackageDirectory,[string]$EngineRoot='')
$ErrorActionPreference='Stop'
if(Get-Process UnrealEditor -ErrorAction SilentlyContinue){throw 'Close Unreal Editor before importing assets.'}
. (Join-Path $PSScriptRoot 'PythonRuntime.ps1')
$python=Resolve-TeamPython $EngineRoot
& $python (Join-Path $PSScriptRoot 'assets.py') import --root (Split-Path $PSScriptRoot -Parent) --packages $PackageDirectory
if($LASTEXITCODE -ne 0){throw 'Asset import failed. Read the message above; local edits were not silently overwritten.'}
