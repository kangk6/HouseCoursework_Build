$ErrorActionPreference = 'Stop'
$root = 'C:\Users\kangk6\Documents\ChatGPT\计算机图形学\House_inspection'
$summaries = @()
foreach ($name in @('living')) {
    $csv = Join-Path $root ('resident_idle_perf_'+$name+'_profile.csv')
    $parser = [Microsoft.VisualBasic.FileIO.TextFieldParser]::new($csv)
    $parser.TextFieldType = [Microsoft.VisualBasic.FileIO.FieldType]::Delimited
    $parser.SetDelimiters(',')
    $header = $parser.ReadFields()
    $frameColumn = [Array]::IndexOf($header,'FrameTime')
    $xColumn = [Array]::IndexOf($header,'View/PosX')
    $yColumn = [Array]::IndexOf($header,'View/PosY')
    $zColumn = [Array]::IndexOf($header,'View/PosZ')
    if ($frameColumn -lt 0) { throw 'No FrameTime column' }
    $frames = [System.Collections.Generic.List[double]]::new()
    $lastView = $null
    while (-not $parser.EndOfData) {
        try { $row = $parser.ReadFields() }
        catch { if ($frames.Count -ge 900) { break }; throw }
        $value = 0.0
        if ($row.Count -gt $frameColumn -and [double]::TryParse($row[$frameColumn],[System.Globalization.NumberStyles]::Float,[System.Globalization.CultureInfo]::InvariantCulture,[ref]$value) -and $value -gt 0) {
            $frames.Add($value)
            $lastView = @($row[$xColumn],$row[$yColumn],$row[$zColumn])
        }
    }
    $parser.Close()
    if ($frames.Count -le 120) { throw 'Insufficient captured frames' }
    $sample = @($frames | Select-Object -Skip 120)
    $mean = ($sample | Measure-Object -Average).Average
    $sorted = @($sample | Sort-Object)
    $summaries += [pscustomobject]@{view=$name; resolution='1280x720'; captured_frames=$frames.Count; warmup_frames=120; sample_frames=$sample.Count; average_fps=1000/$mean; average_ms=$mean; p95_ms=$sorted[[math]::Ceiling($sorted.Count*0.95)-1]; max_ms=$sorted[-1]; final_camera_position=$lastView; scope='Independent packaged executable; fixed representative viewpoint, not a continuous walking benchmark'}
}
$summaries | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'resident_idle_perf_performance_summary.json') -Encoding utf8
$summaries | Format-Table view,average_fps,p95_ms,max_ms,sample_frames
