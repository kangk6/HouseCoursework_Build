param([string]$GameRoot='D:\HouseCoursework_Build\Windows',
      [string]$Label='baseline',[string[]]$Views=@('living','kitchen','bedroom'))
$ErrorActionPreference='Stop'
$output=Join-Path (Split-Path $PSScriptRoot -Parent) ('Docs\LightingPerformance\'+$Label)
New-Item -ItemType Directory -Path $output -Force|Out-Null
$saved=Join-Path $GameRoot 'HouseCoursework\Saved'
New-Item -ItemType Directory -Path (Join-Path $saved 'Profiling\CSV') -Force|Out-Null
foreach($view in $Views){
    $started=Get-Date
    $arguments=@(('-HouseTest=perf_'+$view),'-windowed','-ResX=1280','-ResY=720','-ForceRes','-unattended','-csvCaptureFrames=600','-csvCompression=0','-ExecCmds="r.GPUCsvStatsEnabled 1"',('-abslog='+ (Join-Path $output ($view+'.log'))))
    $process=Start-Process -FilePath (Join-Path $GameRoot 'HouseCoursework.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $deadline=$started.AddSeconds(120);$csv=$null;$lastLength=0;$stable=0
    do{
        Start-Sleep -Milliseconds 500
        $csv=Get-ChildItem -LiteralPath (Join-Path $saved 'Profiling\CSV') -Filter '*.csv'|Where-Object LastWriteTime -ge $started|Sort-Object LastWriteTime -Descending|Select-Object -First 1
        if($csv -and $csv.Length -gt 500000 -and $csv.Length -eq $lastLength){$stable++}else{$stable=0}
        if($csv){$lastLength=$csv.Length}
    }while($stable -lt 3 -and (Get-Date) -lt $deadline -and !$process.HasExited)
    if($stable -lt 3){throw 'Profiling did not finish a complete CSV'}
    # Avoid the observed ExitAfterCsvProfiling shutdown failure; close this window
    # after the CSV writer has finished on the background thread.
    $windows=Get-Process HouseCoursework|Where-Object {$_.Path.StartsWith($GameRoot) -and $_.MainWindowHandle -ne 0}
    foreach($window in $windows){$null=$window.CloseMainWindow()}
    if(!$process.WaitForExit(15000)){throw 'Test window did not close'}
    if($process.ExitCode -ne 0){throw 'Profiling process failed'}
    if(!$csv){throw ('No fresh profile for '+$view)}
    Copy-Item -LiteralPath $csv.FullName -Destination (Join-Path $output ($view+'.csv'))
    Copy-Item -LiteralPath (Join-Path $saved ('Screenshots\perf_'+$view+'.png')) -Destination (Join-Path $output ($view+'.png'))
    Write-Output ($Label+' '+$view+' captured')
}
& 'D:\epic\UE_5.3\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' (Join-Path $PSScriptRoot 'summarize_lighting_profile.py') $output
if($LASTEXITCODE -ne 0){throw 'Profile summary failed'}
