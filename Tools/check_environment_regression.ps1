param(
    [string]$GameRoot='D:\HouseCoursework_Team\Artifacts\EnvironmentFix\Windows',
    [string]$EvidenceRoot='D:\HouseCoursework_Team\Saved\Acceptance\EnvironmentFix',
    [ValidateSet('D3D11','D3D12')][string]$Renderer='D3D11',
    [int]$Height=720
)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$started=Get-Date
$width=[int][Math]::Round($Height*16/9)
$profile='environment_'+[guid]::NewGuid().ToString('N')
$arguments=@('-HouseTest=environment_regression',('-HouseLightingProfile='+$profile),'-RenderOffscreen',('-'+$Renderer.ToLower()),'-windowed',('-ResX='+$width),('-ResY='+$Height),'-ForceRes','-unattended',('-abslog='+(Join-Path $EvidenceRoot ('environment_'+$Renderer+'_'+$Height+'.log'))))
$process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(!$process.WaitForExit(240000)){Stop-Process -Id $process.Id;throw 'Environment regression timed out'}
if($process.ExitCode -ne 0){throw 'Environment regression crashed'}
$file=Get-Item -LiteralPath (Join-Path $saved 'EnvironmentRegression.json')
if($file.LastWriteTime -lt $started){throw 'No fresh environment regression result'}
$report=Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
$failed=@($report.checks|Where-Object passed -ne $true)
if($report.checks.Count -lt 37 -or $failed.Count){throw ('Environment checks failed: '+($failed.case -join ', '))}
foreach($name in @('noon_fire_keeps_clock','noon_fire_has_no_unbuilt_lighting','night_fire_has_no_unbuilt_lighting','1810_after_sunset_enables_guidance','manual_21_guidance_resumes','entrance_glass_center_selectable','entrance_cannot_select_through_blocker','entrance_E_opens_from_glass','entrance_E_closes_from_glass')){
    if(@($report.checks|Where-Object {$_.case -eq $name -and $_.passed}).Count -ne 1){throw "Missing environment check: $name"}
}
Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ('environment_'+$Renderer+'_'+$Height+'.json'))
$shots=@(Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Environment_*.png'|Where-Object LastWriteTime -ge $started)
if($shots.Count -lt 9){throw 'Missing fresh environment screenshots'}
$shots | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($Renderer+'_'+$Height+'_'+$_.Name))}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Environment regression modified normal user save'}
Write-Output ('Environment '+$Renderer+' '+$Height+': '+$report.checks.Count+' checks passed; screenshots require visual review')
