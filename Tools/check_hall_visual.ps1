param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='D:\HouseCoursework\Docs\HallLightingFix\Final')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$started=Get-Date
$arguments=@('-HouseTest=fixture','-HouseHallProbe','-RenderOffscreen','-d3d12','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot 'hall_d3d12.log')))
$process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$process.WaitForExit(120000)){Stop-Process -Id $process.Id;throw 'Hall visual probe timed out'}
if($process.ExitCode -ne 0){throw 'Hall visual probe crashed'}
$file=Get-Item -LiteralPath (Join-Path $saved 'FixtureAcceptance.json')
if($file.LastWriteTime -lt $started){throw 'No fresh hall report'}
Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot 'hall_d3d12.json')
$result=Get-Content -Raw -LiteralPath $file.FullName|ConvertFrom-Json
if($result.checks.Count -lt 80 -or ($result.checks|Where-Object passed -ne $true)){throw ('Hall checks failed: '+(($result.checks|Where-Object passed -ne $true).case -join ', '))}
foreach($name in @('SM_Lamp_E2','SM_Lamp_E3','SM_Lamp_E4','SM_Lamp_C1_2')){
 foreach($state in @('on','glow','off')){
  $shot=Get-Item -LiteralPath (Join-Path $saved ('Screenshots\Fixture_Isolated_'+$name+'_'+$state+'.png'))
  if($shot.LastWriteTime -lt $started){throw 'Stale isolated fixture screenshot'}
  Copy-Item -LiteralPath $shot.FullName -Destination (Join-Path $EvidenceRoot ('d3d12_'+$shot.Name))
 }
}
if((Get-Content -Raw -LiteralPath (Join-Path $EvidenceRoot 'hall_d3d12.log')) -notmatch 'D3D12RHI'){throw 'D3D12 not confirmed'}
Write-Output ('Hall D3D12 input checks passed: '+$result.checks.Count+'; twelve isolated screenshots require review')
