param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='D:\HouseCoursework\Docs\FixtureInteraction')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $EvidenceRoot|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$normal=Join-Path $saved 'Lighting\room_lighting.json'
$before=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
$runs=@()
foreach($height in @(720,1080)){
    $width=if($height -eq 720){1280}else{1920}
    $started=Get-Date
    $argsList=@('-HouseTest=fixture','-windowed','-RenderOffscreen','-d3d11',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ('fixture_'+$height+'.log'))))
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $argsList -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(240000)){Stop-Process -Id $process.Id;throw 'Fixture acceptance timed out'}
    if($process.ExitCode -ne 0){throw 'Fixture acceptance crashed'}
    $file=Get-Item -LiteralPath (Join-Path $saved 'FixtureAcceptance.json')
    if($file.LastWriteTime -lt $started){throw 'No fresh fixture result'}
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $EvidenceRoot ('fixture_'+$height+'.json'))
    Copy-Item -LiteralPath (Join-Path $saved 'FixtureInventory.json') -Destination (Join-Path $EvidenceRoot 'FixtureInventory.json')
    # Actor iteration order can differ between runs; screenshot indices must
    # always be interpreted using the inventory from the same resolution/run.
    Copy-Item -LiteralPath (Join-Path $saved 'FixtureInventory.json') -Destination (Join-Path $EvidenceRoot ('FixtureInventory_'+$height+'.json'))
    $shots=@(Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter 'Fixture_*.png'|Where-Object LastWriteTime -ge $started)
    $shots|ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($height.ToString()+'_'+$_.Name))}
    if($shots.Count -lt 49){throw 'Fixture screenshots not rendered freshly'}
    $result=Get-Content -Raw -LiteralPath $file.FullName|ConvertFrom-Json
    $failed=@($result.checks|Where-Object passed -ne $true)
    if($result.checks.Count -lt 200 -or $failed.Count){throw ('Fixture failed: '+$height+' '+($failed.case -join ', '))}
    $inventory=Get-Content -Raw -LiteralPath (Join-Path $saved 'FixtureInventory.json')|ConvertFrom-Json
    if($inventory.Count -lt 20){throw 'Fixture inventory incomplete'}
    foreach($fixture in $inventory){
        foreach($prefix in @('reachable_','selected_','tap_','dim_','off_feedback_','dim_feedback_','alternate_stance_','alternate_selection_')){
            if(@($result.checks|Where-Object { $_.case -eq ($prefix+$fixture.actor) -and $_.passed }).Count -ne 1){throw ('Missing individual fixture acceptance: '+$fixture.actor)}
        }
    }
    $runs+=@{height=$height;checks=$result.checks.Count;fixtures=$inventory.Count;passed=$true}
    Write-Output ('Fixture '+$height+' passed '+$result.checks.Count+' checks')
}
$after=if(Test-Path -LiteralPath $normal){(Get-FileHash -LiteralPath $normal).Hash}else{'missing'}
if($before -ne $after){throw 'Fixture acceptance modified user lighting save'}
@{passed=$true;runs=$runs;user_save_unchanged=$true;method='Packaged controller input events and real traces; screenshots reviewed separately'}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $EvidenceRoot 'fixture_summary.json') -Encoding UTF8
