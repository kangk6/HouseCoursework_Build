param([string]$GameRoot='D:\HouseCoursework_TabFinal\Windows',
      [string]$EvidenceRoot='D:\HouseCoursework\Docs\Evacuation')
$ErrorActionPreference='Stop'
# v0.12 replaces the old four-second handling demo with real evacuation.
& (Join-Path $PSScriptRoot 'check_evacuation.ps1') -GameRoot $GameRoot -EvidenceRoot $EvidenceRoot
