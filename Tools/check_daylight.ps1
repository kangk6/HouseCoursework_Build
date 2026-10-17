param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',[string]$EvidenceRoot='D:\HouseCoursework\Docs\Daylight')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$runs=@()
foreach($height in @(720,1080)){
    $width=if($height -eq 720){1280}else{1920}
    $profile='daylight_'+[guid]::NewGuid().ToString('N')
    $started=Get-Date
    $arguments=@('-HouseTest=daylight',('-HouseLightingProfile='+$profile),'-RenderOffscreen','-d3d11','-windowed',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ('daylight_'+$height+'.log'))))
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(180000)){Stop-Process -Id $process.Id;throw 'Daylight acceptance timed out'}
    if($process.ExitCode -ne 0){throw 'Daylight acceptance crashed'}
    $file=Get-Item -LiteralPath (Join-Path $saved 'DaylightAcceptance.json')
    if($file.LastWriteTime -lt $started){throw 'No fresh daylight result'}
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ('daylight_'+$height+'.json'))
    $result=Get-Content -LiteralPath $file.FullName -Raw|ConvertFrom-Json
    if($result.checks.Count -lt 30 -or ($result.checks|Where-Object passed -ne $true)){throw ('Daylight checks failed: '+(($result.checks|Where-Object passed -ne $true).case -join ', '))}
    $shots=@(Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Daylight_*.png'|Where-Object LastWriteTime -ge $started)
    if($shots.Count -lt 8){throw 'Daylight screenshots not rendered freshly'}
    $shots|ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($height.ToString()+'_'+$_.Name))}
    $runs+=@{height=$height;checks=$result.checks.Count;passed=$true}
    Write-Output ('Daylight '+$height+' passed '+$result.checks.Count+' checks')
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Daylight checks changed normal user lighting save'}
@{passed=$true;user_save_unchanged=$true;runs=$runs;visual_reviewed=$false;method='Packaged actual Slate/controller handlers; synthetic pointer events; isolated profiles; D3D11 offscreen'}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'daylight_summary.json') -Encoding utf8
