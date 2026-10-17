param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$RunName='paired',[string[]]$Order=@('legacy','cached'))
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$root=Join-Path $repo ('Docs\LightingPerformance\'+$RunName)
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
New-Item -ItemType Directory -Path $root -Force|Out-Null
$results=@()
foreach($mode in $Order){
    $started=Get-Date
    $arguments=@('-HouseTest=route','-HouseNoRouteScreenshots','-windowed','-ResX=1280','-ResY=720','-ForceRes','-unattended',('-abslog='+ (Join-Path $root ($mode+'.log'))))
    if($mode -eq 'legacy'){$arguments+='-HouseLegacyMovableSurfaces'}
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(180000)){Stop-Process -Id $process.Id;throw 'Route comparison timed out'}
    if($process.ExitCode -ne 0){throw 'Route comparison process failed'}
    $output=Get-Item -LiteralPath (Join-Path $saved 'ContinuousRouteAcceptance.json')
    if($output.LastWriteTime -lt $started){throw 'No fresh route result'}
    $report=Get-Content -Raw -LiteralPath $output.FullName|ConvertFrom-Json
    if(!$report.completed -or $report.screenshots -or ($report.stops|Where-Object passed -ne $true)){throw 'Route comparison failed'}
    Copy-Item -LiteralPath $output.FullName -Destination (Join-Path $root ($mode+'.json'))
    Copy-Item -LiteralPath (Join-Path $saved 'RouteFrameTimes.csv') -Destination (Join-Path $root ($mode+'.csv'))
    $results+=@{mode=$mode;report=$report}
    Write-Output ($mode+' average_fps='+$report.average_fps+' p95_ms='+$report.p95_ms+' max_ms='+$report.max_frame_ms)
}
$results|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $root 'comparison.json') -Encoding UTF8
