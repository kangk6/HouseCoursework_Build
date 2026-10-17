param(
    [string]$EngineRoot = 'D:\epic\UE_5.3',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'Artifacts'),
    [switch]$SkipCook
)
$ErrorActionPreference = 'Stop'
$projectPath = Join-Path $PSScriptRoot 'HouseCoursework.uproject'
$uatPath = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if (!(Test-Path -LiteralPath $projectPath)) { throw "Project missing: $projectPath" }
if (!(Test-Path -LiteralPath $uatPath)) { throw "UE installation missing: $EngineRoot" }
# SkipCook is only for C++ changes after a successful cook; content/config or
# reflected property layout changes require the default full cook.
if($SkipCook -and !(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'Saved\Cooked\Windows\HouseCoursework'))){throw 'SkipCook requires an existing Windows cook'}
$cookFlag=if($SkipCook){'-skipcook'}else{'-cook'}
& $uatPath BuildCookRun "-project=$projectPath" '-noP4' '-platform=Win64' '-clientconfig=Development' '-build' $cookFlag '-map=/Game/HouseFurniture/Maps/Demo_01_Night' '-stage' '-pak' '-archive' "-archivedirectory=$OutputDirectory" '-ubtargs=-CompilerVersion=14.38.33130' '-unattended' '-utf8output'
if ($LASTEXITCODE -ne 0) { throw "Packaging failed, exit code: $LASTEXITCODE" }
