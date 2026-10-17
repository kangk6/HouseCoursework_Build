param([string]$EvidenceRoot='D:\HouseCoursework\Docs\DaylightSmoothing\Final',[string]$GameRoot='D:\HouseCoursework_TabFinal\Windows')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
foreach($height in @(720,1080)){
 $width=if($height -eq 720){1280}else{1920}
 $started=Get-Date
 $arguments=@('-HouseTest=daylight_sweep',('-HouseLightingProfile=sweep_'+[guid]::NewGuid().ToString('N')),'-RenderOffscreen','-d3d11','-windowed',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ('sweep_'+$height+'.log'))))
 $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
 if(!$process.WaitForExit(180000)){Stop-Process -Id $process.Id;throw 'Sweep timed out'}
 if($process.ExitCode -ne 0){throw 'Sweep crashed'}
 $report=Get-Item -LiteralPath (Join-Path $saved 'DaylightSweep.json')
 if($report.LastWriteTime -lt $started){throw 'No fresh sweep report'}
 Copy-Item -LiteralPath $report.FullName -Destination (Join-Path $EvidenceRoot ('sweep_'+$height+'.json'))
 $frameRoot=Join-Path $EvidenceRoot ('frames_'+$height)
 New-Item -ItemType Directory -Force -Path $frameRoot|Out-Null
 $frameRootAbsolute=(Resolve-Path -LiteralPath $frameRoot).Path
 $evidenceRootAbsolute=(Resolve-Path -LiteralPath $EvidenceRoot).Path
 if(!$frameRootAbsolute.StartsWith($evidenceRootAbsolute+'\')){throw 'Unexpected sweep frame output path'}
 Get-ChildItem -LiteralPath $frameRootAbsolute -File -Filter '*.png'|ForEach-Object {Remove-Item -LiteralPath $_.FullName}
 Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots\Sweep') -Filter '*.png'|Where-Object LastWriteTime -ge $started|Copy-Item -Destination $frameRoot
 Write-Output ('Sweep '+$height+' captured')
}

$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Sweep changed normal lighting save'}
@{user_save_unchanged=$true;method='Actual Slate down/move/up handlers and every-frame Slate backbuffer readback; screenshot workload is not a normal FPS benchmark'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'sweep_environment.json') -Encoding utf8
