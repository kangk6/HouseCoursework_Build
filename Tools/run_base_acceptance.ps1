param([string]$GameRoot='D:\HouseCoursework_Build\Windows',[string]$EvidenceRoot='D:\HouseCoursework\Docs\BaseAcceptance',[string[]]$Modes=@('furniture','route','ui','map','idle'),[switch]$NightLighting,[ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$cases=@(
    @{Mode='furniture';File='FurnitureAcceptance.json';Extra='-HouseFurnitureCycles=2'},
    @{Mode='route';File='ContinuousRouteAcceptance.json';Extra=''},
    @{Mode='ui';File='CourseworkUIAcceptance.json';Extra=''},
    @{Mode='map';File='CourseworkMapAcceptance.json';Extra=''},
    @{Mode='idle';File='ResidentIdleAcceptance.json';Extra=''}
)
foreach($case in ($cases|Where-Object { $_.Mode -in $Modes })){
    $begin=Get-Date
    $arguments=@(('-HouseTest='+$case.Mode),'-windowed','-RenderOffscreen',('-'+$Renderer.ToLower()),'-ResX=1280','-ResY=720','-ForceRes','-unattended',('-abslog="'+(Join-Path $EvidenceRoot ($case.Mode+'.log'))+'"'))
    if($case.Extra){$arguments+=$case.Extra}
    if($NightLighting -and $case.Mode -eq 'route'){$arguments+='-HouseSmartNightRoute'}
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    if($process.ExitCode -ne 0){throw ($case.Mode+' process failed')}
    $output=Get-Item -LiteralPath (Join-Path $saved $case.File)
    if($output.LastWriteTime -lt $begin){throw ($case.Mode+' produced no fresh evidence')}
    Copy-Item -LiteralPath $output.FullName -Destination $EvidenceRoot
    $result=Get-Content -LiteralPath $output.FullName -Raw|ConvertFrom-Json
    if($case.Mode -eq 'furniture'){
        Copy-Item -LiteralPath (Join-Path $saved 'FurnitureInventory.json') -Destination $EvidenceRoot
        if(($result|Where-Object { !$_.passed -or $_.cycles_completed -ne 2 }) -or $result.Count -ne 131){throw 'Furniture cases failed; inspect saved evidence'}
    }elseif($case.Mode -eq 'route'){
        if(!$result.completed -or $result.mid_route_teleports -ne 0 -or ($result.stops|Where-Object passed -ne $true)){throw 'Continuous route failed'}
        Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Route_*.png'|Where-Object LastWriteTime -ge $begin|Copy-Item -Destination $EvidenceRoot
        $frameCsv=Get-Item -LiteralPath (Join-Path $saved 'RouteFrameTimes.csv') -ErrorAction SilentlyContinue
        if($frameCsv -and $frameCsv.LastWriteTime -ge $begin){Copy-Item -LiteralPath $frameCsv.FullName -Destination $EvidenceRoot}
        if($result.average_fps -lt 25){throw ('Average FPS below coursework requirement: '+$result.average_fps)}
    }elseif($case.Mode -in @('map','ui')){
        if($result.Count -ne 9 -or ($result|Where-Object passed -ne $true)){throw ($case.Mode+' regression failed')}
    }elseif($case.Mode -eq 'idle'){
        & 'D:\epic\UE_5.3\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'validate_idle_samples.py') $output.FullName *> (Join-Path $EvidenceRoot 'idle_validation.txt')
        if($LASTEXITCODE -ne 0){throw 'Idle validation failed'}
        Copy-Item -LiteralPath (Join-Path $saved 'idle_summary.json') -Destination $EvidenceRoot
    }
    Write-Output ($case.Mode+' passed')
    if($case.Mode -eq 'route'){@{night_lighting=[bool]$NightLighting;renderer=($Renderer+' offscreen');candidate_executable_sha256=(Get-FileHash -LiteralPath (Join-Path $GameRoot 'HouseCoursework\Binaries\Win64\HouseCoursework.exe')).Hash}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'route_environment.json') -Encoding utf8}
}
