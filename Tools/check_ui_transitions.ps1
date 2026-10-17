param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='D:\HouseCoursework\Docs\BugAudit20261017\Final')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$runs=@()
foreach($height in @(720,1080,1200)){
 $started=Get-Date
 $width=if($height -eq 720){1280}else{1920}
 $arguments=@('-HouseTest=ui_transitions','-RenderOffscreen','-d3d11','-windowed',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ('transitions_'+$height+'.log'))))
 $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
 if(!$process.WaitForExit(60000)){Stop-Process -Id $process.Id;throw 'Menu transition test timed out'}
 if($process.ExitCode -ne 0){throw 'Menu transition test crashed'}
 $file=Get-Item -LiteralPath (Join-Path $saved 'TransitionAcceptance.json')
 if($file.LastWriteTime -lt $started){throw 'Menu transition report is stale'}
 $report=Get-Content -Raw -LiteralPath $file.FullName|ConvertFrom-Json
 Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ('transitions_'+$height+'.json'))
 if($report.checks.Count -ne 17 -or ($report.checks|Where-Object passed -ne $true)){throw ('Menu transitions failed: '+(($report.checks|Where-Object passed -ne $true).case -join ', '))}
 $shot=Get-Item -LiteralPath (Join-Path $saved 'Screenshots\Transition_safety_tab.png')
 if($shot.LastWriteTime -lt $started){throw 'Menu transition screenshot is stale'}
 Copy-Item -LiteralPath $shot.FullName -Destination (Join-Path $EvidenceRoot ('transitions_'+$height+'.png'))
 $runs+=@{height=$height;checks=17;passed=$true}
 Write-Output ('Menu transitions '+$height+' passed 17 checks')
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Menu transition tests changed normal lighting save'}
@{passed=$true;visual_reviewed=$false;runs=$runs;user_save_unchanged=$true;candidate_executable_sha256=(Get-FileHash -LiteralPath (Join-Path $GameRoot 'HouseCoursework\Binaries\Win64\HouseCoursework.exe')).Hash}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'transitions_summary.json') -Encoding UTF8
