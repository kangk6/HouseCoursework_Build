param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',[string]$EvidenceRoot='D:\HouseCoursework\Docs\EnergyMonitoring')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$runs=@()
foreach($height in @(720,1080)){
    $width=if($height -eq 720){1280}else{1920}
    $profile='energy_'+[guid]::NewGuid().ToString('N')
    $started=Get-Date
    $argsList=@('-HouseTest=energy',('-HouseLightingProfile='+$profile),'-windowed','-RenderOffscreen','-d3d11',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ('energy_'+$height+'.log'))))
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $argsList -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(220000)){Stop-Process -Id $process.Id;throw 'Energy acceptance timed out'}
    if($process.ExitCode -ne 0){throw 'Energy acceptance crashed'}
    $file=Get-Item -LiteralPath (Join-Path $saved 'EnergyAcceptance.json')
    if($file.LastWriteTime -lt $started){throw 'No fresh energy result'}
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ('energy_'+$height+'.json'))
    $result=Get-Content -Raw -LiteralPath $file.FullName|ConvertFrom-Json
    $failed=@($result.checks|Where-Object passed -ne $true)
    if($result.checks.Count -lt 30 -or $failed.Count){throw ('Energy failed: '+$height+' '+($failed.case -join ', '))}
    $save=Get-Item -LiteralPath (Join-Path $saved ('Lighting\'+$profile+'.json'))
    $state=Get-Content -Raw -LiteralPath $save.FullName|ConvertFrom-Json
    $ids=@('living','master','kitchen','child','study','bath_ground','bath_upper','dining','hall_stairs')
    if($state.rooms.Count -ne 9){throw 'Incomplete saved lighting configuration'}
    for($i=0;$i -lt 9;$i++){
        $room=@($state.rooms|Where-Object id -eq $ids[$i]);$expectedOn=($i%3 -ne 0)
        if($room.Count -ne 1 -or $room[0].enabled -ne $expectedOn -or [Math]::Abs($room[0].brightness-(.25+.07*$i)) -gt .0001){throw ('Quit during preview did not restore original room '+$ids[$i])}
    }
    Copy-Item -LiteralPath $save.FullName -Destination (Join-Path $EvidenceRoot ('restored_save_'+$height+'.json'))
    $shots=@(Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Energy_*.png'|Where-Object LastWriteTime -ge $started)
    $shots|ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($height.ToString()+'_'+$_.Name))}
    if($shots.Count -lt 10){throw 'Energy screenshots not rendered freshly'}
    $runs+=@{height=$height;checks=$result.checks.Count;passed=$true;quit_preview_restored=$true}
    Write-Output ('Energy '+$height+' passed '+$result.checks.Count+' checks; all nine saved rooms restored')
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Energy acceptance modified user lighting save'}
@{passed=$true;runs=$runs;user_save_unchanged=$true;method='Packaged controller E/wheel and actual Slate pointer handlers; isolated persistent profiles; screenshots require separate visual review'}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'energy_summary.json') -Encoding UTF8
