$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$binary = Join-Path $root 'bin/HopupCreator-Educational.addon64'
if (!(Test-Path $binary)) { throw 'Build the add-on first with build.ps1.' }
$stage = Join-Path $root 'package-stage'
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item $binary, (Join-Path $root 'README.md'), (Join-Path $root 'SERVER-RULES.md') -Destination $stage
$version = if ($env:HOPUP_VERSION) { $env:HOPUP_VERSION } else { 'local' }
$zip = Join-Path $root "HopupCreator-Educational-$version.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
Remove-Item $stage -Recurse -Force
Write-Output "Created $zip"
