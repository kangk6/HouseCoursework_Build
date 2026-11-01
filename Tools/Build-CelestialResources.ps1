param([string]$EngineRoot='D:\epic\UE_5.3',[string]$GameRoot='', [switch]$PrepareMaterials)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$GameRoot){$GameRoot=Join-Path $root 'Artifacts\ClockFoundation\Windows'}
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$pakTool=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealPak.exe'
$project=Join-Path $root 'HouseCoursework.uproject'
$out=Join-Path $root 'Saved\CelestialResourceBuild';$cooked=Join-Path $out 'Cooked'
$paks=Join-Path $GameRoot 'HouseCoursework\Content\Paks'
New-Item -ItemType Directory -Force -Path $out,$paks|Out-Null
if($PrepareMaterials){& $editor $project -run=pythonscript ('-script='+ (Join-Path $PSScriptRoot 'prepare_celestial_sky.py')) -nullrhi -unattended -nosplash;if($LASTEXITCODE -ne 0){throw 'Material preparation failed'}}
$names=@('M_CelestialSky')
foreach($name in $names){if(!(Test-Path -LiteralPath (Join-Path $root ('Content\Coursework\Celestial\'+$name+'.uasset')))){throw 'Import the celestial team resource pack first.'}}
$packages=($names|ForEach-Object{'/Game/Coursework/Celestial/'+$_}) -join '+'
& $editor $project -run=cook -targetplatform=Windows ('-PACKAGE='+$packages) -cooksinglepackagenorefs -NoDefaultMaps -NoGameAlwaysCook -SkipSoftReferences ('-outputdir='+$cooked) '-ini:Game:[/Script/UnrealEd.ProjectPackagingSettings]:bShareMaterialShaderCode=False' -unattended -nop4 -nosplash
if($LASTEXITCODE -ne 0){throw 'Celestial material cook failed'}
$files=Get-ChildItem -LiteralPath (Join-Path $cooked 'HouseCoursework\Content\Coursework\Celestial') -File
if($files.Count -ne 2){throw 'Expected one cooked material package and export'}
$response=Join-Path $out 'pak_response.txt'
$lines=$files|ForEach-Object{'"'+$_.FullName+'" "../../../HouseCoursework/Content/Coursework/Celestial/'+$_.Name+'"'}
$lines|Set-Content -LiteralPath $response -Encoding utf8
$stem=Join-Path $paks 'HouseCoursework-Celestial_P'
& $pakTool ($stem+'.pak') ('-Create='+$response) -compress
if($LASTEXITCODE -ne 0){throw 'Celestial pak build failed'}
$ioResponse=Join-Path $out 'iostore_response.txt'
$lines|Where-Object {$_ -match '\.uasset"'}|Set-Content -LiteralPath $ioResponse -Encoding utf8
$commands=Join-Path $out 'iostore_commands.txt'
('-Output="'+$stem+'.utoc" -ContainerName=Celestial -ResponseFile="'+$ioResponse+'"')|Set-Content -LiteralPath $commands -Encoding utf8
$metadata=Join-Path $cooked 'HouseCoursework\Metadata'
& $editor $project -run=IoStore ('-CreateGlobalContainer='+ (Join-Path $out 'CelestialGlobal.utoc')) ('-CookedDirectory='+$cooked) ('-PackageStoreManifest='+ (Join-Path $metadata 'packagestore.manifest')) ('-ScriptObjects='+ (Join-Path $metadata 'scriptobjects.bin')) ('-Commands='+$commands) -TargetPlatform=Windows -unattended -nop4 -nosplash
if($LASTEXITCODE -ne 0){throw 'Celestial IoStore container build failed'}
foreach($suffix in @('.pak','.utoc','.ucas')){if(!(Test-Path -LiteralPath ($stem+$suffix))){throw 'Celestial output missing: '+$suffix}}
Write-Output ('Built additive celestial resource containers in '+$paks)
