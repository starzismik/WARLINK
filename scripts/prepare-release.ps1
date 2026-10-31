param([switch]$ZipOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$release=Join-Path $root 'release'
$stage=Join-Path $release 'WARLINK-1.0.0'
if(Test-Path -LiteralPath $stage){throw 'Le dossier de paquet existe déjà. Vérifier son contenu avant de le remplacer.'}
$data=Join-Path $stage 'warlink/data'
New-Item -ItemType Directory -Path $data -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build/bin/x64/Release/WARLINK.exe') -Destination $stage
New-Item -ItemType Directory -Path (Join-Path $data 'cef') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build/runtime/x64/Release/cef') -Destination (Join-Path $data 'cef/release') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'build/runtime/x64/Release/discord') -Destination $data -Recurse
Copy-Item -LiteralPath (Join-Path $root 'src/launcher-ui') -Destination $data -Recurse
Copy-Item -LiteralPath (Join-Path $root 'server-assets') -Destination $data -Recurse
New-Item -ItemType File -Path (Join-Path $stage 'warlink/portable.marker') -Force | Out-Null
foreach($name in @('LICENSE','CREDITS.md','README.md')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage}
$licences=Join-Path $stage 'licences'
New-Item -ItemType Directory -Path $licences | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'deps/cef/LICENSE.txt') -Destination (Join-Path $licences 'CEF-LICENSE.txt')
Get-ChildItem -LiteralPath (Join-Path $root 'deps/discord_social_sdk') -Filter '*License*' -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $licences}
# La sélection ci-dessus exclut user, servers, clients, manifest et les données de développement.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $release 'WARLINK-1.0.0.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,'Optimal',$false)
if(!$ZipOnly){$rar='C:\Program Files\WinRAR\Rar.exe';if(!(Test-Path -LiteralPath $rar)){throw 'WinRAR est nécessaire pour créer le RAR.'};$archive=Join-Path $release 'WARLINK-1.0.0.rar';& $rar a -r -ep1 $archive (Join-Path $stage '*');if($LASTEXITCODE -ne 0){throw 'Échec de création RAR'}}
Get-ChildItem -LiteralPath $release -File | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash
Write-Output 'Paquets préparés localement. Aucune release publiée.'
