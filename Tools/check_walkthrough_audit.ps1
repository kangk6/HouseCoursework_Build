param(
    [string]$GameRoot='D:\HouseCoursework_Team\Artifacts\ClockFoundation\Windows',
    [string]$EvidenceRoot='D:\HouseCoursework_Team\Saved\Acceptance\WalkthroughAudit\Final',
    [ValidateSet('day','rain','night','fire')][string[]]$Scenarios=@('day','rain','night','fire'),
    [ValidateSet(720,1080)][int]$Height=1080,
    [switch]$Foreground,
    [switch]$CaptureScreenshots,
    [string[]]$ExtraArguments=@()
)
$ErrorActionPreference='Stop'
if($Foreground -and $CaptureScreenshots){throw 'Performance runs must not capture screenshots; use a separate visual run.'}
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$inner=Join-Path $GameRoot 'HouseCoursework\Binaries\Win64\HouseCoursework.exe'
$normal='D:\HouseCoursework_Build\Windows\HouseCoursework\Saved\Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$binary=(Get-FileHash -LiteralPath $inner).Hash
$runs=@()
foreach($scenario in $Scenarios){
    $profile='routeaudit_'+[guid]::NewGuid().ToString('N')
    $started=Get-Date
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $stem='route_'+$scenario+'_'+$Height
    $arguments=@('-HouseTest=route',('-HouseRouteScenario='+$scenario),('-HouseLightingProfile='+$profile),'-windowed',('-ResX='+[int]($Height*16/9)),('-ResY='+$Height),'-ForceRes','-d3d12','-unattended',('-abslog='+(Join-Path $EvidenceRoot ($stem+'.log'))))
    if(!$Foreground){$arguments+='-RenderOffscreen'}
    if(!$CaptureScreenshots){$arguments+='-HouseNoRouteScreenshots'}
    if($scenario -eq 'night' -and $CaptureScreenshots){$arguments+='-HouseSmartNightRoute'}
    $arguments+=$ExtraArguments
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $timedOut=$false
    while(!$process.WaitForExit(1000)){
        if($watch.Elapsed.TotalSeconds -gt 240){
            $timedOut=$true
            # The launcher can exit independently of the inner UE process.
            # Only terminate this run's uniquely identified child, never a user run.
            Get-CimInstance Win32_Process|Where-Object {$_.ExecutablePath -eq $inner -and $_.CommandLine.Contains($profile)}|ForEach-Object {Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue}
            if(!$process.HasExited){Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue}
            break
        }
    }
    if($timedOut){throw "$stem timed out; this run's processes were closed. Inspect the log."}
    if($process.ExitCode -ne 0){throw "$stem exited with $($process.ExitCode)"}
    $file=Get-Item -LiteralPath (Join-Path $saved 'ContinuousRouteAcceptance.json')
    if($file.LastWriteTime -lt $started){throw "$stem did not produce fresh evidence"}
    $report=Get-Content -LiteralPath $file.FullName -Raw|ConvertFrom-Json
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ($stem+'.json'))
    Copy-Item -LiteralPath (Join-Path $saved 'RouteAuditFrameTimes.csv') -Destination (Join-Path $EvidenceRoot ($stem+'.csv'))
    if($CaptureScreenshots){
        $shots=Join-Path $EvidenceRoot ($stem+'_screenshots');New-Item -ItemType Directory -Force $shots|Out-Null
        Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Route_*.png'|Where-Object LastWriteTime -ge $started|Copy-Item -Destination $shots
    }
    $functional=$report.completed -and $report.scenario_valid -and $report.mid_route_teleports -eq 0 -and !@($report.stops|Where-Object passed -ne $true).Count
    if($scenario -eq 'fire'){
        $log=Get-Content -LiteralPath (Join-Path $EvidenceRoot ($stem+'.log')) -Raw
        $functional=$functional -and $log.Contains('SAFETY_STATE detector_alarm') -and $log.Contains('EVAC_RESIDENT gathered')
    }
    if($report.scenario -ne $scenario -or $report.resolution[1] -ne $Height){throw "$stem ran with incorrect conditions"}
    $performance=if($Foreground){$report.benchmark_eligible -and $report.average_fps -ge 25}else{$null}
    $runs+=@{scenario=$scenario;functional_passed=$functional;performance_passed=$performance;benchmark_eligible=$report.benchmark_eligible;average_fps=$report.average_fps;p95_ms=$report.p95_ms;frames_over_100ms=$report.frames_over_100ms;report=($stem+'.json')}
    @{binary_sha256=$binary;foreground_requested=[bool]$Foreground;extra_arguments=$ExtraArguments;scope='Native CharacterMovement and actual E input on a continuous route. Background captures are not a foreground FPS certification. Fixed scripted steering does not certify physical mouse use or every object.';runs=$runs}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'walkthrough_summary.json') -Encoding utf8
    if(!$functional){throw "$stem route failed; inspect the saved position and blocker"}
    if($Foreground -and !$performance){throw "$stem did not qualify: require foreground throughout, no screenshots, and average FPS >= 25"}
    Write-Output "$stem functional checks passed; foreground benchmark eligible=$($report.benchmark_eligible)"
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Audit changed normal lighting save'}
if((Get-FileHash -LiteralPath $inner).Hash -ne $binary){throw 'Candidate changed during audit'}
