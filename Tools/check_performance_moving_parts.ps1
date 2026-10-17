param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows')
$ErrorActionPreference='Stop'
$output=Join-Path (Split-Path $PSScriptRoot -Parent) 'Docs\LightingPerformance\moving_parts'
New-Item -ItemType Directory -Path $output -Force|Out-Null
$started=Get-Date
$argsForTest=@('-HouseTest=furniture','-HouseFurnitureCycles=2','-HouseFurnitureFilter=BP_Door_Inside_A3_8|BP_Fridge_A2|BP_Oven_A2_0|BP_WorkingTable_A3_0','-windowed','-ResX=1280','-ResY=720','-ForceRes','-unattended',('-abslog='+ (Join-Path $output 'furniture.log')))
$process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $argsForTest -WindowStyle Hidden -PassThru
if(!$process.WaitForExit(240000)){Stop-Process -Id $process.Id;throw 'Moving-part check timed out'}
if($process.ExitCode -ne 0){throw 'Moving-part process failed'}
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
foreach($name in @('FurnitureAcceptance.json','FurnitureInventory.json')){
    $file=Get-Item -LiteralPath (Join-Path $saved $name)
    if($file.LastWriteTime -lt $started){throw 'No fresh moving-part evidence'}
    Copy-Item -LiteralPath $file.FullName -Destination $output
}
$result=Get-Content -Raw -LiteralPath (Join-Path $output 'FurnitureAcceptance.json')|ConvertFrom-Json
if($result.Count -ne 6 -or ($result|Where-Object { !$_.passed -or $_.cycles_completed -ne 2 })){throw 'Moving-part regression failed'}
Write-Output 'Six moving parts passed two open/close cycles each'
