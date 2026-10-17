function Resolve-TeamPython([string]$EngineRoot) {
    $candidates=@()
    if($EngineRoot){$candidates+=(Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe')}
    if($env:UE53_ROOT){$candidates+=(Join-Path $env:UE53_ROOT 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe')}
    $candidates+=@('D:\epic\UE_5.3\Engine\Binaries\ThirdParty\Python3\Win64\python.exe','C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\ThirdParty\Python3\Win64\python.exe')
    foreach($candidate in $candidates){if(Test-Path -LiteralPath $candidate){return $candidate}}
    $python=Get-Command python -ErrorAction SilentlyContinue
    if($python){return $python.Source}
    throw 'Python 3.9+ required. Pass -EngineRoot with the installed UE 5.3 directory.'
}
