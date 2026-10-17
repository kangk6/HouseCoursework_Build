param(
    [Parameter(Mandatory=$true)][ValidatePattern('^v[0-9]+\.[0-9]+\.[0-9]+-[a-z0-9-]+$')][string]$Version,
    [switch]$SourceCheckout
)
$ErrorActionPreference='Stop'
$repository=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$versionDirectory=Join-Path 'D:\HouseCoursework_Versions' $Version
$package=Join-Path $versionDirectory 'Windows\HouseCoursework.exe'
if($SourceCheckout){
    $checkout=Join-Path $versionDirectory 'Source'
    if(Test-Path -LiteralPath $checkout){throw "A source checkout already exists: $checkout"}
    & git -C $repository rev-parse --verify "$Version^{commit}" | Out-Null
    if($LASTEXITCODE -ne 0){throw 'Version tag missing'}
    & git -C $repository worktree add --detach $checkout $Version
    if($LASTEXITCODE -ne 0){throw 'Source checkout failed'}
    & git -C $checkout lfs fsck
    if($LASTEXITCODE -ne 0){throw 'LFS objects are incomplete'}
    Write-Output "Recovered source without changing current work: $checkout"
}else{
    if(!(Test-Path -LiteralPath $package)){throw "No archived package for $Version. Use -SourceCheckout to recover its source and rebuild."}
    # Launch the preserved package directly; never overwrite current files.
    Start-Process -FilePath $package
    Write-Output "Running preserved version: $Version"
}
