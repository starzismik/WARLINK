param([Parameter(Mandatory=$true)][string]$RequestPath,[switch]$NoRestart)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$request=Get-Content -LiteralPath $RequestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$run=[IO.Path]::GetFullPath((Split-Path -Parent $RequestPath))
$statusPath=Join-Path $run 'status.json'
function Status([string]$phase,[string]$message){
 $json=@{phase=$phase;message=$message}|ConvertTo-Json -Compress
 [IO.File]::WriteAllText($statusPath+'.tmp',$json,[Text.UTF8Encoding]::new($false))
 Move-Item -LiteralPath ($statusPath+'.tmp') -Destination $statusPath -Force
}
function Within([string]$base,[string]$child){return $child.StartsWith($base.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)}
function SafeTarget([string]$root,[string]$relative){
 $target=[IO.Path]::GetFullPath((Join-Path $root $relative))
 if(!(Within $root $target)){throw 'Chemin de mise à jour invalide.'}
 $ancestor=$target
 while($ancestor -and ($ancestor -eq $root -or (Within $root $ancestor))){
  if(Test-Path -LiteralPath $ancestor){if((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Un lien de dossier empêche la mise à jour automatique.'}}
  $ancestor=Split-Path -Parent $ancestor
 }
 return $target
}
$journal=[Collections.Generic.List[object]]::new();$closed=$false
try{
 $root=[IO.Path]::GetFullPath([string]$request.installDir)
 $exe=SafeTarget $root 'WARLINK.exe'
 if(!(Test-Path -LiteralPath $exe)){throw 'Installation WARLINK introuvable.'}
 if([string]$request.url -notmatch '^https://github\.com/starzismik/WARLINK/releases/download/[A-Za-z0-9._-]+/WARLINK-[A-Za-z0-9._-]+\.zip$'){throw 'Source de mise à jour non autorisée.'}
 if([string]$request.sha256 -notmatch '^[a-fA-F0-9]{64}$'){throw 'Empreinte de mise à jour manquante.'}
 $zip=Join-Path $run 'update.zip';$stage=Join-Path $run 'stage';$backup=Join-Path $run 'backup'
 Status 'download' 'Téléchargement de la mise à jour…'
 Invoke-WebRequest -Uri $request.url -OutFile $zip -UseBasicParsing -TimeoutSec 600
 Status 'verify' 'Vérification du téléchargement…'
 if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $request.sha256){throw 'Le téléchargement est incomplet ou son empreinte est incorrecte.'}
 $archive=[IO.Compression.ZipFile]::OpenRead($zip);$files=[Collections.Generic.List[string]]::new();$total=0L
 try{
  if($archive.Entries.Count -gt 10000){throw 'Archive trop grande.'}
  foreach($entry in $archive.Entries){
   $name=$entry.FullName.Replace('\','/')
   if($name.StartsWith('/') -or $name.Contains(':') -or $name.Split('/') -contains '..'){throw 'Chemin interdit dans le ZIP.'}
   if(!$entry.Name){continue}
   $allowed=$name -match '^(WARLINK\.exe|LICENSE|CREDITS\.md|README\.md|LISEZ-MOI\.md)$' -or $name -match '^licences/[^/]+$' -or $name -match '^warlink/manifest/[A-Za-z0-9_.-]+\.json$' -or $name -match '^warlink/data/(cef|discord|launcher-ui|server-assets|update-assets)/'
   if(!$allowed){if($name -eq 'warlink/portable.marker'){continue};throw ('Fichier inattendu dans la mise à jour : '+$name)}
   $total+=$entry.Length;if($total -gt 3GB){throw 'Archive trop grande.'}
   $target=SafeTarget $stage $name;[IO.Directory]::CreateDirectory((Split-Path -Parent $target))|Out-Null
   [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false);$files.Add($name)
  }
 }finally{$archive.Dispose()}
 if(!$files.Contains('WARLINK.exe')){throw 'Exécutable absent du ZIP.'}
 $newVersion=(Get-Item -LiteralPath (Join-Path $stage 'WARLINK.exe')).VersionInfo.ProductVersion
 if([version]$newVersion -ne [version]$request.version){throw 'Version du paquet incorrecte.'}
 # Preflight every destination before closing the running launcher.
 foreach($name in $files){$target=SafeTarget $root $name;$parent=Split-Path -Parent $target;[IO.Directory]::CreateDirectory($parent)|Out-Null;$probe=Join-Path $parent ([IO.Path]::GetRandomFileName());[IO.File]::WriteAllText($probe,'');Remove-Item -LiteralPath $probe}
 Status 'ready' 'Téléchargement vérifié. Fermeture et installation…'
 $deadline=[DateTime]::UtcNow.AddSeconds(120)
 while(Get-Process -Id $request.processId -ErrorAction SilentlyContinue){if([DateTime]::UtcNow -gt $deadline){throw 'Ferme WARLINK pour terminer la mise à jour.'};Start-Sleep -Milliseconds 250}
 $closed=$true;Status 'install' 'Installation de la nouvelle version…'
 foreach($name in $files){
  $target=SafeTarget $root $name;$saved=SafeTarget $backup $name;$existed=Test-Path -LiteralPath $target
  if($existed){[IO.Directory]::CreateDirectory((Split-Path -Parent $saved))|Out-Null;Copy-Item -LiteralPath $target -Destination $saved}
  $journal.Add(@{target=$target;backup=$saved;existed=$existed})
  $copied=$false
  for($attempt=0;$attempt -lt 60;$attempt++){try{Copy-Item -LiteralPath (Join-Path $stage $name) -Destination $target -Force;$copied=$true;break}catch{Start-Sleep -Milliseconds 500}}
  if(!$copied){throw ('Impossible de remplacer : '+$name)}
 }
 Status 'complete' 'Mise à jour installée. Redémarrage de WARLINK…'
 if(!$NoRestart){Start-Process -FilePath $exe -WorkingDirectory $root}
 # Only this updater's own unique temporary directory is removed.
 $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
 if((Within $tempRoot $run) -and (Split-Path -Leaf $run) -like 'WARLINK-update-*'){Remove-Item -LiteralPath $run -Recurse -Force}
}catch{
 $failure=$_.Exception.Message
 for($i=$journal.Count-1;$i -ge 0;$i--){$item=$journal[$i];try{if($item.existed){Copy-Item -LiteralPath $item.backup -Destination $item.target -Force}else{Remove-Item -LiteralPath $item.target -Force -ErrorAction SilentlyContinue}}catch{}}
 Status 'error' ('Mise à jour interrompue : '+$failure)
 if($closed -and !$NoRestart){
  Add-Type -AssemblyName System.Windows.Forms
  [Windows.Forms.MessageBox]::Show('La mise à jour a échoué. Les fichiers précédents ont été restaurés. '+$failure,'WARLINK')|Out-Null
  Start-Process -FilePath $exe -WorkingDirectory $root
 }
 exit 1
}
