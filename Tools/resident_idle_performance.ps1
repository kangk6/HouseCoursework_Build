$ErrorActionPreference = 'Stop'
$evidenceRoot = 'C:\Users\kangk6\Documents\ChatGPT\计算机图形学\House_inspection'
$gameRoot = 'D:\HouseCoursework_Build\Windows'
$profileRoot = Join-Path $gameRoot 'HouseCoursework\Saved\Profiling\CSV'
$imageRoot = Join-Path $gameRoot 'HouseCoursework\Saved\Screenshots'
$views = @(
    @{ Name = 'living'; Position = '-2100 9870 151.5 -18 167 0' },
    @{ Name = 'kitchen'; Position = '-2050 9370 160 -8 0 0' },
    @{ Name = 'bedroom'; Position = '-2150 9350 430 -8 0 0' }
)
$results = @()
foreach ($view in ($views | Where-Object Name -eq living)) {
    if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close editor before independent performance testing.' }
    $begin = Get-Date
    $logPath = Join-Path $evidenceRoot ('resident_idle_perf_' + $view.Name + '_game.log')
    $runArguments = @(('-HouseTest=perf_'+$view.Name),'-windowed','-ResX=1280','-ResY=720','-ForceRes','-unattended','-csvCaptureFrames=900','-csvCompression=0','-ExitAfterCsvProfiling',('-abslog="'+$logPath+'"'))
    $testProcess = Start-Process -FilePath (Join-Path $gameRoot 'HouseCoursework.exe') -ArgumentList $runArguments -WindowStyle Hidden -PassThru
    $testProcess.WaitForExit()
    # Wrapper waits for its child in this packaged build; verify CSV rather than assuming exit means a successful test.
    $csv = Get-ChildItem -LiteralPath $profileRoot -Filter '*.csv' | Where-Object LastWriteTime -ge $begin | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $csv) { throw ('No profile generated for '+$view.Name) }
    $csvDestination = Join-Path $evidenceRoot ('resident_idle_perf_'+$view.Name+'_profile.csv')
    Copy-Item -LiteralPath $csv.FullName -Destination $csvDestination
    $screen = Get-ChildItem -LiteralPath $imageRoot -Filter '*.png' -ErrorAction SilentlyContinue | Where-Object LastWriteTime -ge $begin | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($screen) { Copy-Item -LiteralPath $screen.FullName -Destination (Join-Path $evidenceRoot ('resident_idle_perf_'+$view.Name+'.png')) }
    $results += @{ view=$view.Name; command_position=$view.Position; exit_code=$testProcess.ExitCode; csv=$csvDestination; rendered_image=[bool]$screen }
    $results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'resident_idle_perf_performance_runs.json') -Encoding utf8
    Write-Output ('Completed '+$view.Name)
}
