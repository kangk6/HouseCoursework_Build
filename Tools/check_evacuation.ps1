param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='D:\HouseCoursework\Docs\Evacuation',
      [string[]]$Modes=@('ground','upper','blocked'))
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$runs=@()
foreach($mode in $Modes){
    $height=if($mode -eq 'blocked'){1080}else{720}
    $width=if($height -eq 1080){1920}else{1280}
    $started=Get-Date
    $arguments=@(('-HouseTest=evacuation_'+$mode),'-windowed','-RenderOffscreen','-d3d11',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ($mode+'.log'))))
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(140000)){Stop-Process -Id $process.Id;throw ('Evacuation timed out: '+$mode)}
    if($process.ExitCode -ne 0){throw ('Evacuation crashed: '+$mode)}
    $output=Get-Item -LiteralPath (Join-Path $saved 'EvacuationAcceptance.json')
    if($output.LastWriteTime -lt $started){throw 'No fresh evacuation result'}
    $report=Get-Content -Raw -LiteralPath $output.FullName|ConvertFrom-Json
    Copy-Item -LiteralPath $output.FullName -Destination (Join-Path $EvidenceRoot ($mode+'.json'))
    Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Evacuation_*.png'|Where-Object LastWriteTime -ge $started|ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($mode+'_'+$_.Name))
    }
    if($report.checks.Count -lt 19 -or ($report.checks|Where-Object passed -ne $true)){throw ('Evacuation checks failed: '+$mode)}
    if($report.average_fps -lt 25){throw ('Evacuation FPS below 25: '+$mode)}
    $runs+=@{mode=$mode;height=$height;checks=$report.checks.Count;average_fps=$report.average_fps;passed=$true}
    Write-Output ($mode+' passed '+$report.checks.Count+' checks; FPS='+$report.average_fps)
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Evacuation changed the normal lighting save'}
@{passed=$true;runs=$runs;normal_lighting_save_unchanged=$true;method='Packaged CharacterMovement with collision, actual Slate handlers via synthetic pointer input; screenshots require separate review'}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'evacuation_summary.json') -Encoding UTF8
