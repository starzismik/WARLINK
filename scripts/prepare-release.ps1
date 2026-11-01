param([switch]$ZipOnly, [string]$PackageName='WARLINK-1.0.0-BETA')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$release=Join-Path $root 'release'
if($PackageName -notmatch '^WARLINK-[0-9A-Za-z.-]+$'){throw 'Nom de paquet invalide'}
$stage=Join-Path $release $PackageName
if(Test-Path -LiteralPath $stage){throw 'Le dossier de paquet existe deja.'}
$manifestSource=Join-Path $root 'release-resources/manifest'
$configSource=[IO.File]::ReadAllText((Join-Path $root 'src/launcher/game_config.cpp'))
foreach($match in [regex]::Matches($configSource, '\.manifest_path = "manifest/([^"]+)"')) {
    $file=Join-Path $manifestSource $match.Groups[1].Value
    if(!(Test-Path -LiteralPath $file)){throw "Manifeste requis absent : $file"}
    $null=Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
}
$data=Join-Path $stage 'warlink/data'
[IO.Directory]::CreateDirectory($data)|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build/bin/x64/Release/WARLINK.exe') -Destination $stage
[IO.Directory]::CreateDirectory((Join-Path $data 'cef'))|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build/runtime/x64/Release/cef') -Destination (Join-Path $data 'cef/release') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'build/runtime/x64/Release/discord') -Destination $data -Recurse
Copy-Item -LiteralPath (Join-Path $root 'src/launcher-ui') -Destination $data -Recurse
Copy-Item -LiteralPath (Join-Path $root 'server-assets') -Destination $data -Recurse
Copy-Item -LiteralPath $manifestSource -Destination (Join-Path $stage 'warlink/manifest') -Recurse
[IO.File]::WriteAllText((Join-Path $stage 'warlink/portable.marker'),'')
foreach($name in @('LICENSE','CREDITS.md','README.md')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage}
$licences=Join-Path $stage 'licences'
[IO.Directory]::CreateDirectory($licences)|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'deps/cef/LICENSE.txt') -Destination (Join-Path $licences 'CEF-LICENSE.txt')
Get-ChildItem -LiteralPath (Join-Path $root 'deps/discord_social_sdk') -Filter '*License*' -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $licences}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $release ($PackageName+'.zip')
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,'Optimal',$false)
Get-FileHash -LiteralPath $zip -Algorithm SHA256
'Paquet propre prepare.'
