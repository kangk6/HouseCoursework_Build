param(
    [string]$GameRoot='D:\HouseCoursework_Team\Artifacts\ClockFoundation\Windows',
    [string]$EvidenceRoot='D:\HouseCoursework_Team\Saved\Acceptance\ClockFoundation',
    [ValidateSet('household_weather','clock','daylight','environment_regression','smart_lighting','ventilation','energy','solar','ui_transitions','daylight_sweep','dial','startup','day_cycle','drill_restore','evacuation','evacuation_upper')][string]$Mode='clock',
    [int]$Height=720,
    [string[]]$ExtraArguments=@()
)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$names=@{dial='DialAcceptance.json';startup='StartupAcceptance.json';clock='ClockAcceptance.json';daylight='DaylightAcceptance.json';environment_regression='EnvironmentRegression.json';smart_lighting='SmartLightingAcceptance.json';ventilation='VentilationAcceptance.json';energy='EnergyAcceptance.json';ui_transitions='TransitionAcceptance.json';daylight_sweep='DaylightSweep.json'}
$names.day_cycle='DayCycleAcceptance.json';$names.drill_restore='DrillRestoreAcceptance.json';$names.evacuation='EvacuationAcceptance.json';$names.evacuation_upper='EvacuationAcceptance.json'
$names.solar='SolarAcceptance.json';$names.household_weather='HouseholdWeatherAcceptance.json'
$normal='D:\HouseCoursework_Build\Windows\HouseCoursework\Saved\Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$started=Get-Date
$arguments=@(('-HouseTest='+$Mode),('-HouseLightingProfile=clock_'+[guid]::NewGuid().ToString('N')),'-HouseTypography=4','-RenderOffscreen','-d3d12','-windowed',('-ResX='+[int][Math]::Round($Height*16/9)),('-ResY='+$Height),'-ForceRes','-unattended',('-abslog='+(Join-Path $EvidenceRoot ($Mode+'_'+$Height+'.log'))))
$arguments+=$ExtraArguments
$process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
while(!$process.WaitForExit(1000)){
    if(((Get-Date)-$started).TotalSeconds -gt $(if($Mode -eq 'day_cycle'){450}else{300})){Stop-Process -Id $process.Id;throw "$Mode timed out"}
}
if($process.ExitCode -ne 0){throw "$Mode exited with $($process.ExitCode)"}
$result=Get-Item -LiteralPath (Join-Path $saved $names[$Mode])
if($result.LastWriteTime -lt $started){throw "$Mode did not create fresh evidence"}
$report=Get-Content -LiteralPath $result.FullName -Raw | ConvertFrom-Json
Copy-Item -LiteralPath $result.FullName -Destination (Join-Path $EvidenceRoot ($Mode+'_'+$Height+'.json'))
if($Mode -ne 'daylight_sweep'){
    $failed=@($report.checks|Where-Object passed -ne $true)
    if(!$report.checks.Count -or $failed.Count){throw ('Failed: '+($failed.case -join ', '))}
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Tests modified normal lighting save'}
$shots=Join-Path $saved 'Screenshots'
if(Test-Path -LiteralPath $shots){
    $out=Join-Path $EvidenceRoot ($Mode+'_'+$Height+'_screenshots')
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    Get-ChildItem -LiteralPath $shots -File | Where-Object LastWriteTime -ge $started | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $out}
    if($Mode -eq 'day_cycle'){
        $cycleOut=Join-Path $out 'DayCycle'
        New-Item -ItemType Directory -Force -Path $cycleOut | Out-Null
        Get-ChildItem -LiteralPath $cycleOut -File -Filter '*.png' | ForEach-Object {Remove-Item -LiteralPath $_.FullName}
        Get-ChildItem -LiteralPath (Join-Path $shots 'DayCycle') -File -Filter '*.png' | Where-Object LastWriteTime -ge $started | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $cycleOut}
    }
}
Write-Output ($Mode+' '+$Height+': '+$report.checks.Count+' checks; normal lighting save unchanged')
