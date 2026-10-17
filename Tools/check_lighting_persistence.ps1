param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',[string]$EvidenceRoot='')
$ErrorActionPreference='Stop'
$root=if($EvidenceRoot){$EvidenceRoot}else{Join-Path (Split-Path $PSScriptRoot -Parent) 'Docs\LightingSave'}
New-Item -ItemType Directory -Path $root -Force|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$profile='acceptance_'+[guid]::NewGuid().ToString('N')
$state=Join-Path $saved ('Lighting\'+$profile+'.json')
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$results=@()
function Run-Phase([string]$Name,[string]$Mode){
    $started=Get-Date
    $arguments=@(('-HouseTest=lighting_state_'+$Mode),('-HouseLightingProfile='+$profile),'-windowed','-RenderOffscreen','-d3d11','-ResX=1280','-ResY=720','-ForceRes','-unattended',('-abslog='+ (Join-Path $root ($Name+'.log'))))
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(60000)){Stop-Process -Id $process.Id;throw 'Persistence phase timed out'}
    if($process.ExitCode -ne 0){throw 'Persistence phase crashed'}
    $output=Get-Item -LiteralPath (Join-Path $saved 'LightingStateAcceptance.json')
    if($output.LastWriteTime -lt $started){throw 'No fresh persistence result'}
    $result=Get-Content -Raw -LiteralPath $output.FullName|ConvertFrom-Json
    if(!$result.passed){throw ('Persistence phase failed: '+$Name)}
    Copy-Item -LiteralPath $output.FullName -Destination (Join-Path $root ($Name+'.json'))
    if(Test-Path -LiteralPath $state){Copy-Item -LiteralPath $state -Destination (Join-Path $root ($Name+'_state.json'))}
    Write-Output ($Name+' passed')
}
Run-Phase missing defaults
Run-Phase write_exit_flush write
Run-Phase restart_restores read
Run-Phase debounce debounce
Run-Phase restart_after_debounce read
Run-Phase reset reset
Run-Phase restart_after_reset defaults
'{invalid json'|Set-Content -LiteralPath $state -Encoding UTF8
Run-Phase corrupt_fallback defaults
@{version=1;rooms=@(@{id='living';enabled=$false;brightness=-3},@{id='master';enabled=$true;brightness=9},@{id='unknown';enabled=$true;brightness=.4})}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $state -Encoding UTF8
Run-Phase range_clamping clamp
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Acceptance altered the real user save'}
@{phases=9;passed=$true;user_save_unchanged=$true;isolated_profile=$profile}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'persistence_summary.json') -Encoding UTF8
