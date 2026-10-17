param([string]$EvidenceRoot='D:\HouseCoursework\Docs\Ventilation\Final')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$prior='D:\HouseCoursework\Docs\DaylightSmoothing\Final'
foreach($f in Get-ChildItem -LiteralPath $prior -Filter '*.json'){
 if(!(Test-Path -LiteralPath (Join-Path $EvidenceRoot $f.Name))){Copy-Item -LiteralPath $f.FullName -Destination $EvidenceRoot}
}
& "$PSScriptRoot\check_ventilation.ps1" -EvidenceRoot $EvidenceRoot
& "$PSScriptRoot\check_daylight.ps1" -EvidenceRoot $EvidenceRoot
& "$PSScriptRoot\check_daylight_smoothing.ps1" -EvidenceRoot $EvidenceRoot
& 'D:\epic\UE_5.3\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' "$PSScriptRoot\summarize_daylight_smoothing.py" $EvidenceRoot
if($LASTEXITCODE -ne 0){throw 'Daylight smoothing validation failed'}
& "$PSScriptRoot\check_energy.ps1" -EvidenceRoot $EvidenceRoot
& "$PSScriptRoot\check_fixture_interaction.ps1" -EvidenceRoot $EvidenceRoot
& "$PSScriptRoot\check_evacuation.ps1" -EvidenceRoot $EvidenceRoot
& "$PSScriptRoot\run_tab_acceptance.ps1" -EvidenceRoot $EvidenceRoot
& "$PSScriptRoot\run_base_acceptance.ps1" -EvidenceRoot $EvidenceRoot -Modes route
Write-Output 'Ventilation and affected feature regression checks passed'
