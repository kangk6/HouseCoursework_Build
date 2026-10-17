param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',[string]$EvidenceRoot='D:\HouseCoursework\Docs\WholeHouseLighting',[string[]]$Modes=@('control','lighting','ui','map'))
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
$cases=@(
    @{Mode='control';Width=1280;Height=720;File='ControlPointerAcceptance.json';Count=30;Name='control_720'},
    @{Mode='control';Width=1920;Height=1080;File='ControlPointerAcceptance.json';Count=30;Name='control_1080'},
    @{Mode='lighting';Width=1280;Height=720;File='LightingAcceptance.json';Count=45;Name='lighting'},
    @{Mode='ui';Width=1280;Height=720;File='CourseworkUIAcceptance.json';Count=9;Name='ui'},
    @{Mode='map';Width=1280;Height=720;File='CourseworkMapAcceptance.json';Count=9;Name='map'}
)
foreach($case in ($cases|Where-Object {$_.Mode -in $Modes})){
    $started=Get-Date
    $arguments=@(('-HouseTest='+$case.Mode),'-windowed','-RenderOffscreen','-d3d11',('-ResX='+$case.Width),('-ResY='+$case.Height),'-ForceRes','-unattended',('-abslog='+ (Join-Path $EvidenceRoot ($case.Name+'.log'))))
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(180000)){Stop-Process -Id $process.Id;throw ($case.Name+' timed out')}
    if($process.ExitCode -ne 0){throw ($case.Name+' failed to exit successfully')}
    $output=Get-Item -LiteralPath (Join-Path $saved $case.File)
    if($output.LastWriteTime -lt $started){throw ($case.Name+' produced no fresh result')}
    Copy-Item -LiteralPath $output.FullName -Destination (Join-Path $EvidenceRoot ($case.Name+'.json'))
    $result=Get-Content -LiteralPath $output.FullName -Raw | ConvertFrom-Json
    if($result.Count -ne $case.Count -or ($result|Where-Object passed -ne $true)){throw ($case.Name+' has failed cases')}
    Get-ChildItem -LiteralPath (Join-Path $saved 'Screenshots') -Filter '*.png' | Where-Object LastWriteTime -ge $started | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceRoot ($case.Name+'_'+$_.Name))
    }
    Write-Output ($case.Name+' passed '+$case.Count+' cases')
}
$lightingLog=Get-Content -LiteralPath (Join-Path $EvidenceRoot 'lighting.log') -Raw
if($lightingLog -match 'GLOW_ASSIGNED [^\r\n]+ 0\s*[\r\n]'){throw 'Controlled material detached'}
$settled=[regex]::Matches($lightingLog,'(?:LIGHT_SETTLED \d+ \S+|GLOW_SETTLED \d+) ([\d.]+) EXPECTED ([\d.]+)')
if($settled.Count -lt 340){throw 'Too few settled lighting checks'}
foreach($match in $settled){if([Math]::Abs([double]$match.Groups[1].Value-[double]$match.Groups[2].Value) -ge .01){throw 'Settled light mismatch'}}
foreach($font in @('LXGWWenKai-Light.ttf','LongCang-Regular.ttf')){
    $source=Join-Path $PSScriptRoot ('..\Content\Slate\Fonts\'+$font)
    $packaged=Join-Path $GameRoot ('HouseCoursework\Content\Slate\Fonts\'+$font)
    if((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $packaged).Hash){throw 'Packaged font mismatch'}
}
foreach($license in @('LXGWWenKai-LICENSE.txt','LongCang-LICENSE.txt')){
    $sourceText=(Get-Content -LiteralPath (Join-Path $PSScriptRoot ('..\Content\Slate\Fonts\'+$license)) -Raw -Encoding UTF8).Replace("`r`n","`n").TrimEnd()
    $packagedText=(Get-Content -LiteralPath (Join-Path $GameRoot ('HouseCoursework\Content\Slate\Fonts\'+$license)) -Raw -Encoding UTF8).Replace("`r`n","`n").TrimEnd()
    if($sourceText -ne $packagedText){throw 'Packaged font license mismatch'}
}
@{control_pointer_720=30;control_pointer_1080=30;lighting=45;settled_lighting=$settled.Count;ui=9;map=9;packaged_fonts_verified=$true;pointer_method='Synthetic Slate pointer events using cached rendered geometry and actual mouse handlers; no physical OS mouse claim'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'summary.json') -Encoding UTF8
