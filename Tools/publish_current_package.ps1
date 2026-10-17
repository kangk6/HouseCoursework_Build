param([Parameter(Mandatory=$true)][string]$Version,
      [string]$Candidate='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='', [float]$MinimumFPS=25,[switch]$RequireLightingPersistence,[switch]$RequireSafety,[switch]$RequireEvacuation,[switch]$RequireFireLocomotion,[switch]$RequireFixtureInteraction,[switch]$RequireEnergyMonitoring,[switch]$RequireDaylight,[switch]$RequireDaylightSmoothing,[switch]$RequireVentilation,[switch]$RequireVentilationLightFix,[switch]$RequireHallLighting,[switch]$RequireMenuTransitions,[switch]$RequireSmartLighting)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$destination='D:\HouseCoursework_Build\Windows'
$evidence=if($EvidenceRoot){$EvidenceRoot}else{Join-Path $repo 'Docs\WholeHouseLighting'}
if((Resolve-Path -LiteralPath $Candidate).Path -ne 'D:\HouseCoursework_TabFinal\Windows'){throw 'Unexpected candidate directory'}
if(Get-Process HouseCoursework -ErrorAction SilentlyContinue){throw 'Close running house demos before publishing'}
$checks=Get-Content -LiteralPath (Join-Path $evidence 'summary.json') -Raw|ConvertFrom-Json
$route=Get-Content -LiteralPath (Join-Path $evidence 'ContinuousRouteAcceptance.json') -Raw|ConvertFrom-Json
if($checks.lighting -ne 45 -or $checks.control_pointer_720 -lt 25 -or $checks.control_pointer_1080 -lt 25 -or !$checks.packaged_fonts_verified){throw 'Acceptance evidence missing'}
$persistence=$null
if($RequireLightingPersistence){
    $persistence=Get-Content -Raw -LiteralPath (Join-Path $evidence 'persistence_summary.json')|ConvertFrom-Json
    if(!$persistence.passed -or !$persistence.user_save_unchanged -or $persistence.phases -ne 9 -or $checks.control_pointer_720 -ne 30 -or $checks.control_pointer_1080 -ne 30){throw 'Lighting persistence/control acceptance failed'}
}
$safety=$null
if($RequireSafety){
    $safety=Get-Content -Raw -LiteralPath (Join-Path $evidence 'safety_summary.json')|ConvertFrom-Json
    if(!$safety.passed -or !$safety.visual_reviewed -or !$safety.normal_lighting_save_unchanged -or $safety.runs.Count -ne 2 -or ($safety.runs|Where-Object { !$_.passed -or $_.checks -lt 16 })){throw 'Safety acceptance failed or screenshots not reviewed'}
}
$evacuation=$null
if($RequireEvacuation){
    $evacuation=Get-Content -Raw -LiteralPath (Join-Path $evidence 'evacuation_summary.json')|ConvertFrom-Json
    if(!$evacuation.passed -or !$evacuation.visual_reviewed -or !$evacuation.normal_lighting_save_unchanged -or $evacuation.runs.Count -ne 3 -or ($evacuation.runs|Where-Object {!$_.passed -or $_.checks -lt 19 -or $_.average_fps -lt $MinimumFPS})){throw 'Evacuation acceptance failed or screenshots not reviewed'}
}
$energy=$null
if($RequireEnergyMonitoring){
    $energy=Get-Content -Raw -LiteralPath (Join-Path $evidence 'energy_summary.json')|ConvertFrom-Json
    if(!$energy.passed -or !$energy.visual_reviewed -or !$energy.user_save_unchanged -or $energy.runs.Count -ne 2){throw 'Energy acceptance evidence incomplete'}
    foreach($height in @(720,1080)){
        $report=Get-Content -Raw -LiteralPath (Join-Path $evidence ('energy_'+$height+'.json'))|ConvertFrom-Json
        if($report.checks.Count -lt 30 -or ($report.checks|Where-Object passed -ne $true)){throw 'Energy acceptance checks failed'}
    }
    if($energy.runs|Where-Object { !$_.quit_preview_restored }){throw 'Energy preview quit failed to restore original settings'}
}
$daylight=$null
if($RequireDaylight){
    $daylight=Get-Content -Raw -LiteralPath (Join-Path $evidence 'daylight_summary.json')|ConvertFrom-Json
    if(!$daylight.passed -or !$daylight.visual_reviewed -or !$daylight.user_save_unchanged -or $daylight.runs.Count -ne 2){throw 'Daylight acceptance incomplete'}
    foreach($height in @(720,1080)){
        $report=Get-Content -Raw -LiteralPath (Join-Path $evidence ('daylight_'+$height+'.json'))|ConvertFrom-Json
        if($report.checks.Count -lt 30 -or ($report.checks|Where-Object passed -ne $true)){throw 'Daylight acceptance failed'}
    }
}
$smoothing=$null
if($RequireDaylightSmoothing){
    if(!$RequireDaylight){throw 'Smoothing release requires daylight acceptance'}
    $smoothing=Get-Content -Raw -LiteralPath (Join-Path $evidence 'smoothing_summary.json')|ConvertFrom-Json
    $sweepEnvironment=Get-Content -Raw -LiteralPath (Join-Path $evidence 'sweep_environment.json')|ConvertFrom-Json
    if(!$smoothing.passed -or !$smoothing.visual_reviewed -or $smoothing.runs.Count -ne 2 -or ($smoothing.runs|Where-Object passed -ne $true) -or !$sweepEnvironment.user_save_unchanged){throw 'Continuous daylight smoothing acceptance incomplete'}
}
$ventilation=$null
if($RequireVentilation){
    $ventilation=Get-Content -Raw -LiteralPath (Join-Path $evidence 'ventilation_summary.json')|ConvertFrom-Json
    if(!$ventilation.passed -or !$ventilation.visual_reviewed -or !$ventilation.user_save_unchanged -or $ventilation.runs.Count -ne 2){throw 'Ventilation acceptance incomplete'}
    foreach($height in @(720,1080)){
        $report=Get-Content -Raw -LiteralPath (Join-Path $evidence ('ventilation_'+$height+'.json'))|ConvertFrom-Json
        $performance=Get-Content -Raw -LiteralPath (Join-Path $evidence ('performance_'+$height+'.json'))|ConvertFrom-Json
        if($report.checks.Count -lt 100 -or ($report.checks|Where-Object passed -ne $true) -or $performance.runs.Count -ne 2 -or ($performance.runs|Where-Object {!$_.passed -or $_.average_fps -lt $MinimumFPS})){throw 'Ventilation interaction or rendering acceptance failed'}
    }
}
$fireLocomotion=$null
$fixtureInteraction=$null
if($RequireFixtureInteraction){
    $fixtureInteraction=Get-Content -Raw -LiteralPath (Join-Path $evidence 'fixture_summary.json')|ConvertFrom-Json
    if(!$fixtureInteraction.passed -or !$fixtureInteraction.visual_reviewed -or !$fixtureInteraction.user_save_unchanged -or $fixtureInteraction.runs.Count -ne 2){throw 'Fixture interaction evidence incomplete'}
    foreach($height in @(720,1080)){
        $report=Get-Content -Raw -LiteralPath (Join-Path $evidence ('fixture_'+$height+'.json'))|ConvertFrom-Json
        if($report.checks.Count -lt 200 -or ($report.checks|Where-Object passed -ne $true)){throw 'Fixture interaction checks failed'}
    }
}
if($RequireFireLocomotion){
    if(!$RequireEvacuation){throw 'Fire/locomotion release also requires evacuation gate'}
    foreach($mode in @('ground','upper','blocked')){
        $report=Get-Content -Raw -LiteralPath (Join-Path $evidence ($mode+'.json'))|ConvertFrom-Json
        foreach($name in @('visible_fire_and_local_light','real_locomotion_animation_seen','locomotion_hip_bob_under_14cm','reset_clears_fire_and_light')){
            $check=@($report.checks|Where-Object { $_.case -eq $name -and $_.passed })
            if($check.Count -ne 1){throw ('Fire/locomotion check missing: '+$mode+' '+$name)}
        }
    }
    $fireLocomotion=@{passed=$true;animation='Harper brisk walk with idle/speed blend';fire='Cooked kitchen flame, progressive smoke and local flickering light'}
}
$ventilationLightFix=$null
if($RequireVentilationLightFix){
    if(!$RequireVentilation -or !$RequireFixtureInteraction){throw 'Bugfix release requires ventilation and fixture gates'}
    $ventilationLightFix=Get-Content -Raw -LiteralPath (Join-Path $evidence 'bugfix_summary.json')|ConvertFrom-Json
    $physics=Get-Content -Raw -LiteralPath (Join-Path $evidence 'airflow_physics.json')|ConvertFrom-Json
    $desk=Get-Content -Raw -LiteralPath (Join-Path $evidence 'desk_visual_summary.json')|ConvertFrom-Json
    $binary=Join-Path $Candidate 'HouseCoursework\Binaries\Win64\HouseCoursework.exe'
    if(!$ventilationLightFix.passed -or !$ventilationLightFix.visual_reviewed -or !$ventilationLightFix.fresh_current_binary -or !$physics.passed -or !$desk.passed -or !$desk.visual_reviewed -or $desk.renderer -ne 'D3D12' -or (Get-FileHash -LiteralPath $binary).Hash -ne $ventilationLightFix.candidate_executable_sha256){throw 'Desk/airflow bugfix evidence incomplete or binary changed'}
}
if(!$route.completed -or $route.average_fps -lt $MinimumFPS -or ($route.stops|Where-Object passed -ne $true)){throw 'Route/performance gate failed'}
$hallLighting=$null
if($RequireHallLighting){
    if(!$RequireFixtureInteraction){throw 'Hall lighting release requires whole-house fixture gate'}
    $hallLighting=Get-Content -Raw -LiteralPath (Join-Path $evidence 'hall_visual_summary.json')|ConvertFrom-Json
    if(!$hallLighting.passed -or !$hallLighting.visual_reviewed -or $hallLighting.fixtures.Count -ne 4 -or ($hallLighting.fixtures|Where-Object passed -ne $true)){throw 'Hall isolated lighting evidence incomplete'}
    if((Get-FileHash -LiteralPath (Join-Path $Candidate 'HouseCoursework\Binaries\Win64\HouseCoursework.exe')).Hash -ne $hallLighting.candidate_executable_sha256){throw 'Hall evidence binary changed'}
    foreach($asset in $hallLighting.package_assets){if((Get-FileHash -LiteralPath (Join-Path $Candidate $asset.path)).Hash -ne $asset.sha256){throw 'Hall evidence package changed'}}
    if(!$hallLighting.package_assets.Count){throw 'Hall evidence package fingerprint missing'}
}
$commit=(& git -C $repo rev-parse HEAD).Trim()
$smartLighting=$null
if($RequireSmartLighting){
 $smartLighting=Get-Content -LiteralPath (Join-Path $evidence 'smart_summary.json') -Raw|ConvertFrom-Json
 if(!$smartLighting.passed -or !$smartLighting.visual_reviewed -or !$smartLighting.user_save_unchanged -or $smartLighting.runs.Count -ne 2 -or ($smartLighting.runs|Where-Object {$_.checks -lt 40 -or !$_.passed})){throw 'Smart lighting evidence incomplete'}
 if((Get-FileHash -LiteralPath (Join-Path $Candidate 'HouseCoursework\Binaries\Win64\HouseCoursework.exe')).Hash -ne $smartLighting.candidate_executable_sha256){throw 'Smart lighting evidence binary changed'}
 $nightRoute=Get-Content -LiteralPath (Join-Path $evidence 'NightRoute\ContinuousRouteAcceptance.json') -Raw|ConvertFrom-Json
 $nightEnvironment=Get-Content -LiteralPath (Join-Path $evidence 'NightRoute\route_environment.json') -Raw|ConvertFrom-Json
 if(!$nightEnvironment.night_lighting -or !$nightRoute.completed -or $nightRoute.mid_route_teleports -ne 0 -or $nightRoute.stops.Count -ne 47 -or ($nightRoute.stops|Where-Object passed -ne $true) -or $nightRoute.average_fps -lt $MinimumFPS){throw 'Smart night walking route incomplete'}
 if((Get-FileHash -LiteralPath (Join-Path $Candidate 'HouseCoursework\Binaries\Win64\HouseCoursework.exe')).Hash -ne $nightEnvironment.candidate_executable_sha256){throw 'Night route evidence binary changed'}
}
$menuTransitions=$null
if($RequireMenuTransitions){
    $menuTransitions=Get-Content -Raw -LiteralPath (Join-Path $evidence 'transitions_summary.json')|ConvertFrom-Json
    if(!$menuTransitions.passed -or !$menuTransitions.visual_reviewed -or !$menuTransitions.user_save_unchanged -or $menuTransitions.runs.Count -ne 3){throw 'Menu transition evidence incomplete'}
    if((Get-FileHash -LiteralPath (Join-Path $Candidate 'HouseCoursework\Binaries\Win64\HouseCoursework.exe')).Hash -ne $menuTransitions.candidate_executable_sha256){throw 'Menu evidence binary changed'}
    foreach($height in @(720,1080,1200)){
        $report=Get-Content -Raw -LiteralPath (Join-Path $evidence ('transitions_'+$height+'.json'))|ConvertFrom-Json
        if($report.checks.Count -ne 17 -or ($report.checks|Where-Object passed -ne $true)){throw 'Menu transition checks failed'}
    }
}
$tagCommit=(& git -C $repo rev-parse $Version).Trim()
if($LASTEXITCODE -ne 0 -or $commit -ne $tagCommit){throw 'Tag does not identify current source'}
if(& git -C $repo status --porcelain){throw 'Commit the verified source and evidence before publishing'}
$normalSave=Join-Path $destination 'HouseCoursework\Saved\Lighting\room_lighting.json'
$normalSaveBefore=if(Test-Path -LiteralPath $normalSave){(Get-FileHash -LiteralPath $normalSave).Hash}else{'missing'}
New-Item -ItemType Directory -Path $destination -Force|Out-Null
foreach($item in Get-ChildItem -LiteralPath $Candidate){
    if($item.Name -eq 'HouseCoursework'){
        $projectDestination=Join-Path $destination 'HouseCoursework'
        New-Item -ItemType Directory -Path $projectDestination -Force|Out-Null
        foreach($child in Get-ChildItem -LiteralPath $item.FullName){
            if($child.Name -ne 'Saved'){Copy-Item -LiteralPath $child.FullName -Destination $projectDestination -Recurse -Force}
        }
    }else{Copy-Item -LiteralPath $item.FullName -Destination $destination -Recurse -Force}
}
$payload=@()
foreach($file in Get-ChildItem -LiteralPath $Candidate -File -Recurse){
    $relative=$file.FullName.Substring($Candidate.Length+1)
    if($relative.StartsWith('HouseCoursework\Saved\')){continue}
    $hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if((Get-FileHash -LiteralPath (Join-Path $destination $relative) -Algorithm SHA256).Hash -ne $hash){throw ('Published payload mismatch: '+$relative)}
    $payload+=@{file=$relative;sha256=$hash}
}
$normalSaveAfter=if(Test-Path -LiteralPath $normalSave){(Get-FileHash -LiteralPath $normalSave).Hash}else{'missing'}
if($normalSaveBefore -ne $normalSaveAfter){throw 'Published package changed normal user lighting save'}
$testEnvironmentPath=Join-Path $evidence 'rendering_environment.json'
$testEnvironment=if(Test-Path -LiteralPath $testEnvironmentPath){Get-Content -Raw -LiteralPath $testEnvironmentPath|ConvertFrom-Json}else{$null}
$scopePath=Join-Path $evidence 'validation_scope.json'
$validationScope=if(Test-Path -LiteralPath $scopePath){Get-Content -Raw -LiteralPath $scopePath|ConvertFrom-Json}else{$null}
$routeRenderer=if($route.renderer){$route.renderer}elseif($testEnvironment.renderer){$testEnvironment.renderer}else{$null}
@{version=$Version;git_commit=$commit;validation_scope=$validationScope;package=$destination;checks=$checks;lighting_persistence=$persistence;safety=$safety;evacuation=$evacuation;fire_locomotion=$fireLocomotion;fixture_interaction=$fixtureInteraction;energy_monitoring=$energy;daylight=$daylight;daylight_smoothing=$smoothing;ventilation=$ventilation;ventilation_light_fix=$ventilationLightFix;hall_lighting=$hallLighting;menu_transitions=$menuTransitions;smart_lighting=$smartLighting;user_lighting_save_unchanged=$true;
  test_environment=$testEnvironment;
  route=@{renderer=$routeRenderer;screenshot_overhead_included=$route.screenshots;stops=$route.stops.Count;average_fps=$route.average_fps;max_frame_ms=$route.max_frame_ms;p95_ms=$route.p95_ms;p99_ms=$route.p99_ms};
  typography=@{heading='Long Cang';body='LXGW WenKai Light';numbers='Roboto Light'};
  rollback_archives=@('v0.7.0-comfort-lighting','v0.8.0-tab-refined');payload=$payload
}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath 'D:\HouseCoursework_Build\release.json' -Encoding UTF8
$Version|Set-Content -LiteralPath 'D:\HouseCoursework_Build\VERSION.txt' -Encoding UTF8
Write-Output ('Published '+$Version+'; verified '+$payload.Count+' files; preserved Saved directory')
