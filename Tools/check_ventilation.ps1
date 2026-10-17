param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',[string]$EvidenceRoot='D:\HouseCoursework\Docs\Ventilation\Final',[int[]]$Heights=@(720,1080))
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$runs=@()
foreach($height in $Heights){
 $width=if($height -eq 720){1280}else{1920}
 $started=Get-Date
 $profile='ventilation_'+[guid]::NewGuid().ToString('N')
 $arguments=@('-HouseTest=ventilation',('-HouseLightingProfile='+$profile),'-RenderOffscreen','-d3d11','-windowed',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ('ventilation_'+$height+'.log'))))
 $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
 if(!$process.WaitForExit(240000)){Stop-Process -Id $process.Id;throw 'Ventilation acceptance timed out'}
 if($process.ExitCode -ne 0){throw 'Ventilation acceptance crashed'}
 $file=Get-Item -LiteralPath (Join-Path $saved 'VentilationAcceptance.json')
 if($file.LastWriteTime -lt $started){throw 'No fresh ventilation result'}
 Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ('ventilation_'+$height+'.json'))
 $result=Get-Content -LiteralPath $file.FullName -Raw|ConvertFrom-Json
 $shots=@(Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Ventilation_*.png'|Where-Object LastWriteTime -ge $started)
 $shots|ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($height.ToString()+'_'+$_.Name))}
 if($result.checks.Count -lt 90 -or ($result.checks|Where-Object passed -ne $true)){throw ('Ventilation checks failed: '+(($result.checks|Where-Object passed -ne $true).case -join ', '))}
 if($shots.Count -lt 7){throw 'Ventilation screenshots not rendered freshly'}
 $rendering=Get-Item -LiteralPath (Join-Path $saved 'VentilationRendering.txt')
 if($rendering.LastWriteTime -lt $started){throw 'No fresh airflow render diagnostics'}
 Copy-Item -LiteralPath $rendering.FullName -Destination (Join-Path $EvidenceRoot ('rendering_'+$height+'.txt'))
 $perfStarted=Get-Date
 $perfArgs=$arguments|ForEach-Object{if($_ -eq '-HouseTest=ventilation'){'-HouseTest=ventilation_perf'}elseif($_.StartsWith('-abslog=')){'-abslog='+ (Join-Path $EvidenceRoot ('performance_'+$height+'.log'))}else{$_}}
 $perfProcess=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $perfArgs -WindowStyle Hidden -PassThru
 if(!$perfProcess.WaitForExit(90000)){Stop-Process -Id $perfProcess.Id;throw 'Ventilation performance timed out'}
 if($perfProcess.ExitCode -ne 0){throw 'Ventilation performance crashed'}
 $perfFile=Get-Item -LiteralPath (Join-Path $saved 'VentilationPerformance.json')
 if($perfFile.LastWriteTime -lt $perfStarted){throw 'No fresh ventilation performance result'}
 Copy-Item -LiteralPath $perfFile.FullName -Destination (Join-Path $EvidenceRoot ('performance_'+$height+'.json'))
 $perf=Get-Content -LiteralPath $perfFile.FullName -Raw|ConvertFrom-Json
 if($perf.runs.Count -ne 2 -or ($perf.runs|Where-Object {!$_.passed -or $_.average_fps -lt 25})){throw 'Ventilation rendering below 25 FPS'}
 $runs+=@{height=$height;checks=$result.checks.Count;passed=$true;performance=$perf.runs}
 Write-Output ('Ventilation '+$height+' passed '+$result.checks.Count+' checks')
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Ventilation checks changed normal user lighting save'}
& 'D:\epic\UE_5.3\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' "$PSScriptRoot\verify_window_review.py" $EvidenceRoot $Heights
if($LASTEXITCODE -ne 0){throw 'Per-window rendered evidence is incomplete'}
@{passed=$true;user_save_unchanged=$true;runs=$runs;visual_reviewed=$false;method='Packaged actual controller E and Slate pointer handlers, synthetic input, isolated profiles, D3D11 offscreen'}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'ventilation_summary.json') -Encoding utf8
