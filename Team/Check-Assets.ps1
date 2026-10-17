param([string]$EngineRoot='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'PythonRuntime.ps1')
& (Resolve-TeamPython $EngineRoot) (Join-Path $PSScriptRoot 'assets.py') check --root (Split-Path $PSScriptRoot -Parent)
if($LASTEXITCODE -ne 0){throw 'Assets are missing, changed, or from a different version.'}
