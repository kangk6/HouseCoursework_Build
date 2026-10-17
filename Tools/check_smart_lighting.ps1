param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='D:\HouseCoursework\Saved\Acceptance\SmartLighting',
      [int[]]$Heights=@(720,1080),[ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$runs=@()
foreach($height in $Heights){
 $started=Get-Date
 $width=if($height -eq 720){1280}else{1920}
 $arguments=@('-HouseTest=smart_lighting','-RenderOffscreen',('-'+$Renderer.ToLower()),'-windowed',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+(Join-Path $EvidenceRoot ('smart_'+$height+'.log'))))
 $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
 if(!$process.WaitForExit(180000)){Stop-Process -Id $process.Id;throw 'Smart lighting acceptance timed out'}
 if($process.ExitCode -ne 0){throw 'Smart lighting acceptance crashed'}
 $file=Get-Item -LiteralPath (Join-Path $saved 'SmartLightingAcceptance.json')
 if($file.LastWriteTime -lt $started){throw 'No fresh smart lighting report'}
 Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ('smart_'+$height+'.json'))
 $report=Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
 Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Smart_*.png' | Where-Object LastWriteTime -ge $started | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($height.ToString()+'_'+$_.Name))}
 $failed=@($report.checks | Where-Object passed -ne $true)
 if(@($report.checks|Where-Object {$_.case -eq 'stair_fill_owner_is_visible' -and $_.passed}).Count -ne 1){throw 'Stair fill is not verified as a rendered world component'}
 if($report.checks.Count -lt 60 -or $failed.Count){throw ('Smart lighting failed: '+($failed.case -join ', '))}
 foreach($name in @('bedroom_has_rendered_path_fill','bedroom_door_has_path_coverage','child_exit_has_path_coverage','ground_corridor_has_real_fill','presence_does_not_refresh_floor_above','manual_room_off_disables_local_path_fill','daytime_disables_local_night_fill','scene_exit_clears_every_path_fill')){if(@($report.checks|Where-Object {$_.case -eq $name -and $_.passed}).Count -ne 1){throw "Missing night guidance check: $name"}}
 foreach($shot in @('Smart_night_route.png','Smart_reading_night.png','Smart_reading_day.png','Smart_panel_final.png','Smart_bedroom_exit.png','Smart_bedroom_door.png','Smart_child_exit.png','Smart_ground_connection.png')){if(!(Test-Path -LiteralPath (Join-Path $EvidenceRoot ($height.ToString()+'_'+$shot)))){throw "Missing fresh capture: $shot"}}
 $runs+=@{height=$height;checks=$report.checks.Count;passed=$true}
 Write-Output ('Smart lighting '+$height+' passed '+$report.checks.Count+' checks')
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Smart lighting tests changed normal lighting save'}
@{passed=$true;runs=$runs;renderer=$Renderer;user_save_unchanged=$true;visual_reviewed=$false;candidate_executable_sha256=(Get-FileHash -LiteralPath (Join-Path $GameRoot 'HouseCoursework\Binaries\Win64\HouseCoursework.exe')).Hash;method='Actual Slate/E handlers and real light components; fixed test placements, not a walking or lux-meter claim'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'smart_summary.json') -Encoding utf8
