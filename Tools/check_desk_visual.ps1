param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='D:\HouseCoursework\Docs\VentilationLightFix\Final')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$started=Get-Date
$argsList=@('-HouseTest=fixture','-HouseDeskProbe','-RenderOffscreen','-d3d12','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot 'desk_d3d12.log')))
$process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $argsList -WindowStyle Hidden -PassThru
if(!$process.WaitForExit(90000)){Stop-Process -Id $process.Id;throw 'Desk visual probe timed out'}
if($process.ExitCode -ne 0){throw 'Desk visual probe crashed'}
$file=Get-Item -LiteralPath (Join-Path $saved 'FixtureAcceptance.json')
if($file.LastWriteTime -lt $started){throw 'No fresh desk probe report'}
$result=Get-Content -Raw -LiteralPath $file.FullName|ConvertFrom-Json
if($result.checks.Count -lt 22 -or ($result.checks|Where-Object passed -ne $true)){throw 'Desk input/local emitter checks failed'}
Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot 'desk_d3d12.json')
foreach($index in @('08','09')){foreach($state in @('dim','off')){
 $shot=Get-Item -LiteralPath (Join-Path $saved ('Screenshots\Fixture_Inspection_'+$index+'_'+$state+'.png'))
 if($shot.LastWriteTime -lt $started){throw 'Desk screenshot stale'}
 Copy-Item -LiteralPath $shot.FullName -Destination (Join-Path $EvidenceRoot ('d3d12_'+$shot.Name))
}}
$log=Get-Content -Raw -LiteralPath (Join-Path $EvidenceRoot 'desk_d3d12.log')
if($log -notmatch 'D3D12RHI'){throw 'Requested renderer not confirmed'}
@{passed=$true;height=1080;renderer='D3D12';checks=$result.checks.Count;visual_reviewed=$false;method='Packaged controller E/wheel events and screenshots; no physical mouse claim'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'desk_visual_summary.json') -Encoding utf8
Write-Output ('D3D12 desk checks passed '+$result.checks.Count+' cases; four fresh screenshots')
